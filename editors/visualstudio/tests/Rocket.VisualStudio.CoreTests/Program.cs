using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using Rocket.VisualStudio.Core;
using Rocket.VisualStudio;

internal static class Program
{
    private static int failures;

    private static int Main()
    {
        var root = Path.Combine(Path.GetTempPath(), "rocket-vsix-core-tests-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(Path.Combine(root, "package", "src", "nested"));
            File.WriteAllText(Path.Combine(root, "package", "rocket.toml"),
                "[package]\nname = \"sample\"\nentry = \"src/main.rocket\"\n\n[build]\nkind = \"executable\"\nname = \"sample-app\"\n");
            var source = Path.Combine(root, "package", "src", "nested", "file.rocket");
            File.WriteAllText(source, "fn main() -> Int:\n    return 0\n");
            var target = RocketTargetDiscovery.Discover(source);
            Check(target.IsPackage, "nearest manifest produces package target");
            Check(target.CompilerInput == Path.Combine(root, "package"), "package root is compiler input");
            Check(target.ArtifactName == "sample-app", "manifest build name is honored");
            Check(target.ArtifactPath.EndsWith(Path.Combine(".rocketc", "sample-app.exe")), "artifact path is deterministic");

            var standalone = Path.Combine(root, "standalone.rocket");
            File.WriteAllText(standalone, "fn main() -> Int:\n    return 0\n");
            var standaloneTarget = RocketTargetDiscovery.Discover(standalone);
            Check(!standaloneTarget.IsPackage, "standalone source remains supported");
            Check(standaloneTarget.CompilerInput == standalone, "standalone compiler input is the source file");
            var standaloneWorkingDirectory = RocketTargetDiscovery.ResolveWorkingDirectory(standalone);
            Check(standaloneWorkingDirectory == root,
                "active source resolves to a directory for validation");
            Check(RocketTargetDiscovery.ResolveWorkingDirectory(root) == root,
                "active directory remains the validation working directory");

            const string diagnostic = "{\"schema\":\"rocket-message-1\",\"reason\":\"diagnostic\",\"level\":\"error\",\"code\":\"R4002\",\"message\":\"missing name\",\"span\":{\"file\":\"src/main.rocket\",\"line\":7,\"column\":11}}";
            Check(RocketMessageParser.TryParse(diagnostic, out var parsed), "diagnostic JSON is recognized");
            Check(parsed.Code == "R4002" && parsed.Line == 7 && parsed.Column == 11, "diagnostic fields retain source location");
            Check(RocketMessageParser.FormatForOutput(parsed).Contains("src/main.rocket(7,11)"), "diagnostic output is Visual Studio-shaped");

            const string summary = "{\"schema\":\"rocket-message-1\",\"reason\":\"test-summary\",\"passed\":2,\"failed\":0,\"expectedFailures\":1,\"selected\":3}";
            Check(RocketMessageParser.TryParse(summary, out var summaryMessage), "test summary JSON is recognized");
            Check(summaryMessage.Passed == 2 && summaryMessage.Selected == 3, "test summary counters are retained");
            Check(!RocketMessageParser.TryParse("program output", out _), "ordinary program output is not mistaken for JSON");

            var mapPath = Path.Combine(root, "sample.rocket.map.json");
            File.WriteAllText(mapPath, "{\"format\":\"rocket-source-map-1\",\"optimized\":false,\"functions\":[{\"symbol\":\"rocket_fn_fibonacci_1\",\"name\":\"fibonacci\",\"source\":\"src/main.rocket\",\"line\":1,\"column\":1,\"locations\":[{\"source\":\"src/main.rocket\",\"line\":2,\"column\":5}]}]}" );
            var sourceMap = RocketSourceMap.Read(mapPath);
            Check(sourceMap.Sources.Count == 1 && sourceMap.Symbols[0] == "rocket_fn_fibonacci_1", "source map exposes unique sources and native symbols");

            var debugFlags = RocketDebugLaunchOptions.ForHiddenAttach(0x20 | 0x08000000 | 0x20000000);
            Check(!RocketDebugLaunchOptions.UsesExternalConsoleLaunch(debugFlags),
                "native debugger attach cannot request Visual Studio console or terminal launch");
            Check((debugFlags & 0x20) != 0,
                "hidden debugger attach preserves existing launch behavior");
            Check(RocketDebugLaunchOptions.CreatesHiddenSuspendedProcess(
                    RocketDebugLaunchOptions.HiddenProcessCreationFlags),
                "Rocket creates the debuggee suspended with no window before native attach");
            Check(RocketDebugLaunchOptions.IsInitialAttachBreak("ntdll.dll", "00007ffb"),
                "native attach breakpoint is recognized without relying on a symbol name");
            Check(!RocketDebugLaunchOptions.IsInitialAttachBreak("main.exe", "fibonacci"),
                "Rocket source breakpoints are never auto-continued");

            var debugOutput = new List<string>();
            var debugExited = new ManualResetEventSlim();
            var debugProcess = RocketDebugProcess.Start(
                Environment.GetEnvironmentVariable("COMSPEC") ?? "cmd.exe",
                "/d /c echo hidden-debug-output", root, new Dictionary<string, string>(),
                (line, isError) =>
                {
                    lock (debugOutput) debugOutput.Add(line);
                },
                (process, exitCode) => debugExited.Set());
            Check(debugProcess.IsRunning, "hidden debug process remains suspended before debugger attach");
            debugProcess.Resume();
            Check(debugExited.Wait(TimeSpan.FromSeconds(10)), "hidden debug process resumes and exits");
            lock (debugOutput)
                Check(debugOutput.Contains("hidden-debug-output"),
                    "hidden debug process redirects program output to Rocket");

            var commandLine = WindowsCommandLine.ParseArguments("plain \"two words\" \"trailing\\\\\"");
            Check(commandLine.Count == 3 && commandLine[1] == "two words" && commandLine[2] == "trailing\\", "program arguments follow Windows quoting rules");

            using (var runner = new RocketProcessRunner())
            {
                var output = new List<string>();
                var processResult = runner.RunAsync(Environment.GetEnvironmentVariable("COMSPEC") ?? "cmd.exe",
                    new[] { "/d", "/c", "echo hidden-output" }, standaloneWorkingDirectory, new Dictionary<string, string>(),
                    (line, isError) => output.Add(line), CancellationToken.None).GetAwaiter().GetResult();
                Check(processResult.ExitCode == 0 && output.Contains("hidden-output"), "hidden process output is redirected to the caller");
                Check(!runner.IsRunning, "process runner releases completed commands");
            }
        }
        finally
        {
            if (Directory.Exists(root)) Directory.Delete(root, true);
        }

        Console.WriteLine(failures == 0 ? "Rocket Visual Studio core tests passed" : $"{failures} Rocket Visual Studio core test(s) failed");
        return failures == 0 ? 0 : 1;
    }

    private static void Check(bool condition, string name)
    {
        if (condition)
        {
            Console.WriteLine("PASS " + name);
            return;
        }
        ++failures;
        Console.Error.WriteLine("FAIL " + name);
    }
}
