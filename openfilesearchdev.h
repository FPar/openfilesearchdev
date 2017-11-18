#ifndef OPENFILESEARCHDEV_H
#define OPENFILESEARCHDEV_H

#include <linux/ioctl.h>

#ifdef MODULE
#include <linux/types.h>
#else
#include <sys/types.h>
#endif

#define IOCTL_TYPE 233

/*
 * Find all a process's open() files.
 */
#define OFS_PID _IOW(IOCTL_TYPE, 0, unsigned int)

/*
 * Find all the files a user has open().
 */
#define OFS_UID _IOW(IOCTL_TYPE, 1, unsigned int)

/*
 * Find all open() files that are owned by a user.
 */
#define OFS_OWNER _IOW(IOCTL_TYPE, 2, unsigned int)

/*
 * Find all files with a given name.
 */
#define OFS_NAME _IOW(IOCTL_TYPE, 3, char*)

#define DEVICE_FILE_NAME "/dev/openFileSearchDev"

#define OFS_PATH_LENGTH 64
struct ofs_result {
    pid_t pid;
    uid_t uid;
    uid_t owner;
    unsigned short permissions;
    char name[OFS_PATH_LENGTH];
    unsigned int fsize;
    unsigned long inode_no;
};

#endif