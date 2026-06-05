#!/bin/bash
#
# maintain_store.sh - UME Store maintenance script
#
# Performs the standard maintenance procedure on one or more store instances:
#   1. Cleanup zero-message state and cache files
#   2. Dump last message state (pre-prune snapshot)
#   3. Prune messages older than current timestamp
#   4. Dump last message state (post-prune snapshot)
#   5. Verify "highest message sequence number" matches before/after
#
# Usage:
#   maintain_store.sh -r UMESNAPREPO_PATH -d BACKUP_DIR -l LOG_FILE
#                     -s STATE_DIR1 -c CACHE_DIR1 -n STORE_NAME1
#                     [-s STATE_DIR2 -c CACHE_DIR2 -n STORE_NAME2 ...]
#
# Exit codes:
#   0 = all stores maintained and verified successfully
#   1 = verification failed for one or more stores
#   2 = usage/argument error
#   3 = umesnaprepo execution error
#

set -euo pipefail

UMESNAPREPO=""
BACKUP_DIR="./UMDIR"
LOG_FILE=""
WORK_DIR=""
UTC_TIMESTAMPS=0

# Arrays for store info
declare -a STATE_DIRS=()
declare -a CACHE_DIRS=()
declare -a STORE_NAMES=()

log_msg() {
    local timestamp
    if [ "$UTC_TIMESTAMPS" -eq 1 ]; then
        timestamp=$(date -u '+%Y-%m-%d %H:%M:%S UTC')
    else
        timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    fi
    local msg="[$timestamp] $1"
    echo "$msg"
    if [ -n "$LOG_FILE" ]; then
        echo "$msg" >> "$LOG_FILE"
    fi
}

# Prefix each line of stdin with a timestamp, filter known benign warnings
ts_prefix() {
    while IFS= read -r line; do
        case "$line" in
            *"Backup directory"*"already exists"*) continue ;;
        esac
        if [ "$UTC_TIMESTAMPS" -eq 1 ]; then
            printf "[%s]   %s\n" "$(date -u '+%Y-%m-%d %H:%M:%S UTC')" "$line"
        else
            printf "[%s]   %s\n" "$(date '+%Y-%m-%d %H:%M:%S')" "$line"
        fi
    done
}

log_error() {
    log_msg "ERROR: $1" >&2
}

usage() {
    echo "Usage: $0 -r UMESNAPREPO_PATH -d BACKUP_DIR -l LOG_FILE \\"
    echo "          -s STATE_DIR -c CACHE_DIR -n STORE_NAME [...]"
    echo ""
    echo "Options:"
    echo "  -r PATH    Path to umesnaprepo binary"
    echo "  -d DIR     Backup directory for old files (default: ./UMDIR)"
    echo "  -l FILE    Log file path"
    echo "  -w DIR     Working directory for temp files (default: /tmp)"
    echo "  -u         Log timestamps in UTC (default: local time)"
    echo "  -s DIR     State directory (repeat for multiple stores)"
    echo "  -c DIR     Cache directory (repeat for multiple stores)"
    echo "  -n NAME    Store name (repeat for multiple stores)"
    exit 2
}

# Parse arguments
current_state=""
current_cache=""
current_name=""
store_idx=0

while getopts "r:d:l:w:us:c:n:" opt; do
    case $opt in
        r) UMESNAPREPO="$OPTARG" ;;
        d) BACKUP_DIR="$OPTARG" ;;
        l) LOG_FILE="$OPTARG" ;;
        w) WORK_DIR="$OPTARG" ;;
        u) UTC_TIMESTAMPS=1 ;;
        s)
            STATE_DIRS+=("$OPTARG")
            ;;
        c)
            CACHE_DIRS+=("$OPTARG")
            ;;
        n)
            STORE_NAMES+=("$OPTARG")
            ;;
        *) usage ;;
    esac
done

# Validate
if [ -z "$UMESNAPREPO" ]; then
    log_error "umesnaprepo path (-r) is required"
    usage
fi

if [ ! -x "$UMESNAPREPO" ]; then
    log_error "umesnaprepo not found or not executable: $UMESNAPREPO"
    exit 3
fi

