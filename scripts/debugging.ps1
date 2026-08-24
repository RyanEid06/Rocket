[CmdletBinding()]
param(
    [string]$Compiler = '',
    [string]$OutputDirectory = '',
    [string]$TargetAlias = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $Compiler) {
    . (Join-Path $projectRoot 'dependencies\activate.ps1')
    $Compiler = Join-Path $projectRoot 'out\build\windows-debug\rocketc.exe'
}
$Compiler = [IO.Path]::GetFullPath($Compiler)
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $projectRoot 'out\debugging'
}
if (-not $TargetAlias) {
    $TargetAlias = if ($env:ROCKET_NATIVE_TARGET) {
        $env:ROCKET_NATIVE_TARGET
    } else {
        'windows-x64'
    }
}
if (Test-Path -LiteralPath $OutputDirectory) {
    Remove-Item -LiteralPath $OutputDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$pdbutil = Join-Path $projectRoot 'dependencies\installed\llvm-22.1.6\bin\llvm-pdbutil.exe'
$source = Join-Path $projectRoot 'tests\fixtures\phase6_types.rocket'
$artifactRoot = if ($env:ROCKET_ARTIFACT_ROOT) {
    Join-Path ([IO.Path]::GetFullPath($env:ROCKET_ARTIFACT_ROOT)) 'phase6_types'
} else {
    Split-Path $source -Parent
}
$artifact = Join-Path (Join-Path (Join-Path $artifactRoot '.rocketc') 'targets') $TargetAlias
$records = [Collections.Generic.List[object]]::new()
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

foreach ($configuration in @(
    @{ name = 'debug'; arguments = @('--debug'); optimized = $false },
    @{ name = 'optimized'; arguments = @(); optimized = $true }
)) {
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $Compiler build $source @($configuration.arguments) *> $null
    $ErrorActionPreference = $savedPreference
    if ($LASTEXITCODE -ne 0) { throw "$($configuration.name) debug build failed" }
    $caseDirectory = Join-Path $OutputDirectory $configuration.name
    New-Item -ItemType Directory -Path $caseDirectory | Out-Null
    foreach ($extension in @('.exe', '.pdb', '.rocket.map.json')) {
        Copy-Item -LiteralPath (Join-Path $artifact "phase6_types$extension") -Destination $caseDirectory
    }
    $mapPath = Join-Path $caseDirectory 'phase6_types.rocket.map.json'
    $map = Get-Content -LiteralPath $mapPath -Raw | ConvertFrom-Json
    if ($map.format -ne 'rocket-source-map-1' -or
        [bool]$map.optimized -ne [bool]$configuration.optimized -or
        $map.functions.Count -lt 2) {
        throw "$($configuration.name) Rocket source map is incomplete"
    }
    $pdb = Join-Path $caseDirectory 'phase6_types.pdb'
    $pdbText = (& $pdbutil dump --modi=0 --symbols --files -l $pdb 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0 -or $pdbText -notmatch 'rocket:\\source\\phase6_types\.rocket' -or
        $pdbText -notmatch 'line/column/addr entries' -or
        $pdbText -notmatch 'S_GPROC32' -or $pdbText -notmatch 'S_(LOCAL|CONSTANT)') {
        throw "$($configuration.name) PDB lacks Rocket source, line, function, or variable records"
    }
    $records.Add([pscustomobject]@{
        configuration = $configuration.name
        optimized = $configuration.optimized
        executable_sha256 = Get-Sha256 -Path (Join-Path $caseDirectory 'phase6_types.exe')
        pdb_sha256 = Get-Sha256 -Path $pdb
        source_map_sha256 = Get-Sha256 -Path $mapPath
        functions = $map.functions.Count
    })
}

$panicSource = Join-Path $projectRoot 'tests\fixtures\int_overflow.rocket'
$savedPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $Compiler build $panicSource --debug *> $null
$ErrorActionPreference = $savedPreference
if ($LASTEXITCODE -ne 0) { throw 'panic-location fixture build failed' }
$panicRoot = if ($env:ROCKET_ARTIFACT_ROOT) {
    Join-Path ([IO.Path]::GetFullPath($env:ROCKET_ARTIFACT_ROOT)) 'int_overflow'
} else {
    Split-Path $panicSource -Parent
}
$panicArtifact = Join-Path (Join-Path (Join-Path $panicRoot '.rocketc') 'targets') $TargetAlias
$panicMap = Get-Content (Join-Path $panicArtifact 'int_overflow.rocket.map.json') -Raw
if ($panicMap -notmatch 'int_overflow\.rocket' -or $panicMap -notmatch '"line":') {
    throw 'panic source map lacks the failing Rocket location'
}

$reportPath = Join-Path $OutputDirectory 'report.json'
[pscustomobject]@{
    schema = 'rocket-debug-validation-1'
    debugger_contract = 'CodeView/PDB plus rocket-source-map-1'
    configurations = $records
    panic_location = 'int_overflow.rocket'
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $reportPath -Encoding utf8
Write-Output "Rocket debug validation passed: $reportPath"
