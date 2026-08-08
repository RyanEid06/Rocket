using System;
using System.Collections.Generic;
using System.ComponentModel.Design;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using EnvDTE;
using EnvDTE80;
using Microsoft.VisualStudio.Shell;
using Rocket.VisualStudio.Core;
using Task = System.Threading.Tasks.Task;

namespace Rocket.VisualStudio
{
    [PackageRegistration(UseManagedResourcesOnly = true, AllowsBackgroundLoading = true)]
    [ProvideMenuResource("Menus.ctmenu", 1)]
    [ProvideOptionPage(typeof(RocketOptionsPage), "Rocket", "General", 0, 0, true)]
    [ProvideAutoLoad(Microsoft.VisualStudio.Shell.Interop.UIContextGuids80.NoSolution, PackageAutoLoadFlags.BackgroundLoad)]
    [ProvideAutoLoad(Microsoft.VisualStudio.Shell.Interop.UIContextGuids80.SolutionExists, PackageAutoLoadFlags.BackgroundLoad)]
    [Guid(PackageIds.PackageGuidString)]
    internal sealed class RocketPackage : AsyncPackage
    {
        private static readonly object OptionsGate = new object();
        private static RocketOptionsSnapshot optionsSnapshot = Defaults();
        private static RocketPackage instance;

        private RocketOutputPane output;
        private RocketErrorList errors;
        private RocketProcessRunner processRunner;
        private RocketDebugger debugger;
        private CancellationTokenSource commandCancellation;
        private DTE2 dte;
        private bool commandActive;

        protected override async Task InitializeAsync(CancellationToken cancellationToken, IProgress<ServiceProgressData> progress)
        {
            instance = this;
            await JoinableTaskFactory.SwitchToMainThreadAsync(cancellationToken);
            var dteService = await GetServiceAsync(typeof(DTE)) as DTE2;
            if (dteService == null) throw new InvalidOperationException("Visual Studio automation service is unavailable.");
            dte = dteService;
            output = await RocketOutputPane.CreateAsync(this);
            errors = new RocketErrorList(this, dte);
            processRunner = new RocketProcessRunner();
            debugger = new RocketDebugger(this, output, dte);
            RefreshOptions();
            var commandService = await GetServiceAsync(typeof(IMenuCommandService)) as OleMenuCommandService;
            if (commandService == null) throw new InvalidOperationException("Visual Studio command service is unavailable.");
            RegisterAction(commandService, PackageIds.BuildCommand, RocketAction.Build);
            RegisterAction(commandService, PackageIds.RunCommand, RocketAction.Run);
            RegisterAction(commandService, PackageIds.TestCommand, RocketAction.Test);
            RegisterAction(commandService, PackageIds.DebugCommand, RocketAction.Debug);
            RegisterAction(commandService, PackageIds.ValidateCommand, RocketAction.Validate);

            var stop = new OleMenuCommand((sender, args) => Stop(), new CommandID(PackageIds.CommandSet, PackageIds.StopCommand));
            stop.BeforeQueryStatus += (sender, args) =>
            {
                ThreadHelper.ThrowIfNotOnUIThread();
                var running = commandActive || processRunner.IsRunning || debugger.IsRunning || IsDebugging();
                stop.Visible = running || IsRocketDocument(dte?.ActiveDocument?.FullName);
                stop.Enabled = running;
            };
            commandService.AddCommand(stop);

            var options = new OleMenuCommand((sender, args) => ShowOptionPage(typeof(RocketOptionsPage)),
                new CommandID(PackageIds.CommandSet, PackageIds.OptionsCommand));
            commandService.AddCommand(options);
            output.WriteLine("Rocket Visual Studio integration loaded.");
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                commandCancellation?.Cancel();
                commandCancellation?.Dispose();
                processRunner?.Dispose();
                debugger?.Dispose();
                errors?.Dispose();
            }
            base.Dispose(disposing);
        }

        internal static RocketOptionsSnapshot GetOptionsSnapshot()
        {
            lock (OptionsGate)
            {
                return new RocketOptionsSnapshot
                {
                    CompilerPath = optionsSnapshot.CompilerPath,
                    LanguageServerPath = optionsSnapshot.LanguageServerPath,
                    LoadPinnedEnvironment = optionsSnapshot.LoadPinnedEnvironment,
                    ProgramArguments = optionsSnapshot.ProgramArguments,
                    ShowOutputPane = optionsSnapshot.ShowOutputPane
                };
            }
        }

