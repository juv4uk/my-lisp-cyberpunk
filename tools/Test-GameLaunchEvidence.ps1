[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$GameDir,

    [switch]$RequireF10
)

$ErrorActionPreference = 'Stop'

function Require-Text([string]$Text, [string]$Needle, [string]$Name) {
    if (-not $Text.Contains($Needle)) {
        throw "Missing ${Name}: $Needle"
    }
    Write-Host "PASS: $Name"
}

$gameRoot = (Resolve-Path -LiteralPath $GameDir).Path
$red4extLogs = Join-Path $gameRoot 'red4ext\logs'
$pluginLog = Get-ChildItem -LiteralPath $red4extLogs -Filter 'my-lisp-cyberpunk-plugin-*.log' |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
$red4extLog = Get-ChildItem -LiteralPath $red4extLogs -Filter 'red4ext-*.log' |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
$codewareLog = Get-ChildItem -LiteralPath (Join-Path $gameRoot 'red4ext\plugins\Codeware') -Filter 'Codeware-*.log' |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1

if ($null -eq $pluginLog -or $null -eq $red4extLog -or $null -eq $codewareLog) {
    throw 'Missing one or more launch logs. Start the game once before running this test.'
}

$red4extText = Get-Content -LiteralPath $red4extLog.FullName -Raw
$pluginText = Get-Content -LiteralPath $pluginLog.FullName -Raw
$codewareText = Get-Content -LiteralPath $codewareLog.FullName -Raw
$redscriptText = Get-Content -LiteralPath (Join-Path $gameRoot 'r6\logs\redscript_rCURRENT.log') -Raw

Require-Text $red4extText 'my-lisp-cyberpunk (version:' 'RED4ext loaded our plugin'
Require-Text $red4extText 'RED4ext has been started' 'RED4ext reached startup completion'
Require-Text $codewareText 'Codeware is initialized' 'Codeware initialized'
Require-Text $redscriptText 'NeuralDeck\NeuralDeckOverlay.reds' 'Redscript saw NeuralDeck source'
Require-Text $redscriptText 'Compilation complete' 'Redscript compilation completed'
Require-Text $pluginText 'session ready' 'my-lisp host session initialized'

if ($RequireF10) {
    Require-Text $pluginText 'NeuralDeck F10 edge' 'F10 reached the NeuralDeck adapter'
}

Write-Host "PASS: launch evidence verified from $($pluginLog.Name)"
