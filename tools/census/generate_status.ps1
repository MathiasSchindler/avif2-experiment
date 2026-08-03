param(
    [string]$CensusExecutable = "",
    [string]$ArtifactsDirectory = "",
    [string]$CensusJson = "",
    [string]$SupportManifest = "",
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

function Get-NamedFeatureValues($Feature) {
    $values = @()
    if ($Feature.mrl_index -gt 0) {
        $values += "MRL index $($Feature.mrl_index)"
    }
    if ($Feature.multi_line_mrl -gt 0) {
        $values += "Multi-line MRL $($Feature.multi_line_mrl)"
    }
    if ($Feature.angle_delta -ne 0) {
        $values += "Luma angle delta $($Feature.angle_delta)"
    }
    if ($Feature.uv_angle_delta -ne 0) {
        $values += "Chroma angle delta $($Feature.uv_angle_delta)"
    }
    if ($Feature.fsc_y -gt 0) {
        $values += "Luma FSC $($Feature.fsc_y)"
    }
    if ($Feature.fsc_uv -gt 0) {
        $values += "Chroma FSC $($Feature.fsc_uv)"
    }
    if ($Feature.secondary_tx_type -gt 0) {
        $values += "Secondary transform $($Feature.secondary_tx_type)"
    }
    if ($Feature.skip_transform -gt 0) {
        $values += "Transform skip"
    }
    if ($Feature.uv_mode.name -eq "CFL") {
        $values += "CfL prediction"
    }
    if ($Feature.palette_y -gt 0 -or $Feature.palette_uv -gt 0) {
        $values += "Palette prediction"
    }
    if ($Feature.use_intrabc -gt 0) {
        $values += "Intra block copy"
    }
    if ($Feature.primary_tx_type.name -ne "DCT_DCT") {
        $values += "Primary tx $($Feature.primary_tx_type.name)"
    }
    return $values
}

function Assert-SupportRecord($Record, [string]$Description) {
    $validStatuses = @("supported", "partial", "unsupported", "unknown")
    if ($Record.status -notin $validStatuses) {
        throw "Invalid support status '$($Record.status)' for $Description"
    }
    if (-not $Record.evidence) {
        throw "Missing support evidence for $Description"
    }
}

function Get-SupportRecord([string]$Feature, $Rules) {
    $exact = @($Rules | Where-Object {
        $_.match -eq "exact" -and $_.value -eq $Feature
    })
    if ($exact.Count -gt 1) {
        throw "Multiple exact support rules match '$Feature'"
    }
    if ($exact.Count -eq 1) {
        return $exact[0]
    }
    $prefix = @($Rules | Where-Object {
        $_.match -eq "prefix" -and $Feature.StartsWith($_.value)
    })
    if ($prefix.Count -ne 1) {
        throw "Expected one support rule for '$Feature', found $($prefix.Count)"
    }
    return $prefix[0]
}

function Format-SupportStatus([string]$Status) {
    return $invariantCulture.TextInfo.ToTitleCase($Status)
}

$CensusExecutable = Resolve-RepositoryPath $CensusExecutable `
    "build\census\av2-census.exe"
$ArtifactsDirectory = Resolve-RepositoryPath $ArtifactsDirectory `
    "images\artifacts"
$CensusJson = Resolve-RepositoryPath $CensusJson `
    "build\census\all-artifacts-census.json"
$SupportManifest = Resolve-RepositoryPath $SupportManifest `
    "tools\census\support.json"
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

if (-not (Test-Path -LiteralPath $SupportManifest -PathType Leaf)) {
    throw "Support manifest not found: $SupportManifest"
}
$support = Get-Content -LiteralPath $SupportManifest -Raw | ConvertFrom-Json
if ($support.schema -ne "av2-decoder-feature-support/v1") {
    throw "Unsupported support manifest schema: $($support.schema)"
}
foreach ($family in $support.families) {
    Assert-SupportRecord $family "feature family '$($family.feature)'"
}
foreach ($rule in $support.rules) {
    if ($rule.match -notin @("exact", "prefix") -or -not $rule.value) {
        throw "Invalid support rule match '$($rule.match)' value '$($rule.value)'"
    }
    Assert-SupportRecord $rule "support rule '$($rule.value)'"
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
    [PSCustomObject]@{ Name = "Luma directional angle"; Test = {
        param($Feature) $Feature.angle_delta -ne 0
    } },
    [PSCustomObject]@{ Name = "Chroma directional angle"; Test = {
        param($Feature) $Feature.uv_angle_delta -ne 0
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
    [PSCustomObject]@{ Name = "Chroma FSC"; Test = {
        param($Feature) $Feature.fsc_uv -gt 0
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
        foreach ($name in @(Get-NamedFeatureValues $feature)) {
            Add-DemandStat $demandStats $name $count $file.path
        }
    }
}
$rankedDemand = @(
    foreach ($stat in $demandStats.Values) {
        $coverage = $stat.Files.Count / $document.files.Count
        $frequencySaturation = `
            $stat.Occurrences / ($stat.Occurrences + 100.0)
        $supportRecord = Get-SupportRecord $stat.Feature $support.rules
        [PSCustomObject]@{
            Feature = $stat.Feature
            Files = $stat.Files.Count
            Coverage = 100.0 * $coverage
            Occurrences = $stat.Occurrences
            DemandScore = 80.0 * $coverage + 20.0 * $frequencySaturation
            Status = $supportRecord.status
            Evidence = $supportRecord.evidence
        }
    }
)
$rankedDemand = @($rankedDemand | Sort-Object -Property `
        @{ Expression = { $_.DemandScore }; Descending = $true }, `
        @{ Expression = { $_.Feature }; Ascending = $true })
$wantedFeatures = @($rankedDemand | Select-Object -First 20)
$wantedNotFullySupported = @($rankedDemand | Where-Object {
    $_.Status -ne "supported"
} | Select-Object -First 20)

$artifactMissingFeatures = @(
    foreach ($file in $document.files) {
        $observed = New-StringSet
        foreach ($feature in $file.features) {
            foreach ($name in @(Get-NamedFeatureValues $feature)) {
                [void]$observed.Add($name)
            }
        }
        $missing = @(
            foreach ($name in $observed) {
                $record = Get-SupportRecord $name $support.rules
                if ($record.status -ne "supported") {
                    [PSCustomObject]@{
                        Name = $name
                        Status = Format-SupportStatus $record.status
                    }
                }
            }
        ) | Sort-Object -Property Name
        $missing = @($missing)
        [PSCustomObject]@{
            File = [IO.Path]::GetFileName($file.path)
            MissingCount = $missing.Count
            Missing = $missing
        }
    }
) | Sort-Object -Property `
    @{ Expression = { $_.MissingCount }; Ascending = $true }, `
    @{ Expression = { $_.File }; Ascending = $true }

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
    "AVM-backed census and reviewed ``tools/census/support.json`` manifest. " +
    "The report is deterministic and contains no timestamp.")
$lines.Add("")
$lines.Add("Support classifications describe validated feature paths, not " +
    "production acceptance of every surrounding coding tuple. Current " +
    "end-to-end build results and fail-closed frontiers are tracked in " +
    "[``images/artifacts/status.md``](../../images/artifacts/status.md).")
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
$lines.Add("## Historical production sweep")
$lines.Add("")
$lines.Add("These groups come from the 2026-07-30 production decoder sweep. " +
    "They name the surface where local decoding stops, not necessarily the " +
    "first entropy divergence. See ``images/artifacts/status.md`` for exact " +
    "endpoints and AVM evidence.")
$lines.Add("")
$lines.Add("| Stopping surface at sweep | Files |")
$lines.Add("|---|---|")
$lines.Add("| Luma coefficient prefix | ``flycatcher-397x265-qp048.obu`` |")
$lines.Add("| Luma parse | ``silene-384x307-qp080.obu`` |")
$lines.Add("| Luma inverse transform | ``chalk-320x240-qp128.obu``, ``chimp-240x287-qp192.obu``, ``cuckoo-157x105-qp160.obu``, ``cuckoo-431x287-qp080.obu``, ``emerald-480x240-qp128.obu``, ``flycatcher-223x149-qp176.obu``, ``laarim-256x171-qp096.obu``, ``laarim-448x299-qp192.obu``, ``louis-301x222-qp144.obu``, ``pastell-320x213-qp064.obu``, ``pastell-512x341-qp160.obu``, ``schwaebisch-417x277-qp208.obu``, ``silene-479x383-qp176.obu`` |")
$lines.Add("| Chroma parse | ``louis-463x342-qp224.obu``, ``schwaebisch-353x234-qp112.obu`` |")
$lines.Add("| Frame filter | ``chimp-93x111-qp064.obu``, ``chinnathurai-128x128-qp112.obu`` |")
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
$lines.Add("## Decoder feature support matrix")
$lines.Add("")
$lines.Add("Support state comes from the reviewed ``tools/census/support.json`` " +
    "manifest; it is not inferred from census frequency or decoder return " +
    "statements. ``Supported`` means the feature path itself is validated, " +
    "not that every surrounding coding tuple is accepted. ``Partial`` means " +
    "implemented paths and known coverage gaps coexist. ``Unsupported`` means " +
    "positive use is explicitly rejected. ``Unknown`` means the path has not " +
    "been audited sufficiently.")
$lines.Add("")
$lines.Add("| Feature family | Status | Files | Coverage | Occurrences | Evidence |")
$lines.Add("|---|---|---:|---:|---:|---|")
foreach ($family in $support.families) {
    $demand = @($categoryDemand | Where-Object {
        $_.Feature -eq $family.feature
    })
    if ($demand.Count -ne 1) {
        throw "Expected one demand category for support family '$($family.feature)'"
    }
    $lines.Add("| $($family.feature) | " +
        "$(Format-SupportStatus $family.status) | $($demand[0].Files) | " +
        "$(Format-Decimal $demand[0].Coverage)% | " +
        "$(Format-Integer $demand[0].Occurrences) | $($family.evidence) |")
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
$lines.Add("| Rank | Feature value | Status | Score | Files | Coverage | Occurrences |")
$lines.Add("|---:|---|---|---:|---:|---:|---:|")
for ($index = 0; $index -lt $wantedFeatures.Count; ++$index) {
    $feature = $wantedFeatures[$index]
    $lines.Add("| $($index + 1) | $($feature.Feature) | " +
        "$(Format-SupportStatus $feature.Status) | " +
        "$(Format-Decimal $feature.DemandScore) | $($feature.Files) | " +
        "$(Format-Decimal $feature.Coverage)% | " +
        "$(Format-Integer $feature.Occurrences) |")
}
$lines.Add("")
$lines.Add("## Most wanted not fully supported feature values")
$lines.Add("")
$lines.Add("This is the demand ranking above with values classified as " +
    "``supported`` removed. It includes ``partial``, ``unsupported``, and " +
    "``unknown`` values; status is not converted into an arbitrary numeric " +
    "multiplier.")
$lines.Add("")
$lines.Add("| Rank | Feature value | Status | Demand score | Files | Coverage | Occurrences |")
$lines.Add("|---:|---|---|---:|---:|---:|---:|")
for ($index = 0; $index -lt $wantedNotFullySupported.Count; ++$index) {
    $feature = $wantedNotFullySupported[$index]
    $lines.Add("| $($index + 1) | $($feature.Feature) | " +
        "$(Format-SupportStatus $feature.Status) | " +
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
$lines.Add("## Artifact missing-feature breadth")
$lines.Add("")
$lines.Add("A distinct missing feature is an observed named feature value whose " +
    "reviewed manifest status is not ``Supported``. Files are sorted by this " +
    "count ascending as a low-hanging-fruit heuristic. The count does not " +
    "include coefficient-profile, entropy-context, or traversal blockers, so " +
    "it is not an estimate of the work required for complete decoding. A file " +
    "with zero missing features is a feature-complete candidate at census " +
    "granularity, not a guaranteed successful decode.")
$lines.Add("")
$lines.Add("| Rank | File | Distinct missing features | Missing feature values |")
$lines.Add("|---:|---|---:|---|")
for ($index = 0; $index -lt $artifactMissingFeatures.Count; ++$index) {
    $artifact = $artifactMissingFeatures[$index]
    $missingText = if ($artifact.MissingCount -eq 0) {
        "None"
    } else {
        @($artifact.Missing | ForEach-Object {
            "$($_.Name) ($($_.Status))"
        }) -join "<br>"
    }
    $lines.Add("| $($index + 1) | ``$($artifact.File)`` | " +
        "$($artifact.MissingCount) | $missingText |")
}
$lines.Add("")
$featureCompleteFiles = @($artifactMissingFeatures | Where-Object {
    $_.MissingCount -eq 0
} | ForEach-Object {
    "``$($_.File)``"
}) -join ", "
$lines.Add("Feature-complete candidates at census granularity: " +
    $(if ($featureCompleteFiles) { $featureCompleteFiles } else { "None" }) +
    ". See ``images/artifacts/status.md`` for verified end-to-end results.")
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
$lines.Add("- The census schema reports no nonzero secondary-transform " +
    "occurrence, while production parsing exposes seven live IST tuples. " +
    "The focused reconstruction matrix and artifact walks provide the " +
    "current IST evidence.")
$lines.Add("- ``enable_ibp`` records sequence capability. AVM applies IBP " +
    "deterministically to eligible transform sizes; it is not a separately " +
    "signaled per-block usage flag.")
$lines.Add("- Support classifications are a reviewed snapshot in " +
    "``tools/census/support.json``. Update that manifest when decoder evidence " +
    "changes; the generator validates coverage but cannot prove the entries.")
$lines.Add("- The not-fully-supported ranking measures feature demand only. It " +
    "does not include coefficient-profile or traversal blockers because those " +
    "dimensions are outside the current AVM census schema.")
$lines.Add("- AVM was built with ``CONFIG_ACCOUNTING=0``. Entropy symbols, CDF " +
    "contexts, EOB values, and coefficient levels are outside this census.")

[void][IO.Directory]::CreateDirectory((Split-Path -Parent $OutputPath))
$utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($OutputPath, ($lines -join "`n") + "`n", $utf8WithoutBom)
Write-Output "Wrote $OutputPath"