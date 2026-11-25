[CmdletBinding()]
param(
    [ValidateSet("build", "rebuild", "flash", "clean")]
    [string]$Action = "build",
    [string]$KeilPath = $(if ($env:KEIL_PATH) { $env:KEIL_PATH } else { "E:\Keil_v5" })
)

$ErrorActionPreference = "Stop"

$projectDir = Join-Path $PSScriptRoot "MDK-ARM"
$projectFile = Join-Path $projectDir "stm32f1_demo.uvprojx"
$uv4 = Join-Path $KeilPath "UV4\UV4.exe"
$logFile = Join-Path $projectDir "build.log"

if (-not (Test-Path -LiteralPath $uv4))
{
    throw "Keil UV4 not found: $uv4. Use -KeilPath to specify the Keil installation directory."
}

if (-not (Test-Path -LiteralPath $projectFile))
{
    throw "Project not found: $projectFile"
}

$arguments = switch ($Action)
{
    "build"   { @("-b", $projectFile) }
    "rebuild" { @("-r", $projectFile) }
    "flash"   { @("-f", $projectFile) }
    "clean"   { @("-c", $projectFile) }
}

$arguments += @("-j0", "-o", $logFile)

& $uv4 @arguments
$exitCode = $LASTEXITCODE

if ($exitCode -ge 2)
{
    if (Test-Path -LiteralPath $logFile)
    {
        Get-Content -LiteralPath $logFile -Tail 80
    }

    throw "Keil $Action failed with exit code $exitCode"
}

exit 0
