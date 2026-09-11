#!/usr/bin/env bash
# Sync host-runtime sources from wsm-my-lisp/dll (compatibility mirror).
# Run from repo root after cloning wsm-my-lisp beside or via TMP clone.
#
# PIN: wsm-my-lisp @ 4381e93f818edd41b6358720a0b4b83a1ed7bfb8 (authority guard)
# After Phase C, this script is deleted; this tree is sole source.

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEST="$ROOT/host-runtime"
PIN="${WSM_MY_LISP_PIN:-4381e93f818edd41b6358720a0b4b83a1ed7bfb8}"
SRC="${WSM_MY_LISP_SRC:-}"

if [[ -z "$SRC" ]]; then
  TMP="$(mktemp -d)"
  trap 'rm -rf "$TMP"' EXIT
  git clone --depth 1 https://github.com/juv4uk/wsm-my-lisp.git "$TMP/wsm-my-lisp"
  git -C "$TMP/wsm-my-lisp" fetch --depth 1 origin "$PIN"
  git -C "$TMP/wsm-my-lisp" checkout "$PIN"
  SRC="$TMP/wsm-my-lisp"
fi

mkdir -p "$DEST/src/bin" "$DEST/asm" "$DEST/tests" "$DEST/.cargo"

cp -a "$SRC/dll/src/eval.rs" "$DEST/src/"
cp -a "$SRC/dll/src/reader.rs" "$DEST/src/"
cp -a "$SRC/dll/src/printer.rs" "$DEST/src/"
cp -a "$SRC/dll/src/word.rs" "$DEST/src/"
cp -a "$SRC/dll/src/ffi.rs" "$DEST/src/"
cp -a "$SRC/dll/src/bin/"*.rs "$DEST/src/bin/" 2>/dev/null || true
cp -a "$SRC/dll/tests/"*.rs "$DEST/tests/" 2>/dev/null || true
cp -a "$SRC/dll/build.rs" "$DEST/"
cp -a "$SRC/asm/nucleus-win64.s" "$DEST/asm/"

# Paths inside destination tree
if grep -q 'include_str!("../../asm/nucleus-win64.s")' "$DEST/src/lib.rs" 2>/dev/null; then
  sed -i 's|include_str!("../../asm/nucleus-win64.s")|include_str!("../asm/nucleus-win64.s")|' "$DEST/src/lib.rs"
fi
if grep -q '../external/my-lisp' "$DEST/build.rs" 2>/dev/null; then
  sed -i 's|../external/my-lisp/lib/surface/semantic-registry.wsm|external/my-lisp/lib/surface/semantic-registry.wsm|' "$DEST/build.rs"
fi

echo "Synced host-runtime from wsm-my-lisp @$PIN → $DEST"
echo "Next: cd host-runtime && git submodule update --init external/my-lisp && cargo test --target x86_64-pc-windows-msvc"
