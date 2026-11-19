#!/usr/bin/env python3
"""Optional, on-device appliance setup. Run from a staged repository as root.

Explicit flags select logging and/or an already registered WARP proxy. Each
installation saves a private rollback and arms a five-minute rollback timer;
cancel it only after checking remote access. No vendor software is installed.
"""
# SPDX-License-Identifier: MIT
import argparse
import datetime
import json
import os
from pathlib import Path
import re
import shutil
import subprocess


def command(*args, check=True):
    result = subprocess.run(args, capture_output=True, text=True, timeout=60)
    if check and result.returncode:
        raise RuntimeError(f"Command failed: {args[0]} {args[1] if len(args) > 1 else ''}")
    return result


def rollback(backup):
    manifest = json.loads((backup / "manifest.json").read_text())
    if manifest["archive"]:
        command("systemctl", "disable", "--now", "thinksmart-archive-proxy.socket", check=False)
        command("systemctl", "stop", "thinksmart-archive-proxy.service", check=False)
        command("systemctl", "stop", "warp-svc.service", check=False)
    for item in manifest["files"]:
        target = Path(item["path"])
        if item["existed"]:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(backup / "files" / target.relative_to("/"), target)
        else:
            target.unlink(missing_ok=True)
    command("systemctl", "daemon-reload")
    # Remove only the enable links introduced by this installer.
    if manifest["archive"]:
        command("systemctl", "reset-failed", "thinksmart-archive-proxy.service", check=False)
        command("systemctl", "start", "warp-svc.service")
        import time
        for attempt in range(20):
            if command("warp-cli", "--accept-tos", "proxy", "port", str(manifest["oldPort"]), check=False).returncode == 0:
                break
            time.sleep(0.25)
        else:
            raise RuntimeError("WARP rollback daemon did not become available")
        command("warp-cli", "--accept-tos", "connect")
    for unit, state in manifest["units"].items():
        command("systemctl", "enable" if state["enabled"] else "disable", unit, check=False)
        if state["active"]:
            command("systemctl", "start", unit)
        else:
            command("systemctl", "stop", unit, check=False)
    if manifest["logging"]:
        command("systemctl", "restart", "systemd-journald.service")
        if "tailscaled.service" in manifest["units"]:
            command("systemctl", "restart", "tailscaled.service")
    print(json.dumps({"rolledBack": str(backup)}))


