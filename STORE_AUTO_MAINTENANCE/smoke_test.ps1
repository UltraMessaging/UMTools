<#
smoke_test.ps1 -- validate a published store_auto_maintenance release on Windows.

Default mode (-Quick): downloads the latest published tree, checks the
prebuilt .exe runs, rebuilds from source, and runs `-V` validate-only.
~30 seconds. No license or umestored required.

-Full: also runs an end-to-end maintenance cycle. Starts umestored.exe,
sends 100 messages, waits for the scheduled cycle to prune, sends 5 more
messages to confirm the restarted store accepts them. ~90 seconds.
Requires umestored.exe and a valid LBM license.

-Local: skip download; test the tree in the current directory.

Env vars (override prompt / auto-detect):
  $env:UMQ_DIR              UMQ install root (e.g. C:\UMQ\Win2k-x86_64)
  $env:LBM_LICENSE_FILENAME License file (-Full only)
  $env:GIT_REF              Branch/tag to test (default: master)
  $env:STAY                 If set, leave the sandbox in place after the run

Exit codes:
  0 - all tests passed
  1 - test failure
  2 - prerequisite missing
#>

[CmdletBinding()]
param(
  [switch]$Full,
  [switch]$Quick,
  [switch]$Local,
  [switch]$Help
)

$ErrorActionPreference = 'Continue'

if ($Help) {
  Get-Help $PSCommandPath -Detailed
  exit 0
}

if (-not $Full) { $Quick = $true }

$RepoOwner = "UltraMessaging"
$RepoName  = "UMTools"
$Subdir    = "STORE_AUTO_MAINTENANCE"
$DefaultRef = if ($env:GIT_REF) { $env:GIT_REF } else { "master" }
$TestTopic = "TOPIC_TEST"
$TestPort  = 14444
$IntervalSecs = 60

$script:FailCount = 0

function Step($msg)  { Write-Host "`n==> $msg" -ForegroundColor Green }
function Pass($msg)  { Write-Host "  PASS  $msg" -ForegroundColor Green }
function Fail($msg)  { Write-Host "  FAIL  $msg" -ForegroundColor Red; $script:FailCount++ }
function Warn($msg)  { Write-Host "  WARN  $msg" -ForegroundColor Yellow }
function Info($msg)  { Write-Host "  info  $msg" -ForegroundColor DarkGray }
function Die($msg, $code=1) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit $code }

# ---------- sandbox ----------
$Sandbox = Join-Path $env:TEMP ("store_auto_maint_smoke_" + [guid]::NewGuid().ToString().Substring(0,8))
New-Item -ItemType Directory -Path $Sandbox | Out-Null

$script:Cleanup = {
  if ($script:ParentProc -and -not $script:ParentProc.HasExited) {
    Stop-Process -Id $script:ParentProc.Id -Force -ErrorAction SilentlyContinue
  }
  if (-not $env:STAY -and (Test-Path $Sandbox)) {
    Remove-Item -LiteralPath $Sandbox -Recurse -Force -ErrorAction SilentlyContinue
  } elseif ($env:STAY) {
    Info "sandbox preserved at $Sandbox (STAY env was set)"
  }
}
trap { & $script:Cleanup; break }

$mode = if ($Full) { "full" } else { "quick" }
Step "Smoke test: $mode mode"
Info "sandbox: $Sandbox"

# ---------- step 1: obtain tree ----------
Step "1) Obtain the release tree"