num_stores=${#STATE_DIRS[@]}
if [ "$num_stores" -eq 0 ]; then
    log_error "At least one state directory (-s) is required"
    usage
fi

if [ "${#CACHE_DIRS[@]}" -ne "$num_stores" ]; then
    log_error "Number of cache dirs (-c) must match number of state dirs (-s)"
    exit 2
fi

if [ "${#STORE_NAMES[@]}" -ne "$num_stores" ]; then
    log_error "Number of store names (-n) must match number of state dirs (-s)"
    exit 2
fi

if [ -z "$WORK_DIR" ]; then
    WORK_DIR=$(mktemp -d /tmp/store_maint.XXXXXX)
    trap "rm -rf $WORK_DIR" EXIT
else
    mkdir -p "$WORK_DIR"
fi

# Create backup directory
mkdir -p "$BACKUP_DIR"

overall_result=0

log_msg "=== Store Maintenance Started ==="
log_msg "Stores to process: $num_stores"
log_msg "Backup directory: $BACKUP_DIR"

for ((i=0; i<num_stores; i++)); do
    sdir="${STATE_DIRS[$i]}"
    cdir="${CACHE_DIRS[$i]}"
    sname="${STORE_NAMES[$i]}"

    log_msg "--- Processing store: $sname ---"
    log_msg "  State dir: $sdir"
    log_msg "  Cache dir: $cdir"

    store_backup="${BACKUP_DIR}/${sname}"
    mkdir -p "$store_backup"

    prior_file="${WORK_DIR}/statelogprior_${sname}.txt"
    post_file="${WORK_DIR}/statelogpost_${sname}.txt"

    # Build cache args (cache dir may be empty)
    cache_args=""
    if [ -n "$cdir" ]; then
        cache_args="-c $cdir"
    fi

    # Clean per-store backup dir so umesnaprepo doesn't warn about it existing
    rm -rf "$store_backup"

    # Step 1: Cleanup zero-message state and cache files
    log_msg "  Step 1: Cleaning zero-message files..."
    step1_out="${WORK_DIR}/step1_${sname}.txt"
    if ! "$UMESNAPREPO" -s "$sdir" $cache_args -d "$store_backup" -m0 > "$step1_out" 2>&1; then
        # "no state files found" means the store is empty — not an error, skip it
        if grep -qi "no state files found" "$step1_out" 2>/dev/null; then
            cat "$step1_out" | ts_prefix || true
            log_msg "  [WARN] No state files found for store $sname — nothing to do, skipping."
            continue
        fi
        cat "$step1_out" | ts_prefix || true
        log_error "  umesnaprepo -m0 failed for store $sname"
        overall_result=3
        continue
    fi
    cat "$step1_out" | ts_prefix || true

    # Step 2: Dump pre-prune state
    log_msg "  Step 2: Dumping pre-prune state..."
    if ! "$UMESNAPREPO" -s "$sdir" $cache_args -l > "$prior_file" 2>&1; then
        log_error "  umesnaprepo -l (pre-prune) failed for store $sname"
        overall_result=3
        continue
    fi

    # Step 3: Prune (keep only last message)
    prune_ts=$(date +%s)
    log_msg "  Step 3: Pruning messages older than timestamp $prune_ts..."
    if ! "$UMESNAPREPO" -s "$sdir" $cache_args -d "$store_backup" -P"$prune_ts" 2>&1 | ts_prefix; then
        log_error "  umesnaprepo -P failed for store $sname"
        overall_result=3
        continue
    fi

    # Step 4: Dump post-prune state
    log_msg "  Step 4: Dumping post-prune state..."
    if ! "$UMESNAPREPO" -s "$sdir" $cache_args -l > "$post_file" 2>&1; then
        log_error "  umesnaprepo -l (post-prune) failed for store $sname"
        overall_result=3
        continue
    fi

    # Step 5: Verify highest message sequence numbers match
    log_msg "  Step 5: Verifying highest message sequence numbers..."
    diff1="${WORK_DIR}/diff1_${sname}.txt"
    diff2="${WORK_DIR}/diff2_${sname}.txt"

    grep -e "-cache" -e "highest message sequence number" "$prior_file" > "$diff1" 2>/dev/null || true
    grep -e "-cache" -e "highest message sequence number" "$post_file"  > "$diff2" 2>/dev/null || true

    if diff -q "$diff1" "$diff2" > /dev/null 2>&1; then
        log_msg "  PASS: Highest message sequence numbers match for store $sname"

        # Log trimming summary
        msg_prior="${WORK_DIR}/msg_prior_${sname}.txt"
        msg_post="${WORK_DIR}/msg_post_${sname}.txt"
        grep -e "-cache" -e "highest message sequence number" -e "number of messages" \
            "$prior_file" > "$msg_prior" 2>/dev/null || true
        grep -e "-cache" -e "highest message sequence number" -e "number of messages" \
            "$post_file" > "$msg_post" 2>/dev/null || true
        log_msg "  Pre-prune message counts:"
        while IFS= read -r line; do
            log_msg "    $line"
        done < "$msg_prior"
        log_msg "  Post-prune message counts:"
        while IFS= read -r line; do
            log_msg "    $line"
        done < "$msg_post"
    else
        log_error "  FAIL: Highest message sequence number mismatch for store $sname"
        log_msg "  Pre-prune:"
        while IFS= read -r line; do
            log_msg "    $line"
        done < "$diff1"
        log_msg "  Post-prune:"
        while IFS= read -r line; do
            log_msg "    $line"
        done < "$diff2"
        overall_result=1
    fi

    log_msg "--- Finished store: $sname ---"
done

if [ "$overall_result" -eq 0 ]; then
    log_msg "=== Store Maintenance Completed Successfully ==="
else
    log_msg "=== Store Maintenance Completed With Errors (exit=$overall_result) ==="
fi

exit $overall_result
