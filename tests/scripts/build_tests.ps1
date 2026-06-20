# emhash Test Build Script (Windows PowerShell)
# Usage: .\build_tests.ps1 [target] [compiler]
# target: all | fuzz | verify | stress | attack | debug | bench | quick | run | clean | info
# compiler: gcc | clang (default: auto-detect)

param(
    [Parameter(Position=0)]
    [string]$Target = "quick",
    [Parameter(Position=1)]
    [string]$Compiler = "auto"
)

$ErrorActionPreference = "Stop"

$ROOT_DIR = Split-Path $PSScriptRoot -Parent
$FUZZ_DIR = Join-Path $ROOT_DIR "fuzz"
$STRESS_DIR = Join-Path $ROOT_DIR "stress"
$DEBUG_DIR = Join-Path $ROOT_DIR "debug"
$VERIFY_DIR = Join-Path $ROOT_DIR "verify"
$ATTACK_DIR = Join-Path $ROOT_DIR "attack"
$BENCH_DIR = Join-Path $ROOT_DIR "../bench"

# ============================================================================
# OS / Compiler / Architecture detection
# ============================================================================

function Get-TargetOS {
    # Check if WSL is available
    $wslCheck = wsl --list 2>$null
    if ($wslCheck) { return "wsl" }

    # Check for MinGW/MSYS2
    $gpp = Get-Command g++ -ErrorAction SilentlyContinue
    if ($gpp) {
        $version = & g++ --version 2>$null | Select-String -Pattern "mingw|msys|MinGW" -Quiet
        if ($version) { return "mingw" }
        return "mingw"
    }

    return "unknown"
}

function Get-TargetArch {
    $procArch = $env:PROCESSOR_ARCHITECTURE
    switch ($procArch) {
        "AMD64"  { return "x86_64" }
        "ARM64"  { return "aarch64" }
        "x86"    { return "x86" }
        default  { return $procArch.ToLower() }
    }
}

function Get-TargetCompiler {
    param([string]$Preferred = "auto")

    if ($Preferred -eq "gcc" -or $Preferred -eq "g++") { return "g++" }
    if ($Preferred -eq "clang" -or $Preferred -eq "clang++") { return "clang++" }

    # Auto-detect: prefer WSL g++, then MinGW g++, then clang++
    $wslGcc = wsl which g++ 2>$null
    if ($wslGcc) { return "wsl_g++" }

    $mingw = Get-Command g++ -ErrorAction SilentlyContinue
    if ($mingw) { return "g++" }

    $clang = Get-Command clang++ -ErrorAction SilentlyContinue
    if ($clang) { return "clang++" }

    Log-Error "No C++ compiler found. Install WSL, MinGW g++, or clang++."
    exit 1
}

$Script:OS = Get-TargetOS
$Script:Arch = Get-TargetArch
$Script:CXX = Get-TargetCompiler -Preferred $Compiler

# ============================================================================
# Architecture-specific flags
# ============================================================================

switch ($Script:Arch) {
    "x86_64"  { $Script:MarchFlag = "-march=native" }
    "aarch64" { $Script:MarchFlag = "-march=armv8-a" }
    default   { $Script:MarchFlag = "" }
}

# ============================================================================
# Color output
# ============================================================================

function Log-Info  { Write-Host "[INFO] $args" -ForegroundColor Green }
function Log-Warn  { Write-Host "[WARN] $args" -ForegroundColor Yellow }
function Log-Error { Write-Host "[ERROR] $args" -ForegroundColor Red }

function Print-Env {
    Write-Host "=== Build Environment ===" -ForegroundColor Cyan
    Write-Host "  OS:       $($Script:OS)"
    Write-Host "  Arch:     $($Script:Arch)"
    Write-Host "  Compiler: $($Script:CXX)"
    Write-Host "  MARCH:    $(if ($Script:MarchFlag) { $Script:MarchFlag } else { '(none)' })"
    Write-Host "  Project:  $ROOT_DIR"
    Write-Host "=========================" -ForegroundColor Cyan
}

# ============================================================================
# Build helpers
# ============================================================================

