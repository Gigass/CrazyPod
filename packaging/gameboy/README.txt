CrazyPod GB / GBC

Place your legally obtained .gb / .gbc ROMs in /MiniApps/Games,
/MiniApps/Games/GB or /MiniApps/Games/GBC. Open the GB / GBC desktop icon.
Re-enter the app to refresh.
Keep .cpk apps directly in /MiniApps.
Up to 128 ROMs are listed. ZIP and GBA files are not supported.
No games or Nintendo boot ROM are included.
Move any ROMs from the old /Games directory into /MiniApps/Games;
the old directory is no longer scanned. Battery saves do not need moving.

iPod Classic controls (touch wheel, clockwise from 12 o'clock):
                  UP
             B          A
          LEFT            RIGHT
          SELECT        START
                 DOWN

Center click: A. Any outer click: B (plus the touched wheel sector).
Hold Center for 1 second: pause menu.
Pause menu: scroll to choose, release Center to confirm.
START and SELECT can also be sent from the pause menu.
The game initially opens paused so you can read these controls.

Simulator: Menu/Play/Left/Right = Up/Down/Left/Right;
Center = A; scroll wheel = a short B press. Use the pause menu for
START/SELECT. Hold Center for the pause menu.

Game audio replaces music playback. Set volume before launching.
Save and exit keeps the cartridge's battery-backed save and RTC in
/.crazypod/gameboy/. The filename is the SHA-256 of the ROM contents.
Renaming the ROM preserves its save; modified ROMs have separate saves.
These are in-game battery saves, not emulator quick-save states.
HOLD, USB connection and system shutdown also attempt to save and exit.
On a normal save failure, the game stays in memory: choose Save and exit
again to retry. A corrupt save is refused and is never overwritten.
Do not disconnect power while saving. Back up /.crazypod/gameboy/ with
your other user data. Saves from other emulators are not imported.

Core: Rockboy / gnuboy, reused under the repository's GPL license.
Supported mapper families: ROM, MBC1, MBC2, MBC3, MBC5 (including rumble
carts, without physical rumble). Camera, MBC6/7, HuC and other special
cartridges, link cable and GBA are not supported. Compatibility and speed
vary by game; real iPod validation is required before a release.
