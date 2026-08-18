[CmdletBinding()]
param(
    [ValidateSet('Release')]
    [string]$Configuration = 'Release',

    [ValidateSet('x64')]
    [string]$Platform = 'x64',

    [ValidateSet('Closure', 'Full')]
    [string[]]$ThemeModes = @('Closure', 'Full'),

    [ValidateRange(0, 20)]
    [int]$Warmup = 1,

    [ValidateRange(3, 101)]
    [int]$Iterations = 9,

    [ValidateRange(1, 120)]
    [int]$TimeoutSeconds = 15,

    [string]$OutputPath,

    [string]$BaselinePath,

    [ValidateRange(0.0, 100.0)]
    [double]$StartupTolerancePercent = 15.0,

    [ValidateRange(0.0, 100.0)]
    [double]$MemoryTolerancePercent = 10.0,

    [ValidateRange(0.0, 100.0)]
    [double]$AllocationTolerancePercent = 2.0,

    [ValidateRange(0.0, 100.0)]
    [double]$BinarySizeTolerancePercent = 2.0,

    [string]$MSBuildPath,

    [switch]$Rebuild,

    [switch]$NoBuild,

    [switch]$AllowHostMismatch
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-MSBuildPath {
    param([string]$ExplicitPath)

    if ($ExplicitPath) {
        $resolved = (Resolve-Path -LiteralPath $ExplicitPath).Path
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
            throw "MSBuild executable was not found: $ExplicitPath"
        }
        return $resolved
    }

    # Keep the measurement build on the caller's active toolchain. Mixing a
    # newer globally installed MSBuild with existing LTCG/IPDB outputs from the
    # developer shell can make an otherwise valid incremental link unreadable.
    $activeCommand = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($activeCommand) {
        return $activeCommand.Source
    }

    $candidates = @(
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe')
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    $programFilesX86 = [Environment]::GetFolderPath(
        [Environment+SpecialFolder]::ProgramFilesX86)
    $vswhere = Join-Path $programFilesX86 `
        'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $found = & $vswhere -latest -products '*' `
            -requires Microsoft.Component.MSBuild `
            -find 'MSBuild\**\Bin\MSBuild.exe' |
            Select-Object -First 1
        if ($found -and (Test-Path -LiteralPath $found -PathType Leaf)) {
            return $found
        }
    }

    throw 'Unable to locate MSBuild.exe. Pass -MSBuildPath explicitly.'
}

function Get-Percentile {
    param(
        [double[]]$Values,
        [ValidateRange(0.0, 1.0)]
        [double]$Percentile
    )

    if ($Values.Count -eq 0) {
        throw 'Cannot calculate a percentile for an empty sample.'
    }
    $sorted = @($Values | Sort-Object)
    if ($sorted.Count -eq 1) {
        return [double]$sorted[0]
    }
    $position = ($sorted.Count - 1) * $Percentile
    $lower = [Math]::Floor($position)
    $upper = [Math]::Ceiling($position)
    if ($lower -eq $upper) {
        return [double]$sorted[$lower]
    }
    $fraction = $position - $lower
    return [double]$sorted[$lower] + `
        ([double]$sorted[$upper] - [double]$sorted[$lower]) * $fraction
}

function Get-Statistics {
    param([object[]]$Samples)

    $metricNames = @(
        'process_create_to_first_frame_us',
        'entry_to_component_ready_us',
        'entry_to_first_frame_us',
        'pre_main_cpp_new_calls',
        'pre_main_cpp_new_requested_bytes',
        'entry_to_component_ready_cpp_new_calls',
        'entry_to_component_ready_cpp_new_requested_bytes',
        'entry_to_first_frame_cpp_new_calls',
        'entry_to_first_frame_cpp_new_requested_bytes',
        'component_ready_working_set_bytes',
        'first_frame_working_set_bytes',
        'first_frame_peak_working_set_bytes',
        'first_frame_private_usage_bytes',
        'harness_process_elapsed_ms'
    )
    $statistics = [ordered]@{}
    foreach ($metricName in $metricNames) {
        $values = [double[]]@($Samples | ForEach-Object {
            [double]($_.PSObject.Properties[$metricName].Value)
        })
        $median = Get-Percentile -Values $values -Percentile 0.5
        $deviations = [double[]]@($values | ForEach-Object {
            [Math]::Abs($_ - $median)
        })
        $statistics[$metricName] = [ordered]@{
            median = $median
            mad = Get-Percentile -Values $deviations -Percentile 0.5
            minimum = ($values | Measure-Object -Minimum).Minimum
            maximum = ($values | Measure-Object -Maximum).Maximum
        }
    }
    return $statistics
}

function Assert-MeasurementSample {
    param(
        [object]$Sample,
        [string]$ExpectedThemeMode
    )

    if ([int]$Sample.schema_version -ne 1) {
        throw "Unsupported runtime measurement schema: $($Sample.schema_version)"
    }
    if ([string]$Sample.theme_mode -ne $ExpectedThemeMode) {
        throw "Measurement theme mismatch: expected $ExpectedThemeMode, got $($Sample.theme_mode)"
    }
    if ([int]$Sample.dynamic_xaml -ne 0) {
        throw 'Runtime measurement linked the Design/dynamic-XAML ABI.'
    }
    if ([string]$Sample.timing_scope -ne 'global_operator_new_instrumented') {
        throw "Unsupported runtime timing scope: $($Sample.timing_scope)"
    }
    if ([string]$Sample.allocation_scope -ne 'global_operator_new_only') {
        throw "Unsupported runtime allocation scope: $($Sample.allocation_scope)"
    }
    foreach ($metricName in @(
        'process_create_to_first_frame_us',
        'entry_to_component_ready_us',
        'entry_to_first_frame_us',
        'first_frame_working_set_bytes',
        'first_frame_private_usage_bytes')) {
        if ([double]$Sample.PSObject.Properties[$metricName].Value -le 0) {
            throw "Runtime measurement returned a non-positive $metricName."
        }
    }
    if ([double]$Sample.entry_to_first_frame_us -lt `
        [double]$Sample.entry_to_component_ready_us) {
        throw 'First-frame time is earlier than component-ready time.'
    }
    if ([double]$Sample.entry_to_first_frame_cpp_new_calls -lt `
        [double]$Sample.entry_to_component_ready_cpp_new_calls) {
        throw 'First-frame allocation count is below component-ready count.'
    }
    foreach ($property in $Sample.object_sizes.PSObject.Properties) {
        if ([int64]$property.Value -le 0) {
            throw "Runtime measurement returned an invalid sizeof($($property.Name))."
        }
    }
}

function Invoke-MeasurementProcess {
    param(
        [string]$Executable,
        [string]$WorkingDirectory,
        [string]$ExpectedThemeMode,
        [int]$TimeoutMilliseconds
    )

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        if (-not $process.Start()) {
            throw "Failed to start runtime measurement: $Executable"
        }
        if (-not $process.WaitForExit($TimeoutMilliseconds)) {
            try { $process.Kill() } catch { }
            throw "Runtime measurement timed out after $TimeoutMilliseconds ms."
        }
        $stdout = $process.StandardOutput.ReadToEnd()
        $stderr = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        $stopwatch.Stop()
        if ($process.ExitCode -ne 0) {
            throw "Runtime measurement exited $($process.ExitCode). stderr: $stderr stdout: $stdout"
        }
        $jsonLine = @($stdout -split "`r?`n" |
            Where-Object { $_.TrimStart().StartsWith('{') }) |
            Select-Object -Last 1
        if (-not $jsonLine) {
            throw "Runtime measurement emitted no JSON. stderr: $stderr stdout: $stdout"
        }
        $sample = $jsonLine | ConvertFrom-Json
        $sample | Add-Member -NotePropertyName harness_process_elapsed_ms `
            -NotePropertyValue $stopwatch.Elapsed.TotalMilliseconds
        Assert-MeasurementSample -Sample $sample `
            -ExpectedThemeMode $ExpectedThemeMode
        return $sample
    }
    finally {
        $stopwatch.Stop()
        $process.Dispose()
    }
}

