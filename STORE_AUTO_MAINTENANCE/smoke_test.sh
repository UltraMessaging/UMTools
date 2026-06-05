#!/bin/bash
#
# smoke_test.sh — validate a published store_auto_maintenance release on Linux.
#
# Default mode (--quick): downloads the latest published tree, checks the
# prebuilt binary runs, rebuilds from source, and runs `-V` validate-only.
# ~30 seconds. No license or umestored required.
#
# --full mode: also runs an end-to-end maintenance cycle. Starts umestored,
# sends 100 messages, waits for the scheduled cycle to prune, sends 5 more
# messages to confirm the restarted store accepts them.  ~90 seconds.
# Requires umestored on PATH and a valid LBM license.
#
# --local: skip download; test the tree in the current directory (useful
# during development).
#
# Env vars (override prompt / auto-detect):
#   UMP_DIR                Path to UMP install root (e.g. /opt/UMP_6.17/Linux-glibc-2.17-x86_64)
#   LBM_LICENSE_FILENAME   Path to license file (--full only)
#   GIT_REF                Branch/tag to test (default: master)
#   STAY                   If set, leave the sandbox in place after the run
#
# Exit codes:
#   0 - all tests passed
#   1 - test failure
#   2 - prerequisite missing (no UMP_DIR found, etc.)

set -uo pipefail

readonly REPO_OWNER="UltraMessaging"
readonly REPO_NAME="UMTools"
readonly SUBDIR="STORE_AUTO_MAINTENANCE"
readonly DEFAULT_REF="${GIT_REF:-master}"
readonly TEST_TOPIC="TOPIC_TEST"
readonly TEST_PORT=14444
readonly INTERVAL_SECS=60

# ---------- output helpers ----------
if [ -t 1 ]; then
  C_RED=$'\e[31m'; C_GRN=$'\e[32m'; C_YLW=$'\e[33m'; C_DIM=$'\e[2m'; C_OFF=$'\e[0m'
else
  C_RED=""; C_GRN=""; C_YLW=""; C_DIM=""; C_OFF=""
fi
pass() { printf "  ${C_GRN}PASS${C_OFF}  %s\n" "$1"; }
fail() { printf "  ${C_RED}FAIL${C_OFF}  %s\n" "$1"; FAIL_COUNT=$((FAIL_COUNT+1)); }
warn() { printf "  ${C_YLW}WARN${C_OFF}  %s\n" "$1"; }
info() { printf "  ${C_DIM}info${C_OFF}  %s\n" "$1"; }
step() { printf "\n${C_GRN}==> %s${C_OFF}\n" "$1"; }
die()  { printf "${C_RED}ERROR:${C_OFF} %s\n" "$1" >&2; exit "${2:-1}"; }

FAIL_COUNT=0

usage() {
  cat <<EOF
Usage: $0 [--quick|--full] [--local] [--help]

Options:
  --quick    (default) Download + binary sanity + build + validate. ~30s.
  --full     Also run the end-to-end maintenance cycle.            ~90s.
  --local    Skip download; test the tree in cwd.
  --help     This message.

Env vars:
  UMP_DIR                  UMP install path (auto-detected if unset)
  LBM_LICENSE_FILENAME     License file (--full only; auto-detected)
  GIT_REF                  Branch/tag to download (default: master)
  STAY                     If set, leave sandbox in place after run
EOF
}

# ---------- argument parsing ----------
MODE="quick"
LOCAL=0
while [ $# -gt 0 ]; do
  case "$1" in
    --quick) MODE="quick" ;;
    --full)  MODE="full" ;;
    --local) LOCAL=1 ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown option: $1 (try --help)" 2 ;;
  esac
  shift
done

# ---------- sandbox setup ----------
SANDBOX="$(mktemp -d -t store_auto_maint_smoke.XXXXXX)"
cleanup() {
  if [ -n "${PARENT_PID:-}" ] && kill -0 "$PARENT_PID" 2>/dev/null; then
    kill -INT "$PARENT_PID" 2>/dev/null
    sleep 2
    kill -KILL "$PARENT_PID" 2>/dev/null
  fi
  if [ -z "${STAY:-}" ]; then
    rm -rf "$SANDBOX"
  else
    info "sandbox preserved at $SANDBOX (STAY env was set)"
  fi
}
trap cleanup EXIT INT TERM

