#Requires -Version 5.1
<#
.SYNOPSIS
    Interactive terminal client for the in-process bridge's canonical
    my-lisp REPL (bridge/, #24/#31). Runs entirely outside the game --
    an ordinary TCP client, zero risk to the game process.

.DESCRIPTION
    Connects once to 127.0.0.1:<Port> (default 40777, the bridge's own
    listener inside Cyberpunk2077.exe) and gives you a normal
    read-eval-print loop in this terminal window. Meant to be run in
    its own window on a second monitor while the game runs.

.EXAMPLE
    .\tools\repl-client.ps1
#>

param(
    [string]$HostName = "127.0.0.1",
    [int]$Port = 40777
)

$ErrorActionPreference = "Stop"

Write-Host "Connecting to $HostName`:$Port ..."
$client = New-Object System.Net.Sockets.TcpClient
try {
    $client.Connect($HostName, $Port)
} catch {
    Write-Host "Could not connect. Is the game running with bridge/version.dll loaded?" -ForegroundColor Red
    throw
}

# UTF8Encoding($false) = no BOM preamble. [System.Text.Encoding]::UTF8
# writes a BOM once at the start of the stream, which lands inside the
# first expression sent and breaks its parse ("unknown symbol" on an
# otherwise-valid form) -- found live testing this exact script.
$noBomUtf8 = New-Object System.Text.UTF8Encoding($false)
$stream = $client.GetStream()
$writer = New-Object System.IO.StreamWriter($stream, $noBomUtf8)
$writer.NewLine = "`n"
$writer.AutoFlush = $true
$reader = New-Object System.IO.StreamReader($stream, $noBomUtf8)

Write-Host "Connected. Canonical my-lisp REPL inside the game process." -ForegroundColor Green
Write-Host "Type an expression and press Enter. Ctrl+C to quit." -ForegroundColor Green
Write-Host ""

try {
    while ($true) {
        Write-Host "> " -NoNewline -ForegroundColor Cyan
        $line = [Console]::ReadLine()
        if ($null -eq $line) { break }
        if ($line.Trim().Length -eq 0) { continue }

        $writer.WriteLine($line)
        $reply = $reader.ReadLine()
        if ($null -eq $reply) {
            Write-Host "(connection closed by bridge)" -ForegroundColor Red
            break
        }
        Write-Host $reply
    }
} finally {
    $reader.Dispose()
    $writer.Dispose()
    $client.Close()
}
