[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$GeneratedBase
)

$ErrorActionPreference = 'Stop'

$resolvedBase = [System.IO.Path]::GetFullPath($GeneratedBase)
$generatedHeader = "$resolvedBase.g.h"
$generatedCpp = "$resolvedBase.g.cpp"
foreach ($generatedFile in @($generatedHeader, $generatedCpp)) {
    if (-not (Test-Path -LiteralPath $generatedFile -PathType Leaf)) {
        throw "Binding-scope gate is missing generated output: $generatedFile"
    }
}

$generatedText = [System.IO.File]::ReadAllText($generatedCpp)

# One shared lowering implementation must reach Window, ComponentDefinition,
# DataTemplate, and ControlTemplate. Each domain owns a distinct format marker.
$domainFormats = @(
    'window:{0}|{1}'
    'component:{0}|{1}'
    'data:{0}|{1}'
    'control:{0}|{1}'
)
foreach ($format in $domainFormats) {
    $escapedFormat = [regex]::Escape($format)
    if (-not [regex]::IsMatch(
            $generatedText,
            "DataBindings\.AddMulti\([^\r\n]*$escapedFormat")) {
        throw "Binding-scope gate omitted the '$format' AddMulti domain: $generatedCpp"
    }
}

$multiAttachCount = [regex]::Matches(
    $generatedText,
    'DataBindings\.AddMulti\(').Count
if ($multiAttachCount -lt 4) {
    throw "Binding-scope gate expected four AddMulti domains, found ${multiAttachCount}: $generatedCpp"
}

$requiredPatterns = @(
    [pscustomobject]@{
        Name = 'direct dependency-property endpoint marker'
        Regex = 'CUI:AOT binding-source=direct-dp'
    }
    [pscustomobject]@{
        Name = 'FindAncestor explicit source adapter'
        Regex = 'cui::binding::CreateFindAncestorSource\s*\('
    }
    [pscustomobject]@{
        Name = 'compiled-record endpoint marker'
        Regex = 'CUI:AOT binding-source=direct-record'
    }
    [pscustomobject]@{
        Name = 'compiled-record typed endpoint factory'
        Regex = 'cui::binding::MakeCompiledRecordPropertySource\s*<'
    }
)
foreach ($requirement in $requiredPatterns) {
    if (-not [regex]::IsMatch($generatedText, $requirement.Regex)) {
        throw "Binding-scope gate omitted $($requirement.Name): $generatedCpp"
    }
}

# A complex ElementName path must embed the known native ItemsSource DP as an
# authoritative v2 endpoint resolver. Its token field stays empty, so failure
# to resolve the current DependencyObject cannot fall back to token dispatch.
$exactNativePathStep =
    '(?s)CUI:AOT\s+binding-path-endpoint=exact-dp\s*(?:\r?\n)\s*\{\s*' +
    'CompiledBindingPathStepKind::Property\s*,.*?\{\}\s*,\s*0u\s*,\s*' +
    '\+\[\]\(IBindingSource&\s+source\)\s+noexcept\s*->\s*CompiledSourceHandle\s*' +
    '\{\s*return\s+cui::binding::ResolveCompiledDependencyPropertySource\s*' +
    '\(\s*source\s*,\s*ItemsControl::ItemsSourceProperty\s*\(\s*\)\s*\)\s*;\s*\}'
if (-not [regex]::IsMatch($generatedText, $exactNativePathStep)) {
    throw "Binding-scope gate omitted the name-free exact ItemsSource path resolver: $generatedCpp"
}
$exactStepLines = [regex]::Matches(
    $generatedText,
    '(?m)^\s*//\s*CUI:AOT\s+binding-path-endpoint=exact-dp\s*\r?\n(?<Step>[^\r\n]+)')
foreach ($exactStep in $exactStepLines) {
    if ($exactStep.Groups['Step'].Value.Contains('BindingSourcePropertyToken')) {
        throw "Binding-scope exact DP resolver retained a token fallback: $generatedCpp"
    }
}
if ($generatedText.Contains('L"ItemsSource[0].Name"')) {
    throw "Binding-scope gate retained the authored complex ElementName path string: $generatedCpp"
}

if ([regex]::Matches(
        $generatedText,
        'cui::binding::CreateFindAncestorSource\s*\(').Count -lt 2) {
    throw "Binding-scope gate expected ordinary and MultiBinding FindAncestor adapters: $generatedCpp"
}
$exactAncestorPathStep =
    '(?s)CUI:AOT\s+binding-path-endpoint=exact-dp\s*(?:\r?\n)\s*\{\s*' +
    'CompiledBindingPathStepKind::Property\s*,.*?\{\}\s*,\s*0u\s*,\s*' +
    '\+\[\]\(IBindingSource&\s+source\)\s+noexcept\s*->\s*CompiledSourceHandle\s*' +
    '\{\s*return\s+cui::binding::ResolveCompiledFindAncestorDependencyPropertySource\s*' +
    '\(\s*source\s*,\s*Control::TagProperty\s*\(\s*\)\s*\)\s*;\s*\}'
$exactAncestorStepCount = [regex]::Matches(
    $generatedText,
    $exactAncestorPathStep).Count
if ($exactAncestorStepCount -lt 2) {
    throw "Binding-scope gate expected ordinary and MultiBinding exact FindAncestor Tag resolvers, found ${exactAncestorStepCount}: $generatedCpp"
}

foreach ($forbidden in @(
    'BindingValueConverterRegistry::Create'
    'MultiBindingValueConverterRegistry::Create'
)) {
    if ($generatedText.Contains($forbidden)) {
        throw "Binding-scope gate retained forbidden runtime converter lookup '$forbidden': $generatedCpp"
    }
}

Write-Host "CUI four-scope binding AOT boundary gate passed: $resolvedBase"
