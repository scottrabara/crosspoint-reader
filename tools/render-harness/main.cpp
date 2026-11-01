// Host page-render harness: lays out an EPUB with the firmware's Epub/Section
// code and renders each page through the real GfxRenderer and built-in fonts,
// then writes PNGs and a timing report. See README.md for what is faithful.

#include <Epub.h>
#include <Epub/Page.h>
#include <Epub/ReaderRenderSpec.h>
#include <Epub/Section.h>
#include <FontCacheManager.h>
#include <FontDecompressor.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <Logging.h>
#include <limits.h>
#include <stdlib.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "../../src/fontIds.h"
#include "HarnessFonts.h"
#include "PngWriter.h"

#ifndef HARNESS_GIT_SHA
#define HARNESS_GIT_SHA "unknown"
#endif

namespace {

// Mirrors src/ReaderLineSpacing.h LINE_SPACING_MULTIPLIERS (persisted LINE_COMPRESSION order).
constexpr float LINE_SPACING_MULTIPLIERS[] = {1.0f, 1.1f, 1.2f, 1.3f, 1.45f, 1.6f, 1.75f};
constexpr int LINE_SPACING_COUNT = sizeof(LINE_SPACING_MULTIPLIERS) / sizeof(LINE_SPACING_MULTIPLIERS[0]);

// Mirrors CrossPointSettings::PARAGRAPH_ALIGNMENT.
constexpr const char* ALIGNMENT_NAMES[] = {"justified", "left", "center", "right", "book"};
// Mirrors CrossPointSettings::ORIENTATION and ReaderUtils::applyOrientation.
constexpr const char* ORIENTATION_NAMES[] = {"portrait", "landscape-cw", "inverted", "landscape-ccw"};
constexpr GfxRenderer::Orientation ORIENTATIONS[] = {GfxRenderer::Portrait, GfxRenderer::LandscapeClockwise,
                                                     GfxRenderer::PortraitInverted,
                                                     GfxRenderer::LandscapeCounterClockwise};

// Composite gray levels for the 4-level output. The X4 overlay waveform has no
// calibrated luminance; these are evenly spaced for readability.
constexpr uint8_t GRAY_BLACK = 0;
constexpr uint8_t GRAY_DARK = 85;
constexpr uint8_t GRAY_LIGHT = 170;
constexpr uint8_t GRAY_WHITE = 255;

struct Options {
  std::string epubPath;
  std::string outDir;
  std::string cacheDir;
  // Defaults from CrossPointSettings.h (fontPointSize 14, LINE_SPACING_120,
  // JUSTIFIED, hyphenation off, screenMargin 5, textAntiAliasing 1, PORTRAIT).
  int fontPt = 14;
  int lineSpacing = 2;
  int margin = 5;
  int alignment = 0;
  bool hyphenation = false;
  bool antiAliasing = true;
  int orientation = 0;
  // Lyra/Base theme statusBarVerticalMargin (19) with the default status bar
  // (text lane shown, progress bar hidden), as UITheme::getStatusBarHeight().
  int statusBarHeight = 19;
  bool extraParagraphSpacing = true;
  int paragraphIndent = 2;
  int wordSpacing = 100;
  bool embeddedStyle = true;
  int maxPages = -1;
  int chapterFirst = 0;
  int chapterLast = INT_MAX;
  // Full: one-shot createSectionFile. Incremental: the reader's default
  // startBuild + buildSomeMore(BUILD_PAGES_PER_CHUNK) loop, with pages read
  // back from the half-written section file while the build continues.
  bool incrementalBuild = false;
  bool verbose = false;
};

// Mirrors EpubReaderActivity::BUILD_PAGES_PER_CHUNK.
constexpr int BUILD_PAGES_PER_CHUNK = 8;

struct PageTiming {
  int spine;
  int page;
  double loadMs;
  double prewarmMs;
  double bwRenderMs;
  double grayRenderMs;
};

struct ChapterResult {
  int spine;
  std::string title;
  bool cacheHit;
  double buildMs;
  int pageCount;
  int pagesRendered;
  bool ok;
};

double nowMs() {
  using Clock = std::chrono::steady_clock;
  static const auto start = Clock::now();
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

[[noreturn]] void usage(const char* message) {
  if (message) std::fprintf(stderr, "render-harness: %s\n\n", message);
  std::fprintf(stderr,
               "usage: render-harness --epub PATH --out DIR [--cache-dir DIR]\n"
               "  [--font-pt 12|14|16|18] [--line-spacing 0-6] [--margin PX]\n"
               "  [--align justified|left|center|right|book] [--hyphenation 0|1] [--aa 0|1]\n"
               "  [--orientation portrait|landscape-cw|inverted|landscape-ccw]\n"
               "  [--status-bar-height PX] [--extra-paragraph-spacing 0|1] [--indent N]\n"
               "  [--word-spacing PCT] [--embedded-style 0|1] [--build full|incremental]\n"
               "  [--max-pages N (per chapter)] [--chapters A-B (spine indexes)] [--verbose]\n");
  std::exit(2);
}

int parseInt(const char* flag, const char* value, const int lo, const int hi) {
  char* end = nullptr;
  const long v = std::strtol(value, &end, 10);
  if (!end || *end != '\0' || v < lo || v > hi) {
    std::fprintf(stderr, "render-harness: %s expects an integer in [%d, %d], got '%s'\n", flag, lo, hi, value);
    std::exit(2);
  }
  return static_cast<int>(v);
}

int parseName(const char* flag, const char* value, const char* const* names, const int count) {
  for (int i = 0; i < count; i++) {
    if (std::strcmp(value, names[i]) == 0) return i;
  }
  std::fprintf(stderr, "render-harness: unknown %s '%s'\n", flag, value);
  std::exit(2);
}

Options parseArgs(const int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; i++) {
    const std::string flag = argv[i];
    if (flag == "--verbose") {
      o.verbose = true;
      continue;
    }
    if (flag == "--help" || flag == "-h") usage(nullptr);
    if (i + 1 >= argc) usage(("missing value for " + flag).c_str());
    const char* v = argv[++i];
    if (flag == "--epub") {
      o.epubPath = v;
    } else if (flag == "--out") {
      o.outDir = v;
    } else if (flag == "--cache-dir") {
      o.cacheDir = v;
    } else if (flag == "--font-pt") {
      o.fontPt = parseInt("--font-pt", v, 12, 18);
      if (o.fontPt % 2 != 0) usage("--font-pt must be 12, 14, 16 or 18");
    } else if (flag == "--line-spacing") {
      o.lineSpacing = parseInt("--line-spacing", v, 0, LINE_SPACING_COUNT - 1);
    } else if (flag == "--margin") {
      o.margin = parseInt("--margin", v, 0, 200);
    } else if (flag == "--align") {
      o.alignment = parseName("--align", v, ALIGNMENT_NAMES, 5);
    } else if (flag == "--hyphenation") {
      o.hyphenation = parseInt("--hyphenation", v, 0, 1) != 0;
    } else if (flag == "--aa") {
      o.antiAliasing = parseInt("--aa", v, 0, 1) != 0;
    } else if (flag == "--orientation") {
      o.orientation = parseName("--orientation", v, ORIENTATION_NAMES, 4);
    } else if (flag == "--status-bar-height") {
      o.statusBarHeight = parseInt("--status-bar-height", v, 0, 200);
    } else if (flag == "--extra-paragraph-spacing") {
      o.extraParagraphSpacing = parseInt("--extra-paragraph-spacing", v, 0, 1) != 0;
    } else if (flag == "--indent") {
      o.paragraphIndent = parseInt("--indent", v, 0, 8);
    } else if (flag == "--word-spacing") {
      o.wordSpacing = parseInt("--word-spacing", v, 50, 200);
    } else if (flag == "--embedded-style") {
      o.embeddedStyle = parseInt("--embedded-style", v, 0, 1) != 0;
    } else if (flag == "--build") {
      const std::string mode = v;
      if (mode != "full" && mode != "incremental") usage("--build expects full or incremental");
      o.incrementalBuild = mode == "incremental";
    } else if (flag == "--max-pages") {
      o.maxPages = parseInt("--max-pages", v, 0, INT_MAX);
    } else if (flag == "--chapters") {
      const std::string range = v;
      const size_t dash = range.find('-');
      if (dash == std::string::npos) {
        o.chapterFirst = o.chapterLast = parseInt("--chapters", v, 0, INT_MAX);
      } else {
        o.chapterFirst = parseInt("--chapters", range.substr(0, dash).c_str(), 0, INT_MAX);
        o.chapterLast = parseInt("--chapters", range.substr(dash + 1).c_str(), 0, INT_MAX);
      }
      if (o.chapterLast < o.chapterFirst) usage("--chapters range is empty");
    } else {
      usage(("unknown flag " + flag).c_str());
    }
  }
  if (o.epubPath.empty() || o.outDir.empty()) usage("--epub and --out are required");
  if (o.cacheDir.empty()) o.cacheDir = o.outDir + "/cache";
  return o;
}

std::string absolutePath(const std::string& path) {
  char buf[PATH_MAX];
  return realpath(path.c_str(), buf) ? std::string(buf) : path;
}

// Mirrors CrossPointSettings::getReaderFontId() for the built-in family.
int readerFontId(const int pt) {
  switch (pt) {
    case 12:
      return NOTOSANS_12_FONT_ID;
    case 16:
      return NOTOSANS_16_FONT_ID;
    case 18:
      return NOTOSANS_18_FONT_ID;
    case 14:
    default:
      return NOTOSANS_14_FONT_ID;
  }
}

struct Margins {
  int top;
  int right;
  int bottom;
  int left;
};

// Mirrors the viewport setup in EpubReaderActivity's section load (without
// the automatic-page-turn branch).
Margins readerMargins(const GfxRenderer& renderer, const Options& o) {
  Margins m{};
  renderer.getOrientedViewableTRBL(&m.top, &m.right, &m.bottom, &m.left);
  m.top += o.margin;
  m.left += o.margin;
  m.right += o.margin;
  m.bottom += std::max(o.margin, o.statusBarHeight);
  return m;
}

// Mirrors CrossPointSettings::readerRenderSpec().
ReaderRenderSpec readerSpec(const Options& o, const uint16_t width, const uint16_t height) {
  ReaderRenderSpec spec;
  spec.fontId = readerFontId(o.fontPt);
  spec.lineCompression = LINE_SPACING_MULTIPLIERS[o.lineSpacing];
  spec.characterSpacing = 0;
  spec.wordSpacingPercent = static_cast<uint8_t>(o.wordSpacing);
  spec.extraParagraphSpacing = o.extraParagraphSpacing;
  spec.paragraphIndentSpaces = static_cast<uint8_t>(o.paragraphIndent);
  spec.paragraphAlignment = static_cast<uint8_t>(o.alignment);
  spec.viewportWidth = width;
  spec.viewportHeight = height;
  spec.hyphenationEnabled = o.hyphenation;
  spec.embeddedStyle = o.embeddedStyle;
  spec.imageRendering = 0;  // IMAGES_DISPLAY; decoders are stubbed so images fall back to alt text
  spec.focusReadingEnabled = false;
  return spec;
}

// Same transform as GfxRenderer.cpp rotateCoordinates(): logical -> physical.
void logicalToPhysical(const GfxRenderer::Orientation orientation, const int x, const int y, int& px, int& py) {
  constexpr int W = HalDisplay::DISPLAY_WIDTH;
  constexpr int H = HalDisplay::DISPLAY_HEIGHT;
  switch (orientation) {
    case GfxRenderer::Portrait:
      px = y;
      py = H - 1 - x;
      break;
    case GfxRenderer::LandscapeClockwise:
      px = W - 1 - x;
      py = H - 1 - y;
      break;
    case GfxRenderer::PortraitInverted:
      px = W - 1 - y;
      py = x;
      break;
    case GfxRenderer::LandscapeCounterClockwise:
      px = x;
      py = y;
      break;
  }
}

inline bool bitSet(const uint8_t* plane, const int px, const int py) {
  return (plane[py * HalDisplay::DISPLAY_WIDTH_BYTES + px / 8] >> (7 - (px & 7))) & 1;
}

// Composes the logical-orientation image. Overlay encoding (GrayscaleCapabilities.h,
// GlyphBitmap.h draw()): the BW base clears the bit for every non-white pixel;
// MSB marks light and dark gray, LSB marks dark gray only. So with the base black:
// (LSB,MSB) 00 = black, 01 = light gray, 11 = dark gray. Base white = white.
std::vector<uint8_t> composeImage(const GfxRenderer& renderer, const std::vector<uint8_t>& bw, const uint8_t* lsb,
                                  const uint8_t* msb, const int width, const int height) {
  std::vector<uint8_t> out(static_cast<size_t>(width) * height);
  const auto orientation = renderer.getOrientation();
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int px = 0, py = 0;
      logicalToPhysical(orientation, x, y, px, py);
      uint8_t v;
      if (bitSet(bw.data(), px, py)) {
        v = GRAY_WHITE;
      } else if (!msb) {
        v = GRAY_BLACK;
      } else {
        const bool l = bitSet(lsb, px, py);
        const bool m = bitSet(msb, px, py);
        v = m ? (l ? GRAY_DARK : GRAY_LIGHT) : GRAY_BLACK;
      }
      out[static_cast<size_t>(y) * width + x] = v;
    }
  }
  return out;
}

