#!/usr/bin/env bash
# Manual recovery for the onboard Broadcom adapter; run from the Pi desktop.
# This disconnects SSH/video briefly. Do not run it as a periodic watchdog.
export DISPLAY=:0
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p logs
profile=$(nmcli -g GENERAL.CONNECTION device show wlan0)
if [[ -z "$profile" || "$profile" == '--' ]]; then
  echo 'No active wlan0 profile. Reconnect using NetworkManager first.' >&2
  exit 1
fi
if [[ ! -d /sys/module/brcmfmac || ! -d /sys/module/brcmfmac_wcc ]]; then
  echo 'This recovery is specific to the tested onboard Broadcom/WCC driver.' >&2
  exit 1
fi
# Persist the existing profile, without changing its SSID, credentials or band.
sudo -n nmcli connection modify "$profile" 802-11-wireless.powersave 2
sudo -n /usr/sbin/iw dev wlan0 set power_save off
printf '%s\n' "$profile" > logs/wifi-recovery-profile.txt
task_root=$PWD
cat > logs/wifi-recovery-worker.sh <<'WORKER'
#!/usr/bin/env bash
set -uo pipefail
sleep 2
cd "$1"
profile=$(cat logs/wifi-recovery-profile.txt)
if /usr/sbin/modprobe -r brcmfmac_wcc brcmfmac; then
  sleep 2
  /usr/sbin/modprobe brcmfmac
  sleep 4
  timeout 45 nmcli connection up "$profile" ifname wlan0
  /usr/sbin/iw dev wlan0 get power_save
  /usr/sbin/iw dev wlan0 link
else
  echo 'Driver unload failed; no forced unload or reboot attempted.' >&2
  exit 1
fi
WORKER
sudo -n sh -c 'nohup bash "$1/logs/wifi-recovery-worker.sh" "$1" > "$1/logs/wifi-recovery.log" 2>&1 < /dev/null &' sh "$task_root"
echo 'Wi-Fi recovery queued. Check logs/wifi-recovery.log after reconnecting.'