function Assert-StableObjectSizes {
    param([object[]]$Samples)

    $reference = $Samples[0].object_sizes
    foreach ($property in $reference.PSObject.Properties) {
        $name = $property.Name
        $expected = [int64]$property.Value
        foreach ($sample in $Samples) {
            $actual = [int64]$sample.object_sizes.PSObject.Properties[$name].Value
            if ($actual -ne $expected) {
                throw "sizeof($name) changed within one measurement run: $expected -> $actual"
            }
        }
    }
    return $reference
}

function Get-HostFingerprint {
    return [ordered]@{
        machine_name = [Environment]::MachineName
        os_version = [Environment]::OSVersion.VersionString
        processor_identifier = [Environment]::GetEnvironmentVariable(
            'PROCESSOR_IDENTIFIER')
        processor_count = [Environment]::ProcessorCount
        powershell_version = $PSVersionTable.PSVersion.ToString()
    }
}

function Get-BaselineRegressions {
    param(
        [object]$Current,
        [object]$Baseline,
        [double]$StartupTolerance,
        [double]$MemoryTolerance,
        [double]$AllocationTolerance,
        [double]$BinarySizeTolerance,
        [bool]$PermitHostMismatch
    )

    $regressions = [System.Collections.Generic.List[string]]::new()
    foreach ($identity in @(
        'schema_version',
        'measurement_kind',
        'configuration',
        'platform',
        'runtime_flavor')) {
        $currentValue = [string]$Current.PSObject.Properties[$identity].Value
        $baselineProperty = $Baseline.PSObject.Properties[$identity]
        if (-not $baselineProperty) {
            $regressions.Add("Baseline lacks required metadata '$identity'.")
        } elseif ($currentValue -ne [string]$baselineProperty.Value) {
            $regressions.Add(
                "Baseline $identity differs: current '$currentValue', baseline '$($baselineProperty.Value)'.")
        }
    }
    if ($regressions.Count -ne 0) {
        return $regressions
    }
    if (-not $PermitHostMismatch -and (
        [string]$Current.host.machine_name -ne `
            [string]$Baseline.host.machine_name -or
        [string]$Current.host.processor_identifier -ne `
            [string]$Baseline.host.processor_identifier -or
        [int]$Current.host.processor_count -ne `
            [int]$Baseline.host.processor_count -or
        [string]$Current.host.os_version -ne `
            [string]$Baseline.host.os_version)) {
        $regressions.Add(
            'Baseline machine/CPU/OS differs; pass -AllowHostMismatch only for an intentional non-comparable run.')
        return $regressions
    }

    $metricGroups = @(
        [pscustomobject]@{
            Names = @(
                'process_create_to_first_frame_us',
                'entry_to_component_ready_us',
                'entry_to_first_frame_us')
            Tolerance = $StartupTolerance
        },
        [pscustomobject]@{
            Names = @(
                'component_ready_working_set_bytes',
                'first_frame_working_set_bytes',
                'first_frame_peak_working_set_bytes',
                'first_frame_private_usage_bytes')
            Tolerance = $MemoryTolerance
        },
        [pscustomobject]@{
            Names = @(
                'pre_main_cpp_new_calls',
                'pre_main_cpp_new_requested_bytes',
                'entry_to_component_ready_cpp_new_calls',
                'entry_to_component_ready_cpp_new_requested_bytes',
                'entry_to_first_frame_cpp_new_calls',
                'entry_to_first_frame_cpp_new_requested_bytes')
            Tolerance = $AllocationTolerance
        }
    )

    foreach ($currentMode in $Current.modes) {
        $baselineMode = @($Baseline.modes | Where-Object {
            $_.theme_mode -eq $currentMode.theme_mode
        }) | Select-Object -First 1
        if (-not $baselineMode) {
            $regressions.Add("Baseline lacks theme mode $($currentMode.theme_mode).")
            continue
        }
        if (-not $baselineMode.PSObject.Properties['executable_bytes']) {
            $regressions.Add(
                "Baseline lacks executable_bytes for $($currentMode.theme_mode).")
        } else {
            $currentBytes = [double]$currentMode.executable_bytes
            $baselineBytes = [double]$baselineMode.executable_bytes
            $binarySizeLimit = $baselineBytes * `
                (1.0 + $BinarySizeTolerance / 100.0)
            if ($currentBytes -gt $binarySizeLimit) {
                $regressions.Add(
                    "$($currentMode.theme_mode).executable_bytes $currentBytes exceeds baseline $baselineBytes + $BinarySizeTolerance%.")
            }
        }
        foreach ($group in $metricGroups) {
            foreach ($metricName in $group.Names) {
                $currentProperty = $currentMode.statistics.PSObject.Properties[$metricName]
                $baselineProperty = $baselineMode.statistics.PSObject.Properties[$metricName]
                if (-not $currentProperty -or -not $baselineProperty) {
                    $regressions.Add(
                        "Baseline comparison lacks $($currentMode.theme_mode).$metricName.")
                    continue
                }
                $currentMedian = [double]$currentProperty.Value.median
                $baselineMedian = [double]$baselineProperty.Value.median
                $limit = $baselineMedian * (1.0 + [double]$group.Tolerance / 100.0)
                if ($currentMedian -gt $limit) {
                    $regressions.Add(
                        "$($currentMode.theme_mode).$metricName median $currentMedian exceeds baseline $baselineMedian + $($group.Tolerance)%.")
                }
            }
        }
        foreach ($sizeProperty in $currentMode.object_sizes.PSObject.Properties) {
            $baselineSizeProperty = `
                $baselineMode.object_sizes.PSObject.Properties[$sizeProperty.Name]
            if (-not $baselineSizeProperty) {
                # A newly instrumented type has no historical upper bound yet.
                # Keep reporting it in the artifact and begin enforcing it from
                # this batch's baseline instead of turning schema growth into a
                # runtime regression.
                continue
            }
            if ([int64]$sizeProperty.Value -gt [int64]$baselineSizeProperty.Value) {
                $regressions.Add(
                    "$($currentMode.theme_mode) sizeof($($sizeProperty.Name)) grew from $($baselineSizeProperty.Value) to $($sizeProperty.Value).")
            }
        }
    }
    return $regressions
}

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$projectPath = Join-Path $repoRoot `
    'CuiStaticGeneratedSample\CuiStaticGeneratedSample.vcxproj'
if (-not $OutputPath) {
    $OutputPath = Join-Path $repoRoot `
        'artifacts\validation\cui-runtime-measurement.json'
}
$OutputPath = [IO.Path]::GetFullPath($OutputPath)

