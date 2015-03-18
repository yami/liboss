cmake_minimum_required(VERSION 3.2)

project("liboss")

#include compiler specific settings
include(compiler-${CMAKE_C_COMPILER_ID}.cmake)

add_definitions("-D_FILE_OFFSET_BITS=64")

include_directories(${PROJECT_SOURCE_DIR}/third-party/include)
link_directories(${PROJECT_SOURCE_DIR}/third-party/lib)

set(LIBS libcurl.lib ssleay32.lib libeay32.lib libssh2.lib zlib.lib libxml2.lib libiconv.lib ws2_32.lib wldap32.lib)

set(liboss_srcs
  asprintf.c
  base64.c
  bucket.c
  http.c
  object.c
  util.c
  vasprintf-novacopy.c
  xml.c)

add_library(liboss ${liboss_srcs})
target_link_libraries(liboss ${LIBS})

add_subdirectory(test)