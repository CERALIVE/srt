<!-- Moved verbatim from AGENTS.md on 2026-10-05 by lean-rules-docs-landing-latam -->

## COMMON TASKS

Task-routing for the handful of legitimate CeraLive operations on this vendored
repo. Anything not listed is upstream Haivision work and out of scope (see SCOPE
BOUNDARY).

| I need to… | Do this |
|------------|---------|
| Build the device runtime package (library + bundled SRT tools) | `packaging/build-deb.sh` (outputs `dist/libsrt1.5-ceralive_*.deb`) |
| Use / find `srt-live-transmit` on device | It ships in `libsrt1.5-ceralive` at `/usr/bin/srt-live-transmit` (see [Bundled SRT tools](role-in-the-group.md#bundled-srt-tools-srt-live-transmit-srt-file-transmit-srt-tunnel)) |
| Verify replacement behavior + tool linkage | `packaging/verify-runtime-replacement.sh <deb>` |
| Build the library to test it standalone | [BUILD](build.md#build) — `cmake -B build …` |
| Run the unit + bonding test suite | [TEST (ctest)](test-ctest.md#test-ctest) |
| Find the source / build config / options | [WHERE TO LOOK](where-to-look.md#where-to-look) |
| Touch the reorder-freeze option | See [SANCTIONED CERALIVE PATCH](sanctioned-ceralive-patch-srto-reorderfreeze.md#sanctioned-ceralive-patch--srto_reorderfreeze) — keep it decay-disable-only and decoupled from NAK |
| Update routing/build/test guidance | Edit this `AGENTS.md` |
| Confirm the runtime contract | [ROLE IN THE GROUP](role-in-the-group.md#role-in-the-group); the device uses `libsrt1.5-ceralive` |

`runtime-package.yml` runs the package contract and GStreamer replacement gate. Run
the commands below locally before changing the fork or its package recipe.

