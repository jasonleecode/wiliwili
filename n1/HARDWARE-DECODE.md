# N1 wiliwili hardware decoding

## 2026-11-01 frame and memory optimization

The retained source tree includes the N1-only `N1_LOW_MEMORY` build option.
`patches/ffmpeg-v4l2-copy-fast.patch` applies after the source-change repair.
The deployed decoder is `libavcodec-fast-20261101.so`; its SHA256 is
`acff2e427d4e09f96797e8470527f6495cda3bbbe5478fe7d98ce70dff3cc99d`.
The deployed application SHA256 is
`af6136be590f479ccded27a63fa3ebe9b8e26c7faff058d60608300a2c73bdd4`.

Playback uses `vd-lavc-o=num_capture_buffers=24,n1_copy_frames=0`. This removes
the extra FFmpeg frame-detachment copy, while retaining CPU-visible NV12 output
and the existing mpv GLES upload. It is not end-to-end DMA-BUF zero-copy.
Reducing the former 32-buffer capture pool allows old and new queues to coexist
while mpv holds a displayed frame across a seek. A six-seek held-frame regression
passed at 1080p, resuming in 0.09–0.14 seconds without progressively consuming CMA.
The optional `n1_copy_frames=1` path retains a reusable RAM buffer pool and bounded
AArch64 LDP/STP plane copy as a compatibility fallback. Keep 24 capture buffers;
the separate 20-buffer probe did not successfully decode its initial frame.

All 120 1080p output frame checksums matched software decoding. Headless decode
plus frame hashing improved from 18.7 fps to about 34 fps. The first copy-only
optimization reached 21.3 fps and was superseded by the no-detachment path.
Actual online 1080p30 playback over 60 seconds showed 1.10 output drops/second
(previously 20.56), zero decoder drops/cache pauses, and maximum absolute AV sync
error 0.0122 seconds. This is a substantial improvement, not a zero-drop claim.
The user confirmed clearly smoother playback with normal picture and sound.

The N1 image cache now has a 64 MiB RGBA accounting budget and a lower entry
limit; textures referenced by live views remain protected. Released textures
are trimmed immediately. Reused image views release prior references and cancel
old requests; cancelled UI callbacks no longer create orphaned pinned textures.
N1 cover/portrait/avatar requests use smaller images. The launcher applies a
768 MiB application scope memory limit and 64 MiB scope swap limit when the user
session bus is available. The global 512 MiB `/swapfile` remains unchanged.
The live sample used approximately 203–206 MiB cgroup memory, no swap, and left
about 788 MiB system memory available. No memory-limit or OOM event occurred.

`tests/copy-test.c` checks guarded copy boundaries and strided planes;
`tests/texture-cache-test.cpp` checks eviction/reference handling and 5000-image
churn under ASan/UBSan; `tests/seek-held-frame.c` retains the displayed frame
across decoder flushes. These short tests do not establish multi-day leak freedom.
Original installed files are in `/var/backups/n1-wiliwili-20261101-opt/` on N1.
All builds ran in the retained Mac ARM64 Docker containers.

Target: S905D / Debian 12 arm64 / Mali450 GLES2, wiliwili 1.6.0, mpv 0.35.1,
FFmpeg 5.1.9. Build on the Mac in the retained ARM64 Docker containers. Keep
`/swapfile` (512 MiB); do not compile wiliwili or FFmpeg on the N1.

## Repair

`patches/ffmpeg-v4l2-source-change.patch` applies on top of the retained N1 MPV
reset2 FFmpeg baseline (earlier patches are in `../hwdecode/`). It:

- Waits for the initial SOURCE_CHANGE event before allocating/starting CAPTURE.
- Handles SOURCE_CHANGE before POLLERR and avoids reopening an untouched decoder.
- Copies CPU-visible NV12 frames to ordinary RAM, so a retained rendered frame
  does not pin an entire old CMA capture queue across seeks and video switches.
