#Requires -Version 5.1
<#
.SYNOPSIS
  Builds the Supercell Wx NSIS bootstrapper (.exe) after cpack.

.DESCRIPTION
  Locates the CPack MSI and repo LICENSE.txt, stages VC_redist.{arch}.exe and MUI
  assets, then runs makensis to produce supercell-wx-v{version}-windows-{arch}.exe
  alongside the MSI.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $BuildDir,

    [Parameter(Mandatory = $true)]
    [string] $SourceDir,

    [string] $Version = '',

    [string] $Arch = '',

    [string] $MsiPath = '',

    [string] $LicensePath = '',

    [string] $VcRedistPath = '',

    [string] $NsisBin = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSVersion.Major -ge 7) {
    $PSNativeCommandUseErrorActionPreference = $true
}

function Resolve-ExistingPath {
    param([string] $Path, [string] $Label)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

$BuildDir = Resolve-ExistingPath $BuildDir 'Build directory'
$SourceDir = Resolve-ExistingPath $SourceDir 'Source directory'

if (-not $Version) {
    $cmakeLists = Join-Path $SourceDir 'CMakeLists.txt'
    $match = Select-String -Path $cmakeLists -Pattern 'set\(SCWX_VERSION\s+"([^"]+)"\)' |
        Select-Object -First 1
    if (-not $match) {
        throw "Could not determine SCWX_VERSION from $cmakeLists"
    }
    $Version = $match.Matches[0].Groups[1].Value
}

if (-not $Arch) {
    $Arch = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') {
        'arm64'
    } else {
        'x64'
    }
}
if ($Arch -notin @('x64', 'arm64')) {
    throw "Arch must be x64 or arm64, got: $Arch"
}

$packageStem = "supercell-wx-v$Version-windows-${Arch}"

if (-not $MsiPath) {
    $MsiPath = Join-Path $BuildDir "$packageStem.msi"
}
$MsiPath = Resolve-ExistingPath $MsiPath 'MSI'

if (-not $LicensePath) {
    $LicensePath = Join-Path $SourceDir 'LICENSE.txt'
}
$LicensePath = Resolve-ExistingPath $LicensePath 'License'

if (-not $VcRedistPath) {
    if (-not $env:VCToolsRedistDir) {
        throw 'VCToolsRedistDir is not set; run from a VS developer environment or pass -VcRedistPath'
    }
    $VcRedistPath = Join-Path $env:VCToolsRedistDir "VC_redist.${Arch}.exe"
}
$VcRedistPath = Resolve-ExistingPath $VcRedistPath 'VC++ redistributable'

$nsisDir = Resolve-ExistingPath (Join-Path $SourceDir 'scwx-qt\nsis') 'NSIS assets directory'
$nsiScript = Resolve-ExistingPath (Join-Path $nsisDir 'supercell-wx.nsi') 'NSIS script'
$headerBmp = Resolve-ExistingPath (Join-Path $nsisDir 'scwx-header.bmp') 'MUI header BMP'
$welcomeBmp = Resolve-ExistingPath (Join-Path $nsisDir 'scwx-welcome.bmp') 'MUI welcome BMP'
$pluginDir = Resolve-ExistingPath (Join-Path $SourceDir 'data\nsis\plugins\x86-unicode') 'NSIS Unicode plugins'
$null = Resolve-ExistingPath (Join-Path $pluginDir 'ShellExecAsUser.dll') 'ShellExecAsUser plugin'
$iconFile = Resolve-ExistingPath (Join-Path $SourceDir 'scwx-qt\res\icons\scwx-256.ico') 'Icon'

if (-not $NsisBin) {
    foreach ($candidate in @(
            "${env:ProgramFiles(x86)}\NSIS",
            "${env:ProgramFiles}\NSIS"
        )) {
        if (Test-Path -LiteralPath (Join-Path $candidate 'makensis.exe')) {
            $NsisBin = $candidate
            break
        }
    }
    if (-not $NsisBin) {
        $makensisCmd = Get-Command makensis.exe -ErrorAction SilentlyContinue
        if ($makensisCmd) {
            $NsisBin = Split-Path -Parent $makensisCmd.Source
        }
    }
}
if (-not $NsisBin -or -not (Test-Path -LiteralPath (Join-Path $NsisBin 'makensis.exe'))) {
    throw 'makensis.exe not found; install NSIS 3 or pass -NsisBin'
}
$NsisBin = (Resolve-Path -LiteralPath $NsisBin).Path
$makensis = Join-Path $NsisBin 'makensis.exe'

$stagingDir = Join-Path $BuildDir 'nsis'
New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null

$msiFileName = "$packageStem.msi"
$msiStaging = Join-Path $stagingDir $msiFileName
$redistStaging = Join-Path $stagingDir "VC_redist.${Arch}.exe"
$licenseStaging = Join-Path $stagingDir 'LICENSE.txt'
$iconStaging = Join-Path $stagingDir 'scwx-256.ico'
$headerStaging = Join-Path $stagingDir 'scwx-header.bmp'
$welcomeStaging = Join-Path $stagingDir 'scwx-welcome.bmp'
$outputExe = Join-Path $BuildDir "$packageStem.exe"

Copy-Item -LiteralPath $MsiPath -Destination $msiStaging -Force
Copy-Item -LiteralPath $VcRedistPath -Destination $redistStaging -Force
Copy-Item -LiteralPath $LicensePath -Destination $licenseStaging -Force
Copy-Item -LiteralPath $iconFile -Destination $iconStaging -Force
Copy-Item -LiteralPath $headerBmp -Destination $headerStaging -Force
Copy-Item -LiteralPath $welcomeBmp -Destination $welcomeStaging -Force

Write-Host "Building NSIS bootstrapper:"
Write-Host "  Version:  $Version"
Write-Host "  Arch:     $Arch"
Write-Host "  MSI:      $msiStaging"
Write-Host "  License:  $licenseStaging"
Write-Host "  VCRedist: $redistStaging"
Write-Host "  Header:   $headerStaging"
Write-Host "  Welcome:  $welcomeStaging"
Write-Host "  Output:   $outputExe"

# Run from staging so default relative !defines resolve if any are omitted.
Push-Location $stagingDir
try {
    & $makensis -NOCD `
        "-DSCWX_VERSION=$Version" `
        "-DSCWX_ARCH=$Arch" `
        "-DSCWX_OUTFILE=$outputExe" `
        "-DSCWX_MSI=$msiStaging" `
        "-DSCWX_MSI_FILE=$msiFileName" `
        "-DSCWX_VC_REDIST=$redistStaging" `
        "-DSCWX_LICENSE=$licenseStaging" `
        "-DSCWX_HEADER_BMP=$headerStaging" `
        "-DSCWX_WELCOME_BMP=$welcomeStaging" `
        "-DSCWX_ICON=$iconStaging" `
        "-DSCWX_NSIS_PLUGINDIR=$pluginDir" `
        $nsiScript
    if ($LASTEXITCODE -ne 0) {
        throw "makensis.exe failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

if (-not (Test-Path -LiteralPath $outputExe)) {
    throw "NSIS bootstrapper was not created: $outputExe"
}

Write-Host "Created $outputExe"
