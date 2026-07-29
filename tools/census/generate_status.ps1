param(
    [string]$CensusExecutable = "",
    [string]$ArtifactsDirectory = "",
    [string]$CensusJson = "",
    [string]$OutputPath = "",
    [switch]$UseExistingCensus
)

$ErrorActionPreference = "Stop"
$invariantCulture = [System.Globalization.CultureInfo]::InvariantCulture
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))

function Resolve-RepositoryPath([string]$Path, [string]$Default) {
    if (-not $Path) {
        $Path = $Default
    }
    if ([IO.Path]::IsPathRooted($Path)) {
        return [IO.Path]::GetFullPath($Path)
    }
    return [IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path))
}

function Format-Decimal([double]$Value) {
    return $Value.ToString("0.0", $invariantCulture)
}

function Format-Integer([long]$Value) {
    return $Value.ToString("N0", $invariantCulture)
}

function New-StringSet {
    return New-Object "System.Collections.Generic.HashSet[string]"
}

function Get-FeatureAtoms($Feature) {
    $atoms = @(
        "block:$($Feature.block_size.name)",
        "partition:$($Feature.partition)",
        "tree:$($Feature.tree_type)",
        "mode:$($Feature.mode.name)",
        "uv:$($Feature.uv_mode.name)",
        "angle:$($Feature.angle_delta)",
        "uv-angle:$($Feature.uv_angle_delta)",
        "mrl:$($Feature.mrl_index)",
        "multi-mrl:$($Feature.multi_line_mrl)",
        "fsc-y:$($Feature.fsc_y)",
        "fsc-uv:$($Feature.fsc_uv)",
        "ibp:$($Feature.enable_ibp)",
        "intrabc:$($Feature.use_intrabc)",
        "intrabc-mode:$($Feature.intrabc_mode)",
        "skip:$($Feature.skip_transform)",
        "tx-size:$($Feature.tx_size.name)",
        "primary-tx:$($Feature.primary_tx_type.name)",
        "secondary-tx:$($Feature.secondary_tx_type)",
        "secondary-set:$($Feature.secondary_tx_set)",
        "y-mode-index:$($Feature.y_mode_index)",
        "joint-mode:$($Feature.joint_mode)",
        "palette-y:$($Feature.palette_y)",
        "palette-uv:$($Feature.palette_uv)"
    )
    if ($Feature.uv_mode.name -eq "CFL") {
        $atoms += @(
            "cfl-index:$($Feature.cfl_index)",
            "cfl-alpha-index:$($Feature.cfl_alpha_index)",
            "cfl-signs:$($Feature.cfl_alpha_signs)"
        )
    }
    return $atoms
}

function Add-DemandStat([hashtable]$Stats, [string]$Name, [long]$Count,
                        [string]$File) {
    if (-not $Stats.ContainsKey($Name)) {
        $Stats[$Name] = [PSCustomObject]@{
            Feature = $Name
            Occurrences = [long]0
            Files = New-StringSet
        }
    }
    $Stats[$Name].Occurrences += $Count
    [void]$Stats[$Name].Files.Add($File)
}

$CensusExecutable = Resolve-RepositoryPath $CensusExecutable `
    "build\census\av2-census.exe"
$ArtifactsDirectory = Resolve-RepositoryPath $ArtifactsDirectory `
    "images\artifacts"
