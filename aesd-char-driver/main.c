/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>     /* kmalloc() */
#include <linux/errno.h>    /* error codes */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>    /* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/uaccess.h>  /* copy_*_user */
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>    /* size_t */
#include <linux/cdev.h>
#include <linux/fs.h>       /* file_operations, everything... */
#include <linux/string.h>

#include "aesdchar.h"       /* local definitions */
#include "aesd_ioctl.h"
// #include "aesd-circular-buffer.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Borys Ladanivskyy"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;                     /* device information */
    PDEBUG("open");
    /**
     * TODO: handle open
     * Done
     */

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;                  /* for other methods */
    filp->f_pos = 0;

    return 0;                                            /* success */
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    const char *retbuff;  /* A pointer to the char buffer that will be filled for return */
    struct aesd_buffer_entry *entry; /* A pointer to a structure for iterations */
    size_t entry_offset;
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     * Done
     */

    if (mutex_lock_interruptible(&dev->lock)) return -ERESTARTSYS;
    // Allocate space for count chars to return to the user
    retbuff = kmalloc(count * sizeof(char), GFP_KERNEL);
    if (!(retbuff)) {
        retval = -ENOMEM;
        goto out;
    }
    // Initialize retbuff
    memset((void *)retbuff, 0, count * sizeof(char));

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->buffer), *f_pos, &entry_offset);

    // If nothing to return
    if (!entry) {
        kfree(retbuff);
        goto out;
    }

if (count <= (entry->size - entry_offset)) {
    strncpy((char * const)(retbuff), entry->buffptr + entry_offset, count);
    retval = count;
}
else {
    strncpy((char * const)(retbuff), entry->buffptr + entry_offset, entry->size - entry_offset);
    retval = (entry->size - entry_offset);
}

    if (copy_to_user(buf, retbuff, retval)) {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += retval;
  out:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry * entry;
    struct aesd_buffer_entry * obsolete;
    size_t dummy;
    size_t index;
    size_t prepend = 0;
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    /**
     * TODO: handle write
     * Done
     */

    if (mutex_lock_interruptible(&dev->lock)) return -ERESTARTSYS;
    // Check if append needed
    index = (dev->buffer.in_offs + AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    if ((dev->buffer.entry[index].size != 0) && (dev->buffer.entry[index].buffptr[dev->buffer.entry[index].size - 1] != '\n')) {
        prepend = dev->buffer.entry[index].size;
    }

    // Allocate entry 
    entry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
    if (!entry) {
        goto out;
    }
    memset(entry, 0, sizeof(struct aesd_buffer_entry));
    // Allocate space in the struct 'entry' for count chars to receive from the user
    entry->buffptr = kmalloc((prepend + count) * sizeof(char), GFP_KERNEL);
    if (!(entry->buffptr)) {
        kfree(entry);
        goto out;
    }
    // Initialize entry
    memset((void *)entry->buffptr, 0, (prepend + count) * sizeof(char));

    if (prepend !=0) {
        strncpy((char * const)entry->buffptr, dev->buffer.entry[index].buffptr, prepend);
    }

    if (copy_from_user((void *)(entry->buffptr + prepend), buf, count)) {
        kfree(entry->buffptr);
        kfree(entry);
        retval = -EFAULT;
        goto out;
    }
    entry->size = prepend + count;

    /* The 'entry' is allocated and populated from user space, as needed it is prepended by previous content.
     */

    if (prepend != 0) {
        if (dev->buffer.full) {
            dev->buffer.out_offs = index;
        }
        dev->buffer.in_offs = index;
    }

    if (dev->buffer.full) {
        obsolete = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->buffer), 0, &dummy);
        dev->size -= obsolete->size;
        kfree(obsolete->buffptr);
        obsolete->size = 0;
    }

    aesd_circular_buffer_add_entry(&(dev->buffer), entry);
    dev->size += entry->size;
    kfree(entry);

    *f_pos = 0;
    retval = count;

  out:
    mutex_unlock(&dev->lock);
    return retval;
}

/*
 * The llseek() implementation
 */
loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    struct aesd_dev *dev = filp->private_data;
    loff_t newpos;

    switch(whence) {
      case 0: /* SEEK_SET */
        newpos = off;
        break;

      case 1: /* SEEK_CUR */
        newpos = filp->f_pos + off;
        break;

      case 2: /* SEEK_END */
        newpos = dev->size + off;
        break;

      default: /* can't happen */
        return -EINVAL;
    }
    if ((newpos < 0) || (newpos >= dev->size)) return -EINVAL;
    filp->f_pos = newpos;
    return newpos;
}

/*
 * The ioctl() implementation
 */
long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry = NULL; /* A pointer to a structure for iterations */
    struct aesd_seekto seekto;
    size_t entry_offset;
    loff_t newpos = 0;

    /*
     * extract the type and number bitfields, and don't decode wrong cmds:
     * return ENOTTY (inappropriate ioctl) before
     */
    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC) return -ENOTTY;
    if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR) return -ENOTTY;

    switch(cmd) {

      case AESDCHAR_IOCSEEKTO:
        if (copy_from_user(&seekto, (const struct aesd_seekto __user *)arg, sizeof(struct aesd_seekto))) {
                return -EACCES;
            }
        PDEBUG("ioctl %u, %u",seekto.write_cmd, seekto.write_cmd_offset);
        for (size_t i = 0; i < seekto.write_cmd; ++i) {
            entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->buffer), newpos, &entry_offset);
            if (!entry) {
                return -EINVAL;
            }
            newpos += entry->size;
        }
        entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->buffer), newpos + seekto.write_cmd_offset, &entry_offset);
        if (!entry) {
            return -EINVAL;
        }
        newpos += seekto.write_cmd_offset;
        filp->f_pos = newpos;
        PDEBUG("ioctl newpos = %lli,  f_pos = %lli", newpos, filp->f_pos);
        break;

      default:  /* redundant, as cmd was checked against MAXNR */
        return -ENOTTY;
    }
    return newpos;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .llseek =   aesd_llseek,
    .read =     aesd_read,
    .write =    aesd_write,
    .unlocked_ioctl = aesd_ioctl,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    /* Fail gracefully if need be */
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     * Done.
     */
    aesd_circular_buffer_init(&(aesd_device.buffer));
    aesd_device.size = 0;
    mutex_init(&(aesd_device.lock));
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    int i;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific portions here as necessary
     * Done
     */
    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        kfree(aesd_device.buffer.entry[i].buffptr);
    }

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
