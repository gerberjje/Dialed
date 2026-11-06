# Dialed controller firmware

## mixer-touch — The less tactile experience

**mixer-touch** is the touchscreen variant of Dialed. The current firmware targets
an ESP32-2432S028-family board with an ILI9341 display and XPT2046 resistive touch
controller. ESP32 is a requirement of the current implementation, rather than a
restriction on future mixer-touch variants.

Other ESP32 boards require matching pin assignments, display drivers and touch
configuration; they are not currently validated. Other microcontrollers require
porting the firmware's ESP32-specific components. Additional board profiles can
be supported as they are implemented and tested.

Five channel tabs, a draggable volume arc, minus/plus buttons and MUTE replace
the external knobs. Discord channels include mic mute and deafen touch targets.
The original `mixer` and `mixer_nano` sketches remain available separately.

| Sketch        | Hardware                                                                                       |
| ------------- | ---------------------------------------------------------------------------------------------- |
| `mixer`       | Original upstream ESP32 / GC9A01 controller, restored unchanged                                |
| `mixer_touch` | ESP32-2432S028-family touch board, black-background interface, SD storage and Discord controls |
| `mixer_nano`  | Original upstream classic AVR Nano / OLED controller                                           |

The instructions below target `mixer_touch`. Open
[`mixer_touch/mixer_touch.ino`](mixer_touch/mixer_touch.ino).
This ESP32 sketch cannot run on AVR boards; use `mixer_nano` for the classic
ATmega328P Nano. The Arduino Nano ESP32 is a different, ESP32-based board and has
not been validated with this touch-board pin profile.

