param(
    [ValidateSet("capture", "perf")] [string]$Kind = "capture",
    [string]$Scenario = "std-downtown",
    [int]$Restarts = 4
)
$verify = $PSScriptRoot
$original = "capture-onscreen-backup.ps1"
$candidate = "capture-offscreen.ps1"

function Invoke-Tracked($script, $argumentsText, $interval, $fine) {
    $fineFlag = if ($fine) { "-Fine" } else { "" }
    $lines = @(powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$verify\offscreen-track.ps1' -Script '$script' -Arguments @{ $argumentsText } -IntervalMs $interval $fineFlag")
    $lines | ForEach-Object { "  $_" } | Write-Host
    $clean = $lines | Where-Object { $_ -match 'clean=True exe (.+)$' } | Select-Object -Last 1
    if ($clean -match 'clean=True exe (.+)$') { return $Matches[1] }
    return $null
}

function Invoke-CaptureRun($script, $label) {
    Write-Host "=== $label $script"
    Invoke-Tracked $script "Label = '$label'; ShotList = '$verify\shots-offscreen-pair.txt'; Step = 16; Speed = 1" 2 $true
}

function Invoke-PerfRun($script, $tag) {
    Write-Host "=== $Scenario $tag $script"
    Invoke-Tracked "perf-offscreen.ps1" "Run = '$Scenario'; Tag = '$tag'; Capture = '$script'" 100 $false
}

for ($round = 1; $round -le $Restarts; $round++) {
    if ($Kind -eq "capture") {
        $stamps = @(
            Invoke-CaptureRun $original "offscreen-pair-o1"
            Invoke-CaptureRun $candidate "offscreen-pair-n1"
            Invoke-CaptureRun $original "offscreen-pair-o2"
            Invoke-CaptureRun $candidate "offscreen-pair-n2"
        )
    } else {
        $stamps = @()
        foreach ($pair in 1, 2) {
            for ($retry = 1; $retry -le $Restarts; $retry++) {
                $pairStamps = @(
                    Invoke-PerfRun $original "offscreen-o$pair"
                    Invoke-PerfRun $candidate "offscreen-n$pair"
                )
                if ($pairStamps.Count -eq 2 -and $null -ne $pairStamps[0] -and $pairStamps[0] -eq $pairStamps[1]) { break }
                Write-Host "pair $pair redone: $($pairStamps -join ' / ')"
            }
            $stamps += $pairStamps
        }
    }
    $distinct = @($stamps | Sort-Object -Unique)
    Write-Host "round $round stamps: $($stamps -join ' / ')"
    if ($Kind -eq "perf" -or ($stamps.Count -eq 4 -and $distinct.Count -eq 1 -and $null -ne $distinct[0])) { break }
}
"series done $(Get-Date -Format 'HH:mm:ss')"
