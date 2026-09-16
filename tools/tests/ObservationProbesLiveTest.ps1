<#
.SYNOPSIS
    Runs the real memory-probe-v0.ps1 and screen-probe-v0.ps1 scripts
    (not static fixtures) and verifies their actual output still
    roundtrips through canonical my-lisp's --oracle-check as valid
    game-observation/1 data (#28/#29/#30). Catches drift between the
    fixtures in adapter/tests/fixtures/observation/ (hand-authored
    examples) and what the scripts genuinely emit today.
#>

param(
    [string]$MyLispExe = (Join-Path $PSScriptRoot '..\..\runtime\my-lisp\target\release\my-lisp.exe')
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $MyLispExe)) {
    throw "my-lisp.exe not found at $MyLispExe -- run tools/build-my-lisp-runtime.ps1 first"
}

$probes = @(
    (Join-Path $PSScriptRoot '..\memory-probe-v0.ps1'),
    (Join-Path $PSScriptRoot '..\screen-probe-v0.ps1')
)

foreach ($probe in $probes) {
    $tmp = Join-Path $env:TEMP "observation-probe-live-$([guid]::NewGuid()).lisp"
    try {
        & powershell -NoProfile -ExecutionPolicy Bypass -File $probe |
            Out-File -Encoding utf8 -NoNewline -FilePath $tmp

        $record = Get-Content -Raw -LiteralPath $tmp
        if ([string]::IsNullOrWhiteSpace($record)) {
            throw "$probe produced no output"
        }

        $oracle = & $MyLispExe --oracle-check $tmp
        if ($LASTEXITCODE -ne 0 -or $oracle -notmatch '\(outcome valid\)') {
            throw "$probe output failed canonical my-lisp oracle-check (exit=$LASTEXITCODE): $oracle"
        }

        if ($record -notmatch '\(game-observation/1') {
            throw "$probe output does not start a game-observation/1 record: $record"
        }
        if ($record -notmatch '\(validity (valid|unavailable|unsupported-build|desync)\)') {
            throw "$probe output has no recognized validity symbol: $record"
        }

        Write-Host "PASS: $(Split-Path -Leaf $probe) output is valid canonical my-lisp game-observation/1 data"
    } finally {
        if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue }
    }
}

Write-Host "PASS: all live observation probes emit contract-valid, canonical-my-lisp-parseable output"
