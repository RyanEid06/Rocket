using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using EnvDTE;
using EnvDTE80;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Rocket.VisualStudio.Core;

namespace Rocket.VisualStudio
{
    internal sealed class RocketDebugger : IDisposable
    {
        private readonly object gate = new object();
        private readonly AsyncPackage package;
        private readonly RocketOutputPane output;
        private readonly DTE2 dte;
        private RocketDebugProcess session;

        internal RocketDebugger(AsyncPackage package, RocketOutputPane output, DTE2 dte)
        {
            this.package = package;
            this.output = output;
            this.dte = dte;
        }

        internal bool IsRunning
        {
            get
            {
                lock (gate) return session?.IsRunning == true;
            }
        }

        internal async Task LaunchAsync(RocketTarget target, string artifactPath, string arguments,
            IDictionary<string, string> environment)
        {
            if (!File.Exists(artifactPath)) throw new FileNotFoundException("Rocket debug executable was not produced.", artifactPath);
            var pdb = Path.ChangeExtension(artifactPath, ".pdb");
            if (!File.Exists(pdb)) throw new FileNotFoundException("Rocket CodeView PDB was not produced.", pdb);
            var mapPath = Path.ChangeExtension(artifactPath, ".rocket.map.json");
            if (!File.Exists(mapPath)) mapPath = target.SourceMapPath;
            if (!File.Exists(mapPath)) throw new FileNotFoundException("Rocket source map was not produced.", mapPath);
            var sourceMap = RocketSourceMap.Read(mapPath);
            if (sourceMap.Sources.Count == 0) throw new InvalidDataException("The Rocket source map contains no source files.");
            var resolvedSources = ResolveSources(sourceMap, target);

            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var debugger = await package.GetServiceAsync(typeof(SVsShellDebugger)) as IVsDebugger4;
            if (debugger == null) throw new InvalidOperationException("Visual Studio's native debugger service is unavailable.");
            output.WriteLine($"Native debug: {artifactPath}");
            output.WriteLine($"PDB: {pdb}");
            output.WriteLine($"Rocket source map: {mapPath} ({sourceMap.Sources.Count} source file(s))");
            foreach (var source in resolvedSources)
                output.WriteLine($"Source: rocket:\\source\\{Path.GetFileName(source)} -> {source}");
            var flags = RocketDebugLaunchOptions.ForHiddenAttach(
                (uint)__VSDBGLAUNCHFLAGS.DBGLAUNCH_StopDebuggingOnEnd |
                (uint)__VSDBGLAUNCHFLAGS5.DBGLAUNCH_TerminateOnStop |
                (uint)__VSDBGLAUNCHFLAGS2.DBGLAUNCH_MergeEnv);
            RocketDebugProcess debugProcess = null;
            IntPtr debugEngines = IntPtr.Zero;
            try
            {
                debugProcess = RocketDebugProcess.Start(artifactPath, arguments, target.WorkingDirectory,
                    environment, (line, isError) => output.WriteLine((isError ? "[stderr] " : string.Empty) + line),
                    DebugProcessExited);
                debugEngines = Marshal.AllocCoTaskMem(Marshal.SizeOf(typeof(Guid)));
                Marshal.StructureToPtr(VSConstants.DebugEnginesGuids.NativeOnly_guid, debugEngines, false);
                var targetInfo = new VsDebugTargetInfo4
                {
                    dlo = (uint)DEBUG_LAUNCH_OPERATION.DLO_AlreadyRunning,
                    LaunchFlags = flags,
                    bstrExe = artifactPath,
                    bstrCurDir = target.WorkingDirectory,
                    dwProcessId = debugProcess.ProcessId,
                    dwDebugEngineCount = 1,
                    pDebugEngines = debugEngines,
                    fSendToOutputWindow = false,
                    guidProcessLanguage = Guid.Empty,
                    guidPortSupplier = Guid.Empty
                };
                output.WriteLine($"Hidden debug process: PID {debugProcess.ProcessId}; output redirected to Rocket.");
                debugger.LaunchDebugTargets4(1, new[] { targetInfo }, new VsDebugTargetProcessInfo[1]);
                lock (gate)
                {
                    session?.Stop();
                    session = debugProcess;
                }
                debugProcess.Resume();
                await ContinuePastInitialAttachBreakAsync(debugProcess);
            }
            catch
            {
                debugProcess?.Stop();
                debugProcess?.Dispose();
                throw;
            }
            finally
            {
                if (debugEngines != IntPtr.Zero) Marshal.FreeCoTaskMem(debugEngines);
            }
        }

        private static IList<string> ResolveSources(RocketSourceMap sourceMap, RocketTarget target)
        {
            var resolved = new List<string>();
            var logicalNames = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (var source in sourceMap.Sources)
            {
                var path = Path.IsPathRooted(source)
                    ? Path.GetFullPath(source)
                    : Path.GetFullPath(Path.Combine(target.WorkingDirectory, source));
                if (!File.Exists(path))
                    throw new FileNotFoundException("A Rocket source-map file could not be resolved in the workspace.", path);
                var logicalName = Path.GetFileName(path);
                if (logicalNames.TryGetValue(logicalName, out var existing) &&
                    !string.Equals(existing, path, StringComparison.OrdinalIgnoreCase))
                {
                    throw new InvalidDataException($"Rocket debug information contains two source files named '{logicalName}'. " +
                        "The frozen CodeView contract records source basenames, so Visual Studio cannot bind both files unambiguously.");
                }
                logicalNames[logicalName] = path;
                resolved.Add(path);
            }
            return resolved;
        }

        internal void Stop()
        {
            RocketDebugProcess current;
            lock (gate)
            {
                current = session;
                session = null;
            }
            current?.Stop();
        }

        public void Dispose()
        {
            Stop();
        }

        private void DebugProcessExited(RocketDebugProcess process, int exitCode)
        {
            lock (gate)
            {
                if (ReferenceEquals(session, process)) session = null;
            }
            output.WriteLine($"Rocket debuggee exited with code {exitCode}.");
        }

        private async Task ContinuePastInitialAttachBreakAsync(RocketDebugProcess process)
        {
            for (var attempt = 0; attempt != 100 && process.IsRunning; ++attempt)
            {
                await Task.Delay(50).ConfigureAwait(false);
                await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
                try
                {
                    if (dte?.Debugger?.CurrentMode == dbgDebugMode.dbgRunMode) continue;
                    if (dte?.Debugger?.CurrentMode != dbgDebugMode.dbgBreakMode) return;
                    var frame = dte.Debugger.CurrentStackFrame;
                    if (!RocketDebugLaunchOptions.IsInitialAttachBreak(frame?.Module, frame?.FunctionName)) return;
                    output.WriteLine("Continuing past the native attach breakpoint.");
                    dte.Debugger.Go(false);
                    return;
                }
                catch (COMException exception)
                {
                    output.WriteLine("Could not continue past the native attach breakpoint: " + exception.Message);
                    return;
                }
            }
        }
    }
}
