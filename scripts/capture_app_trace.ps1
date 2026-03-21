param(
  [int]$DurationSeconds = 60,
  [int]$BootDelaySeconds = 5,
  [string]$Prefix = "capture",
  [string]$OutDir = ".",
  [string]$InterfaceCfg = "interface/ftdi/esp32_devkitj_v1.cfg",
  [string]$TargetCfg = "target/esp32.cfg",
  [int]$AdapterKHz = 20000,
  [switch]$NoReset
)

$ErrorActionPreference = "Stop"

# Run from repository root by default
if (-not (Test-Path "platformio.ini")) {
  Write-Error "Execute este script na raiz do projeto (onde existe platformio.ini)."
}

$openocd = "c:\.platformio\packages\tool-openocd-esp32\bin\openocd.exe"
$scripts = "c:\.platformio\packages\tool-openocd-esp32\share\openocd\scripts"

if (-not (Test-Path $openocd)) {
  Write-Error "OpenOCD nao encontrado em $openocd"
}
if (-not (Test-Path $scripts)) {
  Write-Error "Scripts OpenOCD nao encontrados em $scripts"
}

if (-not (Test-Path $OutDir)) {
  New-Item -ItemType Directory -Path $OutDir | Out-Null
}

# Resolve to absolute path so OpenOCD file:// URLs are unambiguous
$OutDirAbs = (Resolve-Path $OutDir).Path

$ts = Get-Date -Format "yyyyMMdd_HHmmss"
$core0Name = "${Prefix}_core0_${ts}.svdat"
$core1Name = "${Prefix}_core1_${ts}.svdat"
$core0Path = Join-Path $OutDirAbs $core0Name
$core1Path = Join-Path $OutDirAbs $core1Name

# OpenOCD expects file:// URLs; use forward slashes for compatibility
$core0Url = "file://" + $core0Path.Replace('\', '/')
$core1Url = "file://" + $core1Path.Replace('\', '/')

if ($NoReset) {
  # Attach to the already-running firmware without resetting
  $cmd = @(
    "adapter speed $AdapterKHz",
    "init",
    "esp sysview start $core0Url $core1Url",
    "sleep $($DurationSeconds * 1000)",
    "esp sysview stop",
    "shutdown"
  ) -join "; "
} else {
  $cmd = @(
    "adapter speed $AdapterKHz",
    "init",
    "reset run",
    "sleep $($BootDelaySeconds * 1000)",
    "esp sysview start $core0Url $core1Url",
    "sleep $($DurationSeconds * 1000)",
    "esp sysview stop",
    "shutdown"
  ) -join "; "
}

Write-Host "[app-trace] Captura iniciada"
Write-Host "[app-trace] Duracao: $DurationSeconds s"
if ($NoReset) {
  Write-Host "[app-trace] Modo: sem reset (pipeline ja ativo)"
} else {
  Write-Host "[app-trace] Boot delay: $BootDelaySeconds s"
}
Write-Host "[app-trace] Saida: $core0Path"
Write-Host "[app-trace] Saida: $core1Path"

& $openocd -s $scripts -f $InterfaceCfg -f $TargetCfg -c $cmd

if ((Test-Path $core0Path) -and (Test-Path $core1Path)) {
  Get-Item $core0Path, $core1Path | Select-Object Name, Length, LastWriteTime
  Write-Host "[app-trace] Captura concluida"
} else {
  Write-Error "Captura terminou sem gerar os dois ficheiros .svdat"
}
