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

#define MAX_COMMANDS 4096

static const char *command_name(uint32_t id)
{
    switch (id) {
    case CXL_MEM_COMMAND_ID_INVALID: return "INVALID";
    case CXL_MEM_COMMAND_ID_IDENTIFY: return "IDENTIFY";
    case CXL_MEM_COMMAND_ID_RAW: return "RAW";
    case CXL_MEM_COMMAND_ID_GET_SUPPORTED_LOGS: return "GET_SUPPORTED_LOGS";
    case CXL_MEM_COMMAND_ID_GET_FW_INFO: return "GET_FW_INFO";
    case CXL_MEM_COMMAND_ID_GET_PARTITION_INFO: return "GET_PARTITION_INFO";
    case CXL_MEM_COMMAND_ID_GET_LSA: return "GET_LSA";
    case CXL_MEM_COMMAND_ID_GET_HEALTH_INFO: return "GET_HEALTH_INFO";
    case CXL_MEM_COMMAND_ID_GET_LOG: return "GET_LOG";
    case CXL_MEM_COMMAND_ID_SET_PARTITION_INFO: return "SET_PARTITION_INFO";
    case CXL_MEM_COMMAND_ID_SET_LSA: return "SET_LSA";
    case CXL_MEM_COMMAND_ID_GET_ALERT_CONFIG: return "GET_ALERT_CONFIG";
    case CXL_MEM_COMMAND_ID_SET_ALERT_CONFIG: return "SET_ALERT_CONFIG";
    case CXL_MEM_COMMAND_ID_GET_SHUTDOWN_STATE: return "GET_SHUTDOWN_STATE";
    case CXL_MEM_COMMAND_ID_SET_SHUTDOWN_STATE: return "SET_SHUTDOWN_STATE";
    case CXL_MEM_COMMAND_ID_GET_SCAN_MEDIA_CAPS: return "GET_SCAN_MEDIA_CAPS";
    case CXL_MEM_COMMAND_ID_GET_TIMESTAMP: return "GET_TIMESTAMP";
    case CXL_MEM_COMMAND_ID_GET_LOG_CAPS: return "GET_LOG_CAPS";
    case CXL_MEM_COMMAND_ID_CLEAR_LOG: return "CLEAR_LOG";
    case CXL_MEM_COMMAND_ID_GET_SUP_LOG_SUBLIST: return "GET_SUP_LOG_SUBLIST";
    default: return "UNKNOWN_OR_DEPRECATED";
    }
}

static void print_size(uint32_t size)
{
    if (size == UINT32_MAX)
        printf("variable");
    else
        printf("%" PRIu32, size);
}

static void print_flags(uint32_t flags)
{
    printf("0x%08" PRIx32 " [", flags);

    if (flags & 0x1)
        printf("ENABLED: %d",flags);
    else
        printf("disabled: %d",flags);

    if (flags & 0x2)
        printf("%sEXCLUSIVE", flags & 0x1 ? "|" : "");

    printf("]");
}

int main(int argc, char **argv)
{
    const char *device = argc > 1 ? argv[1] : "/dev/cxl/mem0";
    int fd = open(device, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "ERROR: open(%s): %s\n", device, strerror(errno));
        return 1;
    }

    struct cxl_mem_query_commands first = {0};
    if (ioctl(fd, CXL_MEM_QUERY_COMMANDS, &first) < 0) {
        fprintf(stderr, "ERROR: first CXL_MEM_QUERY_COMMANDS ioctl: %s\n",
                strerror(errno));
        close(fd);
        return 1;
    }

    uint32_t capacity = first.n_commands;
    if (capacity == 0 || capacity > MAX_COMMANDS) {
        fprintf(stderr, "ERROR: unexpected command count %u\n", capacity);
        close(fd);
        return 1;
    }

    size_t allocation = sizeof(struct cxl_mem_query_commands) +
                        (size_t)capacity * sizeof(struct cxl_command_info);
    struct cxl_mem_query_commands *query = calloc(1, allocation);
    if (!query) {
        perror("calloc");
        close(fd);
        return 1;
    }
    query->n_commands = capacity;

    if (ioctl(fd, CXL_MEM_QUERY_COMMANDS, query) < 0) {
        fprintf(stderr, "ERROR: second CXL_MEM_QUERY_COMMANDS ioctl: %s\n",
                strerror(errno));
        free(query);
        close(fd);
        return 1;
    }

    uint32_t returned = query->n_commands;
    if (returned > capacity) {
        fprintf(stderr,
                "ERROR: kernel returned %u entries, but buffer capacity is %u\n",
                returned, capacity);
        free(query);
        close(fd);
        return 1;
    }

    printf("Device: %s\n", device);
    printf("Commands returned: %" PRIu32 "\n\n", returned);
    printf("%-6s %-30s %-12s %-24s %-12s %-12s\n",
           "INDEX", "COMMAND_NAME", "COMMAND_ID", "FLAGS",
           "INPUT_SIZE", "OUTPUT_SIZE");
    printf("--------------------------------------------------------------------------------------------------------------\n");

    for (uint32_t i = 0; i < returned; i++) {
        const struct cxl_command_info *c = &query->commands[i];
        printf("%-6" PRIu32 " %-30s 0x%08" PRIx32 " ",
               i, command_name(c->id), c->id);
        print_flags(c->flags);
        printf(" ");
        print_size(c->size_in);
        printf("           ");
        print_size(c->size_out);
        printf("\n");
    }

    printf("\nNotes:\n");
    printf("- Command IDs are Linux ioctl command IDs, not raw CCI opcodes.\n");
    printf("- 0xffffffff denotes a variable-size payload.\n");
    printf("- ENABLED and EXCLUSIVE flags are reported by the kernel driver.\n");
    printf("- This program only queries supported commands; it sends no CCI command.\n");

    free(query);
    close(fd);
    return 0;
}

