param(
    [string]$BaseUrl = "http://127.0.0.1:29011"
)

$ErrorActionPreference = "Stop"

# 1. Alap split health mezoellenorzes
$health = Invoke-RestMethod -Method Get -Uri "$BaseUrl/health" -TimeoutSec 60
if ($health.split_mode -ne $true) {
    throw "A split diagnostics smoke split_mode=true allapotot var."
}

$requiredFields = @(
    "split_profile",
    "stage_1_state",
    "stage_2_state",
    "handoff_state",
    "last_failure_scope",
    "fallback_detected",
    "memory_summary",
    "last_failure_summary"
)

foreach ($field in $requiredFields) {
    if (-not $health.PSObject.Properties.Name.Contains($field)) {
        throw "A split diagnostics smoke szerint hianyzik a health mezok kozul: $field"
    }
}

# 2. Stage allapot konzisztencia: ha ready, mindket stage ready kell legyen
if ($health.ready -eq $true) {
    if ($health.stage_1_state -ne "ready") {
        throw "A split diagnostics smoke: ready allapotban stage_1_state=$($health.stage_1_state), vart: ready"
    }
    if ($health.stage_2_state -ne "ready") {
        throw "A split diagnostics smoke: ready allapotban stage_2_state=$($health.stage_2_state), vart: ready"
    }
    if ($health.fallback_detected -eq $true) {
        throw "A split diagnostics smoke: ready allapotban fallback_detected nem lehet true."
    }
}

# 3. Memory summary mezoellenorzes
if ($null -eq $health.memory_summary) {
    throw "A split diagnostics smoke: memory_summary nem lehet null."
}
$memorySummary = $health.memory_summary
if (-not $memorySummary.PSObject.Properties.Name.Contains("stage_1")) {
    throw "A split diagnostics smoke: memory_summary.stage_1 hianyzik."
}
if (-not $memorySummary.PSObject.Properties.Name.Contains("stage_2")) {
    throw "A split diagnostics smoke: memory_summary.stage_2 hianyzik."
}
if (-not $memorySummary.PSObject.Properties.Name.Contains("system")) {
    throw "A split diagnostics smoke: memory_summary.system hianyzik."
}

# 4. Failure scope konzisztencia: ha ready es nincs hiba, last_failure_scope ures/none
if ($health.ready -eq $true) {
    $scope = $health.last_failure_scope
    if ($scope -ne "" -and $scope -ne "none" -and $null -ne $scope) {
        throw "A split diagnostics smoke: ready allapotban last_failure_scope='$scope', vart: ures/none"
    }
    if (-not [string]::IsNullOrEmpty($health.last_failure_summary)) {
        throw "A split diagnostics smoke: ready allapotban last_failure_summary ures kell legyen."
    }
}

# 5. Split profile mezo letezes
if ([string]::IsNullOrEmpty($health.split_profile)) {
    throw "A split diagnostics smoke: split_profile mezo ures vagy hianyzik."
}

# 6. A health valasz ismételt lekérdezés stabilitási ellenőrzés (2 lekérdezés konzisztenciája)
$health2 = Invoke-RestMethod -Method Get -Uri "$BaseUrl/health" -TimeoutSec 30
if ($health2.split_mode -ne $health.split_mode) {
    throw "A split diagnostics smoke: ket egymast koveto health lekerdezesnel split_mode eltert."
}
if ($health2.stage_1_state -ne $health.stage_1_state) {
    throw "A split diagnostics smoke: ket egymast koveto health lekerdezesnel stage_1_state eltert."
}
if ($health2.stage_2_state -ne $health.stage_2_state) {
    throw "A split diagnostics smoke: ket egymast koveto health lekerdezesnel stage_2_state eltert."
}

Write-Host "Manual split diagnostics smoke PASS."
