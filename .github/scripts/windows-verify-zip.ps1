$ErrorActionPreference = 'Stop'

$zip = Get-ChildItem release -Filter *.zip | Select-Object -First 1
if (-not $zip) { throw "No Windows zip was produced" }
$names = @(tar -tf $zip.FullName)
foreach ($pattern in @('*intl*.dll', '*iconv*.dll', '*charset*.dll')) {
    $hit = @($names | Where-Object { $_ -like $pattern })
    if ($hit.Count -eq 0) {
        $names | Write-Host
        throw "Windows package $($zip.Name) is missing a runtime DLL matching $pattern"
    }
    Write-Host "Packaged $($hit -join ', ')"
}
