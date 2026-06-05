@echo off
REM maintain_store.bat - UME Store maintenance script (Windows)
REM
REM Performs the standard maintenance procedure on one or more store instances:
REM   1. Cleanup zero-message state and cache files
REM   2. Dump last message state (pre-prune snapshot)
REM   3. Prune messages older than current timestamp
REM   4. Dump last message state (post-prune snapshot)
REM   5. Verify "highest message sequence number" matches before/after
REM
REM Usage:
REM   maintain_store.bat -r UMESNAPREPO_PATH -d BACKUP_DIR -l LOG_FILE
REM                      -s STATE_DIR1 -c CACHE_DIR1 -n STORE_NAME1
REM                      [-s STATE_DIR2 -c CACHE_DIR2 -n STORE_NAME2 ...]
REM
REM Exit codes:
REM   0 = all stores maintained and verified successfully
REM   1 = verification failed for one or more stores
REM   2 = usage/argument error
REM   3 = umesnaprepo execution error
REM

setlocal enabledelayedexpansion

set UMESNAPREPO=
set "BACKUP_DIR=.\UMDIR"
set LOG_FILE=
set WORK_DIR=
set UTC_TIMESTAMPS=0
set STORE_COUNT=0

REM ---------------------------------------------------------------
REM Parse named options
REM
REM NOTE: "if cond ( ... & goto :label )" with goto inside a
REM parenthesized block is unreliable in cmd.exe â€” the goto can
REM fail to exit the block cleanly.  The safe pattern is a bare
REM "if cond goto :opt_x" with set/shift AFTER the label, outside
REM any conditional block.  Also: "if cond set ... & shift & goto"
REM WITHOUT parens is wrong too â€” the & makes shift and goto run
REM unconditionally regardless of the if result.
REM ---------------------------------------------------------------
:parse_loop
if "%~1"=="" goto :parse_done
if /i "%~1"=="-r" goto :opt_r
if /i "%~1"=="-d" goto :opt_d
if /i "%~1"=="-l" goto :opt_l
if /i "%~1"=="-w" goto :opt_w
if /i "%~1"=="-u" goto :opt_u
if /i "%~1"=="-s" goto :opt_s
if /i "%~1"=="-c" goto :opt_c
if /i "%~1"=="-n" goto :opt_n
echo ERROR: Unknown option: %~1
exit /b 2

:opt_r
set "UMESNAPREPO=%~2"
shift & shift & goto :parse_loop

:opt_d
set "BACKUP_DIR=%~2"
shift & shift & goto :parse_loop

:opt_l
set "LOG_FILE=%~2"
shift & shift & goto :parse_loop

:opt_w
set "WORK_DIR=%~2"
shift & shift & goto :parse_loop

:opt_u
set "UTC_TIMESTAMPS=1"
shift & goto :parse_loop

:opt_s
set /a STORE_COUNT+=1
set "STATE_DIR[!STORE_COUNT!]=%~2"
shift & shift & goto :parse_loop

:opt_c
set "CACHE_DIR[!STORE_COUNT!]=%~2"
shift & shift & goto :parse_loop

:opt_n
set "STORE_NAME[!STORE_COUNT!]=%~2"
shift & shift & goto :parse_loop

:parse_done

REM ---------------------------------------------------------------
REM Validate
REM ---------------------------------------------------------------
if "%UMESNAPREPO%"=="" (
    echo ERROR: umesnaprepo path ^(-r^) is required
    exit /b 2
)

if %STORE_COUNT%==0 (
    echo ERROR: At least one store ^(-s STATE_DIR -c CACHE_DIR -n NAME^) is required
    exit /b 2
)

REM On Windows, if only the basename is given (no .exe), also check basename.exe
if not exist "%UMESNAPREPO%" if not exist "%UMESNAPREPO%.exe" (
    call :log_msg "ERROR: umesnaprepo not found: %UMESNAPREPO%"
    exit /b 3
)

REM Set up work directory
if "%WORK_DIR%"=="" set "WORK_DIR=%TEMP%\store_maint_%RANDOM%"
if not exist "%WORK_DIR%" mkdir "%WORK_DIR%"

REM Create top-level backup directory
if not exist "%BACKUP_DIR%" mkdir "%BACKUP_DIR%"

set OVERALL_RESULT=0

call :log_msg "=== Store Maintenance Started ==="
call :log_msg "Stores to process: %STORE_COUNT%"
call :log_msg "Backup directory: %BACKUP_DIR%"

