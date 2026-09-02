[CmdletBinding()]
param(
    [string]$Compiler = '',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateRange(4, 128)]
    [int]$PackageCount = 32,
    [ValidateRange(1, 100)]
    [int]$Iterations = 5,
    [string]$WorkDirectory = '',
    [string]$TargetAlias = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$configurationName = $Configuration.ToLowerInvariant()
if (-not $Compiler) {
    $Compiler = Join-Path $projectRoot "out\bootstrap\windows-$configurationName\stage3.exe"
}
$Compiler = [System.IO.Path]::GetFullPath($Compiler)
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) {
    throw "Application-validation compiler does not exist: $Compiler"
}
$savedStage0 = $env:ROCKET_STAGE0
$stage0 = Join-Path $projectRoot "out\build\windows-$configurationName\rocketc.exe"
if ([string]::IsNullOrWhiteSpace($env:ROCKET_STAGE0) -and
    (Test-Path -LiteralPath $stage0 -PathType Leaf)) {
    $env:ROCKET_STAGE0 = $stage0
}

$outRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'out'))
if (-not $WorkDirectory) {
    $WorkDirectory = Join-Path $outRoot "application-validation\$configurationName"
}
$work = [System.IO.Path]::GetFullPath($WorkDirectory)
if (-not $work.StartsWith(
        $outRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to replace an application-validation path outside out/.'
}
if (-not $TargetAlias) {
    $TargetAlias = if ($env:ROCKET_NATIVE_TARGET) {
        $env:ROCKET_NATIVE_TARGET
    } else {
        'windows-x64'
    }
}
if (Test-Path -LiteralPath $work) {
    Remove-Item -LiteralPath $work -Recurse -Force
}
New-Item -ItemType Directory -Path $work -Force | Out-Null
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)

function Write-Utf8NoBom {
    param([string]$Path, [string]$Value)
    [System.IO.File]::WriteAllText($Path, $Value, $utf8NoBom)
}

function Get-Sha256 {
    param([string]$Path)
    $stream = [System.IO.File]::OpenRead($Path)
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString(
            $algorithm.ComputeHash($stream))).Replace('-', '').ToLowerInvariant()
    } finally {
        $algorithm.Dispose()
        $stream.Dispose()
    }
}

function Invoke-Rocket {
    param([string[]]$Arguments, [string]$ExpectedPattern = '')
    $savedPreference = $script:ErrorActionPreference
    $script:ErrorActionPreference = 'Continue'
    $output = (& $Compiler @Arguments 2>&1) -join "`n"
    $status = $LASTEXITCODE
    $script:ErrorActionPreference = $savedPreference
    if ($status -ne 0) {
        throw "Rocket command failed ($status): $($Arguments -join ' ')`n$output"
    }
    if ($ExpectedPattern -and $output -notmatch $ExpectedPattern) {
        throw "Rocket output did not match '$ExpectedPattern'.`n$output"
    }
    return $output
}

try {
    for ($index = 0; $index -lt $PackageCount; ++$index) {
        $name = 'layer_{0:d3}' -f $index
        $root = Join-Path $work $name
        New-Item -ItemType Directory -Path (Join-Path $root 'src') -Force | Out-Null
        $manifest = "[package]`nname = `"$name`"`nversion = `"1.0.0`"`nlicense = `"MIT`"`nentry = `"src/main.rocket`"`n"
        if ($index -eq 0) {
            $source = "pub fn value() -> Int:`n    return 1`n"
        } else {
            $previous = 'layer_{0:d3}' -f ($index - 1)
            $manifest += "`n[dependencies]`n$previous = `"path:../$previous`"`n"
            $source = "import $previous`n`npub fn value() -> Int:`n    return $previous.value() + 1`n"
        }
        Write-Utf8NoBom -Path (Join-Path $root 'rocket.toml') -Value $manifest
        Write-Utf8NoBom -Path (Join-Path $root 'src\main.rocket') -Value $source
    }

    $last = 'layer_{0:d3}' -f ($PackageCount - 1)
    $app = Join-Path $work 'application'
    New-Item -ItemType Directory -Path (Join-Path $app 'src') -Force | Out-Null
    $appManifest = "[package]`nname = `"phase20_application`"`nversion = `"2.0.0`"`nlicense = `"MIT`"`nentry = `"src/main.rocket`"`n`n[dependencies]`n$last = `"path:../$last`"`n"
    $appSource = "import $last`n`nfn main() -> Int:`n    print($last.value())`n    return 0`n"
    Write-Utf8NoBom -Path (Join-Path $app 'rocket.toml') -Value $appManifest
    Write-Utf8NoBom -Path (Join-Path $app 'src\main.rocket') -Value $appSource

    Invoke-Rocket @('resolve', $app) 'resolved'
    Invoke-Rocket @('build', $app) 'built'
    for ($iteration = 0; $iteration -lt $Iterations; ++$iteration) {
        Invoke-Rocket @('run', $app) ("(?m)^$PackageCount$")
    }

    $parallelProcesses = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
    foreach ($suffix in 'a', 'b') {
        $parallelRoot = Join-Path $work "parallel_$suffix"
        New-Item -ItemType Directory -Path (Join-Path $parallelRoot 'src') -Force | Out-Null
        $parallelManifest = "[package]`nname = `"parallel_$suffix`"`nversion = `"2.0.0`"`nentry = `"src/main.rocket`"`n"
        $parallelSource = "fn main() -> Int:`n    return 0`n"
        Write-Utf8NoBom -Path (Join-Path $parallelRoot 'rocket.toml') -Value $parallelManifest
        Write-Utf8NoBom -Path (Join-Path $parallelRoot 'src\main.rocket') -Value $parallelSource
        $process = Start-Process -FilePath $Compiler -ArgumentList @('build', "`"$parallelRoot`"") -PassThru -WindowStyle Hidden
        $parallelProcesses.Add($process)
    }
    foreach ($process in $parallelProcesses) {
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            throw "Parallel package build failed with status $($process.ExitCode)."
        }
        $process.Dispose()
    }

    Invoke-Rocket @('test', (Join-Path $projectRoot 'examples\raylib_showcase')) '5 passed; 0 failed'
    Invoke-Rocket @('run', (Join-Path $projectRoot 'examples\ownership_concurrency.rocket')) '41[\r\n]+3[\r\n]+42'

    $reportPath = Join-Path $work 'application-validation.json'
    [pscustomobject]@{
        schema = 'rocket-application-validation-1'
        version = '2.1.0'
        target = $TargetAlias
        configuration = $Configuration
        package_count = $PackageCount + 1
        repeated_runs = $Iterations
        parallel_package_builds = 2
        raylib_headless_tests = 5
        ownership_concurrency_application = 'passed'
        compiler_sha256 = Get-Sha256 -Path $Compiler
    } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $reportPath -Encoding utf8
    Write-Output "Rocket 2.1 application validation passed: $reportPath"
} finally {
    $env:ROCKET_STAGE0 = $savedStage0
}