        internal static void WriteLanguageServerLog(string text)
        {
            var current = instance;
            if (current?.output != null) current.output.WriteLine("[LSP] " + text);
            else Trace.WriteLine("[Rocket LSP] " + text);
        }

        private void RegisterAction(OleMenuCommandService service, int id, RocketAction action)
        {
            var command = new OleMenuCommand((sender, args) =>
            {
                JoinableTaskFactory.RunAsync(() => ExecuteAsync(action));
            }, new CommandID(PackageIds.CommandSet, id));
            command.BeforeQueryStatus += (sender, args) =>
            {
                ThreadHelper.ThrowIfNotOnUIThread();
                var activePath = dte?.ActiveDocument?.FullName;
                command.Visible = action == RocketAction.Validate || IsRocketDocument(activePath);
                command.Enabled = !commandActive && !processRunner.IsRunning && !debugger.IsRunning &&
                    (action == RocketAction.Validate || IsRocketDocument(activePath));
            };
            service.AddCommand(command);
        }

        private async Task ExecuteAsync(RocketAction action)
        {
            var ownsCommand = false;
            try
            {
                await JoinableTaskFactory.SwitchToMainThreadAsync();
                if (commandActive) return;
                commandActive = true;
                ownsCommand = true;
                RefreshOptions();
                var options = GetOptionsSnapshot();
                var activePath = dte?.ActiveDocument?.FullName;
                RocketTarget target = null;
                if (action != RocketAction.Validate) target = RocketTargetDiscovery.Discover(activePath);
                var discoveryPath = target?.ActiveFile ?? activePath ?? Directory.GetCurrentDirectory();
                var workingDirectory = target?.WorkingDirectory ??
                    RocketTargetDiscovery.FindRepositoryRoot(discoveryPath) ??
                    RocketTargetDiscovery.ResolveWorkingDirectory(discoveryPath);
                var compiler = RocketToolLocator.FindCompiler(options.CompilerPath, discoveryPath);
                var server = RocketToolLocator.FindLanguageServer(options.LanguageServerPath, discoveryPath, compiler);
                if (compiler == null) throw new FileNotFoundException("rocketc.exe was not found. Build Rocket or configure Tools > Options > Rocket.");
                if (options.ShowOutputPane && HasInteractiveMainWindow()) output.Activate();
                errors.Clear();
                commandCancellation?.Dispose();
                commandCancellation = new CancellationTokenSource();
                var environment = await RocketEnvironmentLoader.LoadAsync(discoveryPath, options.LoadPinnedEnvironment);

                if (action == RocketAction.Validate)
                {
                    await ValidateAsync(compiler, server, environment, workingDirectory, commandCancellation.Token);
                    return;
                }

                if ((action == RocketAction.Run || action == RocketAction.Debug) && !target.IsExecutable)
                    throw new InvalidOperationException("Run and Debug require an executable Rocket package.");
                var command = action == RocketAction.Test ? "test" : action == RocketAction.Run ? "run" : "build";
                var arguments = new List<string> { command, target.CompilerInput };
                if (action == RocketAction.Debug) arguments.Add("--debug");
                arguments.Add("--message-format=json");
                if (action == RocketAction.Run && !string.IsNullOrWhiteSpace(options.ProgramArguments))
                {
                    arguments.Add("--");
                    arguments.AddRange(WindowsCommandLine.ParseArguments(options.ProgramArguments));
                }
                var diagnostics = new List<RocketMessage>();
                var artifact = target.ArtifactPath;
                output.WriteLine(string.Empty);
                output.WriteLine($"[{DateTime.Now:T}] Rocket {action.ToString().ToLowerInvariant()}: {target.CompilerInput}");
                var result = await processRunner.RunAsync(compiler, arguments, target.WorkingDirectory, environment, (line, isError) =>
                {
                    if (RocketMessageParser.TryParse(line, out var message))
                    {
                        output.WriteLine(RocketMessageParser.FormatForOutput(message));
                        lock (diagnostics)
                        {
                            if (message.Reason == "diagnostic") diagnostics.Add(message);
                            if (!string.IsNullOrWhiteSpace(message.Artifact)) artifact = Path.GetFullPath(message.Artifact);
                        }
                    }
                    else output.WriteLine((isError ? "[stderr] " : string.Empty) + line);
                }, commandCancellation.Token);
                await JoinableTaskFactory.SwitchToMainThreadAsync();
                lock (diagnostics)
                    foreach (var diagnostic in diagnostics) errors.Add(diagnostic, target);
                output.WriteLine(result.Cancelled ? "Rocket command stopped." : $"Rocket {action.ToString().ToLowerInvariant()} exited with code {result.ExitCode}.");
                if (result.ExitCode == 0 && action == RocketAction.Debug)
                    await debugger.LaunchAsync(target, artifact, options.ProgramArguments, environment);
            }
            catch (Exception exception)
            {
                output?.WriteLine("Rocket: " + exception.Message);
                await JoinableTaskFactory.SwitchToMainThreadAsync();
                VsShellUtilities.ShowMessageBox(this, exception.Message, "Rocket", Microsoft.VisualStudio.Shell.Interop.OLEMSGICON.OLEMSGICON_CRITICAL,
                    Microsoft.VisualStudio.Shell.Interop.OLEMSGBUTTON.OLEMSGBUTTON_OK,
                    Microsoft.VisualStudio.Shell.Interop.OLEMSGDEFBUTTON.OLEMSGDEFBUTTON_FIRST);
            }
            finally
            {
                if (ownsCommand)
                {
                    await JoinableTaskFactory.SwitchToMainThreadAsync();
                    commandActive = false;
                }
            }
        }

