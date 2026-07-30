param(
    [string]$AccountingCensus = "",
    [string]$ArtifactsDirectory = "",
    [string]$DecoderExecutable = "",
    [string]$DiagnosticsDirectory = "",
    [string]$OutputPath = "",
    [switch]$RefreshCurrent
)

$ErrorActionPreference = "Stop"
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))

function Resolve-RepositoryPath([string]$Path, [string]$Default) {
    if (-not $Path) { $Path = $Default }
    if ([IO.Path]::IsPathRooted($Path)) {
        return [IO.Path]::GetFullPath($Path)
    }
    return [IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path))
}

function Get-TreeCount($Symbols, [string[]]$Functions, [string]$Tree) {
    return [long](($Symbols |
        Where-Object { $_.function -in $Functions } |
        ForEach-Object { $_.tree_counts.$Tree } |
        Measure-Object -Sum).Sum)
}

function Get-Count($Symbols, [string[]]$Functions) {
    return [long](($Symbols |
        Where-Object { $_.function -in $Functions } |
        Measure-Object count -Sum).Sum)
}

function Get-CurrentStop([string]$ArtifactPath, [string]$Name,
                         [string]$Decoder, [string]$ScratchDirectory) {
    $stderrPath = Join-Path $ScratchDirectory "$Name.stderr"
    $outputPath = Join-Path $ScratchDirectory "$Name.i420"
    Remove-Item -LiteralPath $stderrPath, $outputPath -Force `
        -ErrorAction SilentlyContinue
    $process = Start-Process -FilePath $Decoder `
        -ArgumentList @($ArtifactPath, "-o", $outputPath) `
        -WorkingDirectory $repositoryRoot -RedirectStandardError $stderrPath `
        -NoNewWindow -Wait -PassThru
    $lines = @(Get-Content -LiteralPath $stderrPath -ErrorAction SilentlyContinue)
    $stage = (($lines | Where-Object { $_ -match '^  stage:' } |
        Select-Object -First 1) -replace '^  stage: ', '')
    $leaf = (($lines | Where-Object { $_ -match '^  leaf:' } |
        Select-Object -First 1) -replace '^  leaf: ', '')
    $progress = (($lines | Where-Object { $_ -match '^  entropy tell:' } |
        Select-Object -First 1) -replace '^  entropy tell: ', '')
    $emitted = Test-Path -LiteralPath $outputPath
    Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue
    if ($process.ExitCode -eq 0) {
        return [PSCustomObject]@{ Stage = "complete"; Leaf = ""; Progress = "" }
    }
    if ($emitted) {
        throw "Failed decode emitted output for $Name"
    }
    if (-not $stage) {
        throw "Decoder failed without a structured stage for $Name"
    }
    return [PSCustomObject]@{ Stage = $stage; Leaf = $leaf; Progress = $progress }
}

function Get-RecordedStop([string]$Name, [string]$Directory) {
    $stderrPath = Join-Path $Directory "$Name-continued-final.stderr"
    if (-not (Test-Path -LiteralPath $stderrPath -PathType Leaf)) {
        throw "Verified diagnostic not found: $stderrPath"
    }
    $lines = @(Get-Content -LiteralPath $stderrPath)
    $stage = (($lines | Where-Object { $_ -match '^  stage:' } |
        Select-Object -First 1) -replace '^  stage: ', '')
    $leaf = (($lines | Where-Object { $_ -match '^  leaf:' } |
        Select-Object -First 1) -replace '^  leaf: ', '')
    $progress = (($lines | Where-Object { $_ -match '^  entropy tell:' } |
        Select-Object -First 1) -replace '^  entropy tell: ', '')
    if (-not $stage) {
        return [PSCustomObject]@{ Stage = "complete"; Leaf = ""; Progress = "" }
    }
    return [PSCustomObject]@{ Stage = $stage; Leaf = $leaf; Progress = $progress }
}

function Format-Exposure([long]$Count) {
    if ($Count -eq 0) { return "-" }
    return $Count.ToString([Globalization.CultureInfo]::InvariantCulture)
}

$AccountingCensus = Resolve-RepositoryPath $AccountingCensus `
    "build\census\all-artifacts-accounting.json"
$ArtifactsDirectory = Resolve-RepositoryPath $ArtifactsDirectory `
    "images\artifacts"
