param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,
    [Parameter(Mandatory = $true)]
    [string]$OutputPath,
    [Parameter(Mandatory = $true)]
    [string]$Namespace,
    [Parameter(Mandatory = $true)]
    [string]$Symbol
)

$inputFullPath = [IO.Path]::GetFullPath($InputPath)
$outputFullPath = [IO.Path]::GetFullPath($OutputPath)
$bytes = [IO.File]::ReadAllBytes($inputFullPath)
$outputDirectory = [IO.Path]::GetDirectoryName($outputFullPath)
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null

$builder = [Text.StringBuilder]::new()
[void]$builder.AppendLine("#pragma once")
[void]$builder.AppendLine()
[void]$builder.AppendLine("#include <cstddef>")
[void]$builder.AppendLine()
[void]$builder.AppendLine("namespace $Namespace")
[void]$builder.AppendLine("{")
[void]$builder.AppendLine("inline constexpr unsigned char $Symbol[] =")
[void]$builder.AppendLine("{")
for ($offset = 0; $offset -lt $bytes.Length; $offset += 16) {
    $count = [Math]::Min(16, $bytes.Length - $offset)
    $values = for ($index = 0; $index -lt $count; ++$index) {
        "0x{0:X2}" -f $bytes[$offset + $index]
    }
    [void]$builder.AppendLine("`t" + ($values -join ", ") + ",")
}
[void]$builder.AppendLine("};")
[void]$builder.AppendLine(
    "inline constexpr std::size_t ${Symbol}Size = sizeof($Symbol);")
[void]$builder.AppendLine("}")

$encoding = [Text.UTF8Encoding]::new($false)
$content = $builder.ToString()
if ([IO.File]::Exists($outputFullPath) -and
    [IO.File]::ReadAllText($outputFullPath, $encoding) -ceq $content) {
    exit 0
}
[IO.File]::WriteAllText($outputFullPath, $content, $encoding)
