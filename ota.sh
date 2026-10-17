#!/bin/sh
# Upload firmware over Wi-Fi. Used by `pio run -e ota -t upload`, or by hand:
#   ./ota.sh .pio/build/panel/firmware.bin [host]
# Host: argument, else $PANEL_HOST, else OTA_HOST from include/secrets.h,
# else vgremote.local (the panel's mDNS name).
#
# After the upload it waits for the new firmware to pass probation: it must get
# onto Wi-Fi and run for 30 s, or the panel goes back to the previous firmware.
set -e
cd "$(dirname "$0")"
bin=${1:-.pio/build/panel/firmware.bin}
define() { sed -n "s/^#define $1 *\"\(.*\)\".*/\1/p" include/secrets.h; }
host=${2:-${PANEL_HOST:-$(define OTA_HOST)}}
host=${host:-vgremote.local}
pass=$(define OTA_PASSWORD)
built() { curl -fsS --max-time 5 "http://$host/" 2>/dev/null | sed -n 's/^built //p'; }
state() { curl -fsS --max-time 5 "http://$host/" 2>/dev/null | sed -n 's/^firmware: //p'; }

before=$(built || true)
echo "Uploading $bin to $host ..."
curl -fsS --max-time 120 -u "ota:$pass" -F "firmware=@$bin" "http://$host/update"

echo "Waiting for the new firmware to confirm itself (up to 4 min) ..."
sleep 10
i=0
while [ $i -lt 46 ]; do
  now=$(built || true)
  st=$(state || true)
  if [ -n "$now" ] && [ -n "$before" ] && [ "$now" = "$before" ]; then
    echo "ROLLED BACK: the panel is running the previous firmware ($now)." >&2
    exit 1
  fi
  if [ "$st" = "confirmed" ]; then
    echo "OK: new firmware confirmed (built $now)."
    exit 0
  fi
  sleep 5
  i=$((i + 1))
done
echo "No confirmation within 4 min (last state: ${st:-unreachable}). Check the panel." >&2
exit 1
