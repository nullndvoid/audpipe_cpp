cmake_minimum_required(VERSION 3.10.0)
project(audpipe VERSION 0.1.0 LANGUAGES C CXX)

# Force uvgRTP to link to crypto++.
if(POLICY CMP0079)
  cmake_policy(SET CMP0079 NEW)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(audpipe src/main.cpp src/rtp.cpp src/server.cpp src/client.cpp src/cli.cpp src/config.cpp)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse)
    target_sources(audpipe PRIVATE src/audio/pulseaudio_backend.cpp src/audio/pulseaudio_callbacks.cpp)
    target_link_libraries(audpipe PRIVATE PkgConfig::PULSE)
elseif(WIN32)
    # TODO:
    target_sources(audpipe PRIVATE src/audio/wasapi_backend.cpp)
endif()

include(FetchContent)
include(ExternalProject)

set(CRYPTOPP_INSTALL_DIR ${CMAKE_BINARY_DIR}/_deps/cryptopp-install)
set(CRYPTOPP_LIB_PATH ${CRYPTOPP_INSTALL_DIR}/lib/libcryptopp.a)
set(CRYPTOPP_INCLUDE_PATH ${CRYPTOPP_INSTALL_DIR}/include)
file(MAKE_DIRECTORY ${CRYPTOPP_INSTALL_DIR}/include)
file(MAKE_DIRECTORY ${CRYPTOPP_INSTALL_DIR}/lib/pkgconfig)

# Write a .pc file so uvgRTP's pkg-config check finds cryptopp at configure time.
file(WRITE ${CRYPTOPP_INSTALL_DIR}/lib/pkgconfig/libcrypto++.pc
"prefix=${CRYPTOPP_INSTALL_DIR}
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: libcrypto++
Description: Crypto++ library
Version: 8.9.0
Libs: -L\${libdir} -lcryptopp
Cflags: -I\${includedir}
")

set(ENV{PKG_CONFIG_PATH} "${CRYPTOPP_INSTALL_DIR}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")

include(FetchContent)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

if(NOT EXISTS ${CRYPTOPP_LIB_PATH})
  ExternalProject_Add(cryptopp_ext
    GIT_REPOSITORY https://github.com/weidai11/cryptopp.git
    GIT_TAG 843d74c7c97f9e19a615b8ff3c0ca06599ca501b
    UPDATE_DISCONNECTED TRUE
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE 1
    INSTALL_DIR ${CRYPTOPP_INSTALL_DIR}
    BUILD_COMMAND make static
    INSTALL_COMMAND make PREFIX=<INSTALL_DIR> install
    BUILD_BYPRODUCTS ${CRYPTOPP_LIB_PATH}
  )
endif()

add_library(cryptopp STATIC IMPORTED GLOBAL)
set_target_properties(cryptopp PROPERTIES
  IMPORTED_LOCATION ${CRYPTOPP_LIB_PATH}
  INTERFACE_INCLUDE_DIRECTORIES ${CRYPTOPP_INCLUDE_PATH}
)
if(TARGET cryptopp_ext)
  add_dependencies(cryptopp cryptopp_ext)
endif()

FetchContent_Declare(
  uvgRTP
  GIT_REPOSITORY https://github.com/ultravideo/uvgRTP.git
  GIT_TAG        540bfdcb5d01c38220aa256fde954f738afe6b3f
)

set(UVGRTP_DISABLE_PRINTS OFF CACHE BOOL "Disable uvgRTP logging" FORCE)
set(UVGRTP_DISABLE_EXAMPLES ON CACHE BOOL "Disable uvgRTP examples" FORCE)
set(UVGRTP_DISABLE_TESTS ON CACHE BOOL "Disable uvgRTP tests" FORCE)

FetchContent_MakeAvailable(uvgRTP)

target_compile_definitions(uvgrtp PRIVATE
  $<$<NOT:$<CONFIG:Debug>>:__RTP_SILENT__>
)

target_include_directories(uvgrtp PRIVATE ${CRYPTOPP_INCLUDE_PATH})
target_link_libraries(uvgrtp PRIVATE cryptopp)

# Get libopus for encoding.
FetchContent_Declare(
  Opus
  GIT_REPOSITORY https://github.com/xiph/opus.git
  GIT_TAG        v1.5.2
)
FetchContent_MakeAvailable(Opus)
target_compile_options(opus PRIVATE -O2)
set(OPUS_X86_PRESUME_SSE4_1 ON CACHE BOOL "" FORCE)
set(OPUS_X86_PRESUME_AVX ON CACHE BOOL "" FORCE)
set(OPUS_X86_PRESUME_AVX2 ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG v1.x
)
FetchContent_MakeAvailable(spdlog)

target_link_libraries(audpipe PRIVATE cryptopp uvgrtp opus spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32> tomlplusplus::tomlplusplus)
target_include_directories(audpipe PUBLIC include)