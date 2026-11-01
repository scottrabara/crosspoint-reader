cmake_minimum_required(VERSION 3.16)
project(crosspoint_render_harness C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(ZLIB REQUIRED)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(SDK_ROOT "${REPO_ROOT}/freeink-sdk")

execute_process(
  COMMAND git -C "${REPO_ROOT}" rev-parse --short=12 HEAD
  OUTPUT_VARIABLE HARNESS_GIT_SHA
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
if(NOT HARNESS_GIT_SHA)
  set(HARNESS_GIT_SHA "unknown")
endif()

# Vendored C libraries, built with the firmware's flags (platformio.ini).
add_library(rh_expat STATIC
  ${REPO_ROOT}/lib/expat/xmlparse.c
  ${REPO_ROOT}/lib/expat/xmlrole.c
  ${REPO_ROOT}/lib/expat/xmltok.c)
target_include_directories(rh_expat PUBLIC ${REPO_ROOT}/lib/expat)
target_compile_definitions(rh_expat PUBLIC XML_GE=0 XML_CONTEXT_BYTES=0)

add_library(rh_cdeps STATIC
  ${REPO_ROOT}/lib/uzlib/src/tinflate.c
  ${REPO_ROOT}/lib/miniz/src/miniz_impl.c
  ${REPO_ROOT}/lib/MiniBidi/minibidi.c
  ${SDK_ROOT}/libs/font/FreeInkFont/src/FontAlloc.c)
target_include_directories(rh_cdeps PUBLIC
  ${REPO_ROOT}/lib/uzlib/src
  ${REPO_ROOT}/lib/miniz/src
  ${REPO_ROOT}/lib/MiniBidi
  ${SDK_ROOT}/libs/font/FreeInkFont/include)

add_executable(render-harness
  main.cpp
  HarnessFonts.cpp
  HarnessLinkStubs.cpp
  HostDisplay.cpp
  HostStorage.cpp
  PngWriter.cpp

  ${REPO_ROOT}/lib/Epub/Epub.cpp
  ${REPO_ROOT}/lib/Epub/Epub/BookMetadataCache.cpp
  ${REPO_ROOT}/lib/Epub/Epub/CjkBreaks.cpp
  ${REPO_ROOT}/lib/Epub/Epub/Page.cpp
  ${REPO_ROOT}/lib/Epub/Epub/ParsedText.cpp
  ${REPO_ROOT}/lib/Epub/Epub/Section.cpp
  ${REPO_ROOT}/lib/Epub/Epub/WordStore.cpp
  ${REPO_ROOT}/lib/Epub/Epub/htmlEntities.cpp
  ${REPO_ROOT}/lib/Epub/Epub/blocks/ImageBlock.cpp
  ${REPO_ROOT}/lib/Epub/Epub/blocks/TextBlock.cpp
  ${REPO_ROOT}/lib/Epub/Epub/converters/ImageDimsProbe.cpp
  ${REPO_ROOT}/lib/Epub/Epub/converters/ImageToFramebufferDecoder.cpp
  ${REPO_ROOT}/lib/Epub/Epub/css/CssParser.cpp
  ${REPO_ROOT}/lib/Epub/Epub/hyphenation/HyphenationCommon.cpp
  ${REPO_ROOT}/lib/Epub/Epub/hyphenation/Hyphenator.cpp
  ${REPO_ROOT}/lib/Epub/Epub/hyphenation/LanguageRegistry.cpp
  ${REPO_ROOT}/lib/Epub/Epub/hyphenation/LiangHyphenation.cpp
  ${REPO_ROOT}/lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp
  ${REPO_ROOT}/lib/Epub/Epub/parsers/ContainerParser.cpp
  ${REPO_ROOT}/lib/Epub/Epub/parsers/ContentOpfParser.cpp
  ${REPO_ROOT}/lib/Epub/Epub/parsers/TocNavParser.cpp
  ${REPO_ROOT}/lib/Epub/Epub/parsers/TocNcxParser.cpp

  ${REPO_ROOT}/lib/GfxRenderer/Bitmap.cpp
  ${REPO_ROOT}/lib/GfxRenderer/BitmapHelpers.cpp
  ${REPO_ROOT}/lib/GfxRenderer/FontCacheManager.cpp
  ${REPO_ROOT}/lib/GfxRenderer/GfxRenderer.cpp
  ${REPO_ROOT}/lib/EpdFont/EpdFont.cpp
  ${REPO_ROOT}/lib/EpdFont/EpdFontFamily.cpp
  ${REPO_ROOT}/lib/EpdFont/FontDecompressor.cpp
  ${REPO_ROOT}/lib/EpdFont/SdCardFont.cpp
  ${REPO_ROOT}/lib/InflateReader/InflateReader.cpp

  ${REPO_ROOT}/lib/ZipFile/ZipFile.cpp
  ${REPO_ROOT}/lib/miniz/src/InflateStream.cpp
  ${REPO_ROOT}/lib/Memory/BuildScratch.cpp
  ${REPO_ROOT}/lib/hal/HalMemory.cpp
  ${REPO_ROOT}/lib/MiniBidi/BidiUtils.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
  ${REPO_ROOT}/lib/FsHelpers/FsHelpers.cpp
  ${REPO_ROOT}/lib/Txt/Txt.cpp
  ${REPO_ROOT}/lib/Txt/TxtToHtml.cpp)

# shim/ must come first: it replaces the Arduino core, HAL and SDK headers.
# lib/hal and test/*/stubs are deliberately absent.
target_include_directories(render-harness PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/shim
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/GfxRenderer
  ${REPO_ROOT}/lib/EpdFont
  ${REPO_ROOT}/lib/InflateReader
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/lib/Memory
  ${REPO_ROOT}/lib/Serialization
  ${REPO_ROOT}/lib/FsHelpers
  ${REPO_ROOT}/lib/XmlParserUtils
  ${REPO_ROOT}/lib/AsciiText
  ${REPO_ROOT}/lib/ZipFile
  ${REPO_ROOT}/lib/Txt
  ${REPO_ROOT}/lib/JpegToBmpConverter
  ${REPO_ROOT}/lib/PngToBmpConverter
  ${REPO_ROOT}/lib/GifToBmpConverter
  ${REPO_ROOT}/lib/GifSupport)

target_compile_definitions(render-harness PRIVATE HARNESS_GIT_SHA="${HARNESS_GIT_SHA}")
target_compile_options(render-harness PRIVATE -Wall -Wextra)
target_link_libraries(render-harness PRIVATE rh_expat rh_cdeps ZLIB::ZLIB)
# Generated font headers leave the runtime handler fields zero-initialized.
set_source_files_properties(HarnessFonts.cpp PROPERTIES COMPILE_OPTIONS "-Wno-missing-field-initializers")
