#ifndef CRAZYPOD_GAMEBOY_STUB_SYSTEM_H
#define CRAZYPOD_GAMEBOY_STUB_SYSTEM_H

/*
 * The emulator's compat header pulls in system.h for the IRAM section
 * attributes. A host build has no IRAM, so they are all nothing.
 */
#ifndef IDATA_ATTR
#define IDATA_ATTR
#endif
#ifndef IBSS_ATTR
#define IBSS_ATTR
#endif
#ifndef ICODE_ATTR
#define ICODE_ATTR
#endif
#ifndef ICONST_ATTR
#define ICONST_ATTR
#endif

#endif
