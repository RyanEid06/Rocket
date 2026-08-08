using System;
using System.IO;
using System.Runtime.InteropServices;
using EnvDTE;
using Microsoft.VisualStudio.Shell;
using Rocket.VisualStudio.Core;

namespace Rocket.VisualStudio
{
    internal sealed class RocketErrorList : IDisposable
    {
        private readonly ErrorListProvider provider;
        private readonly DTE dte;

        internal RocketErrorList(IServiceProvider serviceProvider, DTE dte)
        {
            this.dte = dte;
            provider = new ErrorListProvider(serviceProvider)
            {
                ProviderName = "Rocket",
                ProviderGuid = new Guid("B6DFF3E7-933B-4C0D-86FB-B39898BF3F98")
            };
        }

        internal void Clear()
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            provider.Tasks.Clear();
        }

        internal void Add(RocketMessage diagnostic, RocketTarget target)
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            if (!HasInteractiveMainWindow()) return;
            var document = ResolveDocument(diagnostic.File, target);
            var task = new ErrorTask
            {
                Category = TaskCategory.BuildCompile,
                ErrorCategory = MapCategory(diagnostic.Level),
                Text = string.IsNullOrWhiteSpace(diagnostic.Code)
                    ? diagnostic.Message
                    : $"[{diagnostic.Code}] {diagnostic.Message}",
                Document = document,
                Line = Math.Max(0, diagnostic.Line - 1),
                Column = Math.Max(0, diagnostic.Column - 1)
            };
            task.Navigate += Navigate;
            provider.Tasks.Add(task);
            provider.Show();
        }

        public void Dispose()
        {
            foreach (ErrorTask task in provider.Tasks) task.Navigate -= Navigate;
            provider.Dispose();
        }

        private void Navigate(object sender, EventArgs eventArgs)
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            if (!(sender is ErrorTask task) || string.IsNullOrWhiteSpace(task.Document) || !File.Exists(task.Document)) return;
            var window = dte?.ItemOperations.OpenFile(task.Document, Constants.vsViewKindTextView);
            window?.Activate();
            if (dte?.ActiveDocument?.Selection is TextSelection selection)
                selection.MoveToLineAndOffset(task.Line + 1, task.Column + 1, false);
        }

        private static string ResolveDocument(string path, RocketTarget target)
        {
            if (string.IsNullOrWhiteSpace(path)) return string.Empty;
            if (Path.IsPathRooted(path)) return Path.GetFullPath(path);
            var packageCandidate = target.PackageRoot == null ? null : Path.Combine(target.PackageRoot, path);
            if (packageCandidate != null && File.Exists(packageCandidate)) return Path.GetFullPath(packageCandidate);
            return Path.GetFullPath(Path.Combine(target.WorkingDirectory, path));
        }

        private static TaskErrorCategory MapCategory(string level)
        {
            if (string.Equals(level, "warning", StringComparison.OrdinalIgnoreCase)) return TaskErrorCategory.Warning;
            if (string.Equals(level, "info", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(level, "information", StringComparison.OrdinalIgnoreCase)) return TaskErrorCategory.Message;
            return TaskErrorCategory.Error;
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
    }
}
