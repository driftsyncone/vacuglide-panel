#!/bin/sh
# Upload firmware over Wi-Fi. Used by `pio run -e ota -t upload`, or by hand:
#   ./ota.sh .pio/build/panel/firmware.bin [host]
# Host: argument, else $PANEL_HOST, else OTA_HOST from include/secrets.h,
# else vgremote.local (the panel's mDNS name).
set -e
cd "$(dirname "$0")"
bin=${1:-.pio/build/panel/firmware.bin}
define() { sed -n "s/^#define $1 *\"\(.*\)\".*/\1/p" include/secrets.h; }
host=${2:-${PANEL_HOST:-$(define OTA_HOST)}}
host=${host:-vgremote.local}
pass=$(define OTA_PASSWORD)
echo "Uploading $bin to $host ..."
curl -fsS --max-time 120 -u "ota:$pass" -F "firmware=@$bin" "http://$host/update"
