# One-command Windows runner for VS Code's integrated PowerShell.
# The first configure downloads GLFW automatically through CMake FetchContent.

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDirectory = Join-Path $projectRoot 'build'

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'CMake was not found. Install CMake, reopen VS Code, and run this script again.'
}

Write-Host 'Configuring Fallen Signal (the first run downloads GLFW)...' -ForegroundColor Cyan
cmake -S $projectRoot -B $buildDirectory -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE."
}

Write-Host 'Building...' -ForegroundColor Cyan
cmake --build $buildDirectory --config Debug --parallel
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

$game = Join-Path $buildDirectory 'Debug\fallen_signal.exe'
if (-not (Test-Path $game)) {
    throw "Build completed, but the game executable was not found at: $game"
}

Write-Host 'Launching FALLEN SIGNAL...' -ForegroundColor Green
& $game
