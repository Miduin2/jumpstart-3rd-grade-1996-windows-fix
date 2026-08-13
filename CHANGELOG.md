# Changelog

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
