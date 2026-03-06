Add-Type -AssemblyName System.Drawing

$src = [System.Drawing.Bitmap]::new('d:\Dev\Exorcist\icon.png')
Write-Host "Source: $($src.Width)x$($src.Height)"

# ── Auto-crop: find bounding box of non-transparent pixels ──────────────────
$minX = $src.Width; $maxX = 0; $minY = $src.Height; $maxY = 0

for ($y = 0; $y -lt $src.Height; $y++) {
    for ($x = 0; $x -lt $src.Width; $x++) {
        $px = $src.GetPixel($x, $y)
        if ($px.A -gt 10) {
            if ($x -lt $minX) { $minX = $x }
            if ($x -gt $maxX) { $maxX = $x }
            if ($y -lt $minY) { $minY = $y }
            if ($y -gt $maxY) { $maxY = $y }
        }
    }
}

Write-Host "Content bounds: ($minX,$minY) -> ($maxX,$maxY)"
$contentW = $maxX - $minX + 1
$contentH = $maxY - $minY + 1
Write-Host "Content size: ${contentW}x${contentH}"

# ── Crop to content, add 8% padding on each side ────────────────────────────
$pad = [int]([Math]::Max($contentW, $contentH) * 0.08)
$cropSize = [Math]::Max($contentW, $contentH) + $pad * 2

$cropped = [System.Drawing.Bitmap]::new($cropSize, $cropSize)
$g = [System.Drawing.Graphics]::FromImage($cropped)
$g.Clear([System.Drawing.Color]::Transparent)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$offsetX = $pad + ($cropSize - $pad*2 - $contentW) / 2
$offsetY = $pad + ($cropSize - $pad*2 - $contentH) / 2
$g.DrawImage($src,
    [System.Drawing.RectangleF]::new($offsetX, $offsetY, $contentW, $contentH),
    [System.Drawing.RectangleF]::new($minX, $minY, $contentW, $contentH),
    [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose()
$src.Dispose()

# ── Build ICO with 4 sizes ───────────────────────────────────────────────────
$sizes = @(16, 32, 48, 256)
$ms = [System.IO.MemoryStream]::new()
$writer = [System.IO.BinaryWriter]::new($ms)

$writer.Write([uint16]0)
$writer.Write([uint16]1)
$writer.Write([uint16]$sizes.Count)

$dirOffset = $ms.Position
$writer.Write([byte[]]::new($sizes.Count * 16))

$offsets = @(); $lengths = @()

foreach ($sz in $sizes) {
    $bmp = [System.Drawing.Bitmap]::new($sz, $sz)
    $g2 = [System.Drawing.Graphics]::FromImage($bmp)
    $g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g2.DrawImage($cropped, 0, 0, $sz, $sz)
    $g2.Dispose()

    $imgStream = [System.IO.MemoryStream]::new()
    $bmp.Save($imgStream, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()

    $offsets += [int]$ms.Position
    $bytes = $imgStream.ToArray()
    $lengths += [int]$bytes.Length
    $writer.Write($bytes)
    $imgStream.Dispose()
}

$cropped.Dispose()
$endPos = $ms.Position

$ms.Seek($dirOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $sz = $sizes[$i]
    $w2 = if ($sz -eq 256) { 0 } else { $sz }
    $writer.Write([byte]$w2)
    $writer.Write([byte]$w2)
    $writer.Write([byte]0)
    $writer.Write([byte]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]32)
    $writer.Write([uint32]$lengths[$i])
    $writer.Write([uint32]$offsets[$i])
}

$ms.Seek($endPos, [System.IO.SeekOrigin]::Begin) | Out-Null

$outPath = 'd:\Dev\Exorcist\src\app.ico'
[System.IO.File]::WriteAllBytes($outPath, $ms.ToArray())
Write-Host "Written: $outPath  ($($ms.Length) bytes)"
