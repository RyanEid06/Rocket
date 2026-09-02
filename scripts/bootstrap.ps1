[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [switch]$SkipStage0Build
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
. (Join-Path $projectRoot 'dependencies\activate.ps1')
$configurationName = $Configuration.ToLowerInvariant()
$buildDirectory = Join-Path $projectRoot "out\build\windows-$configurationName"
$bootstrapRoot = Join-Path $projectRoot "out\bootstrap\windows-$configurationName"
$compilerPackage = Join-Path $projectRoot 'compiler'
$compilerSource = Join-Path $compilerPackage 'src\main.rocket'
$stage0 = Join-Path $buildDirectory 'rocketc.exe'
$runtime = Join-Path $buildDirectory 'rocket_runtime.lib'
$clang = Join-Path $projectRoot 'dependencies\installed\llvm-22.1.6\bin\clang.exe'
$fixtures = Join-Path $projectRoot 'tests\fixtures'

$resolvedOut = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'out\bootstrap'))
$resolvedBootstrap = [System.IO.Path]::GetFullPath($bootstrapRoot)
if (-not $resolvedBootstrap.StartsWith(
        $resolvedOut + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to bootstrap outside out/bootstrap.'
}

if (-not $SkipStage0Build) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
        (Join-Path $projectRoot 'scripts\build.ps1') -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

foreach ($required in $stage0, $runtime, $clang, $compilerSource) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Missing bootstrap input: $required"
    }
}

$env:ROCKET_CLANG = $clang
$env:ROCKET_RUNTIME = $runtime
$env:ROCKET_STAGE0 = $stage0