step "Smoke test: $MODE mode"
info "sandbox: $SANDBOX"

# ---------- locate or download the release tree ----------
step "1) Obtain the release tree"

if [ "$LOCAL" -eq 1 ]; then
  if [ -f "store_auto_maint.c" ] && [ -f "umesnaprepo.c" ]; then
    TREE="$PWD"
    info "using local tree: $TREE"
  else
    die "--local but cwd doesn't look like store_auto_maintenance/ (no store_auto_maint.c)"
  fi
else
  TREE="$SANDBOX/release"
  mkdir -p "$TREE"
  if command -v gh >/dev/null 2>&1 && gh auth status >/dev/null 2>&1; then
    info "downloading via gh from $REPO_OWNER/$REPO_NAME@$DEFAULT_REF"
    (cd "$SANDBOX" && gh repo clone "$REPO_OWNER/$REPO_NAME" repo -- --depth 1 --branch "$DEFAULT_REF" 2>/dev/null) \
      || die "gh repo clone failed"
    cp -r "$SANDBOX/repo/$SUBDIR/." "$TREE/"
  elif command -v git >/dev/null 2>&1; then
    info "downloading via git clone"
    (cd "$SANDBOX" && git clone --depth 1 --branch "$DEFAULT_REF" "https://github.com/$REPO_OWNER/$REPO_NAME.git" repo) \
      || die "git clone failed"
    cp -r "$SANDBOX/repo/$SUBDIR/." "$TREE/"
  elif command -v curl >/dev/null 2>&1; then
    info "downloading tarball via curl"
    URL="https://github.com/$REPO_OWNER/$REPO_NAME/archive/refs/heads/$DEFAULT_REF.tar.gz"
    curl -sL "$URL" -o "$SANDBOX/repo.tar.gz" || die "curl failed"
    (cd "$SANDBOX" && tar xzf repo.tar.gz) || die "tar extract failed"
    cp -r "$SANDBOX/$REPO_NAME-$DEFAULT_REF/$SUBDIR/." "$TREE/"
  else
    die "need one of: gh, git, or curl" 2
  fi
  pass "downloaded $TREE"
fi

# ---------- step 2: file inventory ----------
step "2) File inventory"
for f in store_auto_maint.c xml_config_parser.c umesnaprepo.c \
         maintain_store.sh bld_umesnaprepo.sh Makefile README.md \
         bin/Linux-glibc-2.17-x86_64/store_auto_maint \
         bin/Linux-glibc-2.17-x86_64/umesnaprepo; do
  if [ -e "$TREE/$f" ]; then
    pass "$f present"
  else
    fail "$f missing"
  fi
done

# ---------- step 3: file modes ----------
step "3) Permissions on extracted binaries and scripts"
for f in bin/Linux-glibc-2.17-x86_64/store_auto_maint \
         bin/Linux-glibc-2.17-x86_64/umesnaprepo \
         maintain_store.sh bld_umesnaprepo.sh; do
  if [ -x "$TREE/$f" ]; then
    pass "$f is executable"
  else
    warn "$f is NOT executable; restoring (this is a known issue when the tree comes from a Windows-side filesystem)"
    chmod +x "$TREE/$f" 2>/dev/null
  fi
done

# ---------- step 4: resolve UMP_DIR ----------
step "4) Locate UMP install"
if [ -n "${UMP_DIR:-}" ]; then
  info "UMP_DIR from env: $UMP_DIR"
