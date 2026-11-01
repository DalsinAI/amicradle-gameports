#!/bin/sh
# Neverball / Neverputt release build for the OpenUp stack.
# Uses OpenGPU SDL2 + GL and OpenGPU satellite libraries. No standalone SDL,
# Mesa, libpng, libjpeg or libvorbis runtime is carried by this port.
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
SRC=${1:?usage: build.sh /path/to/neverball-source [out-dir]}
OUT=${2:-"$ROOT/build/neverball-release"}

: "${OPENUP_SDK:?set OPENUP_SDK to the OpenGPU/Open SDK root}"
CPU=${CPU:-68040}
STOVE=${STOVE:-$HOME/AmigaChrome/stoves/os32-gcc16/prefix}
CC=${CC:-$STOVE/bin/m68k-amigaos-gcc}
CXX=${CXX:-$STOVE/bin/m68k-amigaos-g++}
DATA_PK3=${NEVERBALL_DATA_PK3:-"$ROOT/build/game-ports/runtime-assets/neverball/data-1.6.0.pk3"}
DATA_SHA=b2eddfbe05443e36719541639cb6246c5dd81260bce67a053d0d2c0ad34bc58c
PIN=a1ed09911dca262d80049c12a2824d683af494d6

[ -x "$CC" ] || { echo "missing 68k compiler: $CC" >&2; exit 2; }
[ -x "$CXX" ] || { echo "missing 68k C++ compiler: $CXX" >&2; exit 2; }
[ "$(git -C "$SRC" rev-parse HEAD 2>/dev/null)" = "$PIN" ] || {
    echo "Neverball source is not at pinned commit $PIN" >&2
    exit 2
}
[ -f "$DATA_PK3" ] || { echo "missing prepared Neverball release data: run game_port_prepare.py prepare --game neverball" >&2; exit 2; }
[ "$(sha256sum "$DATA_PK3" | awk '{print $1}')" = "$DATA_SHA" ] || {
    echo "Neverball release-data SHA-256 mismatch" >&2
    exit 2
}

SDL2_CONFIG=${SDL2_CONFIG:-"$OPENUP_SDK/bin/sdl2-config"}
[ -x "$SDL2_CONFIG" ] || { echo "missing OpenGPU sdl2-config: $SDL2_CONFIG" >&2; exit 2; }

