param(
    [Parameter(Mandatory = $true)]
    [string]$MyLispExe,

    [Parameter(Mandatory = $true)]
    [string]$BridgeDll,

    [Parameter(Mandatory = $true)]
    [string]$Corpus,

    [Parameter(Mandatory = $true)]
    [string]$Report
)

$ErrorActionPreference = 'Stop'

$runner = Join-Path (Split-Path -Parent $PSScriptRoot) 'Invoke-ConsumerConformance.ps1'
if (-not (Test-Path $runner)) {
    throw "consumer conformance runner is missing: $runner"
}

& $runner `
    -MyLispExe $MyLispExe `
    -BridgeDll $BridgeDll `
    -Corpus $Corpus `
    -Report $Report

if (-not (Test-Path $Report)) {
    throw "consumer conformance report was not created: $Report"
}

$text = Get-Content -Raw $Report
if ($text -notmatch '\(consumer-conformance-report/1\b') {
    throw 'consumer conformance report has no protocol record'
}

$expectedCases = @(
    'add-symbolic',
    'car-english',
    'car-ukrainian',
    'car-non-pair-error'
)

foreach ($caseId in $expectedCases) {
    $escaped = [regex]::Escape($caseId)
    if ($text -notmatch "\(case \"$escaped\"") {
        throw "consumer conformance report is missing case: $caseId"
    }
}

$equalCount = ([regex]::Matches($text, '\(parity equal\)')).Count
if ($equalCount -ne $expectedCases.Count) {
    throw "expected $($expectedCases.Count) parity-equal cases, found $equalCount"
}

$oracle = & $MyLispExe --oracle-check $Report
$oracleText = $oracle -join "`n"
Write-Output $oracleText
if ($LASTEXITCODE -ne 0 -or $oracleText -notmatch '\(outcome valid\)') {
    throw 'consumer conformance report is not valid canonical my-lisp data'
}

Write-Output "consumer conformance contract witness OK: $equalCount/$($expectedCases.Count) cases"