def install(args):
    source = Path(__file__).resolve().parent.parent
    assets = source / "app/deploy/appliance"
    files = {}
    units = []
    old_port = None
    if args.archive_proxy:
        if Path("/etc/systemd/system/thinksmart-archive-proxy.socket").exists():
            raise RuntimeError("On-demand setup already exists; use its saved rollback before reinstalling")
        for executable in ("/usr/bin/warp-cli", "/usr/lib/systemd/systemd-socket-proxyd"):
            if not os.access(executable, os.X_OK):
                raise RuntimeError("Install and register the optional vendor WARP client separately first")
        settings = command("warp-cli", "--accept-tos", "settings").stdout
        match = re.search(r"Mode: WarpProxy on port (\d+)", settings)
        if not match:
            raise RuntimeError("Existing WARP must already be configured in proxy mode")
        old_port = int(match[1])
        if old_port not in (40000, 40001):
            raise RuntimeError("This optional setup requires the documented loopback proxy ports")
        units += ["warp-svc.service", "thinksmart-archive-proxy.socket"]
        initialized = command("systemctl", "cat", "thinksmart-initialize-warp.service", check=False).returncode == 0
        if initialized:
            if not Path("/var/lib/thinksmart/warp-configured").exists():
                raise RuntimeError("Finish this device's WARP registration before enabling on-demand mode")
            units.append("thinksmart-initialize-warp.service")
        files.update({
            "/usr/local/libexec/thinksmart-warp-ready": source / "tools/thinksmart-warp-ready.py",
            "/etc/systemd/system/thinksmart-archive-proxy.socket": assets / "thinksmart-archive-proxy.socket",
            "/etc/systemd/system/thinksmart-archive-proxy.service": assets / "thinksmart-archive-proxy.service",
            "/etc/systemd/system/warp-svc.service.d/90-thinksmart-on-demand.conf": assets / "warp-on-demand.conf",
        })
    if args.logging:
        files.update({
            "/etc/systemd/journald.conf.d/zz-thinksmart-appliance.conf": assets / "journald.conf",
            "/etc/systemd/system/thinksmart-home.service.d/90-appliance-logging.conf": assets / "home-logging.conf",
        })
        if command("systemctl", "cat", "tailscaled.service", check=False).returncode == 0:
            units.append("tailscaled.service")
            files["/etc/systemd/system/tailscaled.service.d/90-appliance-logging.conf"] = assets / "tailscale-logging.conf"
        for unit in ("rsyslog.service", "syslog-ng.service"):
            if command("systemctl", "is-active", unit, check=False).returncode == 0:
                raise RuntimeError("Review and disable the existing persistent syslog sink before appliance logging")
    for path in files.values():
        if not path.is_file():
            raise RuntimeError("Missing staged appliance source")
    backup = Path("/opt/thinksmart/backups") / (datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d-%H%M%S") + "-appliance")
    backup.mkdir(parents=True, mode=0o700)
    manifest = {"logging": args.logging, "archive": args.archive_proxy, "oldPort": old_port,
                "files": [], "units": {}}
    for unit in units:
        manifest["units"][unit] = {
            "enabled": command("systemctl", "is-enabled", unit, check=False).returncode == 0,
            "active": command("systemctl", "is-active", unit, check=False).returncode == 0,
        }
    for name in files:
        path = Path(name)
        manifest["files"].append({"path": name, "existed": path.is_file()})
        if path.is_file():
            target = backup / "files" / path.relative_to("/")
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
    (backup / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    shutil.copy2(__file__, backup / "install-appliance.py")
    timer = "thinksmart-appliance-rollback-" + backup.name
    command("systemd-run", "--unit=" + timer, "--on-active=5min", "--timer-property=AccuracySec=1s",
            "/usr/bin/python3", str(backup / "install-appliance.py"), "--rollback", str(backup))
    try:
        if args.archive_proxy:
            # Configure while the existing service is still running, before StopWhenUnneeded.
            command("warp-cli", "--accept-tos", "proxy", "port", "40001")
            command("systemctl", "disable", "--now", "warp-svc.service")
            if "thinksmart-initialize-warp.service" in units:
                command("systemctl", "disable", "--now", "thinksmart-initialize-warp.service")
        for name, original in files.items():
            path = Path(name)
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(original, path)
            path.chmod(0o755 if name.startswith("/usr/local/libexec/") else 0o644)
        command("systemctl", "daemon-reload")
        if args.archive_proxy:
            command("systemd-analyze", "verify", "/etc/systemd/system/thinksmart-archive-proxy.socket",
                    "/etc/systemd/system/thinksmart-archive-proxy.service")
            command("systemctl", "enable", "--now", "thinksmart-archive-proxy.socket")
        if args.logging:
            command("systemctl", "restart", "systemd-journald.service")
            command("journalctl", "--relinquish-var")
            # A detached systemd job lets this finish even when SSH briefly reconnects.
            if "tailscaled.service" in units:
                command("systemctl", "restart", "tailscaled.service")
        report = {"backup": str(backup), "rollbackTimer": timer + ".timer",
                  "logging": args.logging, "archiveProxy": args.archive_proxy,
                  "note": "Verify SSH and service health, then stop the rollback timer within five minutes."}
        Path("/run/thinksmart-appliance-install.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report))
    except Exception:
        rollback(backup)
        command("systemctl", "stop", timer + ".timer", check=False)
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--logging", action="store_true")
    parser.add_argument("--archive-proxy", action="store_true")
    parser.add_argument("--rollback", type=Path)
    options = parser.parse_args()
    os.umask(0o077)
    if os.geteuid() != 0:
        parser.error("Run on the device as root")
    if options.rollback:
        rollback(options.rollback)
    elif options.logging or options.archive_proxy:
        install(options)
    else:
        parser.error("Choose --logging and/or --archive-proxy explicitly")
