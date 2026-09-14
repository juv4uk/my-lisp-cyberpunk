[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [string]$GameDir,

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

if (Get-Process -Name Cyberpunk2077 -ErrorAction SilentlyContinue) {
    throw 'Cyberpunk2077 is running. Close the game before installing the plugin payload.'
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$gameRoot = (Resolve-Path -LiteralPath $GameDir).Path
$buildRoot = Join-Path $repoRoot "adapter\\build\\src\\$Configuration"
$pluginRoot = Join-Path $gameRoot 'red4ext\\plugins\\wsm-my-lisp-cyberpunk-plugin'
$scriptsRoot = Join-Path $gameRoot 'r6\\scripts'

$runtimeCandidates = @(
    (Join-Path $repoRoot 'host-runtime\\target\\release\\wsm_my_lisp_cyberpunk_dll.dll'),
    (Join-Path $repoRoot "host-runtime\\target\\x86_64-pc-windows-msvc\\$Configuration\\wsm_my_lisp_cyberpunk_dll.dll")
)
$runtime = $runtimeCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if ($null -eq $runtime) {
    throw 'No built host-runtime DLL was found. Build host-runtime before deployment.'
}

$payloads = @(
    [pscustomobject]@{ Name = 'adapter DLL'; Source = Join-Path $buildRoot 'my-lisp-cyberpunk-plugin.dll'; Destination = Join-Path $pluginRoot 'my-lisp-cyberpunk-plugin.dll'; Directory = $false },
    [pscustomobject]@{ Name = 'host runtime DLL'; Source = $runtime; Destination = Join-Path $pluginRoot 'wsm_my_lisp_cyberpunk_dll.dll'; Directory = $false },
    [pscustomobject]@{ Name = 'Lisp scenarios'; Source = Join-Path $buildRoot 'scripts'; Destination = Join-Path $pluginRoot 'scripts'; Directory = $true },
    [pscustomobject]@{ Name = 'NeuralDeck Redscript'; Source = Join-Path $buildRoot 'redscript'; Destination = $scriptsRoot; Directory = $true }
)

foreach ($payload in $payloads) {
    if (-not (Test-Path -LiteralPath $payload.Source)) {
        throw "Missing $($payload.Name): $($payload.Source)"
    }
}

if ($WhatIfPreference) {
    return $payloads
}

foreach ($payload in $payloads) {
    if (-not $PSCmdlet.ShouldProcess($payload.Destination, "Copy $($payload.Name) from $($payload.Source)")) {
        continue
    }

    if ($payload.Directory) {
        New-Item -ItemType Directory -Force $payload.Destination | Out-Null
        Get-ChildItem -LiteralPath $payload.Source | Copy-Item -Destination $payload.Destination -Recurse -Force
    } else {
        New-Item -ItemType Directory -Force (Split-Path -Parent $payload.Destination) | Out-Null
        Copy-Item -LiteralPath $payload.Source -Destination $payload.Destination -Force
        $sourceHash = (Get-FileHash -LiteralPath $payload.Source -Algorithm SHA256).Hash
        $destinationHash = (Get-FileHash -LiteralPath $payload.Destination -Algorithm SHA256).Hash
        if ($sourceHash -ne $destinationHash) {
            throw "Hash mismatch after copying $($payload.Name)"
        }
        Write-Host "verified $($payload.Name): $destinationHash"
    }
}

Write-Host 'NeuralDeck payload installation completed.'
