using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Win32.SafeHandles;
using Rocket.VisualStudio.Core;

namespace Rocket.VisualStudio
{
    internal sealed class RocketDebugProcess : IDisposable
    {
        private const uint StartfUseStdHandles = 0x00000100;
        private const uint HandleFlagInherit = 0x00000001;
        private const uint Infinite = 0xffffffff;
        private const uint StillActive = 259;
        private const uint GenericRead = 0x80000000;
        private const uint FileShareRead = 0x00000001;
        private const uint FileShareWrite = 0x00000002;
        private const uint OpenExisting = 3;

        private readonly object gate = new object();
        private readonly Action<string, bool> receiveOutput;
        private readonly Action<RocketDebugProcess, int> exited;
        private readonly FileStream standardOutput;
        private readonly FileStream standardError;
        private readonly Task standardOutputTask;
        private readonly Task standardErrorTask;
        private IntPtr processHandle;
        private IntPtr primaryThreadHandle;
        private bool disposed;

        private RocketDebugProcess(ProcessInformation information, FileStream standardOutput,
            FileStream standardError, Action<string, bool> receiveOutput,
            Action<RocketDebugProcess, int> exited)
        {
            ProcessId = information.dwProcessId;
            processHandle = information.hProcess;
            primaryThreadHandle = information.hThread;
            this.standardOutput = standardOutput;
            this.standardError = standardError;
            this.receiveOutput = receiveOutput;
            this.exited = exited;
            standardOutputTask = Task.Run(() => PumpAsync(standardOutput, false));
            standardErrorTask = Task.Run(() => PumpAsync(standardError, true));
            Task.Run(WaitForExit);
        }

        internal uint ProcessId { get; }

        internal bool IsRunning
        {
            get
            {
                lock (gate)
                {
                    if (disposed || processHandle == IntPtr.Zero) return false;
                    return GetExitCodeProcess(processHandle, out var exitCode) && exitCode == StillActive;
                }
            }
        }

        internal static RocketDebugProcess Start(string executable, string arguments, string workingDirectory,
            IDictionary<string, string> environment, Action<string, bool> receiveOutput,
            Action<RocketDebugProcess, int> exited)
        {
            SafeFileHandle stdoutRead = null;
            SafeFileHandle stdoutWrite = null;
            SafeFileHandle stderrRead = null;
            SafeFileHandle stderrWrite = null;
            SafeFileHandle stdin = null;
            IntPtr environmentBlock = IntPtr.Zero;
            try
            {
                var security = new SecurityAttributes
                {
                    nLength = Marshal.SizeOf(typeof(SecurityAttributes)),
                    bInheritHandle = true
                };
                CreateOutputPipe(ref security, out stdoutRead, out stdoutWrite);
                CreateOutputPipe(ref security, out stderrRead, out stderrWrite);
                stdin = CreateFile("NUL", GenericRead, FileShareRead | FileShareWrite, ref security,
                    OpenExisting, 0, IntPtr.Zero);
                if (stdin.IsInvalid) throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not open NUL for Rocket debug input.");

                var startup = new StartupInfo
                {
                    cb = Marshal.SizeOf(typeof(StartupInfo)),
                    dwFlags = StartfUseStdHandles,
                    hStdInput = stdin.DangerousGetHandle(),
                    hStdOutput = stdoutWrite.DangerousGetHandle(),
                    hStdError = stderrWrite.DangerousGetHandle()
                };
                var commandLine = new StringBuilder(QuoteArgument(executable));
                if (!string.IsNullOrWhiteSpace(arguments)) commandLine.Append(' ').Append(arguments);
                environmentBlock = Marshal.StringToHGlobalUni(BuildEnvironmentBlock(environment));
                if (!CreateProcess(executable, commandLine, IntPtr.Zero, IntPtr.Zero, true,
                    RocketDebugLaunchOptions.HiddenProcessCreationFlags, environmentBlock,
                    workingDirectory, ref startup, out var processInformation))
                {
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not start the hidden Rocket debug process.");
                }

                stdoutWrite.Dispose();
                stdoutWrite = null;
                stderrWrite.Dispose();
                stderrWrite = null;
                stdin.Dispose();
                stdin = null;
                var stdoutStream = new FileStream(stdoutRead, FileAccess.Read, 4096, false);
                stdoutRead = null;
                var stderrStream = new FileStream(stderrRead, FileAccess.Read, 4096, false);
                stderrRead = null;
                return new RocketDebugProcess(processInformation, stdoutStream, stderrStream,
                    receiveOutput, exited);
            }
            finally
            {
                if (environmentBlock != IntPtr.Zero) Marshal.FreeHGlobal(environmentBlock);
                stdoutRead?.Dispose();
                stdoutWrite?.Dispose();
                stderrRead?.Dispose();
                stderrWrite?.Dispose();
                stdin?.Dispose();
            }
        }

        internal void Resume()
        {
            lock (gate)
            {
                if (disposed || primaryThreadHandle == IntPtr.Zero)
                    throw new InvalidOperationException("The Rocket debug process is no longer available.");
                if (ResumeThread(primaryThreadHandle) == uint.MaxValue)
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not resume the Rocket debug process.");
                CloseHandle(primaryThreadHandle);
                primaryThreadHandle = IntPtr.Zero;
            }
        }

        internal void Stop()
        {
            lock (gate)
            {
                if (disposed || processHandle == IntPtr.Zero) return;
                if (GetExitCodeProcess(processHandle, out var exitCode) && exitCode == StillActive)
                    TerminateProcess(processHandle, 1);
            }
        }

