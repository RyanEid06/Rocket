[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$package = [System.IO.Path]::GetFullPath($PackageRoot)
$compiler = Join-Path $package 'bin\rocketc.exe'
$workParent = Join-Path $projectRoot 'out\distribution-test'
$work = Join-Path $workParent 'relocated-working-directory'

if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) {
    throw "Distribution compiler does not exist: $compiler"
}
$resolvedOut = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'out'))
$resolvedWork = [System.IO.Path]::GetFullPath($work)
if (-not $resolvedWork.StartsWith(
        $resolvedOut + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to replace a distribution test directory outside out/.'
}

if (Test-Path -LiteralPath $work) {
    Remove-Item -LiteralPath $work -Recurse -Force
}
New-Item -ItemType Directory -Path $work -Force | Out-Null
$source = Join-Path $work 'hello.rocket'
Copy-Item -LiteralPath (Join-Path $projectRoot 'examples\hello.rocket') -Destination $source
$phase11Source = Join-Path $work 'phase11_map_set_tuple.rocket'
Copy-Item -LiteralPath (Join-Path $projectRoot 'tests\fixtures\phase11_map_set_tuple.rocket') `
    -Destination $phase11Source

$savedEnvironment = @{
    ROCKET_CLANG = $env:ROCKET_CLANG
    ROCKET_RUNTIME = $env:ROCKET_RUNTIME
    LIB = $env:LIB
    LIBPATH = $env:LIBPATH
    INCLUDE = $env:INCLUDE
    PATH = $env:PATH
}
try {
    $env:ROCKET_CLANG = $null
    $env:ROCKET_RUNTIME = $null
    $env:LIB = $null
    $env:LIBPATH = $null
    $env:INCLUDE = $null
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"

    Push-Location $work
    try {
        $version = & $compiler --version
        if ($LASTEXITCODE -ne 0 -or ($version -join "`n") -ne 'rocketc 1.2.0') {
            throw "Relocated compiler version check failed: $($version -join ' ')"
        }
        & $compiler check $source
        if ($LASTEXITCODE -ne 0) { throw 'Relocated compiler check failed.' }
        & $compiler build $source
        if ($LASTEXITCODE -ne 0) { throw 'Relocated compiler build failed.' }
        & $compiler run $source
        if ($LASTEXITCODE -ne 0) { throw 'Relocated compiler run failed.' }
        $phase11Output = & $compiler run $phase11Source 2>&1
        if ($LASTEXITCODE -ne 0 -or
            ($phase11Output -join "`n") -notmatch '4567693929835203094') {
            throw "Relocated compiler Phase 11 run failed: $($phase11Output -join ' ')"
        }

        $nativeProgram = Join-Path $work '.rocketc\main.exe'
        if (-not (Test-Path -LiteralPath $nativeProgram -PathType Leaf)) {
            throw "Relocated compiler did not create $nativeProgram"
        }
        & $nativeProgram
        if ($LASTEXITCODE -ne 0) { throw 'Distributed native program failed.' }
    } finally {
        Pop-Location
    }
} finally {
    $env:ROCKET_CLANG = $savedEnvironment.ROCKET_CLANG
    $env:ROCKET_RUNTIME = $savedEnvironment.ROCKET_RUNTIME
    $env:LIB = $savedEnvironment.LIB
    $env:LIBPATH = $savedEnvironment.LIBPATH
    $env:INCLUDE = $savedEnvironment.INCLUDE
    $env:PATH = $savedEnvironment.PATH
}

Write-Output "Rocket 1.2 distribution relocation test passed: $package"
