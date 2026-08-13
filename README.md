# Trampolín 5 / JumpStart 3rd Grade — modern Windows patch

This unofficial, content-free compatibility patch builds a portable copy of
either supported game from a user-owned original CD:

- *Trampolín Educación Primaria 5.º Curso* (Spanish);
- *JumpStart 3rd Grade* (English).

It provides 4:3 borderless scaling, correct mouse coordinates, portable media
paths, working Spanish audio from long installation locations, and a fix for
the `WSOUND32.DLL` shutdown hang seen on modern Windows.

> **Validated scope:** Windows 10 x64. Both editions were runtime-tested for
> image, music, voices/effects, menu interaction, and clean exit from the
> initial player-selection area. Windows 11 and unusual audio drivers remain
> valid targets for user reports but are not yet formally validated.

> **Other language editions:** the compatibility core is not inherently tied
> to Spanish or English. Both validated editions use the exact same WinG proxy,
> launcher, local WinG patch, short-path handling, and audio-shutdown fix.
> What changes between editions is principally how the original CD must be
> reconstructed and which paths/configuration that layout needs. The safe
> builder below deliberately accepts only the two fully verified CD builds,
> but porters and coding assistants can adapt the same core to another edition.
> See [Porting other editions](PORTING_OTHER_EDITIONS.md).

> This project contains no game executable, resource, audio, disc image, or
> original Microsoft WinG binary. You need your own legitimate original CD.

## Build your portable copy

1. Extract this patch to a normal writable folder.
2. Insert the original CD, or right-click its `.iso` file and choose **Mount**.
   Windows will show a new drive under **This PC**, such as `F:`. An unmodified
   complete extraction of the CD also works.
3. Double-click `Build portable game.cmd` and answer its two prompts. When it
   asks for the **CD root**, enter the new drive, for example `F:\`.

The CD root is the location that directly contains the `3G` and `SUPPORT`
folders. It is **not** the folder where the `.iso` file is stored.

### PowerShell alternative

Open PowerShell in the extracted patch folder and run this as one line:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build-portable-from-cd.ps1 -SourceRoot "F:\" -OutputRoot "$env:USERPROFILE\Games\JumpStart3"
```

Replace `F:\` with the mounted CD drive. Choose an output folder that does not
yet exist. The script detects the Spanish or English edition automatically,
verifies known hashes, reconstructs the files selected by the old installer,
and verifies the complete output manifest.

Start the result with `Play Trampolin 5.exe` or
`Play JumpStart 3rd Grade.exe`. The CD is not required after construction.

## Uninstall

The patch does not install system components. Close the game and remove the
portable output folder if you no longer want it. Saved player profiles live in
that folder, so back them up first if desired.

## Supported source files

| Edition | `3G.EXE` SHA-256 |
| --- | --- |
| Trampolín 5 | `6C7FF278C39ACFADD5611F7EE996F8336F53A9EE52DDE34A4DAEC00CFB8936DD` |
| JumpStart 3rd Grade | `63F72788226CA073F6C813008FF3FF889E2C8D93F10179E8CCF994C316CFDC03` |

Unknown builds are refused rather than patched speculatively.

This refusal is a safety boundary in the automatic CD builder, not evidence
that the runtime patch is incompatible with every other build. If you have a
Portuguese, French, German, later English, or other edition, follow
[Porting other editions](PORTING_OTHER_EDITIONS.md) from a disposable copy and
report the resulting hashes and layout. Do not replace or patch an unknown
WinG binary blindly.

## What the patch changes

- It copies files from the original CD into a faithful portable layout.
- It patches two bytes in the user's local WinG copy to remove the obsolete
  “must be installed in the system directory” check.
- It adds the open-source `WING32.DLL` proxy and launcher included here.
- It does not modify `3G.EXE`, `WSOUND32.DLL`, resources, or the original CD.

The launcher uses a temporary drive alias and temporary INI paths while the
game runs, then restores the previous state. The alias is removed on exit.

## Bug reports

Do not upload game files or disc images. Report only:

- edition and `3G.EXE` SHA-256;
- Windows version, display resolution/scaling, and audio device;
- the exact stage where a problem occurs;
- whether the generated manifest verification succeeded.

## Disclaimer

This is an independent preservation and compatibility project. It is not
affiliated with or endorsed by Knowledge Adventure, Davidson & Associates,
Havas, Microsoft, or any current rights holder. All third-party names and
assets belong to their respective owners.

## License

Original compatibility code and scripts are available under the
[MIT License](LICENSE). That license does not apply to either game or any
third-party component created from the user's CD.
