#Requires -Version 5.1
<#
.SYNOPSIS
  Install Conan dependencies for CI, optionally using a private extra remote.

.DESCRIPTION
  Configures Conan profiles, optionally adds CONAN_REMOTE_URL as remote "scwx",
  installs with ConanCenter first then scwx, and uploads only packages whose
  graph binary status is Build when CONAN_REMOTE_USERNAME and
  CONAN_REMOTE_PASSWORD are set.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $SourceDir,

    [Parameter(Mandatory = $true)]
    [string] $BuildDir,

    [Parameter(Mandatory = $true)]
    [string] $BuildType,

    [string] $BuildProfile = '',

    [string] $HostProfile = '',

    [string] $Profile = '',

    [string] $PackageManagerArgs = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSVersion.Major -ge 7) {
    $PSNativeCommandUseErrorActionPreference = $true
}

function Get-OptionalEnv {
    param([string] $Name)
    $value = [Environment]::GetEnvironmentVariable($Name)
    if ([string]::IsNullOrWhiteSpace($value)) {
        return ''
    }
    return $value.Trim()
}

function Test-ConanPkgListHasEntries {
    param([string] $Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        return $false
    }
    $raw = Get-Content -Raw -LiteralPath $Path
    if ([string]::IsNullOrWhiteSpace($raw)) {
        return $false
    }
    $json = $raw | ConvertFrom-Json
    if ($null -eq $json) {
        return $false
    }
    foreach ($prop in $json.PSObject.Properties) {
        $value = $prop.Value
        if ($null -eq $value) {
            continue
        }
        if (@($value.PSObject.Properties).Count -gt 0) {
            return $true
        }
    }
    return $false
}

function Invoke-Conan {
    param([Parameter(Mandatory = $true)][string[]] $ConanArgs)
    & conan @ConanArgs
    if ($LASTEXITCODE -ne 0) {
        throw "conan $($ConanArgs -join ' ') failed with exit code $LASTEXITCODE"
    }
}

if (-not $Profile -and (-not $BuildProfile -or -not $HostProfile)) {
    throw 'Specify -Profile or both -BuildProfile and -HostProfile.'
}

$sourcePath = (Resolve-Path -LiteralPath $SourceDir).Path
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$buildPath = (Resolve-Path -LiteralPath $BuildDir).Path
$conanOutDir = Join-Path $buildPath 'conan'
New-Item -ItemType Directory -Force -Path $conanOutDir | Out-Null

$profilesDir = Join-Path $sourcePath 'tools/conan/profiles'
if ($Profile) {
    Invoke-Conan -ConanArgs @('config', 'install', (Join-Path $profilesDir $Profile), '-tf', 'profiles')
}
else {
    Invoke-Conan -ConanArgs @('config', 'install', (Join-Path $profilesDir $BuildProfile), '-tf', 'profiles')
    Invoke-Conan -ConanArgs @('config', 'install', (Join-Path $profilesDir $HostProfile), '-tf', 'profiles')
}

$remoteUrl = Get-OptionalEnv 'CONAN_REMOTE_URL'
$remoteUser = Get-OptionalEnv 'CONAN_REMOTE_USERNAME'
$remotePassword = Get-OptionalEnv 'CONAN_REMOTE_PASSWORD'
$hasExtraRemote = -not [string]::IsNullOrWhiteSpace($remoteUrl)
$canUpload = $hasExtraRemote -and
    -not [string]::IsNullOrWhiteSpace($remoteUser) -and
    -not [string]::IsNullOrWhiteSpace($remotePassword)

if ($hasExtraRemote) {
    Invoke-Conan -ConanArgs @('remote', 'add', 'scwx', $remoteUrl, '--force')
}

if ($canUpload) {
    $env:CONAN_LOGIN_USERNAME_SCWX = $remoteUser
    $env:CONAN_PASSWORD_SCWX = $remotePassword
    try {
        Invoke-Conan -ConanArgs @('remote', 'login', 'scwx', $remoteUser)
    }
    finally {
        Remove-Item Env:CONAN_PASSWORD_SCWX -ErrorAction SilentlyContinue
    }
}

$graphFile = Join-Path $buildPath 'conan-graph.json'
$installArgs = @(
    'install', $sourcePath,
    '--remote', 'conancenter',
    '--build', 'missing',
    '--settings:all', "build_type=$BuildType",
    '--output-folder', $conanOutDir,
    '--format', 'json',
    '--out-file', $graphFile
)
if ($hasExtraRemote) {
    $installArgs += @('--remote', 'scwx')
}
if ($Profile) {
    $installArgs += @('--profile:all', $Profile)
}
else {
    $installArgs += @('--profile:build', $BuildProfile, '--profile:host', $HostProfile)
}
if (-not [string]::IsNullOrWhiteSpace($PackageManagerArgs)) {
    $installArgs += ($PackageManagerArgs -split '\s+' | Where-Object { $_ })
}

Invoke-Conan -ConanArgs $installArgs

if ($canUpload) {
    $pkgListFile = Join-Path $buildPath 'conan-built.json'
    Invoke-Conan -ConanArgs @(
        'list',
        "--graph=$graphFile",
        '--graph-binaries=build',
        '--format=json',
        "--out-file=$pkgListFile"
    )
    if (Test-ConanPkgListHasEntries $pkgListFile) {
        $env:CONAN_LOGIN_USERNAME_SCWX = $remoteUser
        $env:CONAN_PASSWORD_SCWX = $remotePassword
        try {
            Invoke-Conan -ConanArgs @('upload', "--list=$pkgListFile", '-r', 'scwx', '-c')
        }
        finally {
            Remove-Item Env:CONAN_PASSWORD_SCWX -ErrorAction SilentlyContinue
        }
    }
    else {
        Write-Host 'No packages were built from source; skipping Conan upload.'
    }
}

Invoke-Conan -ConanArgs @('cache', 'clean')
