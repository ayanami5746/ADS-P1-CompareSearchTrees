param(
    [ValidateRange(1, 100)][int]$Repeats = 3,
    [ValidateRange(1000, 100000)][int]$MaxN = 100000,
    [ValidateRange(1, 4294967295)][long]$Seed = 20261003,
    [switch]$Stress,
    [switch]$SkipPlots
)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force results | Out-Null
    if (-not $SkipPlots) {
        & python -c 'import matplotlib'
        if ($LASTEXITCODE -ne 0) { throw 'Install plot dependencies first: python -m pip install -r requirements.txt; or use -SkipPlots' }
    }
    & ./build.ps1
    & ./build/test_lecture.exe | Tee-Object results/lecture_checks.log
    if ($LASTEXITCODE -ne 0) { throw 'Lecture checks failed' }
    $testArguments = @()
    if ($Stress) { $testArguments += '--stress' }
    & ./build/test_trees.exe @testArguments | Tee-Object results/correctness.log
    if ($LASTEXITCODE -ne 0) { throw 'Correctness tests failed' }
    & python tests/test_comments.py
    if ($LASTEXITCODE -ne 0) { throw 'Comment counter tests failed' }
    & python tools/check_comments.py --output results/comment_coverage.json | Tee-Object results/comments.log
    if ($LASTEXITCODE -ne 0) { throw 'Comment coverage failed' }
    & ./build/benchmark.exe --output results/benchmark.csv --repeats $Repeats --max-n $MaxN --seed $Seed 2> results/benchmark.log
    if ($LASTEXITCODE -ne 0) { throw 'Benchmark failed; see results/benchmark.log' }
    if (-not $SkipPlots) {
        & python tools/plot_results.py results/benchmark.csv --output-dir results
        if ($LASTEXITCODE -ne 0) { throw 'Plot generation failed' }
        & python tools/plot_case_comparisons.py results/benchmark.csv --output-dir performance_charts
        if ($LASTEXITCODE -ne 0) { throw 'Case plot generation failed' }
    }
    Write-Output 'Reproduction complete. Results are in results/.'
} finally { Pop-Location }
