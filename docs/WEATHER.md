# Weather

Open the Weather card and tap **Location** to search for a town, city or postal
code. Add a country to distinguish places with the same name. Select a result
to save its coordinates and timezone and fetch its forecast immediately. No
restart, account or API key is needed. **Use installed location** restores the
location in `/etc/thinksmart/device.json`.

The card groups the current temperature and icon on the left, with conditions
and sunrise/sunset on the right. Five larger hourly forecasts appear below,
starting with the next hour rather than repeating the current conditions. The expanded view
has twelve hourly forecasts and five daily forecasts, with condition icons
throughout. Clear and partly cloudy hours distinguish day/night. Sunrise and
sunset appear on the card and prominently in the expanded forecast, in the
selected location's timezone. Missing conditions show an unavailable symbol.
All icons follow the current theme.

Forecasts update every 15 minutes, with Refresh for manual requests. Failed
requests retain the same location's cached forecast and show its age/stale state.
Switching locations cancels an old in-flight request and uses a separate cache;
forecasts from the previous place are never relabelled as the new one. If search
fails, the saved location is unchanged. Provider rate limits are respected.
Disabled Weather does not poll or search.

## Administration and AI access

The per-device override is
`/var/lib/thinksmart-home/.config/ThinkSmart/Home/weather.json`, owned by
`thinksmart`, mode 600. Example:

```json
{"name":"London","latitude":51.5074,"longitude":-0.1278,"timezone":"Europe/London"}
```

For an SSH edit, stop/restart the dashboard around the change so the displayed
forecast reloads. Removing the file restores the deployment default. Application
upgrades preserve it. Weather selection changes forecast timezone, not the OS
clock or previously saved alarm timezones.

Existing `weather.get` / `weather.refresh` tools expose the selected location,
current/hourly/daily condition codes, day/night flags, sunrise/sunset timestamps
and local labels. New assistant conversations use the saved location context.
No new voice permission or conversation is needed to edit the location by touch.

Forecast data: [Open-Meteo Forecast API](https://open-meteo.com/en/docs).
Location search: [Open-Meteo Geocoding API](https://open-meteo.com/en/docs/geocoding-api),
with GeoNames location data. Attribution remains visible in the expanded page.