if (Test-Path -LiteralPath $bootstrapRoot) {
    Remove-Item -LiteralPath $bootstrapRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $bootstrapRoot -Force | Out-Null

Write-Output 'stage0 -> stage1'
& $stage0 build $compilerPackage
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$packageStage1 = Join-Path $compilerPackage '.rocketc\main.exe'
if (-not (Test-Path -LiteralPath $packageStage1 -PathType Leaf)) {
    throw 'Stage 0 did not produce the expected stage1 compiler.'
}
$stage1 = Join-Path $bootstrapRoot 'stage1.exe'
Copy-Item -LiteralPath $packageStage1 -Destination $stage1

function Build-RocketStage {
    param(
        [Parameter(Mandatory)] [string]$Compiler,
        [Parameter(Mandatory)] [string]$StageName
    )
    $ir = Join-Path $bootstrapRoot "$StageName.ll"
    $object = Join-Path $bootstrapRoot "$StageName.obj"
    $executable = Join-Path $bootstrapRoot "$StageName.exe"
    $compilerOutput = & $Compiler --emit-ir $compilerSource $ir
    if ($compilerOutput) { Write-Host ($compilerOutput -join [Environment]::NewLine) }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $compileOutput = & $clang -c '-O2' '-Wno-override-module' $ir -o $object 2>&1
    if ($compileOutput) { Write-Host ($compileOutput -join [Environment]::NewLine) }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $linkOutput = & $clang $object $runtime '-fuse-ld=lld' -o $executable 2>&1
    if ($linkOutput) { Write-Host ($linkOutput -join [Environment]::NewLine) }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    return $executable
}

Write-Output 'stage1 -> stage2'
$stage2 = Build-RocketStage -Compiler $stage1 -StageName 'stage2'
Write-Output 'stage2 -> stage3'
$stage3 = Build-RocketStage -Compiler $stage2 -StageName 'stage3'

$stage2Ir = Join-Path $bootstrapRoot 'stage2.ll'
$stage3Ir = Join-Path $bootstrapRoot 'stage3.ll'
$stage2Hash = (Get-FileHash -LiteralPath $stage2Ir -Algorithm SHA256).Hash.ToLowerInvariant()
$stage3Hash = (Get-FileHash -LiteralPath $stage3Ir -Algorithm SHA256).Hash.ToLowerInvariant()
if ($stage2Hash -ne $stage3Hash) {
    throw "Non-deterministic bootstrap: stage2 $stage2Hash differs from stage3 $stage3Hash"
}

foreach ($compiler in $stage1, $stage2, $stage3) {
    & $compiler --self-test-lexer
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $compiler --self-test-parser
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
& $stage3 --check-hir $compilerSource
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $stage3 --check-mir $compilerSource
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$versions = @(
    ((& $stage1 --version) -join "`n").Trim()
    ((& $stage2 --version) -join "`n").Trim()
    ((& $stage3 --version) -join "`n").Trim()
)
if (($versions | Select-Object -Unique).Count -ne 1) {
    throw "Bootstrap stages report different versions: $($versions -join ', ')"
}

$conformanceSources = @(
    (Join-Path $projectRoot 'examples\hello.rocket')
    (Join-Path $fixtures 'llvm_operators.rocket')
    (Join-Path $fixtures 'runtime_collections.rocket')
    (Join-Path $fixtures 'phase6_types.rocket')
    (Join-Path $fixtures 'phase6_modules.rocket')
    (Join-Path $fixtures 'phase7_stdlib.rocket')
    (Join-Path $fixtures 'phase15_binary_io.rocket')
    (Join-Path $fixtures 'phase15_text_streams.rocket')
    (Join-Path $fixtures 'phase15_crypto.rocket')
    (Join-Path $fixtures 'phase15_network.rocket')
    (Join-Path $fixtures 'phase15_platform.rocket')
    (Join-Path $fixtures 'phase15_archive.rocket')
    (Join-Path $fixtures 'phase15_sqlite.rocket')
    (Join-Path $fixtures 'phase15_testing.rocket')
    (Join-Path $fixtures 'phase11_array_mutation.rocket')
    (Join-Path $fixtures 'phase11_array_growth.rocket')
    (Join-Path $fixtures 'phase11_map_set_tuple.rocket')
    (Join-Path $fixtures 'phase12_methods.rocket')
    (Join-Path $fixtures 'phase12_modules.rocket')
    (Join-Path $fixtures 'phase12_traits.rocket')
    (Join-Path $fixtures 'phase12_closures.rocket')
    (Join-Path $fixtures 'phase12_iterators.rocket')
    (Join-Path $fixtures 'phase12_associated_constants.rocket')
    (Join-Path $fixtures 'phase12_generic_lambdas.rocket')
    (Join-Path $fixtures 'phase13_native_package')
)
foreach ($compiler in $stage1, $stage2, $stage3) {
    foreach ($source in $conformanceSources) {
        & $compiler check $source
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}

$phase18PositiveFixtures = @(
    'async_join'
    'nested_await'
    'weak'
    'unique_buffer'
    'concurrency'
    'async_file'
    'async_socket'
    'async_cancel'
    'process'
    'task_group'
    'task_cancel'
    'thread'
    'structured_cleanup'
    'unsafe_local'
)
$phase18NegativeFixtures = @(
    'await_context'
    'send'
    'async_result_send'
    'suspension'
    'unique_buffer_move'
    'scoped_escape'
    'weak_share'
    'task_weak'
    'pointer_send'
    'guard_release'
    'task_reuse'
    'mutex_share'
    'capture_move'
    'transitive_move'
    'buffer_element_share'
)
$phase18Compilers = @($stage0, $stage1, $stage2, $stage3)
foreach ($compiler in $phase18Compilers) {
    foreach ($fixture in $phase18PositiveFixtures) {
        & $compiler check (Join-Path $fixtures "phase18_$fixture.rocket")
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    foreach ($fixture in $phase18NegativeFixtures) {
        $savedErrorActionPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $failureOutput = & $compiler check `
                (Join-Path $fixtures "phase18_${fixture}_failure.rocket") 2>&1
            $failureStatus = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $savedErrorActionPreference
        }
        if ($failureOutput) {
            Write-Host ($failureOutput -join [Environment]::NewLine)
        }
        if ($failureStatus -ne 1 -or ($failureOutput -join "`n") -notmatch 'R410') {
            throw "Phase 18 negative fixture '$fixture' disagreed in $compiler."
        }
    }
}

foreach ($source in $conformanceSources) {
    & $stage3 run $source
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$raylibReference = Join-Path $projectRoot 'examples\raylib_showcase'
foreach ($compiler in $stage1, $stage2, $stage3) {
    & $compiler check $raylibReference
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
& $stage3 build $raylibReference
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$raylibTestOutput = & $stage3 test $raylibReference 2>&1
$raylibTestStatus = $LASTEXITCODE
if ($raylibTestOutput) { Write-Host ($raylibTestOutput -join [Environment]::NewLine) }
if ($raylibTestStatus -ne 0 -or ($raylibTestOutput -join "`n") -notmatch '5 passed; 0 failed') {
    throw 'The Phase 14 raylib reference validation failed during bootstrap.'
}

& $stage3 build (Join-Path $fixtures 'phase13_static_library')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $stage3 build (Join-Path $fixtures 'phase13_dynamic_library')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$generationDirectory = Join-Path $bootstrapRoot 'phase13-generation'
New-Item -ItemType Directory -Path $generationDirectory -Force | Out-Null
$header1 = Join-Path $generationDirectory 'header1.h'
$header2 = Join-Path $generationDirectory 'header2.h'
$bindings1 = Join-Path $generationDirectory 'bindings1.rocket'
$bindings2 = Join-Path $generationDirectory 'bindings2.rocket'
& $stage3 emit-header (Join-Path $fixtures 'phase13_static_library') --output $header1
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $stage3 emit-header (Join-Path $fixtures 'phase13_static_library') --output $header2
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $stage3 bind (Join-Path $projectRoot 'tests\native\phase13_native.h') --output $bindings1
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $stage3 bind (Join-Path $projectRoot 'tests\native\phase13_native.h') --output $bindings2
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ((Get-FileHash -LiteralPath $header1 -Algorithm SHA256).Hash -ne
    (Get-FileHash -LiteralPath $header2 -Algorithm SHA256).Hash -or
    (Get-FileHash -LiteralPath $bindings1 -Algorithm SHA256).Hash -ne
    (Get-FileHash -LiteralPath $bindings2 -Algorithm SHA256).Hash) {
    throw 'Phase 13 native header or binding generation is not deterministic.'
}

$raylibBindings1 = Join-Path $generationDirectory 'raylib-bindings-stage2.rocket'
$raylibBindings2 = Join-Path $generationDirectory 'raylib-bindings-stage3.rocket'
$raylibBindings3 = Join-Path $generationDirectory 'raylib-bindings-stage3-repeat.rocket'
$raylibHeader = Join-Path $raylibReference 'native\rocket_raylib_adapter.h'
& $stage2 bind $raylibHeader --output $raylibBindings1
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $stage3 bind $raylibHeader --output $raylibBindings2
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $stage3 bind $raylibHeader --output $raylibBindings3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ((Get-FileHash -LiteralPath $raylibBindings1 -Algorithm SHA256).Hash -ne
    (Get-FileHash -LiteralPath $raylibBindings2 -Algorithm SHA256).Hash -or
    (Get-FileHash -LiteralPath $raylibBindings2 -Algorithm SHA256).Hash -ne
    (Get-FileHash -LiteralPath $raylibBindings3 -Algorithm SHA256).Hash) {
    throw 'Phase 14 raylib bindings differ across stage2/stage3 or repeat generation.'
}

$packageFixture = Join-Path $fixtures 'phase8_package'
& $stage3 check $packageFixture
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $stage3 fmt $packageFixture --check
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $stage3 run $packageFixture
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$testOutput = & $stage3 test $packageFixture 2>&1
$testStatus = $LASTEXITCODE
if ($testOutput) { Write-Host ($testOutput -join [Environment]::NewLine) }
if ($testStatus -ne 0 -or ($testOutput -join "`n") -notmatch '2 passed; 0 failed') {
    throw 'The self-hosted package test workflow failed.'
}

$phase16Fixture = Join-Path $bootstrapRoot 'phase16-packages'
if (Test-Path -LiteralPath $phase16Fixture) {
    Remove-Item -LiteralPath $phase16Fixture -Recurse -Force
}
Copy-Item -LiteralPath (Join-Path $fixtures 'phase16_packages') `
    -Destination $phase16Fixture -Recurse
$phase16App = Join-Path $phase16Fixture 'app'
& $stage0 resolve $phase16App
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
foreach ($compiler in $stage1, $stage2, $stage3) {
    & $compiler resolve $phase16App --locked
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $compiler tree $phase16App
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $compiler audit $phase16App
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
Remove-Item -LiteralPath (Join-Path $phase16Fixture 'registry') -Recurse -Force
Remove-Item -LiteralPath (Join-Path $phase16Fixture 'local_text') -Recurse -Force
foreach ($compiler in $stage1, $stage2, $stage3) {
    & $compiler resolve $phase16App --offline
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $compiler check $phase16App
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$savedErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$overflowOutput = & $stage3 run (Join-Path $fixtures 'int_overflow.rocket') 2>&1
$overflowStatus = $LASTEXITCODE
$ErrorActionPreference = $savedErrorActionPreference
if ($overflowOutput) { Write-Host ($overflowOutput -join [Environment]::NewLine) }
if ($overflowStatus -ne 101 -or ($overflowOutput -join "`n") -notmatch 'Int arithmetic overflow') {
    throw 'The self-hosted checked-Int overflow fixture did not fail as specified.'
}

$report = @(
    "version  $($versions[0])"
    "stage1.exe  $((Get-FileHash -LiteralPath $stage1 -Algorithm SHA256).Hash.ToLowerInvariant())"
    "stage2.exe  $((Get-FileHash -LiteralPath $stage2 -Algorithm SHA256).Hash.ToLowerInvariant())"
    "stage3.exe  $((Get-FileHash -LiteralPath $stage3 -Algorithm SHA256).Hash.ToLowerInvariant())"
    "stage2.ll  $stage2Hash"
    "stage3.ll  $stage3Hash"
    "deterministic  true"
    "phase18_parity  true"
)
Set-Content -LiteralPath (Join-Path $bootstrapRoot 'SHA256SUMS.txt') `
    -Value $report -Encoding ascii
Write-Output "Rocket bootstrap succeeded: stage2 and stage3 LLVM IR $stage2Hash"
