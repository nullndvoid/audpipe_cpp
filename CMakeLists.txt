cmake_minimum_required(VERSION 3.21.0 FATAL_ERROR)
project(audpipe VERSION 0.1.0 LANGUAGES C CXX)

include(cmake/CPM.cmake)

find_package(Threads REQUIRED)

# Force uvgRTP to link to crypto++.
if(POLICY CMP0079)
  cmake_policy(SET CMP0079 NEW)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(audpipe src/main.cpp src/rtp.cpp src/server.cpp
                       src/shutdown.cpp src/client.cpp src/config.cpp)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse)
    target_sources(audpipe PRIVATE src/audio/pulseaudio_backend.cpp src/audio/pulseaudio_callbacks.cpp)
    target_link_libraries(audpipe PRIVATE PkgConfig::PULSE)
elseif(WIN32)
    # TODO:
    target_sources(audpipe PRIVATE src/audio/wasapi_backend.cpp)
endif()

CPMAddPackage(
  NAME cryptopp_cmake
  GIT_REPOSITORY "https://github.com/abdes/cryptopp-cmake"
  GIT_TAG "master" # would use 8.9.0 but they didn't fix some stuff in tagged release.
  OPTIONS "CRYPTOPP_BUILD_TESTING OFF"
)

include(FetchContent)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

FetchContent_Declare(
  uvgRTP
  GIT_REPOSITORY https://github.com/ultravideo/uvgRTP.git
  GIT_TAG        540bfdcb5d01c38220aa256fde954f738afe6b3f
)

set(UVGRTP_DISABLE_PRINTS OFF CACHE BOOL "Disable uvgRTP logging" FORCE)
set(UVGRTP_DISABLE_EXAMPLES ON CACHE BOOL "Disable uvgRTP examples" FORCE)
set(UVGRTP_DISABLE_TESTS ON CACHE BOOL "Disable uvgRTP tests" FORCE)
set(UVGRTP_DISABLE_INSTALL ON CACHE BOOL "Disable uvgRTP install/export" FORCE)

FetchContent_MakeAvailable(uvgRTP)

target_compile_definitions(uvgrtp PRIVATE
  $<$<NOT:$<CONFIG:Debug>>:__RTP_SILENT__>
)

# target_include_directories(uvgrtp PRIVATE ${cryptopp_INCLUDE_PATH})
target_link_libraries(uvgrtp PRIVATE cryptopp::cryptopp)

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

CPMAddPackage(
  NAME asiocmake
  GIT_REPOSITORY "https://github.com/OlivierLDff/asio.cmake"
  GIT_TAG "main"
)

target_link_libraries(audpipe PRIVATE cryptopp::cryptopp uvgrtp opus spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32> tomlplusplus::tomlplusplus asio::asio)
target_include_directories(audpipe PUBLIC include)