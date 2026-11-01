#pragma once

class GfxRenderer;
class FontCacheManager;
class FontDecompressor;

namespace harness {
// Registers the built-in compressed fonts exactly as src/main.cpp setupDisplayAndFonts() does.
bool setupBuiltinFonts(GfxRenderer& renderer, FontCacheManager& fontCacheManager, FontDecompressor& fontDecompressor);
}  // namespace harness
