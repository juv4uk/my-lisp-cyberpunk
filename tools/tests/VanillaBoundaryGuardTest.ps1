<#
.SYNOPSIS
    Mutation test for tools/Test-VanillaBoundary.ps1 (#19). Proves the
    guard actually rejects a forbidden runtime dependency instead of
    passing vacuously -- the same discipline as
    adapter/tests/CanonSpellingGuardTest.cmake: a clean fixture must pass,
    a deliberately "infected" fixture must fail.
#>

$ErrorActionPreference = 'Stop'
$guard = Join-Path $PSScriptRoot '..\Test-VanillaBoundary.ps1'
$work = Join-Path $env:TEMP "vanilla-boundary-guard-test-$([guid]::NewGuid())"

function Invoke-Guard([string]$GameDir) {
    # A native child process's stderr, captured via 2>&1 while
    # $ErrorActionPreference is 'Stop', is promoted to a terminating
    # exception in PowerShell 5.1 -- fatal here, since the guard's own
    # `throw` on an infected fixture is exactly the expected outcome we
    # need to inspect via $LASTEXITCODE, not have abort this test.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $out = & powershell -NoProfile -ExecutionPolicy Bypass -File $guard -GameDir $GameDir 2>&1
    } finally {
        $ErrorActionPreference = $previous
    }
    return @{ ExitCode = $LASTEXITCODE; Output = ($out -join "`n") }
}

try {
    # 1. Clean fixture: an empty "game directory" must pass -- nothing
    #    forbidden is present because nothing at all is present.
    $cleanDir = Join-Path $work 'clean'
    New-Item -ItemType Directory -Path $cleanDir -Force | Out-Null
    $cleanResult = Invoke-Guard $cleanDir
    if ($cleanResult.ExitCode -ne 0) {
        throw "clean fixture was rejected (should pass):`n$($cleanResult.Output)"
    }
    Write-Host "PASS: clean fixture accepted"

    # 2. Infected fixture: plant a red4ext directory, exactly the forbidden
    #    dependency #19's table names first. The guard must reject this.
    $infectedDir = Join-Path $work 'infected-red4ext'
    New-Item -ItemType Directory -Path (Join-Path $infectedDir 'red4ext') -Force | Out-Null
    $infectedResult = Invoke-Guard $infectedDir
    if ($infectedResult.ExitCode -eq 0) {
        throw "infected fixture (red4ext/ present) was accepted -- guard does not actually detect it"
    }
    Write-Host "PASS: red4ext/ presence correctly rejected"

    # 3. A second, differently-shaped infection: an unrecognized .asi
    #    loader under bin\x64\plugins, the generic catch-all case.
    $asiDir = Join-Path $work 'infected-asi'
    New-Item -ItemType Directory -Path (Join-Path $asiDir 'bin\x64\plugins') -Force | Out-Null
    Set-Content -Path (Join-Path $asiDir 'bin\x64\plugins\some_other_framework.asi') -Value 'not a real asi, just needs to exist'
    $asiResult = Invoke-Guard $asiDir
    if ($asiResult.ExitCode -eq 0) {
        throw "infected fixture (unrecognized .asi present) was accepted -- guard does not actually detect it"
    }
    Write-Host "PASS: unrecognized .asi loader correctly rejected"

    Write-Host "PASS: vanilla boundary guard mutation test (clean accepted, two distinct infections rejected)"
} finally {
    if (Test-Path -LiteralPath $work) {
        Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
    }
}
