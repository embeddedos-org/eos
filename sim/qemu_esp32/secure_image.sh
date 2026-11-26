#!/usr/bin/env bash
# Sign an EoS ESP32 image for Secure Boot V2 and, optionally, encrypt it for
# flash encryption, so the boot ROM of a chip with those eFuses burned will
# run it from 0x1000.
#   sim/qemu_esp32/secure_image.sh IN.bin OUT.bin SIGNING_KEY.pem [FLASH_KEY.bin]
# IN.bin is the image build.sh writes. OUT.bin is written only when every step
# succeeded: the signature verifies with SIGNING_KEY.pem and, when FLASH_KEY.bin
# is given, the encrypted image decrypts back to the signed one. Flash OUT.bin
# at 0x1000 with a plain write (it is already encrypted):
#   esptool --chip esp32 --port /dev/ttyUSB0 write_flash 0x1000 OUT.bin
#
# The encryption assumes FLASH_CRYPT_CONFIG = 0xF, the value the procedure in
# README.md burns. Nothing here prints key material. ESPSECURE overrides the
# espsecure command.
set -euo pipefail
if [ $# -lt 3 ] || [ $# -gt 4 ]; then
    echo "usage: $0 IN.bin OUT.bin SIGNING_KEY.pem [FLASH_KEY.bin]" >&2
    exit 2
fi
in=$1 out=$2 sign_key=$3 flash_key=${4:-}
espsecure=${ESPSECURE:-espsecure}
for f in "$in" "$sign_key" ${flash_key:+"$flash_key"}; do
    [ -r "$f" ] || { echo "secure_image: cannot read $f" >&2; exit 1; }
done

# OUT is replaced, so it must never be one of the inputs: a key overwritten
# here could be the only copy.
case $out in *.pem)
    echo "secure_image: refusing to write a .pem file: $out" >&2; exit 1 ;;
esac
for f in "$in" "$sign_key" ${flash_key:+"$flash_key"}; do
    if [ "$out" -ef "$f" ]; then
        echo "secure_image: OUT.bin is the same file as $f" >&2
        exit 1
    fi
done

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
rm -f "$out"
# Built beside OUT and renamed into place, so OUT is never left half-written.
staged=$(mktemp "$(dirname "$out")/.secure_image.XXXXXX")
trap 'rm -rf "$work" "$staged"' EXIT

$espsecure sign_data --version 2 --keyfile "$sign_key" --output "$work/signed.bin" "$in"
$espsecure verify_signature --version 2 --keyfile "$sign_key" "$work/signed.bin"
# The ROM looks for the signature block on the 4 KB boundary after the image.
size=$(stat -c %s "$work/signed.bin")
if [ $((size % 4096)) -ne 0 ]; then
    echo "secure_image: signed image is $size bytes, not a multiple of 4096" >&2
    exit 1
fi

if [ -z "$flash_key" ]; then
    cp "$work/signed.bin" "$staged"
    mv "$staged" "$out"
    echo "secure_image: signed (not encrypted) -> $out"
    exit 0
fi

$espsecure encrypt_flash_data --keyfile "$flash_key" --address 0x1000 \
    --flash_crypt_conf 0xf --output "$work/enc.bin" "$work/signed.bin"
$espsecure decrypt_flash_data --keyfile "$flash_key" --address 0x1000 \
    --flash_crypt_conf 0xf --output "$work/dec.bin" "$work/enc.bin"
if ! cmp -s "$work/dec.bin" "$work/signed.bin"; then
    echo "secure_image: encrypted image does not decrypt to the signed image" >&2
    exit 1
fi
if cmp -s "$work/enc.bin" "$work/signed.bin"; then
    echo "secure_image: encryption left the image unchanged" >&2
    exit 1
fi
cp "$work/enc.bin" "$staged"
mv "$staged" "$out"
echo "secure_image: signed and encrypted -> $out"
