# OpenLoco

Target: AmigaOS 3.x / 68040 + FPU / AC090, GCC 16.2.0-open3 and OpenAmiga SDL3.
Status (1 November): the port has reached the title/demo, scenario selection,
new game (Boulder Breakers), Save Game, autosave, title-screen Load Game and a
clean exit on a 68040 lab copy of Instance-32 (OS 3.2.3, OpenRTG, AHI). That
runtime pass used a temporary station diagnostic patch and is historical
evidence, not qualification of the current recipe. The committed recipe omits
the diagnostic, requests a 4 MiB stack, narrows the compiler workaround and
enforces HUNK, stack and GCC 16 FPCR checks after every link. Repeat the target
pass against its exact receipt before release promotion.

A clean build of the current recipe completed on 1 November. The 7,634,020-byte
HUNK executable is SHA-256
`351383c6c97b8027a3e5f185a23793416cfa87d32bd2c612250857db9e0626fa`;
the post-link gates passed for 68040 FPCR safety and the 4 MiB stack. See the
[current build evidence](evidence/build-2026-11-01.json). This is build-qualified,
not runtime-qualified.

The exact upstream source is `OpenLoco/OpenLoco`
`6d7f5b1bfb4abd5ae2f1a2ade628a2b0d46f861e` (version 26.09).
`source.json` independently pins yaml-cpp 0.9.0, fmt 11.1.4 and sfl 2.2.0
by full commits. It does not change the catalog or existing port lanes.

## Offline recipe

```sh
python3 scripts/game_port_prepare.py prepare --catalog ports/openloco/source.json --game openloco
python3 scripts/port_dependencies.py stage --stove "$STOVE" --source /path/to/locked-codec-inputs
OPENAL_CMAKE_DIR=/path/to/openal-cmake \
OPENAL_CONFIG=/path/to/openal-cmake/OpenALConfig.cmake \
OPENAL_LIBRARY=/path/to/libopenal.a \
OPENAL_MODULE=/path/to/OpenAL.module \
SDL3_SDK=/path/to/openrtg-sdl3-output \
  sh ports/openloco/build-amigaos3.sh --out "$PWD/build/openloco-clean"
```

`STOVE` is an isolated stove root produced by `scripts/stage_open2_stove.py
--release 16.2.0-open3`. Source preparation is offline by default; absent
sources need explicitly approved `--allow-network`. The build itself never
fetches. It verifies pristine pins, compiler/runtime hashes and the repository's
codec inventory, exports source and applies checked patches without editing
caches, and records logs and a receipt. Existing SDK/codec binaries are imported,
not newly source-rebuilt or runtime-qualified.

The packaged Amiga drawer includes `Install-OpenLoco`. The native Installer
copies the game to a chosen destination and, with an explicit prompt, can install
the shared `OpenAL.module` into `LIBS:OpenGPU`. Manual installation remains
documented in the package README.

The current platform input is OpenRTG
`8b6d09ff1023edc3fb1e62e3db0490ec385820b2`, SDL 3.4.14 and SDL3_68k
`c82dea042718200eea401e7ed62d88b997bc9fd9`. New platform work is a separate
qualification dependency. The game links the platform SDL3 client.

## Changes and actual results

- Add truthful m68k/AmigaOS identification.
- Add an opt-in sequential viewport path, selected for this port. Desktop
  parallel rendering remains the default. No TBB substitution or invented
  OpenMulticore support; actual multicore rendering needs a later adapter.
- Disable runtime asset fetching/copying; the user supplies lawful Locomotion
  data. Upstream defaults remain unchanged.
- Decode graphics, objects, audio and complete S5/save state explicitly at
  little-endian wire boundaries and preserve compatible files on export.
- Use the pinned OpenRTG OpenAL.module over AHI. Reverb remains off because the
  backend does not advertise EFX.
- Compile only IndustryList.cpp, TerraForm.cpp and TownList.cpp at `-O0` for the
  recorded open3 ICE; the other 91 UI translation units stay optimised.
- Define a 4 MiB libnix stack contract for Shell and Workbench launches.

The receipt records compiler/runtime, SDL3, OpenAL, dependency, codec, patch and
recipe hashes plus the executable hash and post-link results. [Engineering
notes](ENGINEERING.md) describe the AC090 probes and remaining gates.

