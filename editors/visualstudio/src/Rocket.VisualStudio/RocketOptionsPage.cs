using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.VisualStudio.Shell;

namespace Rocket.VisualStudio
{
    [ComVisible(true)]
    internal sealed class RocketOptionsPage : DialogPage
    {
        [Category("Tools")]
        [DisplayName("Rocket compiler path")]
        [Description("Optional path to rocketc.exe. Leave empty to discover it from the active workspace, environment, or PATH.")]
        public string CompilerPath { get; set; } = string.Empty;

        [Category("Tools")]
        [DisplayName("Language server path")]
        [Description("Optional path to rocket-lsp.exe. Leave empty to discover it beside the compiler, in the active workspace, environment, or PATH.")]
        public string LanguageServerPath { get; set; } = string.Empty;

        [Category("Environment")]
        [DisplayName("Load pinned repository environment")]
        [Description("When available, load dependencies/activate.ps1 in a hidden process and pass its pinned MSVC/Ninja/LLVM environment directly to Rocket tools.")]
        [DefaultValue(true)]
        public bool LoadPinnedEnvironment { get; set; } = true;

        [Category("Execution")]
        [DisplayName("Program arguments")]
        [Description("Arguments passed to Rocket programs after --. Quoting follows the Windows command-line rules.")]
        public string ProgramArguments { get; set; } = string.Empty;

        [Category("Execution")]
        [DisplayName("Show Rocket Output")]
        [Description("Show the dedicated Rocket Output pane when a command starts.")]
        [DefaultValue(true)]
        public bool ShowOutputPane { get; set; } = true;
    }

    internal sealed class RocketOptionsSnapshot
    {
        internal string CompilerPath { get; set; }
        internal string LanguageServerPath { get; set; }
        internal bool LoadPinnedEnvironment { get; set; }
        internal string ProgramArguments { get; set; }
        internal bool ShowOutputPane { get; set; }
    }
}
