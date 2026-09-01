# Link the existing emulator core, without the legacy Rockbox plugin shell.
GAMEBOY_CORE := cpu fastmem hw lcd lcdc mem rtc sound
GAMEBOY_SRC := $(addprefix $(APPSDIR)/plugins/rockboy/, \
    $(addsuffix .c,$(GAMEBOY_CORE)))
SRC += $(GAMEBOY_SRC)
GAMEBOY_OBJ := $(call c2obj,$(GAMEBOY_SRC))
$(GAMEBOY_OBJ): CFLAGS += -DCRAZYPOD_GAMEBOY_CORE -fno-strict-aliasing
$(BUILDDIR)/apps/crazypod/gameboy/crazypod_gameboy_core.o: \
    CFLAGS += -DCRAZYPOD_GAMEBOY_CORE