std::string jsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 2);
  for (const unsigned char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

double percentile(std::vector<double> values, const double p) {
  if (values.empty()) return 0;
  std::sort(values.begin(), values.end());
  const double rank = p * static_cast<double>(values.size() - 1);
  const size_t lo = static_cast<size_t>(rank);
  const size_t hi = std::min(lo + 1, values.size() - 1);
  return values[lo] + (values[hi] - values[lo]) * (rank - static_cast<double>(lo));
}

bool writeReport(const std::string& path, const Options& o, const Margins& m, const ReaderRenderSpec& spec,
                 const int screenW, const int screenH, const std::string& title,
                 const std::vector<ChapterResult>& chapters, const std::vector<PageTiming>& pages,
                 const double epubLoadMs, const bool epubCacheHit) {
  std::FILE* f = std::fopen(path.c_str(), "w");
  if (!f) return false;
  std::fprintf(f, "{\n  \"harnessGitSha\": \"%s\",\n", HARNESS_GIT_SHA);
  std::fprintf(f, "  \"epub\": \"%s\",\n  \"title\": \"%s\",\n", jsonEscape(o.epubPath).c_str(),
               jsonEscape(title).c_str());
  std::fprintf(f,
               "  \"settings\": {\"fontPt\": %d, \"fontId\": %d, \"lineSpacingIndex\": %d, \"lineCompression\": %.2f, "
               "\"screenMargin\": %d, \"alignment\": \"%s\", \"hyphenation\": %s, \"antiAliasing\": %s, "
               "\"orientation\": \"%s\", \"statusBarHeight\": %d, \"extraParagraphSpacing\": %s, "
               "\"paragraphIndent\": %d, \"wordSpacing\": %d, \"embeddedStyle\": %s, \"maxPagesPerChapter\": %d, "
               "\"build\": \"%s\"},\n",
               o.fontPt, spec.fontId, o.lineSpacing, spec.lineCompression, o.margin, ALIGNMENT_NAMES[o.alignment],
               o.hyphenation ? "true" : "false", o.antiAliasing ? "true" : "false", ORIENTATION_NAMES[o.orientation],
               o.statusBarHeight, o.extraParagraphSpacing ? "true" : "false", o.paragraphIndent, o.wordSpacing,
               o.embeddedStyle ? "true" : "false", o.maxPages, o.incrementalBuild ? "incremental" : "full");
  std::fprintf(f,
               "  \"layout\": {\"screenWidth\": %d, \"screenHeight\": %d, \"marginTop\": %d, \"marginRight\": %d, "
               "\"marginBottom\": %d, \"marginLeft\": %d, \"viewportWidth\": %d, \"viewportHeight\": %d},\n",
               screenW, screenH, m.top, m.right, m.bottom, m.left, spec.viewportWidth, spec.viewportHeight);
  std::fprintf(f, "  \"epubLoadMs\": %.3f,\n  \"epubCacheHit\": %s,\n", epubLoadMs, epubCacheHit ? "true" : "false");

  std::fprintf(f, "  \"chapters\": [\n");
  for (size_t i = 0; i < chapters.size(); i++) {
    const auto& c = chapters[i];
    std::fprintf(f,
                 "    {\"spineIndex\": %d, \"title\": \"%s\", \"ok\": %s, \"cacheHit\": %s, \"buildMs\": %.3f, "
                 "\"pageCount\": %d, \"pagesRendered\": %d}%s\n",
                 c.spine, jsonEscape(c.title).c_str(), c.ok ? "true" : "false", c.cacheHit ? "true" : "false",
                 c.buildMs, c.pageCount, c.pagesRendered, i + 1 < chapters.size() ? "," : "");
  }
  std::fprintf(f, "  ],\n  \"pages\": [\n");
  std::vector<double> totals;
  double sumLoad = 0, sumPrewarm = 0, sumBw = 0, sumGray = 0;
  for (size_t i = 0; i < pages.size(); i++) {
    const auto& p = pages[i];
    char file[64];
    std::snprintf(file, sizeof(file), "pages/c%03d-p%03d.png", p.spine, p.page);
    std::fprintf(f,
                 "    {\"chapter\": %d, \"page\": %d, \"file\": \"%s\", \"loadMs\": %.3f, \"prewarmMs\": %.3f, "
                 "\"bwRenderMs\": %.3f, \"grayRenderMs\": %.3f}%s\n",
                 p.spine, p.page, file, p.loadMs, p.prewarmMs, p.bwRenderMs, p.grayRenderMs,
                 i + 1 < pages.size() ? "," : "");
    totals.push_back(p.loadMs + p.prewarmMs + p.bwRenderMs + p.grayRenderMs);
    sumLoad += p.loadMs;
    sumPrewarm += p.prewarmMs;
    sumBw += p.bwRenderMs;
    sumGray += p.grayRenderMs;
  }
  double sumBuild = 0;
  int totalPages = 0;
  for (const auto& c : chapters) {
    sumBuild += c.buildMs;
    totalPages += c.pageCount;
  }
  std::fprintf(f, "  ],\n");
  std::fprintf(f,
               "  \"totals\": {\"chapters\": %zu, \"pages\": %d, \"pagesRendered\": %zu, \"buildMs\": %.3f, "
               "\"loadMs\": %.3f, \"prewarmMs\": %.3f, \"bwRenderMs\": %.3f, \"grayRenderMs\": %.3f, "
               "\"pageTotalMedianMs\": %.3f, \"pageTotalP95Ms\": %.3f}\n}\n",
               chapters.size(), totalPages, pages.size(), sumBuild, sumLoad, sumPrewarm, sumBw, sumGray,
               percentile(totals, 0.5), percentile(totals, 0.95));
  return std::fclose(f) == 0;
}

}  // namespace

