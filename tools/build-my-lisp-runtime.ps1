[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$moduleRel = "runtime/my-lisp"
$modulePath = Join-Path $repoRoot $moduleRel
$exePath = Join-Path $modulePath "target/release/my-lisp.exe"
$embedPath = Join-Path $modulePath "target/release/my_lisp_embed.dll"
$embedHeaderPath = Join-Path $modulePath "crates/my-lisp-embed/include/my_lisp_embed.h"
$provenancePath = Join-Path $modulePath "target/release/cyberpunk-my-lisp-provenance.txt"

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
    if (-not (Test-Path $embedHeaderPath)) {
        throw "Canonical my-lisp embed header is missing: $embedHeaderPath"
    }

    Push-Location $modulePath
    try {
        & cargo test --release -p my-lisp-embed
        if ($LASTEXITCODE -ne 0) {
            throw "canonical my-lisp embed tests failed"
        }

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

    $myLispSha = (& git -C $modulePath rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $myLispSha -notmatch '^[0-9a-f]{40}$') {
        throw "Unable to resolve exact initialized my-lisp SHA"
    }

    $embedHeader = Get-Content $embedHeaderPath -Raw
    if ($embedHeader -notmatch '#define\s+MY_LISP_EMBED_ABI_VERSION\s+([0-9]+)u?') {
        throw "Unable to resolve MY_LISP_EMBED_ABI_VERSION from canonical upstream header"
    }
    $embedAbi = $Matches[1]

    @(
        "my-lisp-sha=$myLispSha"
        "embed-abi=$embedAbi"
        "embed-contract=crates/my-lisp-embed/include/my_lisp_embed.h"
        "embed-tests=cargo test --release -p my-lisp-embed"
    ) | Set-Content -Path $provenancePath -Encoding utf8

    Write-Output $exePath
    Write-Output $embedPath
    Write-Output $provenancePath
}
finally {
    Pop-Location
}
