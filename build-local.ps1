#Requires -Version 5.1
# build-local.ps1 - build specified PlatformIO envs and collect firmware artifacts.
#
# Usage:
#   .\build-local.ps1 Heltec_E213_companion_radio_uni
#   .\build-local.ps1 Heltec_E213_companion_radio_uni, Heltec_E290_companion_radio_uni, Heltec_v3_companion_radio_uni
#   .\build-local.ps1 -Envs Heltec_v3_companion_radio_uni -DisableDebug
#
# Output files land in out\ with names matching the GitHub Action convention:
#   <env>-<version>-<hash>.bin          (update binary)
#   <env>-<version>-<hash>-merged.bin   (full flash image)
#   <env>-<version>-<hash>.uf2          (USB drag-and-drop)
#   <env>-<version>-<hash>.zip          (BLE DFU OTA)
#   <env>-<version>-<hash>.hex          (hex image, when produced)

param(
    [Parameter(Mandatory, Position = 0, ValueFromRemainingArguments)]
    [string[]] $Envs,

    [switch] $DisableDebug
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$UF2CONV = "bin\uf2conv\uf2conv.py"

# Version from last tag.
$lastTag = git describe --tags --abbrev=0 2>$null
if (-not $lastTag) { $lastTag = 'v0.0.0' }
# Strip everything up to and including the last '-' (removes "companion-" prefix etc.).
$gitTagVersion = $lastTag -replace '^.*-(?=v\d)', ''
$commitHash = (git rev-parse --short HEAD).Trim()
$versionString = "$gitTagVersion-$commitHash"

Write-Host "Tag:     $lastTag"
Write-Host "Version: $versionString"
Write-Host ""

# Platform detection via pio project config --json-output.
$pioConfigJson = (pio project config --json-output 2>$null) -join "`n"

function Get-EnvPlatform([string]$EnvName) {
    # Try pio json config first
    if ($pioConfigJson) {
        $data = $null
        try {
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
        } catch { }
        if ($data) { return $data.Trim() }
    }
    # Fallback: find the variant ini containing the env, check its extends chain
    $iniFiles = Get-ChildItem -Path 'variants' -Filter 'platformio.ini' -Recurse
    foreach ($f in $iniFiles) {
        $content = Get-Content $f.FullName -Raw -ErrorAction SilentlyContinue
        if ($content -match "\[env:$([regex]::Escape($EnvName))\]") {
            if ($content -match 'extends\s*=\s*esp32_base')  { return 'ESP32_PLATFORM' }
            if ($content -match 'extends\s*=\s*nrf52_base')  { return 'NRF52_PLATFORM' }
            if ($content -match 'extends\s*=\s*stm32_base')  { return 'STM32_PLATFORM' }
            if ($content -match 'extends\s*=\s*rp2040_base') { return 'RP2040_PLATFORM' }
        }
    }
    return ''
}

function Normalize-Envs([string[]]$RawEnvs) {
    $normalized = @()
    foreach ($item in $RawEnvs) {
        foreach ($part in ($item -split ',')) {
            $name = $part.Trim()
            if ($name) { $normalized += $name }
        }
    }
    return $normalized
}

function Copy-Artifact([string]$Source, [string]$Dest) {
    if (Test-Path $Source) {
        Copy-Item $Source $Dest -Force
        Write-Host "  -> $Dest"
        return $true
    }
    return $false
}

$Envs = Normalize-Envs $Envs

# Debug flag stripping.
$debugFlags = ''
if ($DisableDebug) {
    $debugFlags = '-UMESH_DEBUG -UBLE_DEBUG_LOGGING -UWIFI_DEBUG_LOGGING -UBRIDGE_DEBUG -UGPS_NMEA_DEBUG -UCORE_DEBUG_LEVEL'
}

$null = New-Item -ItemType Directory -Force -Path 'out'

$built  = @()
$failed = @()

foreach ($env in $Envs) {
    Write-Host "================================================"
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
        } elseif ($platform -eq 'NRF52_PLATFORM') {
            pio run -t create_uf2 -e $env
            if ((-not (Test-Path "$buildDir\firmware.uf2")) -and (Test-Path "$buildDir\firmware.hex")) {
                python3 $UF2CONV "$buildDir\firmware.hex" -c -o "$buildDir\firmware.uf2" -f 0xADA52840
            }
        }

        $copied = 0
        if (Copy-Artifact "$buildDir\firmware.bin"        "out\$filename.bin")        { $copied++ }
        if (Copy-Artifact "$buildDir\firmware-merged.bin" "out\$filename-merged.bin") { $copied++ }
        if (Copy-Artifact "$buildDir\firmware.uf2"        "out\$filename.uf2")        { $copied++ }
        if (Copy-Artifact "$buildDir\firmware.zip"        "out\$filename.zip")        { $copied++ }
        if (Copy-Artifact "$buildDir\firmware.hex"        "out\$filename.hex")        { $copied++ }

        if ($copied -eq 0) {
            Write-Host "  WARNING: no firmware artifacts found in $buildDir"
        }

        $built += $env
    } else {
        Write-Host "  FAILED: $env"
        $failed += $env
    }

    Write-Host ""
}

Write-Host "================================================"
Write-Host "Built $($built.Count) / $($Envs.Count) envs."
if ($failed.Count -gt 0) { Write-Host "Failed: $($failed -join ', ')" }
Write-Host ""
Write-Host "Files in out\ for this build:"
Get-ChildItem 'out' | Where-Object { $_.Name -like "*$versionString*" } |
    ForEach-Object { Write-Host ("  {0,8}  {1}" -f ('{0:N0} KB' -f ($_.Length / 1KB)), $_.Name) }
