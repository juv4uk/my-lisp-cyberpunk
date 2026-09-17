<#
.SYNOPSIS
    Contract/mutation test for tools/Test-VanillaBoundary.ps1 (#43).
    The vanilla guard must distinguish the exact admitted Cyberpunk bridge
    from a foreign or tampered version.dll while continuing to reject
    third-party runtime frameworks and the retired version-original.dll layout.
#>

$ErrorActionPreference = 'Stop'
$guard = Join-Path $PSScriptRoot '..\Test-VanillaBoundary.ps1'
$work = Join-Path $env:TEMP "vanilla-boundary-guard-test-$([guid]::NewGuid())"

function Invoke-Guard([string]$GameDir, [string]$AdmittedBridge = '') {
    # A native child process's stderr, captured via 2>&1 while
    # $ErrorActionPreference is 'Stop', is promoted to a terminating
    # exception in Windows PowerShell 5.1. Keep the parent alive so expected
    # guard failures can be asserted through the child exit code.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $arguments = @(
            '-NoProfile',
            '-ExecutionPolicy', 'Bypass',
            '-File', $guard,
            '-GameDir', $GameDir
        )
        if (-not [string]::IsNullOrWhiteSpace($AdmittedBridge)) {
            $arguments += @('-AdmittedBridge', $AdmittedBridge)
        }
        $out = & powershell @arguments 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    return @{ ExitCode = $exitCode; Output = ($out -join "`n") }
}

function New-GameDir([string]$Name) {
    $dir = Join-Path $work $Name
    New-Item -ItemType Directory -Path (Join-Path $dir 'bin\x64') -Force | Out-Null
    return $dir
}

function Write-Bytes([string]$Path, [byte[]]$Bytes) {
    $parent = Split-Path -Parent $Path
    if ($parent -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    [System.IO.File]::WriteAllBytes($Path, $Bytes)
}

function Require-Pass($Result, [string]$Case) {
    if ($Result.ExitCode -ne 0) {
        throw "$Case was rejected (should pass):`n$($Result.Output)"
    }
    Write-Host "PASS: $Case accepted"
}

function Require-Fail($Result, [string]$Case) {
    if ($Result.ExitCode -eq 0) {
        throw "$Case was accepted (should fail):`n$($Result.Output)"
    }
    Write-Host "PASS: $Case correctly rejected"
}

try {
    New-Item -ItemType Directory -Path $work -Force | Out-Null

    # Trusted build-side artifact. Unit fixtures use deterministic bytes; the
    # Windows integration workflow supplies the real staged #49 version.dll.
    $admittedBridge = Join-Path $work 'admitted\version.dll'
    Write-Bytes $admittedBridge ([byte[]](0x4d, 0x5a, 0x43, 0x50, 0x2d, 0x34, 0x33))

    # 1. Clean game directory: no mod proxy at all remains valid vanilla.
    $cleanDir = New-GameDir 'clean'
    Require-Pass (Invoke-Guard $cleanDir) 'clean fixture'

    # 2. A random/foreign version.dll must fail even though the filename is the
    #    same entry point used by our own bridge.
    $foreignDir = New-GameDir 'foreign-version'
    Write-Bytes (Join-Path $foreignDir 'bin\x64\version.dll') ([byte[]](1, 2, 3, 4, 5))
    Require-Fail (Invoke-Guard $foreignDir $admittedBridge) 'foreign version.dll'

    # 3. Exact byte-for-byte admitted bridge must pass.
    $exactDir = New-GameDir 'exact-bridge'
    $exactInstalled = Join-Path $exactDir 'bin\x64\version.dll'
    Copy-Item -LiteralPath $admittedBridge -Destination $exactInstalled
    $exact = Invoke-Guard $exactDir $admittedBridge
    Require-Pass $exact 'exact admitted bridge'
    if ($exact.Output -notmatch 'admitted bridge SHA-256') {
        throw "exact bridge pass did not record the admitted SHA-256:`n$($exact.Output)"
    }

    # 4. Clean uninstall is just removal of our owned one-file payload. No
    #    version-original.dll needs to be restored; the boundary must return to
    #    the vanilla baseline immediately after deleting our bridge.
    Remove-Item -LiteralPath $exactInstalled -Force
    Require-Pass (Invoke-Guard $exactDir) 'clean uninstall back to vanilla baseline'

    # 5. One-byte mutation must fail closed.
    $mutatedDir = New-GameDir 'mutated-bridge'
    $mutatedPath = Join-Path $mutatedDir 'bin\x64\version.dll'
    Copy-Item -LiteralPath $admittedBridge -Destination $mutatedPath
    $bytes = [System.IO.File]::ReadAllBytes($mutatedPath)
    $bytes[$bytes.Length - 1] = $bytes[$bytes.Length - 1] -bxor 0x01
    [System.IO.File]::WriteAllBytes($mutatedPath, $bytes)
    Require-Fail (Invoke-Guard $mutatedDir $admittedBridge) 'one-byte-mutated bridge'

    # 6. Exact bridge never suppresses forbidden framework evidence.
    $frameworkDir = New-GameDir 'bridge-plus-red4ext'
    Copy-Item -LiteralPath $admittedBridge -Destination (Join-Path $frameworkDir 'bin\x64\version.dll')
    New-Item -ItemType Directory -Path (Join-Path $frameworkDir 'red4ext') -Force | Out-Null
    Require-Fail (Invoke-Guard $frameworkDir $admittedBridge) 'exact bridge plus RED4ext'

    # 7. Historical adjacent forwarding target is forbidden by the one-file
    #    contract even when version.dll itself is exact.
    $staleLayoutDir = New-GameDir 'stale-version-original'
    Copy-Item -LiteralPath $admittedBridge -Destination (Join-Path $staleLayoutDir 'bin\x64\version.dll')
    Write-Bytes (Join-Path $staleLayoutDir 'bin\x64\version-original.dll') ([byte[]](9, 9, 9))
    Require-Fail (Invoke-Guard $staleLayoutDir $admittedBridge) 'stale version-original.dll layout'

    # 8. A version.dll with no explicit trusted artifact must fail closed. The
    #    filename itself is never an allow-list token.
    $unprovenDir = New-GameDir 'unproven-version'
    Copy-Item -LiteralPath $admittedBridge -Destination (Join-Path $unprovenDir 'bin\x64\version.dll')
    Require-Fail (Invoke-Guard $unprovenDir) 'unproven version.dll without admitted artifact'

    # 9. Keep generic ASI rejection from the old #19 guard.
    $asiDir = New-GameDir 'infected-asi'
    Write-Bytes (Join-Path $asiDir 'bin\x64\plugins\some_other_framework.asi') ([byte[]](0x41, 0x53, 0x49))
    Require-Fail (Invoke-Guard $asiDir) 'unrecognized .asi loader'

    Write-Host 'PASS: CP-VANILLA-GUARD-HASH-1 contract cases complete'
} finally {
    if (Test-Path -LiteralPath $work) {
        Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# Expected negative child-process cases leave PowerShell's process-global
# $LASTEXITCODE at 1. The assertions above already consumed those exit codes;
# make the harness result explicit so a fully-passing contract test exits 0.
exit 0
