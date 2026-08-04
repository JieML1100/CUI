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
        throw "Dependency-property storage gate is missing generated output: $generatedFile"
    }
}

$headerText = [System.IO.File]::ReadAllText($generatedHeader)
$generatedText = [System.IO.File]::ReadAllText($generatedCpp)
$ownerName = 'DependencyPropertyStorageContractWindowGeneratedStorageCard'
$owner = [regex]::Escape($ownerName)

foreach ($property in @('Caption', 'Count', 'State')) {
    $propertyPattern = ('\bstatic\s+const\s+DependencyProperty\s*&\s*{0}Property\s*\(' -f
        [regex]::Escape($property))
    if (-not [regex]::IsMatch($headerText, $propertyPattern)) {
        throw "Dependency-property storage gate omitted $property identity: $generatedHeader"
    }
}
if (-not [regex]::IsMatch($headerText, '\bvoid\s+SetCaption\s*\(') -or
    -not [regex]::IsMatch($headerText, '\bvoid\s+SetCount\s*\(')) {
    throw "Dependency-property storage gate omitted a writable component wrapper: $generatedHeader"
}
if ([regex]::IsMatch($headerText, '\bSetState\s*\(') -or
    -not [regex]::IsMatch($headerText, '\bbool\s+PublishState\s*\(')) {
    throw "Dependency-property storage gate did not preserve the read-only State contract: $generatedHeader"
}
if (-not [regex]::IsMatch(
    $headerText,
    '\bvoid\s+VisitDeclaredInheritedProperties\s*\([^;]+\)\s+const\s+override\s*;',
    [System.Text.RegularExpressions.RegexOptions]::Singleline)) {
    throw "Dependency-property storage gate omitted the sparse inherited-property visitor: $generatedHeader"
}

$staticMarkers = [regex]::Matches(
    $generatedText,
    '(?m)^\s*//\s*CUI:AOT\s+dependency-property\s*=\s*static\s*$').Count
if ($staticMarkers -ne 3) {
    throw "Dependency-property storage gate expected three static DP markers, found ${staticMarkers}: $generatedCpp"
}

$tokenIdentityMarkers = [regex]::Matches(
    $generatedText,
    '(?m)^\s*//\s*CUI:AOT\s+dependency-property-identity\s*=\s*token\s*$').Count
if ($tokenIdentityMarkers -ne 3) {
    throw "Dependency-property storage gate expected three name-free Production identities, found ${tokenIdentityMarkers}: $generatedCpp"
}

$writableStaticPattern =
    "\bDependencyPropertyRegistry\s*::\s*RegisterStatic\s*<\s*$owner\s*,"
$readOnlyStaticPattern =
    "\bDependencyPropertyRegistry\s*::\s*RegisterReadOnlyStatic\s*<\s*$owner\s*,"
$writableStaticCount = [regex]::Matches(
    $generatedText, $writableStaticPattern).Count
$readOnlyStaticCount = [regex]::Matches(
    $generatedText, $readOnlyStaticPattern).Count
if ($writableStaticCount -ne 2 -or $readOnlyStaticCount -ne 1) {
    throw ("Dependency-property storage gate expected 2 writable + 1 read-only static registrations, " +
        "found writable=${writableStaticCount}, readOnly=${readOnlyStaticCount}: $generatedCpp")
}

$legacyOwnerRegistration =
    "\bDependencyPropertyRegistry\s*::\s*(?:Register|RegisterReadOnly)\s*<\s*$owner\s*,"
if ([regex]::IsMatch($generatedText, $legacyOwnerRegistration)) {
    throw "Dependency-property storage gate retained legacy registry publication for ${ownerName}: $generatedCpp"
}

$inheritedVisitorPattern =
    "\b$owner\s*::\s*VisitDeclaredInheritedProperties\s*\([^)]*\)\s*const\s*" +
    '\{[^}]*\bCanvas\s*::\s*VisitDeclaredInheritedProperties\s*\(' +
    '[^}]*\bvisitor\s*\(\s*context\s*,\s*CaptionProperty\s*\(\s*\)\s*\)'
if (-not [regex]::IsMatch(
    $generatedText,
    $inheritedVisitorPattern,
    [System.Text.RegularExpressions.RegexOptions]::Singleline)) {
    throw "Dependency-property storage gate omitted Caption from the sparse inherited-property visitor: $generatedCpp"
}

$directMarkers = [regex]::Matches(
    $generatedText,
    '(?m)^\s*//\s*CUI:AOT\s+binding-source\s*=\s*direct-dp\s*$').Count
if ($directMarkers -lt 3) {
    throw "Dependency-property storage gate expected at least three direct DP bindings, found ${directMarkers}: $generatedCpp"
}

foreach ($property in @('Caption', 'State')) {
    $directComponentSource =
        "\bMakeCompiledDependencyPropertySource\s*\(\s*\*this\s*,\s*$owner\s*::\s*$property" +
        'Property\s*\(\s*\)\s*\)'
    if (-not [regex]::IsMatch($generatedText, $directComponentSource)) {
        throw "Dependency-property storage gate omitted direct $property component source: $generatedCpp"
    }
}

$directComponentTarget =
    "\bDataBindings\s*\.\s*Add\s*\(\s*$owner\s*::\s*CaptionProperty\s*\(\s*\)"
if (-not [regex]::IsMatch($generatedText, $directComponentTarget)) {
    throw "Dependency-property storage gate omitted the writable component DP target: $generatedCpp"
}

Write-Host ("CUI dependency-property static storage boundary gate passed: " +
    "$resolvedBase (writable=${writableStaticCount}, readOnly=${readOnlyStaticCount}, direct=${directMarkers})")
