# Run CTest, then fail if the build directory registered no tests.
# Usage: run-ctest.ps1 -BuildType <Debug|Release>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Debug', 'Release')]
    [string] $BuildType
)

$ErrorActionPreference = 'Stop'

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Folder = if ($BuildType -eq 'Debug') { 'build-debug' } else { 'build-release' }
$BuildDir = Join-Path $Root $Folder

ctest --test-dir "$BuildDir" --output-on-failure
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$Listed = ctest --test-dir "$BuildDir" -N | Select-String -Pattern 'Total Tests:\s+[1-9]'
if (-not $Listed) {
    [Console]::Error.WriteLine("error: CTest registered no tests in ${BuildDir}. Configure with -DCOREDECK_BUILD_TESTS=ON.")
    exit 1
}
