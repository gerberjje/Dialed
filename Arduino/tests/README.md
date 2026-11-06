# Local firmware tests

Run from the repository root with Python 3 and a native C++17 compiler:

```powershell
python Arduino/tests/run_tests.py
# Or select a compiler explicitly:
python Arduino/tests/run_tests.py --compiler C:/path/to/zig.exe
```

The runner supports g++, clang++ and Zig, builds in a temporary directory, and
returns a nonzero exit code on compilation or test failure. No Arduino libraries,
connected board or Python packages are required. These tests are local only;
the firmware CI workflow is unchanged.

`touch_tests.cpp` compiles the real `mixer/knobs_touch.cpp` and its `touch_geometry.h`
against minimal serial, clock, SPI and touchscreen substitutes. It checks:

- Every screen pixel's button hit target, with and without Discord controls.
- Separation between Discord touch targets and the volume arc.
- Arc endpoints, midpoint, interior, outer boundary and bottom gap.
- Serial events for volume, playback mute, Discord mute and deafen.
- Hold-to-repeat timing and suppression of repeated playback mute while held.
- One Discord action per contact, including sliding between controls.
- Wake-contact consumption and rearming after release.
- Absolute-volume duplicate filtering and drag isolation from buttons.
- Channel selection, host-selected channels, inactive tabs and zero channels.
- Rejection of low-pressure contacts.

`protocol_tests.cpp` compiles the shared `mixer/mixer.ino` serial loop twice, with
the default mixer profile and the touch profile. It checks profile identities,
channel limits, boot/version replies, fragmented and CRLF input, volume/mute
routing, invalid channel rejection, output/configuration commands, GIF-handler
precedence, screen-off/resume and ignored commands. Touch checks also cover silent
state updates, Discord state validation, enabling Discord, and storage replies.
The storage replies use a substitute backend, not an SD card.

`free_space_tests.cpp` checks sector-sized scan budgets, exact counts, reserved
clusters and end-of-chain handling, invalidation during writes, read failure and
recovery, known full/empty volumes and 64-bit byte totals. Shared protocol tests
also cover pending/unavailable storage replies without blocking version queries.

`idle_playback_tests.cpp` compiles the actual touch media engine with simulated
storage/display boundaries. It verifies square/native positioning, one-band-per-loop
rendering, sector-sized reads with the lock released before yielding, frame delays,
static-image completion, oversized persisted files and cancellation mid-frame.

These tests do not establish physical storage/display behavior, assignment/icon
parsing, snapshot persistence, power management or LED output. Simulated
touch input also cannot establish calibration or reliability on a physical board.
Use the hardware checklist in the [firmware README](../README.md#verification-and-limitations)
for device verification.

## Windows media encoder checks

```powershell
dotnet run --project Arduino/tests/encoder/encoder-tests.csproj
```

These local checks use the real desktop encoder and test landscape, portrait and
square sources. They cover native 320 x 240 fill, RGB565 packing, payload sizes,
and the mixer controller's retained 240 x 240 square center crop. Profile tests
also enforce controller-specific size limits and the unchanged scratch-buffer
budget. The release workflow explicitly selects the mixer controller profile.

## Connected touch-board checks

The current local touch board is on **COM6**; **COM4 is the Arduino Mini**.
Do not use COM4 for touch tests or touch flashing.

Close Dialed and any serial monitor before using `test-touch-hardware.ps1`.
Flash the compiled **touch** profile first; the script refuses another firmware
identity. Run from the repository root:

```powershell
./Arduino/tests/test-touch-hardware.ps1 -Port COM6 -Phase Startup
./Arduino/tests/test-touch-hardware.ps1 -Port COM6 -Phase Smoke
./Arduino/tests/test-touch-hardware.ps1 -Port COM6 -Phase Capture -CaptureSeconds 45
./Arduino/tests/test-touch-hardware.ps1 -Port COM6 -Phase Sleep
./Arduino/tests/test-touch-hardware.ps1 -Port COM6 -Phase Media
./Arduino/tests/test-touch-hardware.ps1 -Port COM6 -Phase Media -MediaWidth 320 -MediaHeight 240
```

Startup must run immediately after a reset with saved idle media present. It
checks version, media-space and storage replies against a 2.5-second deadline
while counting is pending, then checks that the count completes. Smoke accepts an
explicit pending count as a responsive backend, rather than treating it as full.

Smoke checks write a `Regression20261106` snapshot and replace the board's active
assignments with five test channels. Media checks **clear existing idle media**
and upload a two-frame red/blue animation. Reopen Dialed to resync its assignments;
upload your preferred idle media again after testing. The test snapshot is retained
on the tested storage backend. JSONL evidence is written to `-OutputDirectory`.

Capture records raw events with elapsed timestamps; it requires a person to operate
the controls and review the display. Sleep needs visual confirmation of the display
and LED. Neither phase treats missing human observations as a pass.

For fallback checks, unplug USB, remove the SD card, then reconnect USB and run
Smoke with `-ExpectedStorage flash`. Power off before reinserting the card.
Do not run the mixer profile on the touch board.