$CensusJson = Resolve-RepositoryPath $CensusJson `
    "build\census\all-artifacts-census.json"
$OutputPath = Resolve-RepositoryPath $OutputPath "tools\census\status.md"

if (-not $UseExistingCensus) {
    if (-not (Test-Path -LiteralPath $CensusExecutable -PathType Leaf)) {
        throw "Census executable not found: $CensusExecutable"
    }
    if (-not (Test-Path -LiteralPath $ArtifactsDirectory -PathType Container)) {
        throw "Artifact directory not found: $ArtifactsDirectory"
    }
    $artifactInputs = @(
        Get-ChildItem -LiteralPath $ArtifactsDirectory -Filter "*.obu" -File |
            Sort-Object Name |
            ForEach-Object {
                "images/artifacts/$($_.Name)"
            }
    )
    if ($artifactInputs.Count -eq 0) {
        throw "No .obu artifacts found in $ArtifactsDirectory"
    }
    $censusDirectory = Split-Path -Parent $CensusJson
    [void][IO.Directory]::CreateDirectory($censusDirectory)
    $temporaryStem = [IO.Path]::GetRandomFileName()
    $temporaryCensusJson = Join-Path $censusDirectory "$temporaryStem.json"
    $diagnosticsPath = Join-Path $censusDirectory "$temporaryStem.stderr"
    try {
        $arguments = @("-o", "`"$temporaryCensusJson`"") +
            @($artifactInputs | ForEach-Object { "`"$_`"" })
        $process = Start-Process -FilePath $CensusExecutable `
            -ArgumentList $arguments -WorkingDirectory $repositoryRoot `
            -RedirectStandardError $diagnosticsPath -NoNewWindow -Wait `
            -PassThru
        if ($process.ExitCode -ne 0) {
            $diagnostics = Get-Content -LiteralPath $diagnosticsPath -Tail 20
            throw "Census failed with exit code $($process.ExitCode)`n$($diagnostics -join "`n")"
        }
        if (-not (Test-Path -LiteralPath $temporaryCensusJson -PathType Leaf) -or
            (Get-Item -LiteralPath $temporaryCensusJson).Length -eq 0) {
            throw "Census completed without producing JSON"
        }
        Move-Item -LiteralPath $temporaryCensusJson -Destination $CensusJson `
            -Force
    } finally {
        Remove-Item -LiteralPath $temporaryCensusJson -Force `
            -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $diagnosticsPath -Force `
            -ErrorAction SilentlyContinue
    }
} elseif (-not (Test-Path -LiteralPath $CensusJson -PathType Leaf)) {
    throw "Existing census not found: $CensusJson"
}

$document = Get-Content -LiteralPath $CensusJson -Raw | ConvertFrom-Json
if ($document.schema -ne "av2-feature-census/v1") {
    throw "Unsupported census schema: $($document.schema)"
}
if (@($document.files).Count -eq 0) {
    throw "The census contains no files"
}

$featureSets = @{}
$documentFrequency = @{}
foreach ($file in $document.files) {
    $set = New-StringSet
    foreach ($feature in $file.features) {
        foreach ($atom in Get-FeatureAtoms $feature) {
            [void]$set.Add($atom)
        }
    }
    $featureSets[$file.path] = $set
    foreach ($atom in $set) {
        if ($documentFrequency.ContainsKey($atom)) {
            ++$documentFrequency[$atom]
        } else {
            $documentFrequency[$atom] = 1
        }
    }
}

$rawFileScores = foreach ($file in $document.files) {
    $rarityWeight = 0.0
    $exclusiveValues = 0
    foreach ($atom in $featureSets[$file.path]) {
        $rarityWeight += 1.0 / $documentFrequency[$atom]
        if ($documentFrequency[$atom] -eq 1) {
            ++$exclusiveValues
        }
    }
    [PSCustomObject]@{
        File = [IO.Path]::GetFileName($file.path)
        RarityWeight = $rarityWeight
        DistinctValues = $featureSets[$file.path].Count
        ExclusiveValues = $exclusiveValues
    }
}
$maximumRarityWeight = ($rawFileScores | Measure-Object RarityWeight -Maximum).Maximum
$fileScores = @(
    $rawFileScores |
        ForEach-Object {
            [PSCustomObject]@{
                File = $_.File
                Score = 100.0 * $_.RarityWeight / $maximumRarityWeight
                DistinctValues = $_.DistinctValues
                ExclusiveValues = $_.ExclusiveValues
            }
        } |
        Sort-Object -Property `
            @{ Expression = { $_.Score }; Descending = $true }, `
            @{ Expression = { $_.File }; Ascending = $true }
)

$categories = @(
    [PSCustomObject]@{ Name = "Non-DCT primary transform"; Test = {
        param($Feature) $Feature.primary_tx_type.name -ne "DCT_DCT"
    } },
    [PSCustomObject]@{ Name = "MRL"; Test = {
        param($Feature) $Feature.mrl_index -gt 0
    } },
    [PSCustomObject]@{ Name = "Multi-line MRL"; Test = {
        param($Feature) $Feature.multi_line_mrl -gt 0
    } },
    [PSCustomObject]@{ Name = "CfL"; Test = {
        param($Feature) $Feature.uv_mode.name -eq "CFL"
    } },
    [PSCustomObject]@{ Name = "Luma FSC"; Test = {
        param($Feature) $Feature.fsc_y -gt 0
    } },
    [PSCustomObject]@{ Name = "IntraBC"; Test = {
        param($Feature) $Feature.use_intrabc -gt 0
    } },
    [PSCustomObject]@{ Name = "Transform skip"; Test = {
        param($Feature) $Feature.skip_transform -gt 0
    } },
    [PSCustomObject]@{ Name = "Palette"; Test = {
        param($Feature) $Feature.palette_y -gt 0 -or $Feature.palette_uv -gt 0
    } },
    [PSCustomObject]@{ Name = "Secondary transform"; Test = {
        param($Feature) $Feature.secondary_tx_type -gt 0
    } }
)
$categoryDemand = foreach ($category in $categories) {
    $fileCount = 0
    $occurrences = [long]0
    foreach ($file in $document.files) {
        $fileOccurrences = [long]0
        foreach ($feature in $file.features) {
            if (& $category.Test $feature) {
                $fileOccurrences += [long]$feature.count
            }
        }
        if ($fileOccurrences -gt 0) {
            ++$fileCount
            $occurrences += $fileOccurrences
        }
    }
    [PSCustomObject]@{
        Feature = $category.Name
        Files = $fileCount
        Coverage = 100.0 * $fileCount / $document.files.Count
        Occurrences = $occurrences
    }
}

$demandStats = @{}
foreach ($file in $document.files) {
    foreach ($feature in $file.features) {
        $count = [long]$feature.count
        if ($feature.mrl_index -gt 0) {
            Add-DemandStat $demandStats "MRL index $($feature.mrl_index)" `
                $count $file.path
        }
        if ($feature.multi_line_mrl -gt 0) {
            Add-DemandStat $demandStats `
                "Multi-line MRL $($feature.multi_line_mrl)" $count $file.path
        }
        if ($feature.angle_delta -ne 0) {
            Add-DemandStat $demandStats `
                "Luma angle delta $($feature.angle_delta)" $count $file.path
        }
        if ($feature.uv_angle_delta -ne 0) {
            Add-DemandStat $demandStats `
                "Chroma angle delta $($feature.uv_angle_delta)" $count $file.path
        }
        if ($feature.fsc_y -gt 0) {
            Add-DemandStat $demandStats "Luma FSC $($feature.fsc_y)" `
                $count $file.path
        }
        if ($feature.fsc_uv -gt 0) {
            Add-DemandStat $demandStats "Chroma FSC $($feature.fsc_uv)" `
                $count $file.path
        }
        if ($feature.secondary_tx_type -gt 0) {
            Add-DemandStat $demandStats `
                "Secondary transform $($feature.secondary_tx_type)" `
                $count $file.path
        }
        if ($feature.skip_transform -gt 0) {
            Add-DemandStat $demandStats "Transform skip" $count $file.path
        }
        if ($feature.uv_mode.name -eq "CFL") {
            Add-DemandStat $demandStats "CfL prediction" $count $file.path
        }
        if ($feature.palette_y -gt 0 -or $feature.palette_uv -gt 0) {
            Add-DemandStat $demandStats "Palette prediction" $count $file.path
        }
        if ($feature.use_intrabc -gt 0) {
            Add-DemandStat $demandStats "Intra block copy" $count $file.path
        }
        if ($feature.primary_tx_type.name -ne "DCT_DCT") {
            Add-DemandStat $demandStats `
                "Primary tx $($feature.primary_tx_type.name)" $count $file.path
        }
    }
}
$wantedFeatures = @(
    @(
        foreach ($stat in $demandStats.Values) {
            $coverage = $stat.Files.Count / $document.files.Count
            $frequencySaturation = `
                $stat.Occurrences / ($stat.Occurrences + 100.0)
            [PSCustomObject]@{
                Feature = $stat.Feature
                Files = $stat.Files.Count
                Coverage = 100.0 * $coverage
                Occurrences = $stat.Occurrences
                DemandScore = 80.0 * $coverage + 20.0 * $frequencySaturation
            }
        }
    ) | Sort-Object -Property `
        @{ Expression = { $_.DemandScore }; Descending = $true }, `
        @{ Expression = { $_.Feature }; Ascending = $true } |
        Select-Object -First 20
)

$allAtoms = New-StringSet
foreach ($set in $featureSets.Values) {
    $allAtoms.UnionWith($set)
}
$uncoveredAtoms = New-StringSet
$uncoveredAtoms.UnionWith($allAtoms)
$selectedFiles = New-StringSet
$setCover = @()
while ($uncoveredAtoms.Count -gt 0) {
    $bestPath = $null
    $bestGain = -1
    foreach ($path in @($featureSets.Keys | Sort-Object)) {
        if ($selectedFiles.Contains($path)) {
            continue
        }
        $gain = 0
        foreach ($atom in $featureSets[$path]) {
            if ($uncoveredAtoms.Contains($atom)) {
                ++$gain
            }
        }
        if ($gain -gt $bestGain) {
            $bestGain = $gain
            $bestPath = $path
        }
    }
    if ($bestGain -le 0) {
        throw "Set-cover calculation could not cover all feature values"
    }
    [void]$selectedFiles.Add($bestPath)
    foreach ($atom in $featureSets[$bestPath]) {
        [void]$uncoveredAtoms.Remove($atom)
    }
    $setCover += [PSCustomObject]@{
        Step = $setCover.Count + 1
        File = [IO.Path]::GetFileName($bestPath)
        NewValues = $bestGain
        Coverage = 100.0 * ($allAtoms.Count - $uncoveredAtoms.Count) / `
            $allAtoms.Count
    }
}

$rareFixtures = foreach ($file in $document.files) {
    $palette = [long]0
    $transformSkip = [long]0
    foreach ($feature in $file.features) {
        if ($feature.palette_y -gt 0 -or $feature.palette_uv -gt 0) {
            $palette += [long]$feature.count
        }
        if ($feature.skip_transform -gt 0) {
            $transformSkip += [long]$feature.count
        }
    }
    if ($palette -gt 0 -or $transformSkip -gt 0) {
        [PSCustomObject]@{
            File = [IO.Path]::GetFileName($file.path)
            Palette = $palette
            TransformSkip = $transformSkip
        }
    }
}

$totalFrames = ($document.files | Measure-Object frames -Sum).Sum
$totalBlocks = ($document.files | Measure-Object coding_block_count -Sum).Sum
$totalOccurrences = `
    ($document.files | Measure-Object feature_occurrence_count -Sum).Sum
$totalPerFileTuples = `
    ($document.files | Measure-Object unique_feature_count -Sum).Sum

$lines = New-Object "System.Collections.Generic.List[string]"
$lines.Add("# AV2 feature census status")
$lines.Add("")
$lines.Add("Generated by ``tools/census/generate_status.ps1`` from the pinned " +
    "AVM-backed census. The report is deterministic and contains no timestamp.")
$lines.Add("")
$lines.Add("Regenerate it from all artifact OBUs:")
$lines.Add("")
$lines.Add("``````powershell")
$lines.Add("powershell -NoProfile -ExecutionPolicy Bypass -File " +
    "tools/census/generate_status.ps1")
$lines.Add("``````")
$lines.Add("")
$lines.Add("Use ``-UseExistingCensus`` to rebuild only this Markdown file from " +
    "``build/census/all-artifacts-census.json``.")
$lines.Add("")
$lines.Add("## Corpus summary")
$lines.Add("")
$lines.Add("- Schema: ``$($document.schema)``")
$lines.Add("- AVM revision: ``$($document.avm_revision)``")
$lines.Add("- Files/frames: $(Format-Integer $document.files.Count) / " +
    "$(Format-Integer $totalFrames)")
$lines.Add("- Coding blocks: $(Format-Integer $totalBlocks)")
$lines.Add("- Feature occurrences: $(Format-Integer $totalOccurrences)")
$lines.Add("- Sum of per-file unique tuples: " +
    "$(Format-Integer $totalPerFileTuples)")
$lines.Add("- Distinct atomic feature values: $(Format-Integer $allAtoms.Count)")
$lines.Add("")
$lines.Add("## File value score")
$lines.Add("")
$lines.Add("The score rewards distinct implementation-relevant values and gives " +
    "more weight to values found in fewer files. Q-index and occurrence volume " +
    "are excluded so large images and unique quantizers do not dominate:")
$lines.Add("")
$lines.Add('$$')
$lines.Add('S(f)=100\frac{\sum_{a\in f}1/\operatorname{files}(a)}' +
    '{\max_g\sum_{a\in g}1/\operatorname{files}(a)}')
$lines.Add('$$')
$lines.Add("")
$lines.Add("This is a regression and implementation-value score, not a decoding-" +
    "difficulty score.")
$lines.Add("")
$lines.Add("| Rank | File | Score | Distinct values | Exclusive values |")
$lines.Add("|---:|---|---:|---:|---:|")
for ($index = 0; $index -lt $fileScores.Count; ++$index) {
    $score = $fileScores[$index]
    $lines.Add("| $($index + 1) | ``$($score.File)`` | " +
        "$(Format-Decimal $score.Score) | $($score.DistinctValues) | " +
        "$($score.ExclusiveValues) |")
}
$lines.Add("")
$lines.Add("## Feature demand")
$lines.Add("")
$lines.Add("Coverage counts files containing at least one occurrence. Occurrences " +
    "sum all matching feature-tuple counts.")
$lines.Add("")
$lines.Add("| Feature | Files | Coverage | Occurrences |")
$lines.Add("|---|---:|---:|---:|")
foreach ($demand in $categoryDemand) {
    $lines.Add("| $($demand.Feature) | $($demand.Files) | " +
        "$(Format-Decimal $demand.Coverage)% | " +
        "$(Format-Integer $demand.Occurrences) |")
}
$lines.Add("")
$lines.Add("## Most wanted feature values")
$lines.Add("")
$lines.Add("Demand score weights file breadth at 80% and a saturating occurrence " +
    "term at 20%:")
$lines.Add("")
$lines.Add('$$')
$lines.Add('D(a)=80\frac{F_a}{F}+20\frac{N_a}{N_a+100}')
$lines.Add('$$')
$lines.Add("")
$lines.Add("| Rank | Feature value | Score | Files | Coverage | Occurrences |")
$lines.Add("|---:|---|---:|---:|---:|---:|")
for ($index = 0; $index -lt $wantedFeatures.Count; ++$index) {
    $feature = $wantedFeatures[$index]
    $lines.Add("| $($index + 1) | $($feature.Feature) | " +
        "$(Format-Decimal $feature.DemandScore) | $($feature.Files) | " +
        "$(Format-Decimal $feature.Coverage)% | " +
        "$(Format-Integer $feature.Occurrences) |")
}
$lines.Add("")
$lines.Add("## Compact regression set")
$lines.Add("")
$lines.Add("A deterministic greedy set cover selects the file adding the most " +
    "previously uncovered atomic feature values at each step.")
$lines.Add("")
$lines.Add("| Step | File | New values | Cumulative coverage |")
$lines.Add("|---:|---|---:|---:|")
foreach ($step in $setCover) {
    $lines.Add("| $($step.Step) | ``$($step.File)`` | $($step.NewValues) | " +
        "$(Format-Decimal $step.Coverage)% |")
}
$lines.Add("")
$lines.Add("## Rare-feature fixtures")
$lines.Add("")
$lines.Add("| File | Palette occurrences | Transform-skip occurrences |")
$lines.Add("|---|---:|---:|")
foreach ($fixture in $rareFixtures) {
    $lines.Add("| ``$($fixture.File)`` | $($fixture.Palette) | " +
        "$($fixture.TransformSkip) |")
}
$lines.Add("")
$lines.Add("## Interpretation and limitations")
$lines.Add("")
$lines.Add("- Non-DCT transforms, directional deltas, MRL, multi-line MRL, CfL, " +
    "FSC, and IntraBC have the broadest corpus demand.")
$lines.Add("- Palette appears only in ``silene-384x307-qp080.obu``. Transform " +
    "skip appears only 14 times across seven files.")
$lines.Add("- No corpus file uses a nonzero secondary-transform type. A dedicated " +
    "fixture is required before IST implementation coverage can be measured.")
$lines.Add("- ``enable_ibp`` records sequence capability. AVM applies IBP " +
    "deterministically to eligible transform sizes; it is not a separately " +
    "signaled per-block usage flag.")
$lines.Add("- The score measures observed corpus value, not which features the " +
    "experimental decoder still lacks. A missing-feature priority should multiply " +
    "demand by support status and blocker severity.")
$lines.Add("- AVM was built with ``CONFIG_ACCOUNTING=0``. Entropy symbols, CDF " +
    "contexts, EOB values, and coefficient levels are outside this census.")

[void][IO.Directory]::CreateDirectory((Split-Path -Parent $OutputPath))
$utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($OutputPath, ($lines -join "`n") + "`n", $utf8WithoutBom)
Write-Output "Wrote $OutputPath"