REM ---------------------------------------------------------------
REM Process each store via subroutine call.
REM
REM NOTE: Labels inside a for-loop body (between the outer parens)
REM are invisible to goto â€” goto :next_store_%%i would fail because
REM the label is registered literally as ":next_store_%%i" but the
REM goto looks for ":next_store_1" at runtime.  Calling a subroutine
REM instead lets us use  goto :eof  to skip the remaining steps for
REM a store, with no dynamic label names needed.
REM ---------------------------------------------------------------
for /L %%i in (1,1,%STORE_COUNT%) do call :process_store %%i

if %OVERALL_RESULT%==0 (
    call :log_msg "=== Store Maintenance Completed Successfully ==="
) else (
    call :log_msg "=== Store Maintenance Completed With Errors (exit=%OVERALL_RESULT%) ==="
)

exit /b %OVERALL_RESULT%

REM ---------------------------------------------------------------
REM :process_store IDX
REM   Per-store maintenance logic extracted from the former for-loop
REM   body.  goto :eof replaces goto :next_store_%%i.
REM   OVERALL_RESULT is modified directly (no setlocal) so changes
REM   propagate back to the caller.
REM ---------------------------------------------------------------
:process_store
set "IDX=%~1"
set "SDIR=!STATE_DIR[%IDX%]!"
set "CDIR=!CACHE_DIR[%IDX%]!"
set "SNAME=!STORE_NAME[%IDX%]!"

call :log_msg "--- Processing store: !SNAME! ---"
call :log_msg "  State dir: !SDIR!"
call :log_msg "  Cache dir: !CDIR!"

set "STORE_BACKUP=%BACKUP_DIR%\!SNAME!"

REM Clean per-store backup dir before each cycle so umesnaprepo
REM does not warn about it already existing
if exist "!STORE_BACKUP!" rmdir /s /q "!STORE_BACKUP!"

set "PRIOR_FILE=%WORK_DIR%\statelogprior_!SNAME!.txt"
set "POST_FILE=%WORK_DIR%\statelogpost_!SNAME!.txt"
set "TS_TMP=%WORK_DIR%\ts_tmp_!SNAME!.txt"

set "CACHE_ARGS="
if not "!CDIR!"=="" set "CACHE_ARGS=-c !CDIR!"

REM Step 1: Cleanup zero-message state and cache files
call :log_msg "  Step 1: Cleaning zero-message files..."
"%UMESNAPREPO%" -s "!SDIR!" !CACHE_ARGS! -d "!STORE_BACKUP!" -m0 > "!TS_TMP!" 2>&1
if errorlevel 1 (
    REM "no state files found" means empty store â€” not an error, skip it
    findstr /i /c:"no state files found" "!TS_TMP!" >nul 2>&1
    if not errorlevel 1 (
        call :ts_print "!TS_TMP!"
        call :log_msg "  [WARN] No state files found for store !SNAME! â€” nothing to do, skipping."
        goto :eof
    )
    call :ts_print "!TS_TMP!"
    call :log_msg "ERROR:   umesnaprepo -m0 failed for store !SNAME!"
    set OVERALL_RESULT=3
    goto :eof
)
call :ts_print "!TS_TMP!"

REM Step 2: Dump pre-prune state
call :log_msg "  Step 2: Dumping pre-prune state..."
"%UMESNAPREPO%" -s "!SDIR!" !CACHE_ARGS! -l > "!PRIOR_FILE!" 2>&1
if errorlevel 1 (
    call :log_msg "ERROR:   umesnaprepo -l ^(pre-prune^) failed for store !SNAME!"
    set OVERALL_RESULT=3
    goto :eof
)

