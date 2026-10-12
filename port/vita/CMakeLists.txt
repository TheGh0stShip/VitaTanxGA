cmake_minimum_required(VERSION 3.16)

# The Vita toolchain must be selected before project().  Keeping this target in
# port/vita prevents VitaSDK headers and libraries from leaking into the
# matching N64 build.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "VitaSDK toolchain")
  else()
    message(FATAL_ERROR "Set VITASDK to a VitaSDK installation before configuring this target.")
  endif()
endif()

project(vitatanxga C)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "VitaTanx GA")
# Reserved for this homebrew port; change only before distributing a build.
set(VITA_TITLEID "VTGA00001")
set(VITA_VERSION "00.01")

add_executable(${PROJECT_NAME}
  ../runtime/src/btga_runtime.c
  src/main.c
  src/vita_platform.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
  include
  ../runtime/include
)
target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror -Wpedantic)
target_link_libraries(${PROJECT_NAME} PRIVATE
  vita2d
  SceCtrl_stub
  SceDisplay_stub
  SceGxm_stub
  SceCommonDialog_stub
  SceAppMgr_stub
  ScePgf_stub
  SceShaccCg_stub
  png
  jpeg
  freetype
  z
  m
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})

# Game data is extracted from the user's verified ROM by `make vita-assets`.
# It is deliberately ignored by Git, but each staged file is installed into
# the VPK so app0: is the only runtime asset source.
set(VITA_ASSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/extracted/us/vita")
file(GLOB_RECURSE VITA_GAME_FILES CONFIGURE_DEPENDS "${VITA_ASSET_ROOT}/*")
set(VITA_GAME_FILE_ARGS)
foreach(asset IN LISTS VITA_GAME_FILES)
  if(NOT IS_DIRECTORY "${asset}")
    file(RELATIVE_PATH destination "${VITA_ASSET_ROOT}" "${asset}")
    list(APPEND VITA_GAME_FILE_ARGS FILE "${asset}" "assets/${destination}")
  endif()
endforeach()

vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  ${VITA_GAME_FILE_ARGS}
)
