# Windows deployment script
# Usage: .\deploy-windows.ps1 -TargetName "EZ4Connect" -DisplayName "EZ4Connect" -BuildDir "build" -Architecture "amd64" -ZjuConnectRef "travelsky"

param(
    [string]$TargetName = "EZ4Connect",
    [string]$DisplayName = "EZ4Connect",
    [string]$BuildDir = "build",
    [string]$Architecture = "amd64",
    [string]$ZjuConnectRef = "travelsky",
    [string]$ZjuConnectRepo = $(if ($env:ZJU_CONNECT_REPO) { $env:ZJU_CONNECT_REPO } else { "https://github.com/foxwoods/zju-connect.git" })
)

Import-Module -Name Microsoft.PowerShell.Utility

# Create output directory
New-Item -ItemType Directory -Path "$DisplayName" -Force
Push-Location "$DisplayName"

# Copy executable
Copy-Item -Path "../$BuildDir/Release/$TargetName.exe" -Destination .

# Run windeployqt
& windeployqt.exe "$TargetName.exe"

# Build zju-connect from source
$ZjuSrcDir = "../zju-connect-src"
$ZjuOutput = Join-Path (Get-Location) "zju-connect.exe"
if (Test-Path $ZjuSrcDir) { Remove-Item -Recurse -Force $ZjuSrcDir }
& git clone --depth 1 --branch $ZjuConnectRef $ZjuConnectRepo $ZjuSrcDir
if ($LASTEXITCODE -ne 0) { throw "git clone zju-connect failed" }
Push-Location $ZjuSrcDir
$ZjuCommit = (& git rev-parse --short HEAD).Trim()
# EZ4Connect parses "-version" output with the prefix "ZJU Connect v"
$ZjuVersion = "v" + $ZjuConnectRef.TrimStart("v")
$env:CGO_ENABLED = "0"
$env:GOOS = "windows"
$env:GOARCH = $Architecture
& go build -v -trimpath -o $ZjuOutput -ldflags "-s -w -buildid= -X main.zjuConnectVersion=$ZjuVersion -X main.CommitID=$ZjuCommit" .
$ZjuBuildExit = $LASTEXITCODE
Remove-Item Env:GOOS, Env:GOARCH, Env:CGO_ENABLED
Pop-Location
if ($ZjuBuildExit -ne 0) { throw "go build zju-connect failed" }
Remove-Item -Recurse -Force $ZjuSrcDir

# Copy additional files
Copy-Item -Path "../libs/wintun/bin/$Architecture/wintun.dll" -Destination .
Copy-Item -Path "../resource/qt.conf" -Destination .

# Remove vc_redist executable
if ($Architecture -eq "amd64") {
    Remove-Item -Path vc_redist.x64.exe -ErrorAction SilentlyContinue
} else {
    Remove-Item -Path vc_redist.arm64.exe -ErrorAction SilentlyContinue
}

Pop-Location
