using System;

namespace Dialed.Core.Services;

internal static class DiscordProcess
{
    public static bool IsSupported(string name) =>
        name.Equals("Discord", StringComparison.OrdinalIgnoreCase) ||
        name.Equals("DiscordPTB", StringComparison.OrdinalIgnoreCase) ||
        name.Equals("DiscordCanary", StringComparison.OrdinalIgnoreCase) ||
        name.Equals("DiscordDevelopment", StringComparison.OrdinalIgnoreCase);
}