if ($Local) {
  if ((Test-Path "store_auto_maint.c") -and (Test-Path "umesnaprepo.c")) {
    $Tree = (Get-Location).Path
    Info "using local tree: $Tree"
  } else {
    Die "-Local but cwd doesn't look like store_auto_maintenance/ (no store_auto_maint.c)"
  }
} else {
  $Tree = Join-Path $Sandbox "release"
  New-Item -ItemType Directory -Path $Tree | Out-Null
  $RepoLocal = Join-Path $Sandbox "repo"

  $hasGh   = $null -ne (Get-Command gh -ErrorAction SilentlyContinue)
  $hasGit  = $null -ne (Get-Command git -ErrorAction SilentlyContinue)
  $ghAuth  = $false
  if ($hasGh) {
    & gh auth status 2>&1 | Out-Null
    $ghAuth = ($LASTEXITCODE -eq 0)
  }

  # Helper: copy contents of source dir into existing $Tree
  function Copy-TreeContents([string]$srcDir, [string]$dstDir) {
    if (-not (Test-Path -LiteralPath $srcDir)) { Die "source path not found: $srcDir" }
    Get-ChildItem -LiteralPath $srcDir -Force | ForEach-Object {
      Copy-Item -LiteralPath $_.FullName -Destination $dstDir -Recurse -Force
    }
  }

  if ($hasGh -and $ghAuth) {
    Info "downloading via gh from $RepoOwner/$RepoName@$DefaultRef"
    Push-Location $Sandbox
    & gh repo clone "$RepoOwner/$RepoName" repo -- --depth 1 --branch $DefaultRef 2>$null
    $rc = $LASTEXITCODE
    Pop-Location
    if ($rc -ne 0) { Die "gh repo clone failed" }
    Copy-TreeContents (Join-Path $RepoLocal $Subdir) $Tree
  } elseif ($hasGit) {
    Info "downloading via git clone"
    Push-Location $Sandbox
    & git clone --depth 1 --branch $DefaultRef "https://github.com/$RepoOwner/$RepoName.git" repo 2>$null
    $rc = $LASTEXITCODE
    Pop-Location
    if ($rc -ne 0) { Die "git clone failed" }
    Copy-TreeContents (Join-Path $RepoLocal $Subdir) $Tree
  } else {
    Info "downloading tarball via Invoke-WebRequest"
    $url = "https://github.com/$RepoOwner/$RepoName/archive/refs/heads/$DefaultRef.zip"
    $zip = Join-Path $Sandbox "repo.zip"
    Invoke-WebRequest -Uri $url -OutFile $zip
    Expand-Archive -LiteralPath $zip -DestinationPath $Sandbox -Force
    $extractedRoot = Join-Path $Sandbox "$RepoName-$DefaultRef"
    Copy-TreeContents (Join-Path $extractedRoot $Subdir) $Tree
  }
  Pass "downloaded $Tree"
}

# ---------- step 2: file inventory ----------
Step "2) File inventory"
$expected = @(
  "store_auto_maint.c","xml_config_parser.c","umesnaprepo.c",
  "maintain_store.bat","bld_umesnaprepo.bat","Makefile","README.md",
  "bin\Win2k-x86_64\store_auto_maint.exe","bin\Win2k-x86_64\umesnaprepo.exe"
)
foreach ($f in $expected) {
  if (Test-Path -LiteralPath (Join-Path $Tree $f)) {
    Pass $f
  } else {
    Fail "$f missing"
  }
}

# ---------- step 3: line endings on .bat ----------
Step "3) Line endings on .bat files (CRLF required for Windows cmd)"
foreach ($f in "maintain_store.bat","bld_umesnaprepo.bat") {
  $p = Join-Path $Tree $f
  if (Test-Path -LiteralPath $p) {
    $bytes = [System.IO.File]::ReadAllBytes($p)
    $cr = ($bytes | Where-Object { $_ -eq 0x0D }).Count
    $lf = ($bytes | Where-Object { $_ -eq 0x0A }).Count
    if ($cr -eq $lf -and $cr -gt 0) {
      Pass "$f is CRLF (CR=$cr LF=$lf)"
    } else {
      Fail "$f has wrong line endings (CR=$cr LF=$lf)"
    }
  }
}

