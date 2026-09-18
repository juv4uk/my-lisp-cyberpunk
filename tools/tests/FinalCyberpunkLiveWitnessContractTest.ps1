# Contract-only CI witness for #31's final physical one-file replay runner.
# This test never launches Cyberpunk and must never emit live-cyberpunk evidence.
$ErrorActionPreference = 'Stop'

$runner = Join-Path (Split-Path -Parent $PSScriptRoot) 'Invoke-FinalCyberpunkLiveWitness.ps1'
if (-not (Test-Path -LiteralPath $runner -PathType Leaf)) {
    throw "final live witness runner is missing: $runner"
}

$text = Get-Content -Raw -LiteralPath $runner

$tokens = $null
$parseErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile(
    $runner,
    [ref]$tokens,
    [ref]$parseErrors
)
if ($parseErrors.Count -ne 0) {
    $details = ($parseErrors | ForEach-Object { $_.Message }) -join '; '
    throw "final live witness runner has PowerShell parse errors: $details"
}

$required = @(
    @{ Pattern = '\[string\]\$GameDir'; Label = 'explicit GameDir input' },
    @{ Pattern = '\[string\]\$BridgeArtifact'; Label = 'separate trusted bridge artifact input' },
    @{ Pattern = '\[string\]\$MyLispExe'; Label = 'canonical my-lisp oracle input' },
    @{ Pattern = '\[string\]\$EvidenceOut'; Label = 'evidence output outside runtime layout' },
    @{ Pattern = '\[string\]\$TranscriptOut'; Label = 'transcript output outside runtime layout' },
    @{ Pattern = 'Cyberpunk2077\.exe'; Label = 'hard-bound real game executable' },
    @{ Pattern = '(?s)Start-Process.+?-PassThru'; Label = 'PID authority comes from Start-Process -PassThru' },
    @{ Pattern = '\$gameProcess\.Id'; Label = 'started process PID is reused as verifier authority' },
    @{ Pattern = 'Test-VanillaBoundary\.ps1'; Label = 'merged exact-hash boundary guard is reused' },
    @{ Pattern = 'Test-LiveCyberpunkProbe\.ps1'; Label = 'merged live verifier is reused' },
    @{ Pattern = '-ProcessId\s+\$gameProcess\.Id'; Label = 'verifier receives exact started PID' },
    @{ Pattern = '\(target-mode live-cyberpunk\)'; Label = 'runner requires real-mode evidence marker' },
    @{ Pattern = 'bridge-observation\.lisp'; Label = 'stale observation is explicitly handled' },
    @{ Pattern = 'Read-Host.+MENU'; Label = 'human startup/menu confirmation is recorded' },
    @{ Pattern = 'визначити'; Label = 'Ukrainian canonical persistence witness' },
    @{ Pattern = '\(визначити\s+подвоїти\s+\(функція'; Label = 'Ukrainian closure definition witness' },
    @{ Pattern = '\(подвоїти 21\)'; Label = 'closure execution witness' },
    @{ Pattern = '\(car 5\)'; Label = 'canonical type-error witness' },
    @{ Pattern = 'missing-symbol-24'; Label = 'unknown-symbol failure witness' },
    @{ Pattern = 'malformed-syntax-24'; Label = 'malformed-syntax failure witness marker' },
    @{ Pattern = '\(\+ 20 22\)'; Label = 'post-error valid recovery witness' },
    @{ Pattern = "StartsWith\('error:'"; Label = 'canonical error text is observed rather than reclassified' },
    @{ Pattern = 'PASS Ukrainian closure persisted and executed'; Label = 'closure success is transcripted' },
    @{ Pattern = 'PASS canonical type error recovered in same Session with prior state intact'; Label = 'type-error recovery keeps pre-error state' },
    @{ Pattern = 'PASS unknown symbol failed canonically and Session recovered'; Label = 'unknown-symbol recovery is transcripted' },
    @{ Pattern = 'PASS malformed syntax failed canonically and Session recovered'; Label = 'malformed-syntax recovery is transcripted' },
    @{ Pattern = 'negative.*trust|wrong.*trust|tampered.*trust'; Label = 'fail-closed wrong-trust negative witness' },
    @{ Pattern = 'WaitForExit'; Label = 'cleanup waits for user-closed game rather than killing it' }
)

foreach ($entry in $required) {
    if ($text -notmatch $entry.Pattern) {
        throw "final live witness runner missing contract: $($entry.Label)"
    }
}

$forbidden = @(
    @{ Pattern = 'HarnessMode'; Label = 'CI harness mode must never be used by physical runner' },
    @{ Pattern = 'ExpectedProcessPath'; Label = 'physical runner must not override expected game image' },
    @{ Pattern = '\btasklist\b'; Label = 'tasklist must not be PID authority' },
    @{ Pattern = '(?i)Get-Process[^\r\n]*Cyberpunk'; Label = 'process-name lookup must not replace Start-Process PID authority' },
    @{ Pattern = '\bStop-Process\b'; Label = 'runner must not forcibly terminate the game' },
    @{ Pattern = 'WriteProcessMemory|ReadProcessMemory|OpenProcess'; Label = 'no process-memory API is allowed' },
    @{ Pattern = 'SendInput|keybd_event|mouse_event'; Label = 'no input injection is allowed' }
)

foreach ($entry in $forbidden) {
    if ($text -match $entry.Pattern) {
        throw "final live witness runner violates contract: $($entry.Label)"
    }
}

# Require the trusted artifact to stay separate from the deployed game-side copy.
if ($text -notmatch 'Copy-Item.+\$trustedBridge.+\$installedBridge') {
    throw 'runner must deploy the separate trusted artifact to the game-side version.dll'
}
if ($text -notmatch 'Get-Sha256Hex.+\$installedBridge' -or $text -notmatch 'Get-Sha256Hex.+\$trustedBridge') {
    throw 'runner must compare installed and trusted bridge hashes'
}

# Clean uninstall must be conditional on identity, then re-run vanilla guard.
if ($text -notmatch '(?s)installedHash.+trustedHash.+Remove-Item.+\$installedBridge') {
    throw 'runner must remove game-side version.dll only after exact ownership/hash check'
}
if ($text -notmatch '(?s)Remove-Item.+\$installedBridge.+Test-VanillaBoundary') {
    throw 'runner must prove clean vanilla boundary after uninstall'
}

Write-Output 'final live witness orchestration contract OK'
