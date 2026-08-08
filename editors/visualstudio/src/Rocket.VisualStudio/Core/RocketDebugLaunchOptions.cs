namespace Rocket.VisualStudio.Core
{
    internal static class RocketDebugLaunchOptions
    {
        internal const uint CreateSuspended = 0x00000004;
        internal const uint CreateUnicodeEnvironment = 0x00000400;
        internal const uint CreateNoWindow = 0x08000000;

        // Microsoft.VisualStudio.Shell.Interop.__VSDBGLAUNCHFLAGS158. This
        // delegates console creation to Visual Studio and can open a terminal.
        private const uint IntegratedConsole = 0x08000000;

        // Microsoft.VisualStudio.Shell.Interop.__VSDBGLAUNCHFLAGS175. Despite
        // its name, Visual Studio may host this in an external Windows Terminal.
        private const uint UseIntegratedTerminalService = 0x20000000;

        internal static uint HiddenProcessCreationFlags =>
            CreateSuspended | CreateUnicodeEnvironment | CreateNoWindow;

        internal static uint ForHiddenAttach(uint flags)
        {
            return flags & ~(IntegratedConsole | UseIntegratedTerminalService);
        }

        internal static bool UsesExternalConsoleLaunch(uint flags)
        {
            return (flags & (IntegratedConsole | UseIntegratedTerminalService)) != 0;
        }

        internal static bool CreatesHiddenSuspendedProcess(uint creationFlags)
        {
            return (creationFlags & CreateSuspended) != 0 &&
                   (creationFlags & CreateNoWindow) != 0;
        }

        internal static bool IsInitialAttachBreak(string module, string function)
        {
            return (!string.IsNullOrWhiteSpace(module) &&
                    module.EndsWith("ntdll.dll", System.StringComparison.OrdinalIgnoreCase)) ||
                   (!string.IsNullOrWhiteSpace(function) &&
                    (function.IndexOf("DbgBreakPoint", System.StringComparison.OrdinalIgnoreCase) >= 0 ||
                     function.IndexOf("DebugBreak", System.StringComparison.OrdinalIgnoreCase) >= 0));
        }
    }
}
