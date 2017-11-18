#include "openfilesearchdev.h"

#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/fdtable.h>
#include <linux/sched/signal.h>
#include <linux/uaccess.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>

#define DRIVER_AUTHOR "Fabian Parzefall <fabian.parzefall@hm.edu>"
#define DRIVER_DESC   "A pseudo driver for searching open files."
#define DEVICE_NAME "openFileSearchDev"

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_SUPPORTED_DEVICE(DEVICE_NAME);

#define DEBUG


static ssize_t ofs_read(struct file *file, char __user *buffer, size_t length, loff_t *offset);

static long ofs_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param);

static int ofs_open(struct inode *inode, struct file *file);

static int ofs_release(struct inode *inode, struct file *file);


static struct file_operations Fops = {
        .read = ofs_read,
        .unlocked_ioctl = ofs_ioctl,
        .open = ofs_open,
        .release = ofs_release
};


static atomic_t Device_Open = ATOMIC_INIT(0);

#define RESULT_BUFFER 256

static int result_count = -1;
static int results_read;
struct ofs_result ofs_results[RESULT_BUFFER];


static unsigned int Major;

int init_module() {
    int reg_result = register_chrdev(0, DEVICE_NAME, &Fops);

    if (reg_result < 0) {
        printk(KERN_ALERT "%s failed with %d\n",
               "Sorry, registering the character device ", reg_result);
        return reg_result;
    }

    Major = (unsigned int) reg_result;

    pr_info("Registered %s with device number %d, to create a device file run:\n", DEVICE_NAME, Major);
    pr_info("sudo mknod %s c %d 0\n", DEVICE_FILE_NAME, Major);

    return 0;
}

void cleanup_module() {
    unregister_chrdev(Major, DEVICE_NAME);
}


int ofs_open(struct inode *inode, struct file *file) {
#ifdef DEBUG
    pr_info("device_open(%p)\n", file);
#endif

    if (atomic_inc_return(&Device_Open) > 1) {
        atomic_dec(&Device_Open);
        return -EBUSY;
    }

    try_module_get(THIS_MODULE);
    return 0;
}

int ofs_release(struct inode *inode, struct file *file) {
#ifdef DEBUG
    pr_info("device_release(%p,%p)\n", inode, file);
#endif

    atomic_dec(&Device_Open);

    module_put(THIS_MODULE);
    return 0;
}

ssize_t ofs_read(struct file *file, char __user *buffer, size_t length, loff_t *offset) {
    int remaining_results;
    size_t buf_len;

#ifdef DEBUG
    pr_info("device_read(%p,%p,%ld)\n", file, buffer, length);
#endif

    // No search so far.
    if (result_count == -1) {
#ifdef DEBUG
        pr_info("read before ioctl\n");
#endif

        return -ESRCH;
    }

    remaining_results = result_count - results_read;

    if (length > remaining_results) {
        length = (size_t) remaining_results;
    }

    buf_len = length * sizeof(struct ofs_result);
    if(copy_to_user(buffer, ofs_results + results_read, buf_len))
        return -EFAULT;
    results_read += length;

    return length;
}

void add_file(struct task_struct *task, struct file *file) {
    struct ofs_result *result;
    char *buf;
    char *path;

    result = &ofs_results[result_count];

    result->pid = task->pid;
    result->uid = task->cred->uid.val;

    if (file->f_inode) {
        struct inode *i = file->f_inode;
        result->owner = i->i_uid.val;
        result->permissions = i->i_mode;
        result->fsize = i->i_size;
        result->inode_no = i->i_ino;
    } else {
        result->owner = 0;
        result->permissions = 0;
        result->fsize = 0;
        result->inode_no = 0;
    }

    buf = kmalloc(OFS_PATH_LENGTH, GFP_KERNEL);
    path = d_path(&file->f_path, buf, OFS_PATH_LENGTH);
    if (!IS_ERR(path)) {
        strcpy(result->name, path);
    } else {
        result->name[0] = 0;
    }
    kfree(buf);
}

