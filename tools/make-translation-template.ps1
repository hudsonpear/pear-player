# Builds translations/template.json from every tr("...") in the source.
#
#   pwsh -File tools/make-translation-template.ps1
#
# The result is a file with every translatable string as a key and an empty
# value. Copy it, fill the values in, name it after the language (pt-BR.json)
# and it shows up in Settings.
#
# Regenerate after adding UI strings; existing translation files are untouched,
# and any string they do not carry falls back to English.

$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$outDir = Join-Path $root "translations"
$outFile = Join-Path $outDir "template.json"

New-Item -ItemType Directory -Force $outDir | Out-Null

# tr("..."), QObject::tr("..."), QT_TR_NOOP("...") and the translate("ctx",
# "text") form.
#
# The literal is matched as a *run* of adjacent string literals, because the
# compiler joins them into one string before tr() ever sees it:
#
#     tr("Draws a tick where each chapter starts.\n"
#        "Files without chapters are unaffected.")
#
# arrives at runtime as a single string. Capturing only the first part -- which
# an earlier version of this script did -- produced template keys that could
# never match, so every multi-line tooltip silently stayed in English however
# carefully it was translated.
$literalRun = '((?:"(?:[^"\\]|\\.)*"\s*)+)'
$patterns = @(
    ('(?:^|[^A-Za-z0-9_])tr\(\s*' + $literalRun),
    ('QT_TR_NOOP\(\s*' + $literalRun),
    # translate("Context", "text") -- the text is the second literal.
    ('translate\(\s*"(?:[^"\\]|\\.)*"\s*,\s*' + $literalRun)
)

$strings = [System.Collections.Generic.SortedSet[string]]::new([System.StringComparer]::Ordinal)

Get-ChildItem $root -Include *.cpp, *.h -File -Recurse |
    Where-Object { $_.FullName -notmatch '\\build\\' -and $_.FullName -notmatch '\\third_party\\' } |
    ForEach-Object {
        $text = Get-Content $_.FullName -Raw
        foreach ($pattern in $patterns) {
            foreach ($match in [regex]::Matches($text, $pattern)) {
                # Join the run back into the one string the compiler produces.
                $parts = [regex]::Matches($match.Groups[1].Value, '"((?:[^"\\]|\\.)*)"')
                $value = -join ($parts | ForEach-Object { $_.Groups[1].Value })
                if ($value.Trim().Length -gt 0) {
                    [void]$strings.Add($value)
                }
            }
        }
    }

# Written by hand rather than through ConvertTo-Json: the keys already carry C
# escapes (\n, \") from the source, and they must stay exactly as the compiler
# sees them or the lookup at runtime will not match.
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('{')
$lines.Add('  "language": "Language name shown in Settings",')
$lines.Add('  "strings": {')

$i = 0
foreach ($s in $strings) {
    $i++
    $comma = if ($i -lt $strings.Count) { "," } else { "" }
    $lines.Add('    "' + $s + '": ""' + $comma)
}

$lines.Add('  }')
$lines.Add('}')

Set-Content -Path $outFile -Value $lines -Encoding utf8NoBOM
Write-Host ("Wrote {0} ({1} strings)" -f $outFile, $strings.Count) -ForegroundColor Green
