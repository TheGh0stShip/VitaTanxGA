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

option(BTGA_BUILD_RECOMP "ARM compile-check locally generated full-game translation" OFF)

set(VITA_APP_NAME "VitaTanx GA")
# Reserved for this homebrew port; change only before distributing a build.
set(VITA_TITLEID "VTGA00001")
set(VITA_VERSION "00.01")

add_executable(${PROJECT_NAME}
  ../runtime/src/btga_runtime.c
  ../runtime/src/btga_campaign.c
  ../runtime/src/btga_tank.c
  ../runtime/src/btga_script.c
  ../runtime/src/btga_world.c
  ../runtime/src/btga_geometry.c
  ../runtime/src/btga_scene.c
  ../runtime/src/btga_view.c
  ../runtime/src/btga_terrain.c
  ../runtime/src/btga_material.c
  ../runtime/src/btga_texture.c
  src/main.c
  src/vita_platform.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
  include
  ../runtime/include
)
target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror -Wpedantic)
# Leave room between RX and RW load segments for vita-elf-create's SCE
# metadata. GNU ld's default adjacent-page layout becomes invalid once the
# executable text approaches the next 64 KiB boundary.
target_link_options(${PROJECT_NAME} PRIVATE
  "-Wl,--section-start=.init_array=0x8103F000"
  "-Wl,-Tdata=0x81040000"
)
target_link_libraries(${PROJECT_NAME} PRIVATE
  vita2d
  SceCtrl_stub
  SceAudio_stub
  SceDisplay_stub
  SceGxm_stub
  SceCommonDialog_stub
  SceAppMgr_stub
  SceIofilemgr_stub
  ScePgf_stub
  SceShaccCg_stub
  png
  jpeg
  freetype
  z
  m
)

if(BTGA_BUILD_RECOMP)
  set(BTGA_RECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../recomp")
  set(BTGA_RECOMP_DIR "${BTGA_RECOMP_ROOT}/generated/functions")
  set(BTGA_RECOMP_INCLUDE
      "${CMAKE_CURRENT_SOURCE_DIR}/../../.deps/N64ModernRuntime/N64Recomp/include")
  if(NOT EXISTS "${BTGA_RECOMP_DIR}/funcs.h")
    message(FATAL_ERROR "Run port/recomp/prepare.sh before BTGA_BUILD_RECOMP")
  endif()
  if(NOT EXISTS "${BTGA_RECOMP_INCLUDE}/recomp.h")
    message(FATAL_ERROR "Run port/recomp/bootstrap.sh before BTGA_BUILD_RECOMP")
  endif()
  file(GLOB BTGA_RECOMP_C CONFIGURE_DEPENDS "${BTGA_RECOMP_DIR}/funcs_*.c")
  list(LENGTH BTGA_RECOMP_C BTGA_RECOMP_UNIT_COUNT)
  if(NOT BTGA_RECOMP_UNIT_COUNT EQUAL 35)
    message(FATAL_ERROR "Expected 35 full-game translation units, found ${BTGA_RECOMP_UNIT_COUNT}")
  endif()
  add_library(btga_recompiled_game_armv7 STATIC ${BTGA_RECOMP_C})
  target_include_directories(btga_recompiled_game_armv7 PRIVATE
    "${BTGA_RECOMP_DIR}"
    "${BTGA_RECOMP_ROOT}/generated/include"
    "${BTGA_RECOMP_INCLUDE}"
  )
  target_compile_options(btga_recompiled_game_armv7 PRIVATE
    -fno-strict-aliasing -Wno-unused-variable
    "-include${BTGA_RECOMP_ROOT}/runtime_compat.h"
  )
endif()

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})

# Game data is extracted from the user's verified ROM by `make vita-assets`.
# It is deliberately ignored by Git. Package only assets opened by this
# executable; decoded previews, unused worlds, inventories, and future audio
# banks do not belong in the installable title.
set(VITA_ASSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/extracted/us/vita")
set(VITA_GAME_FILES
  "${VITA_ASSET_ROOT}/world-pools/geometry.bin"
  "${VITA_ASSET_ROOT}/world-pools/state.bin"
  "${VITA_ASSET_ROOT}/world-pools/texture.bin"
  "${VITA_ASSET_ROOT}/gameplay/gameplay.bin"
  "${VITA_ASSET_ROOT}/gameplay/projectile_0.i8"
  "${VITA_ASSET_ROOT}/gameplay/projectile_1.i8"
  "${VITA_ASSET_ROOT}/gameplay/projectile_2.i8"
  "${VITA_ASSET_ROOT}/audio/sfx_bank.pcm16le"
  "${VITA_ASSET_ROOT}/audio/sfx_index.bin"
  "${VITA_ASSET_ROOT}/audio/effect_wave_map.bin"
  "${VITA_ASSET_ROOT}/campaign/campaign_text.bin"
  "${VITA_ASSET_ROOT}/campaign/campaign_config.bin"
)
file(GLOB VITA_COMMON_WORLD_FILES CONFIGURE_DEPENDS
  "${VITA_ASSET_ROOT}/worlds/common_world/component*.bin")
set(VITA_CAMPAIGN_WORLD_IDS
  000 003 005 009 011 017 020 023 025 030 031 036 038 043 045 049
  051 055 057 060 061 066 068 072 074 078 080 084 086 092 094 098
  100 103 123 127 131)
set(VITA_CAMPAIGN_WORLD_FILES)
foreach(world_id IN LISTS VITA_CAMPAIGN_WORLD_IDS)
  file(GLOB mission_world_files CONFIGURE_DEPENDS
    "${VITA_ASSET_ROOT}/worlds/level_world_${world_id}/component*.bin")
  list(APPEND VITA_CAMPAIGN_WORLD_FILES ${mission_world_files})
endforeach()
file(GLOB_RECURSE VITA_MISSION_SCRIPT_FILES CONFIGURE_DEPENDS
  "${VITA_ASSET_ROOT}/scripts/script_*/header.bin"
  "${VITA_ASSET_ROOT}/scripts/script_*/stream_*.bin")
list(APPEND VITA_GAME_FILES
  ${VITA_COMMON_WORLD_FILES}
  ${VITA_CAMPAIGN_WORLD_FILES}
  ${VITA_MISSION_SCRIPT_FILES}
)
foreach(asset IN LISTS VITA_GAME_FILES)
  if(NOT EXISTS "${asset}")
    message(FATAL_ERROR "Required extracted Vita asset is missing: ${asset}")
  endif()
endforeach()
set(VITA_GAME_FILE_ARGS)
foreach(asset IN LISTS VITA_GAME_FILES)
  file(RELATIVE_PATH destination "${VITA_ASSET_ROOT}" "${asset}")
  list(APPEND VITA_GAME_FILE_ARGS FILE "${asset}" "assets/${destination}")
endforeach()

vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  ${VITA_GAME_FILE_ARGS}
)
