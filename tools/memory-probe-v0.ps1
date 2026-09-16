#Requires -Version 5.1
<#
.SYNOPSIS
    CP-PROBE-MEMORY-V0 (issue #29) mechanism-only skeleton: process
    discovery, module base resolve, and build fingerprint, emitting a
    game-observation/1 record (docs/observation-contract-v0.md, #28).

.DESCRIPTION
    This is deliberately NOT a working ReadProcessMemory probe yet. It
    proves the parts of the mechanism that can be done honestly without
    a validated offset: finding the live process, resolving its main
    module base address (no hardcoded absolute address, no admin
    elevation), and reading a build fingerprint from the executable's
    own version resource.

    It does not read player-health-ratio (or any other in-memory value)
    because no offset for it has been validated against a live,
    running game yet -- see the accompanying issue comment for why:
    Cyberpunk 2077's REDengine resolves object layout through its own
    RTTI system at runtime rather than fixed struct offsets, so a
    genuinely honest memory read needs either a validated
    pointer-chain + offset found through live RE, or an RTTI walk
    (which reintroduces exactly the complexity #27's "vanilla" epic is
    trying to avoid). Emitting a fabricated "valid" value here would be
    precisely the silent-lie failure mode the epic's fail-closed
    discipline exists to prevent.

    Every unresolved case fails closed as a proper game-observation/1
    record with `validity unavailable` or `unsupported-build`, per
    docs/observation-contract-v0.md.

.OUTPUTS
    Prints one game-observation/1 s-expression to stdout.
#>

param(
    [string]$ProcessName = "Cyberpunk2077"
)

$ErrorActionPreference = "Stop"

function Format-Observation {
    param(
        [string]$Source,
        [string]$Fingerprint,
        [string]$Fact,
        [string]$Value,
        [string]$Validity,
        [string]$ProvenanceBody
    )
    $timestamp = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    return "(game-observation/1 (source $Source) (game-fingerprint `"$Fingerprint`") (fact $Fact) (value $Value) (timestamp $timestamp) (validity $Validity) (provenance $ProvenanceBody))"
}

$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Select-Object -First 1

if (-not $proc) {
    Format-Observation `
        -Source "memory" `
        -Fingerprint "unknown" `
        -Fact "player-health-ratio" `
        -Value "()" `
        -Validity "unavailable" `
        -ProvenanceBody "(channel-revision 1) (reason `"process $ProcessName not running`")"
    exit 0
}

$module = $proc.MainModule
$moduleBase = "0x{0:X}" -f $module.BaseAddress.ToInt64()
$productVersion = $module.FileVersionInfo.ProductVersion
if (-not $productVersion) { $productVersion = "unknown" }
$fingerprint = "cp2077-$productVersion-x64"

# No offset/AOB profile exists yet for any build -- see the
# ObservationContractFixturesTest.cmake fixture memory-unsupported-build.lisp
# for the same shape this produces live. Adding a real profile is the next
# step, gated on a live RE session against a running game (take damage,
# diff two memory snapshots), not something this script should guess at.
Format-Observation `
    -Source "memory" `
    -Fingerprint $fingerprint `
    -Fact "player-health-ratio" `
    -Value "()" `
    -Validity "unsupported-build" `
    -ProvenanceBody "(channel-revision 1) (module-base `"$moduleBase`") (reason `"no offset/AOB profile exists yet for any build (see issue #29)`")"
