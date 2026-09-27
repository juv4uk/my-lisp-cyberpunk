#Requires -Version 5.1
<#
.SYNOPSIS
    CP-PROBE-SCREEN-V0 (issue #30) mechanism-only skeleton: gathers raw
    facts (window found?, client resolution, build fingerprint) and
    hands the decision of what they MEAN to canonical my-lisp
    (tools/observation/observation-policy.lisp), the same policy
    memory-probe-v0.ps1 uses for #29.

.DESCRIPTION
    Does not recognize any HUD value yet -- see docs/research/
    vanilla-entrypoints.md and the #30 issue thread for why shipping
    unvalidated template/glyph thresholds without a live game to
    calibrate against would risk a plausible-looking wrong digit. This
    script reports only the raw booleans (window-found?, known-profile?)
    and calibration identity (client resolution); Lisp decides
    `validity`, per docs/observation-contract-v0.md.

.OUTPUTS
    Prints one game-observation/1 s-expression to stdout, produced by
    canonical my-lisp, not by this script.
#>

param(
    [string]$ProcessName = "Cyberpunk2077",
    [string]$MyLispExe = (Join-Path $PSScriptRoot "..\runtime\my-lisp\target\release\my-lisp.exe"),
    [string]$PolicyFile = (Join-Path $PSScriptRoot "observation\observation-policy.lisp")
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $MyLispExe)) {
    throw "my-lisp.exe not found at $MyLispExe -- run tools/build-my-lisp-runtime.ps1 first"
}
if (-not (Test-Path -LiteralPath $PolicyFile)) {
    throw "observation policy not found at $PolicyFile"
}

Add-Type -Namespace Win32Probe -Name NativeMethods -MemberDefinition @"
    [System.Runtime.InteropServices.DllImport("user32.dll")]
    public static extern bool GetClientRect(System.IntPtr hWnd, out RECT lpRect);

    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
"@

function Invoke-ObservationPolicy {
    param(
        [string]$Fingerprint,
        [long]$Timestamp,
        [bool]$WindowFound,
        [bool]$KnownProfile,
        [string]$ProvenanceBody
    )
    $windowFoundLisp = if ($WindowFound) { "t" } else { "()" }
    $knownProfileLisp = if ($KnownProfile) { "t" } else { "()" }
    $call = "(make-observation (quote screen) `"$Fingerprint`" (quote player-health-ratio) (quote ()) $Timestamp $windowFoundLisp $knownProfileLisp (quote $ProvenanceBody))"

    $driver = Join-Path $env:TEMP "observation-drive-$([guid]::NewGuid()).lisp"
    try {
        Get-Content -Raw -LiteralPath $PolicyFile | Out-File -Encoding ascii -NoNewline -FilePath $driver
        Add-Content -Path $driver -Value "`n$call" -Encoding ascii
        & $MyLispExe $driver
        if ($LASTEXITCODE -ne 0) {
            throw "canonical my-lisp policy evaluation failed (exit=$LASTEXITCODE)"
        }
    } finally {
        if (Test-Path -LiteralPath $driver) { Remove-Item -LiteralPath $driver -Force -ErrorAction SilentlyContinue }
    }
}

$timestamp = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Select-Object -First 1

if (-not $proc -or $proc.MainWindowHandle -eq [System.IntPtr]::Zero) {
    Invoke-ObservationPolicy `
        -Fingerprint "unknown" -Timestamp $timestamp -WindowFound $false -KnownProfile $false `
        -ProvenanceBody "((channel-revision 1) (reason `"no visible $ProcessName window found`"))"
    exit 0
}

$rect = New-Object Win32Probe.NativeMethods+RECT
$ok = [Win32Probe.NativeMethods]::GetClientRect($proc.MainWindowHandle, [ref]$rect)
if (-not $ok) {
    Invoke-ObservationPolicy `
        -Fingerprint "unknown" -Timestamp $timestamp -WindowFound $false -KnownProfile $false `
        -ProvenanceBody "((channel-revision 1) (reason `"GetClientRect failed on the found window handle`"))"
    exit 0
}

$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
$productVersion = $proc.MainModule.FileVersionInfo.ProductVersion
if (-not $productVersion) { $productVersion = "unknown" }
$fingerprint = "cp2077-$productVersion-x64"

# No HUD region/template calibration exists yet for any resolution or
# UI scale -- a raw fact (do we have a profile for this fingerprint?),
# not a validity decision. observation-policy.lisp turns
# (window-found=t, known-profile=()) into `unsupported-build` on its own.
Invoke-ObservationPolicy `
    -Fingerprint $fingerprint -Timestamp $timestamp -WindowFound $true -KnownProfile $false `
    -ProvenanceBody "((channel-revision 1) (client-width $width) (client-height $height) (reason `"no HUD template/glyph calibration exists yet for this resolution (see issue #30)`"))"
