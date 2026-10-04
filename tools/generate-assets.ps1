Add-Type -AssemblyName System.Drawing

$assetDirectory = Join-Path (Split-Path -Parent $PSScriptRoot) 'assets'
New-Item -ItemType Directory -Force -Path $assetDirectory | Out-Null

$bitmap = New-Object System.Drawing.Bitmap 512, 512
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.Clear([System.Drawing.ColorTranslator]::FromHtml('#0e1e2a'))
$graphics.FillEllipse((New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml('#2ac9d0'))), 102, 96, 284, 284)
$graphics.FillEllipse((New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml('#0e1e2a'))), 190, 134, 176, 176)
$linePen = New-Object System.Drawing.Pen ([System.Drawing.ColorTranslator]::FromHtml('#e2f1f7')), 30
$linePen.StartCap = $linePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
$graphics.DrawLine($linePen, 112, 312, 400, 312)
$graphics.FillEllipse((New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml('#ffbe5c'))), 82, 282, 60, 60)
$graphics.FillEllipse((New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml('#4cd797'))), 370, 282, 60, 60)
$pngPath = Join-Path $assetDirectory 'text-orbit.png'
$bitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()

# An ICO container can embed a PNG image directly. This small writer avoids
# requiring a separate image conversion dependency for the Windows resource.
$pngBytes = [System.IO.File]::ReadAllBytes($pngPath)
$stream = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter $stream
$writer.Write([UInt16]0); $writer.Write([UInt16]1); $writer.Write([UInt16]1)
$writer.Write([Byte]0); $writer.Write([Byte]0); $writer.Write([Byte]0); $writer.Write([Byte]0)
$writer.Write([UInt16]1); $writer.Write([UInt16]32)
$writer.Write([UInt32]$pngBytes.Length); $writer.Write([UInt32]22)
$writer.Write($pngBytes)
[System.IO.File]::WriteAllBytes((Join-Path $assetDirectory 'text-orbit.ico'), $stream.ToArray())
$writer.Dispose(); $stream.Dispose()
