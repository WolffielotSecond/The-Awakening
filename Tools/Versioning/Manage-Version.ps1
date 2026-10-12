[CmdletBinding()]
param(
    [ValidateSet('Show', 'Set', 'Prepare', 'Package')][string]$Action = 'Show',
    [string]$Version,
    [string]$ProjectRoot = (Join-Path $PSScriptRoot '../..'),
    [string]$EngineRoot,
    [ValidateSet('Development', 'Shipping')][string]$Configuration = 'Development',
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$configPath = Join-Path $ProjectRoot 'Config/DefaultGame.ini'
$projectPath = Join-Path $ProjectRoot 'The_Awakening.uproject'
$generalSection = '/Script/EngineSettings.GeneralProjectSettings'
$buildSection = 'TheAwakening.Build'

# Full SemVer syntax, including validation of numeric prerelease identifiers.
function Assert-Version([string]$Value) {
    $number = '(0|[1-9][0-9]*)'
    $identifier = '(?:0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*)'
    $pattern = "\A$number\.$number\.$number(?:-$identifier(?:\.$identifier)*)?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?\z"
    if ($Value -cnotmatch $pattern) { throw "Invalid version '$Value'. Example: 0.1.0-alpha.1" }
    # Build metadata is recorded separately in BuildId.
    if ($Value.Contains('+')) { throw 'Keep build metadata in BuildId; ProjectVersion must not contain +metadata.' }
}

function Read-IniValue([string]$Text, [string]$Section, [string]$Key) {
    $inside = $false
    $values = @()
    foreach ($line in ($Text -split '\r?\n')) {
        if ($line -match '^\s*\[([^\]]+)\]\s*$') { $inside = $Matches[1] -eq $Section; continue }
        if ($inside -and $line -match ('^\s*' + [regex]::Escape($Key) + '\s*=(.*)$')) {
            $values += $Matches[1].Trim()
        }
    }
    if ($values.Count -ne 1) { throw "Expected one $Key in [$Section], found $($values.Count)." }
    return $values[0]
}

function Set-IniValue([string]$Text, [string]$Section, [string]$Key, [string]$Value) {
    # Require existing, unambiguous keys so unrelated configuration is preserved.
    $null = Read-IniValue $Text $Section $Key
    $newline = if ($Text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $lines = $Text -split '\r?\n'
    $inside = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '^\s*\[([^\]]+)\]\s*$') { $inside = $Matches[1] -eq $Section; continue }
        if ($inside -and $lines[$i] -match ('^\s*' + [regex]::Escape($Key) + '\s*=')) {
            $lines[$i] = "$Key=$Value"
        }
    }
    return $lines -join $newline
}

