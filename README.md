# openFileSearchDev

A pseudo driver for searching open files.

## Getting started

Compiling is done by executing `make`.
A successful compilation generates a kernel module `openfilesearchdev.ko` and an executable `ioctl` to test the module.
It can be loaded into the Kernel by executing `insmod openfilesearchdev.ko` as root.
If it was successfully loaded, it will print it's device number and an `mknod` command to dmesg, which will look like following: `sudo mknod /dev/openFileSearchDev c 243 0`.
From here you can test it with the `ioctl` program or use your own, as long as you use the ioctl commands defined in `openfilesearchdev.h`.

## How it works

### open and release

The job of the function open and release is to tell the Kernel, that the module is in use with `try_module_get` and `module_put`.
Additionally they act as "guard", that only one open file descriptor to the module at a time is possible.
To achieve this in a thread safe manner, an `atomic_t` counter is utilized.
The counter is set to 0 in the init method.
The open function increases and checks it's value.
If it is above 1, then it will be decreased and the open fails.
Otherwise it will decreased, once the file descriptor is released.

### read

The read method tracks, how many entries have been read so far and copies the content of the result array without any transformations to the user space.

### ioctl

Finding file pointers is seperated in two steps: finding an appropriate task and adding all files matching a criteria to the result array.
To ensure that tasks aren't freed by the kernel while they are being accessed, the usage count field is increased and decreased.
For a single task, the increase operation is done by the `get_pid_task` function, so it is only necessary to call the `put_task_struct` function after processing.
However the `for_each_task` macro doesn't do this, so the `get_task_struct` macro has to be called.
The `for_each_task` uses `rcu_dereference` to provide a pointer to the next task, so it is wrapped within an RCU block.
The `add_task` function and friends all assume, they are called within an RCU block, so for a single task that call is also wrapped within an RCU block.
To retrieve the file descriptor table, the function `files_fdtable` is called.
This function interally uses `rcu_dereference` so there is no need to do that by yourself.
After that all file descriptors are transformed into a result and added to the results array, if they match the given condition.
