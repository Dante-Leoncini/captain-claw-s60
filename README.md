# Captain Claw for Nokia S60 (N95)

An unfinished port of **Captain Claw** (Monolith, 1997) to Symbian S60 3rd Edition phones. It is
built on [OpenClaw](https://github.com/pjasicek/OpenClaw), the open source reimplementation of the
game by Pavel Jasiček and contributors. It runs on a Nokia N95: OpenGL ES 1.1 on the PowerVR MBX,
movies, sound and music.

> **It is not finished, and I will not continue it.** The game is playable: menus, the first level,
> the movies, saving at checkpoints. But many details are still missing or wrong; see
> [Known problems](#known-problems). I am leaving it here for anyone interested in fixing it.

> **No game data is included.** The installer contains this port only. You need your own copy of
> Captain Claw and have to copy two of its files to the phone by hand (see [Installing](#installing)).

The port was made with [Claude](https://claude.ai) (Anthropic's AI model, through Claude Code), with me
directing and testing it on a real N95.

## Installing

Tested on a Nokia N95 (S60 3rd Edition FP1). Other S60 3rd Edition phones with OpenGL ES graphics
hardware may work.

1. **Allow self-signed applications.** In *Application manager → Options → Settings → Software
   installation*, choose *All*.
2. **Open C / Open C++.** The phone needs the Open C / Open C++ libraries. Many firmwares already
   have them; otherwise install Nokia's Open C/C++ plug-in.
3. **Install** [`symbian/release/CaptainClaw-S60.sisx`](symbian/release/CaptainClaw-S60.sisx) to the
   **memory card (E:)**. It puts `ASSETS.ZIP`, `config.xml` and `SAVES.XML` in `E:\data\claw\`.
4. **Copy the original game data** from your Captain Claw CD to the same folder on the memory card,
   `E:\data\claw\`, for example with the phone in USB mass storage mode:

   | From the Captain Claw CD | To the phone           | Needed for                 |
   |--------------------------|------------------------|----------------------------|
   | `DATA\CLAW.REZ` (120 MB) | `E:\data\claw\CLAW.REZ`| the game (required)        |
   | `MOVIES\CLAW.FEC` (355 MB)| `E:\data\claw\CLAW.FEC`| the movies (optional)     |

   On the card (as seen from a PC) the folder is `data\claw\`. Both the English and the localized
   (e.g. Spanish) CD releases work.

5. **Start** *Captain Claw* from the phone's menu. The first start takes about 30 seconds (the game
   archive is indexed), and a level takes about 45 seconds to load.

Each run writes a log to `E:\data\claw\openclaw.log`. `E:\data\claw\config.xml` holds the settings,
for example:
- `GameSpeed`: 1 = normal speed, less is slower.
- `SoundVolume` and `MusicVolume`.
- `ActiveAreaMargin`: how far outside the screen the world is still simulated.

### Controls

| Key                 | Action                           |
|---------------------|----------------------------------|
| D-pad / 2 4 6 8     | move, climb, duck; menus         |
| centre / 5          | jump, select in menus            |
| 1 / 0 / green       | attack                           |
| 3 / #               | fire (pistol / magic / dynamite) |
| 7 / *               | change ammo                      |
| softkeys / C        | in-game menu, back               |
| volume keys         | volume                           |

The screen follows the phone: landscape (320x240) or portrait (240x320).

## Known problems

- **Power-up music.** It does not play: the phone opens the song and reports it as playing, but
  nothing is heard. The song itself (`GAME\MUSIC\POWERUP.XMI`, converted to MIDI) is silent on the
  phone's synthesizer even as the first song. The level music works. A suspect is its unusual timing
  division (132 ticks per beat).
- **Level start.** The level is revealed from black with a plain wipe. The original game's
  "unrolling" effect is not reproduced.
- **Physics.** OpenClaw uses Box2D with values tuned by eye. Movement, jumps and enemies do not feel
  exactly like the original game.
- **Speed.** Level 1 runs at about 35-40 fps on the N95. Loading a level takes about 45 seconds.
- **Testing.** Only the first levels and the menus were really tested.

## What was changed

[`symbian/CHANGES.md`](symbian/CHANGES.md) lists everything changed from OpenClaw. In short:
- **Platform:** C++03 for the phone compiler, and a small SDL2 replacement drawing with OpenGL ES 1.1
  through texture atlases.
- **Movies:** the Smacker movies play, with a faster decoder.
- **Performance:** the world far from the camera is no longer simulated, and resources are preloaded.
- **Gameplay** fixes toward the original game: deaths, ladders, transitions, respawning.

## Building

How the `.sisx` is made is described in [`symbian/README.md`](symbian/README.md). In short:
- Requirements: the Symbian SDK in `C:\epoc32` (as used by Carbide.c++) and RVCT 2.2 on Windows.
- `symbian\build.bat` compiles `OpenClaw.exe`.
- `python symbian\sis\make_sis.py` packs and signs `build_sis\OpenClaw.sisx`, with a self-made
  certificate created on the first run.
- `symbian/tools/build_pc.sh` builds the same code for Windows (MinGW) to test on a PC.

## Credits

- **Captain Claw** © 1997 Monolith Productions. The game data is not part of this repository.
- **[OpenClaw](https://github.com/pjasicek/OpenClaw)** by Pavel Jasiček and contributors (GPL-3.0),
  on which all of this is built. Their original README is kept as
  [`README.OpenClaw.md`](README.OpenClaw.md).
- [libsmacker](http://libsmacker.sourceforge.net/) by Greg Kennedy (LGPL-2.1), for the movies.
- [Box2D](https://box2d.org/) by Erin Catto, TinyXML and miniz, through OpenClaw.
- Port: Dante Leoncini, made with Claude (Anthropic).

## License

GPL-3.0, like OpenClaw (see [`LICENSE.txt`](LICENSE.txt)). libsmacker keeps its own license, in
[`ThirdParty/libsmacker/COPYING`](ThirdParty/libsmacker/COPYING).
