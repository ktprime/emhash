# build_single.ps1 — PowerShell mirror of build_single.sh.
#
# Builds "one binary = one map x one workload" benchmarks (docs §6 row 7), so a
# sub-10% A/B verdict does not depend on the layout of bench_main.cpp's giant TU.
#
# usage:
#   .\build_single.ps1 [-Maps "emhash9"] [-Ops "insert find_hit"]
#                      [-Out .\build_single] [-Compiler clang++]
#                      [-Std 17] [-StringHash wyhash] [-Extra "-mavx2"] [-List]
#
# Outputs <map>_<op>[_wyhash].exe, each accepting bench_main.cpp's --single
# arguments, so ab.ps1 can drive paired A/B runs unchanged.
#
# -Callgrind exists only in build_single.sh: instruction counting needs valgrind,
# i.e. Linux/WSL.

[CmdletBinding()]
param(
    [string[]]$Maps = @('emhash5', 'emhash6', 'emhash7', 'emhash8', 'emhash9',
                        'emilib1', 'emilib2', 'emilib3', 'emilib4'),
    [string[]]$Ops = @('insert', 'find_hit', 'find_miss', 'iterate', 'erase_all', 'insert_erase',
                        'churn', 'churn_find_miss'),
    [string]$Out = (Join-Path $PSScriptRoot 'build_single'),
    [string]$Compiler = 'clang++',
    [int]$Std = 17,
    [ValidateSet('', 'wyhash')][string]$StringHash = '',
    [string]$Extra = '',
    [switch]$List
)

$ErrorActionPreference = 'Stop'

$MapInclude = @{
    emhash5 = 'emhash/hash_table5.hpp';
    emhash6 = 'emhash/hash_table6.hpp';
    emhash7 = 'emhash/hash_table7.hpp';
    emhash8 = 'emhash/hash_table8.hpp';
    emhash9 = 'emhash/hash_table9.hpp';
    emilib1 = 'emilib/emihmap1.hpp';
    emilib2 = 'emilib/emihmap2.hpp';
    emilib3 = 'emilib/emihmap3.hpp';
    emilib4 = 'emilib/emihmap4.hpp'
}
$MapTemplate = @{
    emhash5 = 'emhash5::HashMap';
    emhash6 = 'emhash6::HashMap';
    emhash7 = 'emhash7::HashMap';
    emhash8 = 'emhash8::HashMap';
    emhash9 = 'emhash9::HashMap';
    emilib1 = 'emilib::HashMap';
    emilib2 = 'emilib2::HashMap';
    emilib3 = 'emilib3::HashMap';
    emilib4 = 'emilib4::HashMap'
}
$OpToken = @{
    insert       = 'Insert';
    find_hit     = 'FindHit';
    find_miss    = 'FindMiss';
    iterate      = 'Iterate';
    erase_all    = 'EraseAll';
    insert_erase = 'InsertErase';
    churn        = 'Churn';
    churn_find_miss = 'ChurnFindMiss'
}

# Both -Maps and -Ops accept space- or comma-separated words in one string.
$Maps = @($Maps | ForEach-Object { $_ -split '[,\s]+' } | Where-Object { $_ -ne '' })
$Ops = @($Ops | ForEach-Object { $_ -split '[,\s]+' } | Where-Object { $_ -ne '' })

foreach ($m in $Maps) {
    if (-not $MapInclude.ContainsKey($m)) { throw "unknown map '$m'" }
}
foreach ($o in $Ops) {
    if (-not $OpToken.ContainsKey($o)) { throw "unknown op '$o'" }
}

# Resolve the matrix up front so a typo fails before compiling anything.
$matrix = foreach ($m in $Maps) {
    foreach ($o in $Ops) {
        $name = "${m}_${o}"
        if ($StringHash -ne '') { $name += "_$StringHash" }
        [pscustomobject]@{
            Name = $name; Map = $m; Op = $o; Include = $MapInclude[$m]
            Template = $MapTemplate[$m]; Token = $OpToken[$o]
        }
    }
}

if ($List) {
    $matrix | Format-Table Name, Include, Token -AutoSize
    return
}

$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$flags = @(
    '-O3', '-g', '-DNDEBUG', "-std=c++$Std", '-Wall', '-Wextra', '-Wno-unused-parameter',
    "-I$root/include", "-I$PSScriptRoot", "-I$(Split-Path $PSScriptRoot -Parent)"
)
if ($StringHash -eq 'wyhash') { $flags += '-DBENCH_WYHASH=1' }
if ($Extra -ne '') { $flags += $Extra.Split(' ', [StringSplitOptions]::RemoveEmptyEntries) }

New-Item -ItemType Directory -Force -Path $Out | Out-Null

foreach ($e in $matrix) {
    $bin = Join-Path $Out "$($e.Name).exe"
    Write-Host ("compile {0,-24} <- {1} / {2}" -f $e.Name, $e.Include, $e.Op)
    $compileArgs = $flags + @(
        "-DBENCH_MAP_INC=<$($e.Include)>",
        "-DBENCH_MAP_TMPL=$($e.Template)",
        "-DBENCH_MAP_NAME=$($e.Map)",
        "-DBENCH_OP=$($e.Token)",
        (Join-Path $PSScriptRoot 'bench_single.cpp'),
        '-o', $bin
    )
    & $Compiler @compileArgs
    if ($LASTEXITCODE -ne 0) {
        throw "compiler failed for $($e.Name) (exit $LASTEXITCODE)"
    }
}

Write-Host ''
Write-Host "built into $Out"
Get-ChildItem -Path $Out -Filter *.exe | Select-Object -ExpandProperty Name
