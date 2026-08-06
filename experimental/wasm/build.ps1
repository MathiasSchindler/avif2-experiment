param(
    [string]$Clang = "",
    [string]$OutputDirectory = $PSScriptRoot
)

$ErrorActionPreference = "Stop"
if (-not $Clang) {
    $standardLlvm = "C:\Program Files\LLVM\bin\clang.exe"
    $Clang = if (Test-Path $standardLlvm) { $standardLlvm } else { "clang" }
}
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$cmake = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
$match = [regex]::Match(
    $cmake,
    'set\(AV2HEIF_CORE_SOURCES(?<sources>.*?)\)\s*\r?\n\r?\nadd_library',
    [System.Text.RegularExpressions.RegexOptions]::Singleline)
if (-not $match.Success) {
    throw "Could not read AV2HEIF_CORE_SOURCES from CMakeLists.txt"
}

$sources = [regex]::Matches($match.Groups['sources'].Value, '[^\s()]+\.c') |
    ForEach-Object { Join-Path $root $_.Value }
$sources += Join-Path $PSScriptRoot "decoder_wasm.c"
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$output = Join-Path $OutputDirectory "decoder.wasm"

$arguments = @(
    '--target=wasm32',
    '-std=c11',
    '-O3',
    '-Wall',
    '-Wextra',
    '-Wpedantic',
    '-Werror',
    '-Wno-tautological-constant-out-of-range-compare',
    '-ffreestanding',
    '-fno-builtin',
    '-fno-stack-protector',
    '-fno-unwind-tables',
    '-fno-asynchronous-unwind-tables',
    '-nostdinc',
    "-I$(Join-Path $root 'include')",
    "-I$(Join-Path $root 'src')",
    "-I$(Join-Path $root 'src\shared')",
    '-nostdlib',
    '-Wl,--no-entry',
    '-Wl,--export-memory',
    '-Wl,--initial-memory=16777216',
    '-Wl,--max-memory=536870912',
    '-Wl,--stack-first',
    '-Wl,-z,stack-size=8388608',
    '-Wl,--gc-sections',
    '-o',
    $output
)
$arguments += $sources

& $Clang @arguments
if ($LASTEXITCODE -ne 0) {
    throw "WebAssembly compilation failed with exit code $LASTEXITCODE"
}
Get-Item $output | Select-Object FullName, Length
