# Wi-Fi settings and on-screen keyboard

Open the settings gear, then **Wi-Fi settings**. The dialog shows nearby networks,
signal strength, security, connection status and the current IPv4 address.
Scan requests a new NetworkManager scan; ordinary refreshes use its cached data.

- Choose an available network to connect. Password entry stays local to the
  device, with show/hide and no predictive text or automatic capitalisation.
- **Hidden network** accepts a network name and either open or personal WPA
  security. Open networks need no password. Visible WPA/WPA2 personal and SAE
  networks use the security advertised by the access point and supported by the
  installed NetworkManager/driver. Enterprise and legacy security require setup
  outside this dialog.
- **Saved** lists existing NetworkManager profiles. Manage a profile to connect,
  optionally replace its personal WPA password, change automatic connection or
  adjust its priority. Higher priority is preferred when choosing an automatic
  connection; a working connection is normally retained until it drops.
- **Forget** requires confirmation and removes the saved profile/credentials.
  **Disconnect** disconnects the Wi-Fi interface. Changing Wi-Fi can interrupt SSH
  and Tailscale until connectivity returns.

Existing profile names, priorities, hidden-network flags and passwords are
preserved unless explicitly changed. The dialog reuses an existing profile for
the same network/security rather than adding duplicate profiles.

## Keyboard

The app embeds **Qt Virtual Keyboard** using UK English layout and an original
theme-aware style under `app/qml/keyboard/`. It appears when a text field gains
focus and supports letters, Shift, numbers/symbols, backspace and hiding. The
Wi-Fi form resizes above it in landscape and portrait and scrolls the focused
field into view. Other integrations can reuse the same input panel.

The GUI selects `QT_IM_MODULE=qtvirtualkeyboard` and disables the separate desktop
keyboard window before creating its Qt application. The command-line control
client and headless helper do not load the keyboard. Runtime packages are
`qml6-module-qtquick-virtualkeyboard` and `qt6-virtualkeyboard-plugin`; install
matching versions without replacing the device's Qt/graphics stack.
Qt Virtual Keyboard retains its own licence; see [third-party notices](../THIRD_PARTY.md).

## Network helper and administration

NetworkManager remains the network manager. The dashboard runs as `thinksmart`
and communicates with `/usr/local/libexec/thinksmart-network` over
`/run/thinksmart-network/control.sock`. The systemd service runs with a restricted
filesystem and accepts only root/the dashboard UID. Its allowlisted operations
use NetworkManager's D-Bus API: list/scan Wi-Fi, activate personal/open profiles,
edit connection preferences, forget a Wi-Fi profile and disconnect Wi-Fi.
It cannot run shell commands or manage non-Wi-Fi profiles. Existing secrets are
retained when preferences change, and never returned to the dashboard or logs.

Wi-Fi data is not advertised as an assistant tool and passwords do not pass
through the voice model. The helper has no network listener. The dashboard polls
connection state only while the dialog is visible, and stops when it closes or
the screen sleeps. The helper itself waits for local requests.

```sh
systemctl status thinksmart-network.service --no-pager
thinksmart-home --command wifi-settings
```

NetworkManager's existing profile files remain under
`/etc/NetworkManager/system-connections/`. No additional Wi-Fi credentials file
is introduced. `THINKSMART_WIFI_SOCKET` can override the dashboard's socket path
for local tests. The helper's `--test-directory` mode requires an owned private
directory and is intended for an isolated mock D-Bus service.
