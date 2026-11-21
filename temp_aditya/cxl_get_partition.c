#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/cxl_mem.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define PARTITION_RESPONSE_SIZE 32U

/* Decode a little-endian 64-bit integer without alignment assumptions. */
static uint64_t read_le64(const uint8_t *p)
{
    uint64_t value = 0;
    for (unsigned int i = 0; i < 8; i++)
        value |= (uint64_t)p[i] << (8U * i);
    return value;
}

static void print_hex(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (i % 16 == 0)
            printf("  %04zx: ", i);
        printf("%02x ", buf[i]);
        if (i % 16 == 15 || i + 1 == len)
            putchar('\n');
    }
}

static int command_is_enabled(int fd, uint32_t wanted_id)
{
    struct cxl_mem_query_commands first = {0};

    if (ioctl(fd, CXL_MEM_QUERY_COMMANDS, &first) < 0) {
        fprintf(stderr, "QUERY_COMMANDS failed: %s\n", strerror(errno));
        return -1;
    }

    uint32_t capacity = first.n_commands;
    if (capacity == 0 || capacity > 4096) {
        fprintf(stderr, "Unexpected command count: %u\n", capacity);
        return -1;
    }

    size_t bytes = sizeof(struct cxl_mem_query_commands) +
                   (size_t)capacity * sizeof(struct cxl_command_info);
    struct cxl_mem_query_commands *q = calloc(1, bytes);
    if (!q) {
        perror("calloc");
        return -1;
    }
    q->n_commands = capacity;

    if (ioctl(fd, CXL_MEM_QUERY_COMMANDS, q) < 0) {
        fprintf(stderr, "QUERY_COMMANDS (list) failed: %s\n",
                strerror(errno));
        free(q);
        return -1;
    }

    uint32_t n = q->n_commands;
    if (n > capacity) {
        fprintf(stderr, "Kernel returned invalid count %u (capacity %u)\n",
                n, capacity);
        free(q);
        return -1;
    }

    int enabled = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (q->commands[i].id == wanted_id) {
            enabled = !!(q->commands[i].flags & 0x1U);
            break;
        }
    }

    free(q);
    return enabled;
}

int main(int argc, char **argv)
{
    const char *device = argc > 1 ? argv[1] : "/dev/cxl/mem0";
    uint8_t response[PARTITION_RESPONSE_SIZE] = {0};

    int fd = open(device, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "Cannot open %s: %s\n", device, strerror(errno));
        return 1;
    }

    int enabled = command_is_enabled(fd, CXL_MEM_COMMAND_ID_GET_PARTITION_INFO);
    if (enabled < 0) {
        close(fd);
        return 1;
    }
    if (!enabled) {
        fprintf(stderr,
                "GET_PARTITION_INFO (Linux command ID 0x%02x) is not enabled "
                "by the kernel/device for userspace on %s.\n",
                CXL_MEM_COMMAND_ID_GET_PARTITION_INFO, device);
        close(fd);
        return 2;
    }

    struct cxl_send_command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.id = CXL_MEM_COMMAND_ID_GET_PARTITION_INFO;
    cmd.in.size = 0;
    cmd.in.payload = 0; /* No request payload for this command. */
    cmd.out.size = sizeof(response);
    cmd.out.payload = (uint64_t)(uintptr_t)response;

    printf("Device: %s\n", device);
    printf("Operation: GET_PARTITION_INFO\n");
    printf("Linux ioctl command ID: 0x%08x\n", cmd.id);
    printf("Expected CCI command: set 0x41, opcode 0x00 (often shown as 0x4100)\n");
    printf("Request payload length: %u bytes\n", cmd.in.size);
    printf("Response buffer capacity: %u bytes\n\n", cmd.out.size);

    if (ioctl(fd, CXL_MEM_SEND_COMMAND, &cmd) < 0) {
        fprintf(stderr, "SEND_COMMAND failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    printf("Device return code (cmd.retval): 0x%08x\n", cmd.retval);
    printf("Response payload length reported by kernel: %u bytes\n",
           cmd.out.size);

    if (cmd.out.size > sizeof(response)) {
        fprintf(stderr,
                "ERROR: kernel reported %u response bytes, buffer is only %zu\n",
                cmd.out.size, sizeof(response));
        close(fd);
        return 1;
    }

    printf("\nRaw response payload (hex):\n");
    print_hex(response, cmd.out.size);

    if (cmd.out.size < PARTITION_RESPONSE_SIZE) {
        fprintf(stderr,
                "\nResponse is shorter than the expected 32-byte partition "
                "information structure; not decoding fields.\n");
        close(fd);
        return 1;
    }

    uint64_t active_volatile = read_le64(&response[0]);
    uint64_t active_persistent = read_le64(&response[8]);
    uint64_t next_volatile = read_le64(&response[16]);
    uint64_t next_persistent = read_le64(&response[24]);

    printf("\nDecoded partition response (bytes):\n");
    printf("  Active Volatile Capacity:   %" PRIu64 "\n", active_volatile);
    printf("  Active Persistent Capacity: %" PRIu64 "\n", active_persistent);
    printf("  Next Volatile Capacity:     %" PRIu64 "\n", next_volatile);
    printf("  Next Persistent Capacity:   %" PRIu64 "\n", next_persistent);

    printf("\nDecoded partition response (MiB; values divided by 2^20):\n");
    printf("  Active Volatile Capacity:   %.2f MiB\n",
           (double)active_volatile / (1024.0 * 1024.0));
    printf("  Active Persistent Capacity: %.2f MiB\n",
           (double)active_persistent / (1024.0 * 1024.0));
    printf("  Next Volatile Capacity:     %.2f MiB\n",
           (double)next_volatile / (1024.0 * 1024.0));
    printf("  Next Persistent Capacity:   %.2f MiB\n",
           (double)next_persistent / (1024.0 * 1024.0));

    printf("\nNotes:\n");
    printf("- Linux command ID 0x05 is not the raw CCI opcode.\n");
    printf("- The command is read-only; this program does not change partitions.\n");
    printf("- Compare these four fields with cxl list -I for the same memdev.\n");

    close(fd);
    return 0;
}

