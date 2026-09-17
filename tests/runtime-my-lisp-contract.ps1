$ErrorActionPreference = "Stop"

$scriptPath = Join-Path $PSScriptRoot "../tools/build-my-lisp-runtime.ps1"
$gitmodulesPath = Join-Path $PSScriptRoot "../.gitmodules"
$expectedUrl = "https://github.com/juv4uk/my-lisp.git"
$runtimeRel = "runtime/my-lisp"
$hostRuntimeRel = "host-runtime/external/my-lisp"

if (-not (Test-Path $scriptPath)) {
    throw "Missing runtime build script: $scriptPath"
}

foreach ($moduleRel in @($runtimeRel, $hostRuntimeRel)) {
    $actualUrl = git config -f $gitmodulesPath --get "submodule.$moduleRel.url"
    if ($LASTEXITCODE -ne 0 -or $actualUrl -ne $expectedUrl) {
        throw "$moduleRel must be a submodule pinned to $expectedUrl"
    }
}

function Get-GitlinkSha([string]$moduleRel) {
    $line = git ls-tree HEAD -- $moduleRel
    if ($LASTEXITCODE -ne 0 -or -not $line) {
        throw "Unable to read gitlink from HEAD: $moduleRel"
    }

    if ($line -notmatch '^160000 commit ([0-9a-f]{40})\t') {
        throw "$moduleRel is not a commit gitlink in HEAD: $line"
    }

    return $Matches[1]
}

$runtimeSha = Get-GitlinkSha $runtimeRel
$hostRuntimeSha = Get-GitlinkSha $hostRuntimeRel

if ($runtimeSha -ne $hostRuntimeSha) {
    throw "Split my-lisp semantic revisions are forbidden: $runtimeRel=$runtimeSha; $hostRuntimeRel=$hostRuntimeSha"
}

Write-Output "canonical-my-lisp-sha=$runtimeSha"
