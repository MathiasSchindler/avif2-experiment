param(
    [string]$Clang = "",
    [string]$Encoder = "",
    [int]$Jobs = 4,
    [switch]$SkipAssets,
    [switch]$ResumeAssets
)

$ErrorActionPreference = "Stop"
$site = Join-Path $PSScriptRoot "site"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if (-not $Encoder) {
    $Encoder = Join-Path $root "build\reference\avm\avmenc.exe"
}

New-Item -ItemType Directory -Force $site | Out-Null
Copy-Item (Join-Path $PSScriptRoot "index.html") $site -Force
Copy-Item (Join-Path $PSScriptRoot "styles.css") $site -Force
Copy-Item (Join-Path $PSScriptRoot "app.js") $site -Force

if (-not $SkipAssets) {
    $assetArguments = @(
        (Join-Path $PSScriptRoot "generate_assets.py"),
        "--output", $site, "--encoder", $Encoder, "--jobs", $Jobs
    )
    if ($ResumeAssets) { $assetArguments += "--resume" }
    & python @assetArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Viewer asset generation failed with exit code $LASTEXITCODE"
    }
}

$wasmArguments = @{ OutputDirectory = $site }
if ($Clang) { $wasmArguments.Clang = $Clang }
& (Join-Path $PSScriptRoot "..\wasm\build.ps1") @wasmArguments
if ($LASTEXITCODE -ne 0) {
    throw "WebAssembly build failed with exit code $LASTEXITCODE"
}

& python (Join-Path $PSScriptRoot "validate_site.py") $site
if ($LASTEXITCODE -ne 0) {
    throw "Viewer site validation failed with exit code $LASTEXITCODE"
}

Get-ChildItem $site | Select-Object Name, Length