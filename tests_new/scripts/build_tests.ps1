# emhash 测试编译脚本 (Windows PowerShell 版本)
# 用法: .\build_tests.ps1 [target]
# target: all | fuzz | test | bench | quick | stress | clean | run

param(
    [Parameter(Position=0)]
    [string]$Target = "quick"
)

$ErrorActionPreference = "Stop"

$ROOT_DIR = $PSScriptRoot
$FUZZ_DIR = Join-Path $ROOT_DIR "fuzz"
$TEST_DIR = Join-Path $ROOT_DIR "test"
$BENCH_DIR = Join-Path $ROOT_DIR "bench"

$CXXFLAGS = "-std=c++17 -O2 -g -I$ROOT_DIR"
$ASAN_FLAGS = "-fsanitize=address,undefined"

# 颜色输出函数
function Log-Info { Write-Host "[INFO] $args" -ForegroundColor Green }
function Log-Warn { Write-Host "[WARN] $args" -ForegroundColor Yellow }
function Log-Error { Write-Host "[ERROR] $args" -ForegroundColor Red }

# 检查编译器
function Get-Compiler {
    # 优先使用 WSL 中的 gcc
    $wslGcc = wsl which g++ 2>$null
    if ($wslGcc) {
        return "wsl g++"
    }
    
    # 检查 MSVC
    $msvc = Get-Command cl -ErrorAction SilentlyContinue
    if ($msvc) {
        return "msvc"
    }
    
    # 检查 MinGW
    $mingw = Get-Command g++ -ErrorAction SilentlyContinue
    if ($mingw) {
        return "mingw"
    }
    
    Log-Error "No C++ compiler found. Please install gcc (WSL) or MSVC."
    exit 1
}

# WSL 编译函数
function Build-WithWSL {
    param($Source, $Output, $ExtraFlags = "")
    
    $wslSource = $Source -replace '^[A-Z]:', '/mnt/$0' -replace '\\', '/' -replace ':', ''
    $wslOutput = $Output -replace '^[A-Z]:', '/mnt/$0' -replace '\\', '/' -replace ':', ''
    $wslRoot = $ROOT_DIR -replace '^[A-Z]:', '/mnt/$0' -replace '\\', '/' -replace ':', ''
    
    $cmd = "g++ -std=c++17 -O2 -g -I$wslRoot $ExtraFlags $wslSource -o $wslOutput"
    Log-Info "Compiling: $Source"
    wsl bash -c $cmd
}

function Build-Fuzz {
    Log-Info "Building fuzz tests..."
    $binDir = Join-Path $FUZZ_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null
    
    # WSL 编译 (fuzzer 需要 clang)
    Build-WithWSL (Join-Path $FUZZ_DIR "fuzz_extreme.cpp") (Join-Path $binDir "fuzz_extreme_asan") "-fsanitize=address,undefined"
    Build-WithWSL (Join-Path $FUZZ_DIR "fuzz_nocoll.cpp") (Join-Path $binDir "fuzz_nocoll_asan") "-fsanitize=address,undefined"
    Build-WithWSL (Join-Path $FUZZ_DIR "min_repro.cpp") (Join-Path $binDir "min_repro")
    Build-WithWSL (Join-Path $FUZZ_DIR "debug_chain.cpp") (Join-Path $binDir "debug_chain")
    Build-WithWSL (Join-Path $FUZZ_DIR "debug_set_erase.cpp") (Join-Path $binDir "debug_set_erase")
    
    Log-Info "Fuzz tests built"
}

function Build-Test {
    Log-Info "Building unit tests..."
    $binDir = Join-Path $TEST_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null
    
    # 核心测试
    Build-WithWSL (Join-Path $TEST_DIR "test_emhash58.cpp") (Join-Path $binDir "test_emhash58")
    Build-WithWSL (Join-Path $TEST_DIR "test_emhash5_verify.cpp") (Join-Path $binDir "test_emhash5_verify")
    Build-WithWSL (Join-Path $TEST_DIR "test_emhash5_stress.cpp") (Join-Path $binDir "test_emhash5_stress")
    Build-WithWSL (Join-Path $TEST_DIR "test_emhash5_hifi.cpp") (Join-Path $binDir "test_emhash5_hifi")
    
    # emilib 测试
    Build-WithWSL (Join-Path $TEST_DIR "test_emilib2o.cpp") (Join-Path $binDir "test_emilib2o")
    Build-WithWSL (Join-Path $TEST_DIR "test_emilib2o_basic.cpp") (Join-Path $binDir "test_emilib2o_basic")
    Build-WithWSL (Join-Path $TEST_DIR "test_emilib2o_stress.cpp") (Join-Path $binDir "test_emilib2o_stress")
    
    # 边界测试
    Build-WithWSL (Join-Path $TEST_DIR "test_extreme.cpp") (Join-Path $binDir "test_extreme")
    Build-WithWSL (Join-Path $TEST_DIR "test_interface_combo.cpp") (Join-Path $binDir "test_interface_combo")
    Build-WithWSL (Join-Path $TEST_DIR "test_probe_bug.cpp") (Join-Path $binDir "test_probe_bug")
    
    Log-Info "Unit tests built"
}

