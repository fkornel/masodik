param(
    [string]$LauncherPath = "C:\dev\copilot\masodik\build\src\main\masodik-launcher.exe",
    [string]$ProfilePath = "C:\dev\copilot\masodik\config\profiles\b580-manual-split-cut23-tail-32k-u8.json",
    [string]$BaseUrl = "http://127.0.0.1:29015"
)

$ErrorActionPreference = "Stop"

function Wait-Ready {
    param([string]$Url)

    $deadline = (Get-Date).AddMinutes(10)
    while ((Get-Date) -lt $deadline) {
        try {
            $response = Invoke-RestMethod -Method Get -Uri "$Url/health" -TimeoutSec 30
            if ($response.ready -eq $true -and $response.state -eq "ready") {
                return
            }
        } catch {
        }
        Start-Sleep -Seconds 2
    }

    throw "A launcher nem valt ready allapotba a restart smoke alatt."
}

for ($iteration = 1; $iteration -le 2; $iteration++) {
    $process = Start-Process -FilePath $LauncherPath -ArgumentList @("--profile", $ProfilePath) -PassThru
    try {
        Wait-Ready -Url $BaseUrl
    } finally {
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id
            $process.WaitForExit()
        }
    }
}

Write-Host "Restart smoke PASS: ket egymast koveto inditas sikeres volt."
