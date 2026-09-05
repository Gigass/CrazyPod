#include "crazypod_gameboy_core.h"

#include <string.h>

bool crazypod_gameboy_path_supported(const char *path)
{
    const char *ext = path != NULL ? strrchr(path, '.') : NULL;

    if(ext == NULL || ext[1] == '\0' || ext[2] == '\0')
        return false;
    return (ext[1] == 'g' || ext[1] == 'G') &&
        (ext[2] == 'b' || ext[2] == 'B') &&
        (ext[3] == '\0' ||
         ((ext[3] == 'c' || ext[3] == 'C') && ext[4] == '\0'));
}

bool crazypod_gameboy_cartridge_probe(
    const uint8_t *header, size_t header_size, size_t file_size,
    struct crazypod_gameboy_cartridge *cart)
{
    static const size_t rom_sizes[] = {
        32u * 1024u, 64u * 1024u, 128u * 1024u, 256u * 1024u,
        512u * 1024u, 1024u * 1024u, 2u * 1024u * 1024u,
        4u * 1024u * 1024u, 8u * 1024u * 1024u
    };
    static const unsigned ram_sizes[] = {
        0, 2048, 8192, 32768, 131072, 65536
    };
    unsigned rom_code;
    unsigned type;

    if(header == NULL || cart == NULL || header_size < 0x150 ||
       header[0x149] > 5)
        return false;
    memset(cart, 0, sizeof(*cart));
    rom_code = header[0x148];
    if(rom_code <= 8)
        cart->rom_size = rom_sizes[rom_code];
    else if(rom_code == 0x52)
        cart->rom_size = 72u * 16u * 1024u;
    else if(rom_code == 0x53)
        cart->rom_size = 80u * 16u * 1024u;
    else if(rom_code == 0x54)
        cart->rom_size = 96u * 16u * 1024u;
    else
        return false;
    if(file_size != cart->rom_size ||
       file_size > CRAZYPOD_GAMEBOY_ROM_MAX)
        return false;
    cart->ram_size = ram_sizes[header[0x149]];
    cart->color = (header[0x143] & 0x80) != 0;
    type = header[0x147];
    switch(type) {
    case 0x00: case 0x08: case 0x09:
        cart->mapper = 0;
        break;
    case 0x01: case 0x02: case 0x03:
        cart->mapper = 1;
        break;
    case 0x05: case 0x06:
        cart->mapper = 2;
        /* MBC2 contains 512 four-bit RAM cells, not an external SRAM. */
        cart->ram_size = 512;
        break;
    case 0x0f: case 0x10: case 0x11: case 0x12: case 0x13:
        cart->mapper = 4;
        cart->clock = type == 0x0f || type == 0x10;
        break;
    case 0x19: case 0x1a: case 0x1b:
        cart->mapper = 8;
        break;
    case 0x1c: case 0x1d: case 0x1e:
        cart->mapper = 16;
        break;
    default:
        return false;
    }
    cart->battery = type == 0x03 || type == 0x06 || type == 0x09 ||
        type == 0x0f || type == 0x10 || type == 0x13 || type == 0x1b ||
        type == 0x1e;
    return true;
}
