# Manifest generator for deep_control repo with SHA-256 hashes
# Run from the repo root in PowerShell

# ===== CONFIG =====
$repoUser = "snapperjonno"
$repoName = "deep_control"
$branch   = "main"   # Change if you use another default branch
$manifestPath = "repo_manifest.json"
# ==================

# Function to compute SHA-256 hash of a file
function Get-FileSHA256($filePath) {
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    $fileStream = [System.IO.File]::OpenRead($filePath)
    try {
        $hashBytes = $sha256.ComputeHash($fileStream)
        return ($hashBytes | ForEach-Object { $_.ToString("x2") }) -join ""
    }
    finally {
        $fileStream.Dispose()
    }
}

$files = Get-ChildItem -Recurse -File | ForEach-Object {
    $relativePath = $_.FullName.Substring((Get-Location).Path.Length + 1) -replace "\\","/"
    @{
        path = $relativePath
        url  = "https://raw.githubusercontent.com/$repoUser/$repoName/$branch/" + ($relativePath -replace " ", "%20")
        size = $_.Length
        sha256 = Get-FileSHA256 $_.FullName
    }
}

# Convert to JSON (pretty-printed) and save
$files | ConvertTo-Json -Depth 3 | Set-Content $manifestPath -Encoding UTF8

Write-Host "Manifest written to $manifestPath with $($files.Count) files." -ForegroundColor Green
