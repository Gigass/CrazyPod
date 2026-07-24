# CrazyPod independent product firmware

This revision establishes CrazyPod as a 6G-only independent firmware product
with local music playback and device-applicable DIY customization.

Implemented:

- LVGL 9.5.0 low-level integration
- dedicated RGB565 framebuffer and panic display
- MaxPodApp-derived animated 14-application icon carousel
- click-wheel focus, Select, Menu, Left, Right, and Play handling
- animated 3D carousel focus transitions, reflections, position indicators,
  default MaxPod wallpaper, and the compressed 6G now-playing capsule
- live battery and clock status
- minimal USB mass-storage configuration
- recursive local metadata scan on a background thread
- artists, albums, songs, M3U/M3U8 playlists, and local text search
- album artwork, queue, shuffle, repeat, resume persistence, codec decoding,
  buffering, and PCM output
- MaxPod-derived icon, detail, background, and appearance-preset controls
- validated, versioned `.upodtheme` import/export
- 16 packaged 72×72 application icon themes
- hardware package containing playback codecs, wallpaper, and CrazyPod icon
  resources

Removed from the product:

- Rockbox root menu and browser UI
- WPS and skin engine
- Apple2026 shell, assets, fonts, and generators
- plugins and the recording/encoder pipeline
- USB Audio, HID, and iPod accessory protocol
- iPod 5G product target

Intentionally not implemented:

- network music, online lyrics, and network import
- physical chassis and click-wheel DIY options
- product logic for applications other than Music and Customize
- physical iPod validation
