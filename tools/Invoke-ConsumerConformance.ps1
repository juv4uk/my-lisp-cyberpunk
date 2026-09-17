[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$MyLispExe,

    [Parameter(Mandatory = $true)]
    [string]$BridgeDll,

    [Parameter(Mandatory = $true)]
    [string]$Corpus,

    [Parameter(Mandatory = $true)]
    [string]$Report
)

$ErrorActionPreference = 'Stop'

function Escape-LispString([string]$Text) {
    return $Text.Replace('\', '\\').Replace('"', '\"').Replace("`r", '\r').Replace("`n", '\n').Replace("`t", '\t')
}

function Escape-RequestString([string]$Text) {
    return Escape-LispString $Text
}

function Connect-Loopback([int]$Port, [int]$Attempts = 60) {
    for ($attempt = 0; $attempt -lt $Attempts; $attempt++) {
        $client = [System.Net.Sockets.TcpClient]::new()
        try {
            $connect = $client.ConnectAsync('127.0.0.1', $Port)
            if ($connect.Wait(200)) {
                return $client
            }
        }
        catch {
            # Retry until the bounded startup window expires.
        }
        $client.Dispose()
        Start-Sleep -Milliseconds 100
    }
    throw "loopback endpoint did not appear on 127.0.0.1:$Port"
}

function Open-Utf8LineChannel([System.Net.Sockets.TcpClient]$Client) {
    $encoding = [System.Text.UTF8Encoding]::new($false)
    $stream = $Client.GetStream()
    $reader = [System.IO.StreamReader]::new($stream, $encoding, $false, 4096, $true)
    $writer = [System.IO.StreamWriter]::new($stream, $encoding, 4096, $true)
    $writer.NewLine = "`n"
    $writer.AutoFlush = $true
    return [pscustomobject]@{ Stream = $stream; Reader = $reader; Writer = $writer }
}

function Close-LineChannel($Channel, [System.Net.Sockets.TcpClient]$Client) {
    if ($null -ne $Channel) {
        $Channel.Writer.Dispose()
        $Channel.Reader.Dispose()
        $Channel.Stream.Dispose()
    }
    if ($null -ne $Client) {
        $Client.Dispose()
    }
}

function Invoke-CanonicalEval($Channel, [int]$Id, [string]$Source) {
    $escaped = Escape-RequestString $Source
    $Channel.Writer.WriteLine("(request (id $Id) (op eval) (source \"$escaped\"))")
    $response = $Channel.Reader.ReadLine()
    if ([string]::IsNullOrWhiteSpace($response)) {
        throw "canonical my-lisp returned no response for case id $Id"
    }

    if ($response -match '\(status ok\)') {
        $match = [regex]::Match($response, '\(value\s+([^()\s]+|\([^\r\n]*\))\)')
        if (-not $match.Success) {
            throw "canonical success response has no parseable value: $response"
        }
        return [pscustomobject]@{
            Status = 'ok'
            Value = $match.Groups[1].Value
            Kind = ''
            Raw = $response
        }
    }

    if ($response -match '\(status error\)') {
        $kindMatch = [regex]::Match($response, '\(kind\s+([^()\s]+)\)')
        if (-not $kindMatch.Success) {
            throw "canonical error response has no kind: $response"
        }
        return [pscustomobject]@{
            Status = 'error'
            Value = ''
            Kind = $kindMatch.Groups[1].Value
            Raw = $response
        }
    }

    throw "canonical response has unknown status: $response"
}

function Invoke-BridgeEval($Channel, [string]$Source) {
    $Channel.Writer.WriteLine($Source)
    $response = $Channel.Reader.ReadLine()
    if ($null -eq $response) {
        throw 'bridge REPL closed while reading a conformance response'
    }
    if ($response.StartsWith('error:')) {
        return [pscustomobject]@{ Status = 'error'; Value = ''; Diagnostic = $response }
    }
    return [pscustomobject]@{ Status = 'ok'; Value = $response; Diagnostic = '' }
}

function Read-Provenance([string]$BridgePath) {
    $directory = Split-Path -Parent (Resolve-Path $BridgePath)
    $path = Join-Path $directory 'cyberpunk-my-lisp-provenance.txt'
    if (-not (Test-Path $path)) {
        throw "canonical provenance missing beside bridge: $path"
    }
    $values = @{}
    foreach ($line in Get-Content $path) {
        if ($line -match '^([^=]+)=(.*)$') {
            $values[$Matches[1]] = $Matches[2]
        }
    }
    $sha = [string]$values['my-lisp-sha']
    $abi = [string]$values['embed-abi']
    if ($sha -notmatch '^[0-9a-f]{40}$' -or $abi -notmatch '^[0-9]+$') {
        throw 'canonical provenance does not contain an exact SHA and numeric ABI'
    }
    return [pscustomobject]@{ Sha = $sha; Abi = $abi }
}

if (-not (Test-Path $MyLispExe)) { throw "canonical my-lisp executable missing: $MyLispExe" }
if (-not (Test-Path $BridgeDll)) { throw "bridge DLL missing: $BridgeDll" }
if (-not (Test-Path $Corpus)) { throw "conformance corpus missing: $Corpus" }

$corpusData = Get-Content -Raw $Corpus | ConvertFrom-Json
if ($corpusData.protocol -ne 'consumer-conformance-corpus/1') {
    throw "unsupported conformance corpus protocol: $($corpusData.protocol)"
}
if ($null -eq $corpusData.cases -or $corpusData.cases.Count -eq 0) {
    throw 'conformance corpus has no cases'
}

$provenance = Read-Provenance $BridgeDll
$canonicalProcess = $null
$canonicalClient = $null
$canonicalChannel = $null
$bridgeClient = $null
$bridgeChannel = $null
$bridgeHandle = [IntPtr]::Zero
$shutdownDelegate = $null

try {
    # Reuse my-lisp's own machine-readable semantic TCP protocol rather than
    # inventing a second oracle or parsing interactive REPL chrome.
    $canonicalProcess = Start-Process -FilePath (Resolve-Path $MyLispExe) `
        -ArgumentList @('--tcp=40778', '--protocol=sexpr') `
        -WindowStyle Hidden -PassThru
    $canonicalClient = Connect-Loopback 40778
    $canonicalChannel = Open-Utf8LineChannel $canonicalClient

    # Load the exact same vanilla bridge artifact that #45 verifies. Loading
    # it starts the bridge-owned persistent canonical Session on 40777.
    $bridgePath = (Resolve-Path $BridgeDll).Path
    $bridgeHandle = [System.Runtime.InteropServices.NativeLibrary]::Load($bridgePath)
    if ($bridgeHandle -eq [IntPtr]::Zero) {
        throw "failed to load bridge DLL: $bridgePath"
    }
    $shutdownPointer = [System.Runtime.InteropServices.NativeLibrary]::GetExport($bridgeHandle, 'MyLispBridgeShutdown')
    $shutdownDelegate = [System.Runtime.InteropServices.Marshal]::GetDelegateForFunctionPointer(
        $shutdownPointer,
        [System.Func[uint32,int]]
    )
    $bridgeClient = Connect-Loopback 40777
    $bridgeChannel = Open-Utf8LineChannel $bridgeClient

    $records = [System.Collections.Generic.List[string]]::new()
    $requestId = 1
    foreach ($case in $corpusData.cases) {
        $source = [string]$case.source
        $oracle = Invoke-CanonicalEval $canonicalChannel $requestId $source
        $consumer = Invoke-BridgeEval $bridgeChannel $source

        $parity = $false
        if ($oracle.Status -eq 'ok' -and $consumer.Status -eq 'ok') {
            $parity = $oracle.Value -ceq $consumer.Value
        }
        elseif ($oracle.Status -eq 'error' -and $consumer.Status -eq 'error') {
            # v0 compares failure class at the semantic boundary: canonical
            # side remains authoritative for ErrorKind; the embed surface
            # returns its canonical rendered `error:` diagnostic.
            $parity = $true
        }

        if (-not $parity) {
            throw "semantic parity failed for case '$($case.id)': oracle=$($oracle.Raw) consumer=$($consumer.Diagnostic)$($consumer.Value)"
        }

        $caseId = Escape-LispString ([string]$case.id)
        $semanticId = Escape-LispString ([string]$case.semantic_id)
        $surface = [string]$case.surface
        $escapedSource = Escape-LispString $source
        if ($oracle.Status -eq 'ok') {
            $oracleRecord = "(oracle (status ok) (value \"$(Escape-LispString $oracle.Value)\"))"
            $consumerRecord = "(consumer (status ok) (value \"$(Escape-LispString $consumer.Value)\"))"
        }
        else {
            $oracleRecord = "(oracle (status error) (kind $($oracle.Kind)))"
            $consumerRecord = "(consumer (status error) (diagnostic \"$(Escape-LispString $consumer.Diagnostic)\"))"
        }
        $records.Add("((case \"$caseId\") (semantic-id \"$semanticId\") (surface $surface) (source \"$escapedSource\") $oracleRecord $consumerRecord (parity equal))")
        $requestId++
    }

    $body = $records -join "`n    "
    $reportText = @"
(consumer-conformance-report/1
  (my-lisp-sha \"$($provenance.Sha)\")
  (embed-abi $($provenance.Abi))
  (oracle-runner \"my-lisp-cli --protocol=sexpr\")
  (consumer-runner \"version.dll -> my_lisp_embed\")
  (cases
    $body))
"@
    $reportDir = Split-Path -Parent $Report
    if ($reportDir -and -not (Test-Path $reportDir)) {
        New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
    }
    Set-Content -Path $Report -Value $reportText -Encoding utf8
    Write-Output "consumer conformance runner wrote $($records.Count) parity records"
}
finally {
    Close-LineChannel $bridgeChannel $bridgeClient
    if ($null -ne $shutdownDelegate) {
        try { [void]$shutdownDelegate.Invoke(5000) } catch { Write-Warning "bridge shutdown call failed: $_" }
    }
    if ($bridgeHandle -ne [IntPtr]::Zero) {
        [System.Runtime.InteropServices.NativeLibrary]::Free($bridgeHandle)
    }
    Close-LineChannel $canonicalChannel $canonicalClient
    if ($null -ne $canonicalProcess -and -not $canonicalProcess.HasExited) {
        Stop-Process -Id $canonicalProcess.Id -Force -ErrorAction SilentlyContinue
        $canonicalProcess.WaitForExit(5000) | Out-Null
    }
}
