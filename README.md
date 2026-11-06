# Dialed

> A hardware-controlled, per-application volume mixer for Windows.

Dialed pairs a small physical controller (an ESP32/Arduino with knobs or rotary
encoders, a push switch, and a round display) with a Windows desktop app so you can
set the volume of individual apps — Spotify, a game, your call — by turning a real
knob, mute with a press, and flip your default output device with a switch. The
controller's round display shows the app icon and volume of whatever you're adjusting.

<!-- Add screenshots to docs/images/ and reference them here, e.g.: -->
<!-- ![Dialed mixer window](docs/images/mixer.png) -->

## Features

- Per-app **and** master volume control from physical knobs / rotary encoders
- Mute an app with a knob press
- Switch the Windows default output device from a hardware switch (or in-app)
- Live app icons + volume mirrored to the controller's round GC9A01 display
- Idle-screen GIF library uploaded to the controller's flash
- Runs from the system tray; optional start-with-Windows
- English and Portuguese (PT-PT) localization

## Microphone and catch-all controls

In a channel's app picker, select **Microphone** to control the default Windows
microphone's input volume and mute, or **Other Apps (Catch-all)** to control detected
apps that are not assigned to other knobs. The catch-all excludes the microphone,
system master volume, and hidden apps. App detection uses the default playback
device; the catch-all does not span every output device or record audio. Its displayed
volume and mute reflect the first eligible app; adjusting it applies to all eligible apps.

The **System** channel controls the master volume of the default playback device,
so changing it affects every app playing through that device. Each app also has its
own volume level, which works together with the system volume. For example, if you
launch a browser and play a video, its sound is subject to the current system volume
and the browser's own volume setting, which Windows may remember from earlier use.

Use **Other Apps (Catch-all)** when you want those unassigned apps quieter or louder
without changing the system volume or the apps on dedicated channels. For example,
keep your game and music player on their own knobs, then turn down the catch-all
knob to lower a browser video's volume along with other detected, unassigned apps.
Assign the browser to its own channel if you want to adjust it separately from that
group.

Newly detected apps join the catch-all group, but they keep their current volume
until you adjust the catch-all control. The catch-all is a control for the group's
current app volumes; it does not set a default volume for applications launched later.

Microphone discovery currently requires an active playback device as well as a
default microphone. Test microphone and catch-all volume/mute with the controller
connected before publishing.

## Hardware

You need the companion controller: an ESP32 (or Arduino Nano variant) with
potentiometers or rotary encoders, a push switch, and a round GC9A01 display,
connected over USB serial. Wiring, the serial protocol, and the firmware sketches
live in **[`Arduino/README.md`](Arduino/README.md)**.

## Installation

1. Go to the [**Releases**](../../releases) page.
2. Download the latest `Dialed-Setup-<version>-x64.exe`.
3. Run it and follow the installer.

> **SmartScreen note:** the installer is not code-signed, so Windows SmartScreen may
> show *"Windows protected your PC"*. Click **More info → Run anyway** to proceed.
> (Code signing requires a paid certificate and isn't set up for this project.)

Requires 64-bit Windows 10 version 1809 (build 17763) or newer.

## Usage

1. Plug in the controller over USB.
2. Launch Dialed; open **Settings** and pick the controller's COM port.
3. On the mixer page, assign an app to each knob.
4. Turn a knob to set that app's volume; press to mute; use the switch to change the
   output device.

## mixer-touch

> The less tactile experience.

**mixer-touch** is a touchscreen controller variant for Dialed. The current target
is an ESP32-2432S028-family board with an ILI9341 320 x 240 landscape display and
XPT2046 resistive touch. Other boards require matching hardware configuration or
firmware porting and have not been validated.

Touch support is additional `*_touch.cpp` / `*_touch.h` files inside `Arduino/mixer`.
The existing mixer files and default build remain in place. Build touch firmware
explicitly with `./Arduino/tools/build-firmware.ps1 -Controller touch`; build the
companion desktop app from this branch. Published upstream releases do not include
these local additions.

For touch use, select the controller's COM port, set **Input type** to **Rotary encoder**
and **Hardware controls** to **5**, then assign apps to the channels. Tap a channel
and drag its volume arc or use minus/plus; tap **MUTE** for playback mute. Discord
channels also offer mic mute and deafen controls. Change the output device in the app.

Touch media fills the native 320 x 240 panel, cropping only when the source aspect
ratio differs. Mixer and older firmware retain their existing 240 x 240 behavior.
SD storage has internal-flash fallback. See [firmware setup and limitations](Arduino/README.md#touch-build-and-flash).

## Build from source

Prerequisites: **.NET 8 SDK** and the **Windows App SDK** tooling (Visual Studio 2022
with the "Windows App SDK" / WinUI workload, or the standalone SDK).

```powershell
dotnet build Dialed.csproj -p:Platform=x64 -c Debug
```

`-p:Platform` is **required** — there is no `AnyCPU` platform (valid values: `x64`,
`x86`, `ARM64`). The app is self-contained, so builds bundle the Windows App SDK
runtime and need a matching `RuntimeIdentifier` (derived automatically from the
platform).

To produce a release publish folder:

```powershell
dotnet publish Dialed.csproj -c Release -p:Platform=x64 -p:PublishProfile=win-x64
```

## Architecture

Dialed is a single MVVM WinUI 3 app. NAudio's Core Audio API applies per-app volumes
to Windows audio sessions; a symmetric, stringly-typed serial protocol talks to the
controller firmware. For a full tour of the code, see
**[`CLAUDE.md`](CLAUDE.md)**.

## Releasing

This branch targets **v1.4.0**, following upstream's published v1.3.0. App versions
continue to come from the release tag; firmware versions remain independently
tracked in `Arduino/mixer/version.h`. No tag or release is created by this plan.

Push a version tag and GitHub Actions builds the installer and publishes a Release:

```bash
git tag v1.4.0
git push origin v1.4.0
```

The [`Release`](.github/workflows/release.yml) workflow publishes the x64
self-contained build, compiles the Inno Setup installer, and attaches
`Dialed-Setup-<version>-x64.exe` to the GitHub Release.

## Contributing

Contributions are welcome — issues and pull requests both. By contributing you agree
your changes are licensed under the project's non-commercial license (below).

## Support

If Dialed's been useful to you, consider buying me a coffee:

[![Buy Me A Coffee](https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png)](https://www.buymeacoffee.com/davideclemente)

## License

Licensed under the **[PolyForm Noncommercial License 1.0.0](LICENSE)**. In short: you
may use, modify, and share Dialed for any **non-commercial** purpose, but you may not
sell it or use it for commercial advantage. See [`LICENSE`](LICENSE) for the full
terms.

## Formatting

Shared defaults are in `.editorconfig`: spaces, UTF-8, LF line endings, a final newline,
and a 100-column guideline. C# and XAML use four spaces; C++ and web files use two.

Use `dotnet format whitespace Dialed.csproj --no-restore` for C#, `clang-format`
with `.clang-format` for shared and touch firmware, and Prettier with `.prettierrc.json` for
HTML, embedded CSS/JavaScript, JSON and YAML. The prototypes are source files and
should remain expanded, rather than minified. Preserve generated bitmap/font data
and the default `mixer` implementation and `mixer_nano` target when doing bulk formatting.
