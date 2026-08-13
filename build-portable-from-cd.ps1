[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputRoot,

    [string]$ComponentRoot
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ComponentRoot)) {
    $ComponentRoot = Join-Path $PSScriptRoot 'components'
}

$originalWingSha256 = 'BB1F552E2525E784B61D2FE0CA23F3402ADEC05AA5F92F4C1DFBEA3966A84CBB'
$patchedWingSha256 = 'EDD26762E7DFD37C5A4306698C77D1A0C4C1F7E734946B3B82C534FAC13065F6'
$proxySha256 = '3B85879F38108E49B93A414CCA78D7F91575D3A7C692DC4D995B2662974AE4DE'
$launcherSha256 = '9F6E951B296B1D192BF6967D24B6D5FFFFF912581513DA95EAFB9DA165A525A5'

$supportedEditions = @{
    '6C7FF278C39ACFADD5611F7EE996F8336F53A9EE52DDE34A4DAEC00CFB8936DD' = [ordered]@{
        Id = 'spanish-trampolin-5'
        DisplayName = 'Trampolin Educacion Primaria 5 Curso'
        LauncherName = 'Play Trampolin 5.exe'
        SoundSha256 = '48C5CCDF926246C27700BC6F2E31654204525FBAF99A65D8E13EC60B9299F4BD'
        ManifestName = 'spanish-trampolin-5.sha256.json'
    }
    '63F72788226CA073F6C813008FF3FF889E2C8D93F10179E8CCF994C316CFDC03' = [ordered]@{
        Id = 'english-jumpstart-3rd-grade'
        DisplayName = 'JumpStart 3rd Grade'
        LauncherName = 'Play JumpStart 3rd Grade.exe'
        SoundSha256 = 'D1867FB71B62DAAC6A1FB5A9A50864074279375FB64D10DB602F33C3E1EE41FF'
        ManifestName = 'english-jumpstart-3rd-grade.sha256.json'
    }
}

function Get-Sha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing required file: $Path"
    }
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Assert-Hash([string]$Path, [string]$Expected) {
    $actual = Get-Sha256 $Path
    if ($actual -ne $Expected) {
        throw "Unsupported or damaged file: $Path`nExpected SHA-256: $Expected`nActual SHA-256:   $actual"
    }
}

function Copy-Layer([string]$Source, [string]$Destination, [bool]$Overwrite) {
    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        throw "Missing required CD directory: $Source"
    }
    foreach ($file in Get-ChildItem -LiteralPath $Source -Force -File) {
        $target = Join-Path $Destination $file.Name
        if ($Overwrite -or -not (Test-Path -LiteralPath $target)) {
            Copy-Item -LiteralPath $file.FullName -Destination $target -Force:$Overwrite
        }
    }
}

function Clear-CopiedMediaAttributes([string]$Root) {
    $flags = [IO.FileAttributes]::ReadOnly -bor
        [IO.FileAttributes]::Hidden -bor
        [IO.FileAttributes]::System
    foreach ($file in Get-ChildItem -LiteralPath $Root -Force -File -Recurse) {
        $file.Attributes = $file.Attributes -band (-bnot $flags)
    }
}

function Copy-MissingFile([string]$Source, [string]$Destination) {
    if (-not (Test-Path -LiteralPath $Destination)) {
        Copy-Item -LiteralPath $Source -Destination $Destination
    }
}

function Install-PatchedLegacyWing([string]$OriginalWing, [string]$Destination) {
    Assert-Hash $OriginalWing $originalWingSha256
    $bytes = [IO.File]::ReadAllBytes($OriginalWing)
    if ($bytes.Length -le 0xA56 -or $bytes[0xA55] -ne 0x75 -or $bytes[0xA56] -ne 0x11) {
        throw 'The expected WinG location check was not found.'
    }
    $bytes[0xA55] = 0x90
    $bytes[0xA56] = 0x90
    $target = Join-Path $Destination 'WING32.legacy.dll'
    [IO.File]::WriteAllBytes($target, $bytes)
    Assert-Hash $target $patchedWingSha256
}

