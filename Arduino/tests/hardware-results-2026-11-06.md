# Touch-controller hardware regression — 2026-11-06

Version alignment: the `1.4.x` labels below describe unreleased local experiments,
identified by their recorded hashes. Upstream's latest release and firmware are
1.3.0; the next planned feature release and source firmware are 1.4.0. This metadata
alignment did not reflash hardware or change those historical test results.

Current local mapping: touch board **COM6**, Arduino Mini **COM4**. The measurements
below used the earlier touch-board COM4 assignment; they have not been relabeled.

Board: ESP32-2432S028-family, ESP32-D0WD-V3 revision 3.1, COM4 at 921600 baud.
Application image: touch profile, firmware 1.4.0, compiled with ESP32 core 3.3.12.
Image SHA-256: `ea4f94ca66954c9677eaa81382de91cda2633ef8bf2b0ce184a1279ecfd8d8f9`.
The mixer controller was not connected and was not hardware-tested.

Last hardware-flashed local candidate: **1.4.4**. See the final verification limits below; the
initial 1.4.0 results and subsequent failures are retained as evidence.

Layout correction: default firmware files, headers, display configuration and
assets are restored to their original locations in `Arduino/mixer`. Default C++
implementation bodies are unchanged, with only touch-exclusion guards added.
Touch support is additional `*_touch` files selected by the touch flag. The earlier
controller subdirectories were removed; native tests pass with the corrected paths.
This layout correction was not flashed or subjected to another hardware test cycle.

## Verified behavior

| Check | Evidence |
| --- | --- |
| Flash application image | esptool verified the written image hash |
| SD-backed serial smoke checks | All 14 checks passed |
| Five-channel touch routing | Events captured from channels 1–5 |
| Single-tap behavior | A second capture recorded exactly one `up` event on each channel |
| Hold-to-repeat | Repeated `down` events captured during a held minus button |
| Playback mute | One `press` event captured |
| Absolute volume | Captured values included 0.000 and 1.000 |
| Discord touch actions | One `discordmute` and one `discorddeafen` event captured |
| Layout and colors | User confirmed readable labels and correct colors |
| Screen-off/resume | Serial replies continued; user confirmed display and LED off, then display on with green LED |
| SD media transfer | Complete two-frame transfer and protocol error checks passed |
| SD media playback | User confirmed centered red/blue animation and footer |
| No-card storage fallback | All 14 smoke checks passed on repeat; flash snapshots and free-space replies worked |
| Internal-flash media transfer/playback | Transfer completed; user confirmed red/blue playback |
| Desktop audio and Discord end-to-end controls | User confirmed restored assignments, matching audio volume/mute controls and Discord controls/state icons with the current desktop build |

The initial no-card run had one storage-status timeout. Two isolated retries
returned `storage:flash` in 41–55 ms, and the complete repeated suite passed.
This transient result is retained in the raw logs.
The first capture's paired plus events were intentional double taps, confirmed
by the user; the second capture independently verified single taps.

Media error checks cover oversized dimensions, too many frames, data/end without
an upload, and ending an incomplete frame. They do not establish recovery from
power loss during a write. Snapshot checks establish successful save/load replies;
they do not compare every persisted field byte-for-byte.

## Confirmed startup defect

After powering off, reinserting SD and reconnecting USB, version, storage,
media-space and snapshot commands received no reply. The user confirmed that
the screen was frozen. A bootloader-driven reset did not restore normal serial
replies within the short query window. The board later recovered without another
reset: version replied in 212 ms and SD status in 1451 ms.

A temporary diagnostic build completed storage mounting, display initialization,
media-header loading and touch setup, then entered the first footer free-space
calculation at device time 2896 ms. It returned at 51540 ms: **48,644 ms blocked**.
Queued version queries were answered immediately afterward. This confirms a
synchronous first-time FAT free-cluster scan in `mediaStorage.freeBytes()`, invoked
from `drawStorageStatus()`, stalls the main firmware loop. It blocks touch handling,
animation and serial processing until the scan completes.

