[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$Compiler,
    [Parameter(Mandatory)] [string]$WorkDirectory
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path $WorkDirectory -Force | Out-Null
$sourcePath = Join-Path $WorkDirectory 'formatter-input.rocket'
$source = "import  std.string   `r`n`r`n# module comment   `r`nfn  main( )->Int: # entry   `r`n    let  values : Array [ Int ] = [ 1,2, 3 ]`r`n    let negative=-1`r`n    if  not false and values [0]==1:`r`n        print ( string.concat(`"A#`",`"B`") ) # result`r`n    return negative+2`r`n"
$expected = "import std.string`n`n# module comment`nfn main() -> Int:  # entry`n    let values: Array[Int] = [1, 2, 3]`n    let negative = -1`n    if not false and values[0] == 1:`n        print(string.concat(`"A#`", `"B`"))  # result`n    return negative + 2`n"
[System.IO.File]::WriteAllText($sourcePath, $source, [System.Text.UTF8Encoding]::new($false))

& $Compiler fmt $sourcePath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$actual = [System.IO.File]::ReadAllText($sourcePath)
if ($actual -cne $expected) {
    throw 'The self-hosted formatter did not produce canonical source.'
}
& $Compiler fmt $sourcePath --check
exit $LASTEXITCODE
