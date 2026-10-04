@echo off
call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

cd /d d:\emhash\bench\emilib_bench
if not exist build_msvc mkdir build_msvc

cd build_msvc
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DEMILIB_BENCH_ENABLE_BOOST=ON
cmake --build . --config Release

echo.
echo === Build complete. Run with: build_msvc\Release\emilib_bench.exe ===