function Write-Utf8([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

if (-not (Test-Path -LiteralPath $projectPath)) { throw "Project not found: $projectPath" }
if ($Action -eq 'Set' -and [string]::IsNullOrEmpty($Version)) { throw 'Set requires -Version.' }
if ($Action -ne 'Set' -and $PSBoundParameters.ContainsKey('Version')) { throw '-Version is supported only with -Action Set.' }

$buildLock = $null
$stateDir = Join-Path $ProjectRoot 'Saved/Versioning'
if ($Action -ne 'Show') {
    $null = New-Item -ItemType Directory -Path $stateDir -Force
    try { $buildLock = [IO.File]::Open((Join-Path $stateDir 'build.lock'), 'OpenOrCreate', 'ReadWrite', 'None') }
    catch { throw 'Another versioning operation is running. Wait for it to finish.' }
}

try {
    $originalBytes = [IO.File]::ReadAllBytes($configPath)
    $configText = [IO.File]::ReadAllText($configPath)
    $currentVersion = Read-IniValue $configText $generalSection 'ProjectVersion'
    Assert-Version $currentVersion
    $buildId = Read-IniValue $configText $buildSection 'BuildId'
    $saveVersion = Read-IniValue $configText $buildSection 'SaveFormatVersion'
    if ($saveVersion -notmatch '^[1-9][0-9]*$' -or [long]$saveVersion -gt [int]::MaxValue) { throw 'SaveFormatVersion must be a positive int32.' }

    if ($Action -eq 'Show') {
        [pscustomobject]@{ ProjectVersion = $currentVersion; BuildId = $buildId; SaveFormatVersion = [int]$saveVersion }
        return
    }
    if ($Action -eq 'Set') {
        Assert-Version $Version
        $updated = Set-IniValue $configText $generalSection 'ProjectVersion' $Version
        # A manually changed release version must not retain an old build identity.
        $updated = Set-IniValue $updated $buildSection 'BuildId' 'local'
        Write-Utf8 $configPath $updated
        Write-Output "ProjectVersion=$Version; BuildId=local"
        return
    }

    if ($Action -eq 'Package') {
        if ([string]::IsNullOrEmpty($EngineRoot)) { throw 'Package requires -EngineRoot (the UE 5.6 installation directory).' }
        $uatPath = Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat'
        if (-not (Test-Path -LiteralPath $uatPath)) { throw "RunUAT not found: $uatPath" }
        if ([string]::IsNullOrEmpty($OutputRoot)) { $OutputRoot = Join-Path $ProjectRoot 'Saved/Releases' }
        $OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
    }

    $utcNow = [DateTime]::UtcNow
    $date = $utcNow.ToString('yyyyMMdd')
    $counterPath = Join-Path $stateDir 'counter.json'
    $sequence = 1
    if (Test-Path -LiteralPath $counterPath) {
        $counter = Get-Content -LiteralPath $counterPath -Raw | ConvertFrom-Json
        if ($counter.Date -eq $date) { $sequence = [int]$counter.Sequence + 1 }
    }

    $commit = 'nogit'
    $dirty = $false
    if (Get-Command git -ErrorAction SilentlyContinue) {
        $gitRoot = & git -C $ProjectRoot rev-parse --show-toplevel 2>$null
        # A copied project inside an unrelated repository is not a Git checkout of this project.
        if ($LASTEXITCODE -eq 0 -and [IO.Path]::GetFullPath($gitRoot.Trim()) -eq $ProjectRoot) {
            $commitOutput = & git -C $ProjectRoot rev-parse --short=8 HEAD 2>$null
            if ($LASTEXITCODE -ne 0) { throw 'Git checkout has no readable HEAD. Commit the project first.' }
            $commit = $commitOutput.Trim()
            $status = & git -C $ProjectRoot status --porcelain --untracked-files=normal
            if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect Git working tree.' }
            $dirty = -not [string]::IsNullOrEmpty(($status -join "`n"))
        }
    }
    $buildId = '{0}.{1:D3}-{2}' -f $date, $sequence, $commit
    if ($dirty) { $buildId += '-dirty' }
    $manifest = [ordered]@{
        ProjectVersion = $currentVersion
        BuildId = $buildId
        SaveFormatVersion = [int]$saveVersion
        BuiltAtUtc = $utcNow.ToString('o')
        GitCommit = $commit
        Dirty = $dirty
        Configuration = $Configuration
        Status = 'Prepared'
    }
    $releaseDir = $null
    if ($Action -eq 'Package') {
        $releaseDir = Join-Path $OutputRoot "The_Awakening-$currentVersion-$buildId-Win64-$Configuration"
        if (Test-Path -LiteralPath $releaseDir) { throw "Output already exists: $releaseDir" }
        $null = New-Item -ItemType Directory -Path $releaseDir
    }
    # Reserve a unique local sequence even when a later package fails.
    Write-Utf8 $counterPath ((@{ Date = $date; Sequence = $sequence } | ConvertTo-Json) + "`n")
    $stamped = Set-IniValue $configText $buildSection 'BuildId' $buildId
    $manifestPath = Join-Path $stateDir 'build-info.json'
    if ($Action -eq 'Package') {
        $manifest.Status = 'Packaging'
        try {
            Write-Utf8 $configPath $stamped
            Write-Utf8 $manifestPath (($manifest | ConvertTo-Json) + "`n")
            & $uatPath BuildCookRun "-project=$projectPath" -noP4 -platform=Win64 "-clientconfig=$Configuration" -build -cook -stage -pak -iostore -archive "-archivedirectory=$releaseDir" -utf8output
            if ($LASTEXITCODE -ne 0) { throw "Unreal packaging failed (exit $LASTEXITCODE)." }
            $manifest.Status = 'Succeeded'
        }
        catch {
            $manifest.Status = 'Failed'
            throw
        }
        finally {
            [IO.File]::WriteAllBytes($configPath, $originalBytes)
            Write-Utf8 $manifestPath (($manifest | ConvertTo-Json) + "`n")
            Write-Utf8 (Join-Path $releaseDir 'build-info.json') (($manifest | ConvertTo-Json) + "`n")
            if (Test-Path -LiteralPath (Join-Path $ProjectRoot 'CHANGELOG.md')) {
                Copy-Item -LiteralPath (Join-Path $ProjectRoot 'CHANGELOG.md') -Destination $releaseDir
            }
        }
    }
    else {
        Write-Utf8 $configPath $stamped
        Write-Utf8 $manifestPath (($manifest | ConvertTo-Json) + "`n")
    }
    Write-Output "Version: $currentVersion; Build: $buildId; manifest: $manifestPath"
    if ($releaseDir) { Write-Output "Package: $releaseDir" }
}
finally { if ($buildLock) { $buildLock.Dispose() } }
