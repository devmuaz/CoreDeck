$ErrorActionPreference = 'Stop'

$Release = 'v1.0-v1.19'
$Base = "https://github.com/mlocati/gettext-iconv-windows/releases/download/$Release"
$Archives = @(
    @{
        Name = 'gettext1.0-iconv1.19-shared-64.zip'
        Sha256 = 'c2f195fc4ed3df4070fb08ff88c2f724afd1eed4e28f859efae1000bb7ba1152'
        Dest = 'tools'
    },
    @{
        Name = 'gettext1.0-iconv1.19-shared-64-dev-msvc.zip'
        Sha256 = 'e181a82fa1ab9cb37cb67868701120bf140f0b34cbd7c0911cb27f03dcc88772'
        Dest = 'dev'
    }
)

$RootParent = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$Root = Join-Path $RootParent 'coredeck-gettext'
New-Item -ItemType Directory -Force -Path $Root | Out-Null

foreach ($Archive in $Archives) {
    $Zip = Join-Path $Root $Archive.Name
    $Url = "$Base/$($Archive.Name)"
    Write-Host "Downloading $Url"
    & curl.exe -fsSL -o $Zip $Url
    if ($LASTEXITCODE -ne 0) {
        throw "Download failed: $Url"
    }
    $Hash = (Get-FileHash -Algorithm SHA256 $Zip).Hash.ToLowerInvariant()
    if ($Hash -ne $Archive.Sha256) {
        throw "Checksum mismatch for $($Archive.Name): $Hash"
    }
    $Dest = Join-Path $Root $Archive.Dest
    if (Test-Path $Dest) {
        Remove-Item -Recurse -Force $Dest
    }
    New-Item -ItemType Directory -Force -Path $Dest | Out-Null
    & tar.exe -xf $Zip -C $Dest
    if ($LASTEXITCODE -ne 0) {
        throw "Extract failed: $Zip"
    }
}

Copy-Item -Force (Join-Path $Root 'dev\lib\intl.dll.lib') (Join-Path $Root 'dev\lib\intl.lib')

$Dev = (Resolve-Path (Join-Path $Root 'dev')).Path -replace '\\', '/'
$ToolsBin = (Resolve-Path (Join-Path $Root 'tools\bin')).Path
$ToolsBinCMake = $ToolsBin -replace '\\', '/'

if ($env:GITHUB_PATH) {
    Add-Content -Path $env:GITHUB_PATH -Value $ToolsBin
}
if ($env:GITHUB_ENV) {
    Add-Content -Path $env:GITHUB_ENV -Value "COREDECK_GETTEXT_ROOT=$Dev"
    Add-Content -Path $env:GITHUB_ENV -Value "COREDECK_GETTEXT_TOOLS=$ToolsBinCMake"
}

Write-Host "gettext tools: $ToolsBinCMake"
Write-Host "gettext prefix: $Dev"
