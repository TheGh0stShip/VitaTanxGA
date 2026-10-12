PYTHON ?= python3
ROM ?= baseroms/us/baserom.z64
VITA_ASSETS := assets/extracted/us/vita
VITA_BUILD := port/vita/build

.PHONY: all verify-rom runtime-test runtime-asset-test runtime-script-asset-test runtime-campaign-asset-test vita-assets vita vita-recomp-bootstrap vita-recomp-prepare vita-recomp-check vita-recomp-runtime-check vita-check clean

all: vita-check

verify-rom:
	$(PYTHON) tools/verify_rom.py $(ROM)

runtime-test:
	$(CC) -std=c11 -Wall -Wextra -Werror -Wpedantic \
		-Iport/runtime/include port/runtime/src/btga_runtime.c \
		port/runtime/src/btga_campaign.c \
		port/runtime/src/btga_tank.c \
		port/runtime/src/btga_script.c \
		port/runtime/src/btga_world.c port/runtime/src/btga_geometry.c \
		port/runtime/src/btga_scene.c port/runtime/src/btga_view.c \
		port/runtime/src/btga_terrain.c port/runtime/src/btga_material.c \
		port/runtime/src/btga_texture.c \
		port/runtime/tests/tank_test.c \
		port/runtime/tests/campaign_test.c \
		port/runtime/tests/script_test.c \
		port/runtime/tests/runtime_test.c port/runtime/tests/world_test.c \
		port/runtime/tests/geometry_test.c port/runtime/tests/view_test.c \
		port/runtime/tests/material_test.c \
		port/runtime/tests/texture_test.c \
		-lm -o /tmp/vitatanxga-runtime-test
	/tmp/vitatanxga-runtime-test

runtime-asset-test: vita-assets
	$(CC) -std=c11 -Wall -Wextra -Werror -Wpedantic \
		-Iport/runtime/include port/runtime/src/btga_world.c \
		port/runtime/src/btga_tank.c \
		port/runtime/src/btga_geometry.c port/runtime/src/btga_scene.c \
		port/runtime/src/btga_view.c port/runtime/src/btga_terrain.c \
		port/runtime/src/btga_material.c \
		port/runtime/src/btga_texture.c \
		port/runtime/tests/world_asset_test.c -lm -o /tmp/vitatanxga-world-asset-test
	/tmp/vitatanxga-world-asset-test $(VITA_ASSETS)/worlds/level_world_127 \
		$(VITA_ASSETS)/world-pools/geometry.bin \
		$(VITA_ASSETS)/world-pools/state.bin \
		$(VITA_ASSETS)/world-pools/texture.bin \
		$(VITA_ASSETS)/worlds/common_world \
		$(VITA_ASSETS)/worlds/level_world_131 \
		$(VITA_ASSETS)/gameplay/gameplay.bin \
		$(VITA_ASSETS)/gameplay/projectile_0.i8 \
		$(VITA_ASSETS)/gameplay/projectile_1.i8 \
		$(VITA_ASSETS)/gameplay/projectile_2.i8

runtime-script-asset-test: vita-assets
	$(CC) -std=c11 -Wall -Wextra -Werror -Wpedantic \
		-Iport/runtime/include port/runtime/src/btga_script.c \
		port/runtime/tests/script_asset_test.c \
		-o /tmp/vitatanxga-script-asset-test
	/tmp/vitatanxga-script-asset-test \
		$(VITA_ASSETS)/scripts/script_00_514720/stream_*.bin

runtime-campaign-asset-test: vita-assets
	$(CC) -std=c11 -Wall -Wextra -Werror -Wpedantic \
		-Iport/runtime/include port/runtime/src/btga_campaign.c \
		port/runtime/tests/campaign_asset_test.c \
		-o /tmp/vitatanxga-campaign-asset-test
	/tmp/vitatanxga-campaign-asset-test \
		$(VITA_ASSETS)/campaign/campaign_text.bin \
		$(VITA_ASSETS)/campaign/campaign_config.bin

vita-assets: verify-rom
	$(PYTHON) tools/extract_world_pools.py $(VITA_ASSETS)/world-pools --rom $(ROM)
	$(PYTHON) tools/inventory_lzari_assets.py --rom $(ROM) --output $(VITA_ASSETS)/lzari_manifest.json --extract-dir $(VITA_ASSETS)/lzari --split-bundles
	$(PYTHON) tools/extract_lzari_images.py $(VITA_ASSETS)/images --rom $(ROM)
	$(PYTHON) tools/extract_lzari_worlds.py $(VITA_ASSETS)/worlds --rom $(ROM)
	$(PYTHON) tools/extract_gameplay_data.py $(VITA_ASSETS)/gameplay --rom $(ROM)
	$(PYTHON) tools/extract_vita_audio.py $(VITA_ASSETS)/audio --rom $(ROM)
	$(PYTHON) tools/extract_campaign_data.py $(VITA_ASSETS)/campaign --rom $(ROM)
	$(PYTHON) tools/inventory_script_assets.py --rom $(ROM) --output $(VITA_ASSETS)/scripts_manifest.json --extract-dir $(VITA_ASSETS)/scripts
	$(PYTHON) tools/inventory_libmus_assets.py --rom $(ROM) --output $(VITA_ASSETS)/libmus_manifest.json --extract-dir $(VITA_ASSETS)/libmus

