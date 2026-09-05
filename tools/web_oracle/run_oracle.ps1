<#
.SYNOPSIS
    Starts the Khepri Web Oracle streaming server and bridge.
.DESCRIPTION
    Checks Python dependencies, installs missing requirements, and runs the streaming server.
#>

param (
    [int]$Port = 8080,
    [int]$FPS = 30,
    [int]$Quality = 75,
    [string]$VirtualDesktop = "",
    [switch]$Isolated,
    [switch]$NoLaunch
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "       Starting Khepri Web Oracle Bridge          " -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# Check for Python
$PythonCmd = $null
if (Get-Command "py" -ErrorAction SilentlyContinue) {
    $PythonCmd = "py"
} elseif (Get-Command "python" -ErrorAction SilentlyContinue) {
    $PythonCmd = "python"
} else {
    Write-Error "Python is not installed or not found on PATH. Please install Python 3.10+."
    exit 1
}

Write-Host "Checking requirements in $ScriptDir/requirements.txt..." -ForegroundColor Gray
& $PythonCmd -m pip install -q -r "$ScriptDir/requirements.txt"

$LaunchArgs = @(
    "$ScriptDir/server.py",
    "--port", $Port,
    "--fps", $FPS,
    "--quality", $Quality
)

if ($Isolated -or ($VirtualDesktop -ne "")) {
    $DesktopName = if ($VirtualDesktop -ne "") { $VirtualDesktop } else { "KhepriVirtualDesktop" }
    $LaunchArgs += @("--virtual-desktop", $DesktopName)
    Write-Host "Running in Isolated Virtual Desktop mode ('$DesktopName')." -ForegroundColor Magenta
}

if ($NoLaunch) {
    $LaunchArgs += "--no-launch"
}

Write-Host "Launching server on http://localhost:$Port (FPS: $FPS, Quality: $Quality)..." -ForegroundColor Green
& $PythonCmd @LaunchArgs
