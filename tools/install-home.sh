#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
test -x build/home-arm64/thinksmart-home
test -x build/home-arm64/thinksmart-power
test -x build/home-arm64/thinksmart-actions
test -x build/home-arm64/thinksmart-network
python3 tools/configure-device.py --config private/device.json --check
# Transfer only the application and its public configuration.
tar -C build/home-arm64 -cf - thinksmart-home thinksmart-power thinksmart-actions thinksmart-network | ./tools/thinksmart-ssh 'install -d -m 755 /opt/thinksmart/staging; tar -xf - -C /opt/thinksmart/staging'
tar -C app/deploy -cf - . | ./tools/thinksmart-ssh 'tar -xf - -C /opt/thinksmart/staging'
cat private/device.json | ./tools/thinksmart-ssh 'cat > /opt/thinksmart/staging/device.json'
cat tools/thinksmartctl | ./tools/thinksmart-ssh 'cat > /opt/thinksmart/staging/thinksmartctl'
cat app/radio-catalog.json | ./tools/thinksmart-ssh 'cat > /opt/thinksmart/staging/radio-catalog.json'
tar -C app -cf - themes | ./tools/thinksmart-ssh 'tar -xf - -C /opt/thinksmart/staging'
./tools/thinksmart-ssh 'sh -s' <<'REMOTE' 
set -eu
test -r /usr/lib/aarch64-linux-gnu/qt6/qml/QtQuick/VirtualKeyboard/qmldir || { echo 'Install Qt Virtual Keyboard runtime packages first (docs/INSTALL.md).'; exit 1; }
if ! id thinksmart >/dev/null 2>&1; then
    useradd --system --create-home --home-dir /var/lib/thinksmart-home --shell /usr/sbin/nologin thinksmart
fi
getent group bluetooth >/dev/null || groupadd --system bluetooth
usermod -aG audio,video,render,input,bluetooth thinksmart
install -d -m 755 /etc/thinksmart/labwc /usr/local/libexec /etc/systemd/system/bluealsa.service.d
bluetooth_enabled=$(python3 -c 'import json; print(int(json.load(open("/opt/thinksmart/staging/device.json")).get("modules",{}).get("bluetooth",True)))')
if [ "$bluetooth_enabled" = 1 ]; then
    systemctl cat bluealsa.service >/dev/null || { echo 'Bluetooth selected: install and configure BlueALSA first (docs/MODULES.md).'; exit 1; }
fi
systemctl stop thinksmart-network.service thinksmart-actions.service thinksmart-home.service thinksmart-power.service 2>/dev/null || true
install -m 755 /opt/thinksmart/staging/thinksmartctl /usr/local/bin/thinksmartctl
install -o thinksmart -g thinksmart -m 644 /opt/thinksmart/staging/device.json /etc/thinksmart/device.json
if [ ! -f /etc/thinksmart/radio-catalog.json ]; then
    install -m 644 /opt/thinksmart/staging/radio-catalog.json /etc/thinksmart/radio-catalog.json
fi
# Seed human-editable prompts without replacing SSH edits on future upgrades.
install -d -o thinksmart -g thinksmart -m 700 /etc/thinksmart/assistant /etc/thinksmart/assistant/personalities
for source in /opt/thinksmart/staging/assistant/*.md /opt/thinksmart/staging/assistant/*.json /opt/thinksmart/staging/assistant/personalities/*.md /opt/thinksmart/staging/assistant/modules/*/*.md; do
    [ -f "$source" ] || continue
    relative=${source#/opt/thinksmart/staging/assistant/}
    target=/etc/thinksmart/assistant/$relative
    if [ ! -e "$target" ]; then
        install -d -o thinksmart -g thinksmart -m 700 "$(dirname -- "$target")"
        install -o thinksmart -g thinksmart -m 600 "$source" "$target"
    fi
done
install -m 755 /opt/thinksmart/staging/thinksmart-home /usr/local/bin/thinksmart-home
# Keep personal notes, schedules, routines and SSH-edited palettes on upgrades.
install -d -o thinksmart -g thinksmart -m 700 /var/lib/thinksmart-home/personal /var/lib/thinksmart-home/personal/themes
for source in /opt/thinksmart/staging/themes/*.json; do
    target=/var/lib/thinksmart-home/personal/themes/${source##*/}
    if [ ! -e "$target" ]; then
        install -d -o thinksmart -g thinksmart -m 700 "$(dirname -- "$target")"
        install -o thinksmart -g thinksmart -m 600 "$source" "$target"
    fi
done
install -m 755 /opt/thinksmart/staging/thinksmart-actions /usr/local/libexec/thinksmart-actions
install -m 644 /opt/thinksmart/staging/thinksmart-actions.service /etc/systemd/system/thinksmart-actions.service
install -m 755 /opt/thinksmart/staging/thinksmart-network /usr/local/libexec/thinksmart-network
install -m 644 /opt/thinksmart/staging/thinksmart-network.service /etc/systemd/system/thinksmart-network.service
install -d -m 755 /usr/local/share/thinksmart/voice-previews
for preview in /opt/thinksmart/staging/voice-previews/*.wav; do
    [ -f "$preview" ] || continue
    install -m 644 "$preview" /usr/local/share/thinksmart/voice-previews/
done
install -m 755 /opt/thinksmart/staging/thinksmart-power /usr/local/libexec/thinksmart-power
install -m 755 /opt/thinksmart/staging/thinksmart-session /usr/local/libexec/thinksmart-session
install -m 644 /opt/thinksmart/staging/labwc/rc.xml /etc/thinksmart/labwc/rc.xml
# Preserve locally configured station addresses on later application upgrades.
if [ ! -f /etc/thinksmart/stations.json ]; then
    install -m 644 /opt/thinksmart/staging/stations.json /etc/thinksmart/stations.json
fi
install -m 644 /opt/thinksmart/staging/thinksmart-home.service /etc/systemd/system/thinksmart-home.service
install -m 644 /opt/thinksmart/staging/thinksmart-power.service /etc/systemd/system/thinksmart-power.service
install -m 644 /opt/thinksmart/staging/bluealsa.service.d/thinksmart.conf /etc/systemd/system/bluealsa.service.d/thinksmart.conf
systemctl daemon-reload
if [ "$bluetooth_enabled" = 1 ]; then
    systemctl enable bluetooth.service bluealsa.service
    systemctl restart bluealsa.service
fi
systemctl enable seatd.service thinksmart-power.service thinksmart-home.service thinksmart-actions.service thinksmart-network.service
systemctl set-default graphical.target
systemctl disable --now getty@tty1.service
systemctl start thinksmart-home.service
systemctl start thinksmart-actions.service
systemctl restart thinksmart-network.service
REMOTE
