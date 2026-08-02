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
$provenancePath = Join-Path $package 'RELEASE-PROVENANCE.json'
$checksumPath = Join-Path $package 'SHA256SUMS.txt'

if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) {
    throw "Distribution compiler does not exist: $compiler"
}
if (-not (Test-Path -LiteralPath $languageServer -PathType Leaf)) {
    throw "Distribution language server does not exist: $languageServer"
}
if (-not (Test-Path -LiteralPath $librarian -PathType Leaf)) {
    throw "Distribution librarian does not exist: $librarian"
}
foreach ($tool in 'debugging.ps1', 'tooling.ps1', 'repl-prototype.ps1',
        'compatibility.ps1', 'hardening.ps1', 'minimize-crash.ps1',
        'application-validation.ps1') {
    if (-not (Test-Path -LiteralPath (Join-Path $package "tools\$tool") -PathType Leaf)) {
        throw "Distribution Phase 17 tool is missing: $tool"
    }
}
foreach ($requiredDocument in 'SECURITY.md', 'CONTRIBUTING.md', 'PACKAGE.md',
        'RELEASE-PROVENANCE.json', 'SHA256SUMS.txt') {
    if (-not (Test-Path -LiteralPath (Join-Path $package $requiredDocument) -PathType Leaf)) {
        throw "Distribution release metadata is missing: $requiredDocument"
    }
}

$provenance = Get-Content -LiteralPath $provenancePath -Raw | ConvertFrom-Json
if ($provenance.schema -ne 'rocket-release-provenance-1' -or
    $provenance.version -ne '2.0.0' -or
    $provenance.target -ne 'windows-x64' -or
    $provenance.runtime_abi -ne 1) {
    throw 'Distribution release provenance is invalid or incompatible.'
}
if ($provenance.compiler_sha256 -ne
    (Get-FileHash -LiteralPath $compiler -Algorithm SHA256).Hash.ToLowerInvariant()) {
    throw 'Distribution compiler does not match release provenance.'
}

$covered = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ($line in (Get-Content -LiteralPath $checksumPath)) {
    if ($line -notmatch '^([0-9a-f]{64})  (.+)$') {
        throw "Malformed checksum record: $line"
    }
    $relative = $Matches[2].Replace('/', [System.IO.Path]::DirectorySeparatorChar)
    $coveredPath = [System.IO.Path]::GetFullPath((Join-Path $package $relative))
    if (-not $coveredPath.StartsWith(
            $package + [System.IO.Path]::DirectorySeparatorChar,
            [System.StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $coveredPath -PathType Leaf) -or
        -not $covered.Add($coveredPath)) {
        throw "Unsafe, missing, or duplicate checksum path: $relative"
    }
    $actual = (Get-FileHash -LiteralPath $coveredPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Matches[1]) {
        throw "Checksum mismatch: $relative"
    }
}
$expectedCovered = @(Get-ChildItem -LiteralPath $package -Recurse -File |
    Where-Object { $_.Name -ne 'SHA256SUMS.txt' -and
        $_.Name -ne 'SHA256SUMS.txt.p7s' })
if ($covered.Count -ne $expectedCovered.Count) {
    throw "Checksum coverage is incomplete: $($covered.Count) of $($expectedCovered.Count) files."
}

$signaturePath = Join-Path $package 'SHA256SUMS.txt.p7s'
if ($provenance.signed) {
    if (-not (Test-Path -LiteralPath $signaturePath -PathType Leaf)) {
        throw 'Signed release provenance is missing the detached checksum signature.'
    }
    Add-Type -AssemblyName System.Security
    $content = [System.Security.Cryptography.Pkcs.ContentInfo]::new(
        [System.IO.File]::ReadAllBytes($checksumPath))
    $signed = [System.Security.Cryptography.Pkcs.SignedCms]::new($content, $true)
    $signed.Decode([System.IO.File]::ReadAllBytes($signaturePath))
    $signed.CheckSignature($true)
} elseif (Test-Path -LiteralPath $signaturePath) {
    throw 'Unsigned provenance cannot contain an unexplained checksum signature.'
}
if ($provenance.official) {
    if ($provenance.channel -ne 'stable' -or -not $provenance.signed -or
        $provenance.working_tree -ne 'clean') {
        throw 'Official release provenance must be stable, signed, and clean.'
    }
    foreach ($signedFile in $compiler, $languageServer,
            (Join-Path $package 'stage0\rocketc-stage0.exe')) {
        if ((Get-AuthenticodeSignature -LiteralPath $signedFile).Status -ne 'Valid') {
            throw "Official release binary signature is not trusted: $signedFile"
        }
    }
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
$phase18Source = Join-Path $work 'ownership_concurrency.rocket'
Copy-Item -LiteralPath (Join-Path $projectRoot 'examples\ownership_concurrency.rocket') `
    -Destination $phase18Source

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
        if ($LASTEXITCODE -ne 0 -or ($version -join "`n") -ne 'rocketc 2.0.0') {
            throw "Relocated compiler version check failed: $($version -join ' ')"
        }
        $languageServerVersion = & $languageServer --version
        if ($LASTEXITCODE -ne 0 -or
            ($languageServerVersion -join "`n") -ne 'rocket-lsp 1.0.0') {
            throw "Relocated language-server version check failed: $($languageServerVersion -join ' ')"
        }
        & $compiler check $source
        if ($LASTEXITCODE -ne 0) { throw 'Relocated compiler check failed.' }
        $machineOutput = (& $compiler check $source --message-format=json 2>$null) -join "`n"
        if ($LASTEXITCODE -ne 0 -or
            ($machineOutput | ConvertFrom-Json).schema -ne 'rocket-message-1') {
            throw 'Relocated machine-readable compiler output failed.'
        }
        & $compiler build $source --debug
        if ($LASTEXITCODE -ne 0 -or
            -not (Test-Path -LiteralPath (Join-Path $work '.rocketc\hello.pdb')) -or
            -not (Test-Path -LiteralPath (Join-Path $work '.rocketc\hello.rocket.map.json'))) {
            throw 'Relocated unoptimized debug build failed.'
        }
        $coverage = Join-Path $work 'coverage.json'
        & $compiler coverage $source --output $coverage
        if ($LASTEXITCODE -ne 0 -or
            (Get-Content $coverage -Raw | ConvertFrom-Json).schema -ne 'rocket-coverage-1') {
            throw 'Relocated coverage workflow failed.'
        }
        & $compiler build $source
        if ($LASTEXITCODE -ne 0) { throw 'Relocated compiler build failed.' }
        & $compiler run $source
        if ($LASTEXITCODE -ne 0) { throw 'Relocated compiler run failed.' }
        $phase11Output = & $compiler run $phase11Source 2>&1
        if ($LASTEXITCODE -ne 0 -or
            ($phase11Output -join "`n") -notmatch '4567693929835203094') {
            throw "Relocated compiler Phase 11 run failed: $($phase11Output -join ' ')"
        }
        & $compiler check $phase18Source
        if ($LASTEXITCODE -ne 0) { throw 'Relocated compiler Phase 18 check failed.' }
        $phase18Output = & $compiler run $phase18Source 2>&1
        if ($LASTEXITCODE -ne 0 -or
            ($phase18Output -join "`n") -notmatch '41[\r\n]+3[\r\n]+42') {
            throw "Relocated compiler Phase 18 run failed: $($phase18Output -join ' ')"
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

Write-Output "Rocket 2.0 distribution relocation and package test passed: $package"
