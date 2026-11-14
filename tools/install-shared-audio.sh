#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
for item in build/home-arm64/thinksmart-home build/shared-arm64/thinksmart-shared-audio build/shared-arm64/thinksmart-voice-audio; do test -x "$item"; done
tar -C build/shared-arm64 -cf - thinksmart-shared-audio thinksmart-voice-audio | ./tools/thinksmart-ssh 'install -d /opt/thinksmart/staging; tar -xf - -C /opt/thinksmart/staging'
tar -C build/home-arm64 -cf - thinksmart-home | ./tools/thinksmart-ssh 'tar -xf - -C /opt/thinksmart/staging'
tar -C app/deploy/shared-audio -cf - . | ./tools/thinksmart-ssh 'install -d /opt/thinksmart/staging/shared-audio; tar -xf - -C /opt/thinksmart/staging/shared-audio'
./tools/thinksmart-ssh 'sh -s' <<'REMOTE'
set -eu
command -v pipewire >/dev/null
command -v wireplumber >/dev/null
stage=/opt/thinksmart/staging/shared-audio
backup=/opt/thinksmart/shared-audio-rollback
install -d -m700 "$backup"
for source in /etc/asound.conf /usr/local/bin/thinksmart-home /usr/local/libexec/thinksmart-voice-audio; do
    name=${source##*/}
    if [ ! -e "$backup/$name" ]; then cp "$source" "$backup/$name"; fi
done
install -m755 "$stage/rollback-shared-audio" /usr/local/libexec/thinksmart-rollback-shared-audio
trap '/usr/local/libexec/thinksmart-rollback-shared-audio' EXIT
systemctl stop thinksmart-home.service
install -d /etc/thinksmart/shared-audio/wireplumber.conf.d /etc/systemd/system/thinksmart-home.service.d
install -m644 "$stage/thinksmart.conf" /etc/thinksmart/shared-audio/thinksmart.conf
cp /usr/share/wireplumber/wireplumber.conf /etc/thinksmart/shared-audio/wireplumber.conf
install -m644 "$stage/wireplumber.conf.d/10-thinksmart.conf" /etc/thinksmart/shared-audio/wireplumber.conf.d/
install -m644 "$stage"/thinksmart-*.service /etc/systemd/system/
install -m644 "$stage/home-shared-audio.conf" /etc/systemd/system/thinksmart-home.service.d/shared-audio.conf
install -m755 "$stage/thinksmart-route-audio" /usr/local/libexec/
cat "$backup/asound.conf" "$stage/alsa-extra.conf" > /etc/asound.conf
install -m755 /opt/thinksmart/staging/thinksmart-home /usr/local/bin/thinksmart-home
install -m755 /opt/thinksmart/staging/thinksmart-shared-audio /opt/thinksmart/staging/thinksmart-voice-audio /usr/local/libexec/
systemctl daemon-reload
systemctl enable thinksmart-pipewire.service thinksmart-wireplumber.service thinksmart-audio-routing.service
systemctl restart thinksmart-pipewire.service thinksmart-wireplumber.service thinksmart-audio-routing.service
touch /etc/thinksmart/shared-audio.enabled
systemctl start thinksmart-home.service
trap - EXIT
REMOTE
