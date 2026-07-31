[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$package = [System.IO.Path]::GetFullPath($PackageRoot)
$compiler = Join-Path $package 'bin\rocketc.exe'
$languageServer = Join-Path $package 'bin\rocket-lsp.exe'
$librarian = Join-Path $package 'bin\llvm-lib.exe'
$workParent = Join-Path $projectRoot 'out\distribution-test'
$work = Join-Path $workParent 'relocated-working-directory'

if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) {
    throw "Distribution compiler does not exist: $compiler"
}
if (-not (Test-Path -LiteralPath $languageServer -PathType Leaf)) {
    throw "Distribution language server does not exist: $languageServer"
}
if (-not (Test-Path -LiteralPath $librarian -PathType Leaf)) {
    throw "Distribution librarian does not exist: $librarian"
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
    ROCKET_STAGE0 = $env:ROCKET_STAGE0
    LIB = $env:LIB
    LIBPATH = $env:LIBPATH
    INCLUDE = $env:INCLUDE
    PATH = $env:PATH
}
try {
    $env:ROCKET_CLANG = $null
    $env:ROCKET_RUNTIME = $null
    $env:ROCKET_STAGE0 = $null
    $env:LIB = $null
    $env:LIBPATH = $null
    $env:INCLUDE = $null
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"

    Push-Location $work
    try {
        $version = & $compiler --version
        if ($LASTEXITCODE -ne 0 -or ($version -join "`n") -ne 'rocketc 1.6.0') {
            throw "Relocated compiler version check failed: $($version -join ' ')"
        }
        $languageServerVersion = & $languageServer --version
        if ($LASTEXITCODE -ne 0 -or
            ($languageServerVersion -join "`n") -ne 'rocket-lsp 0.1.0') {
            throw "Relocated language-server version check failed: $($languageServerVersion -join ' ')"
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

        $phase16Packages = Join-Path $work 'phase16-packages'
        Copy-Item -LiteralPath (Join-Path $projectRoot 'tests\fixtures\phase16_packages') `
            -Destination $phase16Packages -Recurse
        $phase16App = Join-Path $phase16Packages 'app'
        & $compiler resolve $phase16App
        if ($LASTEXITCODE -ne 0) { throw 'Relocated package resolve failed.' }
        & $compiler audit $phase16App
        if ($LASTEXITCODE -ne 0) { throw 'Relocated package audit failed.' }
        Remove-Item -LiteralPath (Join-Path $phase16Packages 'registry') -Recurse -Force
        Remove-Item -LiteralPath (Join-Path $phase16Packages 'local_text') -Recurse -Force
        & $compiler resolve $phase16App --offline
        if ($LASTEXITCODE -ne 0) { throw 'Relocated package offline resolve failed.' }
        & $compiler check $phase16App
        if ($LASTEXITCODE -ne 0) { throw 'Relocated package import check failed.' }
    } finally {
        Pop-Location
    }
} finally {
    $env:ROCKET_CLANG = $savedEnvironment.ROCKET_CLANG
    $env:ROCKET_RUNTIME = $savedEnvironment.ROCKET_RUNTIME
    $env:ROCKET_STAGE0 = $savedEnvironment.ROCKET_STAGE0
    $env:LIB = $savedEnvironment.LIB
    $env:LIBPATH = $savedEnvironment.LIBPATH
    $env:INCLUDE = $savedEnvironment.INCLUDE
    $env:PATH = $savedEnvironment.PATH
}

Write-Output "Rocket 1.6 distribution relocation and package test passed: $package"
