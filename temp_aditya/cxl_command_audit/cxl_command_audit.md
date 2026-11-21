# CXL command source audit

- **Linux tree:** \
- **QEMU tree:** \
- **Raw search output:** \

> Static source inspection only. A source match does not prove a command was executed or received at runtime.

## Command-by-command checklist

| Linux ID (observed) | Command | UAPI ID found? | Kernel table found? | Opcode definition found? | QEMU reference found? | Manual/runtime status |
|---:|---|---|---|---|---|---|
| 1 | \ | NO | YES | YES | YES (text match; inspect context) | [ ] Check implementation and runtime |
| 3 | \ | NO | YES | YES | NOT FOUND | [ ] Check implementation and runtime |
| 4 | \ | NO | YES | YES | NOT FOUND | [ ] Check implementation and runtime |
| 5 | \ | NO | YES | YES | YES (text match; inspect context) | [ ] Check implementation and runtime |
| 6 | \ | NO | YES | YES | YES (text match; inspect context) | [ ] Check implementation and runtime |
| 7 | \ | NO | YES | YES | NOT FOUND | [ ] Check implementation and runtime |
| 8 | \ | NO | YES | YES | YES (text match; inspect context) | [ ] Check implementation and runtime |
| 9 | \ | NO | YES | YES | NOT FOUND | [ ] Check implementation and runtime |
| 10 | \ | NO | YES | YES | NOT FOUND | [ ] Check implementation and runtime |
| 11 | \ | NO | YES | YES | NOT FOUND | [ ] Check implementation and runtime |
| 12 | \ | NO | YES | YES | NOT FOUND | [ ] Check implementation and runtime |
| 13 | \ | NO | YES | YES | YES (text match; inspect context) | [ ] Check implementation and runtime |
| 14 | \ | NO | YES | YES | YES (text match; inspect context) | [ ] Check implementation and runtime |
| 15 | \ | NO | YES | YES | YES (text match; inspect context) | [ ] Check implementation and runtime |
| 16 | \ | NO | YES | YES | NOT FOUND | [ ] Check implementation and runtime |
| 17 | \ | NO | YES | YES | NOT FOUND | [ ] Check implementation and runtime |
| 18 | \ | NO | YES | YES | NOT FOUND | [ ] Check implementation and runtime |
| 21 | \ | NO | YES | YES | NOT FOUND | [ ] Check implementation and runtime |

## How to interpret the results

- **UAPI ID found:** inspect the enum in `include/uapi/linux/cxl_mem.h`; IDs can vary by kernel revision.
- **Kernel table found:** inspect the matching `CXL_CMD(...)` entry in `drivers/cxl/core/mbox.c`.
- **Opcode definition found:** inspect the numeric `CXL_MBOX_OP_...` constant in `drivers/cxl/cxlmem.h`.
- **QEMU text match:** a search hit only. Confirm the surrounding code actually dispatches the mailbox opcode.
- **Runtime verification:** log the opcode at kernel mailbox submission and QEMU mailbox dispatch, then compare values.

## Important source paths

- \ — Linux userspace command IDs and ioctl structures
- \ — command table, ID-to-opcode mapping, validation, submission
- \ — CCI opcode constants
- \ — QEMU CXL device implementation

## Runtime logging note

Your source already logs the opcode in `handle_mailbox_cmd_from_user()` using `dev_dbg()`, before `cxl_mbox->mbox_send(cxl_mbox, mbox_cmd)`. Dynamic debug must be enabled and available. The query-only ioctl does not submit a mailbox command.
