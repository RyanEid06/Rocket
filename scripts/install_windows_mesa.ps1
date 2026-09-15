param(
  [Parameter(Mandatory = $true)]
  [string[]] $Destination
)

$ErrorActionPreference = 'Stop'
$mesaVersion = '26.2.0'
$mesaSha256 = 'dcb2719ef346dab5b609fcb193a5f13cfc4b0502e3f4de1ad43d349477402f47'
$mesaUrl = "https://github.com/pal1000/mesa-dist-win/releases/download/$mesaVersion/mesa3d-$mesaVersion-release-msvc.7z"
$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) "rocket-mesa-$PID"
$archive = Join-Path $temporaryRoot 'mesa.7z'
$extractRoot = Join-Path $temporaryRoot 'extract'

try {
  New-Item -ItemType Directory -Force $temporaryRoot, $extractRoot | Out-Null
  & curl.exe -L --fail --retry 3 --output $archive $mesaUrl
  if ($LASTEXITCODE -ne 0) { throw "Mesa download failed with exit code $LASTEXITCODE" }

  $actualSha256 = (Get-FileHash -Algorithm SHA256 $archive).Hash.ToLowerInvariant()
  if ($actualSha256 -ne $mesaSha256) {
    throw "Mesa SHA-256 mismatch: expected $mesaSha256, got $actualSha256"
  }

  $sevenZip = Join-Path $env:ProgramFiles '7-Zip\7z.exe'
  & $sevenZip x $archive "-o$extractRoot" -y | Out-Null
  if ($LASTEXITCODE -ne 0) { throw "Mesa extraction failed with exit code $LASTEXITCODE" }

  $mesaFiles = @('opengl32.dll', 'libgallium_wgl.dll')
  foreach ($name in $mesaFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $extractRoot "x64\$name"))) {
      throw "Mesa archive is missing x64/$name"
    }
  }

  foreach ($path in $Destination) {
    $resolvedDestination = [System.IO.Path]::GetFullPath($path)
    New-Item -ItemType Directory -Force $resolvedDestination | Out-Null
    foreach ($name in $mesaFiles) {
      Copy-Item -LiteralPath (Join-Path $extractRoot "x64\$name") -Destination $resolvedDestination
    }
    if ($env:GITHUB_PATH) { Add-Content -LiteralPath $env:GITHUB_PATH -Value $resolvedDestination }
  }
  if ($env:GITHUB_ENV) {
    Add-Content -LiteralPath $env:GITHUB_ENV -Value 'GALLIUM_DRIVER=llvmpipe'
    Add-Content -LiteralPath $env:GITHUB_ENV -Value 'LIBGL_ALWAYS_SOFTWARE=1'
  }
} finally {
  $resolvedTemporaryRoot = [System.IO.Path]::GetFullPath($temporaryRoot)
  $resolvedSystemTemp = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
  if ($resolvedTemporaryRoot.StartsWith($resolvedSystemTemp, [System.StringComparison]::OrdinalIgnoreCase) -and
      (Test-Path -LiteralPath $resolvedTemporaryRoot)) {
    Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
  }
}