# ---------- step 4: locate UMQ ----------
Step "4) Locate UMQ install"
$UmqDir = $env:UMQ_DIR
if (-not $UmqDir) {
  # Priority: official Informatica installer location, then locally-built UMQ,
  # then anywhere else the user might have it. Each candidate must have the
  # canonical Win2k-x86_64\{bin,lib} layout.
  $patterns = @(
    "C:\Program Files\Informatica\UMQ_*\Win2k-x86_64",
    "C:\Program Files\Informatica\UMP_*\Win2k-x86_64",
    "C:\CLAUDE\SF_DEV_MAIN\29West\lbm\pp\UMQ_*\Win2k-x86_64",
    "C:\UMQ\Win2k-x86_64",
    "C:\UMP\Win2k-x86_64",
    "C:\Users\$env:USERNAME\UMQ_*\Win2k-x86_64",
    "C:\Users\$env:USERNAME\UMP_*\Win2k-x86_64"
  )
  foreach ($pat in $patterns) {
    $found = Get-Item -Path $pat -ErrorAction SilentlyContinue | Where-Object {
      (Test-Path "$($_.FullName)\bin") -and (Test-Path "$($_.FullName)\lib")
    } | Select-Object -First 1
    if ($found) {
      $UmqDir = $found.FullName
      Info "auto-detected: $UmqDir"
      break
    }
  }
}
if (-not $UmqDir) {
  if ([Environment]::UserInteractive -and -not [Console]::IsInputRedirected) {
    $UmqDir = Read-Host "Enter UMQ_DIR (path to UMQ Win2k-x86_64; e.g. C:\UMQ_6.17\Win2k-x86_64)"
  } else {
    Die "UMQ_DIR not set and not auto-detected; set `$env:UMQ_DIR to your UMQ install" 2
  }
}
if (-not (Test-Path "$UmqDir\bin")) { Die "no bin/ in $UmqDir" 2 }
if (-not (Test-Path "$UmqDir\lib")) { Die "no lib/ in $UmqDir" 2 }
Pass "UMQ_DIR=$UmqDir"
$env:PATH = "$UmqDir\bin;$env:PATH"

# ---------- step 5: prebuilt -h works ----------
Step "5) Prebuilt binaries run"
$samExe = Join-Path $Tree "bin\Win2k-x86_64\store_auto_maint.exe"
$snapExe = Join-Path $Tree "bin\Win2k-x86_64\umesnaprepo.exe"

$out = & $samExe -h 2>&1 | Out-String
if ($out -match "store_auto_maint v") {
  Pass "store_auto_maint.exe -h"
} else {
  Fail "store_auto_maint.exe -h did not print expected banner"
}

# umesnaprepo with no args exits 1 (usage error). Don't treat that as failure.
$out = & $snapExe 2>&1 | Out-String
if ($out -match "Available options:") {
  Pass "umesnaprepo.exe prints usage"
} else {
  Fail "umesnaprepo.exe did not run; first lines:"
  ($out -split "`n") | Select-Object -First 3 | ForEach-Object { "        $_" }
}

# ---------- step 6: -V validate ----------
Step "6) -V validate against test_config.xml"
Push-Location $Tree
$out = & $samExe -x test_config.xml -V -r $snapExe 2>&1 | Out-String
Pop-Location
if ($out -match "Configuration is valid\.") {
  Pass "configuration valid"
} else {
  Fail "validation failed:"
  ($out -split "`n") | Select-Object -First 10 | ForEach-Object { "        $_" }
}

# ---------- if -Quick, stop here ----------
if (-not $Full) {
  Step "Summary"
  if ($script:FailCount -eq 0) {
    Write-Host "OK: all quick checks passed" -ForegroundColor Green
    & $script:Cleanup
    exit 0
  } else {
    Write-Host "FAIL: $($script:FailCount) check(s) failed" -ForegroundColor Red
    & $script:Cleanup
    exit 1
  }
}

# ---------- -Full E2E pipeline ----------

