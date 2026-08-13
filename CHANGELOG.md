# Changelog

## 1.0.2 — 2026-08-14

- Clarifies on the project front page that both validated editions share one
  runtime compatibility core while their CD reconstruction recipes differ.
- Adds `PORTING_OTHER_EDITIONS.md` with layouts, safety boundaries, an
  experimental workflow, and direct guidance for coding assistants adapting
  another language or build.
- Adds a prominent latest-release download path and a concrete successful
  output example for first-time GitHub users.
- Adds a concise Spanish installation guide in `README.es.md`.
- Packages all current documentation in the downloadable release ZIP while
  retaining the exact runtime binaries and builder from 1.0.1.

## 1.0.1 — 2026-08-13

- Copies hidden files from mounted optical media, including the Spanish
  `SUPPORT\CMN\3G.GID` required by the verified reconstruction manifest.
- Clears optical-media read-only, hidden, and system attributes in the
  portable output.
- Adds a guided `Build portable game.cmd` entry point.
- Clarifies that `SourceRoot` is the mounted CD drive, not the folder
  containing an ISO, and provides a specific error for that mistake.

## 1.0.0 — 2026-08-13

- Reconstructs the Spanish and English editions exclusively from original CD
  files and verifies their complete output manifests.
- Adds portable media paths and a short temporary drive alias for the Spanish
  sound library.
- Adds borderless 4:3 presentation and scaled mouse input through a local WinG
  proxy.
- Fixes the reproducible shutdown hang in `WSOUND32.DLL!waveOutClose` by
  resetting the device and bounding the driver close wait.
- Restores both `KA.INI` and `3G.INI` after every run.
- Builds the launcher and proxy reproducibly with `/Brepro`.