else
  # Glob for likely candidates in priority order: locally-built UMQ first
  # (most likely to be set up correctly), then any UMP_* under $HOME, then /opt.
  detected=""
  for pat in "$HOME/CLAUDE/SF_DEV_MAIN/29West/lbm/pp/UMQ_*/Linux-glibc-2.17-x86_64" \
             "$HOME/UMQ_*/Linux-glibc-2.17-x86_64" \
             "$HOME/UMP_*/Linux-glibc-2.17-x86_64" \
             "/opt/UMQ_*/Linux-glibc-2.17-x86_64" \
             "/opt/UMP_*/Linux-glibc-2.17-x86_64"; do
    for d in $pat; do
      [ -d "$d" ] && [ -d "$d/lib" ] && [ -d "$d/bin" ] && { detected="$d"; break 2; }
    done
  done
  if [ -n "$detected" ]; then
    UMP_DIR="$detected"
    info "auto-detected: $UMP_DIR"
  elif [ -t 0 ]; then
    candidates=$(ls -d $HOME/UMP_* $HOME/UMQ_* /opt/UMP_* /opt/UMQ_* 2>/dev/null | head -5)
    if [ -n "$candidates" ]; then
      info "candidates found:"
      printf "    %s\n" $candidates
    fi
    printf "Enter UMP_DIR (path to UMP/UMQ Linux-glibc install; e.g. /opt/UMP_6.17/Linux-glibc-2.17-x86_64): "
    read UMP_DIR
  else
    die "UMP_DIR not set and not auto-detected; set UMP_DIR env var to your UMP install (e.g. /opt/UMP_6.17/Linux-glibc-2.17-x86_64)" 2
  fi
fi
[ -d "$UMP_DIR" ] || die "UMP_DIR does not exist: $UMP_DIR" 2
[ -d "$UMP_DIR/lib" ] || die "$UMP_DIR/lib not found — wrong UMP path?" 2
[ -d "$UMP_DIR/bin" ] || die "$UMP_DIR/bin not found — wrong UMP path?" 2
pass "UMP_DIR=$UMP_DIR"

export LD_LIBRARY_PATH="$UMP_DIR/lib:${LD_LIBRARY_PATH:-}"
export PATH="$UMP_DIR/bin:$PATH"

# ---------- step 5: ldd check ----------
step "5) Check umesnaprepo runtime deps resolve"
LDD_OUT=$(ldd "$TREE/bin/Linux-glibc-2.17-x86_64/umesnaprepo" 2>&1)
if echo "$LDD_OUT" | grep -q "not found"; then
  fail "umesnaprepo has unresolved deps:"
  echo "$LDD_OUT" | grep "not found" | sed 's/^/        /'
else
  pass "all umesnaprepo runtime deps resolve under LD_LIBRARY_PATH=$UMP_DIR/lib"
fi

# ---------- step 6: prebuilt binary -h works ----------
step "6) Prebuilt binaries run"
if "$TREE/bin/Linux-glibc-2.17-x86_64/store_auto_maint" -h 2>&1 | grep -q "store_auto_maint v"; then
  pass "store_auto_maint -h"
else
  fail "store_auto_maint -h did not print expected banner"
fi

# umesnaprepo with no args exits 1 (usage error) — don't let that fail the pipe.
# Capture output, then test it.
SNAP_OUT=$( "$TREE/bin/Linux-glibc-2.17-x86_64/umesnaprepo" 2>&1 || true )
if echo "$SNAP_OUT" | grep -q "Available options:"; then
  pass "umesnaprepo prints usage"
else
  fail "umesnaprepo did not run; first lines of output:"
  echo "$SNAP_OUT" | head -3 | sed 's/^/        /'
fi

# ---------- step 7: build from source ----------
step "7) Build from source"
(cd "$TREE" && make clean >/dev/null 2>&1 && make 2>&1 | tee "$SANDBOX/make.out" >/dev/null)
if [ -x "$TREE/store_auto_maint" ]; then
  pass "store_auto_maint built ($(stat -c %s "$TREE/store_auto_maint") bytes)"
else
  fail "make did not produce store_auto_maint"
  tail -10 "$SANDBOX/make.out"
fi

