$versionFile = "VERSION"
if (-Not (Test-Path $versionFile)) {
    "0.1.0" | Out-File -FilePath $versionFile -Encoding ascii
}

$raw = (Get-Content $versionFile).Trim()
$parts = $raw.Split('.')
if ($parts.Length -ne 3) {
    Write-Host "Invalid version format in VERSION file: $raw"
    exit 1
}

$major = [int]$parts[0]
$minor = [int]$parts[1]
$patch = [int]$parts[2] + 1

$newVersion = "$major.$minor.$patch"
[System.IO.File]::WriteAllText((Get-Item $versionFile).FullName, $newVersion)

Write-Host "Bumping Khepri Engine version: $raw -> $newVersion"

# Update src/core/Version.h
$versionHPath = "src/core/Version.h"
if (Test-Path $versionHPath) {
    $versionHContent = @"
#pragma once

namespace KhepriEngine {
    constexpr int VERSION_MAJOR = $major;
    constexpr int VERSION_MINOR = $minor;
    constexpr int VERSION_PATCH = $patch;
    constexpr const char* VERSION_STRING = "$newVersion";
}
"@
    [System.IO.File]::WriteAllText((Get-Item $versionHPath).FullName, $versionHContent)
}

# Update CMakeLists.txt
$cmakeFile = "CMakeLists.txt"
if (Test-Path $cmakeFile) {
    $cmakeContent = [System.IO.File]::ReadAllText((Get-Item $cmakeFile).FullName)
    $updatedCmake = [regex]::Replace($cmakeContent, 'project\(KhepriEngine VERSION [0-9]+\.[0-9]+\.[0-9]+', "project(KhepriEngine VERSION $newVersion")
    [System.IO.File]::WriteAllText((Get-Item $cmakeFile).FullName, $updatedCmake)
}

# Stage modified files for Git commit
if (Get-Command git -ErrorAction SilentlyContinue) {
    git add VERSION src/core/Version.h CMakeLists.txt
}
