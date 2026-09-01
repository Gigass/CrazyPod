# Link the existing emulator core, without the legacy Rockbox plugin shell.
GAMEBOY_CORE := cpu fastmem hw lcd lcdc mem rtc sound
GAMEBOY_SRC := $(addprefix $(APPSDIR)/plugins/rockboy/, \
    $(addsuffix .c,$(GAMEBOY_CORE)))
SRC += $(GAMEBOY_SRC)
GAMEBOY_OBJ := $(call c2obj,$(GAMEBOY_SRC))
# Dependency generation runs before target-specific CFLAGS are applied. Keep
# the Rockboy compatibility branch active there as well, otherwise plugin.h
# pulls in bitmap headers that are not generated for this target.
PPCFLAGS += -DCRAZYPOD_GAMEBOY_CORE
$(GAMEBOY_OBJ): CFLAGS += -DCRAZYPOD_GAMEBOY_CORE -fno-strict-aliasing
$(BUILDDIR)/apps/crazypod/gameboy/crazypod_gameboy_core.o: \
    CFLAGS += -DCRAZYPOD_GAMEBOY_CORE