vita: vita-assets
	@test -n "$(VITASDK)" || (echo "Set VITASDK to your VitaSDK installation" >&2; exit 2)
	cmake -S port/vita -B $(VITA_BUILD) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(VITA_BUILD)

vita-recomp-bootstrap:
	port/recomp/bootstrap.sh

port/recomp/generated/.prepared: port/recomp/btga.toml port/recomp/n_aspMain.toml \
		port/recomp/runtime_compat.h port/recomp/prepare.sh \
		config/vita/recomp_function_boundaries.toml \
		tools/make_vita_recomp_header.py $(ROM)
	port/recomp/prepare.sh
	@touch $@

vita-recomp-prepare: port/recomp/generated/.prepared

vita-recomp-check: vita-recomp-prepare
	@test -n "$(VITASDK)" || (echo "Set VITASDK to your VitaSDK installation" >&2; exit 2)
	$(CC) -std=c11 -Wall -Wextra -Werror -Wpedantic -U__SIZEOF_INT128__ \
		-Iport/recomp/generated/include port/runtime/tests/recomp_math_test.c \
		-lm -o /tmp/vitatanxga-recomp-math-test
	/tmp/vitatanxga-recomp-math-test
	cmake -S port/vita -B $(VITA_BUILD)-recomp -DCMAKE_BUILD_TYPE=Release -DBTGA_BUILD_RECOMP=ON
	cmake --build $(VITA_BUILD)-recomp --target btga_recompiled_game_armv7
	@for object in $$(find $(VITA_BUILD)-recomp/CMakeFiles/btga_recompiled_game_armv7.dir \
		-name '*.obj' -type f); do \
		$(VITASDK)/bin/arm-vita-eabi-readelf -h "$$object" | \
			grep -q 'Class:.*ELF32' || exit 1; \
		$(VITASDK)/bin/arm-vita-eabi-readelf -h "$$object" | \
			grep -q 'Machine:.*ARM' || exit 1; \
		$(VITASDK)/bin/arm-vita-eabi-readelf -A "$$object" | \
			grep -q 'Tag_CPU_arch: v7' || exit 1; \
		$(VITASDK)/bin/arm-vita-eabi-readelf -A "$$object" | \
			grep -q 'Tag_ABI_VFP_args: VFP registers' || exit 1; \
	done
	@echo "Verified all translated objects as ELF32 ARMv7 hard-float"

vita-recomp-runtime-check: vita-recomp-prepare
	cmake -S port/vita -B $(VITA_BUILD)-runtime \
		-DBTGA_BUILD_RECOMP=ON -DBTGA_BUILD_RECOMP_RUNTIME=ON
	cmake --build $(VITA_BUILD)-runtime --target btga_vita_recomp_runtime
	@for archive in \
		$(VITA_BUILD)-runtime/libbtga_recompiled_game_armv7.a \
		$(VITA_BUILD)-runtime/libbtga_vita_recomp_runtime.a; do \
		test -s $$archive || exit 1; \
	done
	@for object in $$(find $(VITA_BUILD)-runtime/CMakeFiles/btga_vita_recomp_runtime.dir \
		-name '*.obj' -type f); do \
		$(VITASDK)/bin/arm-vita-eabi-readelf -h "$$object" | \
			grep -q 'Class:.*ELF32' || exit 1; \
		$(VITASDK)/bin/arm-vita-eabi-readelf -h "$$object" | \
			grep -q 'Machine:.*ARM' || exit 1; \
		$(VITASDK)/bin/arm-vita-eabi-readelf -A "$$object" | \
			grep -q 'Tag_CPU_arch: v7' || exit 1; \
		$(VITASDK)/bin/arm-vita-eabi-readelf -A "$$object" | \
			grep -q 'Tag_ABI_VFP_args: VFP registers' || exit 1; \
	done
	@echo "Compiled translated game, audio microcode, and Vita runtime services for ARMv7"

vita-check: runtime-test runtime-asset-test runtime-script-asset-test runtime-campaign-asset-test vita
	port/vita/verify_elf.sh $(VITA_BUILD)/vitatanxga \
		$(VITASDK)/arm-vita-eabi/lib/libvita2d.a \
		$(VITASDK)/arm-vita-eabi/lib/libpng.a \
		$(VITASDK)/arm-vita-eabi/lib/libjpeg.a \
		$(VITASDK)/arm-vita-eabi/lib/libfreetype.a \
		$(VITASDK)/arm-vita-eabi/lib/libz.a

clean:
	$(RM) -r $(VITA_ASSETS) $(VITA_BUILD)
