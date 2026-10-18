param(
    [Parameter(Mandatory)][string]$Lane,
    [string]$Branch = "",
    [Parameter(Mandatory)][string]$From,
    [int]$Jobs = 4
)
$ErrorActionPreference = "Stop"
$mainRepo = Split-Path $PSScriptRoot -Parent
$mainBuild = Join-Path $mainRepo "build"
$worktree = Join-Path "E:\ottd-wt" $Lane
$build = Join-Path $worktree "build"
$toolchainPath = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin"
$inheritedEntries = @("CMAKE_BUILD_TYPE", "CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER", "CMAKE_RC_COMPILER", "CMAKE_MAKE_PROGRAM")
$runtimeExtras = @("*.dll", "baseset\*")
$failureTailLines = 40
$env:PATH = "$toolchainPath;$env:PATH"

function Invoke-Native([string]$Program, [string[]]$Arguments) {
    $ErrorActionPreference = "Continue"
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program exited with $LASTEXITCODE" }
}

function Read-CacheEntries([string]$CacheFile) {
    $entries = @{}
    foreach ($line in Get-Content $CacheFile) {
        if ($line -match '^([A-Za-z0-9_\-]+):[A-Z]+=(.*)$') { $entries[$Matches[1]] = $Matches[2] }
    }
    $entries
}

function Read-OptionDefaults([string]$OptionsFile) {
    $defaults = @{}
    foreach ($line in Get-Content $OptionsFile) {
        if ($line -match '^\s*option\((OPTION_\w+)\s+"[^"]*"\s+(ON|OFF)\)') { $defaults[$Matches[1]] = $Matches[2] }
    }
    $defaults
}

function Get-ConfigureDefinitions {
    $defaults = Read-OptionDefaults (Join-Path $worktree "cmake\Options.cmake")
    $changedOptions = $defaults.Keys | Where-Object { $mainCache.ContainsKey($_) -and $mainCache[$_] -ne $defaults[$_] }
    foreach ($name in @($inheritedEntries) + @($changedOptions)) { "-D$name=$($mainCache[$name])" }
}

function New-Worktree {
    if (Test-Path (Join-Path $worktree ".git")) { return }
    $placement = if ($Branch) { @("-b", $Branch, $worktree, $From) } else { @("--detach", $worktree, $From) }
    Invoke-Native git (@("-C", $mainRepo, "worktree", "add") + $placement)
}

function Initialize-Build {
    if (Test-Path (Join-Path $build "CMakeCache.txt")) { return }
    Invoke-Native cmake (@("-S", $worktree, "-B", $build, "-G", $mainCache["CMAKE_GENERATOR"]) + @(Get-ConfigureDefinitions))
}

function Invoke-Build {
    $tail = [Collections.Generic.Queue[string]]::new()
    $ErrorActionPreference = "Continue"
    & ninja -C $build -j $Jobs | ForEach-Object {
        Write-Host $_
        $tail.Enqueue($_)
        if ($tail.Count -gt $failureTailLines) { [void]$tail.Dequeue() }
    }
    if ($LASTEXITCODE -ne 0) { throw "ninja exited with $LASTEXITCODE`n$($tail -join "`n")" }
}

function Copy-MissingRuntimeFiles {
    foreach ($pattern in $runtimeExtras) {
        foreach ($source in Get-ChildItem (Join-Path $mainBuild $pattern) -File -ErrorAction SilentlyContinue) {
            $relative = $source.FullName.Substring($mainBuild.Length + 1)
            $target = Join-Path $build $relative
            if (Test-Path $target) { continue }
            New-Item -ItemType Directory -Force (Split-Path $target -Parent) | Out-Null
            Copy-Item $source.FullName $target
        }
    }
}

$mainCache = Read-CacheEntries (Join-Path $mainBuild "CMakeCache.txt")
New-Worktree
Initialize-Build
Invoke-Build
Copy-MissingRuntimeFiles
Join-Path $build "openttd.exe"
