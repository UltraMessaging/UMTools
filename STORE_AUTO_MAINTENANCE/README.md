# UME Store Auto-Maintenance

Automated maintenance wrapper for the Ultra Messaging `umestored` daemon. Manages the store process lifecycle, monitors logs for error conditions, and executes scheduled maintenance (prune, verify, restart) with multi-tier failure recovery.

## Overview

`store_auto_maint` runs as a parent process that:

1. **Launches `umestored`** as a child process using the same XML configuration file
2. **Monitors** the store's log output for configurable error keywords
3. **Executes maintenance** on a schedule (weekly, daily, or fixed interval)
4. **Recovers automatically** using a 3-tier strategy if restart fails after maintenance

The maintenance procedure uses `umesnaprepo` to clean up zero-message files, prune old messages from the cache, and verify data integrity before restarting the store.

## Architecture

```
  Operator
     |
     v
 store_auto_maint  ---reads--->  store_config.xml
     |                                |
     |  extracts: log path,           |
     |  store names,                  |
     |  state/cache dirs              |
     |                                |
     +---exec--->  umestored store_config.xml  (child process)
     |                 |
     |            stdout/stderr --pipe--> parent (keyword scanner)
     |                 |
     |            log file --tail--> parent (keyword scanner)
     |
     +---on schedule---> SIGINT child -> waitpid
     |                       |
     |               maintain_store.sh
     |               (iterates all store dirs)
     |                       |
     |               restart child (exec umestored)
     |               3-tier recovery if needed
     |
     +---logs to---> store_maint.log
```

### Components

| File | Description |
|------|-------------|
| `store_auto_maint.c` | Parent process: child management, scheduling, log monitoring, recovery |
| `xml_config_parser.c` / `.h` | Minimal XML parser for UME store config (no external dependencies) |
| `maintain_store.sh` | Linux/macOS maintenance script wrapping `umesnaprepo` operations |
| `maintain_store.bat` | Windows maintenance script |
| `Makefile` | Build system |

## Building umesnaprepo

`umesnaprepo` is part of the UMP SDK. If you need to build it from source, adjust `UMP_DIR` to your SDK installation path:

```bash
UMP_DIR=UMP_6.17/Linux-glibc-2.17-x86_64

gcc umesnaprepo.c -o umesnaprepo \
    -D_FILE_OFFSET_BITS=64 -Wno-long-long -fno-strict-aliasing \
    -D_REENTRANT -g -O3 \
    -I${UMP_DIR}/include \
    -I${UMP_DIR}/include/lbm \
    -I. \
    -L${UMP_DIR}/lib \
    -lumestorelib -llbm -llbmsdm -llbmutl -lrsock \
    -lqpid-proton -lstdc++ -lpthread -lcrypto -lssl \
    -lrt -lm -ldl -lsmartheap_smp64 -lprotobuf-c
```

The example above targets UMP 6.17 on Linux x86_64 (glibc 2.17). Adjust the SDK path and library list to match your UMP version and platform.

### Windows (Visual Studio / MSVC)

```bat
set UMQ=C:\path\to\UMQ\Win2k-x86_64

cl -DWIN32_LEAN_AND_MEAN -DWIN32_EXTRA_LEAN -DHAVE_CONFIG_H ^
    -I"%UMQ%\include" ^
    -I"%UMQ%\include\lbm" ^
    -I. ^
    /Ob1 /Oi /Ot /O2 /GL /Z7 -Fd.\ -c umesnaprepo.c

link.exe /OUT:umesnaprepo.exe ws2_32.lib /NOLOGO /INCREMENTAL:no /LTCG /DEBUG ^
    /SUBSYSTEM:console umesnaprepo.obj getopt.obj ^
    "%UMQ%\lib\umestore.lib" "%UMQ%\lib\lbm.lib"
```

