cmake_minimum_required(VERSION 3.10)
project(Safe-iTLS-Companion C)

set(CMAKE_C_STANDARD 11)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK environment variable to point to your VitaSDK root.")
  endif()
endif()

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(TITLE_ID "ITLS00001")
set(APP_TITLE "Safe-iTLS Companion")

set(SOURCES
    src/main.c
    src/storage.c
    src/downloader.c
    src/hw_crypto.c
    src/pup_extractor.c
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME}
    SceNet_stub
    SceNetCtl_stub
    SceHttp_stub
    SceSsl_stub
    SceCtrl_stub
    ScePower_stub
    SceRtc_stub
    SceSysmodule_stub
    debugscreen
)

vita_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)
vita_create_sfo(param.sfo ${TITLE_ID} ${APP_TITLE})

vita_create_vpk(${PROJECT_NAME}.vpk ${TITLE_ID} eboot.bin
    VERSION "01.00"
    NAME ${APP_TITLE}
    FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
