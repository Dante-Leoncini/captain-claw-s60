# Captain Claw for S60 - building and internals

How the S60 port is built and laid out. Installing and playing are explained in the
[main README](../README.md); the changes from OpenClaw are listed in [CHANGES.md](CHANGES.md).

## Building

Requires the Symbian^3 SDK in `C:\epoc32` (as used by Carbide.c++) and RVCT 2.2.

    symbian\build.bat          (cmd or PowerShell, not git-bash)
    python symbian\sis\make_sis.py

`build.bat` compiles `OpenClaw.exe` (ARMV5 urel). `make_sis.py` writes the package description
(`build_sis\OpenClaw.pkg`), runs `makesis` and `signsis`, and leaves `build_sis\OpenClaw.sisx`. The
package holds the executable, its resources and icon, and OpenClaw's own data:
- `ASSETS.ZIP`, with the "Replay movies" menu enabled.
- `config.xml`, from `data/claw/`.
- `SAVES.XML`.

Nothing from the original game goes in. The package is signed with a self-made SHA-1 certificate,
created on the first run with openssl. It must be SHA-1: S60 3rd Edition rejects SHA-256 as a
corrupted file. The phone needs Open C / Open C++ and must allow self-signed applications. The
ready-made package is `release/CaptainClaw-S60.sisx`.

`python symbian\sis\make_sis.py --quick` packs only the executable, as an upgrade of an installed
copy.

## Testing on the phone over USB

`tools/n95_test.py` installs with App TRK (`runonphone`) and follows the log through PC Suite (ConnAPI):
- `--autotest` starts straight in level 1.
- `--autoplay` makes Claw play by himself, `--fire` makes him shoot.

The frame profiler (`OPENCLAW_FRAME_PROFILE`, on in `group/common.mmh`) logs:
- the time per part of the frame every 2 seconds;
- `[slow]`: every frame over 45 ms;
- `[event]`: every event listener over 20 ms.

## Layout

- `compat/` - the C++11 library pieces the game uses, for the C++03 compiler (shared_ptr, unique_ptr,
  nullptr, to_string, ...). Reaches every game file through `symcpp.h` (see the comment there).
- `sdl2lite/` - the subset of SDL2 / SDL_mixer / SDL_image / SDL_ttf / SDL2_gfx the game uses. The
  renderer packs every texture in shared atlas pages and draws the frame in a few batches;
  sounds are mixed on an audio thread; music is MIDI played by the phone synth.
  `sdl2lite/src/pc/` runs the same code on Windows for testing (`tools/build_pc.sh`).
- `src/`, `inc/`, `data/`, `group/` - the S60 application and the build files.
- `tools/check_cxx03.sh` - quick C++03 syntax check of the game sources with MinGW.
