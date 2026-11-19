cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK environment variable to point to your VitaSDK root.")
  endif()
endif()

project(Safe-iTLS-Companion C)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,max-page-size=0x10000 -Wl,-z,common-page-size=0x10000 -Wl,--defsym=__sce_headroom=0x10000")

set(TITLE_ID "ITLS00001")
set(APP_TITLE "Safe-iTLS Companion")

set(SOURCES
    src/main.c
    src/debug_screen.c
    src/storage.c
    src/downloader.c
    src/hw_crypto.c
    src/pup_extractor.c
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME}
    SceDisplay_stub
    SceNet_stub
    SceNetCtl_stub
    SceCtrl_stub
    ScePower_stub
    SceRtc_stub
    SceSysmodule_stub
    SceSblSsMgr_stub
)

vita_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)

vita_create_vpk(${PROJECT_NAME}.vpk ${TITLE_ID} eboot.bin
    VERSION "01.00"
    NAME ${APP_TITLE}
    FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
