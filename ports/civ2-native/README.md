# AmigaCivII integration pointer

The Civilization II native port now lives in its own repository:

`https://github.com/DalsinAI/amigacivii`

This gameports repository does not carry a second implementation copy. The
reviewed integration pin is recorded in `source.json`. Build, probe, evidence
and release work belongs in AmigaCivII; a future gameports package recipe must
consume an immutable AmigaCivII revision and preserve its mandatory single-
package 68040 software fallback.

Status at extraction: experimental, not a release. Menu/gameplay, save/load,
audio, clean exit and measured plain-68040 qualification remain open.
