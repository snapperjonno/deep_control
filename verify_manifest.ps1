# Verify SHA-256 values in repo_manifest.json against actual files
# Run from repo root in PowerShell

$manifestPath = "repo_manifest.json"

if (-Not (Test-Path $manifestPath)) {
    Write-Host "Manifest file not found: $manifestPath" -ForegroundColor Red
    exit 1
}

# Function to compute SHA-256 of a file
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

# Load manifest JSON
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json

$errors = @()
$okCount = 0

foreach ($entry in $manifest) {
    $path = $entry.path
    $expectedSHA = $entry.sha256

    if (-Not (Test-Path $path)) {
        $errors += "Missing file: $path"
        continue
    }

    $actualSHA = Get-FileSHA256 $path

    if ($actualSHA -eq $expectedSHA) {
        $okCount++
    }
    else {
        $errors += "SHA mismatch: $path"
    }
}

Write-Host "$okCount files verified OK." -ForegroundColor Green

if ($errors.Count -gt 0) {
    Write-Host "Found $($errors.Count) issues:" -ForegroundColor Red
    $errors | ForEach-Object { Write-Host $_ -ForegroundColor Yellow }
    exit 1
} else {
    Write-Host "All manifest entries are valid." -ForegroundColor Green
}
