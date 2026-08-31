# Regenerates every derived icon asset from the master artwork.
#
#   pwsh -File tools/make-icons.ps1
#
# Produces:
#   resources/pearicon.ico              - multi-size icon compiled into the exe
#   resources/icons/pearicon-<n>.png    - pre-scaled sizes embedded via app.qrc
#
# Run this after editing pearicon.png; the outputs are committed so a normal
# build never needs PowerShell.

param(
    [string]$Source      = (Join-Path $PSScriptRoot "..\pearicon.png"),
    [string]$IcoPath     = (Join-Path $PSScriptRoot "..\resources\pearicon.ico"),
    [string]$PngDirector = (Join-Path $PSScriptRoot "..\resources\icons")
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$sizes = @(16, 20, 24, 32, 40, 48, 64, 128, 256)

# Renders the artwork centred on a transparent square canvas, preserving its
# aspect ratio and leaving a small margin so the glyph never touches the edge.
function New-SquareBitmap([System.Drawing.Image]$image, [int]$size) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.InterpolationMode    = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode        = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode      = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.CompositingQuality   = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality

    $margin = [Math]::Max(1, [int][Math]::Round($size * 0.04))
    $box    = $size - (2 * $margin)
    $scale  = [Math]::Min($box / $image.Width, $box / $image.Height)
    $w = [Math]::Max(1, [int][Math]::Round($image.Width  * $scale))
    $h = [Math]::Max(1, [int][Math]::Round($image.Height * $scale))
    $x = [int][Math]::Round(($size - $w) / 2.0)
    $y = [int][Math]::Round(($size - $h) / 2.0)

    $g.DrawImage($image, (New-Object System.Drawing.Rectangle($x, $y, $w, $h)))
    $g.Dispose()
    return $bmp
}

# One .ico image entry: a BITMAPINFOHEADER whose height is doubled to cover the
# XOR bitmap plus the AND mask, bottom-up BGRA rows, then a zeroed 1bpp mask
# (unused at 32bpp, but the format still requires it to be present).
function Get-DibBytes([System.Drawing.Bitmap]$bmp) {
    $size = $bmp.Width
    $rect = New-Object System.Drawing.Rectangle(0, 0, $size, $size)
    $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $pixels = New-Object byte[] ($data.Stride * $size)
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $pixels, 0, $pixels.Length)
    $stride = $data.Stride
    $bmp.UnlockBits($data)

    $maskStride = [int][Math]::Floor(($size + 31) / 32) * 4
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)

    $bw.Write([uint32]40)                                        # biSize
    $bw.Write([int32]$size)                                      # biWidth
    $bw.Write([int32]($size * 2))                                # biHeight (XOR + AND)
    $bw.Write([uint16]1)                                         # biPlanes
    $bw.Write([uint16]32)                                        # biBitCount
    $bw.Write([uint32]0)                                         # biCompression = BI_RGB
    $bw.Write([uint32]($size * $size * 4 + $maskStride * $size)) # biSizeImage
    $bw.Write([int32]0); $bw.Write([int32]0)                     # pels per metre
    $bw.Write([uint32]0); $bw.Write([uint32]0)                   # palette counts

    for ($row = $size - 1; $row -ge 0; $row--) {
        $bw.Write($pixels, $row * $stride, $size * 4)
    }
    $bw.Write((New-Object byte[] ($maskStride * $size)))

    $bw.Flush()
    return ,$ms.ToArray()
}

# Draws a small badge over the bottom-right corner, marking which kind of file
# an icon stands for. Explorer draws file icons at 32px and up in practice, so
# the badge is sized as a fraction of the canvas and simply becomes an
# indistinct dot at 16px rather than turning to mush.
function Add-Badge([System.Drawing.Bitmap]$bmp, [string]$kind) {
    if (-not $kind) { return }

    $size = $bmp.Width
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality

    $d  = [double]$size * 0.52          # badge diameter
    $x  = [double]$size - $d - ($size * 0.02)
    $y  = [double]$size - $d - ($size * 0.02)

    # Dark disc behind the glyph, so the mark reads against the pear's green
    # whatever the folder background is.
    $g.FillEllipse((New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(235, 28, 30, 26))),
                    $x, $y, $d, $d)
    $white = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)

    $cx = $x + $d / 2.0
    $cy = $y + $d / 2.0
    $u  = $d / 10.0                     # glyph unit

    switch ($kind) {
        "video" {
            # Play triangle.
            $pts = @(
                (New-Object System.Drawing.PointF(($cx - 1.8 * $u), ($cy - 2.6 * $u))),
                (New-Object System.Drawing.PointF(($cx - 1.8 * $u), ($cy + 2.6 * $u))),
                (New-Object System.Drawing.PointF(($cx + 2.6 * $u), $cy))
            )
            $g.FillPolygon($white, $pts)
        }
        "audio" {
            # Quaver: note head, stem, flag.
            $g.FillEllipse($white, ($cx - 2.6 * $u), ($cy + 0.6 * $u), (2.6 * $u), (2.2 * $u))
            $g.FillRectangle($white, ($cx - 0.3 * $u), ($cy - 3.0 * $u), (0.9 * $u), (4.4 * $u))
            $g.FillRectangle($white, ($cx - 0.3 * $u), ($cy - 3.0 * $u), (2.8 * $u), (0.9 * $u))
        }
        "image" {
            # Photo frame with a hill and a sun.
            $g.FillRectangle($white, ($cx - 3.0 * $u), ($cy - 2.4 * $u), (6.0 * $u), (4.8 * $u))
            $dark = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(235, 28, 30, 26))
            $g.FillEllipse($dark, ($cx - 2.0 * $u), ($cy - 1.6 * $u), (1.6 * $u), (1.6 * $u))
            $hill = @(
                (New-Object System.Drawing.PointF(($cx - 2.6 * $u), ($cy + 2.0 * $u))),
                (New-Object System.Drawing.PointF(($cx - 0.2 * $u), ($cy - 0.6 * $u))),
                (New-Object System.Drawing.PointF(($cx + 2.6 * $u), ($cy + 2.0 * $u)))
            )
            $g.FillPolygon($dark, $hill)
            $dark.Dispose()
        }
    }

    $white.Dispose()
    $g.Dispose()
}

