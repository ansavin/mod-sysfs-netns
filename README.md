# mod-sysfs-netns

Sample kobject usage with net namespaces.
Based on `fs/nfs/sysfs.c`
and `samples/kobject/kobject-example.c`.

## About module

This module shows how to create a simple subdirectory in sysfs called
`/sys/kernel/kset_sysfs_netns/net/data` with file `property` in this directory
where we can read & write integers with following commands:

```bash
# to read property
cat /sys/kernel/kset_sysfs_netns/net/data/property
```

```bash
# to write property
echo 1 > /sys/kernel/kset_sysfs_netns/net/data/property
```

## Building & using module

### Using make

```bash
# build
make KSRC=/lib/modules/$(uname -r)/build
```

```bash
# load module
insmod mod-sysfs-netns.ko
```

```bash
# unload module
rmmod mod-sysfs-netns.ko
```

```bash
# clean up
make KSRC=/lib/modules/$(uname -r)/build clean
```

### Using DKMS

```bash
cd mod-sysfs-netns
```

```bash
# add module source to /usr/src tree
dkms add $(pwd)
```

```bash
# build & install module to /lib/modules/$(uname -r) tree
dkms install mod-sysfs-netns/1.0
```

```bash
# load module
modprobe -v mod-sysfs-netns
```

```bash
# unload module
rmmod mod-sysfs-netns
```

```bash
# remove it from DKMS tree
dkms remove mod-sysfs-netns/1.0 --all
```
