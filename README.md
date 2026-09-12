# T8 Replay Unlimited Takeover Timer

Removes the 10-second limit on **Takeover** in Tekken 8's *My Replay & Tips*.
A takeover now runs until the round clock would have expired (or until someone
gets knocked out, same as before).

Single-file plugin, no dependencies, no UI, no configuration.

## Install

1. Install the [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)
   into `TEKKEN 8/Polaris/Binaries/Win64/` (it is the `dinput8.dll` proxy — the
   `winmm.dll` slot does not work, Tekken 8 rejects a foreign `winmm` and
   relaunch-loops).
2. Drop `T8ReplayUnlimitedTakeoverTimer.asi` into
   `TEKKEN 8/Polaris/Binaries/Win64/plugins/`.
3. Start the game. `t8_replay_unlimited_takeover_timer.log` appears next to the
   `.asi` and should read:

   ```
   build: TimeDateStamp=0x........ SizeOfImage=0x.......
   takeover start : RVA 0x........  600 -> 16777215
   takeover helper: RVA 0x........  600 -> 16777215
   ```

The log is rewritten on every launch. Uninstall = delete the `.asi`.

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

Requires CMake and MSVC (x64). **Every build overwrites the installed `.asi`** —
configure with `-DT8T_DEPLOY_DIR=""` for a compile check that does not deploy.

## Credits

Found by reverse-engineering Tekken 8 v3.02.02 on 2026-09-12. Originally a Cheat
Engine script (`research/ce_replay_takeover_timer.lua` in
[tekken-fashion-hub](https://github.com/)) that had to be re-run after every
game start.
