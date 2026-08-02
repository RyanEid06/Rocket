[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Compiler,
    [Parameter(Mandatory = $true)]
    [string]$InputPath,
    [Parameter(Mandatory = $true)]
    [string]$OutputPath,
    [int]$ExpectedExitCode = -1,
    [string]$MatchText = ''
)

$ErrorActionPreference = 'Stop'
$compilerPath = [System.IO.Path]::GetFullPath($Compiler)
$inputPath = [System.IO.Path]::GetFullPath($InputPath)
$outputPath = [System.IO.Path]::GetFullPath($OutputPath)
if (-not (Test-Path -LiteralPath $compilerPath -PathType Leaf)) {
    throw "Compiler does not exist: $compilerPath"
}
if (-not (Test-Path -LiteralPath $inputPath -PathType Leaf)) {
    throw "Reproducer does not exist: $inputPath"
}
$outputParent = Split-Path $outputPath -Parent
New-Item -ItemType Directory -Path $outputParent -Force | Out-Null
$candidatePath = Join-Path $outputParent 'rocket-minimize-candidate.rocket'
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)

function Write-Candidate {
    param([string]$Path, [string[]]$Lines)
    [System.IO.File]::WriteAllText($Path, (($Lines -join "`n") + "`n"), $utf8NoBom)
}

function Invoke-Reproducer {
    param([string[]]$Lines, [int]$RequiredExitCode)
    Write-Candidate -Path $candidatePath -Lines $Lines
    $savedPreference = $script:ErrorActionPreference
    $script:ErrorActionPreference = 'Continue'
    $text = (& $compilerPath check $candidatePath 2>&1) -join "`n"
    $status = $LASTEXITCODE
    $script:ErrorActionPreference = $savedPreference
    return [pscustomobject]@{
        matches = ($status -eq $RequiredExitCode -and
            (-not $MatchText -or $text -match $MatchText))
        status = $status
        text = $text
    }
}

try {
    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($line in (Get-Content -LiteralPath $inputPath)) { $lines.Add($line) }
    if ($lines.Count -eq 0) { $lines.Add('') }

    Write-Candidate -Path $candidatePath -Lines $lines
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $initialText = (& $compilerPath check $candidatePath 2>&1) -join "`n"
    $initialStatus = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    if ($ExpectedExitCode -lt 0) { $ExpectedExitCode = $initialStatus }
    if ($initialStatus -ne $ExpectedExitCode -or
        ($MatchText -and $initialText -notmatch $MatchText)) {
        throw "Input does not reproduce exit $ExpectedExitCode and pattern '$MatchText'."
    }

    $granularity = 2
    while ($lines.Count -gt 1) {
        $chunkSize = [Math]::Ceiling($lines.Count / [double]$granularity)
        $reduced = $false
        for ($start = 0; $start -lt $lines.Count; $start += $chunkSize) {
            $candidate = [System.Collections.Generic.List[string]]::new()
            for ($index = 0; $index -lt $lines.Count; ++$index) {
                if ($index -lt $start -or $index -ge $start + $chunkSize) {
                    $candidate.Add($lines[$index])
                }
            }
            if ($candidate.Count -eq 0) { $candidate.Add('') }
            $attempt = Invoke-Reproducer -Lines $candidate -RequiredExitCode $ExpectedExitCode
            if ($attempt.matches) {
                $lines = $candidate
                $granularity = [Math]::Max(2, $granularity - 1)
                $reduced = $true
                break
            }
        }
        if (-not $reduced) {
            if ($granularity -ge $lines.Count) { break }
            $granularity = [Math]::Min($lines.Count, $granularity * 2)
        }
    }

    Write-Candidate -Path $outputPath -Lines $lines
    $verified = Invoke-Reproducer -Lines $lines -RequiredExitCode $ExpectedExitCode
    if (-not $verified.matches) { throw 'Minimized output no longer reproduces.' }
    Write-Output "Minimized compiler reproducer to $($lines.Count) line(s): $outputPath"
} finally {
    if (Test-Path -LiteralPath $candidatePath) {
        Remove-Item -LiteralPath $candidatePath -Force
    }
}
