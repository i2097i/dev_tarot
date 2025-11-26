#define AUTHOR "i2097i"
/*
 *  module.c
 *
 *  dev_tarot - a linux module for tarot.
 *
 *      provides a character device driver at /dev/tarot.
 *
 *      adapted from https://github.com/tinmarino/dev_one.
 *
 */

#define DEVICE_NAME "tarot"
#define CLASS_NAME "tarot_cls"

#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include "generate.c"

// prototypes
int register_device(void);
void unregister_device(void);

static dev_t first; // global variable for the first device number (major and minor)
static const unsigned int first_minor = 1; // the first minor number we want to request (needs to be between 1 ans 254)
static const unsigned int device_count = 78; // the number of devices we want to support. make sure that first_minor + device_count <= 254.
static struct cdev c_dev; // global variable for the character device structure
static struct class *cl; // global variable for the device class
static int device_file_major_number = 0;

static struct file_operations simple_driver_fops = {
    .owner   = THIS_MODULE,
    .read    = device_file_read,
};

int register_device(void) {
    int res = 0;
    // hi
    printk(KERN_NOTICE "[tarot]: function register_device is called.\n" );

    // allocate memory
    // this used to be as follows before auto-createdevice (#8)
    // --  res = register_chrdev( 0, DEVICE_NAME, &simple_driver_fops )
    res = alloc_chrdev_region(&first, first_minor, device_count, DEVICE_NAME);
    if( res != 0 ) {
        printk(KERN_WARNING "[tarot]: can\'t register character device with error code = %i\n", res );
        return res;
    }
    device_file_major_number = MAJOR(first);
    printk(KERN_NOTICE "[tarot]: has registered character device with major number = %i and minor numbers %i..%i\n", device_file_major_number, first_minor, first_minor+device_count );

    // create sysfs information:
    printk(KERN_NOTICE "[tarot]: creating device class\n" );
    if ((cl = class_create(CLASS_NAME)) == NULL) {
        printk(KERN_ALERT "[tarot]: device class creation failed\n" );
        unregister_chrdev_region(first, 1);
        return -1;
    }

    // create device
    printk(KERN_NOTICE "[tarot]: creating device\n" );
    if (device_create(cl, NULL, first, NULL, DEVICE_NAME) == NULL) {
        printk(KERN_ALERT "[tarot]: device creation failed\n" );
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        return -1;
    }

    // init device
    printk(KERN_NOTICE "[tarot]: initialising device\n" );
    cdev_init(&c_dev, &simple_driver_fops);

    // register device
    printk(KERN_ALERT "[tarot]: adding device\n" );
    if (cdev_add(&c_dev, first, 1) == -1) {
        printk(KERN_ALERT "[tarot]: device addition failed\n" );
        device_destroy(cl, first);
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        return -1;
    }

    // bye
    printk(KERN_NOTICE "[tarot]: function register_device is returning\n" );
    return 0;
}

void unregister_device(void) {
    printk(KERN_NOTICE "[tarot]: function unregister_device is called\n" );

    // destroy sysfs information:
    printk(KERN_NOTICE "[tarot]: deleting device\n" );
    cdev_del(&c_dev);
    printk(KERN_NOTICE "[tarot]: destroying device\n" );
    device_destroy(cl, first);
    printk(KERN_NOTICE "[tarot]: destroying device class\n" );
    class_destroy(cl);

    if(device_file_major_number != 0) {
        unregister_chrdev_region(first, device_count);
    }

    printk(KERN_NOTICE "[tarot]: function unregister_device is returning\n" );
}

static int mod_init(void) {
    register_device();
    return 0;
}

static void mod_exit(void) {
    unregister_device();
    return;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR (AUTHOR);
MODULE_DESCRIPTION("a linux module for tarot.");
MODULE_VERSION("1.0.0");

// declare register and unregister command
module_init(mod_init);
module_exit(mod_exit);
