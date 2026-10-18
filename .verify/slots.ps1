$gameSlotCount = 4
$gameMutexPrefix = "Local\OpenTTDMiniGame"
$gpuMutexName = "Local\OpenTTDMiniPerf"
$heldSlotVariable = "OTTD_MINI_HELD_SLOT"
$slotPollMilliseconds = 500
$sharedRunDirectories = @("baseset", "game", "mini_art")
$gpuMutexOrder = @($gpuMutexName) + @(0..($gameSlotCount - 1) | ForEach-Object { "$gameMutexPrefix$_" })

function New-SlotLease([int]$index, [bool]$exported) {
    [pscustomobject]@{ Index = $index; Exported = $exported; Held = [Collections.Generic.Stack[Threading.Mutex]]::new() }
}

function Open-Mutex([string]$name, [int]$milliseconds) {
    $mutex = [Threading.Mutex]::new($false, $name)
    $taken = $false
    try { $taken = $mutex.WaitOne($milliseconds) } catch [Threading.AbandonedMutexException] { $taken = $true }
    if (-not $taken) { $mutex.Dispose(); return $null }
    $mutex
}

function Lock-Mutex($lease, [string]$name, [int]$milliseconds) {
    $mutex = Open-Mutex $name $milliseconds
    if ($null -eq $mutex) { return $false }
    $lease.Held.Push($mutex)
    return $true
}

function Lock-AnyGameSlot($lease) {
    for ($slot = 0; $slot -lt $gameSlotCount; $slot++) {
        if (Lock-Mutex $lease "$gameMutexPrefix$slot" 0) { $lease.Index = $slot; return $true }
    }
    return $false
}

function Lock-EveryGpuMutex($lease) {
    $announced = $false
    foreach ($name in $gpuMutexOrder) {
        while (-not (Lock-Mutex $lease $name $slotPollMilliseconds)) {
            if (-not $announced) { Write-Host "waiting for the GPU"; $announced = $true }
        }
    }
}

function Lock-GameSlotWaiting($lease) {
    if (Lock-AnyGameSlot $lease) { return }
    Write-Host "waiting for a free game slot ($gameSlotCount busy)"
    while (-not (Lock-AnyGameSlot $lease)) { Start-Sleep -Milliseconds $slotPollMilliseconds }
}

function Get-HeldSlot { [Environment]::GetEnvironmentVariable($heldSlotVariable) }

function Unlock-Slots($lease) {
    while ($lease.Held.Count -gt 0) {
        $mutex = $lease.Held.Pop()
        try { $mutex.ReleaseMutex() } finally { $mutex.Dispose() }
    }
    if ($lease.Exported) { [Environment]::SetEnvironmentVariable($heldSlotVariable, $null) }
}

function Lock-Slots([scriptblock]$acquire) {
    if (Get-HeldSlot) { return New-SlotLease ([int](Get-HeldSlot)) $false }
    $lease = New-SlotLease 0 $true
    try {
        & $acquire $lease | Out-Null
        [Environment]::SetEnvironmentVariable($heldSlotVariable, [string]$lease.Index)
    } catch {
        Unlock-Slots $lease
        throw
    }
    $lease
}

function Lock-GameSlot { Lock-Slots { param($lease) Lock-GameSlotWaiting $lease } }

function Lock-Gpu { Lock-Slots { param($lease) Lock-EveryGpuMutex $lease } }

function Initialize-Slot([string]$run, [int]$index) {
    $slot = Join-Path $run "slot$index"
    New-Item -ItemType Directory -Force $slot | Out-Null
    foreach ($name in $sharedRunDirectories) {
        $link = Join-Path $slot $name
        $target = Join-Path $run $name
        if ((Test-Path -LiteralPath $target) -and -not (Test-Path -LiteralPath $link)) {
            New-Item -ItemType Junction -Path $link -Target $target | Out-Null
        }
    }
    Copy-Item (Join-Path $run "*.cfg") $slot -Force
    $slot
}
