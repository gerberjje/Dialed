using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using UIA = Interop.UIAutomationClient;

namespace Dialed.Core.Services;

/// <summary>
/// One native UIA session, confined to the calling MTA thread. Managed UIA's
/// action methods automatically focus their target; native UIA allows disabling
/// that behavior so touching a voice-control icon does not activate Discord over a game.
/// </summary>
internal sealed class DiscordVoiceControls : IDisposable
{
    private readonly List<object> _objects = new();
    private readonly UIA.IUIAutomation _automation;
    private readonly UIA.IUIAutomationElement _window;

    public DiscordVoiceControls(IntPtr handle)
    {
        try
        {
            _automation = Track<UIA.IUIAutomation>(new UIA.CUIAutomation8());
            var options = (UIA.IUIAutomation2)_automation;
            options.AutoSetFocus = 0;
            options.ConnectionTimeout = 1000;
            options.TransactionTimeout = 1000;
            _window = Track(_automation.ElementFromHandle(handle));
        }
        catch { Dispose(); throw; }
    }

    public DiscordVoiceState Read()
    {
        var controls = Find(DiscordVoiceStatusService.MicrophoneNames.Concat(DiscordVoiceStatusService.DeafenNames).ToArray());
        bool? muted = null, deafened = null;
        int microphones = 0, headphones = 0;
        for (int i = 0; i < controls.Length; i++)
        {
            var control = Track(controls.GetElement(i));
            var toggle = GetToggle(control);
            if (toggle is null)
                continue;
            bool? state = (int)toggle.CurrentToggleState == 2 ? null : (int)toggle.CurrentToggleState == 1;
            if (DiscordVoiceStatusService.MicrophoneNames.Contains(control.CurrentName, StringComparer.OrdinalIgnoreCase))
            {
                microphones++;
                muted = state;
            }
            else
            {
                headphones++;
                deafened = state;
            }
        }
        return DiscordVoiceStatusService.FromSwitches(microphones, muted, headphones, deafened);
    }

    public bool ToggleMicrophone(CancellationToken token) =>
        Toggle(DiscordVoiceStatusService.MicrophoneNames, token);

    public bool ToggleHeadphones(CancellationToken token) =>
        Toggle(DiscordVoiceStatusService.DeafenNames, token);

    private bool Toggle(string[] names, CancellationToken token)
    {
        var controls = Find(names);
        UIA.IUIAutomationTogglePattern? target = null;
        for (int i = 0; i < controls.Length; i++)
        {
            var toggle = GetToggle(Track(controls.GetElement(i)));
            if (toggle is null)
                continue;
            if (target is not null)
                return false; // ambiguous: never act
            target = toggle;
        }
        if (target is null || (int)target.CurrentToggleState == 2)
            return false;
        token.ThrowIfCancellationRequested();
        target.Toggle();
        return true;
    }

    private UIA.IUIAutomationElementArray Find(string[] names)
    {
        var conditions = names.Select(name => Track(_automation.CreatePropertyConditionEx(
            UIA.UIA_PropertyIds.UIA_NamePropertyId, name, (UIA.PropertyConditionFlags)1))).ToArray();
        var condition = Track(_automation.CreateOrConditionFromArray(conditions));
        return Track(_window.FindAll(UIA.TreeScope.TreeScope_Descendants, condition));
    }

    private UIA.IUIAutomationTogglePattern? GetToggle(UIA.IUIAutomationElement control)
    {
        if (control.CurrentIsEnabled == 0)
            return null;
        try
        {
            var pattern = control.GetCurrentPattern(UIA.UIA_PatternIds.UIA_TogglePatternId);
            return pattern is UIA.IUIAutomationTogglePattern toggle ? Track(toggle) : null;
        }
        catch (COMException) { return null; }
    }

    private T Track<T>(T value) where T : class
    {
        if (!_objects.Any(item => ReferenceEquals(item, value)))
            _objects.Add(value);
        return value;
    }

    public void Dispose()
    {
        for (int i = _objects.Count - 1; i >= 0; i--)
            if (Marshal.IsComObject(_objects[i]))
                Marshal.ReleaseComObject(_objects[i]);
        _objects.Clear();
    }
}