REM Step 3: Prune messages older than current Unix timestamp
REM Get-Date -UFormat %%s is buggy on PowerShell 5.x â€” it uses local time
REM components as if they were UTC, giving a timestamp offset by the UTC bias.
REM [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() is unambiguously UTC.
for /f %%t in ('powershell -NoProfile -Command "[int][DateTimeOffset]::UtcNow.ToUnixTimeSeconds()"') do set PRUNE_TS=%%t
call :log_msg "  Step 3: Pruning messages older than timestamp !PRUNE_TS!..."
"%UMESNAPREPO%" -s "!SDIR!" !CACHE_ARGS! -d "!STORE_BACKUP!" -P!PRUNE_TS! > "!TS_TMP!" 2>&1
if errorlevel 1 (
    call :ts_print "!TS_TMP!"
    call :log_msg "ERROR:   umesnaprepo -P failed for store !SNAME!"
    set OVERALL_RESULT=3
    goto :eof
)
call :ts_print "!TS_TMP!"

REM Step 4: Dump post-prune state
call :log_msg "  Step 4: Dumping post-prune state..."
"%UMESNAPREPO%" -s "!SDIR!" !CACHE_ARGS! -l > "!POST_FILE!" 2>&1
if errorlevel 1 (
    call :log_msg "ERROR:   umesnaprepo -l ^(post-prune^) failed for store !SNAME!"
    set OVERALL_RESULT=3
    goto :eof
)

REM Step 5: Verify highest message sequence numbers match
call :log_msg "  Step 5: Verifying highest message sequence numbers..."
set "DIFF1=%WORK_DIR%\diff1_!SNAME!.txt"
set "DIFF2=%WORK_DIR%\diff2_!SNAME!.txt"

findstr /i /c:"-cache" /c:"highest message sequence number" "!PRIOR_FILE!" > "!DIFF1!" 2>nul
findstr /i /c:"-cache" /c:"highest message sequence number" "!POST_FILE!"  > "!DIFF2!" 2>nul
fc "!DIFF1!" "!DIFF2!" >nul 2>&1
if errorlevel 1 (
    call :log_msg "  FAIL: Highest message sequence number mismatch for store !SNAME!"
    call :log_msg "  Pre-prune:"
    for /f "usebackq delims=" %%L in ("!DIFF1!") do call :log_msg "    %%L"
    call :log_msg "  Post-prune:"
    for /f "usebackq delims=" %%L in ("!DIFF2!") do call :log_msg "    %%L"
    set OVERALL_RESULT=1
) else (
    call :log_msg "  PASS: Highest message sequence numbers match for store !SNAME!"

    REM Log trimming summary
    set "MSG_PRIOR=%WORK_DIR%\msg_prior_!SNAME!.txt"
    set "MSG_POST=%WORK_DIR%\msg_post_!SNAME!.txt"
    findstr /i /c:"-cache" /c:"highest message sequence number" /c:"number of messages" "!PRIOR_FILE!" > "!MSG_PRIOR!" 2>nul
    findstr /i /c:"-cache" /c:"highest message sequence number" /c:"number of messages" "!POST_FILE!"  > "!MSG_POST!"  2>nul
    call :log_msg "  Pre-prune message counts:"
    for /f "usebackq delims=" %%L in ("!MSG_PRIOR!") do call :log_msg "    %%L"
    call :log_msg "  Post-prune message counts:"
    for /f "usebackq delims=" %%L in ("!MSG_POST!") do call :log_msg "    %%L"
)

call :log_msg "--- Finished store: !SNAME! ---"
goto :eof

REM ---------------------------------------------------------------
REM :log_msg MSG
REM   Print message with timestamp to stdout and optional log file.
REM   Uses PowerShell for timestamp (wmic is deprecated on Windows 11+)
REM ---------------------------------------------------------------
:log_msg
set "MSG=%~1"
if "%UTC_TIMESTAMPS%"=="1" (
    for /f "usebackq tokens=*" %%I in (`powershell -NoProfile -Command "(Get-Date).ToUniversalTime().ToString('yyyy-MM-dd HH:mm:ss') + ' UTC'"`) do set TS=%%I
) else (
    for /f "usebackq tokens=*" %%I in (`powershell -NoProfile -Command "Get-Date -Format 'yyyy-MM-dd HH:mm:ss'"`) do set TS=%%I
)
echo [%TS%] %MSG%
if not "%LOG_FILE%"=="" echo [%TS%] %MSG% >> "%LOG_FILE%"
goto :eof

REM ---------------------------------------------------------------
REM :ts_print FILE
REM   Print each line of FILE with a timestamp prefix,
REM   filtering the benign "already exists" warning.
REM ---------------------------------------------------------------
:ts_print
set "TS_FILE=%~1"
if not exist "%TS_FILE%" goto :eof
REM Fetch timestamp once for the whole file â€” avoids a PowerShell
REM launch per output line (each launch costs ~200-500ms on Windows).
if "%UTC_TIMESTAMPS%"=="1" (
    for /f "usebackq tokens=*" %%I in (`powershell -NoProfile -Command "(Get-Date).ToUniversalTime().ToString('yyyy-MM-dd HH:mm:ss') + ' UTC'"`) do set TS2=%%I
) else (
    for /f "usebackq tokens=*" %%I in (`powershell -NoProfile -Command "Get-Date -Format 'yyyy-MM-dd HH:mm:ss'"`) do set TS2=%%I
)
for /f "usebackq delims=" %%L in ("%TS_FILE%") do (
    set "LINE=%%L"
    REM Filter benign backup-dir-already-exists warning
    echo !LINE! | findstr /i /c:"already exists" >nul 2>&1
    if errorlevel 1 (
        echo [!TS2!]   !LINE!
        if not "%LOG_FILE%"=="" echo [!TS2!]   !LINE! >> "%LOG_FILE%"
    )
)
goto :eof
