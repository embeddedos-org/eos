#!/usr/bin/env bash
# Burn the eFuses that turn on Secure Boot V2 and flash encryption
# (development mode) on an ESP32 rev3. THIS IS PERMANENT on a real chip:
# afterwards the ROM boots only images signed with SIGNING_KEY.pem and
# decrypts flash with FLASH_KEY.bin, for the life of the chip.
#   sim/qemu_esp32/secure_burn.sh PORT SIGNING_KEY.pem FLASH_KEY.bin
# PORT is a serial device, or socket://host:port for QEMU in download mode
# (secure_rehearse.sh runs this script that way). Any PORT also needs
# EOS_BURN_CONFIRM=burn-permanently in the environment: a socket:// port can
# be a network bridge to a real board.
#
# Burns, in this order: the flash encryption key into BLOCK1 (read- and
# write-protected), the SHA-256 digest of the signing key's public half into
# BLOCK2 (write-protected), FLASH_CRYPT_CONFIG=0xF, FLASH_CRYPT_CNT=1,
# ABS_DONE_1, and last the write protection of RD_DIS, as ESP-IDF does, so
# BLOCK2 cannot later be read-protected (the ROM would then read a zero
# digest and refuse every image). It refuses a chip that is not rev3, that does not use the
# 256-bit coding scheme, or that already has secure boot or flash encryption
# on, and it fails unless the chip reads back as expected afterwards.
#
# Development mode: UART download mode, download-mode encryption/decryption,
# JTAG and the ROM console stay enabled, and FLASH_CRYPT_CNT has bits left to
# turn flash encryption off again. README.md lists the release-mode eFuses
# this deliberately does not burn. ESPEFUSE and ESPSECURE override the tools.
set -euo pipefail
if [ $# -ne 3 ]; then
    echo "usage: $0 PORT SIGNING_KEY.pem FLASH_KEY.bin" >&2
    exit 2
fi
port=$1 sign_key=$2 flash_key=$3
espefuse=${ESPEFUSE:-espefuse}
espsecure=${ESPSECURE:-espsecure}
for f in "$sign_key" "$flash_key"; do
    [ -r "$f" ] || { echo "secure_burn: cannot read $f" >&2; exit 1; }
done
if [ "$(stat -c %s "$flash_key")" -ne 32 ]; then
    echo "secure_burn: $flash_key is not a 256-bit key" >&2
    exit 1
fi
if [ "${EOS_BURN_CONFIRM:-}" != burn-permanently ]; then
    echo "secure_burn: burning is permanent; set EOS_BURN_CONFIRM=burn-permanently" >&2
    exit 1
fi
case $port in
socket://*) before=no_reset ;;
*) before=default_reset ;;
esac

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
fuse() { $espefuse --chip esp32 --port "$port" --before "$before" "$@"; }

# check FILE NAME=VALUE... : each named eFuse must read back as VALUE.
check() {
    local file=$1; shift
    python3 - "$file" "$@" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
bad = []
for spec in sys.argv[2:]:
    name, want = spec.split("=", 1)
    got = str(d[name]["value"])
    if got != want:
        bad.append(f"{name} is {got}, expected {want}")
for b in bad:
    print("secure_burn:", b, file=sys.stderr)
sys.exit(1 if bad else 0)
PY
}

fuse summary --format json --file "$work/before.json" >/dev/null
if ! check "$work/before.json" WAFER_VERSION_MAJOR=3 ABS_DONE_0=False \
        ABS_DONE_1=False FLASH_CRYPT_CNT=0 \
        "CODING_SCHEME=NONE (BLK1-3 len=256 bits)"; then
    echo "secure_burn: chip is not in the expected starting state; nothing burned" >&2
    exit 1
fi

$espsecure digest_sbv2_public_key --keyfile "$sign_key" --output "$work/digest.bin" >/dev/null
digest=$(python3 -c 'import sys; print(" ".join(f"{b:02x}" for b in open(sys.argv[1], "rb").read()))' "$work/digest.bin")

fuse --do-not-confirm burn_key flash_encryption "$flash_key"
fuse --do-not-confirm burn_key_digest "$sign_key"
fuse --do-not-confirm burn_efuse FLASH_CRYPT_CONFIG 0xF FLASH_CRYPT_CNT 1
fuse --do-not-confirm burn_efuse ABS_DONE_1 1
fuse --do-not-confirm write_protect_efuse RD_DIS

fuse summary --format json --file "$work/after.json" >/dev/null
check "$work/after.json" ABS_DONE_0=False ABS_DONE_1=True FLASH_CRYPT_CNT=1 \
    FLASH_CRYPT_CONFIG=15 "CODING_SCHEME=NONE (BLK1-3 len=256 bits)" "BLOCK2=$digest"
# RD_DIS bit 0 hides BLOCK1 (the flash key); bit 1 must stay clear so the ROM
# can read the digest. WR_DIS bit 0 locks RD_DIS, bits 7 and 8 lock BLOCK1/2.
python3 - "$work/after.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
rd, wr = d["RD_DIS"]["value"], d["WR_DIS"]["value"]
bad = []
if not rd & 1:
    bad.append("BLOCK1 (flash encryption key) is still readable")
if rd & 2:
    bad.append("BLOCK2 (key digest) is read-protected")
for bit, what in ((0, "RD_DIS"), (7, "BLOCK1"), (8, "BLOCK2")):
    if not wr >> bit & 1:
        bad.append(f"{what} is not write-protected (WR_DIS bit {bit})")
for b in bad:
    print("secure_burn:", b, file=sys.stderr)
sys.exit(1 if bad else 0)
PY
echo "secure_burn: Secure Boot V2 and flash encryption (development mode) are on"
