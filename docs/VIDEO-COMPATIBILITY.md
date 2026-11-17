# Video compatibility and the BBC UK exception

Hardware decoding is requested through libmpv/FFmpeg's `v4l2m2m-copy` path and the
Qualcomm Venus decoder. Decoded frames are copied into system memory and rendered
through the existing OpenGL surface. This is not a zero-copy pipeline. Decoder
selection alone does not establish working or sustained displayed playback.

## Measured on the ThinkSmart

| Input | Result |
| --- | --- |
| H.264 High, 8-bit 4:2:0, 1080p30, level 4.1 | Hardware decode completed; native film playback also tested separately |
| HEVC Main, 8-bit 4:2:0, 1080p30 | Hardware decode completed |
| H.264 High, 1080p60, level 4.2 | Earlier probe hung during completion; not a supported production promise |
| HEVC Main10, 1080p30 | Failed on this Linux stack |
| BBC UK H.264 High, 720p50, B-frames present | A short headless hardware probe completed, but displayed hardware playback failed |

The driver advertises additional formats, including VP8/VP9, MPEG-2/4, VC-1 and
H.263. Those are not all validated by this application. Its enumerated dimensions
and profile menus are capabilities reported by a particular driver, not proof
that every combination works. No blanket 4K or Main10 support is claimed.

## BBC News UK HD

The Redge-hosted BBC UK feed repeatedly froze the device during displayed
hardware playback. Resolving its master to a single HD rendition did not cure it.
Standalone displayed mpv reproduced repeated `h264_v4l2m2m: output POLLERR`,
`capture POLLERR` and packet-consumption errors, requiring forced termination;
the OS stayed responsive in that isolated test. Earlier embedded attempts also
lost SSH. The precise kernel/firmware/FFmpeg cause is not yet established.

The app uses explicit software H.264 decoding for this provider/feed, selects a
current muxed rendition up to 1280×720, and enables software fallback for that
file. Other IPTV feeds and Jellyfin retain their hardware path. The channel is
not blocked and its saved master URL is preserved. Per-file options reset when
switching back to hardware playback, so compatibility settings do not leak into
other media.

Metadata probes on 17 November 2026 found:

| Feed | Inspected video | B-frame metadata |
| --- | --- | --- |
| BBC News UK HD | H.264 High, 1280×720, 50 fps, level 3.2 | Present |
| France 24 English | H.264 Main, 1280×720, 25 fps, level 3.1 | Present |
| Bloomberg Europe | H.264 High, 1280×720, 25 fps, level 3.1 | Present |
| Al Jazeera English | H.264 High, up to 1920×1080, 25 fps | Present |

The latter three displayed correctly on the user's device. B-frames alone
therefore do not explain the BBC failure. Its higher cadence is a useful test
difference, not proof of a frame-rate ceiling or overheating. The BBC North
America feed also worked for the user, but its metadata probe was inconclusive.
Streams and encoders may change; this table is evidence from that date.

## Open-source references and further diagnosis

- [Debian base Venus documentation](https://github.com/m0s4ik/thinksmart-view-debian/blob/main/driver/venus/README.md): the rebuilt HFI parser and a GStreamer B-frame drain hang.
- [Ubuntu ThinkSmart investigation](https://github.com/rickx/ubuntu-thinksmart/blob/main/research/video/analysis/venus-decode-findings-2026-06-05.md): older FFmpeg/Venus B-frame startup stalls. Its kernel/player path differs from ours.
- [Linux Venus source](https://github.com/torvalds/linux/tree/master/drivers/media/platform/qcom/venus) and [stateful V4L2 decoder API](https://docs.kernel.org/userspace-api/media/v4l/dev-decoder.html).
- [FFmpeg V4L2 context](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/v4l2_context.c) and [mpv](https://github.com/mpv-player/mpv).

The driver and players are open source; Qualcomm's Venus firmware is a separate
binary from the device firmware. The installed test stack was kernel
7.1.3-msm8953, FFmpeg 7.1.5, mpv 0.40.0 and patched Mesa/Freedreno 26.3.0-2.

Local `status` exposes requested/actual decoder, actual dimensions/frame rate,
cache state and a bounded set of privacy-filtered media warnings under
`video.diagnostics`. Preserve previous-boot kernel logs after a recurrence.
A useful next upstream report would include a minimal standalone reproduction,
codec/SPS metadata, the first V4L2 error and a capture of decoder queue/event
handling. Do not assume the historical B-frame reports prove the same bug, or
replace working kernel/audio/graphics packages speculatively. GPU memory dumps
and raw media logs can contain private data; review them before sharing.
