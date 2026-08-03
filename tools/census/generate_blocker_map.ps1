param(
    [string]$AccountingCensus = "",
    [string]$ArtifactsDirectory = "",
    [string]$OutputPath = ""
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

function Format-Exposure([long]$Count) {
    if ($Count -eq 0) { return "-" }
    return $Count.ToString([Globalization.CultureInfo]::InvariantCulture)
}

$AccountingCensus = Resolve-RepositoryPath $AccountingCensus `
    "build\census\all-artifacts-accounting.json"
$ArtifactsDirectory = Resolve-RepositoryPath $ArtifactsDirectory `
    "images\artifacts"
$OutputPath = Resolve-RepositoryPath $OutputPath `
    "images\artifacts\blocker-map.md"

if (-not (Test-Path -LiteralPath $AccountingCensus -PathType Leaf)) {
    throw "Required file not found: $AccountingCensus"
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
    $rows += [PSCustomObject]@{
        Name = $fileName
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
$lines.Add("# Artifact feature exposure map")
$lines.Add("")
$lines.Add("Generated from the pinned AVM accounting census at revision " +
    "``966a7d7cd6fcf60360caf5dc413b2aeeb65e144d``. It describes entropy " +
    "surfaces observed over each complete AVM-decoded frame. It is useful " +
    "for corpus selection and coverage planning; it is not a decoder status " +
    "or missing-feature report. See [``status.md``](status.md) for current " +
    "production results.")
$lines.Add("")
$lines.Add("## Reading the map")
$lines.Add("")
$lines.Add("- Numeric cells are AVM entropy-event counts over the complete frame. " +
    "They prove that the artifact exercises that surface; they do not by " +
    "themselves prove production support or a missing implementation.")
$lines.Add("- ``-`` means the accounting oracle observed no event in that surface.")
$lines.Add("- Luma/chroma inverse prediction and transform reconstruction are not " +
    "entropy events. A reconstruction gap is confirmed only when production " +
    "or a pinned hosted walk reaches it.")
$lines.Add("")
$lines.Add("## Per-file surface exposure")
$lines.Add("")
$lines.Add("| File | Shared partition | Luma partition | " +
    "Luma mode/parse | Luma prefix | Luma levels | Luma tx/IST syntax | " +
    "Chroma partition | Chroma mode/parse | Chroma prefix | Chroma levels | " +
    "Chroma CCTX | Filter controls |")
$lines.Add("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
foreach ($row in $rows) {
    $lines.Add("| ``$($row.Name)`` | " +
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
$lines.Add("## Interpretation limits")
$lines.Add("")
$lines.Add("- **Secondary inverse transform:** syntax is visible here, but this " +
    "AVM revision's accounting value does not expose the packed secondary set " +
    "and type.")
$lines.Add("- **Prediction and reconstruction:** inverse prediction and transform " +
    "reconstruction are not entropy events, so their exact mode/geometry " +
    "coverage needs separate sample-locked evidence.")
$lines.Add("- **Adaptive syntax:** event counts measure demand, not independent " +
    "reachability. CDF and reconstructed-reference dependencies still require " +
    "ordered differential validation.")
$lines.Add("- **Frame filtering:** control counts show signaled entropy syntax, " +
    "not which pixels a filter changes or whether output is sample-exact.")
$lines.Add("")
$lines.Add("Regenerate after building the accounting census:")
$lines.Add("")
$lines.Add('```powershell')
$lines.Add("powershell -NoProfile -ExecutionPolicy Bypass -File ``")
$lines.Add("  tools/census/generate_blocker_map.ps1")
$lines.Add('```')

[IO.File]::WriteAllLines($OutputPath, $lines, [Text.UTF8Encoding]::new($false))
Write-Output "Wrote $OutputPath"
