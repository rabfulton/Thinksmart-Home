# Third-party code and assets

The root MIT licence applies to original ThinkSmart Home code and documentation.
It does not replace the licences of imported files, downloaded components,
drivers, models or linked libraries. Preserve their notices when redistributing.

## Included source and artwork

| Files | Upstream / terms | Included notice |
| --- | --- | --- |
| `app/third_party/chess/` | Disservin chess-library 0.9.11; MIT | [LICENSE](app/third_party/chess/LICENSE), [SOURCE](app/third_party/chess/SOURCE) |
| `app/third_party/microfrontend/tensorflow/` | TensorFlow Lite Micro frontend, via pinned rhasspy/pymicro-features; Apache-2.0 | [LICENSE](app/third_party/microfrontend/LICENSE), [provenance](app/third_party/microfrontend/SOURCE.json); copyright headers retained |
| `app/third_party/microfrontend/kissfft/` | KissFFT; BSD-style licence | [LICENSE](app/third_party/microfrontend/kissfft/LICENSE) |
| `app/iptv-stations.json` | IPTV-org channel/stream metadata and links; CC0-1.0 | [IPTV-org](https://github.com/iptv-org/iptv), [API](https://github.com/iptv-org/api); no television content bundled |
| `app/assets/chess/` | Colin M. L. Burnett chess artwork; selected BSD-3-Clause terms | [LICENSE and source links](app/assets/chess/LICENSE) |

## Dependencies fetched or installed separately

- **Qt 6:** this project uses the system's Qt libraries. Observe the licences of
  the particular Qt modules/build being distributed; see [Qt licensing](https://doc.qt.io/qt-6/licensing.html).
- **Qt Virtual Keyboard:** the embedded keyboard uses this separately installed
  module under its GPLv3 or commercial terms; see the [module notices](https://doc.qt.io/qt-6.8/qtvirtualkeyboard-index.html#licenses-and-attributions).
  Our original QML keyboard style remains MIT. Preserve the installed module's
  notices and applicable source/distribution obligations when supplying binaries.
- **mpv/libmpv:** mpv defaults to GPL-2.0-or-later; an LGPL build is a separate
  build configuration with restrictions described in its [Copyright file](https://github.com/mpv-player/mpv/blob/master/Copyright).
  This application links libmpv. An MIT licence on our source does not make a
  combined executable linked to GPL libmpv MIT-only. The supplied Debian-based
  build uses the distribution package. Binary distributors must review that
  build and provide the source, notices and other materials its licences require.
- **Arasan and Fathom:** fetched at the revision pinned in
  `tools/build-chess-engine.sh`. The inspected Arasan source/network and Fathom
  have MIT notices; the installer retains their upstream `LICENSE` files.
- **Sherpa-ONNX and ONNX Runtime:** optional downloaded wake-word libraries. The
  fetcher verifies hashes and retains their Apache-2.0 and MIT licence files.
  Downloaded model archives retain their own notices and terms.
- **TensorFlow Lite:** optional runtime for a user-supplied streaming wake model;
  not bundled in this source release. Preserve its upstream terms if supplied.
- **Debian, labwc, PipeWire, WirePlumber, ALSA, BlueALSA, SpeexDSP and WebRTC audio
  processing:** installed/built separately under their respective upstream terms.
  System package copyright files are under `/usr/share/doc/<package>/copyright`.
- **Device firmware, bootloader tools, kernel and Mesa:** supplied by the device
  and upstream Debian/device projects. This release ships no firmware image or
  copied device calibration/identity. Upstream repositories retain their licences.

## Services and optional media

Weather attribution is shown in the UI (Open-Meteo, CC BY 4.0). News/feed content,
Radio Browser metadata, station streams and server artwork remain their owners'
content and are fetched at runtime. Follow the relevant services' terms.

Voice-preview recordings, personal wake recordings, personalised model weights
and demo firmware images are excluded from the public export. Users may install
their own appropriately licensed previews and models locally. The included PNG
screenshots are rendered from our UI with synthetic demonstration data.

Useful upstream projects:
[ThinkSmart Debian](https://github.com/m0s4ik/thinksmart-view-debian),
[ThinkSmart device research](https://github.com/rickx/ubuntu-thinksmart),
[Sherpa-ONNX](https://github.com/k2-fsa/sherpa-onnx),
[microWakeWord](https://github.com/OHF-Voice/micro-wake-word).

Audiobooks use LibriVox API/podcast metadata and Internet Archive audio at runtime;
Top 100 discovery uses [bstjohn.net](https://www.bstjohn.net/audiobooks/) ranking
data. A compact factual index of 100 catalogue links, book/author names, genres,
approximate durations and source ranks is bundled for offline discovery, with
attribution in `app/data/audiobooks/SOURCE`. Website code/prose, download counts,
Guardian list tags, recordings and cover art are excluded. Remote material and
source metadata are not relicensed as our MIT code. See [Audiobooks](docs/AUDIOBOOKS.md).

Cloudflare WARP is an optional, separately installed network service, governed by
Cloudflare's terms. No WARP binaries, packages, source, registration state or
account data are distributed here. Our generic Audiobooks and TV Archive local-proxy adapters are
original MIT code, compiled out unless explicitly enabled.

TV Archive fetches file metadata and programmes from the user-selected Internet
Archive collection at runtime. No programmes, artwork or remote catalogue are
bundled or relicensed as our code. See [TV Archive](docs/TV-ARCHIVE.md).
