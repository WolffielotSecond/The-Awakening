[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$scriptPath = Join-Path $PSScriptRoot 'Manage-Version.ps1'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('TA-VersionTests-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path (Join-Path $fixture 'Config') -Force
$configPath = Join-Path $fixture 'Config/DefaultGame.ini'
$initial = "[/Script/EngineSettings.GeneralProjectSettings]`r`nProjectVersion=0.1.0-alpha.1`r`nProjectName=Keep Me`r`n`r`n[TheAwakening.Build]`r`nBuildId=local`r`nSaveFormatVersion=2`r`n`r`n[Unrelated]`r`nBuildId=do-not-change`r`n"
[IO.File]::WriteAllText($configPath, $initial, [Text.UTF8Encoding]::new($true))
[IO.File]::WriteAllText((Join-Path $fixture 'The_Awakening.uproject'), '{}')
[IO.File]::WriteAllText((Join-Path $fixture 'CHANGELOG.md'), '# Fixture changelog')
$passed = 0
function Assert-That([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "FAIL: $Message" }
    $script:passed++
}
function Invoke-Version([string]$Action, [hashtable]$Extra = @{}) {
    & $scriptPath -Action $Action -ProjectRoot $fixture @Extra
}

try {
    $shown = Invoke-Version 'Show'
    Assert-That ($shown.ProjectVersion -eq '0.1.0-alpha.1' -and $shown.SaveFormatVersion -eq 2) 'Read initial metadata'
    foreach ($value in @('1.2.3', '0.2.0-alpha.2', '1.0.0-rc.1', '1.2.3-test-2')) {
        $null = Invoke-Version 'Set' @{ Version = $value }
        Assert-That ((Invoke-Version 'Show').ProjectVersion -eq $value) "Accept $value"
    }
    foreach ($value in @('01.2.3', '1.2', '1.2.3-alpha.01', '1.2.3+build.1', "1.2.3`nInjected=true", "1.2.3`n", 'v1.2.3')) {
        $before = [Convert]::ToBase64String([IO.File]::ReadAllBytes($configPath))
        $rejected = $false
        try { $null = Invoke-Version 'Set' @{ Version = $value } } catch { $rejected = $true }
        Assert-That $rejected "Reject invalid version $value"
        Assert-That ($before -eq [Convert]::ToBase64String([IO.File]::ReadAllBytes($configPath))) 'Invalid version leaves config untouched'
    }
    $text = [IO.File]::ReadAllText($configPath)
    Assert-That ($text.Contains('ProjectName=Keep Me') -and $text.Contains('BuildId=do-not-change')) 'Preserve unrelated keys and sections'
    $null = Invoke-Version 'Prepare'
    $first = Get-Content -LiteralPath (Join-Path $fixture 'Saved/Versioning/build-info.json') -Raw | ConvertFrom-Json
    Assert-That ($first.BuildId -match '^\d{8}\.001-nogit$' -and $first.GitCommit -eq 'nogit' -and -not $first.Dirty) 'No Git checkout fallback'
    $null = Invoke-Version 'Prepare'
    $second = Get-Content -LiteralPath (Join-Path $fixture 'Saved/Versioning/build-info.json') -Raw | ConvertFrom-Json
    Assert-That ($second.BuildId -match '^\d{8}\.002-nogit$' -and $first.BuildId -ne $second.BuildId) 'Increment local sequence'
    Assert-That ((Invoke-Version 'Show').BuildId -eq $second.BuildId) 'Prepare stamps runtime config'

    # Minimal UAT stand-in verifies stamping, forwarding of spaced paths, and restoration.
    $engine = Join-Path $fixture 'Mock Engine'
    $uatDir = Join-Path $engine 'Engine/Build/BatchFiles'
    $null = New-Item -ItemType Directory -Path $uatDir -Force
    [IO.File]::WriteAllText((Join-Path $uatDir 'RunUAT.bat'), "@echo off`r`ntype `"%TA_VERSION_TEST_CONFIG%`" > `"%TA_VERSION_TEST_CAPTURE%`"`r`nexit /b %TA_VERSION_TEST_EXIT%`r`n")
    $oldConfig = $env:TA_VERSION_TEST_CONFIG
    $oldCapture = $env:TA_VERSION_TEST_CAPTURE
    $oldExit = $env:TA_VERSION_TEST_EXIT
    try {
        $env:TA_VERSION_TEST_CONFIG = $configPath
        $env:TA_VERSION_TEST_CAPTURE = Join-Path $fixture 'captured.ini'
        foreach ($exitCode in @('0', '7')) {
            # Include a BOM so restoration must preserve bytes, not just the parsed values.
            [IO.File]::WriteAllText($configPath, $initial, [Text.UTF8Encoding]::new($true))
            $before = [Convert]::ToBase64String([IO.File]::ReadAllBytes($configPath))
            $env:TA_VERSION_TEST_EXIT = $exitCode
            $failed = $false
            try { $null = Invoke-Version 'Package' @{ EngineRoot = $engine; OutputRoot = (Join-Path $fixture 'Release Output') } }
            catch { $failed = $true; if ($exitCode -eq '0') { throw } }
            Assert-That ($failed -eq ($exitCode -ne '0')) "Propagate packaging exit code $exitCode"
            Assert-That ($before -eq [Convert]::ToBase64String([IO.File]::ReadAllBytes($configPath))) "Restore exact config bytes after exit $exitCode"
            $info = Get-Content -LiteralPath (Join-Path $fixture 'Saved/Versioning/build-info.json') -Raw | ConvertFrom-Json
            $expected = if ($exitCode -eq '0') { 'Succeeded' } else { 'Failed' }
            Assert-That ($info.Status -eq $expected) "Record $expected status"
            Assert-That ([IO.File]::ReadAllText($env:TA_VERSION_TEST_CAPTURE).Contains("BuildId=$($info.BuildId)")) 'UAT sees stamped config'
            $release = Join-Path $fixture "Release Output/The_Awakening-$($info.ProjectVersion)-$($info.BuildId)-Win64-Development"
            Assert-That ((Test-Path -LiteralPath (Join-Path $release 'build-info.json')) -and (Test-Path -LiteralPath (Join-Path $release 'CHANGELOG.md'))) 'Archive metadata and changelog'
        }
    }
    finally {
        $env:TA_VERSION_TEST_CONFIG = $oldConfig
        $env:TA_VERSION_TEST_CAPTURE = $oldCapture
        $env:TA_VERSION_TEST_EXIT = $oldExit
    }
    Write-Output "PASS: $passed versioning checks."
}
finally {
    # Delete only this freshly created fixture, verified inside the system temp directory.
    $resolved = [IO.Path]::GetFullPath($fixture)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if ($resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and (Split-Path $resolved -Leaf) -match '^TA-VersionTests-[0-9a-f]{32}$') {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
