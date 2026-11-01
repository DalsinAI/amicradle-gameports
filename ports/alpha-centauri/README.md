# OpenAlphaC integration pointer

The Alpha Centauri engine and Amiga port now live in the canonical repository:

`https://github.com/DalsinAI/openalphac`

This gameports repository does not carry a second implementation copy. The
reviewed integration pin is recorded in `source.json`. Build, data-import,
qualification and release work belongs in OpenAlphaC. A future aggregate build
must consume that immutable revision and preserve its single-package 68040
software fallback and private original-data boundary.

Status at extraction: engineering pre-release, not a final release.
