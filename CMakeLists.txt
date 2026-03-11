cmake_minimum_required(VERSION 3.10.0)
project(audpipe VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(audpipe src/main.cpp)

include(FetchContent)
include(ExternalProject)

set(CRYPTOPP_INSTALL_DIR ${CMAKE_BINARY_DIR}/_deps/cryptopp-install)

ExternalProject_Add(cryptopp_ext
  GIT_REPOSITORY https://github.com/weidai11/cryptopp.git
  GIT_TAG 843d74c7c97f9e19a615b8ff3c0ca06599ca501b
  CONFIGURE_COMMAND ""
  BUILD_IN_SOURCE 1
  INSTALL_DIR ${CRYPTOPP_INSTALL_DIR}
  BUILD_COMMAND make static
  INSTALL_COMMAND make PREFIX=<INSTALL_DIR> install
)

add_library(cryptopp STATIC IMPORTED GLOBAL)
set_target_properties(cryptopp PROPERTIES
  IMPORTED_LOCATION ${CRYPTOPP_INSTALL_DIR}/lib/libcryptopp.a
  INTERFACE_INCLUDE_DIRECTORIES ${CRYPTOPP_INSTALL_DIR}/include
)
add_dependencies(cryptopp cryptopp_ext)

FetchContent_Declare(
  uvgRTP
  GIT_REPOSITORY https://github.com/ultravideo/uvgRTP.git
  GIT_TAG        540bfdcb5d01c38220aa256fde954f738afe6b3f
  FIND_PACKAGE_ARGS NAMES uvgRTP
)
FetchContent_MakeAvailable(uvgRTP)

target_link_libraries(audpipe PRIVATE cryptopp uvgrtp)
target_include_directories(audpipe PUBLIC include)