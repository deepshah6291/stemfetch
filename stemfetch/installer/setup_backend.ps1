# Sets up Python venv + yt-dlp + Demucs + ffmpeg for StemFetch (runs as the installing user).
$ErrorActionPreference = "Stop"
$root = Join-Path $env:APPDATA "StemFetch"
$venv = Join-Path $root "venv"
New-Item -ItemType Directory -Force -Path $root | Out-Null
Start-Transcript -Path (Join-Path $root "setup.log") -Force | Out-Null

function Get-Python {
    foreach ($c in @("$env:LOCALAPPDATA\Programs\Python\Python311\python.exe",
                     "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe",
                     "$env:LOCALAPPDATA\Programs\Python\Python310\python.exe")) {
        if (Test-Path $c) { return $c }
    }
    if (Get-Command py -ErrorAction SilentlyContinue) {
        $p = & py -3.11 -c "import sys;print(sys.executable)" 2>$null
        if ($LASTEXITCODE -eq 0 -and $p) { return $p }
    }
    return $null
}

try {
    $python = Get-Python
    if (-not $python) {
        Write-Host "Installing Python 3.11 via winget..."
        winget install -e --id Python.Python.3.11 --scope user --silent `
            --accept-package-agreements --accept-source-agreements
        $python = Get-Python
    }
    if (-not $python) { throw "Python 3.11 not found. Install it from python.org, then re-run this script." }

    $vpy = Join-Path $venv "Scripts\python.exe"
    if (-not (Test-Path $vpy)) { & $python -m venv $venv }
    & $vpy -m pip install --upgrade pip

    if (Get-Command nvidia-smi -ErrorAction SilentlyContinue) {
        Write-Host "NVIDIA GPU detected - installing CUDA build of PyTorch"
        & $vpy -m pip install torch==2.5.1 torchaudio==2.5.1 --index-url https://download.pytorch.org/whl/cu121
    } else {
        & $vpy -m pip install torch==2.5.1 torchaudio==2.5.1
    }
    & $vpy -m pip install demucs yt-dlp

    $ff = Join-Path $root "ffmpeg"
    if (-not (Test-Path (Join-Path $ff "ffmpeg.exe"))) {
        Write-Host "Downloading ffmpeg..."
        $zip = Join-Path $env:TEMP "ffmpeg.zip"
        Invoke-WebRequest "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip" -OutFile $zip
        $tmp = Join-Path $env:TEMP "ffmpeg_extract"
        Expand-Archive $zip -DestinationPath $tmp -Force
        New-Item -ItemType Directory -Force -Path $ff | Out-Null
        Get-ChildItem -Path $tmp -Recurse -Filter "ff*.exe" | Copy-Item -Destination $ff -Force
        Remove-Item $zip, $tmp -Recurse -Force
    }
    Write-Host "StemFetch backend ready."
} catch {
    Write-Host "SETUP FAILED: $_"
    Stop-Transcript | Out-Null
    exit 1
}
Stop-Transcript | Out-Null
