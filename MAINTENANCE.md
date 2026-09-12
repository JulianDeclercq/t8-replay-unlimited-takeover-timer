# Maintenance

How the patch works, what to do when a game update breaks it, and how to
build. Nothing here is needed to use the plugin - see the README for that.

## How it works

The limit is a hard-coded `600` (frames, at 60 fps) in two places:

```
duration_frames = min(600, battle_subsystem->remaining_round_frames)
```

The result is stored at `replay_controller+0x64` and decremented once per frame
by the takeover tick; the HUD shows `ceil(frames / 60)`. Raising both constants
to `0xFFFFFF` makes `min()` always pick the remaining round time. Nothing else
needs patching.

Site 1 is the menu start path, site 2 a helper on the lead-in path:

```
41 B8 58 02 00 00 44 39 40 1C 44 0F 4C 40 1C 44 89 43 64   imm at +2
8B 48 1C B8 58 02 00 00 3B C8 0F 4C C1                     imm at +4
```

Both are found by AOB scan over the executable's `.text` at load, so a game
patch that shifts the image costs nothing. Each site is only written if its
immediate currently reads `600` (or `0xFFFFFF`, on a reload); anything else is
logged and skipped. A failed scan logs `NOT FOUND` and the game loads normally.

## Patch day

If a game update breaks it, the log says which site stopped matching and which
build (`TimeDateStamp`) it failed on. Re-derive that signature and update
`kSites` in `takeover.cpp` — nothing else in the plugin knows about addresses.

## Build

```sh
./build.sh
```

Requires CMake and MSVC (x64). Produces both files in `build/Release/`.

**Every build overwrites the installed `.asi`** — configure with
`-DT8T_DEPLOY_DIR=""` for a compile check that does not deploy. The build never
copies `dinput8.dll` anywhere; install that one by hand, or it would clobber
whatever owns the slot. A deploy that fails with *Permission denied* just means
the game is running with the plugin loaded — close it and rebuild.

Run the self-check after any change to the patterns:

```sh
cmake --build build --config Release --target selftest && ./build/Release/selftest.exe
```

## Credits

Found by reverse-engineering Tekken 8 v3.02.02 on 2026-09-12, and verified in
game the same day. Started life as a Cheat Engine script that had to be re-run
after every game start; the `dinput8.dll` proxy is lifted from the same
author's Tekken Outfits mod.
