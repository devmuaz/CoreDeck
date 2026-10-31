# Configure a Ninja tree with tests enabled, then build one target.
# Usage: cmake-build.ps1 -BuildType <Debug|Release> -Target <CoreDeck|coredeck_tests>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Debug', 'Release')]
    [string] $BuildType,

    [Parameter(Mandatory = $true)]
    [ValidateSet('CoreDeck', 'coredeck_tests')]
    [string] $Target
)

$ErrorActionPreference = 'Stop'

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Folder = if ($BuildType -eq 'Debug') { 'build-debug' } else { 'build-release' }
$BuildDir = Join-Path $Root $Folder

$VsWhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $VsWhere)) {
    Write-Error "Visual Studio installer was not found: $VsWhere"
    exit 1
}

$VsInstall = (& $VsWhere -latest -products * -property installationPath | Select-Object -First 1)
if ($VsInstall) {
    $VsInstall = $VsInstall.Trim()
}
if (-not $VsInstall) {
    Write-Error 'Visual Studio was not found.'
    exit 1
}

Import-Module (Join-Path $VsInstall 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $VsInstall -SkipAutomaticLocation -DevCmdArguments '-arch=amd64 -host_arch=amd64'

cmake -S "$Root" -B "$BuildDir" -G Ninja -DCMAKE_BUILD_TYPE="$BuildType" -DCOREDECK_BUILD_TESTS=ON
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

cmake --build "$BuildDir" --parallel --target "$Target"
exit $LASTEXITCODE
