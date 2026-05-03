$ErrorActionPreference = 'Stop'

$projectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$desktop = [Environment]::GetFolderPath('Desktop')
$launcher = Join-Path $projectDir 'launch-stratum.cmd'
$assetsDir = Join-Path $projectDir 'frontend\public'
$iconPath = Join-Path $assetsDir 'icon.ico'
$shortcutPath = Join-Path $desktop 'Stratum DAW.lnk'

if (!(Test-Path $assetsDir)) {
  New-Item -ItemType Directory -Path $assetsDir | Out-Null
}

if (!(Test-Path $iconPath)) {
  Add-Type -AssemblyName System.Drawing

  $size = 256
  $bitmap = New-Object System.Drawing.Bitmap $size, $size
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $graphics.Clear([System.Drawing.Color]::Transparent)

  $rect = New-Object System.Drawing.Rectangle 8, 8, 240, 240
  $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, ([System.Drawing.Color]::FromArgb(255, 39, 39, 42)), ([System.Drawing.Color]::FromArgb(255, 8, 8, 10)), 45
  $graphics.FillEllipse($brush, $rect)

  $penDark = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 0, 0, 0)), 8
  $graphics.DrawEllipse($penDark, $rect)

  $orange = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 249, 115, 22))
  $graphics.FillEllipse($orange, (New-Object System.Drawing.Rectangle 42, 42, 172, 172))

  $inner = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 24, 24, 27))
  $graphics.FillEllipse($inner, (New-Object System.Drawing.Rectangle 70, 70, 116, 116))

  $font = New-Object System.Drawing.Font 'Arial', 78, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
  $textBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 250, 250, 250))
  $format = New-Object System.Drawing.StringFormat
  $format.Alignment = [System.Drawing.StringAlignment]::Center
  $format.LineAlignment = [System.Drawing.StringAlignment]::Center
  $graphics.DrawString('S', $font, $textBrush, (New-Object System.Drawing.RectangleF 0, 4, 256, 256), $format)

  $iconHandle = $bitmap.GetHicon()
  $icon = [System.Drawing.Icon]::FromHandle($iconHandle)
  $fileStream = [System.IO.File]::Create($iconPath)
  $icon.Save($fileStream)
  $fileStream.Close()

  $graphics.Dispose()
  $bitmap.Dispose()
  $icon.Dispose()
}

$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $launcher
$shortcut.WorkingDirectory = $projectDir
$shortcut.IconLocation = $iconPath
$shortcut.Description = 'Open Stratum DAW / FL Studio Clone Electron app'
$shortcut.WindowStyle = 7
$shortcut.Save()

Write-Host "Desktop shortcut created: $shortcutPath"
Write-Host "Launcher: $launcher"
Write-Host "Icon: $iconPath"
