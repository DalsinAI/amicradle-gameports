# Enigma with OXP

Source pin: `6f0c51b26cc013cb6c935046d7d1e6a20bea4179`
([upstream](https://github.com/Enigma-Game/Enigma)), GPL-2.0-or-later.
The port patch replaces Xerces headers, DOM/parser/serializer calls and CMake
linkage with the existing OpenAmigaXML OXP parser and its optional schema module.
No Xerces headers or library are required. The adapter is Enigma-specific;
OXP retains its own public C API.

## Offline build

Prepare the pinned upstream checkout with the existing source intake command:

```sh
python3 scripts/game_port_prepare.py prepare --game enigma --allow-network
```

Stage a private Open4 stove from the checksummed release archive and explicit
OpenNDK/SDL SDK donors (never change a shared stove):

```sh
python3 ports/enigma/stage_toolchain.py \
  --archive /path/to/open4.tar.xz --ndk-prefix /path/to/openndk-donor \
  --sdk-prefix /path/to/target-sdk --destination build/enigma-open4-stove
```

Build with a committed OXP extension revision and an explicit target overlay:

```sh
nice -n 19 sh ports/enigma/build-amiga.sh \
  --source build/game-ports/sources/enigma \
  --stove build/enigma-open4-stove --sdk-overlay /path/to/target-overlay \
  --oxp-source /path/to/openamigaxml \
  --oxp-ref 64f280125aae462b7ff93dd068cae5b8fe99a7e0 \
  --out build/enigma-open4-soft --jobs 4
```

The historically named `build_open3.py` now requires verified Open4 provenance.
It exports both source commits into a fresh output directory, applies patches
without fuzz, builds native host tolua, then cross-builds with OpenNDK,
68040 soft-float specs and the selected SDK overlay. It records patch and input
hashes, commands, a link map and executable hash. The final link rejects Xerces
symbols. `configure_open3.py` is the configuration-only compatibility entry.
No network access or live-instance deployment occurs during the build.

## XML validation and runtime

The adapter supports Enigma's local schemas, DOM edits/imports, defaulted versus
explicit attributes, filtered score serialization and UTF-8/UTF-16 conversion.
Unmapped external resources are rejected. XML files saved by Enigma omit schema
default attributes, preserving the score checksum/reload semantics.
See OpenAmigaXML `oxp/SCHEMA.md` for the deliberately bounded XSD subset.

Checks recorded on 31 October 2026: 72 parser fixtures plus API checks,
148 mutation/schema checks, 29 host adapter checks under ASan/UBSan, and all
1,634 supplied Enigma XML files validate against the seven schemas. Native
AROS tests pass with Open4 soft-float and the FPU disabled: 148 schema checks
and 28 adapter checks, both exit 0. Sanitizers and libFuzzer are host test tools;
the Amiga executable contains none of their runtime symbols.

The full engine builds without Xerces. Fixing the upstream CMake omission of
`stones/InkwellStone.cc` registers 791 objects and permits the tutorial previews
and animated Enigma intro to render. In the isolated 320x240 software lab it
loads 5,626 models and 2,697 level entries. Cold startup is slow; overload messages
and presentation/input issues remain. Player-controlled gameplay is unqualified.

The first GUI exit exposed an obsolete assumption that serialized XML declared
UTF-16. OXP already emits UTF-8; removing that replacement fixes score saving.
`--check-xml --robinson --lang en --log --redirect` exercises the real score,
state and preferences paths before graphics initialization. Two native FPU-disabled
processes pass: fresh profile creation, then saved-score reload/save, both exit 0.
The resulting XOR-encoded ZIP score files pass CRC checks and contain well-formed
XML; state and preferences are also well-formed. Normal GUI exit after this fix
has not been repeated. This diagnostic does not test saving a completed level.
Test sources, native logs and build receipts are in `tests/` and `evidence/`.

`--robinson` skips network initialization for offline play. Native paths use
`PROGDIR:data` and `PROGDIR:User`. `--nosound --nomusic` selects muted startup.

Remaining qualification: rendered level input, performance/presentation, completed
level save/reload, audio, normal GUI exit, instruction audit and intended ROM/hardware.
The selected SDK includes static curl, zlib and SDL_ttf/FreeType dependencies;
the project module/OpenFont/z.library policy must be satisfied before release.
Linker COMDAT section-size warnings also require review. The larger isolated lab
avoids the original 72 MiB memory exhaustion; it does not qualify that smaller setup.
This integration is a draft, not a release or a claim of playable Enigma.
