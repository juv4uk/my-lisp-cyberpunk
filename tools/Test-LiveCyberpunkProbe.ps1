[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$GameDir,

    [Parameter(Mandatory)]
    [string]$BridgeArtifact,

    [Parameter(Mandatory)]
    [int]$ProcessId,

    [string]$MyLispExe = (Join-Path $PSScriptRoot '..\runtime\my-lisp\target\release\my-lisp.exe'),

    [string]$EvidenceOut = '',

    [ValidateRange(1, 300)]
    [int]$ObservationTimeoutSeconds = 30,

    # CI may exercise the physical verification mechanism with a disposable
    # Windows process, but harness evidence must never be presented as the
    # missing Cyberpunk2077.exe witness from #31.
    [switch]$HarnessMode,

    # Deliberately test-only. A real live-game claim is always hard-bound to
    # <GameDir>\bin\x64\Cyberpunk2077.exe and cannot override this path.
    [string]$ExpectedProcessPath = ''
)

<#
.SYNOPSIS
    Verifies #31's bounded in-process bridge evidence without reading or
    mutating game memory.

.DESCRIPTION
    This verifier does not inject anything and does not inspect RTTI, offsets,
    engine objects, or raw game pointers. It verifies only externally visible
    facts that bind one process to one exact bridge artifact:

      1. #19/#43 vanilla boundary admits the installed version.dll by exact
         SHA-256 identity with a separate trusted build artifact;
      2. the target process image is exactly Cyberpunk2077.exe for a real
         claim (or an explicit process path in HarnessMode);
      3. that process has loaded the exact game-side bin\x64\version.dll;
      4. bridge-observation.lisp reports bridge-alive for the same PID and is
         fresh for this process lifetime;
      5. canonical my-lisp accepts the observation as plain Lisp data;
      6. a machine-readable Lisp evidence record is emitted outside the game
         runtime layout.

    HarnessMode proves this verification mechanism in CI. Only a successful
    run without HarnessMode is eligible to say `(target-mode live-cyberpunk)`.
#>

$ErrorActionPreference = 'Stop'

