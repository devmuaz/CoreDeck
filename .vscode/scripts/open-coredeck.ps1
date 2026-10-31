# Launch the Windows CoreDeck executable without blocking the task.
# Usage: open-coredeck.ps1 -BuildType <Debug|Release>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Debug', 'Release')]
    [string] $BuildType
)

$ErrorActionPreference = 'Stop'

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Folder = if ($BuildType -eq 'Debug') { 'build-debug' } else { 'build-release' }
$Exe = Join-Path $Root $Folder 'CoreDeck.exe'

if (-not (Test-Path $Exe)) {
    Write-Error "Windows executable not found: $Exe"
    exit 1
}

Start-Process $Exe
