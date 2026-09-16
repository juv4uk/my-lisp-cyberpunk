$ErrorActionPreference = "Stop"

$scriptPath = Join-Path $PSScriptRoot "../tools/build-my-lisp-runtime.ps1"
$gitmodulesPath = Join-Path $PSScriptRoot "../.gitmodules"
$expectedUrl = "https://github.com/juv4uk/my-lisp.git"

if (-not (Test-Path $scriptPath)) {
    throw "Missing runtime build script: $scriptPath"
}

$actualUrl = git config -f $gitmodulesPath --get 'submodule.runtime/my-lisp.url'
if ($LASTEXITCODE -ne 0 -or $actualUrl -ne $expectedUrl) {
    throw "runtime/my-lisp must be a submodule pinned to $expectedUrl"
}
