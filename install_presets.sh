#!/usr/bin/env bash
# Install the bundled WillyBeat preset library into the user's plugin folder.
#
# Usage:
#   ./install_presets.sh           # copy any missing presets, skip files that already exist
#   ./install_presets.sh --force   # overwrite all destination files
set -euo pipefail

force=0
if [[ "${1:-}" == "--force" ]]; then
    force=1
fi

src="$(cd "$(dirname "$0")" && pwd)/Presets"
dst="$HOME/Library/WillyBeat/Presets"

if [[ ! -d "$src" ]]; then
    echo "error: $src does not exist" >&2
    exit 1
fi

mkdir -p "$dst"

copied=0
skipped=0
for f in "$src"/*.beat; do
    name="$(basename "$f")"
    target="$dst/$name"
    if [[ -e "$target" && $force -eq 0 ]]; then
        skipped=$((skipped + 1))
        continue
    fi
    # -p preserves mtime so Empty.beat keeps its sort-first date.
    cp -p "$f" "$target"
    copied=$((copied + 1))
done

echo "Installed $copied preset(s) into:"
echo "  $dst"
if [[ $skipped -gt 0 ]]; then
    echo "Skipped $skipped existing file(s). Re-run with --force to overwrite."
fi
