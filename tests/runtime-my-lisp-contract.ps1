$ErrorActionPreference = "Stop"

$scriptPath = Join-Path $PSScriptRoot "../tools/build-my-lisp-runtime.ps1"
$modulePath = Join-Path $PSScriptRoot "../runtime/my-lisp"

if (-not (Test-Path $scriptPath)) {
    throw "Missing runtime build script: $scriptPath"
}

if (-not (Test-Path $modulePath)) {
    throw "Missing my-lisp submodule checkout: $modulePath"
}