$resolvedMSBuild = if ($NoBuild) {
    $null
} else {
    Resolve-MSBuildPath -ExplicitPath $MSBuildPath
}
$uniqueThemeModes = @($ThemeModes | Select-Object -Unique)
$modeResults = [System.Collections.Generic.List[object]]::new()
$buildTarget = if ($Rebuild) { 'Rebuild' } else { 'Build' }

foreach ($themeMode in $uniqueThemeModes) {
    if (-not $NoBuild) {
        Write-Host "Building Release runtime measurement ($themeMode)..."
        # The sample selects Production locally. Passing CuiRuntimeFlavor as a
        # global property would also force design-only CodeGen/CuiRuntime
        # project references into the Production ABI.
        $buildArguments = @(
            $projectPath,
            "/t:$buildTarget",
            '/m:1',
            "/p:Configuration=$Configuration",
            "/p:Platform=$Platform",
            '/p:CuiRuntimeMeasurement=true',
            "/p:CuiFrameworkThemeMode=$themeMode",
            '/p:CuiMeasureLinkMap=true',
            '/v:minimal'
        )
        & $resolvedMSBuild @buildArguments
        if ($LASTEXITCODE -ne 0) {
            throw "Runtime measurement build failed for $themeMode."
        }
    }

    $executableName = "CuiStaticGeneratedSample.Measure.$themeMode.exe"
    $executable = Join-Path $repoRoot `
        "$Platform\$Configuration\$executableName"
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Runtime measurement executable was not found: $executable"
    }

    Write-Host "Warming runtime measurement ($themeMode) $Warmup time(s)..."
    for ($index = 0; $index -lt $Warmup; ++$index) {
        $null = Invoke-MeasurementProcess `
            -Executable $executable `
            -WorkingDirectory $repoRoot `
            -ExpectedThemeMode $themeMode `
            -TimeoutMilliseconds ($TimeoutSeconds * 1000)
    }

    Write-Host "Collecting $Iterations fresh-process sample(s) ($themeMode)..."
    $samples = [System.Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt $Iterations; ++$index) {
        $sample = Invoke-MeasurementProcess `
            -Executable $executable `
            -WorkingDirectory $repoRoot `
            -ExpectedThemeMode $themeMode `
            -TimeoutMilliseconds ($TimeoutSeconds * 1000)
        $samples.Add($sample)
    }
    $sampleArray = @($samples)
    $file = Get-Item -LiteralPath $executable
    $modeResults.Add([ordered]@{
        theme_mode = $themeMode
        executable = $file.FullName
        executable_bytes = $file.Length
        executable_sha256 = (Get-FileHash -LiteralPath $executable `
            -Algorithm SHA256).Hash
        object_sizes = Assert-StableObjectSizes -Samples $sampleArray
        statistics = Get-Statistics -Samples $sampleArray
        samples = $sampleArray
    })
}

