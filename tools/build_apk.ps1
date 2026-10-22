# Build DigiCore control APK without Gradle: javac -> d8 -> aapt2 -> zipalign -> apksigner
# Usage: powershell -ExecutionPolicy Bypass -File tools\build_apk.ps1
$ErrorActionPreference = 'Stop'

$Root     = Split-Path -Parent $PSScriptRoot
$Sdk      = Join-Path $env:LOCALAPPDATA "Android\Sdk"
$Bt       = Join-Path $Sdk "build-tools\36.1.0"
$Android  = Join-Path $Sdk "platforms\android-34\android.jar"
$Src      = Join-Path $Root "android\app\src\main"
$Build    = Join-Path $Root "android\build"
$OutDir   = Join-Path $Root "docs"
$Apk      = Join-Path $OutDir "DigiCore.apk"
$Keystore = Join-Path $env:USERPROFILE ".android\debug.keystore"

foreach ($p in @($Bt, $Android, $Keystore)) {
    if (!(Test-Path $p)) { throw "missing: $p" }
}

Write-Host "[1/7] prepare"
if (Test-Path $Build) { Remove-Item $Build -Recurse -Force }
New-Item -ItemType Directory -Force -Path "$Build\classes" | Out-Null
New-Item -ItemType Directory -Force -Path "$Build\dex" | Out-Null
New-Item -ItemType Directory -Force -Path "$Build\gen" | Out-Null
New-Item -ItemType Directory -Force -Path "$Src\assets" | Out-Null

Copy-Item (Join-Path $Root "app\index.html") (Join-Path $Src "assets\index.html") -Force

Write-Host "[2/7] javac"
$files = @(Get-ChildItem "$Src\java" -Recurse -Filter *.java | ForEach-Object { $_.FullName })
if ($files.Count -eq 0) { throw "no java sources" }
$javacArgs = @('-encoding', 'UTF-8', '-source', '8', '-target', '8',
               '-bootclasspath', $Android, '-d', "$Build\classes") + $files
& javac @javacArgs
if ($LASTEXITCODE -ne 0) { throw "javac failed" }

Write-Host "[3/7] d8"
$classes = @(Get-ChildItem "$Build\classes" -Recurse -Filter *.class | ForEach-Object { $_.FullName })
$d8Args = @('--lib', $Android, '--min-api', '23', '--output', "$Build\dex") + $classes
& (Join-Path $Bt "d8.bat") @d8Args
if ($LASTEXITCODE -ne 0) { throw "d8 failed" }

Write-Host "[4/7] aapt2"
& (Join-Path $Bt "aapt2.exe") compile --dir (Join-Path $Src "res") -o "$Build\res.zip"
if ($LASTEXITCODE -ne 0) { throw "aapt2 compile failed" }

$linkArgs = @(
    'link', '-o', "$Build\base.apk", '-I', $Android,
    '--manifest', (Join-Path $Src "AndroidManifest.xml"),
    '-A', (Join-Path $Src "assets"),
    '--java', "$Build\gen",
    '--min-sdk-version', '23', '--target-sdk-version', '34',
    "$Build\res.zip"
)
& (Join-Path $Bt "aapt2.exe") @linkArgs
if ($LASTEXITCODE -ne 0) { throw "aapt2 link failed" }

Write-Host "[5/7] add classes.dex"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open("$Build\base.apk", [System.IO.Compression.ZipArchiveMode]::Update)
try {
    $entry = $zip.CreateEntry("classes.dex", [System.IO.Compression.CompressionLevel]::NoCompression)
    $es = $entry.Open()
    try {
        $bytes = [System.IO.File]::ReadAllBytes("$Build\dex\classes.dex")
        $es.Write($bytes, 0, $bytes.Length)
    } finally { $es.Dispose() }
} finally { $zip.Dispose() }

Write-Host "[6/7] zipalign"
& (Join-Path $Bt "zipalign.exe") -f 4 "$Build\base.apk" "$Build\aligned.apk"
if ($LASTEXITCODE -ne 0) { throw "zipalign failed" }

Write-Host "[7/7] sign"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
if (Test-Path $Apk) { Remove-Item $Apk -Force }
$signArgs = @('sign', '--ks', $Keystore, '--ks-pass', 'pass:android', '--key-pass', 'pass:android',
              '--v1-signing-enabled', 'true', '--v2-signing-enabled', 'true',
              '--out', $Apk, "$Build\aligned.apk")
& (Join-Path $Bt "apksigner.bat") @signArgs
if ($LASTEXITCODE -ne 0) { throw "apksigner failed" }

$verify = & (Join-Path $Bt "apksigner.bat") verify --print-certs $Apk 2>&1
$verify | Select-Object -First 2

$size = (Get-Item $Apk).Length / 1KB
Write-Host ""
Write-Host ("OK -> {0}  ({1} KB)" -f $Apk, [math]::Round($size, 1))