function Write-SafeGameIni([string]$Destination) {
    $content = @"
[Video]
FullScreen=0
HideTaskBar=0
NoWarnings=1

[CONTROL]
TuneFileExtension=WGM
"@ -replace "`n", "`r`n"
    [IO.File]::WriteAllText(
        (Join-Path $Destination '3G.INI'),
        $content,
        [Text.Encoding]::ASCII)
}

function Test-ExpectedManifest([string]$EditionRoot, [string]$ManifestPath) {
    if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
        throw "Missing release manifest: $ManifestPath"
    }
    $expectedEntries = ConvertFrom-Json -InputObject (
        [IO.File]::ReadAllText($ManifestPath, [Text.Encoding]::UTF8))
    $expectedByPath = @{}
    foreach ($entry in $expectedEntries) {
        $expectedByPath[$entry.path.ToLowerInvariant()] = $entry
        $path = Join-Path $EditionRoot $entry.path.Replace('/', '\')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Reconstruction is incomplete; missing: $($entry.path)"
        }
        if ((Get-Item -LiteralPath $path).Length -ne $entry.size) {
            throw "Reconstruction size mismatch: $($entry.path)"
        }
        Assert-Hash $path $entry.sha256
    }

    $actualPaths = Get-ChildItem -LiteralPath $EditionRoot -Force -File -Recurse |
        ForEach-Object {
            $_.FullName.Substring($EditionRoot.Length + 1).Replace('\', '/')
        }
    foreach ($path in $actualPaths) {
        if (-not $expectedByPath.ContainsKey($path.ToLowerInvariant())) {
            throw "Reconstruction contains an unexpected file: $path"
        }
    }
}

if (-not (Test-Path -LiteralPath $SourceRoot -PathType Container)) {
    throw "The source path is not a directory: $SourceRoot"
}
$source = (Resolve-Path -LiteralPath $SourceRoot).Path.TrimEnd('\')
$components = (Resolve-Path -LiteralPath $ComponentRoot).Path.TrimEnd('\')
$output = [IO.Path]::GetFullPath($OutputRoot).TrimEnd('\')
if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "The CD root is not a directory: $source"
}
if (Test-Path -LiteralPath $output) {
    throw "The output already exists; choose a new empty destination: $output"
}
if ($output.StartsWith($source + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The output must not be created inside the source CD tree.'
}

$sourceExe = Join-Path $source 'SUPPORT\W32\3G.EXE'
if (-not (Test-Path -LiteralPath $sourceExe -PathType Leaf)) {
    $discImages = @(Get-ChildItem -LiteralPath $source -Force -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in @('.iso', '.bin', '.img') })
    if ($discImages.Count -gt 0) {
        throw "SourceRoot points to a folder containing a disc-image file, not to the mounted CD root. Mount the image in File Explorer, then use the new drive shown in This PC (for example, F:\)."
    }
    throw "SourceRoot is not a supported CD root. The selected directory must contain the 3G and SUPPORT folders directly: $source"
}
$exeSha256 = Get-Sha256 $sourceExe
$edition = $supportedEditions[$exeSha256]
if ($null -eq $edition) {
    throw "This disc edition is not supported.`n3G.EXE SHA-256: $exeSha256"
}

$sourceWing = Join-Path $source 'SUPPORT\WINSYS\WING32.DLL'
$sourceSound = Join-Path $source 'SUPPORT\W32\WSOUND32.DLL'
Assert-Hash $sourceWing $originalWingSha256
Assert-Hash $sourceSound $edition.SoundSha256

$proxySource = Join-Path $components 'WING32.DLL'
$launcherSource = Join-Path $components 'GameVaultLauncher.exe'
$manifestSource = Join-Path $components ('manifests\' + $edition.ManifestName)
Assert-Hash $proxySource $proxySha256
Assert-Hash $launcherSource $launcherSha256

$createdOutput = $false
try {
    New-Item -ItemType Directory -Path $output | Out-Null
    $createdOutput = $true

    if ($edition.Id -eq 'spanish-trampolin-5') {
        $gameRoot = Join-Path $output 'hd'
        $cdRoot = Join-Path $output 'cd\3G'
        New-Item -ItemType Directory -Path $gameRoot -Force | Out-Null
        New-Item -ItemType Directory -Path $cdRoot -Force | Out-Null

        Copy-Layer (Join-Path $source '3G') $gameRoot $true
        foreach ($layer in @('SUPPORT\W32', 'SUPPORT\CMN', 'SUPPORT\WGM32')) {
            Copy-Layer (Join-Path $source $layer) $gameRoot $true
        }
        Copy-Item -LiteralPath (Join-Path $source '3G.CNT') -Destination (Join-Path $gameRoot '3G.CNT') -Force
        Copy-Layer (Join-Path $source '3G') $cdRoot $true
    }
    else {
        $gameRoot = Join-Path $output 'game'
        New-Item -ItemType Directory -Path $gameRoot | Out-Null

        Copy-Layer (Join-Path $source '3G\32BIT') $gameRoot $true
        Copy-Layer (Join-Path $source '3G') $gameRoot $true
        foreach ($layer in @('SUPPORT\W32', 'SUPPORT\CMN', 'SUPPORT\WGM32')) {
            Copy-Layer (Join-Path $source $layer) $gameRoot $false
        }
        foreach ($name in @('3G.CNT', '3G.HLP')) {
            Copy-MissingFile (Join-Path $source $name) (Join-Path $gameRoot $name)
        }
    }

    Clear-CopiedMediaAttributes $output
    Install-PatchedLegacyWing $sourceWing $gameRoot
    Copy-Item -LiteralPath $proxySource -Destination (Join-Path $gameRoot 'WING32.DLL')
    Copy-Item -LiteralPath $launcherSource -Destination (Join-Path $output $edition.LauncherName)
    Write-SafeGameIni $gameRoot

    Test-ExpectedManifest $output $manifestSource
    Copy-Item -LiteralPath $manifestSource -Destination (Join-Path $output 'MANIFEST.sha256.json')

    Write-Host "Portable edition built and verified: $($edition.DisplayName)" -ForegroundColor Green
    Write-Host "Output: $output"
    Write-Host "Launcher: $($edition.LauncherName)"
}
catch {
    if ($createdOutput -and (Test-Path -LiteralPath $output)) {
        Remove-Item -LiteralPath $output -Recurse -Force -ErrorAction SilentlyContinue
    }
    throw
}
