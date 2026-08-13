# Porting the compatibility core to other editions

## Status and promise

The runtime compatibility core is shared by the two validated releases:

- *Trampolín Educación Primaria 5.º Curso* (Spanish);
- *JumpStart 3rd Grade* (English).

They use byte-identical copies of the project's `WING32.DLL` proxy and native
launcher, the same two-byte patch to a user-supplied Microsoft WinG binary,
the same temporary short-drive mechanism, and the same `WSOUND32.DLL`
shutdown workaround.

This makes other editions reasonable porting candidates. It does **not** make
an untested edition officially supported. Its CD layout, installer layers,
configuration syntax, WinG build, sound library, and runtime behaviour still
need to be examined.

The public CD builder rejects unknown `3G.EXE` hashes intentionally. Do not
remove that check from the stable path merely to make an unknown disc pass.
Create and test a new edition recipe, or prepare an experimental installed
tree separately.

## What is common

The following project components are identical in both validated outputs:

- `WING32.DLL`: forwards the ten legacy WinG exports, provides borderless 4:3
  presentation and corrected mouse coordinates, and installs the bounded
  `waveOutClose` workaround after `WSOUND32.DLL` loads;
- `GameVaultLauncher.exe`: creates a temporary drive alias, writes temporary
  `KA.INI` and `3G.INI` paths, starts `3G.EXE`, then restores the previous
  files and removes the alias;
- `WING32.legacy.dll`: generated locally from the original CD's known WinG
  binary by changing `75 11` to `90 90` at offset `0xA55`.

The two known `WSOUND32.DLL` files are different binaries (Spanish: 155,136
bytes; English: 144,896 bytes), but both work with the same proxy hook because
both import `waveOutClose` from `WINMM.dll`. An unknown sound library must be
checked for the same import and tested; matching the filename alone is not a
guarantee.

## Layouts understood by the current launcher

The launcher recognises these package shapes relative to itself.

Split-media layout, used by the validated Spanish edition:

```text
portable-root/
  Play Trampolin 5.exe
  hd/
    3g.exe
    WING32.DLL
    WING32.legacy.dll
    WSOUND32.DLL
    ...installed files...
  cd/3G/
    Sound.bal
    ...CD media files...
```

Merged-media layout, used by the validated English edition:

```text
portable-root/
  Play JumpStart 3rd Grade.exe
  game/
    3g.exe
    WING32.DLL
    WING32.legacy.dll
    WSOUND32.DLL
    Sound.bal
    ...installed and CD media files...
```

The launcher detects the split layout from `hd/3g.exe` plus
`cd/3G/Sound.bal`; otherwise it tries `game`. It currently uses `SOUND2.BAL`
as the signal for the English path block and product name. An edition with a
different signal may need a small launcher detection/configuration change even
when the proxy itself works unchanged.

Renaming the launcher is harmless. Its location relative to `hd`, `cd`, or
`game` is what matters.

## Recommended experimental workflow

Work only from a disposable copy. Keep the original disc image and any
existing installation unchanged.

1. Inventory the complete CD and record at least the SHA-256, size, and path
   of `3G.EXE`, `WSOUND32.DLL`, `WING32.DLL`, `Sound.bal`, `SOUND2.BAL` if
   present, and all INI files.
2. Determine what the original installer selects. A successful existing
   installation is the best reference. Otherwise inspect installer layers and
   compare their same-named files before choosing overwrite order.
3. Arrange the reconstructed files as either the split or merged layout above.
   If neither represents the edition accurately, adapt the launcher rather
   than moving files blindly.
4. Verify the original WinG binary. The current automatic patch is valid only
   for SHA-256
   `BB1F552E2525E784B61D2FE0CA23F3402ADEC05AA5F92F4C1DFBEA3966A84CBB`.
   For that exact file, create `WING32.legacy.dll` using the documented
   two-byte change. Do not apply those offsets to a different hash.
5. Place the project's proxy beside `3g.exe` as `WING32.DLL`, retain the
   patched original as `WING32.legacy.dll`, and place a copy of the launcher at
   the portable root.
6. Test image, 4:3 scaling, mouse coordinates, music, speech/effects, several
   activities, saved-player creation, relaunch, and exit from both the initial
   selection screen and normal gameplay.
7. Confirm that the temporary drive alias and temporary INI changes disappear
   after normal exit and after a failed launch.
8. Generate a complete output manifest. Once the reconstruction is
   deterministic, add a separate detected edition and recipe to
   `build-portable-from-cd.ps1` instead of weakening the existing hash checks.

## Guidance for coding assistants

Treat an unknown-edition request as a port of the source/reconstruction recipe,
not as a request to redesign the validated runtime core immediately.

- Preserve the proxy and launcher binaries initially.
- Identify the source layout and installer overwrite order before copying.
- Compare component hashes independently; a new `3G.EXE` does not imply that
  WinG or the sound library is also new.
- Keep stable support and experimental support visibly separate.
- Never claim validation without actually launching and testing that edition.
- Never include proprietary executables, resources, audio, or disc images in
  a patch or bug report.

If the current launcher heuristic is insufficient, make the smallest explicit
edition-profile change and document it. Avoid broad guesses based only on a
language or filename.

## Useful report for adding official support

Do not upload game files. A useful report contains:

- title, language, publisher, year, and disc label;
- Windows version and audio/display setup;
- directory tree with file sizes;
- SHA-256 values for `3G.EXE`, `WSOUND32.DLL`, and the original WinG DLL;
- whether `Sound.bal` and `SOUND2.BAL` exist and where;
- the reconstructed layout and installer layer order;
- results for image, input, every audio category, saving/relaunching, and clean
  shutdown;
- any launcher or INI adjustment required.

With that evidence, the edition can receive its own safe builder recipe and
verified output manifest while continuing to use the common compatibility
core wherever the tests support it.
