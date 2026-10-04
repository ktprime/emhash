# ab.ps1 - paired A/B comparison runner (PowerShell mirror of ab.sh).
#
# Implements the same methodology as ab.sh:
#   1. interleave A/B runs and alternate which side goes first within each pair,
#      so first-run/cache/turbo effects cancel out
#   2. count per-pair wins, not just the means
#   3. only trust a change that wins nearly every pair; sub-2% with no pairing
#      advantage is noise
#
# The harness must print a bare number on stdout (its --single mode).
#
# usage:
#   ./ab.ps1 -BinA <path> -BinB <path> -Op <op> -Map <map> -Key <key> -Size <n> [-Pairs 6] [-Reps 1]
#   ./ab.ps1 -BinA <path> -BinB <path> -All [-Pairs 6] [-Reps 1]
#
# example:
#   ./ab.ps1 -BinA .\build\bench_base.exe -BinB .\build\bench_cand.exe `
#            -Op find_hit -Map emhash7 -Key int64 -Size 100000

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BinA,
    [Parameter(Mandatory = $true)][string]$BinB,

    [string]$Op   = 'find_hit',
    [string]$Map  = 'emhash7',
    [string]$Key  = 'int64',
    [int]   $Size = 100000,

    [switch]$All,

    [int]$Pairs = 6,
    # One harness invocation costs ~40ms, so taking the min of several is cheap
    # and measurably reduces run-to-run spread on a noisy host.
    [int]$Reps  = 3,

    [ValidateSet('min', 'median')][string]$Metric = 'min'
)

$ErrorActionPreference = 'Stop'

foreach ($b in @($BinA, $BinB)) {
    if (-not (Test-Path -LiteralPath $b)) {
        Write-Error "error: '$b' not found"
        exit 2
    }
}

# Workload matrix for -All: "op map key size"
$AllWorkloads = @(
    @('insert',       'emhash7', 'int64',  100000),
    @('find_hit',     'emhash7', 'int64',  100000),
    @('find_miss',    'emhash7', 'int64',  100000),
    @('erase_all',    'emhash7', 'int64',  100000),
    @('insert_erase', 'emhash7', 'int64',  100000),
    @('insert',       'emhash7', 'string', 100000),
    @('find_hit',     'emhash7', 'string', 100000)
)

function Invoke-SideOnce {
    param([string]$Bin, [string]$Op, [string]$Map, [string]$Key, [int]$Size)
    $out = & $Bin --single --op $Op --map $Map --key $Key --size $Size --metric $Metric
    if (-not $out) { throw "harness '$Bin' produced no output" }
    return [double]$out
}

function Invoke-SideMin {
    param([string]$Bin, [string]$Op, [string]$Map, [string]$Key, [int]$Size)
    $best = [double]::MaxValue
    for ($i = 0; $i -lt $Reps; $i++) {
        $v = Invoke-SideOnce -Bin $Bin -Op $Op -Map $Map -Key $Key -Size $Size
        if ($v -lt $best) { $best = $v }
    }
    return $best
}

function Compare-Workload {
    param([string]$Op, [string]$Map, [string]$Key, [int]$Size)

    $aWins = 0; $bWins = 0; $aSum = 0.0; $bSum = 0.0

    for ($i = 1; $i -le $Pairs; $i++) {
        # Alternate the running order so first-run bias cancels out.
        if ($i % 2 -eq 1) {
            $ra = Invoke-SideMin -Bin $BinA -Op $Op -Map $Map -Key $Key -Size $Size
            $rb = Invoke-SideMin -Bin $BinB -Op $Op -Map $Map -Key $Key -Size $Size
        } else {
            $rb = Invoke-SideMin -Bin $BinB -Op $Op -Map $Map -Key $Key -Size $Size
            $ra = Invoke-SideMin -Bin $BinA -Op $Op -Map $Map -Key $Key -Size $Size
        }
        if ($ra -lt $rb) { $aWins++ } elseif ($rb -lt $ra) { $bWins++ }
        $aSum += $ra; $bSum += $rb
    }

    $aMean = $aSum / $Pairs
    $bMean = $bSum / $Pairs
    $delta = if ($bMean -ne 0) { ($aMean - $bMean) / $bMean * 100.0 } else { 0.0 }

    if ($aWins -eq $Pairs) { $verdict = 'A faster (all pairs)' }
    elseif ($bWins -eq $Pairs) { $verdict = 'B faster (all pairs)' }
    elseif ($aWins -eq $bWins) { $verdict = 'NOISE (even split)' }
    elseif ([Math]::Abs($delta) -lt 2.0) { $verdict = 'NOISE (<2%)' }
    elseif ($delta -lt 0) { $verdict = 'A faster (majority)' }
    else { $verdict = 'B faster (majority)' }

    [pscustomobject]@{
        Op      = $Op
        Map     = $Map
        Key     = $Key
        Size    = $Size
        A_ns    = [Math]::Round($aMean, 1)
        B_ns    = [Math]::Round($bMean, 1)
        A_wins  = $aWins
        B_wins  = $bWins
        Delta   = '{0:+0.00;-0.00;0.00}%' -f $delta
        Verdict = $verdict
    }
}

Write-Host '=== paired A/B comparison ==='
Write-Host "A     : $BinA"
Write-Host "B     : $BinB"
Write-Host "metric: $Metric   pairs: $Pairs   reps: $Reps"
if ($Pairs -lt 5) {
    Write-Host ''
    Write-Host "WARNING: only $Pairs pairs requested. On a noisy host a single lucky run can"
    Write-Host '         produce a fake ''unanimous'' verdict. Use at least 5-6 pairs.'
}
Write-Host ''

$results = @()
if ($All) {
    foreach ($w in $AllWorkloads) {
        $results += Compare-Workload -Op $w[0] -Map $w[1] -Key $w[2] -Size ([int]$w[3])
    }
} else {
    $results += Compare-Workload -Op $Op -Map $Map -Key $Key -Size $Size
}

Write-Host '=== summary (negative delta => A faster; lower ns is better) ==='
$results | Format-Table -AutoSize

$real  = @($results | Where-Object { $_.Verdict -like '*all pairs*' }).Count
$noise = @($results | Where-Object { $_.Verdict -like 'NOISE*' }).Count
$total = @($results).Count

Write-Host "verdict: $real/$total workloads show a unanimous winner, $noise/$total are within noise."
Write-Host 'Reminder: a change is only real if it wins nearly every pair, not just on the mean.'
