$ErrorActionPreference = 'Stop'

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)]
        [scriptblock]$Command
    )
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE"
    }
}

Invoke-Native { choco install wixtoolset }

Push-Location build
try {
    Invoke-Native { cpack -G ZIP }
    Invoke-Native { cpack -G WIX }

    $release = Join-Path (Get-Location) '..\release'
    New-Item -ItemType Directory -Force -Path $release | Out-Null

    $msi = @(Get-ChildItem -File -Filter *.msi)
    $zip = @(Get-ChildItem -File -Filter *.zip)
    if ($msi.Count -eq 0) { throw "cpack did not produce an MSI" }
    if ($zip.Count -eq 0) { throw "cpack did not produce a ZIP" }

    foreach ($file in $msi) {
        Copy-Item -Force $file.FullName (Join-Path $release 'coredeck-windows-x86-64.msi')
    }
    foreach ($file in $zip) {
        Copy-Item -Force $file.FullName (Join-Path $release 'coredeck-windows-x86-64.zip')
    }
}
finally {
    Pop-Location
}