# Assembles one .ico from the master artwork, optionally badged, and returns
# the path written.
function Write-IconFile([System.Drawing.Image]$src, [string]$path, [string]$kind, [bool]$emitPngs) {
    $entries = @()
    foreach ($size in $script:sizes) {
        $bmp = New-SquareBitmap $src $size
        Add-Badge $bmp $kind

        if ($emitPngs) {
            $bmp.Save((Join-Path $PngDirector "pearicon-$size.png"), [System.Drawing.Imaging.ImageFormat]::Png)
        }

        # Every entry stays a classic 32bpp DIB. PNG-compressed entries would
        # make the file far smaller, but some consumers silently skip them and
        # lose the largest size along the way.
        [byte[]]$bytes = Get-DibBytes $bmp
        $bmp.Dispose()
        $entries += [pscustomobject]@{ Size = $size; Bytes = $bytes }
    }

    $out = New-Object System.IO.MemoryStream
    $bw  = New-Object System.IO.BinaryWriter($out)
    $bw.Write([uint16]0)                # reserved
    $bw.Write([uint16]1)                # type = icon
    $bw.Write([uint16]$entries.Count)

    $offset = 6 + (16 * $entries.Count)
    foreach ($e in $entries) {
        $dim = if ($e.Size -ge 256) { 0 } else { $e.Size }  # 256 is encoded as 0
        $bw.Write([byte]$dim)           # width
        $bw.Write([byte]$dim)           # height
        $bw.Write([byte]0)              # palette entries
        $bw.Write([byte]0)              # reserved
        $bw.Write([uint16]1)            # colour planes
        $bw.Write([uint16]32)           # bits per pixel
        $bw.Write([uint32]$e.Bytes.Length)
        $bw.Write([uint32]$offset)
        $offset += $e.Bytes.Length
    }
    foreach ($e in $entries) { $bw.Write([byte[]]$e.Bytes, 0, $e.Bytes.Length) }
    $bw.Flush()

    [System.IO.File]::WriteAllBytes($path, $out.ToArray())
    $bw.Dispose()
    return $path
}

$sourcePath = (Resolve-Path $Source).Path
$src = [System.Drawing.Image]::FromFile($sourcePath)
New-Item -ItemType Directory -Force -Path $PngDirector | Out-Null
$icoDir = Split-Path -Parent $IcoPath
New-Item -ItemType Directory -Force -Path $icoDir | Out-Null

# Plain icon first: it is the application's own, and also the one that emits
# the PNG set embedded through app.qrc.
Write-IconFile $src $IcoPath "" $true | Out-Null

# Badged variants, one per file family. Registered by the installer so a video,
# an audio file and an image are told apart at a glance in Explorer.
$variants = @{
    "video" = Join-Path $icoDir "pearicon-video.ico"
    "audio" = Join-Path $icoDir "pearicon-audio.ico"
    "image" = Join-Path $icoDir "pearicon-image.ico"
}
foreach ($kind in $variants.Keys) {
    Write-IconFile $src $variants[$kind] $kind $false | Out-Null
}

$src.Dispose()

"Source : $sourcePath"
"Icon   : $IcoPath ($((Get-Item $IcoPath).Length) bytes)"
foreach ($kind in $variants.Keys | Sort-Object) {
    "  {0,-6}: {1} ({2} bytes)" -f $kind, $variants[$kind], (Get-Item $variants[$kind]).Length
}
"PNGs   : $PngDirector ($($sizes -join ', '))"
