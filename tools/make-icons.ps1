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

# Retints the artwork: every pixel keeps the lightness it had and takes the
# given hue and saturation. The pear's shading, outline and highlights all
# survive, so a recoloured icon still reads as the same pear.
#
# A hue *rotation* was the obvious first try and is not good enough: the
# luminance-preserving form of it turns this bright yellow-green into pastels --
# blue arrives as cyan, red as salmon -- and no angle fixes that, because what
# is wrong is the saturation it keeps rather than the angle it turns through.
#
# scale darkens the result and lift washes it toward white; a colour wants one
# or the other, not both.
function Set-BitmapTint([System.Drawing.Bitmap]$bmp, [double]$hue, [double]$saturation,
                         [double]$scale, [double]$lift) {
    $size = $bmp.Width
    $rect = New-Object System.Drawing.Rectangle(0, 0, $size, $size)
    $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadWrite,
                           [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $stride = $data.Stride
    $bytes = New-Object byte[] ($stride * $size)
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)

    # The same for every pixel, so the sector of the colour wheel and its chroma
    # ramp are worked out once rather than a hundred thousand times.
    $sector = $hue / 60.0
    $xFactor = 1.0 - [Math]::Abs(($sector % 2.0) - 1.0)
    $sectorIndex = [int][Math]::Floor($sector)

    for ($y = 0; $y -lt $size; $y++) {
        $row = $y * $stride
        for ($x = 0; $x -lt $size; $x++) {
            $i = $row + ($x * 4)
            if ($bytes[$i + 3] -eq 0) { continue }   # transparent: nothing to tint

            # BGRA in memory, little-endian.
            $b = $bytes[$i] / 255.0
            $g = $bytes[$i + 1] / 255.0
            $r = $bytes[$i + 2] / 255.0

            $max = [Math]::Max($r, [Math]::Max($g, $b))
            $min = [Math]::Min($r, [Math]::Min($g, $b))
            $l = ($max + $min) / 2.0
            $l = $l * $scale
            $l = $l + ((1.0 - $l) * $lift)

            $c = (1.0 - [Math]::Abs((2.0 * $l) - 1.0)) * $saturation
            $xc = $c * $xFactor
            $m = $l - ($c / 2.0)

            switch ($sectorIndex) {
                0 { $r1 = $c;   $g1 = $xc;  $b1 = 0.0 }
                1 { $r1 = $xc;  $g1 = $c;   $b1 = 0.0 }
                2 { $r1 = 0.0;  $g1 = $c;   $b1 = $xc }
                3 { $r1 = 0.0;  $g1 = $xc;  $b1 = $c }
                4 { $r1 = $xc;  $g1 = 0.0;  $b1 = $c }
                default { $r1 = $c; $g1 = 0.0; $b1 = $xc }
            }

            $bytes[$i]     = [byte][Math]::Round(255.0 * [Math]::Min(1.0, [Math]::Max(0.0, $b1 + $m)))
            $bytes[$i + 1] = [byte][Math]::Round(255.0 * [Math]::Min(1.0, [Math]::Max(0.0, $g1 + $m)))
            $bytes[$i + 2] = [byte][Math]::Round(255.0 * [Math]::Min(1.0, [Math]::Max(0.0, $r1 + $m)))
        }
    }

    [System.Runtime.InteropServices.Marshal]::Copy($bytes, 0, $data.Scan0, $bytes.Length)
    $bmp.UnlockBits($data)
}

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
            # Quaver: a slanted head, a stem and a curved flag. Drawn as strokes
            # with round caps and a rotated head rather than three axis-aligned
            # rectangles -- those read as a blob with a hyphen stuck on it once
            # the badge is scaled down to icon size.
            # Heavy strokes and a large head on purpose: Explorer shows this at
            # 32px and smaller, where anything finer thins away to nothing.
            $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::White, [float](1.15 * $u))
            $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
            $pen.EndCap   = [System.Drawing.Drawing2D.LineCap]::Round

            $headCx = $cx - 1.2 * $u
            $headCy = $cy + 1.9 * $u
            $stemX  = $headCx + 1.7 * $u
            $stemTop = $cy - 3.2 * $u

            $g.DrawLine($pen, [float]$stemX, [float]$stemTop, [float]$stemX, [float]($headCy - 0.3 * $u))

            # Flag: one bezier off the top of the stem, falling away to the
            # right the way an engraved quaver's does.
            $g.DrawBezier($pen,
                [float]$stemX,               [float]$stemTop,
                [float]($stemX + 2.2 * $u),  [float]($stemTop + 0.4 * $u),
                [float]($stemX + 1.9 * $u),  [float]($stemTop + 1.6 * $u),
                [float]($stemX + 0.6 * $u),  [float]($stemTop + 2.4 * $u))
            $pen.Dispose()

            # Head, tilted so it sits under the stem like a real note head.
            $state = $g.Save()
            $g.TranslateTransform([float]$headCx, [float]$headCy)
            $g.RotateTransform(-22)
            $g.FillEllipse($white, [float](-2.0 * $u), [float](-1.5 * $u),
                                    [float](4.0 * $u), [float](3.0 * $u))
            $g.Restore($state)
        }
        "playlist" {
            # Three stacked lines with a play triangle off the last one: the
            # queue, and the fact that it plays.
            $g.FillRectangle($white, ($cx - 3.4 * $u), ($cy - 3.3 * $u), (6.2 * $u), (1.0 * $u))
            $g.FillRectangle($white, ($cx - 3.4 * $u), ($cy - 1.1 * $u), (6.2 * $u), (1.0 * $u))
            $g.FillRectangle($white, ($cx - 3.4 * $u), ($cy + 1.1 * $u), (3.4 * $u), (1.0 * $u))
            $pts = @(
                (New-Object System.Drawing.PointF(($cx + 0.9 * $u), ($cy + 0.3 * $u))),
                (New-Object System.Drawing.PointF(($cx + 0.9 * $u), ($cy + 3.3 * $u))),
                (New-Object System.Drawing.PointF(($cx + 3.5 * $u), ($cy + 1.8 * $u)))
            )
            $g.FillPolygon($white, $pts)
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
function Write-IconFile([System.Drawing.Image]$src, [string]$path, [string]$kind, [bool]$emitPngs, $tint) {
    $entries = @()
    foreach ($size in $script:sizes) {
        $bmp = New-SquareBitmap $src $size
        # Before the badge, which is deliberately the one part every icon shares:
        # colour says which format, the badge says which family.
        if ($tint) { Set-BitmapTint $bmp $tint.Hue $tint.Sat $tint.Scale $tint.Lift }
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
Write-IconFile $src $IcoPath "" $true $null | Out-Null

# Badged variants, one per file family. Registered by the installer so a video,
# an audio file and an image are told apart at a glance in Explorer.
$variants = @{
    "video" = Join-Path $icoDir "pearicon-video.ico"
    "audio" = Join-Path $icoDir "pearicon-audio.ico"
    "image" = Join-Path $icoDir "pearicon-image.ico"
}
foreach ($kind in $variants.Keys) {
    Write-IconFile $src $variants[$kind] $kind $false $null | Out-Null
}

# Per-format colours, so a folder of mixed media reads at a glance. The hue is
# a rotation away from the pear's own yellow-green, which is why the plain
# green icons above are simply the unrotated art. Order here is the order the
# icons are listed in app.rc, which is the order their indexes fall in -- see
# RegisterExtension in the installer script.
$colored = @(
    @{ File = "pearicon-video-darkgreen.ico";   Kind = "video"; Hue =  96; Sat = 0.70; Scale = 0.62; Lift = 0.00 }
    @{ File = "pearicon-video-blue.ico";        Kind = "video"; Hue = 212; Sat = 0.78; Scale = 0.95; Lift = 0.00 }
    @{ File = "pearicon-video-red.ico";         Kind = "video"; Hue =   2; Sat = 0.80; Scale = 0.90; Lift = 0.00 }
    @{ File = "pearicon-video-pink.ico";        Kind = "video"; Hue = 330; Sat = 0.75; Scale = 1.00; Lift = 0.00 }
    @{ File = "pearicon-video-purple.ico";      Kind = "video"; Hue = 278; Sat = 0.62; Scale = 0.95; Lift = 0.00 }
    @{ File = "pearicon-video-teal.ico";        Kind = "video"; Hue = 176; Sat = 0.75; Scale = 0.90; Lift = 0.00 }
    @{ File = "pearicon-audio-lightblue.ico";   Kind = "audio"; Hue = 200; Sat = 0.70; Scale = 1.00; Lift = 0.32 }
    @{ File = "pearicon-audio-lightgreen.ico";  Kind = "audio"; Hue = 112; Sat = 0.60; Scale = 1.00; Lift = 0.32 }
    @{ File = "pearicon-audio-lightpurple.ico"; Kind = "audio"; Hue = 272; Sat = 0.55; Scale = 1.00; Lift = 0.36 }
    @{ File = "pearicon-audio-lightyellow.ico"; Kind = "audio"; Hue =  48; Sat = 0.85; Scale = 1.00; Lift = 0.28 }
    @{ File = "pearicon-audio-lightred.ico";    Kind = "audio"; Hue =   6; Sat = 0.70; Scale = 1.00; Lift = 0.34 }
    @{ File = "pearicon-audio-lightpink.ico";   Kind = "audio"; Hue = 334; Sat = 0.65; Scale = 1.00; Lift = 0.38 }
    @{ File = "pearicon-audio-lightorange.ico"; Kind = "audio"; Hue =  32; Sat = 0.80; Scale = 1.00; Lift = 0.30 }
    # Playlists: a deeper orange than the pastel above, and the only icon with
    # a list badge, since this one is not a medium of its own but a list of them.
    @{ File = "pearicon-playlist.ico";          Kind = "playlist"; Hue = 28; Sat = 0.85; Scale = 0.98; Lift = 0.00 }
    # Pastels with the play badge rather than the note one: same colours as
    # their audio namesakes, told apart by the badge.
    @{ File = "pearicon-video-lightgreen.ico";  Kind = "video"; Hue = 112; Sat = 0.60; Scale = 1.00; Lift = 0.32 }
    @{ File = "pearicon-video-lightblue.ico";   Kind = "video"; Hue = 200; Sat = 0.70; Scale = 1.00; Lift = 0.32 }
)
foreach ($variant in $colored) {
    Write-IconFile $src (Join-Path $icoDir $variant.File) $variant.Kind $false $variant | Out-Null
}

$src.Dispose()

"Source : $sourcePath"
"Icon   : $IcoPath ($((Get-Item $IcoPath).Length) bytes)"
foreach ($kind in $variants.Keys | Sort-Object) {
    "  {0,-6}: {1} ({2} bytes)" -f $kind, $variants[$kind], (Get-Item $variants[$kind]).Length
}
foreach ($variant in $colored) {
    $path = Join-Path $icoDir $variant.File
    "  {0,-34}: {1} bytes" -f $variant.File, (Get-Item $path).Length
}
"PNGs   : $PngDirector ($($sizes -join ', '))"
