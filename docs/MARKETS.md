# Markets

An optional native widget and detail page for a personal market watchlist. Enable
**Markets** in Settings → Pages & modules, then add its widget or full-page view.
It is disabled by default. Removing a widget does not disable its alerts; disable
the module to stop its network requests, alerts and AI capabilities. Runtime
switches do not remove code from the binary.

## Free data

No account, API key or subscription is required. This version uses Yahoo Finance's
unauthenticated chart endpoint for quotes and history. It is an unofficial,
best-effort interface: availability, rate limits and timing can change. The UI
attributes the source and never promises a guaranteed real-time feed. Quotes may
be delayed; their local quote date/time is shown. An old quote can be the correct
last close when a market is shut. Do not confuse quote age with market status.

Defaults:

| Display | Provider symbol | Instrument |
| --- | --- | --- |
| S&P 500 | `^GSPC` | S&P 500 cash index, not a broker CFD |
| DXY | `DX-Y.NYB` | ICE US Dollar Index |
| NVDA | `NVDA` | NVIDIA shares |
| BTC | `BTC-USD` | Bitcoin in USD, aggregated provider quote |
| ETH | `ETH-USD` | Ethereum in USD, aggregated provider quote |

Aliases `SPX500USD`, `SPX`, `DXY`, `BTC` and `ETH` map to those symbols. Custom
symbols use Yahoo's naming, for example `MSFT`, `^FTSE` or `VOD.L`. Unknown symbols
remain unavailable; the app does not silently substitute a different security.

While visible, refresh is approximately once a minute; with any armed alert it
continues once a minute in the background and during screen sleep. Otherwise
background refresh is every 15 minutes. At most two HTTP requests run at once;
manual refreshes are throttled, chart history is cached, and HTTP 429 backs off
for 15 minutes. Hidden charts have no animation or frame timer.

## Display and customisation

Tap the widget to open Markets. Select an instrument to see its line chart;
choose 1D, 1W, 1M or 1Y. **Focus widget / List widget** selects a persistent widget
mode. Focus uses the chosen symbol and current chart range; list shows the first
five symbols. The full page supports 1–12 ordered symbols. Tap its settings icon
to edit the comma/newline-separated list. Order in that field is display order.

Shares/indices show change versus previous close and regular-session prices.
Pre-market/after-hours status does not imply the quote includes extended-hours
trades. Crypto change is calculated against a chart observation approximately
24 hours earlier, rather than the UTC midnight close; it is unavailable if that
comparison is absent. Session labels use the provider's supplied trading periods,
including its calendar/DST handling. The app does not guess future holidays after
that calendar expires. Crypto is labelled 24/7.

The free one-minute polling interval and provider delay set the practical alert
latency; this is not a broker execution or tick-accurate monitoring service.

## Price alerts

Choose a symbol, tap **Alert**, choose crosses above/below, a price and optionally
a short chime. Alerts monitor strict threshold crossings, not merely being on one
side of a price. The first fresh quote after creation, rearming or restarting
establishes a baseline without notifying. A triggered alert is persisted as
unarmed before notification. Rearm or remove it through **Alerts**.

Only increasing quote timestamps no more than 180 seconds old can trigger, during
the regular session or crypto 24/7. Cached, stale, repeated or out-of-order quotes
never trigger; a data gap clears the baseline. Consequently a feed delayed by
more than three minutes cannot trigger alerts. Notifications wake the screen;
the optional one-shot chime respects mute and does not stop current media or
open an AI conversation. At most 32 alerts are saved. Remove armed alerts before
removing their symbol from the watchlist.

## Storage and AI

- Settings: `/var/lib/thinksmart-home/.config/ThinkSmart/Home/markets.json`.
- Replaceable quote/chart cache: `/var/lib/thinksmart-home/.cache/ThinkSmart/Home/markets.json`.
- Module instructions: `/etc/thinksmart/assistant/modules/markets/{realtime,live,backend}.md`.

Writes are atomic and private (mode 600). Invalid settings are left untouched,
with alerts disabled and an explanatory message. Ordinary upgrades preserve
watchlist, alerts and custom module instructions. No user configuration is public.

Native JSON actions are available through `thinksmartctl` and both voice APIs:

```json
{"action":"markets.get"}
{"action":"markets.get","symbol":"NVDA"}
{"action":"markets.refresh"}
{"action":"markets.focus","symbol":"BTC","range":"1W"}
{"action":"markets.watchlist","symbols":["SPX","DXY","NVDA","BTC","ETH"]}
{"action":"markets.alert.add","symbol":"NVDA","direction":"above","price":200,"chime":false}
{"action":"markets.alert.rearm","id":"ID returned by markets.get"}
{"action":"markets.alert.remove","id":"ID returned by markets.get"}
```

Data includes quote timestamps, availability/staleness, source, comparison period,
market session and saved alert state. A refresh request is not confirmation that
new data has arrived. The assistant must report actual results and qualify stale
quotes. Tests use a local HTTP fixture and mocked Realtime/Live sessions, with no
paid conversations or microphone recording.
