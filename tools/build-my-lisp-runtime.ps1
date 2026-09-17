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
$staticPath = $null
$nativeStaticLibs = @()

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

        # Reuse the exact artifact-discovery mechanism proven upstream by
        # my-lisp#316.  Cargo, not a guessed filename, tells this consumer
        # which output is the canonical staticlib from the pinned tree.
        $artifactLines = & cargo build --release -p my-lisp-embed --message-format=json-render-diagnostics
        if ($LASTEXITCODE -ne 0) {
            throw "cargo build failed for canonical my-lisp embed runtime"
        }

        $artifactRows = @()
        foreach ($line in $artifactLines) {
            try {
                $row = $line | ConvertFrom-Json -ErrorAction Stop
            }
            catch {
                continue
            }
            if ($row.reason -eq 'compiler-artifact' -and $row.target.name -eq 'my_lisp_embed') {
                $artifactRows += $row
            }
        }
        if ($artifactRows.Count -eq 0) {
            throw "Cargo emitted no compiler-artifact row for my_lisp_embed"
        }

        $filenames = @($artifactRows | ForEach-Object { $_.filenames } | Sort-Object -Unique)
        $staticCandidates = @($filenames | Where-Object {
            $_ -match '\.lib$' -and $_ -notmatch '\.dll\.lib$'
        })
        if ($staticCandidates.Count -ne 1) {
            throw "Expected exactly one Cargo-reported my-lisp-embed static .lib, found $($staticCandidates.Count)"
        }
        $staticPath = (Resolve-Path $staticCandidates[0]).Path

        # Keep native linker inputs authoritative as well.  Rust owns these
        # requirements; Cyberpunk must not maintain a copied Windows library list.
        # GitHub's Rust setup enables colored Cargo output globally; rustc's
        # machine-consumed native-static-libs line must be plain text, exactly as
        # the upstream my-lisp#316 witness requests it.
        $previousCargoColor = $env:CARGO_TERM_COLOR
        try {
            $env:CARGO_TERM_COLOR = 'never'
            $nativeOutput = @(& cargo rustc --release -p my-lisp-embed --lib -- --print native-static-libs 2>&1 | ForEach-Object { "$_" })
        }
        finally {
            if ($null -eq $previousCargoColor) {
                Remove-Item Env:CARGO_TERM_COLOR -ErrorAction SilentlyContinue
            }
            else {
                $env:CARGO_TERM_COLOR = $previousCargoColor
            }
        }
        if ($LASTEXITCODE -ne 0) {
            throw "rustc failed while reporting native-static-libs for my-lisp-embed"
        }
        $nativeLines = @($nativeOutput | Where-Object { $_ -match 'native-static-libs:\s*(.+)$' })
        if ($nativeLines.Count -eq 0) {
            throw "rustc did not report native-static-libs for my-lisp-embed"
        }
        $nativeMatch = [regex]::Match($nativeLines[-1], 'native-static-libs:\s*(.+)$')
        $nativeStaticLibs = @($nativeMatch.Groups[1].Value.Trim() -split '\s+' | Where-Object { $_ })
        if ($nativeStaticLibs.Count -eq 0) {
            throw "rustc native-static-libs report was empty"
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
    if ([string]::IsNullOrWhiteSpace($staticPath) -or -not (Test-Path $staticPath)) {
        throw "Expected canonical embed static library was not resolved"
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
        "linkage=static"
        "static-artifact=$staticPath"
        "native-static-libs=$($nativeStaticLibs -join ';')"
    ) | Set-Content -Path $provenancePath -Encoding utf8

    Write-Output $exePath
    Write-Output $embedPath
    Write-Output $staticPath
    Write-Output $provenancePath
}
finally {
    Pop-Location
}
