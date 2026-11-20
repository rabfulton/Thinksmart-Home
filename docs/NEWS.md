# News sources and columns

Open **Settings → Pages & modules → News → Edit**. News can be a launcher widget,
a full-page view, or available on request; all use the same source settings.

Choose one or two columns, and a source for each. A single column fills the page;
two sit side by side in landscape and stack vertically in portrait. **Save news
settings** applies immediately, without restarting the app. Cancel discards the
news draft. Other page/module changes still use the page editor's Apply button.
These preferences are shared by all placements of the News module.

Existing installs keep their chosen Google News section in the first column and
Hacker News in the second. The initial default is **World & finance / Hacker
News**. **Defaults** restores that pair in the draft; save to apply it. A source
cannot occupy both columns. Enabling News and placing a News page are separate
from choosing its sources.

## Included sources

The Google choices retain the existing topic feeds. Publisher feeds are public
RSS endpoints checked during development; no account or API key is required.
Availability can change. The reader shows feed summaries, not a full-article web
browser or a subscription bypass.

| Choice | Feed/source |
| --- | --- |
| World & finance | Existing alternating Google News international and global-finance mix |
| International | [Google News world](https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-GB&gl=GB&ceid=GB:en) |
| Global finance | Existing Google News search for global markets, the world economy, central banks and bonds |
| Business | [Google News business](https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-GB&gl=GB&ceid=GB:en) |
| Technology | [Google News technology](https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-GB&gl=GB&ceid=GB:en) |
| Science | [Google News science](https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=en-GB&gl=GB&ceid=GB:en) |
| BBC World | [BBC](https://feeds.bbci.co.uk/news/world/rss.xml) |
| BBC UK | [BBC](https://feeds.bbci.co.uk/news/uk/rss.xml) |
| BBC Scotland | [BBC](https://feeds.bbci.co.uk/news/scotland/rss.xml) |
| BBC Business | [BBC](https://feeds.bbci.co.uk/news/business/rss.xml) |
| BBC Technology | [BBC](https://feeds.bbci.co.uk/news/technology/rss.xml) |
| Guardian World | [The Guardian](https://www.theguardian.com/world/rss) |
| Guardian Business | [The Guardian](https://www.theguardian.com/business/rss) |
| Guardian Environment | [The Guardian](https://www.theguardian.com/environment/rss) |
| Al Jazeera English | [Al Jazeera](https://www.aljazeera.com/xml/rss/all.xml) |
| France 24 English | [France 24](https://www.france24.com/en/rss) |
| NPR World | [NPR](https://feeds.npr.org/1004/rss.xml) |
| CNBC World | [CNBC](https://www.cnbc.com/id/100727362/device/rss/rss.html) |
| Ars Technica | [Ars Technica](https://feeds.arstechnica.com/arstechnica/index) |
| NASA | [NASA](https://www.nasa.gov/feed/) |
| New York Times World | [The New York Times](https://rss.nytimes.com/services/xml/rss/nyt/World.xml) |
| Hacker News | [Official API](https://github.com/HackerNews/API), including native story comments/replies |

Only selected feeds update. The daily refresh and per-column **Update** buttons
remain; hiding the page does not prevent a selected feed's daily cache update.
Disabling News stops feed work. Existing cached headlines are retained on network
failure, and each source has a separate cache. Articles are attributed to their
publisher; some feeds provide only a short description. Hacker News continues to
open its comment thread rather than an embedded browser.

## Administration and extension

Preferences: `/var/lib/thinksmart-home/.config/ThinkSmart/Home/news.json` (mode 600).
The versioned file is written atomically and preserved on upgrades:

```json
{"version":1,"columns":["WORLD_FINANCE","HN"]}
```

Set `THINKSMART_NEWS_FILE` for an isolated fixture/profile. Invalid configuration
is left untouched and automatic feed work pauses, with an error in the editor.
Repair the file over SSH and restart the dashboard. Cache data stays under
`~thinksmart/.cache/ThinkSmart/Home/news/`, separate from preferences. The legacy
Google section in `Home.conf` is used only when `news.json` is absent.

The curated source IDs and URLs live in `app/newsfeeds.h`. New publisher feeds
need an explicit entry, a compatible RSS response and a rebuild; arbitrary URLs
are not accepted in the preference file or UI. Keep publisher attribution and
plain-text summary rendering. Add local fixtures for new feed formats and retain
one/two-column, disabled-feed, cache and touch-scrolling checks. See
[DEVELOPMENT.md](DEVELOPMENT.md) for the offline test workflow.
