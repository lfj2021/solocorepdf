# SoloCorePDF

A tiny, blazing-fast PDF viewer for Windows — **open, read, present (F11). Nothing else.**

> 极简原生 PDF 阅读器：打开 · 阅读 · F11 演示，其他功能全部砍掉。

One person, core features only. That's the whole product.

- **Engine**: MuPDF 1.27 (native rendering, base-14 fonts embedded)
- **Shell**: pure Win32 + GDI, ~400 lines of C, zero dependencies
- **Size**: single exe, 6.6 MB (or 3.4 MB UPX-packed) — fits on a USB 2.0 stick
- **Speed**: opens and renders in tens of milliseconds, page-by-page memory model

## Download

| File | Size | Notes |
|---|---|---|
| `SoloCorePDF-x64.exe` | 6.6 MB | 64-bit, uncompressed, stable choice |
| `SoloCorePDF-x64-upx.exe` | 3.4 MB | 64-bit UPX-packed, best for USB drives |
| `SoloCorePDF-arm64.exe` | 6.4 MB | native ARM64 build for Windows 11/10 on ARM |
| `SoloCorePDF-x86.exe` | 7.1 MB | 32-bit build, runs on Windows XP SP3+ / any 32-bit Windows |
| `SoloCorePDF-x86-upx.exe` | 3.5 MB | 32-bit UPX-packed |

> ⚠️ Some antivirus engines may false-positive on the UPX-packed binary (self-extracting at runtime). If blocked, use the uncompressed `SoloCorePDF-x64.exe` — identical functionality.

No installer, no runtime, no WebView dependencies. Copy to a USB stick and run anywhere on Windows 10/11 x64 (or grab the ARM64 build for Windows-on-ARM devices, or the 32-bit build for Windows XP SP3+).

## Usage

```
SoloCorePDF-x64.exe [file.pdf]
```

- **Open**: command line arg / drag & drop onto window / Ctrl+O dialog
- **Navigate**: PgUp/PgDn, arrows, space, mouse wheel; Home/End for first/last page
- **Zoom**: `+` / `-` on top of Fit Page / Fit Width (menu View)
- **Present**: **F11** → fullscreen black presentation (auto-fit, hides menu & cursor). `←`/`→`/space/wheel/click to flip, `Esc` to exit
- Window title shows current page / total pages in real time

## Build from source (Linux)

```bash
# Requires: x86_64-w64-mingw32-gcc, make; upx optional
# Layout:
#   reader/
#     build.sh
#     src/main.c
#   ../mupdf-official/          ← MuPDF 1.27.0 source (with thirdparty)
cd reader && sh build.sh        # builds engine + shell into dist/
```

Engine flags (baked into build.sh): `-Os -DTOFU_CJK -DTOFU -DFZ_ENABLE_HYPHEN=0` (drops ~47 MB of CJK/Noto fonts, keeps base-14), `-UARCH_HAS_SSE -DARCH_HAS_SSE=0` (MinGW SSE misdetection fix).

## Known limitations (features intentionally cut, not bugs)

- No CJK fallback font: PDFs with Chinese but no embedded fonts will show tofu boxes (most PDFs embed fonts and render fine)
- No search / bookmarks / printing / annotations / tabs
- PDF only — every other format is rejected
- Single-threaded render: very large pages may block for tens of ms on page turn

## Verified

Tested under Wine against a 15-page arXiv paper (Attention Is All You Need, vector figures + math): open / navigation / F11 presentation / Esc all pass. Window title shows a plain `page N/M` (pure ASCII — renders identically on real Windows). ARM64 build cross-compiled with llvm-mingw, PE/Aarch64 machine type verified (runtime test needs a real ARM Windows device).

## License

Two parts, two licenses:

| Part | Content | License |
|---|---|---|
| **Shell** | `src/main.c`, `build.sh` (in this repo) | **MIT** |
| **Engine** | MuPDF 1.27.0 (embedded in both exes) | **AGPL-3.0** © Artifex Software Inc. |

The binaries embed the MuPDF engine, so redistribution is bound by **AGPL-3.0**: you must keep this notice and provide recipients a way to obtain the complete corresponding source.

- Engine source: https://github.com/ArtifexSoftware/mupdf (tag `1.27.0`)
- Shell source: `src/main.c` + `build.sh` in this repository
- Full license text: `LICENSE` (this repo) / https://www.gnu.org/licenses/agpl-3.0.html

---

## 中文说明

### 这是什么

SoloCorePDF 是一个极简、极快的 Windows PDF 阅读器。只做三件事：**打开 PDF · 窗口阅读 · F11 演示**。没有搜索、没有书签、没有打印、没有其它格式——其他功能全部砍掉。

### 文件与体积

| 文件 | 体积 | 说明 |
|---|---|---|
| `SoloCorePDF-x64.exe` | 6.6 MB | 64 位未压缩版，稳定首选 |
| `SoloCorePDF-x64-upx.exe` | 3.4 MB | 64 位 UPX 压缩版，U 盘首选 |
| `SoloCorePDF-arm64.exe` | 6.4 MB | ARM64 原生版（Win10/11 ARM 设备） |
| `SoloCorePDF-x86.exe` | 7.1 MB | 32 位版（XP SP3+ / 任何 32 位 Windows） |
| `SoloCorePDF-x86-upx.exe` | 3.5 MB | 32 位 UPX 压缩版 |

单个 exe 即完整程序，无任何外部依赖，双击即用，放 U 盘即插即用。

### 使用方法

```
SoloCorePDF-x64.exe [文件.pdf]
```

- **打开**：命令行参数 / 拖拽到窗口 / Ctrl+O
- **翻页**：PgUp/PgDn、←→↑↓、空格、滚轮；Home/End 首末页
- **缩放**：+ / -（叠加在适应页面/适应宽度之上）
- **演示**：F11 全屏黑底演示（自动适应屏幕、隐藏菜单和鼠标），←→/空格/滚轮/单击翻页，Esc 退出
- 窗口标题实时显示页码

### 已知限制

- 无 CJK 回退字体：PDF 未内嵌字体且含中文时显示方框（大多数 PDF 内嵌字体，正常显示）；需要可加编 DroidSansFallback（+4.8MB）
- 无搜索/书签/打印/标注/多标签；只支持 PDF

### 许可

壳（`src/main.c`、`build.sh`）：MIT。引擎 MuPDF 1.27.0：**AGPL-3.0**（二进制内嵌引擎，对外分发须遵守，源码获取途径见英文部分）。私有 U 盘使用无任何义务。
