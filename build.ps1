param([switch]$Test, [switch]$Stress)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force build | Out-Null
    $flags = @('-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Wconversion', '-Wshadow', '-Werror', '-O2', '-Iinclude')
    & gcc @flags src/trees.c tests/test_trees.c -o build/test_trees.exe
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
    & gcc @flags src/trees.c src/benchmark.c -o build/benchmark.exe
    if ($LASTEXITCODE -ne 0) { throw 'Benchmark compilation failed' }
    & gcc @flags tests/test_lecture.c -o build/test_lecture.exe
    if ($LASTEXITCODE -ne 0) { throw 'Lecture test compilation failed' }
    if ($Test -or $Stress) {
        & ./build/test_lecture.exe
        if ($LASTEXITCODE -ne 0) { throw 'Lecture checks failed' }
        if ($Stress) { & ./build/test_trees.exe --stress } else { & ./build/test_trees.exe }
        if ($LASTEXITCODE -ne 0) { throw 'Correctness tests failed' }
    }
} finally { Pop-Location }
