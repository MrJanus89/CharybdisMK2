#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent
OVERLAY = ROOT / "config/boards/shields/charybdis/charybdis_right.overlay"
CONF = ROOT / "config/charybdis.conf"

if not OVERLAY.exists():
    sys.exit(f"ERROR: missing {OVERLAY}")
if not CONF.exists():
    sys.exit(f"ERROR: missing {CONF}")

text = OVERLAY.read_text(encoding="utf-8")
original = text

# These properties are not declared by the inorichi PMW3610 binding used by this repo.
patterns = [
    r"(?m)^\s*cpi\s*=\s*<[^;]+>;\s*\n?",
    r"(?m)^\s*swap-xy\s*;\s*\n?",
    r"(?m)^\s*invert-x\s*;\s*\n?",
    r"(?m)^\s*invert-y\s*;\s*\n?",
    r"(?m)^\s*evt-type\s*=\s*<[^;]+>;\s*\n?",
    r"(?m)^\s*x-input-code\s*=\s*<[^;]+>;\s*\n?",
    r"(?m)^\s*y-input-code\s*=\s*<[^;]+>;\s*\n?",
]
for pattern in patterns:
    text = re.sub(pattern, "", text)

if text != original:
    backup = OVERLAY.with_suffix(OVERLAY.suffix + ".bak")
    if not backup.exists():
        backup.write_text(original, encoding="utf-8")
    OVERLAY.write_text(text, encoding="utf-8", newline="\n")
    print(f"UPDATED: {OVERLAY}")
else:
    print(f"NO CHANGE: unsupported PMW3610 properties were already absent")

required = [
    "CONFIG_PMW3610_CPI=800",
    "CONFIG_PMW3610_CPI_DIVIDOR=2",
    "CONFIG_PMW3610_SMART_ALGORITHM=y",
    "CONFIG_PMW3610_ORIENTATION_90=y",
    "CONFIG_PMW3610_INVERT_X=y",
    "CONFIG_ZMK_INPUT_PROCESSOR_THRESHOLD_LAYER=y",
]

conf_text = CONF.read_text(encoding="utf-8")
existing_keys = {}
for line in conf_text.splitlines():
    stripped = line.strip()
    if stripped.startswith("CONFIG_") and "=" in stripped:
        key = stripped.split("=", 1)[0]
        existing_keys[key] = stripped

additions = []
for line in required:
    key = line.split("=", 1)[0]
    if key not in existing_keys:
        additions.append(line)

if additions:
    with CONF.open("a", encoding="utf-8", newline="\n") as f:
        if conf_text and not conf_text.endswith("\n"):
            f.write("\n")
        f.write("\n# PMW3610 settings moved from Devicetree overlay\n")
        for line in additions:
            f.write(line + "\n")
    print(f"UPDATED: {CONF}")
    for line in additions:
        print(f"  + {line}")
else:
    print(f"NO CHANGE: required Kconfig settings already exist")

print("DONE: commit the modified files and push Charybdis-RGB")
