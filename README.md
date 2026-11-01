# AmigaChrome Game Ports


**[Unified Port Atlas](https://github.com/DalsinAI/amigachrome/tree/main/docs/porting)** — searchable cross-programme game/application catalogue with existing Amiga-port evidence, source/rights assessment and proposed Open-family acceleration or development benefit. This does not replace the local source pins, build notes or qualification gates.

Open-source game-port and compatibility work for **m68k AROS**, **AmigaOS 3.x** and **AmigaChrome**.

The repository contains compiled revival ports and the next 3D source-intake targets. Compile success is useful evidence, but it is **not** synonymous with release readiness. See [RELEASE_POLICY.md](RELEASE_POLICY.md).

## Live programme - 24 October 2026

The [live execution order and SDL3 intake](PORT_BACKLOG.md#live-execution-order---24-october-2026)
accept the reviewed games, applications and lab targets into the programme.
Finish the existing playable/SDL3 ports first. With OpenAL nearing readiness,
prepare its AC090 qualification and AstroMenace as the first consumer; availability
and audio success remain explicit gates. See the [full SDL3 review](docs/SDL3_PORT_REVIEW_20261024.md).
New intake entries still need audited source pins before becoming fetch/build targets.

## Post-merge checks - 13 October 2026

The port/SDL changes are merged and rebuilt. A new Amiga-only OpenJazz
configuration-directory check fixes the demonstrated false "not writable"
result. Chocolate Doom reached software-rendered gameplay, saved/loaded a
slot and exited 0 with sound explicitly disabled. Final OpenJazz startup/focus
repetition and Doom audio remain open, not release passes.

[Exact results, hashes, limits and next checks](evidence/postmerge-20261013/README.md).

## Latest verified state - dependency isolation and recovered text input

- **Fresh build work:** the affected recipes now require a locked codec inventory
  inside the selected stove. Both AssaultCube targets were rebuilt from a new
  exact-pinned checkout without the old WebKit include/library fallback.
- **Recovered earlier runtime evidence:** the PR67 SDL2 candidate delivered real
  shared/static ASCII text. Neverball accepted `DALEX`, Backspace made `DALE`,
  and the player name persisted across exit/relaunch. This is the candidate in
  the isolated lab, not an upstream merge or a production deployment.
- **SDLPoP:** a new `SDL_OPENGPU=0` lab run rendered Level 1, quicksaved,
  moved, quickloaded back to the saved position and returned exit 0. This is
  one bounded emulated-68040/software-mode check, not a complete release.

[Build/review receipts](evidence/open2-20261013-resume/README.md) and
[recovered runtime evidence](evidence/sdltext-20261012-recovered/README.md)
retain the source identities, hashes, raw output and remaining limitations.
No port has been promoted to RELEASE.

### Rendering qualification lanes

| Lane | Ports | Required additional gate |
|---|---|---|
| Plain 68040 + FPU / software 2D | OpenJazz, SDLPoP, Chocolate Doom | Run on the intended SDL CPU path with `SDL_OPENGPU=0`, plus input/audio/save/exit tests. This is a requirement, not a completed pass for all three. |
| OpenGPU-dependent 3D client | AssaultCube client | Qualify the chosen supported OpenGPU backend. No plain-68040 performance claim. |
| No rendering | AssaultCube dedicated server | Socket/listen/stop tests, with the guest library version recorded. |

## Historical Open2 follow-up - actual guest first light

NXEngine-evo completed its Open2 link, and Neverball/Neverputt now rebuild and
pass the package gate. AssaultCube has two targeted fixes: one Amiga definition
for stream virtuals, and correct array deletion in game-mode parsing. The
previous duplicate stream-section and mismatched-delete diagnostics are absent
from the resulting links; other warnings and the audio stub remain.

**Runtime evidence:** SDLPoP rendered its intro and a diagnostic Level 1; Neverball
rendered its title and Level 01 3D start scene. Both returned guest exit code 0
in a separate, muted, stopped-source lab. Sixteen host regression tests and a
21-check stream diagnostic in the AmigaOS guest passed. No full game release.
Native name typing in Neverball remained empty on the recorded existing SDL2
runtime; the on-screen keyboard was a workaround, not a text-input pass.

[Follow-up results, screenshots, hashes and remaining gates](evidence/open2-20261012-followup/README.md).

## Open2 compiler pass - 12 October 2026

The port queue has resumed with **OpenAmigaGCC 16.2.0-open2**. OpenJazz,
Chocolate Doom, SDLPoP and both AssaultCube executables have new compile/link
artifacts. NXEngine-evo's longer rebuild was still running at the evidence
snapshot. No new game runtime or release pass is claimed.

See [retained results and risks](evidence/open2-20261012/README.md) and
[Open2 build instructions](docs/OPEN2_BUILDING.md). The shared CMake toolchain
now honours `STOVE`, preventing an accidental fallback to an older compiler.
The OpenGPU SDK archives remain explicitly recorded reused dependencies.

## Current intake state

| Port | Build state | Runtime state | Target / notes |
|---|---|---|---|
| OpenLara | SOURCE PINNED | NOT STARTED | 3D engine / OpenGPU target |
| OpenTTD | SOURCE PINNED | NOT STARTED | large C++/SDL application target |
| The Battle for Wesnoth | SOURCE PINNED | NOT STARTED | large C++/SDL strategy target |
| The Dark Mod | SOURCE PINNED | NOT STARTED | long-range idTech 4-derived stretch target |
| Hedgewars | SOURCE PINNED | NOT STARTED | SDL/audio/networking/physics target |

Implemented ports live in their standalone repositories below. GamePorts now
retains only the five intake pins above plus historical programme evidence.

## Repository layout

- `scripts/` — source preparation, build orchestration and field-test staging code.
- `port-layer/acgame/` — AmigaChrome AGA presentation code, smoke test and unit tests.
- `toolchain/aros-local/` — the AROS m68k toolchain bootstrap recovered from the AmigaChrome worktree.
- `gameports/catalog.json` — exact upstream pins and content policy for intake-only candidates.

See [BUILDING.md](BUILDING.md) for the rebuild path.

## Source preparation and network policy

Upstream engine source remains in the upstream repositories and is referenced by exact pinned revisions or archive hashes.

Source preparation is **offline by default**. Existing valid prepared sources and cached archives are reused locally. If preparation would require an external fetch, it stops unless the caller explicitly supplies `--allow-network` after approval. An existing build or preparation script is not permission to access the network.

Game data, ROMs, proprietary assets and local test caches are **not** part of this repository unless a port's upstream licence explicitly permits redistribution and the release recipe records that provenance.

## Status language

- **SOURCE PINNED** means the exact upstream input has been selected but no successful target build is claimed.
- **COMPILED** means the pinned source has produced the target m68k executable.
- **FIRST LIGHT** means the executable has launched on an AmigaChrome guest.
- **TESTING** means runtime qualification is underway and at least one required release gate remains open.
- **TESTED** means the stated graphics, input, audio, persistence and clean-exit gates have been exercised.
- **RELEASE** means the port has passed its complete release gates, package/provenance checks and clean-instance runtime qualification.

**DONE means RELEASE.** Compiled, First Light and Testing are intermediate states.

## Licence

AmigaChrome-authored glue, scripts and documentation are MIT licensed unless a file says otherwise. Upstream projects retain their own licences. Game data retains its own rights and is not redistributed unless its licence and the port's release policy permit it.

No upstream game engine source is committed here. Prepared sources live outside Git at their pinned revision. Binaries built from them remain subject to the upstream licence, including source-offer and notice obligations where applicable.

| Port | Upstream | Licence / data boundary |
|---|---|---|
| OpenLara | https://github.com/XProger/OpenLara | engine source only for intake; original Tomb Raider data external |
| OpenTTD | https://github.com/OpenTTD/OpenTTD | preserve upstream and third-party notices; runtime asset packs reviewed separately |
| The Battle for Wesnoth | https://github.com/wesnoth/wesnoth | preserve upstream code/data/translation licensing |
| The Dark Mod | https://github.com/stgatilov/darkmod_src | official engine mirror; missions/full installation external |
| Hedgewars | https://github.com/hedgewars/hw | preserve upstream code and mixed asset licensing |

Each project's own licence files at the pinned revision are authoritative.

Copyright (c) 2026 AmigaChrome Team.

## Contributors

AmigaChrome Game Ports is created and maintained by [SacredTrees](https://github.com/SacredTrees) with The AmigaChrome Team, copyright AmigaChrome Team. Everyone whose work it includes is credited in [`CONTRIBUTORS.md`](CONTRIBUTORS.md).

## Standalone game repositories

These ports have their own canonical repositories and no longer have entries,
source pins, recipes or integration pointers in GamePorts:

- [OpenAlphaC](https://github.com/DalsinAI/openalphac)
- [AmigaCivII](https://github.com/DalsinAI/amigacivii)
- [Amiga fheroes2](https://github.com/DalsinAI/amigafheroes2)
- [AmigaNeverBall](https://github.com/DalsinAI/amiganeverball)
- [Amiga OpenLoco](https://github.com/DalsinAI/amigaopenloco)
- [OpenSeriousSam](https://github.com/DalsinAI/openserioussam)
- [Amiga AssaultCube](https://github.com/DalsinAI/amigaassaultcube)
- [Amiga AstroMenace](https://github.com/DalsinAI/amigaastromenace)
- [Amiga C-Dogs](https://github.com/DalsinAI/amigacdogs)
- [Amiga Chocolate Doom](https://github.com/DalsinAI/amigachocolatedoom)
- [Amiga Doom 3](https://github.com/DalsinAI/amigadoom3)
- [Amiga Enigma](https://github.com/DalsinAI/amigaenigma)
- [Amiga LBA2](https://github.com/DalsinAI/amigalba2)
- [Amiga NXEngine-evo](https://github.com/DalsinAI/amiganxengine)
- [Amiga OpenJazz](https://github.com/DalsinAI/amigaopenjazz)
- [Amiga OpenOMF](https://github.com/DalsinAI/amigaopenomf)
- [Amiga OpenTyrian](https://github.com/DalsinAI/amigaopentyrian)
- [Amiga SDL2 demos](https://github.com/DalsinAI/amigasdl2demos)
- [Amiga SDLPoP](https://github.com/DalsinAI/amigasdlpop)
- [Amiga SWIV](https://github.com/DalsinAI/amigaswiv)
- [Amiga UQM](https://github.com/DalsinAI/amigauqm)
- [Amiga Warzone 2100](https://github.com/DalsinAI/amigawarzone2100)