The diagnostic changes were restricted to a temporary sketch under `tmp`.
The exact normal 1.4.0 application image was restored afterward. The scan remains
in the production source; this test run is **not a full pass** and should not be
treated as release approval. This investigation does not establish whether the
defect was introduced by consolidation or already existed in the touch firmware.

After restoration and allowing the scan to finish, production firmware reported
the correct identity, mounted SD and successfully loaded the snapshot saved before
the power cycle. The current desktop build was reopened and left running.

## Follow-up fixes: firmware 1.4.1 and 1.4.2

The startup defect is fixed by a bounded background FAT counter. A shared recursive
mutex protects all foreground and worker SdFat access; writes invalidate the count.
Pending and failed counts are distinct from a known full volume. The desktop waits
asynchronously for pending upload budgets and supports cancellation.

On firmware **1.4.1**, the count completed in 54.269 seconds while serial checks
continued passing. The slowest recorded response was 1.412 seconds, rather than a
48.644-second stall. The user confirmed that touch and idle animation remained
responsive during counting. A probe using the actual desktop `SerialManager`
confirmed pending-query cancellation and budget completion, with **104/104** version
heartbeats received and a maximum inter-reply gap of **520 ms**.

Firmware **1.4.2** adds native touch media dimensions. Its startup check completed
counting in 54.405 seconds with all replies within the 2.5-second deadline (maximum
recorded response: 1.496 seconds). The actual desktop capability query returned
`320x240` on 1.4.2 and retained the `240x240` fallback on older 1.4.1 firmware.
The SD smoke checks and both 240x240 and 320x240 media upload/error suites passed.
These transfers exercise writes and playback while the counter is active.

Mixer-controller compatibility is protected separately:

- The original mixer controller modules and assets remain byte-for-byte unchanged.
- Shared protocol/profile tests require mixer media dimensions of 240x240 and reject
  320x240 for that profile. Touch accepts both existing square files and native frames.
- Encoder comparisons against the pre-change source produced byte-identical mixer
  RGB565 output and delays for landscape, portrait and square source images.
- The release workflow retains its original default setup and build commands; the default is mixer.
- Both firmware profiles and the x64 Debug desktop build compile. Local native and
  encoder tests pass; no behavioral tests were added to CI.

1.4.2 touch image SHA-256:
`7db74f768adae611216405b4d54d9bc5067971a042e73ab5ded5f8a33008452f`.
Additional raw evidence is under `tmp/free-space-fix-hardware` and
`tmp/free-space-media-*-hardware` / `tmp/free-space-media-hardware`.

## Remaining checks

### Final candidate and verification limits

The user confirmed that 1.4.2 native media fills the rectangular display. A later
actual desktop upload exposed recount starvation under continuous wide playback:
the 1.4.3 client probe returned pending after its 120-second budget deadline,
while 235/236 heartbeat replies arrived with a maximum gap of 964 ms.

1.4.4 uses sector-sized data reads, two-sector counter batches, and eight-row
incremental rendering bands. It streams pending bands without a per-band 33 ms
delay, reduces the touch scratch buffer, and permits a cancellable five-minute
budget wait. Native tests exercise actual playback code, interruption, geometry,
bounded reads and lock release before yielding; they pass. The touch build and
desktop build pass. Mixer 1.4.3 was the last embedded mixer build; the only later
mixer change is the shared version literal. Native mixer/profile tests pass.

At the user's request to limit further time/cost, the long hardware recount and
budget-wait run was **not repeated on 1.4.4**. Its scheduling change is therefore
not yet fully hardware-verified. The selected cached image was restored at
320x240 using the previously measured ample SD capacity, rather than waiting for
another full scan. That restoration does not verify the five-minute budget path.

- Actual Windows suspend/resume and USB reconnect with the desktop app running.
- Full/unreadable card, power interruption during upload, and long-duration playback.
- Physical mixer-controller regression.

Raw evidence is under `tmp/hardware-regression-20261106` (local and ignored by Git).
The selected 320x240 image has been restored on SD and the current desktop build
is running. Internal flash retains the test animation from fallback checks.
A `Regression20261106` snapshot remains on each backend.
