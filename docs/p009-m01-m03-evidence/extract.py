"""Extract complete candidate Editor methods into the contract fixture."""
from pathlib import Path
import re
import sys

source, output = map(Path, sys.argv[1:])
text = source.read_text().split("class Editor final:", 1)[1]
names = ("close", "setContentScaleFactor", "beforeSizeChange", "onSize",
         "checkSizeConstraint", "syncPreferences")
methods = []
for name in names:
    pattern = rf"^    [^ \n][^\n]*\b{name}\([^\n]*\)\s*(?:override)?\{{\n.*?^    \}}"
    matches = re.findall(pattern, text, re.MULTILINE | re.DOTALL)
    if len(matches) != 1:
        raise SystemExit(f"{name}: found {len(matches)} declarations for extraction")
    methods.extend(matches)
output.write_text("\n\n".join(methods) + "\n")
