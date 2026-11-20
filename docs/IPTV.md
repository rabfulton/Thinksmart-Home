# IPTV

Choose **IPTV** under **Settings → Pages & modules → Add application**. Place a
widget on a page with a free slot, or choose a dedicated full-page view. Apply & restart. It is
an optional module, disabled by default; upgrading preserves existing layouts.
No account or server is required. The TV widget opens the browser; use Back to
return to its widget page. Cards keep their normal size even on a sparse page.

The browser has one toolbar: Back, a Saved/Suggested/All channels selector,
search, filters and an options menu. Country/category filters open in a popup;
Add stream and Refresh directory are in the options menu. Typing a search
switches to All channels. Press Done to close the keyboard and view the results.
The top bar stays visible while browsing, with the clock, page tabs and device
controls. The media footer stays hidden. Playback uses the fullscreen player.

**Suggested** starts with seven English-language news/business channels: France 24,
DW, NHK World-Japan, Bloomberg, Euronews, Africanews and Al Jazeera. **All channels**
searches the cached [IPTV-org catalogue](https://github.com/iptv-org/api) by name,
country and category. Tap the star to save a channel; **Saved** provides quick
access on subsequent visits and after restarting the device. **Add stream** accepts
a name and direct HTTP(S) stream URL using the on-screen keyboard.

The channel directory refreshes when opened or queried after 24 hours, with a
manual Refresh button. It combines channel and stream metadata, keeps one stream
per channel/feed and favours the starter URLs, broadcaster domains and 720p.
It excludes channels marked adult, closed channels, streams above 1080p and links
requiring special HTTP headers. Only the first 30 results are displayed initially;
More channels loads the next batch. Saved channels are independent of the cache.

Stream availability varies, including regional restrictions. Directory inclusion
is not a playback check. The starter manifests were checked when added; any link
can subsequently stop working. Refresh can update directory URLs without losing
saved channel identities. A failed refresh leaves starter, cached and saved
channels available. Custom stream URLs are never replaced by a directory refresh.

Playback fills the screen using the existing **libmpv** surface and hardware
H.264/HEVC decoding where supported by the installed driver. Tap to show/hide
controls; Back stops TV and returns to the channel browser. Live controls omit
seeking and the film timeline. Overall speaker volume, physical volume keys and
shared assistant audio use the existing media routing. No EPG, recording, DRM,
browser-only players or catch-up service integration is included.

## Assistant and local control

Enabled IPTV adds `iptv.list`, `iptv.search`, `iptv.status`, `iptv.play`, `iptv.save`
and `iptv.remove` to the device_action tool in Realtime and Live. Search accepts
`search`, `country` (ISO two-letter code), and `category`, normally returning five
matches. Play/save/remove take a returned `id`. Use `media.status`, `media.stop`
and `media.volume` for playback. Tool results contain IDs and channel metadata,
not URLs or credentials. Playback acceptance does not prove a stream is playing.

```sh
thinksmartctl '{"action":"iptv.search","country":"JP","category":"news"}'
thinksmartctl '{"action":"iptv.list"}'
thinksmartctl '{"action":"iptv.play","id":"ID_FROM_SEARCH"}'
```

The assistant policy is editable over SSH in
`/etc/thinksmart/assistant/modules/iptv/{realtime,live,backend}.md`. Disabling IPTV
stops directory requests and removes its tools and module policy on restart.

## Files

Owned by `thinksmart`, with JSON files mode 600:

- Saved channels and last selection:
  `/var/lib/thinksmart-home/.local/share/ThinkSmart/Home/iptv/favourites.json`
- Replaceable catalogue cache:
  `/var/lib/thinksmart-home/.cache/ThinkSmart/Home/iptv/catalogue.json`

Back up favourites separately; a corrupt favourites file is left untouched and
editing is refused until repaired. Custom links can contain private tokens, so
keep this file out of public exports. The cache and seven bundled starter entries
contain IPTV-org metadata/links under CC0; the streams remain broadcaster content.
See [third-party notices](../THIRD_PARTY.md).

## Playback compatibility

The directory's quality label describes its entry, not necessarily the adaptive
variant playing. The local control socket's `status` response includes
`video.diagnostics` with actual video dimensions, codec and container frame rate,
alongside the decoder name. A reported decoder alone is not proof of sustained
stable playback.

The BBC News UK HD feed's multi-variant master triggered whole-device freezes on
the tested ThinkSmart. Its direct 1280×720, 50 fps H.264 rendition completed an
isolated hardware decode check, including B-frames. The client resolves that
provider's current muxed HD rendition before handing it to libmpv. Displayed
hardware playback still failed after that change. That provider/feed now uses
an explicit software H.264 decoder, up to 720p, while other feeds retain hardware
decoding. Software fallback is enabled only for this compatibility path. Saved links remain the stable
master URL; the rendition is selected afresh on each play. Back/Stop cancels a
pending selection. No channel block is applied. Favourite stars update native model rows instead
of resetting the list, preserving scroll position.

This is a targeted compatibility change; the full cause of the freezes remains
unconfirmed. For driver background see the Debian base's
[Venus notes](https://github.com/m0s4ik/thinksmart-view-debian/blob/main/driver/venus/README.md)
and the Ubuntu project's [B-frame investigation](https://github.com/rickx/ubuntu-thinksmart/blob/main/research/video/analysis/venus-decode-findings-2026-06-05.md).
Their observations differ by kernel/player path; neither is proof of a blanket
hardware B-frame limitation. Capture previous-boot kernel logs after a recurrence.

The kernel-side driver is open source under
[Linux drivers/media/platform/qcom/venus](https://github.com/torvalds/linux/tree/master/drivers/media/platform/qcom/venus),
with a patched `venus-core` in the Debian base. The
[stateful V4L2 decoder API](https://docs.kernel.org/userspace-api/media/v4l/dev-decoder.html)
documents buffering, format changes and draining. Qualcomm's Venus firmware is
a separate binary loaded from the original device firmware. These are distinct
from Mesa/Freedreno, which renders decoded frames through OpenGL.

A standalone displayed hardware test reproduced `h264_v4l2m2m` output/capture
`POLLERR` errors and needed forced termination while the OS stayed reachable.
That is concrete evidence for a decoder/driver-path investigation, but does not
yet isolate a specific kernel, firmware or FFmpeg defect. The short headless
hardware success should not be interpreted as proof of displayed stability.

See [VIDEO-COMPATIBILITY.md](VIDEO-COMPATIBILITY.md) for measured formats,
BBC-versus-working-feed metadata and the current driver investigation.
