#Requires -Version 5.1
<#
.SYNOPSIS
    CP-PROBE-MEMORY-V0 (issue #29) mechanism-only skeleton: gathers raw
    facts (process found?, module base, build fingerprint) and hands
    the decision of what they MEAN to canonical my-lisp
    (tools/observation/observation-policy.lisp), instead of deciding
    `validity` itself. Prints the resulting game-observation/1 record.

.DESCRIPTION
    This script does not read player-health-ratio (or any other
    in-memory value) because no offset for it has been validated
    against a live, running game yet -- see docs/research/
    vanilla-entrypoints.md and the #29 issue thread for why. It reports
    only the raw booleans (process-found?, known-profile?) and lets
    Lisp -- this repo's standing semantic authority -- decide what
    `validity` follows from them, per docs/observation-contract-v0.md.

.OUTPUTS
    Prints one game-observation/1 s-expression to stdout, produced by
    canonical my-lisp, not by this script.
#>

param(
    [string]$ProcessName = "Cyberpunk2077",
    [string]$MyLispExe = (Join-Path $PSScriptRoot "..\runtime\my-lisp\target\release\my-lisp.exe"),
    [string]$PolicyFile = (Join-Path $PSScriptRoot "observation\observation-policy.lisp")
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $MyLispExe)) {
    throw "my-lisp.exe not found at $MyLispExe -- run tools/build-my-lisp-runtime.ps1 first"
}
if (-not (Test-Path -LiteralPath $PolicyFile)) {
    throw "observation policy not found at $PolicyFile"
}

function Invoke-ObservationPolicy {
    param(
        [string]$Source,
        [string]$Fingerprint,
        [string]$Fact,
        [string]$LispValue,
        [long]$Timestamp,
        [bool]$ProcessFound,
        [bool]$KnownProfile,
        [string]$ProvenanceBody
    )
    $processFoundLisp = if ($ProcessFound) { "t" } else { "()" }
    $knownProfileLisp = if ($KnownProfile) { "t" } else { "()" }
    $call = "(make-observation (quote $Source) `"$Fingerprint`" (quote $Fact) $LispValue $Timestamp $processFoundLisp $knownProfileLisp (quote $ProvenanceBody))"

    $driver = Join-Path $env:TEMP "observation-drive-$([guid]::NewGuid()).lisp"
    try {
        Get-Content -Raw -LiteralPath $PolicyFile | Out-File -Encoding ascii -NoNewline -FilePath $driver
        Add-Content -Path $driver -Value "`n$call" -Encoding ascii
        & $MyLispExe $driver
        if ($LASTEXITCODE -ne 0) {
            throw "canonical my-lisp policy evaluation failed (exit=$LASTEXITCODE)"
        }
    } finally {
        if (Test-Path -LiteralPath $driver) { Remove-Item -LiteralPath $driver -Force -ErrorAction SilentlyContinue }
    }
}

$timestamp = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Select-Object -First 1

if (-not $proc) {
    Invoke-ObservationPolicy `
        -Source "memory" -Fingerprint "unknown" -Fact "player-health-ratio" -LispValue "(quote ())" `
        -Timestamp $timestamp -ProcessFound $false -KnownProfile $false `
        -ProvenanceBody "((channel-revision 1) (reason `"process $ProcessName not running`"))"
    exit 0
}

$module = $proc.MainModule
$moduleBase = "0x{0:X}" -f $module.BaseAddress.ToInt64()
$productVersion = $module.FileVersionInfo.ProductVersion
if (-not $productVersion) { $productVersion = "unknown" }
$fingerprint = "cp2077-$productVersion-x64"

# No offset/AOB profile exists yet for any build -- this is a raw fact
# (do we have a profile for this fingerprint?), not a validity
# decision. observation-policy.lisp turns (process-found=t,
# known-profile=()) into `unsupported-build` on its own.
Invoke-ObservationPolicy `
    -Source "memory" -Fingerprint $fingerprint -Fact "player-health-ratio" -LispValue "(quote ())" `
    -Timestamp $timestamp -ProcessFound $true -KnownProfile $false `
    -ProvenanceBody "((channel-revision 1) (module-base `"$moduleBase`") (reason `"no offset/AOB profile exists yet for any build (see issue #29)`"))"
