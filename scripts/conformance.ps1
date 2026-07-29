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
$reportPath = Join-Path $reportDirectory "rocket-1.2-development-$configurationName.txt"
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

Invoke-ConformanceCase 'version' @('--version') 0 '^rocketc 1\.2\.0$'
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
Invoke-ConformanceCase 'package-check' @('check', (Join-Path $fixtures 'phase8_package')) 0 'check succeeded'
Invoke-ConformanceCase 'package-format' @('fmt', (Join-Path $fixtures 'phase8_package'), '--check') 0 'format check succeeded'
Invoke-ConformanceCase 'package-run' @('run', (Join-Path $fixtures 'phase8_package')) 0 '(?m)^42$'
Invoke-ConformanceCase 'package-test' @('test', (Join-Path $fixtures 'phase8_package')) 0 '2 passed; 0 failed'
Invoke-ConformanceCase 'private-visibility-diagnostic' @('check', (Join-Path $fixtures 'phase6_visibility.rocket')) 1 'R3003'
Invoke-ConformanceCase 'import-cycle-diagnostic' @('check', (Join-Path $fixtures 'phase6_cycle.rocket')) 1 'R3002'
Invoke-ConformanceCase 'invalid-map-key-diagnostic' @('check', (Join-Path $fixtures 'phase11_invalid_key.rocket')) 1 'R4001'
Invoke-ConformanceCase 'invalid-method-receiver' @('check', (Join-Path $fixtures 'phase12_invalid_receiver.rocket')) 1 'method receiver must have impl type Counter'
Invoke-ConformanceCase 'negative-reserve' @('run', (Join-Path $fixtures 'phase11_negative_reserve.rocket')) 101 'Array reserve capacity cannot be negative'
Invoke-ConformanceCase 'insert-bounds' @('run', (Join-Path $fixtures 'phase11_insert_bounds.rocket')) 101 'index 2 out of bounds for length 1'
Invoke-ConformanceCase 'remove-bounds' @('run', (Join-Path $fixtures 'phase11_remove_bounds.rocket')) 101 'index 1 out of bounds for length 1'
Invoke-ConformanceCase 'checked-overflow' @('run', (Join-Path $fixtures 'int_overflow.rocket')) 101 'Int arithmetic overflow'

$header = @(
    'Rocket 1.2 development conformance report'
    "compiler  $Compiler"
    "sha256  $((Get-FileHash -LiteralPath $Compiler -Algorithm SHA256).Hash.ToLowerInvariant())"
    "configuration  $Configuration"
    "cases  $($results.Count)"
    ''
)
Set-Content -LiteralPath $reportPath -Value ($header + $results) -Encoding utf8
Write-Output "Rocket 1.2 development conformance passed: $($results.Count) cases ($reportPath)"
