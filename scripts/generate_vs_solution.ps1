# Generate Visual Studio solution
param(
    [string]$Generator = "Visual Studio 17 2022",
    [string]$VulkanSdkPath = $env:VULKAN_SDK,
    [switch]$OpenSolution
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Resolve-Path "$PSScriptRoot\.."

Write-Host "Generating Visual Studio solution for KhepriEngine using $Generator..." -ForegroundColor Cyan

$CmakeArgs = @("-B", "$ProjectRoot\build", "-G", "$Generator", "-A", "x64")
if ($VulkanSdkPath) {
    Write-Host "Using Vulkan SDK: $VulkanSdkPath" -ForegroundColor DarkCyan
    $CmakeArgs += "-DVULKAN_SDK_PATH=$VulkanSdkPath"
}

cmake @CmakeArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nSuccessfully generated Visual Studio solution at $ProjectRoot\build\KhepriEngine.sln" -ForegroundColor Green
    if ($OpenSolution) {
        Write-Host "Opening solution in Visual Studio..." -ForegroundColor Cyan
        Start-Process "$ProjectRoot\build\KhepriEngine.sln"
    }
} else {
    Write-Error "CMake generation failed with exit code $LASTEXITCODE"
}
