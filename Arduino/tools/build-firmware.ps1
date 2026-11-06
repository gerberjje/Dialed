#requires -version 7
# Builds the mixer/ firmware into a single merged image flashable at offset 0x0,
# and writes firmware/esp32/manifest.json. Version/board are read from
# Arduino/mixer/version.h so the .bin name, manifest, and device-reported version
# can never drift. Run this by hand when the firmware changes; it is NOT part of
# the dotnet build.
param(
  [ValidateSet('mixer', 'touch')]
  [string]$Controller = 'mixer',
  [string]$ConfigFile
)
$ErrorActionPreference = 'Stop'

$repo      = Resolve-Path "$PSScriptRoot\..\.."
$sketch    = Join-Path $repo 'Arduino/mixer'
$cliArgs   = @()
if ($ConfigFile) { $cliArgs = @('--config-file', $ConfigFile) }
$versionH  = Join-Path $sketch 'version.h'
$esptool   = Join-Path $repo 'tools\esptool\esptool.exe'
$outDir    = Join-Path $repo 'firmware\esp32'
$buildDir  = Join-Path $env:TEMP "dialed-fw-build-$Controller"

function Read-Define([string]$name) {
  $line = Select-String -Path $versionH -Pattern "#define\s+$name\s+`"([^`"]+)`"" | Select-Object -First 1
  if (-not $line) { throw "Could not find #define $name in $versionH" }
  return $line.Matches[0].Groups[1].Value
}

$board   = if ($Controller -eq 'touch') { Read-Define 'FW_BOARD_TOUCH' } else { Read-Define 'FW_BOARD' }
$version = Read-Define 'FW_VERSION'
Write-Host "Building $board $version"

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
New-Item -ItemType Directory -Force -Path $outDir   | Out-Null

# Select the matching display driver/pins before compiling either hardware target.
$userDir = (arduino-cli @cliArgs config get directories.user | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or -not $userDir) { throw 'Could not resolve Arduino library directory' }
$tftSetup = Join-Path $userDir 'libraries/TFT_eSPI/User_Setup.h'
if (-not (Test-Path $tftSetup)) { throw "TFT_eSPI not found: $tftSetup" }
$setupName = if ($Controller -eq 'touch') { 'User_Setup_touch.h' } else { 'User_Setup.h' }
Copy-Item -LiteralPath (Join-Path $sketch $setupName) -Destination $tftSetup -Force

# 1. Compile -> bootloader/partitions/app binaries in $buildDir
$compileArgs = @('compile', '--fqbn', 'esp32:esp32:esp32', '--output-dir', $buildDir)
$flags = '-DDIALED_CONTROLLER_TOUCH=0'
if ($Controller -eq 'touch') {
  # Force the touch setup: Arduino also adds the sketch directory to the include path,
  # where the original mixer User_Setup.h can shadow TFT_eSPI's library copy.
  $setup = (Join-Path $sketch 'User_Setup_touch.h').Replace('\', '/')
  $flags = '-DDIALED_CONTROLLER_TOUCH=1 -DSPI_DRIVER_SELECT=2 -DMAINTAIN_FREE_CLUSTER_COUNT=1 -DUSER_SETUP_LOADED -include "' + $setup + '"'
}
$compileArgs += @('--build-property', "compiler.cpp.extra_flags=$flags")
arduino-cli @cliArgs @compileArgs $sketch
if ($LASTEXITCODE -ne 0) { throw "arduino-cli compile failed" }

$appBin  = Join-Path $buildDir "mixer.ino.bin"
$bootBin = Join-Path $buildDir "mixer.ino.bootloader.bin"
$partBin = Join-Path $buildDir "mixer.ino.partitions.bin"
foreach ($f in @($appBin,$bootBin,$partBin)) {
  if (-not (Test-Path $f)) { throw "Expected build artifact missing: $f" }
}

# boot_app0.bin ships with the ESP32 Arduino core. Locate it under the arduino-cli data dir.
$dataDir = (arduino-cli @cliArgs config get directories.data | Out-String).Trim()
$bootApp0 = Get-ChildItem -Path $dataDir -Recurse -Filter 'boot_app0.bin' `
            -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $bootApp0) { throw "boot_app0.bin not found under $dataDir (install esp32 core)" }

# 2. Merge into one image (classic ESP32 offsets; merge-bin pads from 0x0).
#    esptool v5 command/flag syntax (hyphens).
$binName = "$board-$version.bin"
$mergedBin = Join-Path $outDir $binName
& $esptool --chip esp32 merge-bin -o $mergedBin `
  --flash-mode dio --flash-freq 40m --flash-size 4MB `
  0x1000 $bootBin 0x8000 $partBin 0xe000 $bootApp0.FullName 0x10000 $appBin
if ($LASTEXITCODE -ne 0) { throw "esptool merge-bin failed" }

# 3. Write manifest.json (sha256 of the merged bin)
$sha = (Get-FileHash -Algorithm SHA256 -Path $mergedBin).Hash.ToLowerInvariant()
$manifest = [ordered]@{ board = $board; version = $version; bin = $binName; sha256 = $sha }
# Each controller keeps its own descriptor; touch must never replace mixer.
$manifestName = if ($Controller -eq 'touch') { 'manifest-touch.json' } else { 'manifest.json' }
$manifestPath = Join-Path $outDir $manifestName
$manifest | ConvertTo-Json | Set-Content -Path $manifestPath -Encoding UTF8

Write-Host "Wrote $mergedBin"
Write-Host "Wrote $manifestPath (sha256 $sha)"
