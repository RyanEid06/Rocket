using System;

namespace Rocket.VisualStudio
{
    internal static class PackageIds
    {
        internal const string PackageGuidString = "af73aab0-8e72-43c7-959b-ec59016fb30d";
        internal const string CommandSetGuidString = "5d1be1d7-213a-45ea-965f-2f73ae68aa23";
        internal static readonly Guid CommandSet = new Guid(CommandSetGuidString);

        internal const int BuildCommand = 0x0100;
        internal const int RunCommand = 0x0101;
        internal const int TestCommand = 0x0102;
        internal const int StopCommand = 0x0103;
        internal const int DebugCommand = 0x0104;
        internal const int ValidateCommand = 0x0105;
        internal const int OptionsCommand = 0x0106;
    }

    internal enum RocketAction
    {
        Build,
        Run,
        Test,
        Debug,
        Validate
    }
}