$hostFingerprint = Get-HostFingerprint
$result = [ordered]@{
    schema_version = 1
    measurement_kind = 'warm_fresh_process_first_committed_frame'
    generated_utc = [DateTime]::UtcNow.ToString('o')
    configuration = $Configuration
    platform = $Platform
    runtime_flavor = 'Production'
    warmup = $Warmup
    iterations = $Iterations
    timeout_seconds = $TimeoutSeconds
    timing_scope = 'global_operator_new_instrumented'
    allocation_scope = 'global_operator_new_only'
    tolerances_percent = [ordered]@{
        startup = $StartupTolerancePercent
        memory = $MemoryTolerancePercent
        allocation = $AllocationTolerancePercent
        binary_size = $BinarySizeTolerancePercent
    }
    host = $hostFingerprint
    modes = @($modeResults)
    baseline_path = if ($BaselinePath) {
        [IO.Path]::GetFullPath($BaselinePath)
    } else {
        $null
    }
    regressions = @()
    passed = $true
}

if ($BaselinePath) {
    $resolvedBaselinePath = (Resolve-Path -LiteralPath $BaselinePath).Path
    $baseline = Get-Content -Raw -LiteralPath $resolvedBaselinePath |
        ConvertFrom-Json
    $normalizedCurrent = $result | ConvertTo-Json -Depth 12 |
        ConvertFrom-Json
    $regressions = @(Get-BaselineRegressions `
        -Current $normalizedCurrent `
        -Baseline $baseline `
        -StartupTolerance $StartupTolerancePercent `
        -MemoryTolerance $MemoryTolerancePercent `
        -AllocationTolerance $AllocationTolerancePercent `
        -BinarySizeTolerance $BinarySizeTolerancePercent `
        -PermitHostMismatch ([bool]$AllowHostMismatch))
    $result.regressions = $regressions
    $result.passed = $regressions.Count -eq 0
}

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    $null = New-Item -ItemType Directory -Path $outputDirectory -Force
}
$json = $result | ConvertTo-Json -Depth 12
$utf8WithoutBom = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText($OutputPath, $json + [Environment]::NewLine, `
    $utf8WithoutBom)

Write-Host "CUI runtime measurement written to $OutputPath"
foreach ($mode in $result.modes) {
    $firstFrame = $mode.statistics.entry_to_first_frame_us.median
    $workingSet = $mode.statistics.first_frame_working_set_bytes.median
    $allocations = `
        $mode.statistics.entry_to_first_frame_cpp_new_calls.median
    Write-Host (("{0}: first frame {1:N3} us, working set {2:N0} B, " +
        "C++ new calls {3:N0}") -f `
        $mode.theme_mode, $firstFrame, $workingSet, $allocations)
}

if (-not $result.passed) {
    foreach ($regression in $result.regressions) {
        Write-Error $regression -ErrorAction Continue
    }
    exit 3
}
