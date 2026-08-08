using System;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;

namespace Rocket.VisualStudio
{
    internal sealed class RocketOutputPane
    {
        private static readonly Guid PaneGuid = new Guid("35632848-471D-4EE1-B135-3636044A5FF5");
        private readonly IVsOutputWindowPane pane;

        private RocketOutputPane(IVsOutputWindowPane pane)
        {
            this.pane = pane;
        }

        internal static async System.Threading.Tasks.Task<RocketOutputPane> CreateAsync(AsyncPackage package)
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var outputWindow = await package.GetServiceAsync(typeof(SVsOutputWindow)) as IVsOutputWindow;
            if (outputWindow == null) throw new InvalidOperationException("Visual Studio Output Window service is unavailable.");
            var paneGuid = PaneGuid;
            ErrorHandler.ThrowOnFailure(outputWindow.CreatePane(ref paneGuid, "Rocket", 1, 1));
            ErrorHandler.ThrowOnFailure(outputWindow.GetPane(ref paneGuid, out var outputPane));
            return new RocketOutputPane(outputPane);
        }

        internal void WriteLine(string text)
        {
            pane.OutputStringThreadSafe((text ?? string.Empty) + Environment.NewLine);
        }

        internal void Activate()
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            pane.Activate();
        }

        internal void Clear()
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            pane.Clear();
        }
    }
}
