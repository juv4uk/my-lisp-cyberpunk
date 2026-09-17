<#
.SYNOPSIS
    RED-first contract for #31's live-process evidence verifier.

.DESCRIPTION
    CI cannot claim a real Cyberpunk2077.exe witness. Instead this test loads
    the exact staged bridge into a disposable child PowerShell process and
    requires the production verifier to prove the same mechanics that a later
    local live-game run will use:
      - the target process image is the explicitly expected image;
      - the exact game-side version.dll is loaded in that process;
      - installed and trusted bridge SHA-256 identities match;
      - bridge-observation.lisp belongs to the same process id;
      - canonical my-lisp accepts both the observation and evidence records;
      - third-party runtime markers, a wrong trust anchor, a wrong process
        image, and stale/wrong-PID observation data all fail closed;
      - removing our one owned version.dll restores the clean vanilla guard.

    Harness evidence must identify itself as harness-only. It must never be
    mistaken for the still-missing physical Cyberpunk2077.exe witness.
#>

param(
    [string]$BridgeDll = (Join-Path $PSScriptRoot '..\..\bridge\runtime-stage\version.dll'),
    [string]$MyLispExe = (Join-Path $PSScriptRoot '..\..\runtime\my-lisp\target\release\my-lisp.exe')
)

$ErrorActionPreference = 'Stop'

$verifier = Join-Path $PSScriptRoot '..\Test-LiveCyberpunkProbe.ps1'
$boundary = Join-Path $PSScriptRoot '..\Test-VanillaBoundary.ps1'

if (-not (Test-Path -LiteralPath $verifier -PathType Leaf)) {
    throw "live Cyberpunk probe verifier missing: $verifier"
}
if (-not (Test-Path -LiteralPath $boundary -PathType Leaf)) {
    throw "vanilla boundary guard missing: $boundary"
}
if (-not (Test-Path -LiteralPath $BridgeDll -PathType Leaf)) {
    throw "staged bridge missing: $BridgeDll"
}
if (-not (Test-Path -LiteralPath $MyLispExe -PathType Leaf)) {
    throw "canonical my-lisp missing: $MyLispExe"
}

$BridgeDll = (Resolve-Path -LiteralPath $BridgeDll).Path
$MyLispExe = (Resolve-Path -LiteralPath $MyLispExe).Path
$selfExe = (Get-Process -Id $PID).Path
$pwshExe = $selfExe
$tempBase = if ([string]::IsNullOrWhiteSpace($env:RUNNER_TEMP)) { $env:TEMP } else { $env:RUNNER_TEMP }
$root = Join-Path $tempBase "cp31-live-probe-$([guid]::NewGuid())"
$gameBin = Join-Path $root 'bin\x64'
$installed = Join-Path $gameBin 'version.dll'
$observation = Join-Path $gameBin 'bridge-observation.lisp'
$evidence = Join-Path $root 'live-probe-evidence.lisp'
$childScript = Join-Path $root 'load-bridge-and-wait.ps1'
$childOut = Join-Path $root 'child.out.txt'
$childErr = Join-Path $root 'child.err.txt'

function Invoke-ExpectedFailure {
    param(
        [Parameter(Mandatory)][string]$Label,
        [Parameter(Mandatory)][string[]]$Arguments,
        [Parameter(Mandatory)][string]$ExpectedPattern
    )

    # This is a new pwsh process, so a flat argv array containing named
    # parameter tokens is intentional here. Direct script invocation below
    # uses a hashtable splat instead.
    $output = & $pwshExe -NoProfile -ExecutionPolicy Bypass -File $verifier @Arguments 2>&1
    $exit = $LASTEXITCODE
    $text = $output -join "`n"
    if ($exit -eq 0) {
        throw "$Label unexpectedly succeeded: $text"
    }
    if ($text -notmatch $ExpectedPattern) {
        throw "$Label failed for the wrong reason. Expected /$ExpectedPattern/ but got: $text"
    }
    Write-Host "PASS: $Label fails closed"
}

New-Item -ItemType Directory -Path $gameBin -Force | Out-Null
Copy-Item -LiteralPath $BridgeDll -Destination $installed

@'
param([Parameter(Mandatory)][string]$Dll)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Cp31Loader {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr LoadLibraryW(string lpFileName);
}
"@
$resolved = (Resolve-Path -LiteralPath $Dll).Path
$handle = [Cp31Loader]::LoadLibraryW($resolved)
if ($handle -eq [IntPtr]::Zero) {
    $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "LoadLibraryW failed for $resolved (win32=$err)"
}
Write-Output "loaded=$resolved handle=$handle"
Start-Sleep -Seconds 90
'@ | Set-Content -LiteralPath $childScript -Encoding utf8

