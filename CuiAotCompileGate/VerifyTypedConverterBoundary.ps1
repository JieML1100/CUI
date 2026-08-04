[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$GeneratedBase
)

$ErrorActionPreference = 'Stop'

$resolvedBase = [System.IO.Path]::GetFullPath($GeneratedBase)
$generatedCpp = "$resolvedBase.g.cpp"
if (-not (Test-Path -LiteralPath $generatedCpp -PathType Leaf)) {
    throw "Typed converter gate is missing generated output: $generatedCpp"
}

$generatedText = [System.IO.File]::ReadAllText($generatedCpp)
$requiredPatterns = @(
    [pscustomobject]@{
        Name = 'typed converter fixture include'
        Regex = '(?m)^\s*#\s*include\s*"TypedBindingConverters\.h"\s*$'
    }
    [pscustomobject]@{
        Name = 'direct single-value converter factory call'
        Regex = '::CuiAotFixture::Converters::CreatePrefixConverter\s*\(\s*\)'
    }
    [pscustomobject]@{
        Name = 'direct multi-value converter factory call'
        Regex = '::CuiAotFixture::Converters::CreateJoinConverter\s*\(\s*\)'
    }
)
foreach ($requirement in $requiredPatterns) {
    if (-not [regex]::IsMatch($generatedText, $requirement.Regex)) {
        throw "Typed converter gate omitted $($requirement.Name): $generatedCpp"
    }
}

foreach ($forbidden in @(
    'AotFixture.Prefix'
    'AotFixture.Join'
    'BindingValueConverterRegistry::Create'
    'MultiBindingValueConverterRegistry::Create'
)) {
    if ($generatedText.Contains($forbidden)) {
        throw "Typed converter gate retained forbidden runtime identity '$forbidden': $generatedCpp"
    }
}

Write-Host "CUI typed converter AOT boundary gate passed: $resolvedBase"