> **WARNING — Run `umesnaprepo` on the same platform that wrote the state files.**
> `umesnaprepo` reads umestored's persistent state and cache files via `lbm_srp_*` calls.
> When the binary's view of the on-disk format doesn't match what's actually on disk,
> `lbm_srp_get_repo_state()` rejects the file with status `-15`:
>
> ```
> Error: called lbm_srp_get_repo_state() on corrupt repository at index [0];
>        status [-15] info [0] state file name [.../1234-state]
> ```
>
> In our testing, this resolved when we used the platform-native `umesnaprepo` to read
> files that umestored had written on the same platform — i.e. Linux `umesnaprepo` for
> Linux-written state, Windows `umesnaprepo.exe` for Windows-written state. Mixing the
> two reproduced the `-15` error reliably (each side rejected the other side's files).
>
> If you see `-15` and the file isn't actually corrupt, the most likely fixes, in order:
> 1. Use the `umesnaprepo` from the platform that wrote the file.
> 2. If that's not possible, rebuild `umesnaprepo` against the same UMP/UMQ version as
>    the umestored that wrote the file. (UMP version may also matter; we did not test a
>    full version-compatibility matrix and can't rule it out.)
>
> The two state files we tested differed in both platform AND umestored version
> (Windows + 6.14.0.0 vs Linux + 6.17.0.0), so we cannot fully separate the two
> effects from our data. Treat platform as the primary expected variable; treat
> UMP-version-skew as a secondary suspect to check if a platform-native binary still
> rejects the file.

## Prerequisites

- **`umestored`** binary accessible via PATH or specified with `-e`
- **`umesnaprepo`** binary accessible via PATH or specified with `-r`
- A valid UME store XML configuration file with `disk-state-directory` and `disk-cache-directory` defined per store
- Linux: `gcc`, standard C library (no external library dependencies)
- Windows: Visual Studio or MinGW

## Building

### Linux

```bash
make
```

Produces `store_auto_maint` with no external dependencies beyond the standard C library and pthreads.

### Windows (MinGW cross-compile)

```bash
CC=x86_64-w64-mingw32-gcc CFLAGS="-Wall -O2 -D_WIN32" LDFLAGS="-lws2_32" make
```

### Windows (Visual Studio)

```
cl /O2 /W3 store_auto_maint.c xml_config_parser.c /Fe:store_auto_maint.exe
```

> **Note:** Windows support is verified end-to-end with VS 2022 BuildTools. Validate-only,
> schedule parsing, multi-store maintenance script, 3-tier recovery, and PID-file handling
> all pass. Operator console-event handling uses `CTRL_BREAK_EVENT` (not `CTRL_C_EVENT`)
> because the child is launched with `CREATE_NEW_PROCESS_GROUP`. See `bin/Win2k-x86_64/`
> for prebuilt executables and `BUILD_ENVIRONMENT.txt` for the exact toolchain used.

## Configuration

### XML Config File

`store_auto_maint` reads the **same XML config** that `umestored` uses. It extracts:

- **Log file path** from `<daemon><log>`:
  ```xml
  <daemon>
    <log>/var/log/umestored.log</log>
  </daemon>
  ```
  If no `<log>` element is present, the parent monitors the child's stdout/stderr via pipe.

- **Store instances and directories** from `<stores><store>`:
  ```xml
  <stores>
    <store name="store0" port="14667">
      <ume-attributes>
        <option type="store" name="disk-cache-directory" value="/data/store0/cache"/>
        <option type="store" name="disk-state-directory" value="/data/store0/state"/>
      </ume-attributes>
    </store>
    <store name="store1" port="14668">
      <ume-attributes>
        <option type="store" name="disk-cache-directory" value="/data/store1/cache"/>
        <option type="store" name="disk-state-directory" value="/data/store1/state"/>
      </ume-attributes>
    </store>
  </stores>
  ```

All stores with a `disk-state-directory` are included in the maintenance cycle by default.

## Usage

```
store_auto_maint -x CONFIG_FILE [options] [-- UMESTORED_OPTS...]
```

### Required

| Option | Description |
|--------|-------------|
| `-x FILE` | UME store XML configuration file |

### Optional

| Option | Description | Default |
|--------|-------------|---------|
| `-f SCHED` | Maintenance schedule (see [Scheduling](#scheduling)) | `weekly:fri:00:00` |
| `-k LEVEL` | Minimum log keyword severity to monitor: `warn`, `err`, `alert`, `crit` | `crit` |
| `-N NUM` | Number of keyword occurrences to trigger alert | `2` |
| `-M SECS` | Time window in seconds for keyword threshold | `60` |
| `-d DIR` | Backup directory for pre-maintenance files | `./UMDIR` |
| `-L FILE` | Maintenance wrapper log file | `./store_maint.log` |
| `-e PATH` | Path to `umestored` binary | `umestored` |
| `-r PATH` | Path to `umesnaprepo` binary | `umesnaprepo` |
| `-R DIR` | Directory containing `maintain_store.sh` | `.` |
| `-S NAME` | Process only this named store (skip others) | all stores |
| `-W SECS` | Graceful shutdown wait in seconds (phase 1 SIGINT) | `3600` (1 hour) |
| `-P FILE` | PID file for the managed `umestored` process | `./umestored_managed.pid` |
| `-u` | Log timestamps in UTC instead of local time | local time |
| `-y` | Skip operator confirmation prompt | interactive |
| `-V` | Validate configuration and exit (do not start) | - |
| `-h` | Display help | - |
| `-- OPTS...` | Pass remaining arguments through to `umestored` | none |

### Passthrough Options

Any arguments after `--` are passed directly to `umestored` on each launch. This allows forwarding `umestored`-specific options (e.g., `-u`, `-a`) without `store_auto_maint` interpreting them. The XML config file is always appended as the final argument.

```bash
# Pass -u and -a options to umestored
./store_auto_maint -x store_config.xml -- -u -a 1,3,5
```

### Examples

**Typical production use** (weekly Friday midnight, default keyword monitoring):
```bash
./store_auto_maint -x /etc/ume/store_config.xml \
    -e /opt/ume/bin/umestored \
    -r /opt/ume/bin/umesnaprepo \
    -d /var/backup/ume
```

**Daily maintenance at 2 AM, monitoring for any "err" level messages**:
```bash
./store_auto_maint -x store_config.xml -f daily:02:00 -k err
```

**Test every 5 minutes, auto-confirm**:
```bash
./store_auto_maint -x store_config.xml -f interval:300 -y
```

**Maintain only one specific store**:
```bash
./store_auto_maint -x store_config.xml -S store0
```

**Pass umestored options through** (e.g., activity threads and affinity):
```bash
./store_auto_maint -x store_config.xml -- -u -a 1,3,5
```

**Custom PID file location** (useful when running multiple instances or non-default working directories):
```bash
./store_auto_maint -x store_config.xml -P /var/run/ume/umestored.pid
```

**UTC timestamps** (useful when hosts span multiple time zones):
```bash
./store_auto_maint -x store_config.xml -u
```

**Validate configuration without starting**:
```bash
./store_auto_maint -x store_config.xml -V
```

## Scheduling

The `-f` option accepts three formats:

| Format | Example | Description |
|--------|---------|-------------|
| `weekly:DAY:HH:MM` | `weekly:fri:00:00` | Every week on the given day and time |
| `daily:HH:MM` | `daily:02:00` | Every day at the given time |
| `interval:SECONDS` | `interval:3600` | Fixed interval between maintenance cycles |

Day names: `sun`, `mon`, `tue`, `wed`, `thu`, `fri`, `sat` (case-insensitive).

Times are in local system time. The scheduler runs internally (no cron dependency).

## Log Keyword Monitoring

The parent process continuously scans the store's output for severity keywords:

| Level | Keyword | UME log tag matched | Description |
|-------|---------|---------------------|-------------|
| `warn` | `-k warn` | `[WARN]`, `[WARNING]` | Warnings and above |
| `err` | `-k err` | `[ERROR]`, `[ERR]` | Errors and above |
| `alert` | `-k alert` | `[ALERT]` | Alerts and above |
| `crit` | `-k crit` | `[CRIT]`, `[CRITICAL]` | Critical only (default) |

Keyword matching is **case-insensitive** and **hierarchical**: `-k err` matches `[ERROR]`, `[ALERT]`, and `[CRIT]` but not `[WARNING]`.

### Severity tag matching

UME log lines carry a bracketed severity tag, for example:
```
[ERROR]: Core-11187-1: Error parsing line 293 attribute name 'default_interface'
[CRIT]: Store disk full — cannot continue
```

The scanner matches against the `[LEVEL]` tag specifically, **not** the full line body. This avoids false positives where a keyword appears incidentally in the message text:

```
# With -k crit, this line does NOT trigger — [ERROR] tag is below crit threshold:
[ERROR]: CoreApi-5688-4105: no interfaces matching crit ...

# This line DOES trigger:
[CRIT]: Store disk full
```

Lines with no `[LEVEL]` tag (unstructured output from umestored) fall back to bare substring matching for compatibility.

### Threshold

Configured with `-N` and `-M`:
- `-N 2 -M 60` (default): trigger when 2 or more keyword matches occur within 60 seconds

When the threshold is exceeded, `store_auto_maint` logs an error and **exits** — the store is left running but unmanaged. This is intentional: repeated critical errors indicate a condition requiring operator intervention rather than automated recovery.

### Monitoring sources

- **stderr/stdout** from the child process is always captured via pipe
- If a `<log>` file is configured in the XML, it is also tailed for keyword matches
- If no `<log>` is configured, the child pipe is the sole source

## Maintenance Procedure

When scheduled maintenance triggers, the following sequence executes:

### Step 1: Stop the Store
The parent uses a 3-phase escalating shutdown. The shutdown duration and each phase transition is logged.

| Phase | Action | Wait Time | Default |
|-------|--------|-----------|---------|
| 1 | Single `SIGINT` | `-W` seconds | 3600s (1 hour) |
| 2 | Two `SIGINT`s in succession | Fixed | 600s (10 minutes) |
| 3 | `SIGKILL` (forced) | Immediate | - |

Progress is logged every 60 seconds during long waits. On completion, the total shutdown time is recorded:
```
[INFO] Phase 1: Sending SIGINT, waiting up to 3600 seconds for graceful shutdown...
[INFO] Shutdown in progress... 60/3600 seconds elapsed
...
[INFO] umestored exited (status 0) after 245 seconds (phase 1 - single SIGINT)
```

Use `-W` to adjust the phase 1 wait time (e.g., `-W 1800` for 30 minutes).

### Step 2: Run Maintenance Script
`maintain_store.sh` executes for each store instance:

1. **Cleanup zero-message files**: `umesnaprepo -s STATE -c CACHE -d BACKUP -m0`
2. **Dump pre-prune state**: `umesnaprepo -s STATE -c CACHE -l > statelogprior.txt`
3. **Prune old messages**: `umesnaprepo -s STATE -c CACHE -d BACKUP -P<timestamp>`
4. **Dump post-prune state**: `umesnaprepo -s STATE -c CACHE -l > statelogpost.txt`
5. **Verify**: compare "highest message sequence number" before and after pruning

If verification fails for any store, the script exits with a non-zero status.

### Step 3: Restart with 3-Tier Recovery

| Tier | Condition | Action |
|------|-----------|--------|
| 1 | First attempt | Restart `umestored` with pruned files |
| 2 | Tier 1 failed | Revert to backup files from `BACKUP_DIR`, restart |
| 3 | Tier 2 failed | Wipe all state/cache directories, restart fresh |

After each restart attempt, the parent waits 5 seconds and checks if the child is still alive. If all 3 tiers fail, a `CRITICAL` error is logged.

## Startup Confirmation

On launch, the program displays all extracted configuration and prompts the operator:

```
  Store Auto-Maintenance Configuration:
  ======================================
    XML Config:  /etc/ume/store_config.xml
    Log source:  /var/log/umestored.log
    Stores found: 2
      [1] store0               state=/data/store0/state  cache=/data/store0/cache
      [2] store1               state=/data/store1/state  cache=/data/store1/cache
    Schedule:    Every Friday at 00:00
    Keyword:     crit (threshold: 2 in 60s)
    Shutdown:    3600s graceful -> 600s escalated -> SIGKILL
    Backup dir:  ./UMDIR
    Maint log:   ./store_maint.log
    umestored:   umestored
    umesnaprepo: umesnaprepo

    Proceed? [Y/n]
```

Use `-y` to skip this prompt for unattended/automated deployments.

## Maintenance Log

All activity is logged to the file specified by `-L` (default: `./store_maint.log`) and echoed to stdout. Log entries include:

- Child process start/stop events
- Keyword match warnings
- Maintenance cycle initiation, completion, and failure
- Running statistics: `initiated=N completed=N failed=N`
- Per-store prune results and verification outcomes

## Statistics

The parent tracks and periodically logs:

| Metric | Description |
|--------|-------------|
| `initiated` | Number of maintenance cycles started |
| `completed` | Number of cycles completed successfully (store restarted) |
| `errors` | Cycles where the maintenance script failed but the store was still restarted |
| `failed` | Number of cycles where all recovery tiers failed |

Statistics are logged after each maintenance cycle and at program exit.

## Signals

| Signal | Behavior |
|--------|----------|
| `SIGINT` / `SIGTERM` | Graceful shutdown: stops child, logs final stats, exits |
| `SIGCHLD` | Detects unexpected child exit, triggers restart |
| `SIGPIPE` | Ignored |

On Windows, `Ctrl+C` and `Ctrl+Break` are handled equivalently.

### Double Ctrl-C: skip the phase-1 wait

When you `Ctrl-C` twice within 5 seconds (or send two `SIGINT`s on Linux, two `Ctrl+Break` events on Windows), `store_auto_maint` skips the configurable phase-1 wait (`-W`, default 1 hour) and goes straight to phase-2 escalation (two `SIGINT`s to the child, capped at 10 minutes). Useful when you know the store is hung and don't want to wait the default hour. Logged as:

```
[WARN] Second signal within 5s; skipping phase-1 wait.
[WARN] Phase 2: Operator requested fast shutdown. Sending two SIGINTs, waiting up to 600 seconds...
```

> **Windows note:** Pressing `Ctrl+C` in the console window where `store_auto_maint` is
> running may terminate only the batch file (`maintain_store.bat`) and not the underlying
> `umestored` or `store_auto_maint` process. If the maintenance script appears to hang,
> use Task Manager or `taskkill /F /PID <pid>` to stop the process explicitly. The PID
> of the managed `umestored` is recorded in the PID file (default `./umestored_managed.pid`).

## PID File

`store_auto_maint` writes a PID file containing the `umestored` child process ID immediately after each successful start. The file is removed when the child exits.

**Default location:** `./umestored_managed.pid`
**Override with:** `-P FILE`

On startup, the PID file is checked before launching a new child:

| PID file state | Action |
|----------------|--------|
| No file | Clean start, proceed normally |
| File exists, process not running | `[WARN]` logged, stale file removed, proceed |
| File exists, process still running | `[ERROR]` logged, exit with code 1 |

The live-process check prevents accidentally starting a second `umestored` against the same store if the wrapper crashed and left a running child behind. Example error output:

```
[ERROR] PID file ./umestored_managed.pid exists and PID 12345 is still running.
[ERROR] A previously managed umestored may still be active.
[ERROR] Stop it manually or remove the PID file to proceed.
```

## Maintenance Script (Manual Use)

`maintain_store.sh` can be run independently for manual maintenance:

```bash
./maintain_store.sh \
    -r /path/to/umesnaprepo \
    -d /backup/dir \
    -l /path/to/logfile \
    -s /data/store0/state -c /data/store0/cache -n store0 \
    -s /data/store1/state -c /data/store1/cache -n store1
```

Exit codes:
- `0` — all stores maintained and verified successfully
- `1` — verification failed (sequence number mismatch)
- `2` — usage/argument error
- `3` — `umesnaprepo` execution error

## File Layout

```
STORE_AUTO_MAINTENANCE/
  store_auto_maint.c         Main parent process
  xml_config_parser.c        XML config parser (implementation)
  xml_config_parser.h        XML config parser (header)
  maintain_store.sh          Linux maintenance script
  maintain_store.bat         Windows maintenance script
  Makefile                   Build system for store_auto_maint
  umesnaprepo.c              Repository tool source (forked UMP sample)
  bld_umesnaprepo.sh         Linux build script for umesnaprepo
  bld_umesnaprepo.bat        Windows build script for umesnaprepo
  getopt.c                   getopt() implementation for Windows umesnaprepo build
  lbm-example-util.h         UMP example helper header (required by umesnaprepo.c)
  replgetopt.h               UMP example getopt helper header
  test_config.xml            Sample umestored configuration
  bin/
    Linux-glibc-2.17-x86_64/ Prebuilt Linux x86_64 binaries
      store_auto_maint
      umesnaprepo
    Win2k-x86_64/            Prebuilt Windows x64 binaries
      store_auto_maint.exe
      umesnaprepo.exe
  BUILD_ENVIRONMENT.txt      Toolchain & UMP versions used to build the bin/ tree
  README.md                  This file
```

The bundled binaries in `bin/` are provided as a convenience. They will only work if the
target machine's umestored matches the UMP version they were linked against (see the
state-file-format-version warning above). For any other UMP version, rebuild from source.
