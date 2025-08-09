# Manifest generator for deep_control repo
# Run from the repo root in PowerShell

# ===== CONFIG =====
$repoUser = "snapperjonno"
$repoName = "deep_control"
$branch   = "main"   # Change if you use another default branch
$manifestPath = "repo_manifest.json"
# ==================

$files = Get-ChildItem -Recurse -File | ForEach-Object {
    $relativePath = $_.FullName.Substring((Get-Location).Path.Length + 1) -replace "\\","/"
    @{
        path = $relativePath
        url  = "https://raw.githubusercontent.com/$repoUser/$repoName/$branch/" + ($relativePath -replace " ", "%20")
        size = $_.Length
    }
}

# Convert to JSON (pretty-printed) and save
$files | ConvertTo-Json -Depth 3 | Set-Content $manifestPath -Encoding UTF8

Write-Host "Manifest written to $manifestPath with $($files.Count) files." -ForegroundColor Green
