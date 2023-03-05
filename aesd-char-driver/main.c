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
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/string.h>
#include "aesdchar.h"
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

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    const char *retbuff;  /* A pointer to the char buffer that will be filled for return */
    struct aesd_buffer_entry *entry; /* A pointer to a structure for iterations */
    size_t entry_offset;
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    // Allocate space for count chars to return to the user
    if (mutex_lock_interruptible(&dev->lock)) return -ERESTARTSYS;
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

    do {
        if ((retval + entry->size) < count) {
            strncpy(retbuff + retval, entry->buffptr, entry->size);
            retval += entry->size;
        }
        else {
            strncpy(retbuff + retval, entry->buffptr, (entry_offset + 1));
            retval += (entry_offset + 1);
            break;
        }
        entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->buffer), *f_pos + retval, &entry_offset);
    } while (entry);

    if (copy_to_user(buf, retbuff, retval)) {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += retval;

  out:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry * entry;
    struct aesd_buffer_entry * obsolete;
    size_t dummy;
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    /**
     * TODO: handle write
     */

    if (mutex_lock_interruptible(&dev->lock)) return -ERESTARTSYS;
    // Allocate entry 
    entry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
    if (!entry) {
        goto out;
    }
    memset(entry, 0, sizeof(struct aesd_buffer_entry));
    // Allocate space for count chars to receive from the user
    entry->buffptr = kmalloc(count * sizeof(char), GFP_KERNEL);
    if (!(entry->buffptr)) {
        kfree(entry);
        goto out;
    }
    // Initialize entry
    memset((void *)entry->buffptr, 0, count * sizeof(char));
//    entry->size = count;

    if (copy_from_user((void *)entry->buffptr, buf, count)) {
        kfree(entry->buffptr);
        kfree(entry);
        retval = -EFAULT;
        goto out;
    }
    entry->size = count;

    /* The 'entry' is allocated and populated from user space.
    */
    if (dev->buffer.full) {
        obsolete = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->buffer), 0, &dummy);
        kfree(obsolete->buffptr);
        obsolete->size = 0;
    }

    aesd_circular_buffer_add_entry(&(dev->buffer), entry);
    kfree(entry);
    retval = count;

  out:
    mutex_unlock(&dev->lock);
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
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
    aesd_circular_buffer_init(&aesd_device.buffer);
    mutex_init(&aesd_device.lock);

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