# ---------- step 8: -V validate ----------
step "8) -V validate against test_config.xml"
V_OUT=$( (cd "$TREE" && ./store_auto_maint -x test_config.xml -V \
            -r ./bin/Linux-glibc-2.17-x86_64/umesnaprepo) 2>&1)
if echo "$V_OUT" | grep -q "Configuration is valid."; then
  pass "configuration valid"
else
  fail "validation failed:"
  echo "$V_OUT" | sed 's/^/        /'
fi

# ---------- if --quick, stop here ----------
if [ "$MODE" = "quick" ]; then
  step "Summary"
  if [ "$FAIL_COUNT" -eq 0 ]; then
    printf "${C_GRN}OK: all quick checks passed${C_OFF}\n"
    exit 0
  else
    printf "${C_RED}FAIL: %d check(s) failed${C_OFF}\n" "$FAIL_COUNT"
    exit 1
  fi
fi

# ---------- --full E2E pipeline ----------

# ---------- step 9: license ----------
step "9) Locate LBM license (--full only)"
if [ -n "${LBM_LICENSE_FILENAME:-}" ] && [ -f "${LBM_LICENSE_FILENAME}" ]; then
  info "license from env: $LBM_LICENSE_FILENAME"
elif [ -f "$HOME/lic.6.0.txt" ]; then
  export LBM_LICENSE_FILENAME="$HOME/lic.6.0.txt"
  info "auto-detected: $LBM_LICENSE_FILENAME"
elif [ -f "$HOME/Documents/lic.6.0.txt" ]; then
  export LBM_LICENSE_FILENAME="$HOME/Documents/lic.6.0.txt"
  info "auto-detected: $LBM_LICENSE_FILENAME"
else
  if [ -t 0 ]; then
    printf "Enter LBM license file path (e.g. /home/you/lic.6.0.txt): "
    read LBM_LICENSE_FILENAME
    export LBM_LICENSE_FILENAME
  else
    die "LBM_LICENSE_FILENAME not set and not auto-detected; --full requires a license" 2
  fi
fi
[ -f "$LBM_LICENSE_FILENAME" ] || die "license file not found: $LBM_LICENSE_FILENAME" 2
pass "LBM_LICENSE_FILENAME=$LBM_LICENSE_FILENAME"

# ---------- step 10: pick a usable interface ----------
step "10) Pick interface for default_interface"
WSL_IP=$(ip -o -4 addr show eth0 2>/dev/null | awk '{print $4}')
if [ -z "$WSL_IP" ]; then
  WSL_IP=$(ip -o -4 addr show 2>/dev/null | grep -v "127.0.0.1" | grep -v "169.254" | head -1 | awk '{print $4}')
fi
[ -n "$WSL_IP" ] || die "no usable IPv4 interface found"
SUBNET=$(echo "$WSL_IP" | awk -F. '{print $1"."$2"."$3".0/24"}')
pass "interface subnet: $SUBNET"

# ---------- step 11: build sandbox configs ----------
step "11) Build sandbox"
WORK="$SANDBOX/run"
mkdir -p "$WORK/cache" "$WORK/state" "$WORK/UMDIR"
cp "$TREE/maintain_store.sh" "$WORK/"
chmod +x "$WORK/maintain_store.sh"
cp "$TREE/store_auto_maint" "$WORK/"
cp "$TREE/bin/Linux-glibc-2.17-x86_64/umesnaprepo" "$WORK/"

cat > "$WORK/app.cfg" <<EOF
context resolver_multicast_address 226.16.16.16
context default_interface $SUBNET
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
EOF

