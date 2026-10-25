$ErrorActionPreference = 'Stop'

if (-not $env:BUILD_TYPE) { throw "BUILD_TYPE is required" }

$stage = Join-Path $env:RUNNER_TEMP 'coredeck-stage'
cmake --install build --config $env:BUILD_TYPE --prefix $stage
if ($LASTEXITCODE -ne 0) { throw "cmake --install failed with exit code $LASTEXITCODE" }

foreach ($pattern in @('*intl*.dll', '*iconv*.dll', '*charset*.dll')) {
    $hit = @(Get-ChildItem $stage -File -Filter $pattern)
    if ($hit.Count -eq 0) {
        Write-Host "Staged files:"
        Get-ChildItem $stage -Recurse -File | ForEach-Object { $_.FullName }
        throw "Installed CoreDeck is missing a runtime DLL matching $pattern"
    }
    Write-Host "Staged $($hit.Name -join ', ')"
}
