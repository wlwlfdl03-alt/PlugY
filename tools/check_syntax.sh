#!/bin/bash
# Syntax-check PlugY sources with clang (MS inline asm blocks) against the mingw-w64 headers.
# Only a quick check : it doesn't link, and the real build is MSVC (see .github/workflows/build.yml).
#
# Requirements : clang, mingw-w64 headers (apt-get install mingw-w64-i686-dev).
# Usage : tools/check_syntax.sh [file.cpp ...]   (default : every PlugY/*.cpp)
#
# D2functions.cpp, LocalizedStrings.cpp and WorldEvent.cpp report errors with clang only
# (MSVC-specific code); MSVC builds them. Compare with the result on master.

ROOT=$(cd "$(dirname "$0")/.." && pwd)
MINGW=/usr/i686-w64-mingw32/include
B=$(mktemp -d)
trap 'rm -rf "$B"' EXIT

# Copy the tree and add case-insensitive include links (the sources were written for Windows).
cp -r "$ROOT/PlugY" "$ROOT/Commons" "$B/"
cd "$B"
for d in PlugY Commons; do
  grep -ho '#include *"[^"]*"' $d/*.cpp $d/*.h | sed 's/.*"\(.*\)"/\1/' | sort -u | while read -r inc; do
    p="$d/$inc"; [ -e "$p" ] && continue
    dir=$(dirname "$p"); base=$(basename "$p")
    real=$(ls "$dir" 2>/dev/null | grep -ix "$base" | head -1)
    [ -n "$real" ] && ln -s "$real" "$dir/$base"
  done
done
mkdir -p sysinc
grep -ho '#include *<[^>]*>' PlugY/*.cpp PlugY/*.h Commons/*.h | sed 's/.*<\(.*\)>/\1/' | sort -u | while read -r inc; do
  [ -e "$MINGW/$inc" ] && continue
  real=$(ls "$MINGW" | grep -ix "$inc" | head -1)
  [ -n "$real" ] && ln -s "$MINGW/$real" "sysinc/$inc"
done

cd "$B/PlugY"
FILES=("$@")
[ ${#FILES[@]} -eq 0 ] && FILES=(*.cpp)
for f in "${FILES[@]}"; do
  clang++ -fsyntax-only --target=i686-w64-mingw32 -fms-extensions -fdeclspec -fasm-blocks -fdelayed-template-parsing \
    -fpermissive -w -DWIN32 -D_WINDOWS -D_USRDLL -DPLUGY_EXPORTS -D_CRT_SECURE_NO_WARNINGS -D_CRT_NON_CONFORMING_SWPRINTFS \
    -isystem "$B/sysinc" -ferror-limit=5 "$(basename "$f")" 2>&1 | grep -E "error" | head -5 | sed "s|^|$(basename "$f"): |"
done
