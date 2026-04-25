#!/bin/bash
# Generate PDF from the EmbeddedOS book
set -e

BOOK_DIR="$(cd "$(dirname "$0")" && pwd)"
OUTPUT="$BOOK_DIR/embeddedos-complete-guide.pdf"

echo "=== Generating EmbeddedOS Book PDF ==="

# Collect all chapter files in order
CHAPTERS=(
  README.md
  part1-foundations/ch01-introduction.md
  part1-foundations/ch02-getting-started.md
  part1-foundations/ch03-architecture.md
  part2-kernel-hal/ch04-hal.md
  part2-kernel-hal/ch05-hal-extended.md
  part2-kernel-hal/ch06-kernel.md
  part2-kernel-hal/ch07-multicore.md
  part2-kernel-hal/ch08-drivers.md
  part3-services/ch09-crypto.md
  part3-services/ch10-security.md
  part3-services/ch11-networking.md
  part3-services/ch12-filesystem.md
  part3-services/ch13-ota.md
  part3-services/ch14-sensor-motor.md
  part3-services/ch15-compat.md
  part3-services/ch16-ui.md
  part4-ecosystem/ch17-eboot.md
  part4-ecosystem/ch18-ebuild.md
  part4-ecosystem/ch19-eipc.md
  part4-ecosystem/ch20-eai.md
  part4-ecosystem/ch21-eni.md
  part4-ecosystem/ch22-eosim.md
  part4-ecosystem/ch23-eostudio.md
  part5-hardware/ch24-eradar360.md
  part5-hardware/ch25-ehealth365.md
  part5-hardware/ch26-epam.md
  part6-apps-tools/ch27-eapps.md
  part6-apps-tools/ch28-edb.md
  part6-apps-tools/ch29-ebrowser.md
  part6-apps-tools/ch30-eoffice.md
  part6-apps-tools/ch31-evera.md
  part6-apps-tools/ch32-estocks.md
  part7-advanced/ch33-cross-compile.md
  part7-advanced/ch34-testing.md
  part7-advanced/ch35-safety.md
  part7-advanced/ch36-contributing.md
  appendices/appendix-a-api-quickref.md
  appendices/appendix-b-profiles.md
  appendices/appendix-c-pinouts.md
  appendices/appendix-d-glossary.md
  appendices/appendix-e-bibliography.md
)

# Filter to only existing files
EXISTING=()
for ch in "${CHAPTERS[@]}"; do
  if [ -f "$BOOK_DIR/$ch" ]; then
    EXISTING+=("$BOOK_DIR/$ch")
  fi
done

echo "Found ${#EXISTING[@]} chapters"

# Use cover image if available
COVER_ARGS=""
if [ -f "$BOOK_DIR/cover.png" ]; then
  COVER_ARGS="-V titlepage-background=$BOOK_DIR/cover.png"
fi

# Use references if available
CITE_ARGS=""
if [ -f "$BOOK_DIR/references.bib" ]; then
  CITE_ARGS="--citeproc --bibliography=$BOOK_DIR/references.bib"
fi

pandoc \
  "${EXISTING[@]}" \
  -o "$OUTPUT" \
  --pdf-engine=xelatex \
  --from=markdown+smart \
  --template=eisvogel \
  --listings \
  $COVER_ARGS \
  $CITE_ARGS \
  -V titlepage=true \
  -V titlepage-color="1a1a2e" \
  -V titlepage-text-color="ffffff" \
  -V titlepage-rule-color="58a6ff" \
  -V titlepage-rule-height=2 \
  -V page-background-color="ffffff" \
  -V title="EmbeddedOS: The Complete Guide" \
  -V author="Srikanth Patchava \& EmbeddedOS Contributors" \
  -V date="$(date +'%B %Y')" \
  -V toc=true \
  -V toc-depth=3 \
  -V toc-own-page=true \
  -V colorlinks=true \
  -V linkcolor=blue \
  -V urlcolor=blue \
  -V book=true \
  -V classoption=oneside \
  -V fontsize=11pt \
  -V geometry:margin=1in \
  --highlight-style=tango \
  --number-sections

echo "=== PDF generated: $OUTPUT ==="
ls -lh "$OUTPUT"