$DecoderExecutable = Resolve-RepositoryPath $DecoderExecutable `
    "build\windows-x86_64\av2heif-decoder.exe"
$DiagnosticsDirectory = Resolve-RepositoryPath $DiagnosticsDirectory "build"
$OutputPath = Resolve-RepositoryPath $OutputPath `
    "images\artifacts\blocker-map.md"

foreach ($required in @($AccountingCensus, $DecoderExecutable)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required file not found: $required"
    }
}
if (-not (Test-Path -LiteralPath $ArtifactsDirectory -PathType Container)) {
    throw "Artifacts directory not found: $ArtifactsDirectory"
}

$document = Get-Content -LiteralPath $AccountingCensus -Raw | ConvertFrom-Json
if ($document.schema -ne "av2-feature-census/v1") {
    throw "Unsupported census schema: $($document.schema)"
}
if ($document.avm_revision -ne "966a7d7cd6fcf60360caf5dc413b2aeeb65e144d") {
    throw "Unexpected AVM revision: $($document.avm_revision)"
}

$lumaModeFunctions = @(
    "read_intra_frame_mode_info", "read_intra_luma_mode", "read_fsc_mode",
    "read_mrl_index", "read_multi_line_mrl", "read_palette_mode_info",
    "read_palette_colors_y", "decode_color_map_tokens", "read_intrabc_info",
    "read_intrabc_drl_idx", "read_mv", "assign_dv", "av2_read_uniform",
    "read_truncated_unary_mvd", "read_tu_quasi_uniform"
)
$partitionFunctions = @("read_partition")
$prefixFunctions = @("av2_read_sig_txtype", "decode_eob", "read_skip_txfm")
$levelFunctions = @(
    "av2_read_coeffs_txb", "av2_read_coeffs_txb_skip",
    "read_coeffs_forward_2d", "read_coeffs_reverse",
    "read_coeffs_reverse_2d", "read_low_range", "read_truncated_rice",
    "read_exp_golomb"
)
$transformFunctions = @("av2_read_tx_type", "av2_read_sec_tx_type")
$chromaModeFunctions = @(
    "read_intra_uv_mode", "read_cfl_index", "read_cfl_mhccp_switch",
    "read_cfl_alphas", "read_mh_dir"
)
$chromaTransformFunctions = @("av2_read_cctx_type")
$filterFunctions = @("read_gdf", "read_cdef", "read_ccso")

$scratchDirectory = Join-Path $repositoryRoot "build\blocker-map"
[void][IO.Directory]::CreateDirectory($scratchDirectory)
$rows = @()
foreach ($file in $document.files) {
    $fileName = [IO.Path]::GetFileName($file.path)
    $name = [IO.Path]::GetFileNameWithoutExtension($file.path)
    $artifactPath = Join-Path $ArtifactsDirectory $fileName
    if (-not (Test-Path -LiteralPath $artifactPath -PathType Leaf)) {
        throw "Artifact not found: $artifactPath"
    }
    $symbols = @($file.accounting_symbols)
    if ($symbols.Count -eq 0) {
        throw "Accounting data missing for $name"
    }
    $current = if ($RefreshCurrent) {
        Get-CurrentStop $artifactPath $name $DecoderExecutable $scratchDirectory
    } else {
        Get-RecordedStop $name $DiagnosticsDirectory
    }
    $rows += [PSCustomObject]@{
        Name = $fileName
        Current = $current
        SharedPartition = Get-TreeCount $symbols $partitionFunctions "shared"
        LumaPartition = Get-TreeCount $symbols $partitionFunctions "luma"
        LumaMode = Get-TreeCount $symbols $lumaModeFunctions "luma"
        LumaPrefix = Get-TreeCount $symbols $prefixFunctions "luma"
        LumaLevels = Get-TreeCount $symbols $levelFunctions "luma"
        LumaTransform = Get-TreeCount $symbols $transformFunctions "luma"
        ChromaPartition = Get-TreeCount $symbols $partitionFunctions "chroma"
        ChromaMode = Get-TreeCount $symbols $chromaModeFunctions "chroma"
        ChromaPrefix = Get-TreeCount $symbols $prefixFunctions "chroma"
        ChromaLevels = Get-TreeCount $symbols $levelFunctions "chroma"
        ChromaTransform = Get-TreeCount $symbols $chromaTransformFunctions "chroma"
        Filter = Get-Count $symbols $filterFunctions
    }
}