function Convert-ToWslPath {
    param([string]$WinPath)
    $drive = $WinPath.Substring(0,1).ToLower()
    $rest = $WinPath.Substring(2).Replace('\', '/')
    return "/mnt/$drive$rest"
}

function Build-Source {
    param($Source, $Output, $ExtraFlags = "")

    $wslRoot = Convert-ToWslPath $ROOT_DIR
    $wslProject = Split-Path $wslRoot -Parent

    if ($Script:CXX -eq "wsl_g++") {
        $wslSource = Convert-ToWslPath $Source
        $wslOutput = Convert-ToWslPath $Output
        $wslInclude = "$wslRoot/../include"
        $cmd = "g++ -std=c++17 -O2 -g -I$wslInclude $ExtraFlags $wslSource -o $wslOutput"
        Log-Info "Compiling (WSL): $(Split-Path $Source -Leaf)"
        wsl bash -c $cmd
    }
    elseif ($Script:CXX -match "clang\+\+") {
        $cxxFlags = "-std=c++17 -O2 -g -I`"$($ROOT_DIR)\..\include`""
        Log-Info "Compiling (clang++): $(Split-Path $Source -Leaf)"
        & clang++ $cxxFlags.Split(' ') $ExtraFlags.Split(' ') $Source -o $Output
    }
    else {
        $cxxFlags = "-std=c++17 -O2 -g -I`"$($ROOT_DIR)\..\include`""
        Log-Info "Compiling (g++): $(Split-Path $Source -Leaf)"
        & g++ $cxxFlags.Split(' ') $ExtraFlags.Split(' ') $Source -o $Output
    }
}

# ============================================================================
# Build functions
# ============================================================================

function Build-Fuzz {
    Log-Info "Building fuzz tests..."
    $binDir = Join-Path $FUZZ_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null

    # Only build fuzz files that actually exist
    $fuzzFiles = @("fuzz_extreme", "fuzz_nocoll")
    foreach ($f in $fuzzFiles) {
        $src = Join-Path $FUZZ_DIR "$f.cpp"
        if (Test-Path $src) {
            Build-Source $src (Join-Path $binDir "${f}_asan") "-fsanitize=address,undefined"
        } else {
            Log-Warn "Skipping $f.cpp (not found)"
        }
    }

    # Always build reproduce_emhash8_bug
    Build-Source (Join-Path $FUZZ_DIR "reproduce_emhash8_bug.cpp") (Join-Path $binDir "reproduce_emhash8_bug")

    Log-Info "Fuzz tests built"
}

function Build-Verify {
    Log-Info "Building verification tests..."
    $binDir = Join-Path $VERIFY_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null

    $verifyTests = @(
        "test_all_maps", "test_extreme", "test_interface_combo",
        "test_hashset_allocator", "test_emilib_comprehensive", "test_size_sweep",
        "edge_test", "test_hashmap_full_api", "test_hashset_full_api", "test_special_key_types", "test_merge_correctness"
    )
    foreach ($test in $verifyTests) {
        Build-Source (Join-Path $VERIFY_DIR "$test.cpp") (Join-Path $binDir $test)
    }

    Log-Info "Verification tests built"
}

function Build-Stress {
    Log-Info "Building stress tests..."
    $binDir = Join-Path $STRESS_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null

    Build-Source (Join-Path $STRESS_DIR "stress_all_maps.cpp") (Join-Path $binDir "stress_all_maps")
    Build-Source (Join-Path $STRESS_DIR "stress_fix.cpp") (Join-Path $binDir "stress_fix_asan") "-fsanitize=address,undefined"
    Build-Source (Join-Path $STRESS_DIR "highload_test.cpp") (Join-Path $binDir "highload_test")
    Build-Source (Join-Path $STRESS_DIR "test_emhash5_stress.cpp") (Join-Path $binDir "test_emhash5_stress_asan") "-fsanitize=address,undefined"
    Build-Source (Join-Path $STRESS_DIR "test_emhash5_hifi.cpp") (Join-Path $binDir "test_emhash5_hifi_asan") "-fsanitize=address,undefined"
    Build-Source (Join-Path $STRESS_DIR "test_bad_hash_stress.cpp") (Join-Path $binDir "test_bad_hash_stress")

    Log-Info "Stress tests built"
}

function Build-Attack {
    Log-Info "Building hash attack tests..."
    $binDir = Join-Path $ATTACK_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null

    Build-Source (Join-Path $ATTACK_DIR "hash_attack_all.cpp") (Join-Path $binDir "hash_attack_all") "-DEMH_SAFE_PSL"

    Log-Info "Hash attack tests built"
}

function Build-Debug {
    Log-Info "Building debug tests..."
    $binDir = Join-Path $DEBUG_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null

    Build-Source (Join-Path $DEBUG_DIR "debug_all_maps.cpp") (Join-Path $binDir "debug_all_maps") "-g -O0"
    Build-Source (Join-Path $DEBUG_DIR "test_probe_bug.cpp") (Join-Path $binDir "test_probe_bug")
    Build-Source (Join-Path $DEBUG_DIR "test_probe_coverage.cpp") (Join-Path $binDir "test_probe_coverage")
    Build-Source (Join-Path $DEBUG_DIR "test_debug_find.cpp") (Join-Path $binDir "test_debug_find") "-g -O0"
    Build-Source (Join-Path $DEBUG_DIR "test_repro_collision.cpp") (Join-Path $binDir "test_repro_collision") "-g -O0"

    Log-Info "Debug tests built"
}

function Build-Quick {
    Log-Info "Building quick validation tests..."
    Build-Verify
    Build-Debug
    Log-Info "Quick tests built"
}

function Run-Tests {
    Log-Info "Running quick tests..."

    if ($Script:CXX -eq "wsl_g++") {
        $wslBinVerify = Convert-ToWslPath (Join-Path $VERIFY_DIR "bin")
        $wslBinDebug = Convert-ToWslPath (Join-Path $DEBUG_DIR "bin")
        wsl bash -c "cd $wslBinVerify && ./test_all_maps"
        wsl bash -c "cd $wslBinDebug && ./debug_all_maps"
    }
    else {
        & (Join-Path $VERIFY_DIR "bin\test_all_maps.exe")
        & (Join-Path $DEBUG_DIR "bin\debug_all_maps.exe")
    }

    Log-Info "All tests completed"
}

function Clean {
    Log-Info "Cleaning build artifacts..."

    @($FUZZ_DIR, $STRESS_DIR, $DEBUG_DIR, $VERIFY_DIR, $ATTACK_DIR, $BENCH_DIR) | ForEach-Object {
        $binDir = Join-Path $_ "bin"
        if (Test-Path $binDir) { Remove-Item -Recurse -Force $binDir }
    }

    Log-Info "Clean completed"
}

# ============================================================================
# Main entry
# ============================================================================

Print-Env

switch ($Target) {
    "all"     { Build-Fuzz; Build-Verify; Build-Stress; Build-Attack; Build-Debug }
    "fuzz"    { Build-Fuzz }
    "verify"  { Build-Verify }
    "stress"  { Build-Stress }
    "attack"  { Build-Attack }
    "debug"   { Build-Debug }
    "bench"   { Log-Warn "Benchmarks not supported from PowerShell. Use WSL: ./build_tests.sh bench" }
    "quick"   { Build-Quick }
    "run"     { Build-Quick; Run-Tests }
    "clean"   { Clean }
    "info"    { Print-Env }
    default {
        Write-Host "Usage: .\build_tests.ps1 [target] [compiler]"
        Write-Host ""
        Write-Host "Targets:"
        Write-Host "  all      - Build all tests"
        Write-Host "  verify   - Build verification tests"
        Write-Host "  stress   - Build stress tests"
        Write-Host "  attack   - Build hash attack tests"
        Write-Host "  debug    - Build debug tests"
        Write-Host "  fuzz     - Build fuzz tests"
        Write-Host "  quick    - Build verification + debug tests"
        Write-Host "  run      - Build and run quick tests"
        Write-Host "  clean    - Remove all build artifacts"
        Write-Host "  info     - Show build environment info"
        Write-Host ""
        Write-Host "Compilers: gcc | clang | auto (default: auto-detect)"
        Write-Host ""
        Write-Host "Examples:"
        Write-Host "  .\build_tests.ps1 quick          # Quick build (auto-detect)"
        Write-Host "  .\build_tests.ps1 all gcc        # Build all with gcc"
        Write-Host "  .\build_tests.ps1 verify clang   # Build verify with clang++"
        Write-Host ""
        Write-Host "Detected: OS=$($Script:OS)  Arch=$($Script:Arch)  Compiler=$($Script:CXX)"
        exit 1
    }
}