$child = $null
try {
    $child = Start-Process -FilePath $pwshExe `
        -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $childScript, '-Dll', $installed) `
        -RedirectStandardOutput $childOut -RedirectStandardError $childErr -PassThru

    $commonDirect = @{
        GameDir = $root
        BridgeArtifact = $BridgeDll
        ProcessId = $child.Id
        MyLispExe = $MyLispExe
        HarnessMode = $true
        ExpectedProcessPath = $selfExe
        ObservationTimeoutSeconds = 30
    }
    $commonArgs = @(
        '-GameDir', $root,
        '-BridgeArtifact', $BridgeDll,
        '-ProcessId', [string]$child.Id,
        '-MyLispExe', $MyLispExe,
        '-HarnessMode',
        '-ExpectedProcessPath', $selfExe,
        '-ObservationTimeoutSeconds', '30'
    )

    & $verifier @commonDirect -EvidenceOut $evidence
    if ($LASTEXITCODE -ne 0) {
        throw "live probe verifier returned non-zero exit $LASTEXITCODE"
    }

    if (-not (Test-Path -LiteralPath $evidence -PathType Leaf)) {
        throw "verifier did not write evidence: $evidence"
    }
    $report = Get-Content -Raw -LiteralPath $evidence
    if ($report -notmatch '\(cyberpunk-live-probe-evidence/1') {
        throw "evidence has wrong record tag: $report"
    }
    if ($report -notmatch '\(target-mode harness\)') {
        throw "CI evidence is not explicitly marked harness-only: $report"
    }
    if ($report -notmatch "\(process-id $($child.Id)\)") {
        throw "evidence does not bind the target child process id: $report"
    }
    if ($report -notmatch '\(module-load valid\)') {
        throw "evidence does not prove exact module load: $report"
    }
    if ($report -notmatch '\(bridge-identity valid\)') {
        throw "evidence does not prove bridge identity: $report"
    }
    if ($report -notmatch '\(observation-fact bridge-alive\)') {
        throw "evidence does not bind bridge-alive observation: $report"
    }
    if ($report -notmatch '\(canonical-oracle valid\)') {
        throw "evidence does not record canonical oracle acceptance: $report"
    }

    $evidenceOracle = & $MyLispExe --oracle-check $evidence
    $evidenceOracleText = $evidenceOracle -join "`n"
    if ($LASTEXITCODE -ne 0 -or $evidenceOracleText -notmatch '\(outcome valid\)') {
        throw "live probe evidence is not canonical my-lisp-readable data: $evidenceOracleText"
    }
    Write-Host 'PASS: exact loaded bridge + same-PID canonical observation produce harness-scoped evidence'

    $foreign = Join-Path $root 'foreign-version.dll'
    Copy-Item -LiteralPath $BridgeDll -Destination $foreign
    [System.IO.File]::AppendAllText($foreign, 'tamper')
    $wrongTrustArgs = @(
        '-GameDir', $root,
        '-BridgeArtifact', $foreign,
        '-ProcessId', [string]$child.Id,
        '-MyLispExe', $MyLispExe,
        '-HarnessMode',
        '-ExpectedProcessPath', $selfExe,
        '-ObservationTimeoutSeconds', '3'
    )
    Invoke-ExpectedFailure `
        -Label 'wrong trust anchor' `
        -Arguments $wrongTrustArgs `
        -ExpectedPattern '(?i)(foreign|tampered|SHA-256|identity|boundary)'

    $wrongPathArgs = @(
        '-GameDir', $root,
        '-BridgeArtifact', $BridgeDll,
        '-ProcessId', [string]$child.Id,
        '-MyLispExe', $MyLispExe,
        '-HarnessMode',
        '-ExpectedProcessPath', $MyLispExe,
        '-ObservationTimeoutSeconds', '3'
    )
    Invoke-ExpectedFailure `
        -Label 'wrong process image' `
        -Arguments $wrongPathArgs `
        -ExpectedPattern '(?i)process image'

    $originalObservation = Get-Content -Raw -LiteralPath $observation
    $wrongPid = $child.Id + 1
    $tamperedObservation = $originalObservation -replace "\(process-id $($child.Id)\)", "(process-id $wrongPid)"
    if ($tamperedObservation -eq $originalObservation) {
        throw "test could not locate process-id $($child.Id) in observation"
    }
    Set-Content -LiteralPath $observation -Value $tamperedObservation -Encoding utf8 -NoNewline
    Invoke-ExpectedFailure `
        -Label 'stale or wrong-PID observation' `
        -Arguments $commonArgs `
        -ExpectedPattern '(?i)(observation.*process-id|process-id.*observation)'
    Set-Content -LiteralPath $observation -Value $originalObservation -Encoding utf8 -NoNewline

    New-Item -ItemType Directory -Path (Join-Path $root 'red4ext') -Force | Out-Null
    Invoke-ExpectedFailure `
        -Label 'forbidden third-party runtime marker' `
        -Arguments $commonArgs `
        -ExpectedPattern '(?i)(vanilla boundary|forbidden runtime|RED4ext)'
    Remove-Item -LiteralPath (Join-Path $root 'red4ext') -Recurse -Force

} finally {
    if ($null -ne $child) {
        Stop-Process -Id $child.Id -Force -ErrorAction SilentlyContinue
        try { $child.WaitForExit(10000) | Out-Null } catch { }
    }

    if (Test-Path -LiteralPath $installed) {
        Remove-Item -LiteralPath $installed -Force
    }
    if (Test-Path -LiteralPath $observation) {
        Remove-Item -LiteralPath $observation -Force
    }

    # Clean uninstall witness: once our one owned version.dll is removed, the
    # same directory must return to the clean-vanilla boundary state.
    & $boundary -GameDir $root

    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Host 'PASS: #31 evidence verifier contract holds in harness mode; no live-game claim was made'
