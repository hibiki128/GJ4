param(
    [switch]$Debug = $false,
    [string]$SourcePath = "..\module\TakoEngine\project\engine\resources",
    [string]$DestinationPath = "..\app\EngineResources"
)

Set-Location -Path $PSScriptRoot

Write-Host ">> $(Split-Path -Leaf $MyInvocation.MyCommand.Path) ----------------------------------------"
if ($Debug) {
    Write-Host "Debug mode is ON"
    Write-Host "Source Path: $SourcePath"
    Write-Host "Destination Path: $DestinationPath"
    Write-Host "Current Directory: $(Get-Location)"
}


# Create destination directory if it doesn't exist
if (!(Test-Path -Path $DestinationPath)) {
    New-Item -ItemType Directory -Path $DestinationPath
}

# Copy all files from source to destination
Copy-Item -Path "$SourcePath\*" -Destination $DestinationPath -Recurse -Force

Write-Host "Engine resources copied from $SourcePath to $DestinationPath"
