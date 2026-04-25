#!/bin/bash
# Generate PDF from the EmbeddedOS multi-chapter book
# Handles images, figures, tables, code blocks, and citations
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

# Filter to only existing files and clean control chars
EXISTING=()
for ch in "${CHAPTERS[@]}"; do
  if [ -f "$BOOK_DIR/$ch" ]; then
    sed -i 's/[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]//g' "$BOOK_DIR/$ch"
    EXISTING+=("$BOOK_DIR/$ch")
  fi
done

echo "Found ${#EXISTING[@]} chapters"

# Cover image
COVER_ARGS=""
if [ -f "$BOOK_DIR/cover.png" ]; then
  COVER_ARGS="-V titlepage-background=$BOOK_DIR/cover.png"
fi

# Bibliography
CITE_ARGS=""
if [ -f "$BOOK_DIR/references.bib" ]; then
  CITE_ARGS="--citeproc --bibliography=$BOOK_DIR/references.bib"
fi

# Check for Eisvogel
TEMPLATE_ARGS=""
if pandoc --template=eisvogel -o /dev/null /dev/null 2>/dev/null; then
  TEMPLATE_ARGS="--template=eisvogel --listings"
  echo "Using Eisvogel template"
fi

pandoc \
  "${EXISTING[@]}" \
  -o "$OUTPUT" \
  --pdf-engine=xelatex \
  --from=markdown+smart+implicit_figures \
  --resource-path="$BOOK_DIR" \
  $TEMPLATE_ARGS \
  $COVER_ARGS \
  $CITE_ARGS \
  -V titlepage=true \
  -V titlepage-color="1a1a2e" \
  -V titlepage-text-color="ffffff" \
  -V titlepage-rule-color="58a6ff" \
  -V titlepage-rule-height=2 \
  -V "title=EmbeddedOS: The Complete Guide" \
  -V "subtitle=From Bootloader to Browser — Version 1.0.0" \
  -V "author=Srikanth Patchava & EmbeddedOS Contributors" \
  -V "date=$(date +'%B %Y')" \
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
  -V float-placement-figure=H \
  -V table-use-row-colors=true \
  -V "header-includes=\
    \usepackage{float}\
    \usepackage{booktabs}\
    \usepackage{longtable}\
    \usepackage{caption}\
    \captionsetup{font=small,labelfont=bf}\
    \usepackage{fancyhdr}\
    \pagestyle{fancy}\
    \fancyhead[L]{\small\leftmark}\
    \fancyhead[R]{\small v1.0.0}\
    \fancyfoot[C]{\small EmbeddedOS Press}\
    \fancyfoot[R]{\thepage}\
    \usepackage{graphicx}\
    \makeatletter\def\maxwidth{\ifdim\Gin@nat@width>\linewidth\linewidth\else\Gin@nat@width\fi}\makeatother\
    \setkeys{Gin}{width=\maxwidth,keepaspectratio}" \
  --highlight-style=tango \
  --number-sections

echo "=== PDF generated: $OUTPUT ==="
ls -lh "$OUTPUT"
