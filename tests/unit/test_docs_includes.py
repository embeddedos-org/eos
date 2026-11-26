"""Every `#include <eos/...>` in docs must resolve to a header in the tree.

eos #277: `docs/tutorials/wireless-stacks.md` taught BLE, WiFi, and LoRa
with code that included four headers that did not exist (`eos/ble.h`,
`eos/wifi.h`, `eos/lora.h`, `eos/lorawan.h`). A developer at the
"starting" stage copied the tutorial and got `fatal error: eos/ble.h:
No such file or directory`. The four were the only unresolvable
`<eos/...>` includes anywhere under `docs/` — so the check is over
every Markdown file, not just the one that was broken, and it fails
here (in CI, via `pytest tests/`) instead of in a developer's build.

Resolution rule: `eos/<name>` resolves if some file in the tree lives
at `.../include/eos/<name>`, or a `.../include/eos/<name>.in` template
generates it (today that is only `version.h`, from
`include/eos/version.h.in`). Headers outside an `include/eos/`
directory do not count.
"""

import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DOCS_DIR = REPO_ROOT / "docs"

INCLUDE_RE = re.compile(r"#include\s*<eos/([^>]+)>")


def _provided_headers():
    """Names available as `eos/<name>` from any include/eos/ directory."""
    provided = set()
    for path in REPO_ROOT.rglob("include/eos/*"):
        if path.is_file():
            if path.suffix == ".h":
                provided.add(path.name)
            elif path.suffix == ".in" and path.stem.endswith(".h"):
                # e.g. include/eos/version.h.in generates eos/version.h.
                provided.add(path.stem)
    return provided


def test_docs_eos_includes_resolve():
    provided = _provided_headers()
    assert provided, "no eos headers found — repo root resolution is wrong"

    unresolved = {}
    for md in sorted(DOCS_DIR.rglob("*.md")):
        includes = sorted(set(INCLUDE_RE.findall(md.read_text(errors="replace"))))
        missing = [inc for inc in includes if inc not in provided]
        if missing:
            unresolved[str(md.relative_to(REPO_ROOT))] = missing

    assert not unresolved, (
        "docs cite <eos/...> headers that do not exist in the tree "
        "(eos #277): "
        + "; ".join(f"{f}: {', '.join(h)}" for f, h in unresolved.items())
    )
