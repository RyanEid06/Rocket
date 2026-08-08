using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using Rocket.VisualStudio.Core;

namespace Rocket.VisualStudio
{
    internal static class RocketEnvironmentLoader
    {
        private static readonly object Gate = new object();
        private static string cachedScript;
        private static DateTime cachedWriteTime;
        private static IDictionary<string, string> cachedEnvironment;

        internal static async Task<IDictionary<string, string>> LoadAsync(string activePath, bool enabled)
        {
            if (!enabled) return new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            var repository = RocketTargetDiscovery.FindRepositoryRoot(activePath);
            if (repository == null) return new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            var script = Path.Combine(repository, "dependencies", "activate.ps1");
            var writeTime = File.GetLastWriteTimeUtc(script);
            lock (Gate)
            {
                if (string.Equals(script, cachedScript, StringComparison.OrdinalIgnoreCase) &&
                    writeTime == cachedWriteTime && cachedEnvironment != null)
                    return new Dictionary<string, string>(cachedEnvironment, StringComparer.OrdinalIgnoreCase);
            }

            var systemDirectory = Environment.GetFolderPath(Environment.SpecialFolder.System);
            var powershell = Path.Combine(systemDirectory, "WindowsPowerShell", "v1.0", "powershell.exe");
            if (!File.Exists(powershell)) throw new FileNotFoundException("Windows PowerShell is required to load Rocket's pinned environment.", powershell);
            var escapedScript = script.Replace("'", "''");
            var command = "$ErrorActionPreference='Stop'; . '" + escapedScript +
                          "'; [Environment]::GetEnvironmentVariables('Process').GetEnumerator() | ForEach-Object { " +
                          "[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes([string]$_.Key)) + ':' + " +
                          "[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes([string]$_.Value)) }";
            var startInfo = new ProcessStartInfo
            {
                FileName = powershell,
                Arguments = RocketProcessRunner.JoinArguments(new[] { "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", command }),
                WorkingDirectory = repository,
                UseShellExecute = false,
                CreateNoWindow = true,
                WindowStyle = ProcessWindowStyle.Hidden,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };
            using (var process = Process.Start(startInfo))
            {
                var outputTask = process.StandardOutput.ReadToEndAsync();
                var errorTask = process.StandardError.ReadToEndAsync();
                await Task.Run(() => process.WaitForExit()).ConfigureAwait(false);
                var output = await outputTask.ConfigureAwait(false);
                var error = await errorTask.ConfigureAwait(false);
                if (process.ExitCode != 0)
                    throw new InvalidOperationException("Rocket environment activation failed: " + error.Trim());
                var environment = ParseEnvironment(output);
                lock (Gate)
                {
                    cachedScript = script;
                    cachedWriteTime = writeTime;
                    cachedEnvironment = environment;
                }
                return new Dictionary<string, string>(environment, StringComparer.OrdinalIgnoreCase);
            }
        }

        private static IDictionary<string, string> ParseEnvironment(string output)
        {
            var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (var rawLine in output.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries))
            {
                var separator = rawLine.IndexOf(':');
                if (separator <= 0) continue;
                try
                {
                    var key = Encoding.UTF8.GetString(Convert.FromBase64String(rawLine.Substring(0, separator)));
                    var value = Encoding.UTF8.GetString(Convert.FromBase64String(rawLine.Substring(separator + 1)));
                    if (!string.IsNullOrEmpty(key)) result[key] = value;
                }
                catch (FormatException)
                {
                    // Ignore non-environment output emitted by a locally customized activation script.
                }
            }
            return result;
        }
    }
}
