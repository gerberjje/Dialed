using System;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Dialed.Core.Services;

public readonly record struct DiscordVoiceState(bool? IsMuted, bool? IsDeafened)
{
    public static DiscordVoiceState Unknown => new(null, null);
    public bool IsKnown => IsMuted.HasValue && IsDeafened.HasValue;
}

/// <summary>Reads and controls Discord's real switches without keyboard input.</summary>
internal sealed class DiscordVoiceStatusService
{
    private readonly SemaphoreSlim _reader = new(1, 1);

    internal static readonly string[] MicrophoneNames =
        { "Mute", "Unmute", "Silenciar", "Ativar microfone", "Desativar microfone" };
    internal static readonly string[] DeafenNames =
        { "Deafen", "Undeafen", "Ensurdecer", "Desensurdecer", "Desativar áudio", "Ativar áudio" };

    public async Task<DiscordVoiceState> ReadAsync(string processName)
    {
        if (!DiscordProcess.IsSupported(processName) || !_reader.Wait(0))
            return DiscordVoiceState.Unknown;

        // Native UIA calls run on an MTA worker. Keep its slot until it returns,
        // even after a timeout, so a stuck provider cannot accumulate workers.
        var read = Task.Run(() =>
        {
            try
            {
                var handle = FindWindow(processName);
                if (handle == IntPtr.Zero)
                    return DiscordVoiceState.Unknown;
                using var controls = new DiscordVoiceControls(handle);
                return controls.Read();
            }
            catch { return DiscordVoiceState.Unknown; }
            finally { _reader.Release(); }
        });
        try
        {
            return await read.WaitAsync(TimeSpan.FromSeconds(2));
        }
        catch (TimeoutException) { return DiscordVoiceState.Unknown; }
    }

    public Task<bool> ToggleMuteAsync(string processName) => ToggleAsync(processName, deafen: false);

    public Task<bool> ToggleDeafenAsync(string processName) => ToggleAsync(processName, deafen: true);

    private async Task<bool> ToggleAsync(string processName, bool deafen)
    {
        if (!DiscordProcess.IsSupported(processName) ||
            !await _reader.WaitAsync(TimeSpan.FromSeconds(2)))
            return false;

        using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(2));
        var token = deadline.Token;
        var toggle = Task.Run(() =>
        {
            try
            {
                var handle = FindWindow(processName);
                if (handle == IntPtr.Zero)
                    return false;
                using var controls = new DiscordVoiceControls(handle);
                return deafen ? controls.ToggleHeadphones(token) : controls.ToggleMicrophone(token);
            }
            catch { return false; }
            finally { _reader.Release(); }
        });
        try
        {
            return await toggle.WaitAsync(token);
        }
        catch (OperationCanceledException) { return false; }
    }

    private static IntPtr FindWindow(string processName)
    {
        var processes = Process.GetProcessesByName(processName);
        try
        {
            var windows = processes.Where(p => p.MainWindowHandle != IntPtr.Zero).ToArray();
            return windows.Length == 1 ? windows[0].MainWindowHandle : IntPtr.Zero;
        }
        finally { foreach (var process in processes) process.Dispose(); }
    }

    internal static DiscordVoiceState FromSwitches(int microphones, bool? muted, int headphones, bool? deafened) =>
        new(microphones == 1 ? muted : null, headphones == 1 ? deafened : null);
}
