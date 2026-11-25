[CmdletBinding()]
param(
    [string]$Demo = "esp32s3_demo",
    [ValidateSet("build", "flash", "flash-monitor", "monitor", "menuconfig", "fullclean", "set-target")]
    [string]$Action = "build",
    [string]$Port = "",
    [string]$Target = "esp32s3",
    [string]$IdfPath = $env:IDF_PATH
)

$ErrorActionPreference = "Stop"

$projectDir = Join-Path $PSScriptRoot "demo\$Demo"

if (-not (Test-Path (Join-Path $projectDir "CMakeLists.txt")))
{
    throw "Demo project not found: $projectDir"
}

if (-not $IdfPath)
{
    throw "IDF_PATH is not set. Install ESP-IDF 5.x or pass -IdfPath."
}

$exportScript = Join-Path $IdfPath "export.ps1"
if (-not (Test-Path $exportScript))
{
    throw "Cannot find $exportScript. Point -IdfPath at the ESP-IDF root directory."
}

. $exportScript

if (($Action -eq "build" -or $Action -eq "flash" -or $Action -eq "flash-monitor") -and
    -not (Test-Path (Join-Path $projectDir "sdkconfig")))
{
    & idf.py -C $projectDir set-target $Target
    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }
}

$idfArgs = @("-C", $projectDir)

switch ($Action)
{
    "build"         { $idfArgs += "build" }
    "set-target"    { $idfArgs += @("set-target", $Target) }
    "menuconfig"    { $idfArgs += @("menuconfig") }
    "fullclean"     { $idfArgs += @("fullclean") }
    "monitor"       { if ($Port) { $idfArgs += @("-p", $Port) }; $idfArgs += @("monitor") }
    "flash"         { if ($Port) { $idfArgs += @("-p", $Port) }; $idfArgs += @("flash") }
    "flash-monitor" { if ($Port) { $idfArgs += @("-p", $Port) }; $idfArgs += @("flash", "monitor") }
}

& idf.py @idfArgs
exit $LASTEXITCODE

