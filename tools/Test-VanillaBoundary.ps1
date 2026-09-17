[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$GameDir,

    # Trusted build-side copy of our own one-file bridge. This is optional
    # only for the truly vanilla case where the game directory has no
    # bin\x64\version.dll at all. A deployed version.dll is never admitted by
    # filename alone.
    [string]$AdmittedBridge = ''
)

<#
.SYNOPSIS
    Vanilla-runtime boundary guard for #19/#43.

.DESCRIPTION
    Fails if a forbidden third-party runtime framework is present. The final
    one-file architecture legitimately installs our own bin\x64\version.dll,
    so that path is handled separately: it is allowed only when its SHA-256
    exactly matches an explicitly supplied trusted build artifact.

    The retired adjacent version-original.dll forwarding layout is always
    rejected. PR #49's bridge resolves the genuine Windows version.dll from
    System32 by absolute path instead.
#>

$ErrorActionPreference = 'Stop'

function Get-Sha256Hex([string]$Path) {
    # Do not depend on Get-FileHash here. This guard is also executed from the
    # historical CTest/Windows-PowerShell path, where module discovery can be
    # narrower than in pwsh. SHA-256 itself is the contract, so use the .NET
    # primitive directly and keep the guard portable across both hosts.
    $stream = [System.IO.File]::OpenRead($Path)
    $hasher = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = $hasher.ComputeHash($stream)
    } finally {
        $stream.Dispose()
        $hasher.Dispose()
    }

    return ([System.BitConverter]::ToString($bytes).Replace('-', '').ToLowerInvariant())
}

$gameRoot = (Resolve-Path -LiteralPath $GameDir).Path

$forbidden = @(
    @{ Path = 'red4ext'; Name = 'RED4ext (loader directory)' }
    @{ Path = 'bin\x64\winmm.dll'; Name = 'RED4ext winmm.dll proxy loader' }
    @{ Path = 'bin\x64\version-original.dll'; Name = 'retired adjacent version.dll forwarding target' }
    @{ Path = 'bin\x64\cyber_engine_tweaks.asi'; Name = 'CET (legacy top-level path)' }
    @{ Path = 'bin\x64\plugins\cyber_engine_tweaks.asi'; Name = 'CET' }
    @{ Path = 'bin\x64\plugins\cyber_engine_tweaks'; Name = 'CET plugin directory' }
    @{ Path = 'r6\scripts\NeuralDeck'; Name = 'our old RED4ext-era Redscript' }
)

# r6\cache\modded\ and final.redscripts.modded are NOT forbidden on their
# own: official first-party REDmod writes there. The forbidden signal is a
# third-party runtime framework, not that cache directory's existence.

$failures = @()

foreach ($entry in $forbidden) {
    $fullPath = Join-Path $gameRoot $entry.Path
    if (Test-Path -LiteralPath $fullPath) {
        $failures += "FAIL: forbidden runtime dependency present: $($entry.Name) at $fullPath"
    } else {
        Write-Host "PASS: absent - $($entry.Name)"
    }
}

# `version.dll` is the one intentionally shared filename between forbidden
# historical loaders and our final bridge. Never whitelist the name. Admit
# only byte identity with a separate trusted build-side artifact.
$gameVersion = Join-Path $gameRoot 'bin\x64\version.dll'
if (Test-Path -LiteralPath $gameVersion) {
    if (-not (Test-Path -LiteralPath $gameVersion -PathType Leaf)) {
        $failures += "FAIL: bin\x64\version.dll exists but is not a regular file: $gameVersion"
    } elseif ([string]::IsNullOrWhiteSpace($AdmittedBridge)) {
        $failures += "FAIL: unproven version.dll present with no admitted bridge artifact: $gameVersion"
    } elseif (-not (Test-Path -LiteralPath $AdmittedBridge -PathType Leaf)) {
        $failures += "FAIL: admitted bridge artifact does not exist as a file: $AdmittedBridge"
    } else {
        $resolvedGameVersion = (Resolve-Path -LiteralPath $gameVersion).Path
        $resolvedAdmitted = (Resolve-Path -LiteralPath $AdmittedBridge).Path

        if ([string]::Equals($resolvedGameVersion, $resolvedAdmitted, [System.StringComparison]::OrdinalIgnoreCase)) {
            $failures += 'FAIL: admitted bridge must be an independent trusted build artifact, not the deployed version.dll itself'
        } else {
            $actualHash = Get-Sha256Hex $resolvedGameVersion
            $admittedHash = Get-Sha256Hex $resolvedAdmitted

            if ($actualHash -ne $admittedHash) {
                $failures += "FAIL: foreign or tampered version.dll: actual SHA-256 $actualHash does not match admitted SHA-256 $admittedHash"
            } else {
                Write-Host "PASS: admitted bridge SHA-256 $actualHash - exact version.dll identity verified"
            }
        }
    }
} else {
    Write-Host 'PASS: absent - mod version.dll (clean vanilla baseline)'
}

# Any leftover .asi under bin\x64\plugins is a third-party loader by
# convention on this engine; flag anything we did not already name above.
$pluginsDir = Join-Path $gameRoot 'bin\x64\plugins'
if (Test-Path -LiteralPath $pluginsDir) {
    Get-ChildItem -LiteralPath $pluginsDir -Filter '*.asi' -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
        $failures += "FAIL: unrecognized .asi loader present: $($_.FullName)"
    }
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Host $_ }
    throw "Vanilla boundary violated: $($failures.Count) forbidden or unproven item(s) found in $gameRoot"
}

Write-Host "PASS: vanilla runtime boundary holds in $gameRoot"
