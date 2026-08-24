# One-command Windows runner for VS Code's integrated PowerShell.
# The first configure downloads GLFW automatically through CMake FetchContent.
# If CMake is absent, this script tries to install it with winget first.

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDirectory = Join-Path $projectRoot 'build'

function Find-CMake {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    $programFilesX86 = [Environment]::GetFolderPath('ProgramFilesX86')
    $candidates = @(
        (Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe'),
        (Join-Path $programFilesX86 'CMake\bin\cmake.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\CMake\bin\cmake.exe')
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return $candidate
        }
    }

    return $null
}

$cmake = Find-CMake
if (-not $cmake) {
    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if ($winget) {
        $wingetPath = $winget.Source
        Write-Host 'CMake was not found; installing it with winget...' -ForegroundColor Yellow
        & $wingetPath install --id Kitware.CMake --exact --source winget --accept-package-agreements --accept-source-agreements
        if ($LASTEXITCODE -ne 0) {
            throw "winget could not install CMake (exit code ${LASTEXITCODE}). Install CMake from https://cmake.org/download/ and run this script again."
        }
        $cmake = Find-CMake
    }
}

if (-not $cmake) {
    throw @"
CMake was not found.

Run this once in an elevated PowerShell, then rerun this script:
  winget install --id Kitware.CMake --exact --source winget --accept-package-agreements --accept-source-agreements

If winget is unavailable, install CMake from https://cmake.org/download/ and select "Add CMake to the system PATH".
"@
}

Write-Host "Using CMake: $cmake" -ForegroundColor DarkGray
Write-Host 'Configuring Fallen Signal (the first run downloads GLFW)...' -ForegroundColor Cyan
& $cmake -S $projectRoot -B $buildDirectory -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) {
    throw @"
CMake configuration failed with exit code ${LASTEXITCODE}.

This project needs Visual Studio 2022 Build Tools (Desktop development with C++).
If the Visual Studio generator was not found, run:
  winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget --accept-package-agreements --accept-source-agreements --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"

Then restart VS Code and run this script again.
"@
}

Write-Host 'Building...' -ForegroundColor Cyan
& $cmake --build $buildDirectory --config Debug --parallel
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code ${LASTEXITCODE}. See the compiler errors above."
}

$game = Join-Path $buildDirectory 'Debug\fallen_signal.exe'
if (-not (Test-Path $game)) {
    throw "Build completed, but the game executable was not found at: $game"
}

Write-Host 'Launching FALLEN SIGNAL...' -ForegroundColor Green
& $game
