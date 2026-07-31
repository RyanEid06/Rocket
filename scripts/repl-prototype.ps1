[CmdletBinding()]
param(
    [string]$Compiler = '',
    [switch]$Measure,
    [string[]]$Expression
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $Compiler) { $Compiler = Join-Path $projectRoot 'out\build\windows-debug\rocketc.exe' }
$session = Join-Path $projectRoot 'out\repl-prototype'
New-Item -ItemType Directory -Path $session -Force | Out-Null
$source = Join-Path $session 'session.rocket'
$expressions = [Collections.Generic.List[string]]::new()
if ($Expression) { foreach ($item in $Expression) { $expressions.Add($item) } }
if (-not $Expression) {
    Write-Host 'Rocket incremental-AOT REPL prototype. Enter expressions; :quit exits; :reset clears history.'
    while ($true) {
        $line = Read-Host 'rocket>'
        if ($line -eq ':quit') { break }
        if ($line -eq ':reset') { $expressions.Clear(); continue }
        $expressions.Add($line)
        & $PSCommandPath -Compiler $Compiler -Expression $expressions.ToArray()
    }
    exit 0
}

$body = @('fn main() -> Int:')
foreach ($item in $expressions) { $body += "    print($item)" }
$body += '    return 0'
[IO.File]::WriteAllText($source, (($body -join "`n") + "`n"), [Text.UTF8Encoding]::new($false))
$watch = [Diagnostics.Stopwatch]::StartNew()
$savedPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $Compiler run $source
$status = $LASTEXITCODE
$ErrorActionPreference = $savedPreference
$watch.Stop()
if ($Measure) {
    [pscustomobject]@{
        schema = 'rocket-repl-evaluation-1'
        status = $status
        elapsed_ms = [math]::Round($watch.Elapsed.TotalMilliseconds, 3)
        model = 'incremental source accumulation with cached AOT artifacts'
    } | ConvertTo-Json -Compress
}
exit $status
