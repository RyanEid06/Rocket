[CmdletBinding()]
param(
    [string]$Compiler = '',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$configurationName = $Configuration.ToLowerInvariant()
if (-not $Compiler) {
    . (Join-Path $projectRoot 'dependencies\activate.ps1')
    $Compiler = Join-Path $projectRoot "out\bootstrap\windows-$configurationName\stage3.exe"
}
$Compiler = [System.IO.Path]::GetFullPath($Compiler)
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) {
    throw "Conformance compiler does not exist: $Compiler"
}

$fixtures = Join-Path $projectRoot 'tests\fixtures'
$reportDirectory = Join-Path $projectRoot 'out\conformance'
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
$reportPath = Join-Path $reportDirectory "rocket-2.0-$configurationName.txt"
$env:ROCKET_STAGE0 = Join-Path $projectRoot "out\build\windows-$configurationName\rocketc.exe"
$results = [System.Collections.Generic.List[string]]::new()

function Invoke-ConformanceCase {
    param(
        [string]$Name,
        [string[]]$Arguments,
        [int]$ExpectedStatus = 0,
        [string]$ExpectedPattern = ''
    )
    $savedPreference = $script:ErrorActionPreference
    $script:ErrorActionPreference = 'Continue'
    $output = & $Compiler @Arguments 2>&1
    $status = $LASTEXITCODE
    $script:ErrorActionPreference = $savedPreference
    $text = $output -join "`n"
    if ($status -ne $ExpectedStatus) {
        throw "Conformance case '$Name' returned $status, expected $ExpectedStatus.`n$text"
    }
    if ($ExpectedPattern -and $text -notmatch $ExpectedPattern) {
        throw "Conformance case '$Name' did not match '$ExpectedPattern'.`n$text"
    }
    $results.Add("PASS  $Name  status=$status")
}