Targeted tests build the real patched Sawyer loader and MemoryStream with
synthetic data. The host probe passed exact length/checksum bytes, payload
round-trip, position restoration, corrupt checksums and truncated input. The
same probe and the Core Numerics/Prng sources compile/link with open3 to HUNK
and a target archive. Complete state and widget probes passed on the 68k lab;
see [S5 boundaries](S5_ENDIAN.md). The duplicate libstdc++ COMDAT warnings still
need a toolchain investigation.

After a game configure attempt has exported the patched source:

```sh
cmake -S ports/openloco/probes -B build/openloco-probes \
  -DOPENLOCO_SOURCE="$PWD/build/openloco-clean/source" \
  -DFMT_SOURCE="$PWD/build/game-ports/dependencies/fmt" \
  -DSFL_SOURCE="$PWD/build/game-ports/dependencies/sfl"
cmake --build build/openloco-probes --target port-probes -j2
ctest --test-dir build/openloco-probes --output-on-failure
```

Add the existing Amiga toolchain and `-DAMIGA_STOVE="$STOVE"` for cross-builds;
run that executable in an Amiga guest, not with host CTest.

## Remaining game work

The [audio assessment](AUDIO_ASSESSMENT.md) selected the reusable OpenRTG
OpenAL-compatible module over native OpenAHI/ACAHI, and the recipe now uses it.
Initialisation and source-capacity queries passed in the muted lab. Audible
music/effects, pitch/pan fidelity, dropouts, latency and sustained 68040 cost
still need testing on an unmuted target.

Use a locally generated versioned native asset cache where beneficial, per
[the preprocessing standard](../../docs/ASSET_PREPROCESSING.md). The full
schema audit and converter are not implemented. Object records, CSS/WAV
metadata, PCM and packed S5/save structures cannot be converted by blindly
swapping a container's words. Preserve original data and compatible saves.

Implemented and exercised in the historical lab pass: native paths/user
directories, time, single-instance handling, OpenGPU presentation, input,
persistence and clean exit. Still outstanding for the current exact receipt:
the target repeat, linker-warning investigation, long-session vehicle/track
play, audible audio and measured memory/CPU costs on the plain 68040 fallback.

OpenLoco's engine is MIT; preserve dependency notices. Original Locomotion
assets are user-supplied. The disabled OpenGraphics v0.1.12 package is pinned
upstream by SHA-256 `9e6596aaa52b2296cfc779bbe3aab01d0fb37ef61fcb23cd0e5ff4d2b9e846ca`;
it is not fetched or bundled here and still needs a separate packaging/rights
review. No assets, SDK headers or binaries are committed. All
[release gates](../../RELEASE_POLICY.md) remain open.

The earlier no-audio checkpoint reached retail data but stopped on little-endian
G1/CSS metadata; [its receipt](evidence/noaudio-native-results.json) remains as
historical failure evidence. Patches 0012-0016 supersede that checkpoint.

## Lab results (31 October)

The historical diagnostic build was made on Daletop through Cradle (`stove_build`, target `ports/openloco`,
stove `os32-gcc16`); `build-amiga.sh` keeps an incremental work folder,
builds OpenRTG's OpenAL.module and link stub at the commit in `source.json`
(`openrtg`), and installs into lab `t3-openloco` only (never an instance).

| Output | Bytes |
| --- | --- |
| OpenLoco (HUNK) | 9,832,716 |
| OpenAL.module | 58,528 |

- It included an uncommitted station-tool logging patch and cannot qualify the
  current source. Its behavioural observations must be repeated.
- New game, date advances, Save Game (763,801 byte .SV5 in under 0.5 s),
  autosave, Load Game of that save, quit from the window's close box: all OK.
- Patch 0017 lets the engine build against an OpenAL without EFX (OpenRTG's
  has none): reverb stays off because `ALC_EXT_EFX` is absent.
- Patch 0018 fixes the Save Game hang. The engine shows a busy pointer during
  a save and then puts the system pointer back. SDL3.modules built before
  OpenRTG 164dd9b (24 October; Instance-32's is one) link a system cursor
  into SDL's list twice, so `SDL_SetCursor` looped forever. On the Amiga the
  port now uses SDL's default cursor for the system pointers.
- Not yet checked: hearing the sound and music on an unmuted instance,
  building track and running trains for a long session.