# ---------- step 7: license ----------
Step "7) Locate LBM license (-Full only)"
if (($env:LBM_LICENSE_FILENAME) -and (Test-Path -LiteralPath $env:LBM_LICENSE_FILENAME)) {
  Info "license from env: $($env:LBM_LICENSE_FILENAME)"
} else {
  $licCandidates = @(
    "$env:USERPROFILE\Documents\lic.6.0.txt",
    "$env:USERPROFILE\Downloads\lic.6.0.txt",
    "$env:USERPROFILE\lic.6.0.txt"
  )
  foreach ($c in $licCandidates) {
    if (Test-Path -LiteralPath $c) {
      $env:LBM_LICENSE_FILENAME = $c
      Info "auto-detected: $c"
      break
    }
  }
  if (-not $env:LBM_LICENSE_FILENAME) {
    if ([Environment]::UserInteractive -and -not [Console]::IsInputRedirected) {
      $env:LBM_LICENSE_FILENAME = Read-Host "Enter LBM license file path (e.g. C:\Users\you\Documents\lic.6.0.txt)"
    } else {
      Die "LBM_LICENSE_FILENAME not set and not auto-detected; -Full requires a license" 2
    }
  }
}
if (-not (Test-Path -LiteralPath $env:LBM_LICENSE_FILENAME)) {
  Die "license file not found: $($env:LBM_LICENSE_FILENAME)" 2
}
Pass "LBM_LICENSE_FILENAME=$($env:LBM_LICENSE_FILENAME)"

# ---------- step 8: pick interface ----------
Step "8) Pick interface for default_interface"
$winIp = (Get-NetIPAddress -AddressFamily IPv4 -PrefixLength 24 -ErrorAction SilentlyContinue |
          Where-Object { $_.IPAddress -notlike "169.*" -and $_.IPAddress -ne "127.0.0.1" } |
          Select-Object -First 1).IPAddress
if (-not $winIp) { Die "no usable IPv4 interface found" }
$octets = $winIp -split '\.'
$subnet = "$($octets[0]).$($octets[1]).$($octets[2]).0/24"
Pass "interface subnet: $subnet"

# ---------- step 9: build sandbox ----------
Step "9) Build run sandbox"
$Work = Join-Path $Sandbox "run"
New-Item -ItemType Directory -Path "$Work\cache","$Work\state","$Work\UMDIR" | Out-Null
Copy-Item -LiteralPath (Join-Path $Tree "maintain_store.bat") -Destination $Work
Copy-Item -LiteralPath (Join-Path $Tree "bin\Win2k-x86_64\store_auto_maint.exe") -Destination $Work
Copy-Item -LiteralPath (Join-Path $Tree "bin\Win2k-x86_64\umesnaprepo.exe") -Destination $Work

@"
context resolver_multicast_address 226.16.16.16
context default_interface $subnet
source ume_store_name store0
source ume_store_behavior qc
source ume_session_id 12345
source ume_repository_size_threshold 1024
source ume_repository_size_limit 10485760
source ume_message_stability_lifetime 5000
source ume_repository_ack_on_reception 1
source ume_write_delay 100
source ume_flight_size 50
source ume_state_lifetime 3600000
"@ | Set-Content -LiteralPath "$Work\app.cfg" -Encoding ASCII

