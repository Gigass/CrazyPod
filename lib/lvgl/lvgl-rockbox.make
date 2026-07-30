LVGL_DIR := $(ROOTDIR)/lib/lvgl

INCLUDES += -I$(LVGL_DIR) -I$(LVGL_DIR)/src

LVGL_SRC := \
	$(LVGL_DIR)/src/lv_init.c \
	$(shell find $(LVGL_DIR)/src/core -maxdepth 1 -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/display -maxdepth 1 -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/draw -maxdepth 1 -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/draw/convert -maxdepth 1 -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/draw/sw -type f -name '*.c') \
	$(LVGL_DIR)/src/font/lv_font.c \
	$(LVGL_DIR)/src/font/fmt_txt/lv_font_fmt_txt.c \
	$(LVGL_DIR)/src/font/lv_font_montserrat_8.c \
	$(LVGL_DIR)/src/font/lv_font_montserrat_10.c \
	$(LVGL_DIR)/src/font/lv_font_montserrat_12.c \
	$(LVGL_DIR)/src/font/lv_font_crazypod_i18n_8.c \
	$(LVGL_DIR)/src/font/lv_font_crazypod_i18n_10.c \
	$(LVGL_DIR)/src/font/lv_font_crazypod_i18n_12.c \
	$(LVGL_DIR)/src/font/lv_font_source_han_sans_sc_14_cjk.c \
	$(LVGL_DIR)/src/font/lv_font_source_han_sans_sc_16_cjk.c \
	$(LVGL_DIR)/src/font/lv_font_montserrat_16.c \
	$(LVGL_DIR)/src/font/lv_font_montserrat_24.c \
	$(LVGL_DIR)/src/font/lv_font_montserrat_48.c \
	$(shell find $(LVGL_DIR)/src/indev -maxdepth 1 -type f -name '*.c') \
	$(LVGL_DIR)/src/layouts/lv_layout.c \
	$(LVGL_DIR)/src/libs/bin_decoder/lv_bin_decoder.c \
	$(shell find $(LVGL_DIR)/src/misc -maxdepth 1 -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/misc/cache -type f -name '*.c') \
	$(LVGL_DIR)/src/osal/lv_os.c \
	$(LVGL_DIR)/src/osal/lv_os_none.c \
	$(LVGL_DIR)/src/stdlib/lv_mem.c \
	$(shell find $(LVGL_DIR)/src/stdlib/builtin -maxdepth 1 -type f -name '*.c') \
	$(LVGL_DIR)/src/themes/lv_theme.c \
	$(LVGL_DIR)/src/tick/lv_tick.c \
	$(LVGL_DIR)/src/widgets/arc/lv_arc.c \
	$(LVGL_DIR)/src/widgets/image/lv_image.c \
	$(LVGL_DIR)/src/widgets/label/lv_label.c

SRC += $(LVGL_SRC)
OTHER_SRC += $(LVGL_SRC)