function Build-Bench {
    Log-Info "Building benchmarks..."
    $binDir = Join-Path $BENCH_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null
    
    # 快速验证
    Build-WithWSL (Join-Path $BENCH_DIR "quick_test8.cpp") (Join-Path $binDir "quick_test8")
    Build-WithWSL (Join-Path $BENCH_DIR "test_hidden_bugs8.cpp") (Join-Path $binDir "test_hidden_bugs8")
    Build-WithWSL (Join-Path $BENCH_DIR "highload_test.cpp") (Join-Path $binDir "highload_test")
    Build-WithWSL (Join-Path $BENCH_DIR "smallsize_test.cpp") (Join-Path $binDir "smallsize_test")
    
    # 压力测试
    Build-WithWSL (Join-Path $BENCH_DIR "stress_fix.cpp") (Join-Path $binDir "stress_fix_asan") "-fsanitize=address,undefined"
    Build-WithWSL (Join-Path $BENCH_DIR "fuzz_reproduce.cpp") (Join-Path $binDir "fuzz_reproduce")
    Build-WithWSL (Join-Path $BENCH_DIR "reproduce_crash.cpp") (Join-Path $binDir "reproduce_crash")
    
    # Hash攻击
    Build-WithWSL (Join-Path $BENCH_DIR "hash_attack.cpp") (Join-Path $binDir "hash_attack")
    Build-WithWSL (Join-Path $BENCH_DIR "hash_attack7.cpp") (Join-Path $binDir "hash_attack7")
    
    # 性能基准
    Build-WithWSL (Join-Path $BENCH_DIR "ebench.cpp") (Join-Path $binDir "ebench")
    Build-WithWSL (Join-Path $BENCH_DIR "highload_bench.cpp") (Join-Path $binDir "highload_bench")
    
    Log-Info "Benchmarks built"
}

function Build-Quick {
    Log-Info "Building quick validation tests..."
    $binDir = Join-Path $BENCH_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null
    
    Build-WithWSL (Join-Path $BENCH_DIR "quick_test8.cpp") (Join-Path $binDir "quick_test8")
    Build-WithWSL (Join-Path $BENCH_DIR "test_hidden_bugs8.cpp") (Join-Path $binDir "test_hidden_bugs8")
    Build-WithWSL (Join-Path $BENCH_DIR "highload_test.cpp") (Join-Path $binDir "highload_test")
    
    Log-Info "Quick tests built"
}

function Build-Stress {
    Log-Info "Building stress tests with ASan..."
    $binDir = Join-Path $BENCH_DIR "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null
    
    Build-WithWSL (Join-Path $BENCH_DIR "stress_fix.cpp") (Join-Path $binDir "stress_fix_asan") "-fsanitize=address,undefined"
    Build-WithWSL (Join-Path $TEST_DIR "test_emhash5_stress.cpp") (Join-Path $TEST_DIR "bin/test_emhash5_stress_asan") "-fsanitize=address,undefined"
    Build-WithWSL (Join-Path $TEST_DIR "test_emhash5_hifi.cpp") (Join-Path $TEST_DIR "bin/test_emhash5_hifi_asan") "-fsanitize=address,undefined"
    
    Log-Info "Stress tests built with ASan"
}

function Run-Tests {
    Log-Info "Running quick tests..."
    
    $compiler = Get-Compiler
    if ($compiler -eq "wsl g++") {
        Log-Info "=== Quick validation (WSL) ==="
        wsl bash -c "cd /mnt/d/emhash/bench/bin && ./quick_test8"
        wsl bash -c "cd /mnt/d/emhash/bench/bin && ./test_hidden_bugs8"
        wsl bash -c "cd /mnt/d/emhash/bench/bin && ./highload_test"
        
        Log-Info "=== Unit tests (WSL) ==="
        wsl bash -c "cd /mnt/d/emhash/test/bin && ./test_emhash58"
        wsl bash -c "cd /mnt/d/emhash/test/bin && ./test_emhash5_verify"
        wsl bash -c "cd /mnt/d/emhash/test/bin && ./test_extreme"
    }
    
    Log-Info "All tests completed"
}

function Clean {
    Log-Info "Cleaning build artifacts..."
    
    Remove-Item -Recurse -Force (Join-Path $FUZZ_DIR "bin") -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force (Join-Path $TEST_DIR "bin") -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force (Join-Path $BENCH_DIR "bin") -ErrorAction SilentlyContinue
    
    Get-ChildItem -Path $FUZZ_DIR, $TEST_DIR, $BENCH_DIR -Filter "*.o" -Recurse | Remove-Item -Force
    
    Log-Info "Clean completed"
}

# 主入口
switch ($Target) {
    "all" {
        Build-Fuzz
        Build-Test
        Build-Bench
    }
    "fuzz" {
        Build-Fuzz
    }
    "test" {
        Build-Test
    }
    "bench" {
        Build-Bench
    }
    "quick" {
        Build-Quick
    }
    "stress" {
        Build-Stress
    }
    "run" {
        Build-Quick
        Run-Tests
    }
    "clean" {
        Clean
    }
    default {
        Write-Host "用法: .\build_tests.ps1 [target]"
        Write-Host "target: all | fuzz | test | bench | quick | stress | run | clean"
        Write-Host ""
        Write-Host "示例:"
        Write-Host "  .\build_tests.ps1 quick    # 快速构建验证测试"
        Write-Host "  .\build_tests.ps1 stress   # 构建压力测试(带ASan)"
        Write-Host "  .\build_tests.ps1 run      # 构建并运行快速测试"
        exit 1
    }
}