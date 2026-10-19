#!/usr/bin/env bash
# Re-creates third_party/mutable from the pinned upstream commits.
#
# Only the files listed in third_party/mutable/MANIFEST.txt are copied (the header closure of
# Source/Engine/PlaitsOscillator.cc plus the .cc files needed to link). Upstream is MIT licensed
# (Copyright Emilie Gillet); every copied file keeps its own license header.
#
# One local modification is applied, see third_party/mutable/README.md.
set -euo pipefail

EURORACK_SHA=f4cfa7697095a28d292bff758beb51deb49b5a86
STMLIB_SHA=d18def816c51d1da0c108236928b2bbd25c17481

root="$(cd "$(dirname "$0")/.." && pwd)"
dest="$root/third_party/mutable"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fetch() {  # <repo-url> <sha> <dir>
  git init -q "$3"
  git -C "$3" fetch -q --depth 1 "$1" "$2"
  git -C "$3" checkout -q FETCH_HEAD
}
fetch https://github.com/pichenettes/eurorack.git "$EURORACK_SHA" "$tmp/eurorack"
fetch https://github.com/pichenettes/stmlib.git   "$STMLIB_SHA"   "$tmp/stmlib"

# stmlib is a git submodule of eurorack; flatten both under one include root.
mkdir -p "$tmp/src"
cp -R "$tmp/eurorack/plaits" "$tmp/src/plaits"
cp -R "$tmp/stmlib" "$tmp/src/stmlib"

find "$dest" -mindepth 1 ! -name MANIFEST.txt ! -name README.md -delete 2>/dev/null || true
while read -r f; do
  [ -z "$f" ] && continue
  mkdir -p "$dest/$(dirname "$f")"
  cp "$tmp/src/$f" "$dest/$f"
done < "$dest/MANIFEST.txt"

cp "$tmp/stmlib/LICENSE" "$dest/stmlib/LICENSE"

# Local modification: make the random-number generator state thread-local so that several plug-in
# instances running on different audio threads never share (and race on) it.
sed -i 's/static uint32_t rng_state_;/static thread_local uint32_t rng_state_;/' "$dest/stmlib/utils/random.h"
sed -i 's/^uint32_t Random::rng_state_/thread_local uint32_t Random::rng_state_/' "$dest/stmlib/utils/random.cc"

# Local modification: an empty table initialiser is a zero-size array, which GCC accepts as an
# extension but MSVC rejects. The table is declared in resources.h and never used.
sed -z -i 's/lookup_table_i32_table\[\] = {\n};/lookup_table_i32_table[] = { nullptr };/' "$dest/plaits/resources.cc"

echo "Vendored $(wc -l < "$dest/MANIFEST.txt") files from eurorack@${EURORACK_SHA:0:7} and stmlib@${STMLIB_SHA:0:7}"
