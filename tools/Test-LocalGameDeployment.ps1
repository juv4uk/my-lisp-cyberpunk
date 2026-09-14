[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$GameDir,

    [string]$RepoRoot = (Split-Path -Parent $PSScriptRoot),

    [switch]$RequireFreshAdapter
)

$ErrorActionPreference = 'Stop'

function Require-Path([string]$Path, [string]$Name) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing ${Name}: $Path"
    }
    Write-Host "PASS: $Name"
}

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

$gameRoot = (Resolve-Path -LiteralPath $GameDir).Path
$repoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
$pluginRoot = Join-Path $gameRoot 'red4ext\plugins\wsm-my-lisp-cyberpunk-plugin'

Require-Path (Join-Path $gameRoot 'bin\x64\Cyberpunk2077.exe') 'Cyberpunk executable'
Require-Path (Join-Path $gameRoot 'red4ext\RED4ext.dll') 'RED4ext runtime'
Require-Path (Join-Path $gameRoot 'red4ext\plugins\Codeware\Codeware.dll') 'Codeware dependency'
Require-Path (Join-Path $pluginRoot 'my-lisp-cyberpunk-plugin.dll') 'Cyberpunk adapter DLL'
Require-Path (Join-Path $pluginRoot 'wsm_my_lisp_cyberpunk_dll.dll') 'my-lisp host runtime DLL'
Require-Path (Join-Path $pluginRoot 'scripts\dispatcher.lisp') 'fixed dispatch scenario'
Require-Path (Join-Path $gameRoot 'r6\scripts\NeuralDeck\NeuralDeckOverlay.reds') 'NeuralDeck Redscript'

if ($RequireFreshAdapter) {
    $builtAdapter = Join-Path $repoRoot 'adapter\build\src\Release\my-lisp-cyberpunk-plugin.dll'
    $installedAdapter = Join-Path $pluginRoot 'my-lisp-cyberpunk-plugin.dll'
    Require-Path $builtAdapter 'built adapter DLL'
    if ((Get-Sha256 $builtAdapter) -ne (Get-Sha256 $installedAdapter)) {
        throw 'Installed adapter differs from the current Release build. Close the game and run Install-LocalGame.ps1.'
    }
    Write-Host 'PASS: installed adapter matches current Release build'
}

Write-Host "PASS: deployment layout verified at $gameRoot"
