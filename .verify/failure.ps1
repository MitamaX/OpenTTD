$errorDialogTitles = @("Error!", "Fatal Application Failure", "Fatal Application Exit", "Assertion Failure")
$crashLogFilter = "crash*.json.log"

function Test-ErrorDialog([string[]]$titles) {
    @($titles | Where-Object { $errorDialogTitles -contains $_ }).Count -gt 0
}

function Get-CrashLogNames([string]$slot) {
    @(Get-ChildItem -LiteralPath $slot -Filter $crashLogFilter -ErrorAction SilentlyContinue | ForEach-Object Name)
}

function Get-NewCrashLog([string]$slot, [string[]]$known) {
    Get-ChildItem -LiteralPath $slot -Filter $crashLogFilter -ErrorAction SilentlyContinue |
        Where-Object { $known -notcontains $_.Name } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
}

function Get-CrashReason($crashLog) {
    try { (Get-Content -LiteralPath $crashLog.FullName -Raw | ConvertFrom-Json).crash.reason }
    catch { "unreadable $($crashLog.Name)" }
}
