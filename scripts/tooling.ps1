[CmdletBinding()]
param([string]$Compiler = '')

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $Compiler) {
    . (Join-Path $projectRoot 'dependencies\activate.ps1')
    $Compiler = Join-Path $projectRoot 'out\build\windows-debug\rocketc.exe'
}
$work = Join-Path $projectRoot 'out\tooling'
New-Item -ItemType Directory -Path $work -Force | Out-Null
$hello = Join-Path $projectRoot 'examples\hello.rocket'
$coverage = Join-Path $work 'coverage.json'
$profile = Join-Path $work 'profile.json'
$benchmark = Join-Path $work 'benchmark.json'

$savedPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $Compiler coverage $hello --output $coverage *> $null
if ($LASTEXITCODE -ne 0) { throw 'coverage command failed' }
& $Compiler profile $hello --output $profile *> $null
if ($LASTEXITCODE -ne 0) { throw 'profile command failed' }
& $Compiler benchmark $hello --iterations 3 --output $benchmark *> $null
if ($LASTEXITCODE -ne 0) { throw 'benchmark command failed' }
$messages = @(& $Compiler check $hello --message-format=json 2>$null)
if ($LASTEXITCODE -ne 0) { throw 'machine-readable check failed' }
$buildMessages = @(& $Compiler build $hello --message-format=json 2>$null)
if ($LASTEXITCODE -ne 0) { throw 'machine-readable build failed' }
$testPackage = Join-Path $projectRoot 'tests\fixtures\phase15_test_package'
$testMessages = @(& $Compiler test $testPackage --message-format=json 2>$null)
if ($LASTEXITCODE -ne 0) { throw 'machine-readable test failed' }
$ErrorActionPreference = $savedPreference

if ((Get-Content $coverage -Raw | ConvertFrom-Json).schema -ne 'rocket-coverage-1') {
    throw 'coverage schema mismatch'
}
if ((Get-Content $profile -Raw | ConvertFrom-Json).schema -ne 'rocket-profile-1') {
    throw 'profile schema mismatch'
}
if ((Get-Content $benchmark -Raw | ConvertFrom-Json).schema -ne 'rocket-benchmark-1') {
    throw 'benchmark schema mismatch'
}
foreach ($stream in @($messages, $buildMessages, $testMessages)) {
    foreach ($line in $stream) {
        $message = $line | ConvertFrom-Json
        if ($message.schema -ne 'rocket-message-1') {
            throw 'machine-readable compiler message schema mismatch'
        }
    }
}
if (($messages | ConvertFrom-Json).reason -ne 'build-finished') {
    throw 'machine-readable check completion missing'
}
if (($buildMessages | ConvertFrom-Json).reason -ne 'build-finished') {
    throw 'machine-readable build completion missing'
}
$testReasons = @($testMessages | ForEach-Object { ($_ | ConvertFrom-Json).reason })
if ('test-summary' -notin $testReasons) {
    throw 'machine-readable test summary missing'
}
Write-Output "Rocket coverage, profile, benchmark, and machine-output validation passed: $work"