cat > "$WORK/store.xml" <<EOF
<?xml version="1.0"?>
<ume-store version="1.3">
  <daemon>
    <log>umestored.log</log>
    <pidfile>umestored.pid</pidfile>
    <lbm-config>app.cfg</lbm-config>
    <web-monitor>*:15405</web-monitor>
  </daemon>
  <stores>
    <store name="store0" port="$TEST_PORT" interface="0.0.0.0">
      <ume-attributes>
        <option type="store" name="disk-cache-directory" value="./cache"/>
        <option type="store" name="disk-state-directory" value="./state"/>
        <option type="store" name="context-name" value="store0"/>
      </ume-attributes>
      <topics>
        <topic pattern=".*" type="PCRE">
          <ume-attributes>
            <option type="store" name="repository-type" value="disk"/>
            <option type="store" name="repository-size-threshold" value="2048"/>
            <option type="store" name="repository-size-limit" value="10485760"/>
            <option type="store" name="repository-disk-file-size-limit" value="1073741824"/>
            <option type="store" name="repository-allow-ack-on-reception" value="1"/>
            <option type="store" name="source-state-lifetime" value="3600000"/>
          </ume-attributes>
        </topic>
      </topics>
    </store>
  </stores>
</ume-store>
EOF
pass "sandbox prepared at $WORK"

# ---------- step 12: pre-flight cleanup ----------
step "12) Pre-flight: clear any orphan umestored on port $TEST_PORT"
ORPHANS=$(ss -tlnp 2>/dev/null | awk -v p=":$TEST_PORT" '$4 ~ p {print $0}')
if [ -n "$ORPHANS" ]; then
  warn "found a process listening on $TEST_PORT — killing"
  pkill -KILL -f "umestored.*store.xml" 2>/dev/null || true
  sleep 1
fi
# Also clear any leftover from earlier smoke-test sandboxes
pkill -KILL -f "/store_auto_maint_smoke\." 2>/dev/null || true
sleep 1
pass "port $TEST_PORT clear"

step "13) Launch store_auto_maint -f interval:$INTERVAL_SECS"
( cd "$WORK" && \
  ./store_auto_maint -x store.xml -y -f "interval:$INTERVAL_SECS" \
    -r ./umesnaprepo -e "$(which umestored)" -R . -L store_maint.log \
    > parent.stdout 2>&1 ) &
PARENT_PID=$!
sleep 5
if ! kill -0 "$PARENT_PID" 2>/dev/null; then
  fail "store_auto_maint died at startup:"
  tail "$WORK/parent.stdout" | sed 's/^/        /'
  exit 1
fi
pass "store_auto_maint PID=$PARENT_PID alive"

# ---------- step 13: send 100 messages ----------
step "14) umesrc -M 100"
( cd "$WORK" && timeout 45 umesrc -c app.cfg -t store0 -M 100 -P 50 -l 256 -V -L 5 "$TEST_TOPIC" ) \
  > "$WORK/umesrc1.out" 2>&1 || true

# Decide pass/fail by observable outcome (cache file written) rather than
# guessing at umesrc stdout phrasing — the store-resolution warning is benign
# and the actual proof is "did the store persist any messages?"
sleep 2  # allow umestored to finish flushing
CACHE_FILE=$(ls "$WORK/cache/"*-cache 2>/dev/null | head -1)
if [ -n "$CACHE_FILE" ]; then
  pre_size=$(stat -c %s "$CACHE_FILE")
  if [ "$pre_size" -gt 1000 ]; then
    pass "cache populated: $(basename "$CACHE_FILE") ($pre_size bytes)"
  else
    fail "cache file exists but is too small: $pre_size bytes (umesrc may have hit store-unresolved errors)"
    tail -3 "$WORK/umesrc1.out" | sed 's/^/        /'
  fi
else
  if grep -qi license "$WORK/umesrc1.out"; then
    fail "license issue:"
    grep -i license "$WORK/umesrc1.out" | sed 's/^/        /'
    exit 1
  fi
  fail "no cache file created — umesrc could not reach the store:"
  tail -3 "$WORK/umesrc1.out" | sed 's/^/        /'
fi

# ---------- step 14: poll for cycle ----------
step "15) Wait for scheduled cycle to complete"
for i in $(seq 1 18); do
  sleep 5
  if [ -f "$WORK/store_maint.log" ]; then
    cmp=$(grep -c "STATS.*completed=1" "$WORK/store_maint.log" 2>/dev/null || true)
  else
    cmp=0
  fi
  printf "  +%ds: completed=%d\n" $((i*5)) "$cmp"
  if [ "$cmp" -ge 1 ]; then
    pass "cycle completed within $((i*5))s"
    break
  fi
