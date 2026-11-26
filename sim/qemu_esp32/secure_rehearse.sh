#!/usr/bin/env bash
# Rehearse Secure Boot V2 + flash encryption on Espressif's QEMU before burning
# a real chip. Uses the same scripts and keys the board procedure uses.
#   sim/qemu_esp32/secure_rehearse.sh IMAGE.bin SIGNING_KEY.pem FLASH_KEY.bin [expected_text]
# IMAGE.bin is an unsigned image from build.sh; expected_text defaults to
# "SCHEDULER TEST PASSED".
#
# Starts from blank emulated eFuses, marks the emulated chip rev3 (the board
# already is), runs secure_burn.sh against it over QEMU's download-mode UART,
# then boots four images on the burned chip:
#   signed+encrypted         must print expected_text
#   unsigned, encrypted      must be refused by the ROM
#   signed, one byte changed must be refused by the ROM
#   signed, not encrypted    must not run
# Exits 0 only if all four behave. Writes nothing outside a temporary
# directory. QEMU overrides the emulator binary, QEMU_PORT the download-mode
# TCP port (default 5556).
set -euo pipefail
if [ $# -lt 3 ] || [ $# -gt 4 ]; then
    echo "usage: $0 IMAGE.bin SIGNING_KEY.pem FLASH_KEY.bin [expected_text]" >&2
    exit 2
fi
image=$1 sign_key=$2 flash_key=$3
expect=${4:-"SCHEDULER TEST PASSED"}
here=$(cd "$(dirname "$0")" && pwd)
qemu=${QEMU:-qemu-system-xtensa}
qport=${QEMU_PORT:-5556}
espefuse=${ESPEFUSE:-espefuse}
espsecure=${ESPSECURE:-espsecure}

# In /dev/shm when there is one: with the board's own keys, the emulated
# eFuse files hold the flash key in plain text until the script exits.
work=$(mktemp -d -p /dev/shm 2>/dev/null || mktemp -d)
qpid=""
cleanup() {
    [ -n "$qpid" ] && kill "$qpid" 2>/dev/null
    rm -rf "$work"
}
trap cleanup EXIT

head -c 124 /dev/zero >"$work/efuse.bin"
efuse_args=(-drive "file=$work/efuse.bin,if=none,format=raw,id=efuse"
            -global "driver=nvram.esp32.efuse,property=drive,value=efuse")

# Flash file of 4 MB erased flash with $1 written at 0x1000.
mkflash() {
    python3 - "$1" "$2" <<'PY'
import sys
img = open(sys.argv[1], "rb").read()
flash = bytearray(b"\xff" * (4 << 20))
flash[0x1000:0x1000 + len(img)] = img
open(sys.argv[2], "wb").write(flash)
PY
}

# Download mode, UART0 on TCP, as a real board in its ROM loader.
head -c $((4 << 20)) /dev/zero | tr '\0' '\377' >"$work/dl_flash.bin"
"$qemu" -machine esp32 -nographic -display none \
    -drive "file=$work/dl_flash.bin,if=mtd,format=raw" "${efuse_args[@]}" \
    -global driver=esp32.gpio,property=strap_mode,value=0x0f \
    -serial "tcp::$qport,server,nowait" >"$work/dl.log" 2>&1 &
qpid=$!
sleep 2
emu="socket://localhost:$qport"
$espefuse --chip esp32 --port "$emu" --before no_reset --do-not-confirm \
    burn_efuse CHIP_VER_REV1 1 CHIP_VER_REV2 1
EOS_BURN_CONFIRM=burn-permanently ESPEFUSE=$espefuse ESPSECURE=$espsecure \
    "$here/secure_burn.sh" "$emu" "$sign_key" "$flash_key"
kill "$qpid"; wait "$qpid" 2>/dev/null || true; qpid=""

# The four images.
ESPSECURE=$espsecure "$here/secure_image.sh" "$image" "$work/good.bin" "$sign_key" "$flash_key"
ESPSECURE=$espsecure "$here/secure_image.sh" "$image" "$work/signed_plain.bin" "$sign_key"
$espsecure encrypt_flash_data --keyfile "$flash_key" --address 0x1000 \
    --flash_crypt_conf 0xf --output "$work/unsigned.bin" "$image"
python3 - "$work/signed_plain.bin" "$work/tampered_plain.bin" <<'PY'
import sys
d = bytearray(open(sys.argv[1], "rb").read())
d[0x100] ^= 0x01        # inside the first segment, covered by the signature
open(sys.argv[2], "wb").write(d)
PY
$espsecure encrypt_flash_data --keyfile "$flash_key" --address 0x1000 \
    --flash_crypt_conf 0xf --output "$work/tampered.bin" "$work/tampered_plain.bin"

# boot NAME IMAGE SECONDS: boot IMAGE on the burned chip; log in $work/NAME.log.
boot() {
    local name=$1 img=$2 secs=$3 pid
    mkflash "$img" "$work/$name.flash"
    cp "$work/efuse.bin" "$work/$name.efuse"
    "$qemu" -machine esp32 -nographic -no-reboot \
        -drive "file=$work/$name.flash,if=mtd,format=raw" \
        -drive "file=$work/$name.efuse,if=none,format=raw,id=efuse" \
        -global driver=nvram.esp32.efuse,property=drive,value=efuse \
        >"$work/$name.log" 2>&1 &
    pid=$!
    for _ in $(seq $((secs * 10))); do
        grep -qaF -- "$expect" "$work/$name.log" && break
        kill -0 "$pid" 2>/dev/null || break
        sleep 0.1
    done
    kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null || true
    tr -d '\r' <"$work/$name.log" >"$work/$name.txt"
}
ran()     { grep -qaF -- "$expect" "$work/$1.txt"; }
refused() { grep -qaF "secure boot verification failed" "$work/$1.txt"; }

fails=0
result() {  # result NAME OK DESCRIPTION
    if [ "$2" = 1 ]; then echo "PASS  $1: $3"; else echo "FAIL  $1: $3"; fails=$((fails + 1)); fi
    { grep -m 4 -aiE 'secure boot|signature|invalid|flash read err|ets_main|rst:' "$work/$1.txt" || true; } |
        sed 's/^/        /'
}
boot good "$work/good.bin" 30
ok=0; ran good && ! refused good && ok=1
result good "$ok" "signed+encrypted image prints \"$expect\""
for n in unsigned tampered; do
    boot "$n" "$work/$n.bin" 8
    ok=0; refused "$n" && ! ran "$n" && ok=1
    result "$n" "$ok" "ROM refuses it"
done
boot signed_plain "$work/signed_plain.bin" 8
ok=0; ran signed_plain || ok=1
result signed_plain "$ok" "unencrypted image does not run"

if [ "$fails" -ne 0 ]; then
    echo "secure_rehearse: $fails case(s) failed; do not burn the board" >&2
    exit 1
fi
echo "secure_rehearse: all cases passed"
