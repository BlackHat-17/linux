
#include <errno.h>
#include <fcntl.h>
#include <linux/cxl_mem.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

static unsigned int get_opcode(unsigned int id)
{
    switch (id) {
    case 1:  return 0x4000; /* IDENTIFY */
    case 3:  return 0x0400; /* GET_SUPPORTED_LOGS */
    case 4:  return 0x0200; /* GET_FW_INFO */
    case 5:  return 0x4100; /* GET_PARTITION_INFO */
    case 6:  return 0x4102; /* GET_LSA */
    case 7:  return 0x4200; /* GET_HEALTH_INFO */
    case 8:  return 0x0401; /* GET_LOG */
    case 9:  return 0x4101; /* SET_PARTITION_INFO */
    case 10: return 0x4103; /* SET_LSA */
    case 11: return 0x4201; /* GET_ALERT_CONFIG */
    case 12: return 0x4202; /* SET_ALERT_CONFIG */
    case 13: return 0x4203; /* GET_SHUTDOWN_STATE */
    case 14: return 0x4204; /* SET_SHUTDOWN_STATE */
    case 18: return 0x4303; /* GET_SCAN_MEDIA_CAPS */
    case 21: return 0x0300; /* GET_TIMESTAMP */
    case 22: return 0x0402; /* GET_LOG_CAPS */
    case 23: return 0x0403; /* CLEAR_LOG */
    case 24: return 0x0405; /* GET_SUPPORTED_LOG_SUBLIST */
    default: return 0;
    }
}

int main(int argc, char **argv)
{
    const char *dev = argc > 1 ? argv[1] : "/dev/cxl/mem0";
    struct cxl_mem_query_commands q = {0};
    int fd = open(dev, O_RDONLY);

    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (ioctl(fd, CXL_MEM_QUERY_COMMANDS, &q) < 0) {
        perror("CXL_MEM_QUERY_COMMANDS");
        close(fd);
        return 1;
    }

    unsigned int count = q.n_commands;
    printf("Command entries: %u\n", count);

    size_t bytes = sizeof(q) +
                   count * sizeof(struct cxl_command_info);

    struct cxl_mem_query_commands *full = calloc(1, bytes);
    if (!full) {
        perror("calloc");
        close(fd);
        return 1;
    }

    full->n_commands = count;

    if (ioctl(fd, CXL_MEM_QUERY_COMMANDS, full) < 0) {
        perror("CXL_MEM_QUERY_COMMANDS");
        free(full);
        close(fd);
        return 1;
    }

    for (unsigned int i = 0; i < full->n_commands; i++) {
        struct cxl_command_info *c = &full->commands[i];

        if (c->id == 0)
            continue;

        unsigned int opcode = get_opcode(c->id);

        if (opcode != 0) {
            printf("opcode=0x%04x flags=0x%x in=%u out=%u\n",
                   opcode, c->flags, c->size_in, c->size_out);
        } else {
            printf("id=%u opcode=UNKNOWN flags=0x%x in=%u out=%u\n",
                   c->id, c->flags, c->size_in, c->size_out);
        }
    }

    free(full);
    close(fd);
    return 0;
}
