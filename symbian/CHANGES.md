# Changes from upstream OpenClaw

What this port changes compared to [pjasicek/OpenClaw](https://github.com/pjasicek/OpenClaw), grouped by
area. Most of it was needed to run on a Nokia N95 (ARM11 at 332 MHz, PowerVR MBX Lite, ~20 MB of free
RAM for the game). Some of it fixes OpenClaw's behaviour so that the game plays closer to the original
Captain Claw (1997).

## Platform

- **C++03.** The phone compiler (RVCT 2.2) only knows C++03. `compat/cxx03.h` provides the C++11
  library pieces the game uses: `shared_ptr`, `weak_ptr`, `unique_ptr`, `nullptr`, `override`,
  `to_string` and so on. The game sources were adjusted where the language itself differs: `>>` in
  templates, range-for loops, lambdas, `auto`.
- **sdl2lite.** A small reimplementation of the parts of SDL2, SDL_mixer, SDL_image, SDL_ttf and
  SDL2_gfx the game calls. It runs on Symbian, and on Windows for testing (`sdl2lite/src/pc/`).
- **No original data in the package.** The installer carries OpenClaw only. `CLAW.REZ` (and
  `CLAW.FEC` for the movies) are copied from the CD to `E:\data\claw\`.
- **REZ reading.** The archive index is read in 16 KB blocks instead of byte by byte, which brought the
  start-up time down from minutes to seconds.
- **Screen.** Landscape 320x240 and portrait 240x320, following the phone's rotation. The game world
  and the menus are drawn at half scale (the same view as 640x480). The in-game HUD is drawn at 1:1
  so the counters can be read.
- **Phone integration.**
  - The volume keys change the volume.
  - Sound goes silent when the game is in the background.
  - Quitting from the menu or the softkey closes the game at once, with no music left playing.
  - The game also quits when `E:\data\claw\quit.txt` appears; remote tests use this.

## Rendering (OpenGL ES 1.1)

- **Atlas renderer.**
  - Every image goes into a shared 1024x1024 atlas page, and a frame is drawn in a few batched draw
    calls (~8 per frame in level 1, against one per sprite before).
  - Sprites with on/off transparency use RGBA5551 pages; translucent ones use RGBA4444, half the memory
    of 8888.
  - Single-colour surfaces such as fades are drawn with one white texel, tinted.
- **Texture memory.**
  - Atlas pages are given back to the driver as soon as they are empty.
  - Entering a level frees the menu's images, the previous level's images, and OpenClaw's own pictures
    in `ASSETS.ZIP` (credits, touch controls).
  - Without this the N95 ran out of texture memory and drew garbage, or crashed, when a level was
    started from the menu.
- **Streaming textures** (movies). They alternate between two textures and replace the whole texture.
  On the MBX this is about 8x faster than `glTexSubImage2D` on a texture still in use.

## Movies

- **Playback.** OpenClaw does not play them. This port plays the Smacker movies stored in `CLAW.FEC`
  with libsmacker (LGPL, in `ThirdParty/libsmacker`).
  - `CLAW.FEC`'s directory is encrypted, so each movie is found by walking the movie headers.
  - The intro movies play at start-up, the story scenes between levels, and the "Replay movies" menu
    plays all of them.
- **libsmacker speed-up.** Huffman decoding uses lookup tables (10 bits for the 16-bit trees, 8 for the
  8-bit trees) instead of walking the trees bit by bit. It is 2.4x faster with bit-identical output.
  On the N95 the 320x240 movies now play at full speed with plenty of headroom.

## Sound and music

- **Software mixer.** Chunks are kept in their native 8 or 16 bit mono format and resampled while
  mixing. Converting them all to 16-bit stereo would have used too much memory.
- **Music.** MIDI played by the phone's synthesizer: OpenClaw converts the XMI files to MIDI. One MIDI
  player is reused for all songs.
- **Movie sound.** Goes through the mixer's music hook.

## Performance

- **Active area.**
  - Actors and physics bodies more than a margin away from the camera are not updated.
  - Bodies outside the area are put to sleep in Box2D and woken when they come back.
  - Level 1 went from 13 to ~40 fps on the N95.
- **Time step.** One update of at most 50 ms per frame. A slow frame slows the game down instead of
  making the physics tunnel, or making the next frames slower with catch-up steps.
- **Hitches removed:**
  - Score, health, lives and ammo changes reloaded the HUD digits from the card: ~260 ms per coin.
  - Score popups, loot, glitter, explosions, hits and HUD images are now loaded with the level and kept.
  - Raw image data is released from the resource cache once the image is on the GPU. The freed space
    keeps Claw's, the game's and the level's sounds and the music cached, so nothing is read from the
    card during play.
  - Images that are still alive are reused instead of being loaded again.
- **Loading.**
  - Tiles are stored as compact text.
  - Each actor's XML is released once the actor exists.
  - Resource pattern matches are memoized.

## Gameplay (closer to the original game)

- **Death by enemies.** Claw screams, jumps and falls through the floor off the screen while the
  camera stays still, as in the original. OpenClaw always used the spikes death. Spikes and death
  tiles keep their own deaths.
- **Ladder top.** Climbing up, Claw plays the top-of-ladder animation and ends standing on the floor
  at the top. In OpenClaw he climbed on into the air and dropped back. Holding up at the top looks
  up, as in the original.
- **Level start.** The objects and Claw start on the ground. They used to start a few pixels above it
  and drop when the level appeared.
- **Respawn.** After a death Claw comes back standing on the checkpoint's floor. He used to appear
  9 px above it, in mid-jump.
- **Enemies killed** are knocked up and away from the blow, then fall off the screen in a curve. In
  OpenClaw they always slid down-right in a straight line.
- **Death transition.** The original iris: a circle closes on the spot where Claw died (without him)
  and opens on the checkpoint with Claw standing there. OpenClaw closed a square in from the edges.
- **Level start.**
  - The menu fades to black.
  - The loading screen fades in from black and back out to black.
  - The level unrolls out of black, from left to right, as a still picture before play starts.
- **Physics body boxes** (`VGetAABB`). They are now computed where the body is, not over the area it
  swept during the last step, which was a few pixels off for moving bodies.
- **"Game saved" message** at save checkpoints, drawn with the game's own font.
- **Game speed setting.** `GameSpeed` in `config.xml` (1 = normal) scales the whole game's time.

## Menus

- **Menu text** is centred by its real width; it was misplaced in the localized releases.
- **Missing images** of the localized REZ releases are skipped, or matched by number.
- **Translucent text** (the menu's purple text) keeps its transparency.
- **"Replay movies"** is enabled, with a page listing the ten movies.

## Tools

- **`tools/build_pc.sh`** builds the same code for Windows with MinGW.
- **`tools/n95_test.py`** installs on the phone over USB and follows the log.
  - `--autotest` starts straight in level 1, with no menu and no intro movies.
  - `--autoplay` makes Claw run, jump and attack by himself, so hitches can be measured with nobody at
    the phone.
- **Frame profiler** (`OPENCLAW_FRAME_PROFILE`). Logs time per part of the frame every 2 seconds.
  Every frame over 45 ms is logged with its breakdown, and every event listener over 20 ms by name.

## Still different from the original

- **Power-up music:** silent on the phone.
- **Level reveal:** the level is revealed with a plain wipe, not the original's rolling effect.- **Physics in general.** OpenClaw uses Box2D with values tuned by eye, not the original game's
  movement.
