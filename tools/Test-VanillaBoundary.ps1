[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$GameDir
)

<#
.SYNOPSIS
    Negative test for docs/vanilla-runtime-boundary.md (#19): fails if any
    forbidden third-party runtime mod framework is present in the game
    directory. Passes silently on a clean vanilla install and on our own
    read-only probe artifacts (tools/memory-probe-v0.ps1,
    tools/screen-probe-v0.ps1 install nothing into the game directory by
    construction, so this test never needs to special-case them).

.NOTES
    This supersedes tools/Test-LocalGameDeployment.ps1's assumption that
    RED4ext/Codeware/our old adapter plugin SHOULD be present -- that was
    the pre-vanilla-pivot architecture (see
    docs/owner-decision-2026-09-16-pause-and-revert-to-vanilla.md). The two
    scripts intentionally assert opposite things and are not meant to both
    pass against the same install at the same time.
#>

$ErrorActionPreference = 'Stop'

$gameRoot = (Resolve-Path -LiteralPath $GameDir).Path

$forbidden = @(
    @{ Path = 'red4ext'; Name = 'RED4ext (loader directory)' }
    @{ Path = 'bin\x64\winmm.dll'; Name = 'RED4ext winmm.dll proxy loader' }
    @{ Path = 'bin\x64\version.dll'; Name = 'CET version.dll proxy loader' }
    @{ Path = 'bin\x64\cyber_engine_tweaks.asi'; Name = 'CET (legacy top-level path)' }
    @{ Path = 'bin\x64\plugins\cyber_engine_tweaks.asi'; Name = 'CET' }
    @{ Path = 'bin\x64\plugins\cyber_engine_tweaks'; Name = 'CET plugin directory' }
    @{ Path = 'r6\scripts\NeuralDeck'; Name = 'our old RED4ext-era Redscript' }
    @{ Path = 'r6\cache\final.redscripts.modded'; Name = 'compiled modded script cache' }
    @{ Path = 'r6\cache\modded'; Name = 'modded script cache directory' }
)

$failures = @()

foreach ($entry in $forbidden) {
    $fullPath = Join-Path $gameRoot $entry.Path
    if (Test-Path -LiteralPath $fullPath) {
        $failures += "FAIL: forbidden runtime dependency present: $($entry.Name) at $fullPath"
    } else {
        Write-Host "PASS: absent - $($entry.Name)"
    }
}

# Any leftover .asi under bin\x64\plugins is a third-party loader by
# convention on this engine (ASI loaders are how CET and similar
# frameworks inject); flag anything we didn't already name above.
$pluginsDir = Join-Path $gameRoot 'bin\x64\plugins'
if (Test-Path -LiteralPath $pluginsDir) {
    Get-ChildItem -LiteralPath $pluginsDir -Filter '*.asi' -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
        $failures += "FAIL: unrecognized .asi loader present: $($_.FullName)"
    }
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Host $_ }
    throw "Vanilla boundary violated: $($failures.Count) forbidden item(s) found in $gameRoot"
}

Write-Host "PASS: vanilla runtime boundary holds - 0 forbidden third-party mod frameworks in $gameRoot"
