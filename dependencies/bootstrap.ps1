[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$python = Get-Command py.exe -ErrorAction SilentlyContinue
if ($python) {
    & $python.Source -3 (Join-Path $PSScriptRoot 'bootstrap.py')
} else {
    $python = Get-Command python.exe -ErrorAction Stop
    & $python.Source (Join-Path $PSScriptRoot 'bootstrap.py')
}
exit $LASTEXITCODE
