# Building and source preparation

GamePorts no longer contains implemented game ports. Each implementation owns
its build scripts, source and runtime-data pins, toolchain contract, tests and
release instructions in the standalone repository linked from `README.md`.

This repository retains an intake-only catalogue for five candidates:
OpenLara, OpenTTD, Wesnoth, The Dark Mod and Hedgewars. Their exact upstream
commits can be checked locally with:

```sh
python3 scripts/game_port_prepare.py check --game all
```

Source preparation is offline by default. Supplying `--allow-network` is an
explicit request to fetch the pinned upstream source into the ignored
`build/game-ports/` workspace; it does not create a port or a release claim.

Do not add a build recipe here for a game that has a standalone repository.
Continue that work in its owning repository instead.
