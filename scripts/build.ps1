param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build\mingw'
$compiler = 'C:\MinGW\bin\g++.exe'
$resourceCompiler = 'C:\MinGW\bin\windres.exe'

if (-not (Test-Path -LiteralPath $compiler)) {
    throw 'MinGW g++ was not found. Use CMake from an MSVC developer prompt instead.'
}
if (-not (Test-Path -LiteralPath $resourceCompiler)) {
    throw 'MinGW windres was not found. Use CMake from an MSVC developer prompt instead.'
}

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

$resourceObject = Join-Path $buildDir 'resource.o'
& $resourceCompiler `
    '--include-dir' (Join-Path $projectRoot 'assets') `
    (Join-Path $projectRoot 'assets\resource.rc') `
    '-O' 'coff' `
    '-o' $resourceObject

if ($LASTEXITCODE -ne 0) {
    throw "Resource compilation failed with exit code $LASTEXITCODE."
}

$outputPath = Join-Path $buildDir 'SBShot.exe'
$arguments = @(
    '-std=c++1z',
    '-mwindows',
    '-DUNICODE',
    '-D_UNICODE',
    '-DWINVER=0x0601',
    '-D_WIN32_WINNT=0x0601',
    '-ffunction-sections',
    '-fdata-sections',
    '-Wall',
    '-Wextra',
    '-Wpedantic',
    (Join-Path $projectRoot 'src\main.cpp'),
    $resourceObject,
    '-static-libgcc',
    '-static-libstdc++',
    '-Wl,--gc-sections',
    '-luser32',
    '-lgdi32',
    '-lshell32',
    '-lmsimg32',
    '-lcomctl32',
    '-lcomdlg32',
    '-lgdiplus',
    '-o',
    $outputPath
)

if ($Configuration -eq 'Release') {
    $arguments = @('-Os', '-s', '-fno-ident') + $arguments
} else {
    $arguments = @('-O0', '-g') + $arguments
}

& $compiler @arguments
if ($LASTEXITCODE -ne 0) {
    throw "C++ compilation failed with exit code $LASTEXITCODE."
}

$artifact = Get-Item -LiteralPath $outputPath
Write-Host "Built $($artifact.FullName) ($($artifact.Length) bytes)"
