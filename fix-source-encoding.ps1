[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter(Position = 0)]
    [string] $Root,

    # By default only files that contain non-ASCII bytes are changed. Use this
    # switch when a completely uniform UTF-8 BOM policy is desired.
    [switch] $AllCodeFiles
)

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = $PSScriptRoot
}

$rootPath = (Resolve-Path -LiteralPath $Root).Path
$extensions = @(
    '.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.inl', '.ixx',
    '.rc', '.rc2', '.def', '.idl', '.manifest', '.natvis', '.xaml', '.xml',
    '.json', '.props', '.targets', '.vcxproj', '.filters', '.csproj', '.sln',
    '.ps1', '.cmd', '.bat'
)
$excludedDirectoryNames = @(
    '.git', '.vs', 'DotNetSource', 'packages', 'artifacts',
    'Release', 'Debug', 'x64', 'Win32', 'obj', 'bin'
)
$utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)
$bom = [byte[]](0xEF, 0xBB, 0xBF)

$scanned = 0
$changed = 0
$alreadyBom = 0
$asciiOnly = 0
$invalid = 0

$files = New-Object System.Collections.Generic.List[System.IO.FileInfo]
$directories = New-Object System.Collections.Generic.Stack[string]
$directories.Push($rootPath)

# Prune excluded directories before enumerating their contents. In particular,
# DotNetSource is a large reference tree and must never be scanned.
while ($directories.Count -gt 0) {
    $directory = $directories.Pop()
    foreach ($entry in Get-ChildItem -LiteralPath $directory -Force) {
        if ($entry.PSIsContainer) {
            if ($excludedDirectoryNames -notcontains $entry.Name -and
                (($entry.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -eq 0)) {
                $directories.Push($entry.FullName)
            }
            continue
        }

        if ($extensions -contains $entry.Extension.ToLowerInvariant()) {
            [void]$files.Add($entry)
        }
    }
}

foreach ($file in $files) {
    $scanned++
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)

    if ($bytes.Length -ge 3 -and
        $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        $alreadyBom++
        continue
    }

    try {
        [void]$utf8Strict.GetString($bytes)
    }
    catch {
        $invalid++
        Write-Warning ("Skipping non-UTF-8 file: {0}" -f $file.FullName)
        continue
    }

    $hasNonAscii = $false
    foreach ($byte in $bytes) {
        if ($byte -gt 0x7F) {
            $hasNonAscii = $true
            break
        }
    }

    if (-not $AllCodeFiles -and -not $hasNonAscii) {
        $asciiOnly++
        continue
    }

    $newBytes = New-Object byte[] ($bytes.Length + $bom.Length)
    [System.Array]::Copy($bom, 0, $newBytes, 0, $bom.Length)
    [System.Array]::Copy($bytes, 0, $newBytes, $bom.Length, $bytes.Length)

    if ($PSCmdlet.ShouldProcess($file.FullName, 'prepend UTF-8 BOM')) {
        [System.IO.File]::WriteAllBytes($file.FullName, $newBytes)
        $changed++
    }
}

Write-Host ("Scanned {0} code files; added BOM to {1}; already had BOM: {2}; ASCII-only skipped: {3}; invalid UTF-8: {4}." -f `
    $scanned, $changed, $alreadyBom, $asciiOnly, $invalid)

if ($invalid -gt 0) {
    exit 2
}
