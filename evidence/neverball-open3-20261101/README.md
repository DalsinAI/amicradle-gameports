# Neverball / Neverputt Open3 qualification - 1 November 2026

This is the consolidated record for the active Neverball port. The canonical
implementation is `ports/neverball`; no other checkout is an independent source
of truth.

## Identity

- Repository baseline: `b031f466591a58e02ea829ae9672d5efa1769041`.
- Neverball upstream: 1.6.0 at
  `a1ed09911dca262d80049c12a2824d683af494d6`.
- Target: AmigaOS 3.x, 68040 + FPU, GCC 16.2.0-open3.
- Graphics: OpenGPU SDL2 and GL, virgl renderer reporting Radeon RX 460.
- Test machine: isolated copy of Instance-32 using OpenKick/OpenUp, AC090 JIT,
  Zorro III 512 MB. This was not a stock or clean installation.
- Original qualification package: 27 files, 79,083,754 bytes. Its raw verifier
  report is `package-check.json`.

## What is proved

- Clean Open3 compile and link of both `Neverball` and `Neverputt`.
- Package completeness/provenance verifier passed for the original package.
- Neverball title and accelerated 3D scene rendered; the historical virgl black
  frame did not reproduce.
- Native keyboard entry produced player name `dale`.
- Level 01 was completed with 12 coins and Level 02 was entered.
- Pause/resume and normal Neverball exit returned `NEVERBALL_RETURNED 0`.
- The player config was written beneath the program's `User` directory.
- Neverputt title and Hole 01 rendered. Mouse strokes and menu/pause input worked.
- Neverputt created a 1920x1080 fullscreen GL window, opened joystick device 0,
  later removed and closed it, and returned normally to Workbench.

The screenshots and logs are evidence for those bounded statements only. The
Neverputt run ended from the pause menu at score 10; it does **not** prove a
completed hole or controller-driven gameplay.

## Release gate state

| Gate | State | Evidence / remaining work |
|---|---|---|
| 1. Reproducible clean build | PASS | Open3 build completed; `reproduce.log`. |
| 2. Clean-install runtime modules | PARTIAL | Runtime stack worked on the isolated Instance-32 copy; clean install remains. |
| 3. Three Neverball levels | PARTIAL | Level 01 complete and Level 02 entered; a third level remains. |
| 4. Complete one Neverputt hole | PARTIAL | Hole 01 rendered and accepted strokes; no cup completion. |
| 5. Keyboard | PASS | Native name entry and gameplay/menu input. |
| 6. Mouse focus/capture/release | PASS in this lab | Neverputt strokes, pause/menu input and clean return to Workbench. |
| 7. OpenInput controller | PARTIAL | Enumeration/open/close logged; controller-driven menu/gameplay remains. |
| 8. Music/effects through AHI | OPEN | No listening or waveform proof. |
| 9. Window/fullscreen transition | PARTIAL | Both modes and size events are logged; a deliberate round trip remains. |
| 10. Persistence writes | PASS | Player/config write observed; fresh-process restart check remains desirable. |
| 11. Clean exit | PASS in this lab | Both programs returned normally; Neverputt joystick teardown is logged and its return to Workbench was observed. |
| 12. Notices and hash inventory | PASS after fix | Embedded inventory is now generated and verified; see fixed-package receipt. |
| 13. Clean-instance repeat | OPEN | Current lab is a copied development instance. |

`qualification.json` is the machine-readable version of this table. `progress.json`
is the unedited earlier checkpoint captured before the Neverputt continuation.

## Packaging defect fixed during consolidation

The old recipe wrote `SHA256SUMS` beside `Neverball/`, so the deliverable did
not contain the inventory required by gate 12, and the verifier did not check
it. The recipe now:

1. refuses a source checkout not at the exact upstream pin;
2. writes a self-excluding `Neverball/SHA256SUMS` inside the package;
3. verifies exact file coverage, safe paths, duplicate/absent entries and every
   digest; and
4. has regression tests for valid, modified, missing and extra files.

## Evidence map

- `neverball-runtime-first.log`: guest launch paths, GL identity, joystick and
  clean Neverball return.
- `logs/neverball-neverputt.log`: accumulated application log including
  window/fullscreen changes and joystick lifecycle.
- `screenshots/`: title, player name, sets, Levels 01/02, motion, pause,
  and the Neverputt title.
- `package-check.json`: original package verifier report.
- `fixed-package-check.json`: verifier report after embedding the inventory.
- `reproduce.log`: original clean-build reproduction log.
- `SHA256SUMS`: hashes for this evidence directory, excluding itself.

No release promotion is claimed until every mandatory gate is green.
