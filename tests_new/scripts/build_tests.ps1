# emhash Test Build Script (Windows PowerShell version)
# Usage: .\build_tests.ps1 [target]
# target: all | fuzz | test | bench | quick | stress | clean | run

param(
    [Parameter(Position=0)]
    [string]$Target = "quick"
)

$ErrorActionPreference = "Stop"

$ROOT_DIR = Split-Path $PSScriptRoot -Parent
$FUZZ_DIR = Join-Path $ROOT_DIR "fuzz"
$STRESS_DIR = Join-Path $ROOT_DIR "stress"
$DEBUG_DIR = Join-Path $ROOT_DIR "debug"
$VERIFY_DIR = Join-Path $ROOT_DIR "verify"
$ATTACK_DIR = Join-Path $ROOT_DIR "attack"
$BENCH_DIR = Join-Path $ROOT_DIR "bench"

# Color output functions
function Log-Info { Write-Host "[INFO] $args" -ForegroundColor Green }
function Log-Warn { Write-Host "[WARN] $args" -ForegroundColor Yellow }
function Log-Error { Write-Host "[ERROR] $args" -ForegroundColor Red }

# Check compiler
function Get-Compiler {
    # Prefer gcc in WSL
    $wslGcc = wsl which g++ 2>$null
    if ($wslGcc) {
        return "wsl g++"
    }
    
    # Check MSVC
    $msvc = Get-Command cl -ErrorAction SilentlyContinue
    if ($msvc) {
        return "msvc"
    }
    
    # Check MinGW
    $mingw = Get-Command g++ -ErrorAction SilentlyContinue
    if ($mingw) {
        return "mingw"
    }
    
    Log-Error "No C++ compiler found. Please install gcc (WSL) or MSVC."
    exit 1
}

# WSL build function
function Build-WithWSL {
    param($Source, $Output, $ExtraFlags = "")

    $wslSource = $Source -replace '^[A-Z]:', '/mnt/$0' -replace '\\', '/' -replace ':', ''
    $wslOutput = $Output -replace '^[A-Z]:', '/mnt/$0' -replace '\\', '/' -replace ':', ''
    $wslRoot = $ROOT_DIR -replace '^[A-Z]:', '/mnt/$0' -replace '\\', '/' -replace ':', ''
    $wslProject = Split-Path $wslRoot -Parent

    $cmd = "g++ -std=c++17 -O2 -g -I$wslRoot -I$wslProject/thirdparty $ExtraFlags $wslSource -o $wslOutput"
    Log-Info "Compiling: $Source"
    wsl bash -c $cmd
}

function Build-Fuzz {
    Log-Info "Building fuzz tests..."
    $binDir = Join-Path $FUZZ_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null
    
    # WSL build (fuzzer requires clang)
    Build-WithWSL (Join-Path $FUZZ_DIR "fuzz_extreme.cpp") (Join-Path $binDir "fuzz_extreme_asan") "-fsanitize=address,undefined"
    Build-WithWSL (Join-Path $FUZZ_DIR "fuzz_nocoll.cpp") (Join-Path $binDir "fuzz_nocoll_asan") "-fsanitize=address,undefined"
    
    Log-Info "Fuzz tests built"
}

function Build-Verify {
    Log-Info "Building verification tests..."
    $binDir = Join-Path $VERIFY_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null
    
    # Core tests
    Build-WithWSL (Join-Path $VERIFY_DIR "test_all_maps.cpp") (Join-Path $binDir "test_all_maps")
    Build-WithWSL (Join-Path $VERIFY_DIR "test_extreme.cpp") (Join-Path $binDir "test_extreme")
    Build-WithWSL (Join-Path $VERIFY_DIR "test_interface_combo.cpp") (Join-Path $binDir "test_interface_combo")
    
    Log-Info "Verification tests built"
}

function Build-Stress {
    Log-Info "Building stress tests..."
    $binDir = Join-Path $STRESS_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null
    
    # Stress tests
    Build-WithWSL (Join-Path $STRESS_DIR "stress_all_maps.cpp") (Join-Path $binDir "stress_all_maps")
    Build-WithWSL (Join-Path $STRESS_DIR "stress_fix.cpp") (Join-Path $binDir "stress_fix_asan") "-fsanitize=address,undefined"
    Build-WithWSL (Join-Path $STRESS_DIR "highload_test.cpp") (Join-Path $binDir "highload_test")
    
    Log-Info "Stress tests built"
}

function Build-Attack {
    Log-Info "Building hash attack tests..."
    $binDir = Join-Path $ATTACK_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null
    
    # Hash attack
    Build-WithWSL (Join-Path $ATTACK_DIR "hash_attack_all.cpp") (Join-Path $binDir "hash_attack_all") "-DEMH_SAFE_PSL"
    
    Log-Info "Hash attack tests built"
}

function Build-Debug {
    Log-Info "Building debug tests..."
    $binDir = Join-Path $DEBUG_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null
    
    # Debug tests
    Build-WithWSL (Join-Path $DEBUG_DIR "debug_all_maps.cpp") (Join-Path $binDir "debug_all_maps") "-g -O0"
    
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
    
    $compiler = Get-Compiler
    if ($compiler -eq "wsl g++") {
        Log-Info "=== Verification tests (WSL) ==="
        wsl bash -c "cd /mnt/d/emhash/tests_new/verify/bin && ./test_all_maps"
        wsl bash -c "cd /mnt/d/emhash/tests_new/debug/bin && ./debug_all_maps"
    }
    
    Log-Info "All tests completed"
}

function Clean {
    Log-Info "Cleaning build artifacts..."
    
    Remove-Item -Recurse -Force (Join-Path $FUZZ_DIR "bin") -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force (Join-Path $STRESS_DIR "bin") -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force (Join-Path $DEBUG_DIR "bin") -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force (Join-Path $VERIFY_DIR "bin") -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force (Join-Path $ATTACK_DIR "bin") -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force (Join-Path $BENCH_DIR "bin") -ErrorAction SilentlyContinue
    
    Log-Info "Clean completed"
}

# Main entry
switch ($Target) {
    "all" {
        Build-Fuzz
        Build-Verify
        Build-Stress
        Build-Attack
        Build-Debug
    }
    "fuzz" {
        Build-Fuzz
    }
    "verify" {
        Build-Verify
    }
    "stress" {
        Build-Stress
    }
    "attack" {
        Build-Attack
    }
    "debug" {
        Build-Debug
    }
    "quick" {
        Build-Quick
    }
    "run" {
        Build-Quick
        Run-Tests
    }
    "clean" {
        Clean
    }
    default {
        Write-Host "Usage: .\build_tests.ps1 [target]"
        Write-Host "target: all | fuzz | verify | stress | attack | debug | quick | run | clean"
        Write-Host ""
        Write-Host "Examples:"
        Write-Host "  .\build_tests.ps1 quick    # Quick build validation tests"
        Write-Host "  .\build_tests.ps1 stress   # Build stress tests (with ASan)"
        Write-Host "  .\build_tests.ps1 run      # Build and run quick tests"
        exit 1
    }
}