$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$catalog=Get-Content -LiteralPath (Join-Path $projectRoot 'src/translations/fa.json') -Raw | ConvertFrom-Json -AsHashtable
$sources=[Collections.Generic.HashSet[string]]::new()
Get-ChildItem -LiteralPath (Join-Path $projectRoot 'src') -Recurse -File -Include '*.cpp','*.h' | ForEach-Object {
    $content=[IO.File]::ReadAllText($_.FullName)
    foreach($match in [regex]::Matches($content,'\btr\(\s*((?:"(?:\\.|[^"\\])*"\s*)+)')) {
        $source=''
        foreach($literal in [regex]::Matches($match.Groups[1].Value,'"(?:\\.|[^"\\])*"')) {
            $source += [System.Text.Json.JsonSerializer]::Deserialize[string]($literal.Value)
        }
        [void]$sources.Add($source)
    }
}
Get-ChildItem -LiteralPath (Join-Path $projectRoot 'src') -Recurse -Filter '*.ui' | ForEach-Object {
    [xml]$document=Get-Content -LiteralPath $_.FullName -Raw
    foreach($node in $document.SelectNodes('//string[not(@notr="true")]')) { [void]$sources.Add($node.InnerText) }
}
$missing=@($sources | Where-Object { -not $catalog.ContainsKey($_) } | Sort-Object)
if($missing.Count) { $missing | ConvertTo-Json; throw "$($missing.Count) application strings lack Persian translations" }
foreach($key in $catalog.Keys) {
    $expected=@([regex]::Matches($key,'%[1-9][0-9]*|%n').Value | Sort-Object)
    $actual=@([regex]::Matches($catalog[$key],'%[1-9][0-9]*|%n').Value | Sort-Object)
    if(($expected -join '|') -ne ($actual -join '|')) { throw "Translation changes substitution fields: $key" }
}
Write-Output "Persian coverage: $($sources.Count) / $($sources.Count) application strings; $($catalog.Count) total entries including Qt controls. Substitution fields verified."