void add_task(struct task_struct *task) {
    struct files_struct *filess;
    struct fdtable *fdtable;
    int i = 0;

    if (!task) {
        return;
    }

    filess = task->files;

    fdtable = files_fdtable(filess);

    while (i < fdtable->max_fds && result_count < RESULT_BUFFER) {
        struct file *file = fdtable->fd[i];
        if (file) {
            add_file(task, file);
            ++result_count;
        }
        ++i;
    }
}

void add_task_filter_owner(struct task_struct *task, unsigned int owner) {
    struct files_struct *filess;
    struct fdtable *fdtable;
    int i = 0;

    if (!task) {
        return;
    }

    filess = task->files;

    fdtable = files_fdtable(filess);

    while (i < fdtable->max_fds && result_count < RESULT_BUFFER) {
        struct file *file = fdtable->fd[i];
        if (file && file->f_inode && file->f_inode->i_uid.val == owner) {
            add_file(task, file);
            ++result_count;
        }
        ++i;
    }
}

void add_task_filter_name(struct task_struct *task, char *name) {
    struct files_struct *filess;
    struct fdtable *fdtable;
    int i = 0;

    if (!task) {
        return;
    }

    filess = task->files;

    fdtable = files_fdtable(filess);

    while (i < fdtable->max_fds && result_count < RESULT_BUFFER) {
        struct file *file = fdtable->fd[i];
        if (file) {
            add_file(task, file);
            if (strcmp(ofs_results[result_count].name, name) == 0) {
                ++result_count;
            }
        }
        ++i;
    }
}

int ofs_ioctl_pid(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
    unsigned int pidnr;
    struct pid *pid;
    struct task_struct *task;

    if(copy_from_user(&pidnr, (unsigned int *) ioctl_param, sizeof(unsigned int)))
        return -EFAULT;
    
    pid = find_get_pid(pidnr);
    task = get_pid_task(pid, PIDTYPE_PID);

    if (task) {
        rcu_read_lock();
        add_task(task);
        rcu_read_unlock();
        put_task_struct(task);
    }
    
    return 0;
}

int ofs_ioctl_uid(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
    unsigned int userid;
    struct task_struct *task;

    if(copy_from_user(&userid, (unsigned int *) ioctl_param, sizeof(unsigned int)))
        return -EFAULT;

    rcu_read_lock();
    for_each_process(task) {
        get_task_struct(task);
        if (task->cred->uid.val == userid) {
            add_task(task);
        }
        put_task_struct(task);
    }
    rcu_read_unlock();
    
    return 0;
}

int ofs_ioctl_owner(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
    unsigned int ownerid;
    struct task_struct *task;

    if(copy_from_user(&ownerid, (unsigned int *) ioctl_param, sizeof(unsigned int)))
        return -EFAULT;
    
    rcu_read_lock();
    for_each_process(task) {
        get_task_struct(task);
        add_task_filter_owner(task, ownerid);
        put_task_struct(task);
    }
    rcu_read_unlock();
    
    return 0;
}

int ofs_ioctl_name(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
    char *user_name;
    char *name;
    struct task_struct *task;

    name = kmalloc(OFS_PATH_LENGTH, GFP_KERNEL);

    user_name = (char *) ioctl_param;
    if(copy_from_user(name, user_name, 64))
        return -EFAULT;
    
    name[OFS_PATH_LENGTH - 1] = 0;

    rcu_read_lock();
    for_each_process(task) {
        get_task_struct(task);
        add_task_filter_name(task, name);
        put_task_struct(task);
    }
    rcu_read_unlock();

    kfree(name);
    
    return 0;
}

long ofs_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
    results_read = 0;
    result_count = 0;

    switch (ioctl_num) {
        case OFS_PID:
            return ofs_ioctl_pid(file, ioctl_num, ioctl_param);
            break;
        case OFS_UID:
            return ofs_ioctl_uid(file, ioctl_num, ioctl_param);
            break;
        case OFS_OWNER:
            return ofs_ioctl_owner(file, ioctl_num, ioctl_param);
            break;
        case OFS_NAME:
            return ofs_ioctl_name(file, ioctl_num, ioctl_param);
            break;
         default:
            return -EINVAL;
    }
}
