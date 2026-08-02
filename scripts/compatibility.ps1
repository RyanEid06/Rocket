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
    $Compiler = Join-Path $projectRoot "out\bootstrap\windows-$configurationName\stage3.exe"
}
$Compiler = [System.IO.Path]::GetFullPath($Compiler)
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) {
    throw "Compatibility compiler does not exist: $Compiler"
}
$savedStage0 = $env:ROCKET_STAGE0
$stage0 = Join-Path $projectRoot "out\build\windows-$configurationName\rocketc.exe"
if ([string]::IsNullOrWhiteSpace($env:ROCKET_STAGE0) -and
    (Test-Path -LiteralPath $stage0 -PathType Leaf)) {
    $env:ROCKET_STAGE0 = $stage0
}

$results = [System.Collections.Generic.List[object]]::new()
function Invoke-CompatibilityCase {
    param(
        [string]$Release,
        [string]$Name,
        [string[]]$Arguments,
        [string]$ExpectedPattern = ''
    )
    $savedPreference = $script:ErrorActionPreference
    $script:ErrorActionPreference = 'Continue'
    $output = (& $Compiler @Arguments 2>&1) -join "`n"
    $status = $LASTEXITCODE
    $script:ErrorActionPreference = $savedPreference
    if ($status -ne 0) {
        throw "Compatibility case '$Name' failed with status $status.`n$output"
    }
    if ($ExpectedPattern -and $output -notmatch $ExpectedPattern) {
        throw "Compatibility case '$Name' did not match '$ExpectedPattern'.`n$output"
    }
    $results.Add([pscustomobject]@{
        release = $Release
        name = $Name
        status = 'passed'
    })
}

try {
    $fixtures = Join-Path $projectRoot 'tests\fixtures'
    $compatibilityWork = Join-Path $projectRoot "out\compatibility\phase16-packages-$configurationName"
    if (Test-Path -LiteralPath $compatibilityWork) {
        Remove-Item -LiteralPath $compatibilityWork -Recurse -Force
    }
    New-Item -ItemType Directory -Path (Split-Path $compatibilityWork -Parent) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $fixtures 'phase16_packages') `
        -Destination $compatibilityWork -Recurse
    $phase16App = Join-Path $compatibilityWork 'app'
    Invoke-CompatibilityCase '2.0' 'compiler-version' @('--version') '^rocketc 2\.0\.0$'
    Invoke-CompatibilityCase '1.0' 'hello-source' @('run', (Join-Path $projectRoot 'examples\hello.rocket')) 'Hello from Rocket'
    Invoke-CompatibilityCase '1.1' 'collections-source' @('check', (Join-Path $fixtures 'phase11_map_set_tuple.rocket')) 'check succeeded'
    Invoke-CompatibilityCase '1.2' 'traits-source' @('check', (Join-Path $fixtures 'phase12_traits.rocket')) 'check succeeded'
    Invoke-CompatibilityCase '1.3' 'native-package' @('check', (Join-Path $fixtures 'phase13_native_package')) 'check succeeded'
    Invoke-CompatibilityCase '1.4' 'raylib-package' @('check', (Join-Path $projectRoot 'examples\raylib_showcase')) 'check succeeded'
    Invoke-CompatibilityCase '1.5' 'standard-library-source' @('check', (Join-Path $fixtures 'phase15_text_streams.rocket')) 'check succeeded'
    Invoke-CompatibilityCase '1.6' 'package-resolution' @('resolve', $phase16App) 'resolved 3 package'
    Invoke-CompatibilityCase '1.6' 'locked-package-source' @('check', $phase16App) 'check succeeded'
    Invoke-CompatibilityCase '1.7' 'machine-message-schema' @('check', (Join-Path $projectRoot 'examples\hello.rocket'), '--message-format=json') 'rocket-message-1'
    Invoke-CompatibilityCase '1.8' 'ownership-concurrency-source' @('check', (Join-Path $projectRoot 'examples\ownership_concurrency.rocket')) 'check succeeded'

    $reportDirectory = Join-Path $projectRoot 'out\compatibility'
    New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
    $reportPath = Join-Path $reportDirectory "rocket-2.0-$configurationName.json"
    [pscustomobject]@{
        schema = 'rocket-compatibility-1'
        version = '2.0.0'
        target = 'windows-x64'
        configuration = $Configuration
        compiler_sha256 = (Get-FileHash -LiteralPath $Compiler -Algorithm SHA256).Hash.ToLowerInvariant()
        cases = $results
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $reportPath -Encoding utf8
    Write-Output "Rocket 2.0 compatibility passed: $($results.Count) release-line cases ($reportPath)"
} finally {
    $env:ROCKET_STAGE0 = $savedStage0
}
