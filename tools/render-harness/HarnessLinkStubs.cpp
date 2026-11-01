// Link-time stand-ins for firmware pieces the harness leaves out: protected
// books, cover converters, and image decoders (images are skipped in v1, so
// the parser falls back to alt text exactly as it does for an unsupported
// format on the device).

#include <ContentProtection.h>
#include <Epub/converters/ImageDecoderFactory.h>
#include <GifToBmpConverter.h>
#include <JpegToBmpConverter.h>
#include <PngToBmpConverter.h>

#include <cstdint>
#include <cstdlib>

namespace freeink::content {
std::unique_ptr<ContentDecryptor> openProtectedBook(const std::string&, std::string&) { return nullptr; }
}  // namespace freeink::content

bool JpegToBmpConverter::jpegFileToBmpStream(HalFile&, Print&, bool, bool) { return false; }
bool JpegToBmpConverter::jpegFileToBmpStreamWithSize(HalFile&, Print&, int, int) { return false; }
bool JpegToBmpConverter::jpegFileTo1BitBmpStreamWithSize(HalFile&, Print&, int, int) { return false; }
bool PngToBmpConverter::pngFileToBmpStream(HalFile&, Print&, bool, bool) { return false; }
bool PngToBmpConverter::pngFileToBmpStreamWithSize(HalFile&, Print&, int, int) { return false; }
bool PngToBmpConverter::pngFileTo1BitBmpStreamWithSize(HalFile&, Print&, int, int) { return false; }
bool GifToBmpConverter::gifFileToBmpStream(HalFile&, Print&, bool, bool) { return false; }
bool GifToBmpConverter::gifFileTo1BitBmpStreamWithSize(HalFile&, Print&, int, int) { return false; }

ImageToFramebufferDecoder* ImageDecoderFactory::getDecoder(const std::string&) { return nullptr; }
bool ImageDecoderFactory::isFormatSupported(const std::string&) { return false; }

// Vendored uzlib ships without its checksum sources; font groups are raw
// deflate, so these are never reached (same as test/inflate_reader).
extern "C" uint32_t uzlib_adler32(const void*, unsigned int, uint32_t) { std::abort(); }
extern "C" uint32_t uzlib_crc32(const void*, unsigned int, uint32_t) { std::abort(); }
