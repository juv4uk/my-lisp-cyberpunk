#Requires -Version 5.1
<#
.SYNOPSIS
    CP-PROBE-SCREEN-V0 (issue #30) mechanism-only skeleton: window
    discovery and calibration-identity capture, emitting a
    game-observation/1 record (docs/observation-contract-v0.md, #28).

.DESCRIPTION
    Mirrors tools/memory-probe-v0.ps1's honesty discipline for the
    SCREEN family: this does NOT recognize any HUD value yet. It finds
    the game's window (no OpenProcess/ReadProcessMemory -- window
    handles and window rects are a distinct, memory-access-free Win32
    surface) and records the calibration identity (window client-area
    resolution) that any future glyph/template matcher would need to
    be valid for.

    Building a template/glyph recognizer without a live game to
    calibrate it against would mean shipping unvalidated thresholds
    that could return a plausible-looking wrong digit -- exactly the
    silent-lie failure mode #27's fail-closed discipline forbids. That
    step is deferred to a live session; see the #30 issue comment.

.OUTPUTS
    Prints one game-observation/1 s-expression to stdout.
#>

param(
    [string]$ProcessName = "Cyberpunk2077"
)

$ErrorActionPreference = "Stop"

Add-Type -Namespace Win32Probe -Name NativeMethods -MemberDefinition @"
    [System.Runtime.InteropServices.DllImport("user32.dll")]
    public static extern bool GetClientRect(System.IntPtr hWnd, out RECT lpRect);

    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
"@

function Format-Observation {
    param(
        [string]$Fingerprint,
        [string]$Validity,
        [string]$ProvenanceBody
    )
    $timestamp = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    return "(game-observation/1 (source screen) (game-fingerprint `"$Fingerprint`") (fact player-health-ratio) (value ()) (timestamp $timestamp) (validity $Validity) (provenance $ProvenanceBody))"
}

$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Select-Object -First 1

if (-not $proc -or $proc.MainWindowHandle -eq [System.IntPtr]::Zero) {
    Format-Observation `
        -Fingerprint "unknown" `
        -Validity "unavailable" `
        -ProvenanceBody "(channel-revision 1) (reason `"no visible $ProcessName window found`")"
    exit 0
}

$rect = New-Object Win32Probe.NativeMethods+RECT
$ok = [Win32Probe.NativeMethods]::GetClientRect($proc.MainWindowHandle, [ref]$rect)
if (-not $ok) {
    Format-Observation `
        -Fingerprint "unknown" `
        -Validity "unavailable" `
        -ProvenanceBody "(channel-revision 1) (reason `"GetClientRect failed on the found window handle`")"
    exit 0
}

$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
$productVersion = $proc.MainModule.FileVersionInfo.ProductVersion
if (-not $productVersion) { $productVersion = "unknown" }
$fingerprint = "cp2077-$productVersion-x64"

# No HUD region/template calibration exists yet for any resolution or UI
# scale -- see the #30 issue comment for why this is deferred to a live
# session rather than shipped with guessed thresholds.
Format-Observation `
    -Fingerprint $fingerprint `
    -Validity "unsupported-build" `
    -ProvenanceBody "(channel-revision 1) (client-width $width) (client-height $height) (reason `"no HUD template/glyph calibration exists yet for this resolution (see issue #30)`")"