function Get-Sha256Hex([string]$Path) {
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

function Test-PathEqual([string]$Left, [string]$Right) {
    return [string]::Equals(
        [System.IO.Path]::GetFullPath($Left).TrimEnd('\'),
        [System.IO.Path]::GetFullPath($Right).TrimEnd('\'),
        [System.StringComparison]::OrdinalIgnoreCase)
}

function ConvertTo-LispString([string]$Value) {
    # Normalize Windows separators before quoting so the Lisp string contains
    # no accidental backslash escape sequences.
    $normalized = $Value.Replace('\', '/')
    $normalized = $normalized.Replace('"', '\"')
    return '"' + $normalized + '"'
}

$gameRoot = (Resolve-Path -LiteralPath $GameDir).Path
if (-not (Test-Path -LiteralPath $BridgeArtifact -PathType Leaf)) {
    throw "trusted bridge artifact does not exist: $BridgeArtifact"
}
$trustedBridge = (Resolve-Path -LiteralPath $BridgeArtifact).Path

if (-not (Test-Path -LiteralPath $MyLispExe -PathType Leaf)) {
    throw "canonical my-lisp executable does not exist: $MyLispExe"
}
$MyLispExe = (Resolve-Path -LiteralPath $MyLispExe).Path

$gameBin = Join-Path $gameRoot 'bin\x64'
$installedBridge = Join-Path $gameBin 'version.dll'
$observationPath = Join-Path $gameBin 'bridge-observation.lisp'
$boundary = Join-Path $PSScriptRoot 'Test-VanillaBoundary.ps1'

if (-not (Test-Path -LiteralPath $installedBridge -PathType Leaf)) {
    throw "game-side bridge is absent: $installedBridge"
}
if (-not (Test-Path -LiteralPath $boundary -PathType Leaf)) {
    throw "vanilla boundary guard is absent: $boundary"
}

if ($HarnessMode) {
    if ([string]::IsNullOrWhiteSpace($ExpectedProcessPath)) {
        throw 'HarnessMode requires -ExpectedProcessPath so CI cannot accept an arbitrary process'
    }
    if (-not (Test-Path -LiteralPath $ExpectedProcessPath -PathType Leaf)) {
        throw "expected harness process image does not exist: $ExpectedProcessPath"
    }
    $expectedImage = (Resolve-Path -LiteralPath $ExpectedProcessPath).Path
    $targetMode = 'harness'
} else {
    if (-not [string]::IsNullOrWhiteSpace($ExpectedProcessPath)) {
        throw '-ExpectedProcessPath is permitted only with -HarnessMode; live evidence is hard-bound to Cyberpunk2077.exe'
    }
    $expectedImage = Join-Path $gameBin 'Cyberpunk2077.exe'
    if (-not (Test-Path -LiteralPath $expectedImage -PathType Leaf)) {
        throw "Cyberpunk2077.exe does not exist at the required live target path: $expectedImage"
    }
    $expectedImage = (Resolve-Path -LiteralPath $expectedImage).Path
    $targetMode = 'live-cyberpunk'
}

# The boundary guard is the authority for absence of third-party runtime
# frameworks and for exact artifact admission. Do not reproduce a second list
# of forbidden loaders here.
& $boundary -GameDir $gameRoot -AdmittedBridge $trustedBridge

$installedHash = Get-Sha256Hex $installedBridge
$trustedHash = Get-Sha256Hex $trustedBridge
if ($installedHash -ne $trustedHash) {
    throw "bridge identity mismatch after boundary check: installed SHA-256 $installedHash, trusted SHA-256 $trustedHash"
}

try {
    $process = [System.Diagnostics.Process]::GetProcessById($ProcessId)
} catch {
    throw "target process id $ProcessId is not running: $($_.Exception.Message)"
}

try {
    $actualImage = $process.MainModule.FileName
} catch {
    throw "could not read target process image for pid ${ProcessId}: $($_.Exception.Message)"
}
if (-not (Test-PathEqual $actualImage $expectedImage)) {
    throw "process image mismatch for pid ${ProcessId}: expected '$expectedImage', actual '$actualImage'"
}
if (-not $HarnessMode -and (Split-Path -Leaf $actualImage) -cne 'Cyberpunk2077.exe') {
    throw "live target process image is not exactly Cyberpunk2077.exe: $actualImage"
}

$processStartUtc = $process.StartTime.ToUniversalTime()
$deadline = [DateTime]::UtcNow.AddSeconds($ObservationTimeoutSeconds)
$loadedModulePath = $null
$lastModuleError = $null

do {
    try {
        $process = [System.Diagnostics.Process]::GetProcessById($ProcessId)
        $modules = @($process.Modules)
        foreach ($module in $modules) {
            if (-not [string]::IsNullOrWhiteSpace($module.FileName) -and
                (Test-PathEqual $module.FileName $installedBridge)) {
                $loadedModulePath = $module.FileName
                break
            }
        }
        $lastModuleError = $null
    } catch {
        $lastModuleError = $_.Exception.Message
    }

    if ($null -ne $loadedModulePath) { break }
    if ([DateTime]::UtcNow -ge $deadline) { break }
    Start-Sleep -Milliseconds 200
} while ($true)

if ($null -eq $loadedModulePath) {
    if ($null -ne $lastModuleError) {
        throw "exact game-side version.dll was not observed in pid $ProcessId before timeout; module enumeration error: $lastModuleError"
    }
    throw "exact game-side version.dll was not observed loaded in pid $ProcessId before timeout: $installedBridge"
}

$observationText = $null
do {
    if (Test-Path -LiteralPath $observationPath -PathType Leaf) {
        $observationText = Get-Content -Raw -LiteralPath $observationPath
        break
    }
    if ([DateTime]::UtcNow -ge $deadline) { break }
    Start-Sleep -Milliseconds 200
} while ($true)

if ([string]::IsNullOrWhiteSpace($observationText)) {
    throw "bridge observation did not appear before timeout: $observationPath"
}
if ($observationText -notmatch '\(game-observation/1') {
    throw "bridge observation is not a game-observation/1 record: $observationText"
}
if ($observationText -notmatch '\(source in-process\)') {
    throw "bridge observation source is not in-process: $observationText"
}
if ($observationText -notmatch '\(fact bridge-alive\)') {
    throw "bridge observation fact is not bridge-alive: $observationText"
}
if ($observationText -notmatch '\(value t\)') {
    throw "bridge-alive observation is not true: $observationText"
}
if ($observationText -notmatch '\(validity valid\)') {
    throw "bridge observation is not valid: $observationText"
}

$pidMatch = [regex]::Match($observationText, '\(process-id\s+([0-9]+)\)')
if (-not $pidMatch.Success) {
    throw "bridge observation has no provenance process-id: $observationText"
}
$observationPid = [int]$pidMatch.Groups[1].Value
if ($observationPid -ne $ProcessId) {
    throw "observation process-id mismatch: expected $ProcessId, observed $observationPid"
}

$timestampMatch = [regex]::Match($observationText, '\(timestamp\s+([0-9]+)\)')
if (-not $timestampMatch.Success) {
    throw "bridge observation has no timestamp: $observationText"
}
$observationTimestamp = [long]$timestampMatch.Groups[1].Value
$processStartMs = [DateTimeOffset]::new($processStartUtc).ToUnixTimeMilliseconds()
$nowMs = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
if ($observationTimestamp -lt ($processStartMs - 2000)) {
    throw "bridge observation predates this process lifetime: observation=$observationTimestamp process-start=$processStartMs"
}
if ($observationTimestamp -gt ($nowMs + 10000)) {
    throw "bridge observation timestamp is implausibly in the future: observation=$observationTimestamp now=$nowMs"
}

$oracle = & $MyLispExe --oracle-check $observationPath 2>&1
$oracleExit = $LASTEXITCODE
$oracleText = $oracle -join "`n"
if ($oracleExit -ne 0 -or $oracleText -notmatch '\(outcome valid\)') {
    throw "bridge observation failed canonical my-lisp oracle-check (exit=$oracleExit): $oracleText"
}

if ([string]::IsNullOrWhiteSpace($EvidenceOut)) {
    $EvidenceOut = Join-Path (Get-Location).Path "cyberpunk-live-probe-evidence-$ProcessId.lisp"
} elseif (-not [System.IO.Path]::IsPathRooted($EvidenceOut)) {
    $EvidenceOut = Join-Path (Get-Location).Path $EvidenceOut
}
$evidenceParent = Split-Path -Parent $EvidenceOut
if (-not [string]::IsNullOrWhiteSpace($evidenceParent) -and -not (Test-Path -LiteralPath $evidenceParent)) {
    New-Item -ItemType Directory -Path $evidenceParent -Force | Out-Null
}

$observationHash = Get-Sha256Hex $observationPath
$verifiedAt = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$record = @"
(cyberpunk-live-probe-evidence/1
  (target-mode $targetMode)
  (process-id $ProcessId)
  (process-image $(ConvertTo-LispString $actualImage))
  (process-start $processStartMs)
  (bridge-path $(ConvertTo-LispString $installedBridge))
  (trusted-bridge $(ConvertTo-LispString $trustedBridge))
  (bridge-sha256 $(ConvertTo-LispString $installedHash))
  (bridge-identity valid)
  (module-load valid)
  (vanilla-boundary valid)
  (observation-path $(ConvertTo-LispString $observationPath))
  (observation-sha256 $(ConvertTo-LispString $observationHash))
  (observation-fact bridge-alive)
  (observation-process-id $observationPid)
  (observation-timestamp $observationTimestamp)
  (canonical-oracle valid)
  (verified-at $verifiedAt))
"@.Trim()

Set-Content -LiteralPath $EvidenceOut -Value $record -Encoding utf8 -NoNewline

$evidenceOracle = & $MyLispExe --oracle-check $EvidenceOut 2>&1
$evidenceOracleExit = $LASTEXITCODE
$evidenceOracleText = $evidenceOracle -join "`n"
if ($evidenceOracleExit -ne 0 -or $evidenceOracleText -notmatch '\(outcome valid\)') {
    Remove-Item -LiteralPath $EvidenceOut -Force -ErrorAction SilentlyContinue
    throw "live probe evidence record is not canonical my-lisp-readable data (exit=$evidenceOracleExit): $evidenceOracleText"
}

Write-Host "PASS: target process image verified: $actualImage"
Write-Host "PASS: exact bridge module loaded: $loadedModulePath"
Write-Host "PASS: exact bridge SHA-256: $installedHash"
Write-Host "PASS: same-PID bridge-alive observation accepted by canonical my-lisp"
Write-Host "PASS: evidence mode=$targetMode written to $EvidenceOut"