# The patches apply in order. A tree carrying the same patch-set stamp is
# idempotent; a changed patch set must start from the pinned clean source.
STAMP=$(git -C "$SRC" rev-parse --absolute-git-dir)/openup-patches-applied
SUM=$(cat "$HERE"/patches/*.patch | sha256sum | cut -d' ' -f1)
if [ "$(cat "$STAMP" 2>/dev/null)" != "$SUM" ]; then
    for PATCH in "$HERE"/patches/*.patch; do
        [ -f "$PATCH" ] || { echo "missing Neverball OpenUp patch: $PATCH" >&2; exit 2; }
        if git -C "$SRC" apply --check "$PATCH" >/dev/null 2>&1; then
            git -C "$SRC" apply "$PATCH"
        elif [ ! -f "$STAMP" ] && git -C "$SRC" apply --reverse --check "$PATCH" >/dev/null 2>&1; then
            : # already applied by an older build.sh
        else
            echo "Neverball patch $(basename "$PATCH") does not apply cleanly to pinned source" >&2
            exit 3
        fi
    done
    echo "$SUM" > "$STAMP"
fi

mkdir -p "$OUT/bin" "$OUT/package/Neverball"
PKG="$OUT/package/Neverball"

SDL_CFLAGS=$("$SDL2_CONFIG" --cflags)
SDL_LIBS=$("$SDL2_CONFIG" --libs --gl)
case " $SDL_LIBS " in *" -lSDL2 "*) ;; *) echo "OpenGPU sdl2-config did not return SDL2 stub" >&2; exit 2;; esac
case " $SDL_LIBS " in *" -lGL "*)   ;; *) echo "OpenGPU sdl2-config did not return GL stub" >&2; exit 2;; esac

# GCC 16's 68040 loop-pattern transform can emit a bad FPCR-indexed float
# store. The OpenRTG/OpenGPU SDK requires disabling that transform and scanning
# final binaries. Keep the CPU baseline at 68040+68881: no AC090 instruction is
# allowed into the fallback executable.
PORT_CFLAGS="-O2 -m$CPU -m68881 -fomit-frame-pointer -fno-delete-null-pointer-checks -fno-tree-loop-distribute-patterns"

make -C "$SRC" clean-src
make -C "$SRC" -j${JOBS:-6} neverball neverputt \
  BUILD=release \
  CC="$CC" CXX="$CXX" \
  CFLAGS="$PORT_CFLAGS" \
  CXXFLAGS="$PORT_CFLAGS" \
  CPPFLAGS="-DNDEBUG" \
  SDL_CPPFLAGS="$SDL_CFLAGS" SDL_LIBS="$SDL_LIBS" \
  OGL_LIBS="" \
  TTF_LIBS="-L$OPENUP_SDK/lib -lSDL2_image -lSDL2_mixer -lSDL2_ttf" \
  PNG_CPPFLAGS="" PNG_LIBS="" \
  JPEG_CPPFLAGS="" JPEG_LIBS="" \
  OGG_LIBS="" \
  ENABLE_FETCH=0 ENABLE_NLS=0 \
  USERDIR="PROGDIR:User" DATADIR="PROGDIR:data" LOCALEDIR="PROGDIR:locale"

OBJDUMP=${OBJDUMP:-$STOVE/bin/m68k-amigaos-objdump}
[ -x "$OBJDUMP" ] || { echo "missing m68k objdump for FPCR scan: $OBJDUMP" >&2; exit 2; }
python3 "$ROOT/scripts/check_m68040_fpcr.py" "$OBJDUMP" "$SRC/neverball" "$SRC/neverputt"
NM=${NM:-$STOVE/bin/m68k-amigaos-nm}
[ -x "$NM" ] || { echo "missing m68k nm for stack-contract scan: $NM" >&2; exit 2; }
for PROGRAM in "$SRC/neverball" "$SRC/neverputt"; do
  "$NM" "$PROGRAM" | grep -q ' D ___stack$' || {
    echo "$(basename "$PROGRAM") does not carry the OpenGPU 1 MiB stack contract" >&2
    exit 2
  }
done

rm -rf "$PKG"
mkdir -p "$OUT/bin" "$PKG/doc" "$PKG/data"
cp "$SRC/neverball" "$OUT/bin/Neverball"
cp "$SRC/neverputt" "$OUT/bin/Neverputt"
cp "$OUT/bin/Neverball" "$PKG/Neverball"
cp "$OUT/bin/Neverputt" "$PKG/Neverputt"
cp "$DATA_PK3" "$PKG/data/data-1.6.0.pk3"
# OpenUp's own data beside the archive (the game reads a folder's files before an archive's): the
# Boing Ball (patches/0003 makes it the default ball). Its solid is the basic ball's from the release
# data, the material's name changed (the same length, in the solid's fixed-size field).
cp -R "$HERE/overlay/data/." "$PKG/data/"
python3 - "$DATA_PK3" "$PKG/data/ball/boing-ball/boing-ball-solid.sol" <<'PY_SOL'
import sys, zipfile
sol = zipfile.ZipFile(sys.argv[1]).read("ball/basic-ball/basic-ball-solid.sol")
old, new = b"ball/basic-ball/basic-ball", b"ball/boing-ball/boing-ball"
assert len(old) == len(new) and sol.count(old) == 1, "the basic ball's solid is not as expected"
open(sys.argv[2], "wb").write(sol.replace(old, new))
PY_SOL
cp "$SRC/LICENSE.md" "$PKG/"
for f in authors.txt manual.txt release-notes.md; do
    [ ! -f "$SRC/doc/$f" ] || cp "$SRC/doc/$f" "$PKG/doc/"
done
[ ! -d "$SRC/doc/legal" ] || cp -R "$SRC/doc/legal" "$PKG/doc/legal"

cat > "$PKG/OPENUP-RELEASE.txt" <<EOF
Neverball / Neverputt for OpenUp
Upstream: Neverball 1.6.0, commit $PIN
Official release data SHA-256: $DATA_SHA
Target: AmigaOS 3.2.x, 68040+FPU
Graphics: OpenGPU SDL2 + OpenGPU GL
Images: OpenGPU SDL2_image
Audio: OpenGPU SDL2_mixer -> SDL2 audio -> AHI
Controllers: OpenGPU SDL2 -> OpenInput
Keyboard/mouse: OpenGPU SDL2 native Amiga backend
68040 fallback: the same binary uses GL.module softpipe, native input and AHI/Paula
Runtime dependencies: opengpu.library + SDL2.module + GL.module
Optional acceleration: virgl/OpenGPU, OpenInput and host-mixed AHI when installed
EOF

cat > "$PKG/OPENUP-PERFORMANCE.txt" <<'EOF'
OpenUp performance and 68040 fallback

The shipped Neverball and Neverputt executables are one 68040+68881 build.
They never require AC090-only instructions or OpenMulticore.

Default / accelerated path
  GL.module chooses virgl/OpenGPU when AmigaChrome acceleration is available.
  SDL2 routes controllers through OpenInput when that backend is installed.
  SDL2_mixer sends audio to SDL2, then AHI/OpenAHI (including host mixing when
  the installed AHI stack provides it).

68040 fallback path
  GL.module automatically chooses its CPU softpipe renderer when virgl is not
  present. SDL2 retains native keyboard, mouse and controller support. Audio
  uses AHI and can fall back to Paula through SDL_AUDIODRIVER=paula.
  Set NEVERBALL_PROFILE=68040 before launch to disable reflections, shadows,
  anisotropic filtering, multisampling, mipmaps, high-DPI rendering and vsync.
  The profile also selects a 640x480 window. It applies to both games.

Useful SDK controls
  SDL_OPENGPU_GL=cpu       force the 68040 softpipe renderer
  SDL_OPENGPU=0            disable the OpenGPU SDL video backend
  SDL_OPENGPU_STATS=1      print OpenGPU command statistics
  SDL_AUDIODRIVER=paula    force the Paula audio fallback

OpenMulticore is deliberately not a hard dependency. Its workers cannot call
AmigaOS, SDL, OpenGPU or AHI, while Neverball's physics is a pointer-rich state
graph with callbacks. Moving it without a measured data-oriented rewrite would
be unsafe and would add dispatch overhead on a real 68040. Rendering, codecs,
input and audio use the applicable Open libraries; simulation remains on CPU0.
EOF

# The inventory is part of the package it authenticates. Generate it last and
# exclude the manifest itself so every listed digest remains reproducible.
rm -f "$OUT/package/SHA256SUMS"
(
  cd "$PKG"
  find . -type f ! -name 'SHA256SUMS' ! -name '.SHA256SUMS.tmp' -print0 \
    | sort -z | xargs -0 sha256sum > .SHA256SUMS.tmp
  mv .SHA256SUMS.tmp SHA256SUMS
)
echo "Neverball release tree: $PKG"