int main(int argc, char** argv) {
  const Options o = parseArgs(argc, argv);
  harness::verboseLogging = o.verbose;

  const std::string epubPath = absolutePath(o.epubPath);
  if (!Storage.exists(epubPath.c_str())) {
    std::fprintf(stderr, "render-harness: EPUB not found: %s\n", epubPath.c_str());
    return 1;
  }
  if (!Storage.mkdir((o.outDir + "/pages").c_str()) || !Storage.mkdir(o.cacheDir.c_str())) {
    std::fprintf(stderr, "render-harness: cannot create output/cache directories\n");
    return 1;
  }
  const std::string cacheDir = absolutePath(o.cacheDir);

  display.begin();
  GfxRenderer renderer(display);
  renderer.begin();
  FontCacheManager fontCacheManager(renderer.getFontMap(), renderer.getSdCardFonts(), renderer.getTtfFonts());
  FontDecompressor fontDecompressor;
  if (!harness::setupBuiltinFonts(renderer, fontCacheManager, fontDecompressor)) {
    std::fprintf(stderr, "render-harness: font setup failed\n");
    return 1;
  }
  renderer.setOrientation(ORIENTATIONS[o.orientation]);

  const Margins margins = readerMargins(renderer, o);
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const auto viewportW = static_cast<uint16_t>(screenW - margins.left - margins.right);
  const auto viewportH = static_cast<uint16_t>(screenH - margins.top - margins.bottom);
  const ReaderRenderSpec spec = readerSpec(o, viewportW, viewportH);

  // Same load sequence as EpubReaderActivity::loadBook().
  auto epub = std::make_shared<Epub>(epubPath, cacheDir);
  const bool epubCacheHit = Storage.exists((epub->getCachePath() + "/book.bin").c_str());
  const double tLoad0 = nowMs();
  bool loaded;
  {
    GfxRenderer::FrameBufferLoan loan(renderer);
    loaded = epub->load(true, !o.embeddedStyle);
  }
  const double epubLoadMs = nowMs() - tLoad0;
  if (!loaded) {
    std::fprintf(stderr, "render-harness: failed to load EPUB %s\n", epubPath.c_str());
    return 1;
  }
  epub->setupCacheDir();

  const int spineCount = epub->getSpineItemsCount();
  const int firstSpine = std::max(0, o.chapterFirst);
  const int lastSpine = std::min(spineCount - 1, o.chapterLast);
  std::printf("%s: %d spine items, rendering %d-%d at %dx%d (viewport %dx%d)\n", epub->getTitle().c_str(), spineCount,
              firstSpine, lastSpine, screenW, screenH, viewportW, viewportH);

  std::vector<ChapterResult> chapters;
  std::vector<PageTiming> pages;
  std::vector<uint8_t> bwPlane(HalDisplay::BUFFER_SIZE);
  bool anyFailure = false;

  for (int spine = firstSpine; spine <= lastSpine; spine++) {
    ChapterResult chapter{spine, "", false, 0, 0, 0, false};
    const int tocIndex = epub->getTocIndexForSpineIndex(spine);
    if (tocIndex >= 0) chapter.title = epub->getTocItem(tocIndex).title;

    Section section(epub, spine, renderer);
    const double tBuild0 = nowMs();
    chapter.cacheHit = section.loadSectionFile(spec) && !section.isPartial();
    bool buildOk = true;
    if (!chapter.cacheHit) {
      // Both builds start under a framebuffer loan, like the reader's section load.
      GfxRenderer::FrameBufferLoan loan(renderer);
      buildOk = o.incrementalBuild ? section.startBuild(spec) : section.createSectionFile(spec);
    }
    chapter.buildMs = nowMs() - tBuild0;
    // Incremental: lay out more pages until `page` exists or the build ends.
    const auto buildUntil = [&](const int page) {
      while (buildOk && section.isBuilding() && !section.isBuildComplete() &&
             (page < 0 || static_cast<int>(section.pageCount) <= page)) {
        const double tStep = nowMs();
        buildOk = section.buildSomeMore(BUILD_PAGES_PER_CHUNK);
        chapter.buildMs += nowMs() - tStep;
      }
    };
    if (!buildOk) {
      std::fprintf(stderr, "render-harness: section build failed for spine %d\n", spine);
      chapters.push_back(chapter);
      anyFailure = true;
      continue;
    }

    for (int pageIndex = 0; o.maxPages < 0 || pageIndex < o.maxPages; pageIndex++) {
      buildUntil(pageIndex);
      if (!buildOk || pageIndex >= static_cast<int>(section.pageCount)) break;
      PageTiming t{spine, pageIndex, 0, 0, 0, 0};
      const double t0 = nowMs();
      auto page = section.loadPage(pageIndex);
      t.loadMs = nowMs() - t0;
      if (!page) {
        std::fprintf(stderr, "render-harness: failed to load spine %d page %d\n", spine, pageIndex);
        anyFailure = true;
        continue;
      }

      // EpubReaderActivity::renderContents(), non-tiled path, no status bar.
      renderer.clearScreen();
      display.clearGrayPlanes();
      const double tPrewarm0 = nowMs();
      auto scope = fontCacheManager.createPrewarmScope();
      page->render(renderer, spec.fontId, margins.left, margins.top);
      scope.endScanAndPrewarm();
      const double tBw0 = nowMs();
      t.prewarmMs = tBw0 - tPrewarm0;

      page->render(renderer, spec.fontId, margins.left, margins.top);
      t.bwRenderMs = nowMs() - tBw0;
      std::memcpy(bwPlane.data(), renderer.getFrameBuffer(), bwPlane.size());

      if (o.antiAliasing) {
        const double tGray0 = nowMs();
        if (!renderer.storeBwBuffer()) {
          std::fprintf(stderr, "render-harness: storeBwBuffer failed\n");
          return 1;
        }
        renderer.clearScreen(0x00);
        renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
        page->render(renderer, spec.fontId, margins.left, margins.top);
        renderer.copyGrayscaleLsbBuffers();
        renderer.clearScreen(0x00);
        renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
        page->render(renderer, spec.fontId, margins.left, margins.top);
        renderer.copyGrayscaleMsbBuffers();
        renderer.displayGrayBuffer();
        renderer.setRenderMode(GfxRenderer::BW);
        renderer.restoreBwBuffer();
        t.grayRenderMs = nowMs() - tGray0;
      }

      char name[64];
      std::snprintf(name, sizeof(name), "/pages/c%03d-p%03d", spine, pageIndex);
      const std::string base = o.outDir + name;
      const auto image = composeImage(renderer, bwPlane, o.antiAliasing ? display.lsbPlane().data() : nullptr,
                                      o.antiAliasing ? display.msbPlane().data() : nullptr, screenW, screenH);
      bool written = harness::writeGrayPng(base + ".png", screenW, screenH, image);
      if (o.antiAliasing) {
        const auto bwImage = composeImage(renderer, bwPlane, nullptr, nullptr, screenW, screenH);
        written &= harness::writeGrayPng(base + "-bw.png", screenW, screenH, bwImage);
      }
      if (!written) {
        std::fprintf(stderr, "render-harness: failed to write %s.png\n", base.c_str());
        return 1;
      }
      pages.push_back(t);
      chapter.pagesRendered++;
    }
    // Finish the build so pageCount is exact and the cache is committed.
    buildUntil(-1);
    if (!buildOk) {
      std::fprintf(stderr, "render-harness: section build failed for spine %d\n", spine);
      anyFailure = true;
    }
    chapter.ok = buildOk;
    chapter.pageCount = section.pageCount;
    std::printf("  spine %3d: %4d pages, build %8.2f ms%s  %s\n", spine, chapter.pageCount, chapter.buildMs,
                chapter.cacheHit ? " (cached)" : "         ", chapter.title.c_str());
    chapters.push_back(chapter);
  }

  if (!writeReport(o.outDir + "/report.json", o, margins, spec, screenW, screenH, epub->getTitle(), chapters, pages,
                   epubLoadMs, epubCacheHit)) {
    std::fprintf(stderr, "render-harness: failed to write report.json\n");
    return 1;
  }
  std::printf("wrote %zu pages and report.json to %s\n", pages.size(), o.outDir.c_str());
  return anyFailure ? 1 : 0;
}
