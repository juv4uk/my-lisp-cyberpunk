$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$installer = Join-Path $repoRoot 'tools/Install-LocalGame.ps1'
$gameRoot = 'D:\games\Cyberpunk 2077 v.2.31 (2020)\Cyberpunk 2077'

$plan = & $installer -GameDir $gameRoot -WhatIf
$actual = @($plan | Where-Object { $_.PSObject.Properties.Name -contains 'Name' } | ForEach-Object Name)

foreach ($expected in @(
    'adapter DLL',
    'host runtime DLL',
    'Lisp scenarios',
    'NeuralDeck Redscript'
)) {
    if ($actual -notcontains $expected) {
        throw "Dry-run deployment did not include $expected"
    }
}

Write-Host 'PASS: complete NeuralDeck payload is planned for deployment.'

$testGameRoot = Join-Path $env:TEMP "my-lisp-cyberpunk-install-test-$PID"
try {
    New-Item -ItemType Directory -Force (Join-Path $testGameRoot 'red4ext\\plugins\\wsm-my-lisp-cyberpunk-plugin') | Out-Null
    New-Item -ItemType Directory -Force (Join-Path $testGameRoot 'r6\\scripts') | Out-Null

    & $installer -GameDir $testGameRoot

    foreach ($expected in @(
        'red4ext\\plugins\\wsm-my-lisp-cyberpunk-plugin\\my-lisp-cyberpunk-plugin.dll',
        'red4ext\\plugins\\wsm-my-lisp-cyberpunk-plugin\\wsm_my_lisp_cyberpunk_dll.dll',
        'red4ext\\plugins\\wsm-my-lisp-cyberpunk-plugin\\scripts\\dispatcher.lisp',
        'r6\\scripts\\NeuralDeck\\NeuralDeckOverlay.reds'
    )) {
        if (-not (Test-Path -LiteralPath (Join-Path $testGameRoot $expected))) {
            throw "Deployment did not copy $expected"
        }
    }

    Write-Host 'PASS: complete NeuralDeck payload is installed.'
} finally {
    Remove-Item -LiteralPath $testGameRoot -Recurse -Force -ErrorAction SilentlyContinue
}
