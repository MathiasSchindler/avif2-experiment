param(
    [string]$ToolchainRoot = "",
    [ValidateSet("x64", "arm64")]
    [string]$Architecture = "x64"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$vendorDir = Join-Path $root "vendor\avm"
$architectureConfig = @{
    x64 = @{
        BuildDirectory = "avm"
        CompilerPrefix = "x86_64-w64-mingw32"
        Toolchain = "x86_64-mingw-gcc.cmake"
        TargetCpu = "generic"
    }
    arm64 = @{
        BuildDirectory = "avm-arm64"
        CompilerPrefix = "aarch64-w64-mingw32"
        Toolchain = "arm64-mingw-gcc.cmake"
        TargetCpu = "arm64"
    }
}[$Architecture]
$buildDir = Join-Path $root "build\reference\$($architectureConfig.BuildDirectory)"
$revision = "966a7d7cd6fcf60360caf5dc413b2aeeb65e144d"
$sourceUrl = "https://github.com/AOMediaCodec/avm.git"

function Find-Program([string]$Name, [string[]]$Fallbacks) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($command) {
        return $command.Source
    }
    foreach ($fallback in $Fallbacks) {
        if (Test-Path $fallback) {
            return $fallback
        }
    }
    throw "Could not find $Name"
}

if (-not $ToolchainRoot) {
    $packageRoot = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
    $package = Get-ChildItem $packageRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object Name -Like "MartinStorsjo.LLVM-MinGW.UCRT_*" |
        Select-Object -First 1
    if ($package) {
        $ToolchainRoot = Get-ChildItem $package.FullName -Directory |
            Where-Object Name -Like "llvm-mingw-*-ucrt-*" |
            Select-Object -First 1 -ExpandProperty FullName
    }
}
if (-not $ToolchainRoot -or -not (Test-Path (Join-Path $ToolchainRoot (
        "bin\$($architectureConfig.CompilerPrefix)-gcc.exe")))) {
    throw "Could not find LLVM-MinGW UCRT; install MartinStorsjo.LLVM-MinGW.UCRT with winget"
}

$cmake = Find-Program "cmake.exe" @("C:\Program Files\CMake\bin\cmake.exe")
$ninja = Find-Program "ninja.exe" @(
    (Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe\ninja.exe")
)
$git = Find-Program "git.exe" @("C:\Program Files\Git\cmd\git.exe")
$perlDir = "C:\Program Files\Git\usr\bin"
if (-not (Test-Path (Join-Path $perlDir "perl.exe"))) {
    throw "Could not find Perl from Git for Windows"
}

if (-not (Test-Path (Join-Path $vendorDir ".git"))) {
    & $git clone --no-checkout $sourceUrl $vendorDir
    if ($LASTEXITCODE -ne 0) { throw "AVM clone failed" }
    & $git -C $vendorDir checkout --detach $revision
    if ($LASTEXITCODE -ne 0) { throw "AVM checkout failed" }
}

$actualRevision = (& $git -C $vendorDir rev-parse HEAD).Trim()
if ($actualRevision -ne $revision) {
    throw "vendor/avm is at $actualRevision, expected $revision"
}
if (& $git -C $vendorDir status --short) {
    throw "vendor/avm has local modifications"
}

$toolchainBin = Join-Path $ToolchainRoot "bin"
$env:PATH = "$toolchainBin;$perlDir;$env:PATH"
$toolchainFile = Join-Path $vendorDir (
    "cmake\toolchains\$($architectureConfig.Toolchain)")
$configureArguments = @(
    "-S", $vendorDir,
    "-B", $buildDir,
    "-G", "Ninja",
    "-DCMAKE_MAKE_PROGRAM=$ninja",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_EXE_LINKER_FLAGS=-static",
    "-DAVM_TARGET_CPU=$($architectureConfig.TargetCpu)",
    "-DENABLE_DOCS=OFF",
    "-DENABLE_EXAMPLES=ON",
    "-DENABLE_TESTS=OFF",
    "-DCONFIG_TENSORFLOW_LITE=0",
    "-DCONFIG_ML_PART_SPLIT=0",
    "-DCONFIG_DIP_EXT_PRUNING=0",
    "-DCONFIG_WEBM_IO=0"
)
& $cmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw "AVM configure failed" }

& $cmake --build $buildDir --target avmenc avmdec --parallel
if ($LASTEXITCODE -ne 0) { throw "AVM build failed" }

foreach ($name in "avmenc", "avmdec") {
    $binary = Join-Path $buildDir "$name.exe"
    & $binary --help | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "$name smoke test failed" }
}

Write-Output "AVM $Architecture reference tools are ready in $buildDir"