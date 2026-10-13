#!/bin/sh
# SoloCorePDF — build script (Linux cross-compile to Windows x64)
# Prereqs: x86_64-w64-mingw32-gcc, upx (optional), MuPDF source at ../mupdf-official
set -e
cd "$(dirname "$0")"
ROOT=$(pwd)
MUPDF=../mupdf-official
OUT=dist
mkdir -p "$OUT"

# 1. Build MuPDF engine libs (PDF-capable minimal config) if not present
#    NOTE: uses build_suffix=-win so host builds in the same tree never clash.
MUPDF_OUT="$MUPDF/build/release-win"
if [ ! -f "$MUPDF_OUT/libmupdf.a" ]; then
  echo "[1/3] building MuPDF engine (MinGW cross)..."
  ( cd "$MUPDF" && make libs build=release build_suffix=-win \
        CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++ \
        AR=x86_64-w64-mingw32-ar RANLIB=x86_64-w64-mingw32-ranlib \
        USE_SYSTEM_CURL=no \
        XCFLAGS="-DTOFU_CJK -DTOFU -DFZ_ENABLE_HYPHEN=0 -Os -UARCH_HAS_SSE -DARCH_HAS_SSE=0" \
        -j"$(nproc)" )
fi

# 2. Compile the reader
echo "[2/3] compiling SoloCorePDF.exe..."
RES=$(find "$MUPDF_OUT/resources" -name '*.o' | tr '\n' ' ')
x86_64-w64-mingw32-windres app.rc -O coff -o "$OUT/app.res.o"
x86_64-w64-mingw32-gcc src/main.c "$OUT/app.res.o" \
    -O2 -s -static -municode -mwindows \
    -I "$MUPDF/include" \
    "$MUPDF_OUT/libmupdf.a" \
    "$MUPDF_OUT/libmupdf-third.a" \
    $RES \
    -lgdi32 -lcomdlg32 -lshell32 \
    -o "$OUT/SoloCorePDF.exe"

# 3. Optional UPX-compressed copy
echo "[3/3] UPX-compressed copy..."
if command -v upx >/dev/null 2>&1; then
  cp "$OUT/SoloCorePDF.exe" "$OUT/SoloCorePDF-upx.exe"
  upx -q -9 "$OUT/SoloCorePDF-upx.exe" >/dev/null 2>&1 || true
fi

ls -la "$OUT"
echo "DONE."
