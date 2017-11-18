#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "openfilesearchdev.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <ioctl_cmd> <ioctl_arg>\n", *argv);
        return -1;
    }

    int fd = open(DEVICE_FILE_NAME, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    char *ioctl_cmd_str = argv[1];
    unsigned long ioctl_cmd;
    unsigned int uint_arg;
    char *str_arg;
    void *ioctl_arg;
    if (strcmp("OFS_PID", ioctl_cmd_str) == 0) {
        ioctl_cmd = OFS_PID;
        uint_arg = (unsigned int) atoi(argv[2]);
        ioctl_arg = &uint_arg;
    } else if (strcmp("OFS_UID", ioctl_cmd_str) == 0) {
        ioctl_cmd = OFS_UID;
        uint_arg = (unsigned int) atoi(argv[2]);
        ioctl_arg = &uint_arg;
    } else if (strcmp("OFS_OWNER", ioctl_cmd_str) == 0) {
        ioctl_cmd = OFS_OWNER;
        uint_arg = (unsigned int) atoi(argv[2]);
        ioctl_arg = &uint_arg;
    } else if (strcmp("OFS_NAME", ioctl_cmd_str) == 0) {
        ioctl_cmd = OFS_NAME;
        str_arg = argv[2];
        ioctl_arg = str_arg;
    } else {
        fprintf(stderr, "Unknown ioctl command %s\n", ioctl_cmd_str);
        exit(-1);
    }

    if ((ioctl(fd, ioctl_cmd, ioctl_arg))) {
        fprintf(stderr, "fail ioctl %s: %s\n", ioctl_cmd_str, strerror(errno));
        exit(-1);
    } else {
        printf("ioctl %s\n", ioctl_cmd_str);
    }

    struct ofs_result results[256];
    ssize_t result_count;
    if ((result_count = read(fd, results, 256)) < 0) {
        perror("read");
    } else {
        printf("read: %ld results\n", result_count);

        for (int result_index = 0; result_index < result_count; result_index++) {
            struct ofs_result *result = &results[result_index];
            printf("         %3d: %s\n              pid: %d, uid: %d, owner: %d, permissions: %u, fsize: %u, inode_no: %lu\n",
                   result_index, result->name, result->pid, result->uid, result->owner, result->permissions,
                   result->fsize, result->inode_no);
        }
    }

    if (close(fd)) {
        perror("close");
        return -1;
    }
    return 0;
}