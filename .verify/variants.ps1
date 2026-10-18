param(
    [string[]]$Names,
    [string[]]$Runs = @("user"),
    [string]$Tag = "v"
)
$ErrorActionPreference = "Stop"
$verify = $PSScriptRoot
$repo = Split-Path $verify -Parent
$python = "C:\msys64\ucrt64\bin\python.exe"
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH

function Build {
    $ErrorActionPreference = "Continue"
    $output = & ninja -C (Join-Path $repo "build") 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { $output | Select-Object -Last 30; throw "build failed" }
}

$report = Join-Path $verify "variants-$Tag.txt"
foreach ($name in $Names) {
    try {
        if ($name -ne "base") {
            & $python -I (Join-Path $verify "variant.py") ($name -split "\+")
            if ($LASTEXITCODE -ne 0) { throw "variant $name failed to apply" }
        }
        Build
        $table = & (Join-Path $verify "terms.ps1") -Label "$Tag-$name" -Runs $Runs
        "## $name" | Add-Content $report
        $table | Where-Object { $_ -notmatch "^exit=" } | Add-Content $report
    } finally {
        & $python -I (Join-Path $verify "variant.py") restore
    }
}
Build
Get-Content $report