@"
<?xml version=`"1.0`"?>
<ume-store version=`"1.3`">
  <daemon>
    <log>umestored.log</log>
    <pidfile>umestored.pid</pidfile>
    <lbm-config>app.cfg</lbm-config>
    <web-monitor>*:15405</web-monitor>
  </daemon>
  <stores>
    <store name=`"store0`" port=`"$TestPort`" interface=`"0.0.0.0`">
      <ume-attributes>
        <option type=`"store`" name=`"disk-cache-directory`" value=`"./cache`"/>
        <option type=`"store`" name=`"disk-state-directory`" value=`"./state`"/>
        <option type=`"store`" name=`"context-name`" value=`"store0`"/>
      </ume-attributes>
      <topics>
        <topic pattern=`".*`" type=`"PCRE`">
          <ume-attributes>
            <option type=`"store`" name=`"repository-type`" value=`"disk`"/>
            <option type=`"store`" name=`"repository-size-threshold`" value=`"2048`"/>
            <option type=`"store`" name=`"repository-size-limit`" value=`"10485760`"/>
            <option type=`"store`" name=`"repository-disk-file-size-limit`" value=`"1073741824`"/>
            <option type=`"store`" name=`"repository-allow-ack-on-reception`" value=`"1`"/>
            <option type=`"store`" name=`"source-state-lifetime`" value=`"3600000`"/>
          </ume-attributes>
        </topic>
      </topics>
    </store>
  </stores>
</ume-store>
"@ | Set-Content -LiteralPath "$Work\store.xml" -Encoding ASCII
Pass "sandbox prepared at $Work"

# ---------- step 10a: pre-flight orphan kill ----------
Step "10) Pre-flight: clear any orphan umestored on port $TestPort"
$onPort = Get-NetTCPConnection -State Listen -LocalPort $TestPort -ErrorAction SilentlyContinue
if ($onPort) {
  Warn "port $TestPort already in use; killing umestored.exe processes"
  Get-Process -Name umestored -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep 1
}
Get-Process -Name umestored,store_auto_maint -ErrorAction SilentlyContinue |
  Where-Object { $_.Path -like "*store_auto_maint_smoke_*" } |
  Stop-Process -Force -ErrorAction SilentlyContinue
Pass "port $TestPort clear"

# ---------- step 11: launch store_auto_maint.exe ----------
Step "11) Launch store_auto_maint.exe -f interval:$IntervalSecs"
$pParams = @{
  FilePath = "$Work\store_auto_maint.exe"
  ArgumentList = @("-x","store.xml","-y","-f","interval:$IntervalSecs",
                   "-r","$Work\umesnaprepo.exe","-e","$UmqDir\bin\umestored.exe",
                   "-R",".","-L","store_maint.log")
  WorkingDirectory = $Work
  PassThru = $true
  RedirectStandardOutput = "$Work\parent.stdout"
  RedirectStandardError = "$Work\parent.stderr"
  NoNewWindow = $true
}
$script:ParentProc = Start-Process @pParams
Start-Sleep 5
if ($script:ParentProc.HasExited) {
  Fail "store_auto_maint.exe died at startup:"
  Get-Content "$Work\parent.stderr","$Work\parent.stdout" -ErrorAction SilentlyContinue | Select-Object -First 20 | ForEach-Object { "        $_" }
  & $script:Cleanup
  exit 1
}
Pass "store_auto_maint.exe PID=$($script:ParentProc.Id) alive"

# ---------- step 12: send 100 messages ----------
Step "12) umesrc.exe -M 100"
$sParams = @{
  FilePath = "$UmqDir\bin\umesrc.exe"
  ArgumentList = @("-c","app.cfg","-t","store0","-M","100","-P","50","-l","256","-V","-L","5",$TestTopic)
  WorkingDirectory = $Work
  PassThru = $true
  RedirectStandardOutput = "$Work\umesrc1.out"
  RedirectStandardError = "$Work\umesrc1.err"
  NoNewWindow = $true
}
$srcProc = Start-Process @sParams
$srcProc.WaitForExit(45000) | Out-Null
Start-Sleep 2  # let umestored finish flushing

# Decide pass/fail by observable outcome (cache file written)
$cacheFile = Get-ChildItem -LiteralPath "$Work\cache" -Filter "*-cache" -ErrorAction SilentlyContinue | Select-Object -First 1
$preSize = if ($cacheFile) { $cacheFile.Length } else { 0 }
if ($cacheFile -and $preSize -gt 1000) {
  Pass "cache populated: $($cacheFile.Name) ($preSize bytes)"
} else {
  $o = Get-Content "$Work\umesrc1.out" -ErrorAction SilentlyContinue
  if ($o | Select-String -Pattern "license" -SimpleMatch) {
    Fail "license issue in umesrc1.out"
    $o | Select-String -Pattern "license" -SimpleMatch | ForEach-Object { "        $_" }
  } else {
    Fail "no cache populated (size=$preSize) -- umesrc could not reach the store"
    $o | Select-Object -Last 3 | ForEach-Object { "        $_" }
  }
}

# ---------- step 12: poll for cycle ----------
Step "13) Wait for scheduled cycle to complete"
$cmp = 0
for ($i=1; $i -le 18; $i++) {
  Start-Sleep 5
  if (Test-Path "$Work\store_maint.log") {
    $cmp = (Get-Content "$Work\store_maint.log" | Select-String "STATS.*completed=1").Count
  }
  Write-Host ("  +{0}s: completed={1}" -f ($i*5), $cmp)
  if ($cmp -ge 1) { Pass "cycle completed within $($i*5)s"; break }
}
if ($cmp -lt 1) {
  Fail "cycle did not complete in 90s"
  Get-Content "$Work\store_maint.log" -ErrorAction SilentlyContinue | Select-Object -Last 20 | ForEach-Object { "        $_" }
}

# ---------- step 13: verify prune ----------
Step "14) Verify prune"
$log = Get-Content "$Work\store_maint.log" -ErrorAction SilentlyContinue
if ($log | Select-String "PASS: Highest message sequence numbers match") {
  Pass "maintain_store.bat reported PASS"
} else {
  Fail "maintain_store.bat did not report PASS"
}
if ((Test-Path "$Work\UMDIR\store0") -and (Get-ChildItem -LiteralPath "$Work\UMDIR\store0" -ErrorAction SilentlyContinue)) {
  Pass "backup exists in UMDIR/store0"
} else {
  Fail "no backup in UMDIR/store0"
}
$cacheAfter = Get-ChildItem -LiteralPath "$Work\cache" -Filter "*-cache" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($cacheAfter -and $preSize -gt 0) {
  if ($cacheAfter.Length -lt $preSize) {
    Pass "cache shrunk: $preSize -> $($cacheAfter.Length) bytes"
  } else {
    Fail "cache did not shrink ($preSize -> $($cacheAfter.Length))"
  }
}

# ---------- step 14: send 5 more ----------
Step "15) umesrc.exe -M 5 (resume after restart)"
$s2 = @{
  FilePath = "$UmqDir\bin\umesrc.exe"
  ArgumentList = @("-c","app.cfg","-t","store0","-M","5","-P","100","-l","256","-V","-L","3",$TestTopic)
  WorkingDirectory = $Work
  PassThru = $true
  RedirectStandardOutput = "$Work\umesrc2.out"
  RedirectStandardError = "$Work\umesrc2.err"
  NoNewWindow = $true
}
$src2Proc = Start-Process @s2
$src2Proc.WaitForExit(20000) | Out-Null
$o2 = Get-Content "$Work\umesrc2.out" -ErrorAction SilentlyContinue
if ($o2 | Select-String "OLD\[SQN") {
  Pass "store recognized source (OLD[SQN ...] flag set on registration)"
  $o2 | Select-String "OLD\[" | Select-Object -First 1 | ForEach-Object { "        $_" }
} else {
  Warn "no OLD[SQN] flag -- store treated as new source (state-lifetime may have expired)"
}
if ($o2 | Select-String "SQN 100") {
  Pass "registration continued at SQN 100"
} elseif ($o2 | Select-String "Sent 5 messages") {
  Warn "5 messages sent but SQN did not continue from 99 -- store may have started fresh"
} else {
  Fail "umesrc-2 did not complete"
  $o2 | Select-Object -Last 3 | ForEach-Object { "        $_" }
}

# ---------- step 15: stop ----------
Step "16) Clean shutdown"
Stop-Process -Id $script:ParentProc.Id -Force -ErrorAction SilentlyContinue
Start-Sleep 3
Pass "parent stopped"
$script:ParentProc = $null

# ---------- summary ----------
Step "Summary"
Write-Host "  Final stats from store_maint.log:"
Get-Content "$Work\store_maint.log" -ErrorAction SilentlyContinue | Select-String "STATS" | ForEach-Object { "    $_" }

if ($script:FailCount -eq 0) {
  Write-Host "`nOK: all full-cycle checks passed" -ForegroundColor Green
  & $script:Cleanup
  exit 0
} else {
  Write-Host "`nFAIL: $($script:FailCount) check(s) failed" -ForegroundColor Red
  & $script:Cleanup
  exit 1
}