done
if [ "${cmp:-0}" -lt 1 ]; then
  fail "cycle did not complete in 90s"
  tail -20 "$WORK/store_maint.log" | sed 's/^/        /'
fi

# ---------- step 15: verify prune happened ----------
step "16) Verify prune"
if grep -q "PASS: Highest message sequence numbers match" "$WORK/store_maint.log"; then
  pass "maintain_store.sh reported PASS"
else
  fail "maintain_store.sh did not report PASS:"
  grep -E "Step|PASS|FAIL" "$WORK/store_maint.log" | sed 's/^/        /'
fi

if [ -d "$WORK/UMDIR/store0" ] && [ -n "$(ls -A "$WORK/UMDIR/store0" 2>/dev/null)" ]; then
  pass "backup exists in UMDIR/store0"
else
  fail "no backup in UMDIR/store0"
fi

CACHE_AFTER=$(ls "$WORK/cache/"*-cache 2>/dev/null | head -1)
if [ -n "$CACHE_AFTER" ] && [ -n "${pre_size:-}" ]; then
  post_size=$(stat -c %s "$CACHE_AFTER")
  if [ "$post_size" -lt "$pre_size" ]; then
    pass "cache shrunk: $pre_size -> $post_size bytes"
  else
    fail "cache did not shrink ($pre_size -> $post_size)"
  fi
fi

# ---------- step 16: send 5 more, expect SQN continuation ----------
step "17) umesrc -M 5 (resume after restart)"
( cd "$WORK" && timeout 20 umesrc -c app.cfg -t store0 -M 5 -P 100 -l 256 -V -L 3 "$TEST_TOPIC" ) \
  > "$WORK/umesrc2.out" 2>&1 || true
if grep -q "OLD\[SQN" "$WORK/umesrc2.out"; then
  pass "store recognized source (OLD[SQN ...] flag set on registration)"
  grep "OLD\[" "$WORK/umesrc2.out" | head -1 | sed 's/^/        /'
else
  warn "no OLD[SQN] flag — store treated as new source (state-lifetime may have expired)"
fi
if grep -q "SQN 100" "$WORK/umesrc2.out"; then
  pass "registration continued at SQN 100"
elif grep -q "Sent 5 messages" "$WORK/umesrc2.out"; then
  warn "5 messages sent but SQN did not continue from 99 — store may have started fresh"
else
  fail "umesrc-2 did not complete:"
  tail -3 "$WORK/umesrc2.out" | sed 's/^/        /'
fi

# ---------- step 17: stop ----------
step "18) Clean shutdown (double Ctrl-C)"
# Send first SIGINT, wait briefly, send second to trigger fast-shutdown path.
kill -INT "$PARENT_PID"
sleep 1
kill -INT "$PARENT_PID" 2>/dev/null
# Parent should exit within ~10s (phase-2 SIGINT delivered to child)
for _ in 1 2 3 4 5 6 7 8 9 10; do
  sleep 1
  if ! kill -0 "$PARENT_PID" 2>/dev/null; then
    pass "parent exited on double-Ctrl-C"
    PARENT_PID=""
    break
  fi
done
if [ -n "${PARENT_PID:-}" ] && kill -0 "$PARENT_PID" 2>/dev/null; then
  warn "parent didn't exit in 10s; sending SIGKILL"
  kill -KILL "$PARENT_PID"
  PARENT_PID=""
fi

# ---------- summary ----------
step "Summary"
echo "  Final stats from store_maint.log:"
grep "STATS" "$WORK/store_maint.log" | sed 's/^/    /'
echo
if [ "$FAIL_COUNT" -eq 0 ]; then
  printf "${C_GRN}OK: all full-cycle checks passed${C_OFF}\n"
  exit 0
else
  printf "${C_RED}FAIL: %d check(s) failed${C_OFF}\n" "$FAIL_COUNT"
  exit 1
fi
