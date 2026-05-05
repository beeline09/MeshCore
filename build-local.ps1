#Requires -Version 5.1
# build-local.ps1 — build specified PlatformIO envs and produce all firmware types.
#
# Usage:
#   .\build-local.ps1 Heltec_E213_companion_radio_ble
#   .\build-local.ps1 Heltec_E213_companion_radio_ble, Heltec_E290_companion_ble, LilyGo_T-Echo_companion_radio_ble
#   .\build-local.ps1 -Envs Heltec_E213_companion_radio_ble -DisableDebug
#
# Output files land in out\ with names matching the GitHub Action convention:
#   <env>-<version>-<hash>.bin          (ESP32 update binary)
#   <env>-<version>-<hash>-merged.bin   (ESP32 full flash)
#   <env>-<version>-<hash>.uf2          (NRF52/RP2040 USB drag-and-drop)
#   <env>-<version>-<hash>.zip          (NRF52 BLE DFU OTA)

param(
    [Parameter(Mandatory, Position = 0, ValueFromRemainingArguments)]
    [string[]] $Envs,

    [switch] $DisableDebug
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$BASH = 'C:\Program Files\Git\bin\bash.exe'
$UF2CONV = "bin\uf2conv\uf2conv.py"

# ── Version from last tag ─────────────────────────────────────────────────────
$lastTag = git describe --tags --abbrev=0 2>$null
if (-not $lastTag) { $lastTag = 'v0.0.0' }
# Strip everything up to and including the last '-' (removes "companion-" prefix etc.)
$gitTagVersion = $lastTag -replace '^.*-(?=v\d)', ''
$commitHash = (git rev-parse --short HEAD).Trim()
$versionString = "$gitTagVersion-$commitHash"

Write-Host "Tag:     $lastTag"
Write-Host "Version: $versionString"
Write-Host ""

# ── Platform detection via pio project config --json-output ───────────────────
$pioConfigJson = (pio project config --json-output 2>$null) -join "`n"

function Get-EnvPlatform([string]$EnvName) {
    $data = $pioConfigJson | python3 -c @"
import sys, json, re
data = json.load(sys.stdin)
for section, options in data:
    if section == 'env:$EnvName':
        for key, value in options:
            if key == 'build_flags':
                for flag in value:
                    m = re.search(r'(ESP32_PLATFORM|NRF52_PLATFORM|STM32_PLATFORM|RP2040_PLATFORM)', flag)
                    if m:
                        print(m.group(1))
                        sys.exit(0)
"@
    return $data.Trim()
}

# ── Debug flag stripping ──────────────────────────────────────────────────────
$debugFlags = ''
if ($DisableDebug) {
    $debugFlags = '-UMESH_DEBUG -UBLE_DEBUG_LOGGING -UWIFI_DEBUG_LOGGING -UBRIDGE_DEBUG -UGPS_NMEA_DEBUG -UCORE_DEBUG_LEVEL'
}

$null = New-Item -ItemType Directory -Force -Path 'out'

# ── Build loop ────────────────────────────────────────────────────────────────
$built  = @()
$failed = @()

foreach ($env in $Envs) {
    Write-Host "════════════════════════════════════════════════"
    Write-Host "  ENV: $env"

    $platform = Get-EnvPlatform $env
    Write-Host "  Platform: $(if ($platform) { $platform } else { 'unknown' })"

    $filename = "$env-$versionString"
    $buildDir = ".pio\build\$env"

    $env:PLATFORMIO_BUILD_FLAGS = "$debugFlags -DFIRMWARE_VERSION=`\`"$versionString`\`""

    $ok = $false
    pio run -e $env
    if ($LASTEXITCODE -eq 0) { $ok = $true }

    if ($ok) {
        if ($platform -eq 'ESP32_PLATFORM') {
            pio run -t mergebin -e $env
            foreach ($pair in @(
                @("$buildDir\firmware.bin",        "out\$filename.bin"),
                @("$buildDir\firmware-merged.bin", "out\$filename-merged.bin")
            )) {
                if (Test-Path $pair[0]) {
                    Copy-Item $pair[0] $pair[1] -Force
                    Write-Host "  -> $($pair[1])"
                }
            }

        } elseif ($platform -eq 'NRF52_PLATFORM') {
            python3 $UF2CONV "$buildDir\firmware.hex" -c -o "$buildDir\firmware.uf2" -f 0xADA52840
            foreach ($pair in @(
                @("$buildDir\firmware.uf2", "out\$filename.uf2"),
                @("$buildDir\firmware.zip", "out\$filename.zip")
            )) {
                if (Test-Path $pair[0]) {
                    Copy-Item $pair[0] $pair[1] -Force
                    Write-Host "  -> $($pair[1])"
                }
            }

        } elseif ($platform -eq 'RP2040_PLATFORM') {
            foreach ($pair in @(
                @("$buildDir\firmware.bin", "out\$filename.bin"),
                @("$buildDir\firmware.uf2", "out\$filename.uf2")
            )) {
                if (Test-Path $pair[0]) {
                    Copy-Item $pair[0] $pair[1] -Force
                    Write-Host "  -> $($pair[1])"
                }
            }

        } elseif ($platform -eq 'STM32_PLATFORM') {
            foreach ($pair in @(
                @("$buildDir\firmware.bin", "out\$filename.bin"),
                @("$buildDir\firmware.hex", "out\$filename.hex")
            )) {
                if (Test-Path $pair[0]) {
                    Copy-Item $pair[0] $pair[1] -Force
                    Write-Host "  -> $($pair[1])"
                }
            }

        } else {
            if (Test-Path "$buildDir\firmware.bin") {
                Copy-Item "$buildDir\firmware.bin" "out\$filename.bin" -Force
                Write-Host "  -> out\$filename.bin  (unknown platform, fallback)"
            }
        }

        $built += $env

    } else {
        Write-Host "  FAILED: $env"
        $failed += $env
    }

    Write-Host ""
}

# ── Summary ───────────────────────────────────────────────────────────────────
Write-Host "════════════════════════════════════════════════"
Write-Host "Built $($built.Count) / $($Envs.Count) envs."
if ($failed.Count -gt 0) { Write-Host "Failed: $($failed -join ', ')" }
Write-Host ""
Write-Host "Files in out\ for this build:"
Get-ChildItem 'out' | Where-Object { $_.Name -like "*$versionString*" } |
    ForEach-Object { Write-Host ("  {0,8}  {1}" -f ('{0:N0} KB' -f ($_.Length / 1KB)), $_.Name) }
