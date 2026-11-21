#!/usr/bin/env bash
# Static source audit for Linux CXL command IDs, CCI opcodes, and QEMU references.
# Usage: ./cxl_command_audit.sh [LINUX_SOURCE_DIR] [QEMU_SOURCE_DIR] [OUTPUT_DIR]
# This script searches source code; it does not prove runtime execution.
set -uo pipefail

LINUX_DIR="${1:-$HOME/linux}"
QEMU_DIR="${2:-$HOME/qemu}"
OUT_DIR="${3:-$PWD/cxl_command_audit}"
MB0X="$LINUX_DIR/drivers/cxl/core/mbox.c"
UAPI="$LINUX_DIR/include/uapi/linux/cxl_mem.h"
OPCODES="$LINUX_DIR/drivers/cxl/cxlmem.h"
QEMU_CXL="$QEMU_DIR/hw/cxl"

for f in "$MB0X" "$UAPI" "$OPCODES"; do
    if [[ ! -f "$f" ]]; then
        echo "ERROR: Required Linux source file not found: $f" >&2
        exit 1
    fi
done
[[ -d "$QEMU_CXL" ]] || echo "WARNING: QEMU CXL directory not found: $QEMU_CXL" >&2
mkdir -p "$OUT_DIR"
REPORT="$OUT_DIR/cxl_command_audit.md"
RAW="$OUT_DIR/source_searches.txt"

# IDs and names observed in the user's kernel command table. The report checks
# these against the actual source so differences across kernel revisions surface.
cat > "$OUT_DIR/observed_commands.tsv" <<'LIST'
id	name
1	IDENTIFY
3	GET_SUPPORTED_LOGS
4	GET_FW_INFO
5	GET_PARTITION_INFO
6	GET_LSA
7	GET_HEALTH_INFO
8	GET_LOG
9	GET_LOG_CAPS
10	CLEAR_LOG
11	GET_SUP_LOG_SUBLIST
12	SET_PARTITION_INFO
13	SET_LSA
14	GET_ALERT_CONFIG
15	SET_ALERT_CONFIG
16	GET_SHUTDOWN_STATE
17	SET_SHUTDOWN_STATE
18	GET_SCAN_MEDIA_CAPS
21	GET_TIMESTAMP
LIST

{
    echo '=== Linux command table ==='
    grep -n -E 'CXL_CMD\(|cxl_mem_commands\[' "$MB0X" || true
    echo; echo '=== UAPI command IDs and ioctl definitions ==='
    grep -n -E 'CXL_MEM_COMMAND_ID_|CXL_MEM_SEND_COMMAND|CXL_MEM_QUERY_COMMANDS' "$UAPI" || true
    echo; echo '=== CCI opcode definitions ==='
    grep -n -E 'CXL_MBOX_OP_[A-Z0-9_]+' "$OPCODES" || true
    echo; echo '=== Kernel userspace submission path ==='
    grep -n -A70 'handle_mailbox_cmd_from_user' "$MB0X" || true
    echo; echo '=== QEMU CXL source references ==='
    if [[ -d "$QEMU_CXL" ]]; then
        grep -R -n -E 'IDENTIFY|GET_SUPPORTED_LOGS|GET_FW_INFO|GET_PARTITION_INFO|GET_LSA|GET_HEALTH_INFO|GET_LOG|GET_LOG_CAPS|CLEAR_LOG|GET_SUP_LOG_SUBLIST|SET_PARTITION_INFO|SET_LSA|GET_ALERT_CONFIG|SET_ALERT_CONFIG|GET_SHUTDOWN_STATE|SET_SHUTDOWN_STATE|GET_SCAN_MEDIA_CAPS|GET_TIMESTAMP|opcode|mailbox' "$QEMU_CXL" || true
    else
        echo 'QEMU CXL source directory unavailable.'
    fi
} > "$RAW"

{
    echo '# CXL command source audit'; echo
    echo "- **Linux tree:** \\`$LINUX_DIR\\`"
    echo "- **QEMU tree:** \\`$QEMU_DIR\\`"
    echo "- **Raw search output:** \\`$RAW\\`"; echo
    echo '> Static source inspection only. A source match does not prove a command was executed or received at runtime.'; echo
    echo '## Command-by-command checklist'; echo
    echo '| Linux ID (observed) | Command | UAPI ID found? | Kernel table found? | Opcode definition found? | QEMU reference found? | Manual/runtime status |'
    echo '|---:|---|---|---|---|---|---|'
    while IFS=$'\t' read -r id name; do
        [[ "$id" == 'id' ]] && continue
        uapi_match='NO'; table_match='NO'; opcode_match='NO'; qemu_match='NOT FOUND'
        grep -Eq "CXL_MEM_COMMAND_ID_${name}([,[:space:]]|=)" "$UAPI" && uapi_match='YES'
        grep -Eq "CXL_CMD\\(${name}," "$MB0X" && table_match='YES'
        grep -Eq "CXL_MBOX_OP_${name}[[:space:]]*=" "$OPCODES" && opcode_match='YES'
        if [[ -d "$QEMU_CXL" ]] && grep -R -q -E "$name|CXL_MBOX_OP_${name}" "$QEMU_CXL"; then
            qemu_match='YES (text match; inspect context)'
        fi
        echo "| $id | \\`$name\\` | $uapi_match | $table_match | $opcode_match | $qemu_match | [ ] Check implementation and runtime |"
    done < "$OUT_DIR/observed_commands.tsv"
    echo; echo '## How to interpret the results'; echo
    echo '- **UAPI ID found:** inspect the enum in `include/uapi/linux/cxl_mem.h`; IDs can vary by kernel revision.'
    echo '- **Kernel table found:** inspect the matching `CXL_CMD(...)` entry in `drivers/cxl/core/mbox.c`.'
    echo '- **Opcode definition found:** inspect the numeric `CXL_MBOX_OP_...` constant in `drivers/cxl/cxlmem.h`.'
    echo '- **QEMU text match:** a search hit only. Confirm the surrounding code actually dispatches the mailbox opcode.'
    echo '- **Runtime verification:** log the opcode at kernel mailbox submission and QEMU mailbox dispatch, then compare values.'
    echo; echo '## Important source paths'; echo
    echo "- \\`$UAPI\\` — Linux userspace command IDs and ioctl structures"
    echo "- \\`$MB0X\\` — command table, ID-to-opcode mapping, validation, submission"
    echo "- \\`$OPCODES\\` — CCI opcode constants"
    echo "- \\`$QEMU_CXL\\` — QEMU CXL device implementation"
    echo; echo '## Runtime logging note'; echo
    echo 'Your source already logs the opcode in `handle_mailbox_cmd_from_user()` using `dev_dbg()`, before `cxl_mbox->mbox_send(cxl_mbox, mbox_cmd)`. Dynamic debug must be enabled and available. The query-only ioctl does not submit a mailbox command.'
} > "$REPORT"

echo 'Audit complete.'
echo "Markdown report: $REPORT"
echo "Raw source matches: $RAW"
echo "Observed command list: $OUT_DIR/observed_commands.tsv"
