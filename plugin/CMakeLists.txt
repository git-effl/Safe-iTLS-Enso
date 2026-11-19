cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK environment variable to point to your VitaSDK root.")
  endif()
endif()

project(itls C)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -std=gnu99 -nostdlib")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,max-page-size=0x10000 -Wl,-z,common-page-size=0x10000 -Wl,--defsym=__sce_headroom=0x10000")

add_executable(itls
  src/main.c
  src/redirect.c
)

target_include_directories(itls PRIVATE src)

target_link_libraries(itls
  taihenForKernel_stub
  SceIofilemgrForDriver_stub
  SceSysclibForDriver_stub
  SceDebugForDriver_stub
)

vita_create_self(itls.skprx itls CONFIG exports.yml UNSAFE)