        private async Task ValidateAsync(string compiler, string server, IDictionary<string, string> environment,
            string workingDirectory, CancellationToken token)
        {
            output.WriteLine(string.Empty);
            output.WriteLine($"[{DateTime.Now:T}] Validating Rocket environment");
            output.WriteLine("Compiler: " + compiler);
            var compilerResult = await processRunner.RunAsync(compiler, new[] { "--version" }, workingDirectory, environment,
                (line, isError) => output.WriteLine(line), token);
            if (compilerResult.ExitCode != 0) throw new InvalidOperationException("rocketc --version failed.");
            if (server == null) throw new FileNotFoundException("rocket-lsp.exe was not found beside the compiler, in the workspace, environment, or PATH.");
            output.WriteLine("Language server: " + server);
            var serverResult = await processRunner.RunAsync(server, new[] { "--version" }, workingDirectory, environment,
                (line, isError) => output.WriteLine(line), token);
            if (serverResult.ExitCode != 0) throw new InvalidOperationException("rocket-lsp --version failed.");
            output.WriteLine("Rocket environment validation succeeded. Processes are configured for hidden-window, redirected-stream execution.");
        }

        private void Stop()
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            commandCancellation?.Cancel();
            processRunner.Stop();
            if (IsDebugging()) dte.Debugger.Stop(false);
            debugger.Stop();
            output.WriteLine("Stop requested.");
        }

        private bool IsDebugging()
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            return dte?.Debugger != null && dte.Debugger.CurrentMode != dbgDebugMode.dbgDesignMode;
        }

        private bool HasInteractiveMainWindow()
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            try
            {
                return dte?.MainWindow != null && dte.MainWindow.HWnd != IntPtr.Zero;
            }
            catch (COMException)
            {
                return false;
            }
        }

        private void RefreshOptions()
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            var page = (RocketOptionsPage)GetDialogPage(typeof(RocketOptionsPage));
            lock (OptionsGate)
            {
                optionsSnapshot = new RocketOptionsSnapshot
                {
                    CompilerPath = page.CompilerPath ?? string.Empty,
                    LanguageServerPath = page.LanguageServerPath ?? string.Empty,
                    LoadPinnedEnvironment = page.LoadPinnedEnvironment,
                    ProgramArguments = page.ProgramArguments ?? string.Empty,
                    ShowOutputPane = page.ShowOutputPane
                };
            }
        }

        private static RocketOptionsSnapshot Defaults()
        {
            return new RocketOptionsSnapshot
            {
                CompilerPath = string.Empty,
                LanguageServerPath = string.Empty,
                LoadPinnedEnvironment = true,
                ProgramArguments = string.Empty,
                ShowOutputPane = true
            };
        }

        private static bool IsRocketDocument(string path)
        {
            if (string.IsNullOrWhiteSpace(path)) return false;
            return string.Equals(Path.GetExtension(path), ".rocket", StringComparison.OrdinalIgnoreCase) ||
                   string.Equals(Path.GetFileName(path), "rocket.toml", StringComparison.OrdinalIgnoreCase);
        }

    }
}
