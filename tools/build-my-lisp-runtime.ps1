[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$moduleRel = "runtime/my-lisp"
$modulePath = Join-Path $repoRoot $moduleRel
$exePath = Join-Path $modulePath "target/release/my-lisp.exe"
$embedPath = Join-Path $modulePath "target/release/my_lisp_embed.dll"

Push-Location $repoRoot
try {
    & git submodule sync -- $moduleRel
    if ($LASTEXITCODE -ne 0) {
        throw "git submodule sync failed for $moduleRel"
    }

    & git submodule update --init -- $moduleRel
    if ($LASTEXITCODE -ne 0) {
        throw "git submodule update failed for $moduleRel"
    }

    if (-not (Test-Path (Join-Path $modulePath "Cargo.toml"))) {
        throw "my-lisp submodule is not initialized at $modulePath"
    }

    Push-Location $modulePath
    try {
        & cargo build --release -p my-lisp-cli --bin my-lisp
        if ($LASTEXITCODE -ne 0) {
            throw "cargo build failed for my-lisp CLI runtime"
        }

        & cargo build --release -p my-lisp-embed
        if ($LASTEXITCODE -ne 0) {
            throw "cargo build failed for canonical my-lisp embed runtime"
        }
    }
    finally {
        Pop-Location
    }

    if (-not (Test-Path $exePath)) {
        throw "Expected runtime CLI was not produced: $exePath"
    }
    if (-not (Test-Path $embedPath)) {
        throw "Expected canonical embed DLL was not produced: $embedPath"
    }

    Write-Output $exePath
    Write-Output $embedPath
}
finally {
    Pop-Location
}