- Limits the input backlog to 24 packets while waiting for output. A bounded
  four-second wait requests software fallback instead of feeding a stalled
  decoder indefinitely. This also keeps input within mpv's fallback replay cache.

`n1-hwdec-fallback-seek.patch` retries the requested seek once when V4L2 switches
to software decoding during a seek, allowing the software decoder to receive
its keyframe. GLES2 framebuffer and controller-slot fixes remain included.

## Build and deployment

- `build-codec.sh`: rebuild in `n1-hwdecode-workbench` on the Mac.
- `build.sh`: rebuild the application in `n1-wiliwili-builder` on the Mac.
- Installed library: `/opt/n1-wiliwili/lib/libavcodec-safe.so` with the
  `libavcodec.so.59` symlink. The launcher loads this directory ahead of
  `/opt/n1-mpv/lib`. Original MPV/Kodi libraries remain intact.
- The application/archive is `output/wiliwili-1.6.0-n1-arm64.tar.gz`; the archive
  includes the isolated codec. `configure.py` configures the N1 launcher and
  hardware H.264 defaults while preserving account data.
- `output/wili-codec-safe.bsdiff` reconstructs the library with
  `apply-codec-patch.py`, which verifies both hashes. Install a versioned file,
  swap the symlink, then restart; never overwrite a mapped library inode.

Hashes:

| Artifact | SHA256 |
| --- | --- |
| Baseline codec | `55664501d416f6271fccb820bcd154f935d1a5fbbfe5d4e42cfdceb3592c2944` |
| Repaired codec | `9695c3326709a6c684fe9ec5ae9f5cd204c878492355399fd755945bb48a0386` |
| Repaired application | `380e2b4c2cb5b961da1976beadf35116cdacb003cac921276c9d42a96f78fabd` |

## Verification

`output/hardware-validation.json` records the final regression: five loads and
five seeks, all retaining `v4l2m2m-copy` and NV12 output. It includes two 1080P
loads, 720P landscape, and two 720x1280 portrait videos, including the short clip
that previously stalled. No crash occurred during the final switching test.

A local 1080P comparison showed three seeks falling back to software before the
frame-copy repair; all three remained hardware decoded afterward. Free CMA after
seeking stayed around 78 MiB. The final online 1080P seeks took about 7-8 seconds
to resume. A manual transition to software also resumed playback after seeking.

Normal UI playback produced a correct 720P screenshot, stereo audio parameters,
and AV sync difference 0.000218 seconds. Physical TV confirmation remains pending.
Default quality is 720P; 1080P can be selected manually. No 4K/high-frame-rate or
prolonged-stability guarantee is implied. Incompatible streams can fall back to
software decoding.

For signed Bilibili URLs, IPC `loadfile` needs per-file referrer/user-agent
options. Missing headers can cause 403 responses and must not be mistaken for
decoder failures. Do not print signed URLs, cookies or full account settings.
Read bounded console-log portions, split CR/LF and cap line length: mpv terminal
status can create enormous CR-separated output. The final launcher disables
verbose playback tracing and GDB is removed after testing.

## Mouse and gamepad coexistence

`linux-mouse-gamepad-switch.patch` leaves the native pointer unlocked and
visible on Linux when gamepad/keyboard navigation is selected. GLFW also reports
ordinary mouse-position deltas, allowing movement alone to restore mouse mode.
Runtime checks after directional navigation moved the pointer outside and back
inside the window and successfully clicked a video; V4L2 playback remained active.

## Dedicated controller menu key

The MY-POWER controller's independent Start button remains joystick button 11
(Linux BTN_START 315). Its separate three-line menu button is emitted by the
Consumer Control keyboard interface as KEY_MENU 139 / XF86MenuKB. The
`controller-menu-key.patch` translates this X11 keysym into GLFW_KEY_MENU and
merges that key into BUTTON_START, opening/closing the quality selector without
changing joystick mappings. Both synthetic XF86MenuKB and the physical menu key
were verified; the user confirmed closing and reopening the quality selector.
Quality selection is provided by native Bilibili playback; cast streams do not
provide this list locally.
