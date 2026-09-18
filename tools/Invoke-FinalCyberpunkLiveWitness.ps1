[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$GameDir,
  [Parameter(Mandatory)][string]$BridgeArtifact,
  [string]$MyLispExe = (Join-Path $PSScriptRoot '..\runtime\my-lisp\target\release\my-lisp.exe'),
  [string]$EvidenceOut = '',
  [string]$TranscriptOut = '',
  [ValidateRange(1,300)][int]$ObservationTimeoutSeconds = 30,
  [ValidateRange(1,65535)][int]$Port = 40777
)

$ErrorActionPreference = 'Stop'

function Get-Sha256Hex([string]$Path) {
  $s=[IO.File]::OpenRead($Path); $h=[Security.Cryptography.SHA256]::Create()
  try { $b=$h.ComputeHash($s) } finally { $s.Dispose(); $h.Dispose() }
  ([BitConverter]::ToString($b).Replace('-','').ToLowerInvariant())
}
function AbsOut([string]$Path,[string]$Default) {
  if ([string]::IsNullOrWhiteSpace($Path)) { $Path=Join-Path (Get-Location).Path $Default }
  elseif (-not [IO.Path]::IsPathRooted($Path)) { $Path=Join-Path (Get-Location).Path $Path }
  [IO.Path]::GetFullPath($Path)
}
function Under([string]$Path,[string]$Root) {
  $r=[IO.Path]::GetFullPath($Root).TrimEnd('\'); $p=[IO.Path]::GetFullPath($Path)
  $p.Equals($r,[StringComparison]::OrdinalIgnoreCase) -or $p.StartsWith($r+'\',[StringComparison]::OrdinalIgnoreCase)
}
function Log([string]$m) {
  $l='[{0}] {1}' -f [DateTimeOffset]::Now.ToString('o'),$m
  Write-Host $l; Add-Content -LiteralPath $script:TranscriptOut -Value $l -Encoding utf8
}
function Repl([string[]]$Forms) {
  $c=$null; $deadline=[DateTime]::UtcNow.AddSeconds(15)
  do {
    $c=[Net.Sockets.TcpClient]::new()
    try { $a=$c.ConnectAsync('127.0.0.1',$Port); if ($a.Wait(250) -and $c.Connected) { break } } catch {}
    $c.Dispose(); $c=$null
    if ([DateTime]::UtcNow -ge $deadline) { throw "REPL not reachable on 127.0.0.1:$Port" }
    Start-Sleep -Milliseconds 150
  } while ($true)
  $e=[Text.UTF8Encoding]::new($false); $st=$c.GetStream()
  $rd=[IO.StreamReader]::new($st,$e,$false,4096,$true); $wr=[IO.StreamWriter]::new($st,$e,4096,$true)
  $wr.NewLine="`n"; $wr.AutoFlush=$true; $out=[Collections.Generic.List[string]]::new()
  try { foreach($f in $Forms){ Log "REPL > $f"; $wr.WriteLine($f); $r=$rd.ReadLine(); if($null -eq $r){throw 'REPL closed'}; Log "REPL < $r"; $out.Add($r) } }
  finally { $wr.Dispose(); $rd.Dispose(); $st.Dispose(); $c.Dispose() }
  @($out)
}

$gameRoot=(Resolve-Path -LiteralPath $GameDir).Path; $gameBin=Join-Path $gameRoot 'bin\x64'
$gameExe=Join-Path $gameBin 'Cyberpunk2077.exe'; $installedBridge=Join-Path $gameBin 'version.dll'
$observationPath=Join-Path $gameBin 'bridge-observation.lisp'; $boundary=Join-Path $PSScriptRoot 'Test-VanillaBoundary.ps1'
$probe=Join-Path $PSScriptRoot 'Test-LiveCyberpunkProbe.ps1'
foreach($f in @($gameExe,$BridgeArtifact,$MyLispExe,$boundary,$probe)){ if(-not(Test-Path -LiteralPath $f -PathType Leaf)){ throw "required file absent: $f" } }
$trustedBridge=(Resolve-Path -LiteralPath $BridgeArtifact).Path; $myLisp=(Resolve-Path -LiteralPath $MyLispExe).Path
if($trustedBridge.Equals([IO.Path]::GetFullPath($installedBridge),[StringComparison]::OrdinalIgnoreCase)){ throw 'trusted bridge must be separate from deployed version.dll' }
$stamp=[DateTimeOffset]::Now.ToString('yyyyMMdd-HHmmss')
$EvidenceOut=AbsOut $EvidenceOut "cyberpunk-final-live-evidence-$stamp.lisp"; $TranscriptOut=AbsOut $TranscriptOut "cyberpunk-final-live-transcript-$stamp.txt"; $script:TranscriptOut=$TranscriptOut
if(Under $EvidenceOut $gameRoot){throw "EvidenceOut must be outside game dir: $EvidenceOut"}; if(Under $TranscriptOut $gameRoot){throw "TranscriptOut must be outside game dir: $TranscriptOut"}
foreach($o in @($EvidenceOut,$TranscriptOut)){ $p=Split-Path -Parent $o; if(-not(Test-Path $p)){New-Item -ItemType Directory -Force -Path $p|Out-Null} }
Set-Content -LiteralPath $TranscriptOut -Value '' -Encoding utf8

$trustedHash=Get-Sha256Hex $trustedBridge; $deployed=$false; $gameProcess=$null; $primary=$null; $cleanup=$null
$wrongTrust=Join-Path (Split-Path -Parent $EvidenceOut) "negative-wrong-trust-$stamp.version.dll"; $negativeEvidence=Join-Path (Split-Path -Parent $EvidenceOut) "negative-wrong-trust-$stamp.lisp"
try {
  Log "START trusted-sha256=$trustedHash"
  & $boundary -GameDir $gameRoot; Log 'PASS clean vanilla boundary before deploy'
  if(Test-Path -LiteralPath $observationPath){Remove-Item -LiteralPath $observationPath -Force; Log 'removed stale bridge-observation.lisp'}
  Copy-Item -LiteralPath $trustedBridge -Destination $installedBridge; $deployed=$true
  $installedHash=Get-Sha256Hex $installedBridge; if($installedHash -ne $trustedHash){throw "SHA mismatch installed=$installedHash trusted=$trustedHash"}
  & $boundary -GameDir $gameRoot -AdmittedBridge $trustedBridge; Log "PASS exact deployed version.dll sha256=$installedHash"

  $gameProcess=Start-Process -FilePath $gameExe -WorkingDirectory $gameBin -PassThru; Log "launched Cyberpunk2077.exe pid=$($gameProcess.Id)"
  $menu=Read-Host 'Game startup check: when normal startup/menu is visible, type MENU'; if($menu -cne 'MENU'){throw 'startup/menu confirmation missing'}; Log 'PASS human MENU confirmation'

  $po=& $probe -GameDir $gameRoot -BridgeArtifact $trustedBridge -ProcessId $gameProcess.Id -MyLispExe $myLisp -EvidenceOut $EvidenceOut -ObservationTimeoutSeconds $ObservationTimeoutSeconds 2>&1
  foreach($line in @($po)){Log ('probe> '+$line.ToString())}
  $et=Get-Content -Raw -LiteralPath $EvidenceOut; if($et -notmatch '\(target-mode live-cyberpunk\)'){throw 'evidence lacks (target-mode live-cyberpunk)'}
  if($et -notmatch ('\(process-id\s+'+[regex]::Escape([string]$gameProcess.Id)+'\)')){throw 'evidence PID differs from Start-Process PID'}; Log 'PASS exact PID/module/SHA live-cyberpunk evidence'

  # Reuse exact closure forms already proven by the pinned my-lisp-embed
  # tests and the standalone bridge harness.  Expected values here are not a
  # second semantic oracle: they are the same accepted upstream witness used
  # by this exact pinned canonical Session.
  $a=Repl @(
    '(визначити x31 42)',
    'x31',
    '(визначити подвоїти (функція (значення) (+ значення значення)))',
    '(подвоїти 21)'
  )
  if($a.Count -ne 4 -or $a[0] -cne '42' -or $a[1] -cne '42' -or $a[2] -cne '<lambda>' -or $a[3] -cne '42'){
    throw 'initial Ukrainian value/closure Session witness failed'
  }
  Log 'PASS Ukrainian closure persisted and executed'

  $b=Repl @('x31','(подвоїти 21)')
  if($b.Count -ne 2 -or $b[0] -cne '42' -or $b[1] -cne '42'){throw 'state/closure did not survive reconnect'}
  Log 'PASS Ukrainian визначити state and closure survived reconnect'

  # #24/#8 live error boundary: an ordinary canonical error must be returned
  # as canonical embed text, not classified by the bridge, and the same
  # Session must still contain definitions created before the error.
  $typeError=Repl @('(car 5)','x31','(подвоїти 21)','(+ 20 22)')
  if($typeError.Count -ne 4 -or -not $typeError[0].StartsWith('error:') -or
     $typeError[1] -cne '42' -or $typeError[2] -cne '42' -or $typeError[3] -cne '42'){
    throw 'canonical type error did not recover with prior Session state intact'
  }
  Log 'PASS canonical type error recovered in same Session with prior state intact'

  $unknown=Repl @('missing-symbol-24','x31','(+ 20 22)')
  if($unknown.Count -ne 3 -or -not $unknown[0].StartsWith('error:') -or
     $unknown[1] -cne '42' -or $unknown[2] -cne '42'){
    throw 'unknown symbol did not fail canonically and recover'
  }
  Log 'PASS unknown symbol failed canonically and Session recovered'

  Log 'malformed-syntax-24: sending incomplete list as one bounded REPL request'
  $malformed=Repl @('(','x31','(+ 20 22)')
  if($malformed.Count -ne 3 -or -not $malformed[0].StartsWith('error:') -or
     $malformed[1] -cne '42' -or $malformed[2] -cne '42'){
    throw 'malformed syntax did not fail canonically and recover'
  }
  Log 'PASS malformed syntax failed canonically and Session recovered'

  Copy-Item -LiteralPath $trustedBridge -Destination $wrongTrust; $bytes=[IO.File]::ReadAllBytes($wrongTrust); if($bytes.Length -lt 1){throw 'negative wrong trust copy empty'}; $bytes[$bytes.Length-1]=$bytes[$bytes.Length-1] -bxor 1; [IO.File]::WriteAllBytes($wrongTrust,$bytes)
  $negativeRejected=$false
  try { & $probe -GameDir $gameRoot -BridgeArtifact $wrongTrust -ProcessId $gameProcess.Id -MyLispExe $myLisp -EvidenceOut $negativeEvidence -ObservationTimeoutSeconds $ObservationTimeoutSeconds | Out-Null }
  catch { $negativeRejected=$true; Log "PASS negative wrong trust rejected: $($_.Exception.Message)" }
  if(-not $negativeRejected){throw 'negative wrong trust was accepted'}

  Log 'Positive+negative evidence complete; close game normally'
  [void](Read-Host 'Close Cyberpunk completely, then press Enter'); $gameProcess.Refresh(); if (-not $gameProcess.HasExited){$gameProcess.WaitForExit()}
} catch { $primary=$_; Log "FAIL: $($_.Exception.Message)" }
finally {
  try {
    if ($null -ne $gameProcess){$gameProcess.Refresh(); if (-not $gameProcess.HasExited){Log 'cleanup waits for normal user close; no forced termination'; [void](Read-Host 'Close Cyberpunk completely, then press Enter'); $gameProcess.WaitForExit()}}
    foreach($f in @($wrongTrust,$negativeEvidence)){if(Test-Path -LiteralPath $f){Remove-Item -LiteralPath $f -Force}}
    if($deployed -and (Test-Path -LiteralPath $installedBridge)){ $installedHash=Get-Sha256Hex $installedBridge; $trustedHash=Get-Sha256Hex $trustedBridge; if ($installedHash -ne $trustedHash){throw "REFUSE CLEANUP changed version.dll installed=$installedHash trusted=$trustedHash"}; Remove-Item -LiteralPath $installedBridge -Force; Log 'removed exact owned game-side version.dll' }
    if(Test-Path -LiteralPath $observationPath){Remove-Item -LiteralPath $observationPath -Force; Log 'removed bridge-observation.lisp'}
    $cleanBoundary=Join-Path $PSScriptRoot 'Test-VanillaBoundary.ps1'; & $cleanBoundary -GameDir $gameRoot; Log 'PASS clean vanilla boundary restored after uninstall'
  } catch { $cleanup=$_; Log "CLEANUP FAIL: $($_.Exception.Message)" }
}
if ($null -ne $primary -and $null -ne $cleanup){throw "live witness failed: $($primary.Exception.Message); cleanup failed: $($cleanup.Exception.Message)"}; if ($null -ne $primary){throw $primary}; if ($null -ne $cleanup){throw $cleanup}
Log "SUCCESS evidence=$EvidenceOut"; Log "SUCCESS transcript=$TranscriptOut"
