[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ProductionBindingObject,

    [Alias('ProductionRuntimeBoundaryObject')]
    [string]$ProductionRuntimeBoundaryBinary,

    [string]$DumpbinPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-Dumpbin {
    param([string]$ExplicitPath)

    if ($ExplicitPath) {
        return (Resolve-Path -LiteralPath $ExplicitPath).Path
    }
    $command = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $programFilesX86 = [Environment]::GetFolderPath(
        [Environment+SpecialFolder]::ProgramFilesX86)
    $vswhere = Join-Path $programFilesX86 `
        'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $installation = & $vswhere -latest -products '*' `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath | Select-Object -First 1
        if ($installation) {
            $toolRoot = Join-Path $installation 'VC\Tools\MSVC'
            foreach ($version in @(Get-ChildItem -LiteralPath $toolRoot `
                    -Directory | Sort-Object Name -Descending)) {
                $candidate = Join-Path $version.FullName `
                    'bin\Hostx64\x64\dumpbin.exe'
                if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                    return (Resolve-Path -LiteralPath $candidate).Path
                }
            }
        }
    }
    throw 'Cannot locate dumpbin.exe; pass -DumpbinPath explicitly.'
}

$objectPath = (Resolve-Path -LiteralPath $ProductionBindingObject).Path
$dumpbin = Resolve-Dumpbin -ExplicitPath $DumpbinPath
$symbols = (& $dumpbin /nologo /symbols $objectPath) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw "dumpbin.exe failed for $objectPath"
}

$expected = @(
    '\?GetMetadata@DependencyPropertyRegistry'
)
foreach ($pattern in $expected) {
    if ($symbols -notmatch $pattern) {
        throw "A8e3 boundary gate did not find expected symbol: $pattern"
    }
}

$forbidden = @(
    'DependencyPropertyMetadataCache',
    '\?RegisteredDependencyProperties@@',
    '\?RegisteredBindingProperties@@',
    '\?RegisteredBindingPropertyTokenLayers@@',
    '\?BindingPropertyMutex@@',
    '\?NextDependencyPropertyGlobalIndex@@',
    '\?Register@DependencyPropertyRegistry@@',
    '\?RegisterReadOnly@DependencyPropertyRegistry@@',
    '\?AddOwner@DependencyPropertyRegistry@@',
    '\?OverrideMetadata@DependencyPropertyRegistry@@',
    '\?FindProperty@DependencyPropertyRegistry@@',
    '\?FindNative@DependencyPropertyRegistry@@',
    '\?FindNativeCore@DependencyPropertyRegistry@@',
    '\?FindRegistered@DependencyPropertyRegistry@@',
    '\?GetRegisteredProperties@DependencyPropertyRegistry@@',
    '\?ResolveMetadata@DependencyPropertyRegistry@@',
    '\?CreateStandalone@DependencyPropertyRegistry@@',
    '\?Find@DependencyPropertyRegistry@@'
)

$violations = [System.Collections.Generic.List[string]]::new()
foreach ($pattern in $forbidden) {
    if ($symbols -match $pattern) {
        $violations.Add($pattern)
    }
}
if ($violations.Count -ne 0) {
    throw ('A8e3 Production Binding.obj retains legacy dependency-property ' +
        'registry symbols: ' + ($violations -join ', '))
}

if ($ProductionRuntimeBoundaryBinary) {
    $boundaryBinaryPath =
        (Resolve-Path -LiteralPath $ProductionRuntimeBoundaryBinary).Path
    $boundaryArguments = if ([IO.Path]::GetExtension($boundaryBinaryPath) `
            -ieq '.lib') {
        @('/nologo', '/linkermember:1', $boundaryBinaryPath)
    } else {
        @('/nologo', '/symbols', $boundaryBinaryPath)
    }
    $boundarySymbols = (& $dumpbin @boundaryArguments) -join "`n"
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin.exe failed for $boundaryBinaryPath"
    }

    $layoutMarker = 'CuiA8e4DependencyPropertySidecarLayout_128_696'
    if ($boundarySymbols -notmatch [regex]::Escape($layoutMarker)) {
        throw ('A8e4 Production runtime-boundary object does not publish the ' +
            'expected name-free DependencyProperty layout marker: ' +
            $layoutMarker)
    }
}

$layoutSummary = if ($ProductionRuntimeBoundaryBinary) {
    '; identity=128 bytes, metadata=696 bytes, authored-name sidecars=0'
} else {
    ''
}
Write-Host ('CUI A8e3/A8e4 dependency-property boundary gate passed: ' +
    'metadata cache, global registries, name lookup and legacy mutation symbols=0' +
    $layoutSummary)
