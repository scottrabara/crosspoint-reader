# Render harness

A host (Linux/macOS) program that lays out an EPUB and renders its pages with
the firmware's own code, then writes each page as a PNG plus a timing report.
Use it to review layout changes without a device and to compare the cost of
build and render changes.

It compiles the real `Epub`, `Section`, `ChapterHtmlSlimParser`, `ParsedText`,
`Page`, `GfxRenderer`, `FontCacheManager` and `FontDecompressor` sources, and
the committed built-in fonts (`lib/EpdFont/builtinFonts`). Only the hardware
and platform layer is replaced, by the headers in `shim/` and the small host
files next to `main.cpp`. Nothing in `lib/` or `src/` is modified.

## Build

Needs CMake 3.16+, a C++20 compiler and zlib headers (`zlib1g-dev`).

```sh
cmake -S tools/render-harness -B build/rh -G Ninja
cmake --build build/rh
```

## Run

```sh
./build/rh/render-harness --epub test/epubs/font-prewarm-benchmark.epub --out out/prewarm
python3 tools/render-harness/contact_sheet.py out/prewarm   # writes out/prewarm/index.html
```

Open `out/prewarm/index.html` to see the settings, a timing table per chapter
and every page as a thumbnail.

Options (defaults are the firmware's default settings):

| Flag | Default | Meaning |
| --- | --- | --- |
| `--epub PATH` | required | Book to render |
| `--out DIR` | required | Output directory |
| `--cache-dir DIR` | `DIR/cache` | Section/metadata cache, the host copy of `/.crosspoint` |
| `--font-pt 12\|14\|16\|18` | 14 | Built-in Noto Sans size |
| `--line-spacing 0-6` | 2 (+20%) | Index into `LINE_SPACING_MULTIPLIERS` |
| `--margin PX` | 5 | Screen margin setting |
| `--align justified\|left\|center\|right\|book` | justified | Paragraph alignment |
| `--hyphenation 0\|1` | 0 | Hyphenation |
| `--aa 0\|1` | 1 | Text anti-aliasing (grayscale passes) |
| `--orientation portrait\|landscape-cw\|inverted\|landscape-ccw` | portrait | Reader orientation |
| `--status-bar-height PX` | 19 | Space reserved for the status bar (Lyra/Classic default) |
| `--extra-paragraph-spacing 0\|1`, `--indent N`, `--word-spacing PCT`, `--embedded-style 0\|1` | 1, 2, 100, 1 | Other text settings |
| `--build full\|incremental` | full | One-shot `createSectionFile`, or the reader's `startBuild` + `buildSomeMore(8)` loop |
| `--chapters A-B` | all | Spine index range |
| `--max-pages N` | all | Pages rendered per chapter (the chapter is still fully built) |
| `--verbose` | off | Print firmware `LOG_*` output to stderr |

## Output

- `pages/cSSS-pPPP.png`: page `PPP` of spine item `SSS`, in reading
  orientation (480x800 in portrait). With AA on this is the 4-level composite;
  `-bw.png` is the B/W pass alone. With AA off there is only the B/W image.
- `report.json`: settings, viewport and margins, per chapter `{spineIndex,
  title, cacheHit, buildMs, pageCount, pagesRendered}`, per page `{chapter,
  page, loadMs, prewarmMs, bwRenderMs, grayRenderMs}`, totals, and the git SHA.
- `index.html` (from `contact_sheet.py`): a page that references `pages/`.

Running again with the same `--cache-dir` loads the cached sections (build
time near zero) and must give byte-identical PNGs.

## Page render sequence

`main.cpp` follows `EpubReaderActivity::renderContents()` on its non-tiled
grayscale path: a prewarm scope (`createPrewarmScope`, `page->render`,
`endScanAndPrewarm`), the B/W render into a white framebuffer, then
`storeBwBuffer`, the LSB and MSB passes into a cleared (`0x00`) framebuffer
with `copyGrayscale{Lsb,Msb}Buffers`, and `restoreBwBuffer`. The margins and
viewport are computed as in the reader's section load
(`getOrientedViewableTRBL` + screen margin, bottom += max(margin, status bar)).

## Gray composite

The X4 uses overlay grayscale (`GrayscaleCapabilities.h`, `OverlayMasks`):
the B/W base frame clears the bit of every non-white glyph pixel; the MSB plane
sets light and dark gray pixels; the LSB plane sets dark gray only
(`GlyphBitmap.h` `draw()`). The composite is therefore:

| B/W bit | LSB | MSB | Output |
| --- | --- | --- | --- |
| 1 | any | any | white (255) |
| 0 | 0 | 0 | black (0) |
| 0 | 0 | 1 | light gray (170) |
| 0 | 1 | 1 | dark gray (85) |

The gray values are evenly spaced for review. They are not a measurement of
the panel's real gray levels, which depend on the waveform and temperature.

## Faithful vs not

Faithful: EPUB parsing, CSS, section build and cache format, line breaking,
justification, hyphenation (real tries), page breaks, font metrics, kerning,
glyph decompression and the B/W and gray planes the renderer produces.

Not faithful or not covered:

- Timings are host timings. Use them to compare runs on the same machine, not
  as device numbers. The ESP32-C3 is far slower, and SD access is not modelled.
- The status bar is not drawn; only its reserved height is applied.
- Images are skipped: the image decoders are stubbed, so images use the
  parser's alt-text fallback (`[Image: ...]`), and pages lay out as on a device
  that cannot decode them.
- Only built-in fonts. SD card fonts (`.cpfont`) and TTF fonts are not loaded.
- No memory pressure: the host `MemoryManager` and `ESP` report a large heap,
  so low-memory fallbacks (skipping CSS, shedding caches) never trigger.
- The display shim reports no strip uploads, so the reader's whole-plane
  grayscale path is used instead of the X4's tiled one. Both produce the same
  planes from the same draw calls; only the order of work differs.
- Protected (LCP) books are not supported.