$lines = [Collections.Generic.List[string]]::new()
$lines.Add("# Decoder blocker map")
$lines.Add("")
$lines.Add("Generated from the pinned AVM accounting census at revision " +
    "``966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`` and a live production " +
    "decoder sweep. It describes the complete syntax surfaces each artifact " +
    "will exercise, not a speculative sequence of future failures.")
$lines.Add("")
$lines.Add("## Reading the map")
$lines.Add("")
$lines.Add("- **Current stop** is executable evidence from the production decoder.")
$lines.Add("- Numeric cells are AVM entropy-event counts over the complete frame. " +
    "They prove that the artifact exercises that surface beyond the current " +
    "stop; they do not by themselves prove a missing implementation.")
$lines.Add("- ``-`` means the accounting oracle observed no event in that surface.")
$lines.Add("- Luma/chroma inverse prediction and transform reconstruction are not " +
    "entropy events. A reconstruction gap is confirmed only when production " +
    "or a pinned hosted walk reaches it.")
$lines.Add("")
$lines.Add("## Per-file surface exposure")
$lines.Add("")
$lines.Add("| File | Current stop | Shared partition | Luma partition | " +
    "Luma mode/parse | Luma prefix | Luma levels | Luma tx/IST syntax | " +
    "Chroma partition | Chroma mode/parse | Chroma prefix | Chroma levels | " +
    "Chroma CCTX | Filter controls |")
$lines.Add("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
foreach ($row in $rows) {
    $lines.Add("| ``$($row.Name)`` | $($row.Current.Stage) | " +
        "$(Format-Exposure $row.SharedPartition) | " +
        "$(Format-Exposure $row.LumaPartition) | " +
        "$(Format-Exposure $row.LumaMode) | " +
        "$(Format-Exposure $row.LumaPrefix) | " +
        "$(Format-Exposure $row.LumaLevels) | " +
        "$(Format-Exposure $row.LumaTransform) | " +
        "$(Format-Exposure $row.ChromaPartition) | " +
        "$(Format-Exposure $row.ChromaMode) | " +
        "$(Format-Exposure $row.ChromaPrefix) | " +
        "$(Format-Exposure $row.ChromaLevels) | " +
        "$(Format-Exposure $row.ChromaTransform) | " +
        "$(Format-Exposure $row.Filter) |")
}
$lines.Add("")
$lines.Add("## Confirmed current frontiers")
$lines.Add("")
$lines.Add("| File | Stage | Leaf | Progress |")
$lines.Add("|---|---|---|---|")
foreach ($row in $rows) {
    $leaf = if ($row.Current.Leaf) { $row.Current.Leaf } else { "-" }
    $progress = if ($row.Current.Progress) { $row.Current.Progress } else { "-" }
    $lines.Add("| ``$($row.Name)`` | $($row.Current.Stage) | $leaf | $progress |")
}
$lines.Add("")
$lines.Add("## What is actually still uncertain")
$lines.Add("")
$lines.Add("- **Secondary inverse transform:** syntax is visible here, but this " +
    "AVM revision's accounting value does not expose the packed secondary set " +
    "and type. Exact missing tuples remain reach-and-pin work.")
$lines.Add("- **Luma prediction:** all current corpus prediction frontiers clear, " +
    "but tuple admission is intentionally narrow; later oracle-observed " +
    "geometries are exposure, not proof of support.")
$lines.Add("- **Coefficient and parse stages:** complete-frame event counts show " +
    "where each file will return after earlier fixes. Adaptive CDF state means " +
    "a future failure cannot be ordered safely until preceding blocks decode " +
    "sample- and range-exactly.")
$lines.Add("- **Frame filtering:** controls show which tools are signaled. Only the " +
    "two current frame-filter stops are confirmed missing production paths; a " +
    "nonzero control count alone is not a blocker.")
$lines.Add("")
$lines.Add("Regenerate after building the accounting census and production decoder:")
$lines.Add("")
$lines.Add('```powershell')
$lines.Add("powershell -NoProfile -ExecutionPolicy Bypass -File ``")
$lines.Add("  tools/census/generate_blocker_map.ps1")
$lines.Add('```')
$lines.Add("")
$lines.Add("Pass ``-RefreshCurrent`` to rerun the production decoder instead " +
    "of consuming the verified ``build/*-continued-final.stderr`` sweep.")

[IO.File]::WriteAllLines($OutputPath, $lines, [Text.UTF8Encoding]::new($false))
Write-Output "Wrote $OutputPath"
