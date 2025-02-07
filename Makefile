all: desktop

-include .config

desktop:
	@make -C ports/unix deplibs
	@make -C ports/unix MICROPY_PY_LVGL=1 \
		LVGL_RESOURCES_DIR=$(LVGL_RESOURCES_DIR) LVGL_RESOURCES_HEADER=$(LVGL_RESOURCES_HEADER)

embed: export CROSS_COMPILE=$(_CROSS_COMPILE)
embed: export STAGING_DIR=$(_STAGING_DIR)
embed: export LDFLAGS_EXTRA=$(_LDFLAGS_EXTRA)
embed:
	# libffi 需要 MICROPY_STANDALONE=1
	@make -C ports/unix MICROPY_STANDALONE=1 deplibs
	@make -C ports/unix MICROPY_STANDALONE=1 \
		MICROPY_PY_LVGL=1 MICROPY_PY_UBUS=1 MICROPY_PY_EVDEV=1 LVGL_RESOURCES_DIR=$(LVGL_RESOURCES_DIR) \
		LVGL_RESOURCES_HEADER=$(LVGL_RESOURCES_HEADER)

clean:
	@make -C ports/unix clean

.PHONY: all desktop embed clean