        public void Dispose()
        {
            lock (gate)
            {
                if (disposed) return;
                disposed = true;
                standardOutput.Dispose();
                standardError.Dispose();
                if (primaryThreadHandle != IntPtr.Zero)
                {
                    CloseHandle(primaryThreadHandle);
                    primaryThreadHandle = IntPtr.Zero;
                }
                if (processHandle != IntPtr.Zero)
                {
                    CloseHandle(processHandle);
                    processHandle = IntPtr.Zero;
                }
            }
        }

        private static void CreateOutputPipe(ref SecurityAttributes security, out SafeFileHandle read,
            out SafeFileHandle write)
        {
            if (!CreatePipe(out read, out write, ref security, 0))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not create a Rocket debug output pipe.");
            if (!SetHandleInformation(read, HandleFlagInherit, 0))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not protect a Rocket debug output pipe.");
        }

        private async Task PumpAsync(Stream stream, bool isError)
        {
            try
            {
                using (var reader = new StreamReader(stream, Encoding.UTF8, true, 4096, true))
                {
                    string line;
                    while ((line = await reader.ReadLineAsync().ConfigureAwait(false)) != null)
                        receiveOutput?.Invoke(line, isError);
                }
            }
            catch (ObjectDisposedException)
            {
            }
            catch (IOException exception)
            {
                receiveOutput?.Invoke("debug output error: " + exception.Message, true);
            }
        }

        private void WaitForExit()
        {
            IntPtr handle;
            lock (gate) handle = processHandle;
            if (handle == IntPtr.Zero) return;
            WaitForSingleObject(handle, Infinite);
            var exitCode = 0;
            GetExitCodeProcess(handle, out var nativeExitCode);
            if (nativeExitCode != StillActive) exitCode = unchecked((int)nativeExitCode);
            try
            {
                Task.WaitAll(standardOutputTask, standardErrorTask);
            }
            catch (AggregateException)
            {
            }
            exited?.Invoke(this, exitCode);
            Dispose();
        }

        private static string BuildEnvironmentBlock(IDictionary<string, string> overrides)
        {
            var values = new SortedDictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (DictionaryEntry item in Environment.GetEnvironmentVariables())
                values[(string)item.Key] = item.Value?.ToString() ?? string.Empty;
            if (overrides != null)
                foreach (var item in overrides) values[item.Key] = item.Value ?? string.Empty;
            var builder = new StringBuilder();
            foreach (var item in values) builder.Append(item.Key).Append('=').Append(item.Value).Append('\0');
            builder.Append('\0');
            return builder.ToString();
        }

        private static string QuoteArgument(string value)
        {
            if (string.IsNullOrEmpty(value)) return "\"\"";
            if (value.IndexOfAny(new[] { ' ', '\t', '\"' }) < 0) return value;
            var result = new StringBuilder("\"");
            var slashes = 0;
            foreach (var character in value)
            {
                if (character == '\\')
                {
                    ++slashes;
                    continue;
                }
                if (character == '\"')
                {
                    result.Append('\\', slashes * 2 + 1).Append('\"');
                    slashes = 0;
                    continue;
                }
                result.Append('\\', slashes).Append(character);
                slashes = 0;
            }
            return result.Append('\\', slashes * 2).Append('\"').ToString();
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct SecurityAttributes
        {
            internal int nLength;
            internal IntPtr lpSecurityDescriptor;
            [MarshalAs(UnmanagedType.Bool)] internal bool bInheritHandle;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct StartupInfo
        {
            internal int cb;
            internal string lpReserved;
            internal string lpDesktop;
            internal string lpTitle;
            internal int dwX;
            internal int dwY;
            internal int dwXSize;
            internal int dwYSize;
            internal int dwXCountChars;
            internal int dwYCountChars;
            internal int dwFillAttribute;
            internal uint dwFlags;
            internal short wShowWindow;
            internal short cbReserved2;
            internal IntPtr lpReserved2;
            internal IntPtr hStdInput;
            internal IntPtr hStdOutput;
            internal IntPtr hStdError;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct ProcessInformation
        {
            internal IntPtr hProcess;
            internal IntPtr hThread;
            internal uint dwProcessId;
            internal uint dwThreadId;
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CreatePipe(out SafeFileHandle readPipe, out SafeFileHandle writePipe,
            ref SecurityAttributes pipeAttributes, int size);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool SetHandleInformation(SafeFileHandle handle, uint mask, uint flags);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern SafeFileHandle CreateFile(string fileName, uint desiredAccess, uint shareMode,
            ref SecurityAttributes securityAttributes, uint creationDisposition, uint flagsAndAttributes,
            IntPtr templateFile);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool CreateProcess(string applicationName, StringBuilder commandLine,
            IntPtr processAttributes, IntPtr threadAttributes, bool inheritHandles, uint creationFlags,
            IntPtr environment, string currentDirectory, ref StartupInfo startupInfo,
            out ProcessInformation processInformation);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern uint ResumeThread(IntPtr thread);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool TerminateProcess(IntPtr process, uint exitCode);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GetExitCodeProcess(IntPtr process, out uint exitCode);

        [DllImport("kernel32.dll")]
        private static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

        [DllImport("kernel32.dll")]
        private static extern bool CloseHandle(IntPtr handle);
    }
}