For the Windows app overview and channel setup, see the
[main README](../README.md#usage).

## Display preview

![Touch display overview: Spotify main channel, Discord channel 2 with mic and headset controls, and original idle graphic with storage and connection status](mixer_touch/resources/preview.png)

This single image is rendered from the
[interactive touch-display preview](../docs/superpowers/prototypes/esp32-touch.html).
It shows the default black-background interface, Spotify as the main example,
Discord on channel 2, and the built-in idle screen. The music/chat symbols are
generic drawings, not Spotify or Discord logos. The values and connection state
are illustrative; this is a layout preview, not a photograph or live device feed.

## Tested hardware

| Component        | Tested board / configuration                                                           |
| ---------------- | -------------------------------------------------------------------------------------- |
| Board family     | ESP32-2432S028-family / common CYD resistive-touch profile                             |
| MCU              | ESP32-D0WD-V3, revision 3.1, dual core, 240 MHz, 40 MHz crystal                        |
| Flash            | 4 MB                                                                                   |
| Display profile  | TFT_eSPI `ILI9341_2_DRIVER`, 320 x 240 landscape, rotation 3, panel inversion enabled  |
| Touch controller | XPT2046, calibrated bounds in `mixer_touch/hardware_profile.h`                         |
| Storage          | 32 GB FAT32 microSD; mounting and free-space reporting verified on the connected board |
| USB serial       | 921600 baud; tested on COM4, which is a host-specific port assignment                  |
| PSRAM            | Not required; rendering has lower-memory fallbacks                                     |
| Arduino target   | Generic ESP32 Dev Module, `esp32:esp32:esp32`                                          |

This is the tested hardware profile, not a claim that every similarly named board
has the same panel, touch controller or wiring. Other ESP32/S3/C3 boards need a
matching hardware profile. Keep the original `mixer` sketch and its GC9A01
`User_Setup.h` separate from this touch-board configuration.

### Pin assignments

| Peripheral                 | Pins                    |
| -------------------------- | ----------------------- |
| TFT MISO / MOSI / SCLK     | 12 / 13 / 14            |
| TFT CS / DC / backlight    | 15 / 2 / 21             |
| Touch MISO / MOSI / SCLK   | 39 / 32 / 25            |
| Touch CS / IRQ             | 33 / 36                 |
| SD MISO / MOSI / SCLK / CS | 19 / 23 / 18 / 5        |
| RGB LED red / green / blue | 4 / 16 / 17, active low |

Display settings are in `mixer_touch/User_Setup.h`; touch and LED settings are in
`mixer_touch/hardware_profile.h`. SD pins are in `mixer_touch/firmware_config.h`.
The common RGB pin mapping matches
[Espressif's CYD board definition](https://github.com/espressif/arduino-esp32/blob/master/variants/jczn_2432s028r/pins_arduino.h).

## Libraries and tools

| Dependency                   | Locally tested version  | Purpose                                        |
| ---------------------------- | ----------------------- | ---------------------------------------------- |
| Arduino CLI                  | 1.5.1                   | Compile and upload                             |
| Espressif Arduino ESP32 core | 3.3.12                  | ESP32 runtime, GPIO and serial support         |
| TFT_eSPI                     | 2.5.43                  | Display, primitives, sprites and bundled fonts |
| XPT2046_Touchscreen          | 1.4.0                   | Resistive touch input                          |
| SdFat                        | 2.3.0                   | FAT32 SD storage over software SPI             |
| SPI / FS / LittleFS          | Bundled with ESP32 core | Bus and internal-flash storage support         |

The CI setup currently pins ESP32 core 3.3.10; the local builds and hardware checks
listed here used 3.3.12. Do not treat the CI pin as a separate hardware test.
No bitmap/icon package is needed: simple audio symbols use TFT_eSPI primitives,
large numbers use its bundled font, and detailed app artwork comes from the PC.
The unused local bitmap/font copies and unused white touch layout were removed.

## Supported controls

- Five touch tabs select the five hardware channels configured in Dialed.
- The volume arc sets an absolute level; bottom minus/plus buttons support hold-to-repeat.
- The bottom MUTE button controls the selected channel's playback mute.
- On a Discord channel, the microphone icon toggles Discord mic mute and the
  headset icon toggles deafen. Each contact fires once until released.
- Discord icons display confirmed state, with a slash when muted/deafened and a
  question mark when state is unknown or stale. Playback mute is independent.
- Output-device notifications and idle-media upload/playback remain supported.

The desktop app directly accesses Discord's available Windows accessibility
switches without changing foreground focus. This is not an official Discord API
integration. An unavailable or ambiguous switch is reported as unavailable.
Windows audio control still requires the Dialed desktop app.

## Idle screen, connection and power

The built-in idle graphic reuses the original mixer design: a fixed breathing
ring, three drifting dots, and centered AudioMixer / READY text. It uses the
standard black-background palette. Uploaded idle media retains playback priority.

The idle footer shows SD/free-space details on the left and Connected/Disconnected
on the right. Free space refreshes every five seconds. It is not shown over the
active mixer controls or upload screen.

| Condition                      | Display / onboard RGB LED                |
| ------------------------------ | ---------------------------------------- |
| Connected and screen on        | Green LED; normal active or idle display |
| Awake without a PC connection  | Red LED                                  |
| Screen off or PC suspended     | LED off; panel asleep and backlight off  |

There is no heartbeat or automatic screen-off timeout for missing PC commands.
Connection status reflects received PC commands and explicit screen commands;
it cannot detect a silent disconnect while USB power remains present.
`screen:off` holds the display and LED off while serial communication continues.
`screen:on` wakes it on PC resume/reconnect.
There is no separate manual connected-screen-off toggle in the desktop UI.

## Build and flash

Install the versions above and copy `mixer_touch/User_Setup.h` to the installed
TFT_eSPI library directory. Run from the repository root:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 `
  --build-property "compiler.cpp.extra_flags=-DSPI_DRIVER_SELECT=2 -DMAINTAIN_FREE_CLUSTER_COUNT=1" `
  Arduino/mixer_touch

arduino-cli upload --fqbn esp32:esp32:esp32 --port COM4 Arduino/mixer_touch
```

The SdFat flags must apply to the library as well as the sketch. Software SPI
avoids reconfiguring the buses already used by display and touch. Cached free-space
counts avoid repeatedly scanning the full FAT; the first query can take longer.

For a merged image and manifest, use PowerShell 7 and
[`tools/build-firmware.ps1`](tools/build-firmware.ps1). It defaults to `mixer_touch`
and applies that sketch's display configuration. `-SketchName mixer` builds the
original upstream target with its own configuration and without the SD flags.
The helper requires the esptool executable described in
[the flashing-tool notes](../tools/esptool/README.md).

Only the selected target is written to `firmware/esp32/manifest.json`.
`mixer_touch/version.h` is the firmware-version source. CI compiles the original
and touch targets with their respective TFT_eSPI profiles.

## SD storage and memory

Use FAT32 and power off before inserting/removing the card; hot swapping is not
supported. Firmware mounts SD first and falls back to internal LittleFS if needed.
Neither medium is automatically formatted.

| File / data                       | Purpose                                                                                         |
| --------------------------------- | ----------------------------------------------------------------------------------------------- |
| `/idle.dat`, `/idle.tmp`          | Converted idle media and upload staging                                                         |
| `/icon0.bin` through `/icon4.bin` | Current channel app artwork, 64 x 64 RGB565                                                     |
| `/p_NAME.bin`                     | Named controller snapshots, including assignment labels, colours, kinds, output names and icons |
| `/flash-copy.done`                | Records initial idle-media migration so clearing SD media does not restore an old flash copy    |

One 8 KiB icon cache stays in RAM instead of five resident icons, saving 32 KiB.
Display sprites and active rendering buffers still require RAM. SD is storage,
not additional executable RAM. Existing internal idle media is copied once when
appropriate; subsequent SD and internal-flash uploads are independent.

Idle media remains 240 x 240, centered on the 320 x 240 panel. Firmware accepts up
to 64 frames; the current desktop encoder caps at 60 frames and 20 fps. A bigger
SD card does not remove these limits or the USB upload bandwidth limit.
Playback throughput still needs physical measurement for each card.

Snapshots are device-side serial commands, not a desktop profile picker or a
standalone Windows audio controller. Names contain 1-24 ASCII letters, digits,
underscores or hyphens. Loading a snapshot can be overwritten by the next PC sync.

## Serial commands

921600 baud, newline terminated; channel IDs are one-based (`knob1` to `knob5`).

```text
ver?                     -> fw:esp32-mixer:1.0.10
storage?                 -> storage:sd:<free bytes> or storage:flash:<free bytes>
profile:save:Desktop      -> profile:ok or profile:err
profile:load:Desktop      -> profile:ok or profile:err
screen:off               -> explicit panel/backlight sleep
screen:on                -> explicit wake
```

The existing volume, mute, assignment, icon, output and GIF commands remain.
Touch sends `knobN:up`, `down`, `press` or an absolute volume. Discord icons send
`knobN:discordmute` and `knobN:discorddeafen`; PC state responses update both icons.
Set Dialed Input type to Rotary encoder and Hardware controls to 5.

## Verification and limitations

- The user confirmed the on-device touch controls and Discord mute/deafen actions.
- Live Windows tests confirmed both Discord actions work in the background without
  changing focus; the original mute/deafen states were restored after testing.
- SD mounting/free-space reporting and firmware flashing were verified on COM4.
- The default touch firmware and restored original mixer have compiled locally.
- The preview's mic/headset targets were click-tested in a hidden browser; its
  restored idle graphic and footer were visually checked.
- Preview screenshots do not establish physical font rendering, LED colour,
  SD playback speed, or behaviour on other board revisions.

After changing hardware, check touch calibration, icon colours, all channels,
SD upload/play/clear, no-card fallback, sleep/resume and LED states. A full/unreadable
card and interrupted uploads also need device testing.

## Ownership, attribution and third-party assets

This is derived from [DavideClemente/Dialed](https://github.com/DavideClemente/Dialed).
The repository's [PolyForm Noncommercial 1.0.0 license](../LICENSE) still applies;
these local changes do not grant unrestricted commercial rights or transfer
ownership of upstream work.

The documentation preview uses generic music/chat/audio drawings rather than
recreated product logos. Spotify and Discord are descriptive compatibility labels;
their names and marks belong to their respective owners. No affiliation or
endorsement is claimed. Relevant primary guidelines are
[Spotify's branding guidance](https://developer.spotify.com/documentation/design)
and [Discord's brand guidelines](https://discord.com/branding).
Runtime app icons and user-uploaded media retain their owners' rights and are not
relicensed by this project.

Reviewed dependency notices are retained under
[`mixer_touch/resources/licenses`](mixer_touch/resources/licenses): TFT_eSPI,
its Adafruit GFX/font notice, XPT2046_Touchscreen and SdFat. The ESP32 core/SDK and
bundled fonts retain their upstream terms; see also
[GNU FreeFont's license and embedding exception](https://www.gnu.org/software/freefont/license.html).
These notices are not an exhaustive SDK license inventory or legal clearance for
all uses or redistribution. Preserve applicable upstream notices and permissions.