Invoke-ConformanceCase 'version' @('--version') 0 '^rocketc 2\.0\.0$'
Invoke-ConformanceCase 'lexer-self-test' @('--self-test-lexer') 0 'lexer tests passed'
Invoke-ConformanceCase 'parser-self-test' @('--self-test-parser') 0 'parser tests passed'
Invoke-ConformanceCase 'hello-check' @('check', (Join-Path $projectRoot 'examples\hello.rocket')) 0 'check succeeded'
Invoke-ConformanceCase 'types-check' @('check', (Join-Path $fixtures 'phase6_types.rocket'))
Invoke-ConformanceCase 'modules-check' @('check', (Join-Path $fixtures 'phase6_modules.rocket'))
Invoke-ConformanceCase 'stdlib-check' @('check', (Join-Path $fixtures 'phase7_stdlib.rocket'))
Invoke-ConformanceCase 'bootstrap-primitives-check' @('check', (Join-Path $fixtures 'phase9_bootstrap_primitives.rocket'))
Invoke-ConformanceCase 'array-mutation-check' @('check', (Join-Path $fixtures 'phase11_array_mutation.rocket'))
Invoke-ConformanceCase 'array-growth-check' @('check', (Join-Path $fixtures 'phase11_array_growth.rocket'))
Invoke-ConformanceCase 'map-set-tuple-check' @('check', (Join-Path $fixtures 'phase11_map_set_tuple.rocket'))
Invoke-ConformanceCase 'methods-check' @('check', (Join-Path $fixtures 'phase12_methods.rocket'))
Invoke-ConformanceCase 'method-modules-check' @('check', (Join-Path $fixtures 'phase12_modules.rocket'))
Invoke-ConformanceCase 'traits-check' @('check', (Join-Path $fixtures 'phase12_traits.rocket'))
Invoke-ConformanceCase 'closures-check' @('check', (Join-Path $fixtures 'phase12_closures.rocket'))
Invoke-ConformanceCase 'iterators-check' @('check', (Join-Path $fixtures 'phase12_iterators.rocket'))
Invoke-ConformanceCase 'associated-constants-check' @('check', (Join-Path $fixtures 'phase12_associated_constants.rocket'))
Invoke-ConformanceCase 'generic-lambdas-check' @('check', (Join-Path $fixtures 'phase12_generic_lambdas.rocket'))
Invoke-ConformanceCase 'native-interop-check' @('check', (Join-Path $fixtures 'phase13_native_package'))
Invoke-ConformanceCase 'raylib-reference-check' @('check', (Join-Path $projectRoot 'examples\raylib_showcase'))
Invoke-ConformanceCase 'hello-run' @('run', (Join-Path $projectRoot 'examples\hello.rocket')) 0 'Hello from Rocket'
Invoke-ConformanceCase 'operators-run' @('run', (Join-Path $fixtures 'llvm_operators.rocket'))
Invoke-ConformanceCase 'collections-run' @('run', (Join-Path $fixtures 'runtime_collections.rocket'))
Invoke-ConformanceCase 'types-run' @('run', (Join-Path $fixtures 'phase6_types.rocket'))
Invoke-ConformanceCase 'modules-run' @('run', (Join-Path $fixtures 'phase6_modules.rocket'))
Invoke-ConformanceCase 'stdlib-run' @('run', (Join-Path $fixtures 'phase7_stdlib.rocket'))
Invoke-ConformanceCase 'array-mutation-run' @('run', (Join-Path $fixtures 'phase11_array_mutation.rocket')) 0 '99[\r\n]+20[\r\n]+20[\r\n]+new[\r\n]+old'
Invoke-ConformanceCase 'array-growth-run' @('run', (Join-Path $fixtures 'phase11_array_growth.rocket')) 0 '8[\r\n]+0[\r\n]+old[\r\n]+new[\r\n]+1[\r\n]+old[\r\n]+first[\r\n]+old[\r\n]+0[\r\n]+8'
Invoke-ConformanceCase 'map-set-tuple-run' @('run', (Join-Path $fixtures 'phase11_map_set_tuple.rocket')) 0 '4567693929835203094'
Invoke-ConformanceCase 'methods-run' @('run', (Join-Path $fixtures 'phase12_methods.rocket')) 0 '42[\r\n]+7[\r\n]+method[\r\n]+3[\r\n]+12'
Invoke-ConformanceCase 'method-modules-run' @('run', (Join-Path $fixtures 'phase12_modules.rocket')) 0 '(?m)^42$'
Invoke-ConformanceCase 'traits-run' @('run', (Join-Path $fixtures 'phase12_traits.rocket')) 0 '(?m)^42$'
Invoke-ConformanceCase 'closures-run' @('run', (Join-Path $fixtures 'phase12_closures.rocket')) 0 '(?m)^42$'
Invoke-ConformanceCase 'iterators-run' @('run', (Join-Path $fixtures 'phase12_iterators.rocket')) 0 '(?m)^6$'
Invoke-ConformanceCase 'associated-constants-run' @('run', (Join-Path $fixtures 'phase12_associated_constants.rocket')) 0 '(?m)^42$'
Invoke-ConformanceCase 'generic-lambdas-run' @('run', (Join-Path $fixtures 'phase12_generic_lambdas.rocket')) 0 '42[\r\n]+generic[\r\n]+7[\r\n]+capture[\r\n]+9[\r\n]+immediate[\r\n]+8[\r\n]+nested[\r\n]+10[\r\n]+deep'
Invoke-ConformanceCase 'native-interop-run' @('run', (Join-Path $fixtures 'phase13_native_package')) 0 'native-interop-ok'
Invoke-ConformanceCase 'native-static-library-build' @('build', (Join-Path $fixtures 'phase13_static_library')) 0 'phase13_math.lib'
Invoke-ConformanceCase 'native-dynamic-library-build' @('build', (Join-Path $fixtures 'phase13_dynamic_library')) 0 'phase13_math_dynamic.dll'
Invoke-ConformanceCase 'raylib-reference-build' @('build', (Join-Path $projectRoot 'examples\raylib_showcase')) 0 'rocket-raylib-showcase.exe'
Invoke-ConformanceCase 'raylib-reference-test' @('test', (Join-Path $projectRoot 'examples\raylib_showcase')) 0 '5 passed; 0 failed'
$nativeHeader1 = Join-Path $reportDirectory 'phase13-header-1.h'
$nativeHeader2 = Join-Path $reportDirectory 'phase13-header-2.h'
$nativeBindings1 = Join-Path $reportDirectory 'phase13-bindings-1.rocket'
$nativeBindings2 = Join-Path $reportDirectory 'phase13-bindings-2.rocket'
Invoke-ConformanceCase 'native-header-generate' @('emit-header', (Join-Path $fixtures 'phase13_static_library'), '--output', $nativeHeader1)
Invoke-ConformanceCase 'native-header-repeat' @('emit-header', (Join-Path $fixtures 'phase13_static_library'), '--output', $nativeHeader2)
Invoke-ConformanceCase 'native-bindings-generate' @('bind', (Join-Path $projectRoot 'tests\native\phase13_native.h'), '--output', $nativeBindings1)
Invoke-ConformanceCase 'native-bindings-repeat' @('bind', (Join-Path $projectRoot 'tests\native\phase13_native.h'), '--output', $nativeBindings2)
if ((Get-FileHash -LiteralPath $nativeHeader1 -Algorithm SHA256).Hash -ne
    (Get-FileHash -LiteralPath $nativeHeader2 -Algorithm SHA256).Hash -or
    (Get-FileHash -LiteralPath $nativeBindings1 -Algorithm SHA256).Hash -ne
    (Get-FileHash -LiteralPath $nativeBindings2 -Algorithm SHA256).Hash) {
    throw 'Phase 13 generated native artifacts are not deterministic.'
}
$raylibBindings1 = Join-Path $reportDirectory 'phase14-raylib-bindings-1.rocket'
$raylibBindings2 = Join-Path $reportDirectory 'phase14-raylib-bindings-2.rocket'
Invoke-ConformanceCase 'raylib-bindings-generate' @('bind', (Join-Path $projectRoot 'examples\raylib_showcase\native\rocket_raylib_adapter.h'), '--output', $raylibBindings1)
Invoke-ConformanceCase 'raylib-bindings-repeat' @('bind', (Join-Path $projectRoot 'examples\raylib_showcase\native\rocket_raylib_adapter.h'), '--output', $raylibBindings2)
if ((Get-FileHash -LiteralPath $raylibBindings1 -Algorithm SHA256).Hash -ne
    (Get-FileHash -LiteralPath $raylibBindings2 -Algorithm SHA256).Hash) {
    throw 'Phase 14 raylib bindings are not deterministic.'
}
Invoke-ConformanceCase 'package-check' @('check', (Join-Path $fixtures 'phase8_package')) 0 'check succeeded'
Invoke-ConformanceCase 'package-format' @('fmt', (Join-Path $fixtures 'phase8_package'), '--check') 0 'format check succeeded'
Invoke-ConformanceCase 'package-run' @('run', (Join-Path $fixtures 'phase8_package')) 0 '(?m)^42$'
Invoke-ConformanceCase 'package-test' @('test', (Join-Path $fixtures 'phase8_package')) 0 '2 passed; 0 failed'
Invoke-ConformanceCase 'private-visibility-diagnostic' @('check', (Join-Path $fixtures 'phase6_visibility.rocket')) 1 'R3003'
Invoke-ConformanceCase 'import-cycle-diagnostic' @('check', (Join-Path $fixtures 'phase6_cycle.rocket')) 1 'R3002'
Invoke-ConformanceCase 'invalid-map-key-diagnostic' @('check', (Join-Path $fixtures 'phase11_invalid_key.rocket')) 1 'R4001'
Invoke-ConformanceCase 'invalid-method-receiver' @('check', (Join-Path $fixtures 'phase12_invalid_receiver.rocket')) 1 'method receiver must have impl type Counter'
Invoke-ConformanceCase 'generic-lambda-result-diagnostic' @('check', (Join-Path $fixtures 'phase12_generic_lambda_failure.rocket')) 1 'String, expected Int'
Invoke-ConformanceCase 'immediate-lambda-argument-diagnostic' @('check', (Join-Path $fixtures 'phase12_immediate_lambda_failure.rocket')) 1 'String, expected Int'
Invoke-ConformanceCase 'unsafe-boundary-diagnostic' @('check', (Join-Path $fixtures 'phase13_unsafe_failure.rocket')) 1 'requires an explicit unsafe block'
Invoke-ConformanceCase 'raylib-unsafe-boundary-diagnostic' @('check', (Join-Path $fixtures 'phase14_unsafe_failure.rocket')) 1 'requires an explicit unsafe block'
Invoke-ConformanceCase 'phase15-binary-io' @('run', (Join-Path $fixtures 'phase15_binary_io.rocket')) 0 'phase15-binary-ok'
Invoke-ConformanceCase 'phase15-text-streams' @('run', (Join-Path $fixtures 'phase15_text_streams.rocket')) 0 'phase15-text-streams-ok'
Invoke-ConformanceCase 'phase15-crypto' @('run', (Join-Path $fixtures 'phase15_crypto.rocket')) 0 'phase15-crypto-ok'
Invoke-ConformanceCase 'phase15-network' @('run', (Join-Path $fixtures 'phase15_network.rocket')) 0 'phase15-network-ok'
Invoke-ConformanceCase 'phase15-platform' @('run', (Join-Path $fixtures 'phase15_platform.rocket')) 0 'phase15-platform-ok'
Invoke-ConformanceCase 'phase15-archive' @('run', (Join-Path $fixtures 'phase15_archive.rocket')) 0 'phase15-archive-ok'
Invoke-ConformanceCase 'phase15-sqlite' @('run', (Join-Path $fixtures 'phase15_sqlite.rocket')) 0 'phase15-sqlite-ok'
Invoke-ConformanceCase 'phase15-testing' @('run', (Join-Path $fixtures 'phase15_testing.rocket')) 0 'phase15-testing-ok'
Invoke-ConformanceCase 'phase15-test-runner' @('test', (Join-Path $fixtures 'phase15_test_package')) 0 '1 passed; 0 failed; 1 expected failure'
Invoke-ConformanceCase 'phase18-async-check' @('check', (Join-Path $fixtures 'phase18_nested_await.rocket')) 0 'check succeeded'
Invoke-ConformanceCase 'phase18-ownership-run' @('run', (Join-Path $fixtures 'phase18_weak.rocket')) 0 '(?m)^42$'
Invoke-ConformanceCase 'phase18-buffer-run' @('run', (Join-Path $fixtures 'phase18_unique_buffer.rocket')) 0 '2[\r\n]+20[\r\n]+10[\r\n]+99[\r\n]+30[\r\n]+7[\r\n]+7'
Invoke-ConformanceCase 'phase18-concurrency-run' @('run', (Join-Path $fixtures 'phase18_concurrency.rocket')) 0 'published'
Invoke-ConformanceCase 'phase18-async-file-run' @('run', (Join-Path $fixtures 'phase18_async_file.rocket')) 0 'R[\r\n]+T'
Invoke-ConformanceCase 'phase18-structured-run' @('run', (Join-Path $fixtures 'phase18_task_group.rocket')) 0 'first[\r\n]+second[\r\n]+third'
Invoke-ConformanceCase 'phase18-thread-run' @('run', (Join-Path $fixtures 'phase18_thread.rocket')) 0 'thread-result'
Invoke-ConformanceCase 'phase18-cancel-run' @('run', (Join-Path $fixtures 'phase18_async_cancel.rocket')) 0 'operation cancelled'
Invoke-ConformanceCase 'phase18-await-context-diagnostic' @('check', (Join-Path $fixtures 'phase18_await_context_failure.rocket')) 1 'R4105'
Invoke-ConformanceCase 'phase18-send-diagnostic' @('check', (Join-Path $fixtures 'phase18_send_failure.rocket')) 1 'R4101'
Invoke-ConformanceCase 'phase18-move-diagnostic' @('check', (Join-Path $fixtures 'phase18_unique_buffer_move_failure.rocket')) 1 'R4103'
Invoke-ConformanceCase 'phase18-scope-diagnostic' @('check', (Join-Path $fixtures 'phase18_scoped_escape_failure.rocket')) 1 'R4104'
$phase16Work = Join-Path $reportDirectory "phase16-packages-$configurationName"
if (Test-Path -LiteralPath $phase16Work) {
    Remove-Item -LiteralPath $phase16Work -Recurse -Force
}
Copy-Item -LiteralPath (Join-Path $fixtures 'phase16_packages') `
    -Destination $phase16Work -Recurse
$phase16App = Join-Path $phase16Work 'app'
Invoke-ConformanceCase 'phase16-resolve' @('resolve', $phase16App) 0 'resolved 3 package'
Invoke-ConformanceCase 'phase16-locked' @('resolve', $phase16App, '--locked') 0 'locked resolution verified'
Invoke-ConformanceCase 'phase16-tree' @('tree', $phase16App) 0 'math@1\.2\.0'
Invoke-ConformanceCase 'phase16-audit' @('audit', $phase16App) 0 'SHA-256 cache verified'
Remove-Item -LiteralPath (Join-Path $phase16Work 'registry') -Recurse -Force
Remove-Item -LiteralPath (Join-Path $phase16Work 'local_text') -Recurse -Force
Invoke-ConformanceCase 'phase16-offline' @('resolve', $phase16App, '--offline') 0 'offline resolution verified'
Invoke-ConformanceCase 'phase16-import-check' @('check', $phase16App) 0 'check succeeded'
Invoke-ConformanceCase 'negative-reserve' @('run', (Join-Path $fixtures 'phase11_negative_reserve.rocket')) 101 'Array reserve capacity cannot be negative'
Invoke-ConformanceCase 'insert-bounds' @('run', (Join-Path $fixtures 'phase11_insert_bounds.rocket')) 101 'index 2 out of bounds for length 1'
Invoke-ConformanceCase 'remove-bounds' @('run', (Join-Path $fixtures 'phase11_remove_bounds.rocket')) 101 'index 1 out of bounds for length 1'
Invoke-ConformanceCase 'checked-overflow' @('run', (Join-Path $fixtures 'int_overflow.rocket')) 101 'Int arithmetic overflow'

$header = @(
    'Rocket 2.0 conformance report'
    "compiler  $Compiler"
    "sha256  $((Get-FileHash -LiteralPath $Compiler -Algorithm SHA256).Hash.ToLowerInvariant())"
    "configuration  $Configuration"
    "cases  $($results.Count)"
    ''
)
Set-Content -LiteralPath $reportPath -Value ($header + $results) -Encoding utf8
Write-Output "Rocket 2.0 conformance passed: $($results.Count) cases ($reportPath)"
