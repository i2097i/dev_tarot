#define AUTHOR "i2097i"
/*
 *  dev_tarot - a linux module for tarot.
 *
 *      provides a character device driver at /dev/tarot.
 *
 *      adapted from https://github.com/tinmarino/dev_one.
 *
 */

#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/cdev.h>
#include <linux/module.h>

// prototypes
int register_device(void);
void unregister_device(void);

static dev_t first; // global variable for the first device number (major and minor)
static const unsigned int first_minor = 1; // the first minor number we want to request (needs to be between 1 ans 254)
static const unsigned int device_count = 78; // the number of devices we want to support. make sure that first_minor + device_count <= 254.
static struct cdev c_dev; // global variable for the character device structure
static struct class *cl; // global variable for the device class

static int device_file_major_number = 0;
static const char device_name[] = "tarot";


static const char orientations[] = "+-";
static const char suites[] = "TCSPW";
static int major_arcana_count = 22;
static int minor_arcana_count = 14;

static ssize_t device_file_read(
        struct file *file_ptr,
        char __user *user_buffer,
        size_t count,
        loff_t *position) {

    // capture start ktime
    ktime_t start_ktime = ktime_get();

    // random seed value
    int seed = 0;

    // get non-negative random number
    get_random_bytes(&seed, sizeof(int) - 1);

    // default orientation to '+'
    int orientation_idx = 0;

    // calculate nanoseconds elapsed
    int ns_elapsed_ktime = ktime_to_ns(
        ktime_sub(ktime_get(), start_ktime)
    );
    // note: this is the logic that determines uprght/reverse selection weight
    if (ns_elapsed_ktime % 2 > 0) {
        // if elapsed time not even, use seed to set orientation
        orientation_idx = seed % strlen(orientations);
    }

    // set orientation char
    char orientation_char = orientations[orientation_idx];

    // set suite char
    char suite_char = suites[seed % strlen(suites)];
    bool is_major_arcana = suite_char == suites[0];

    // set card number
    int card_number;
    if (is_major_arcana) {
        card_number = seed % major_arcana_count;
    } else {
        card_number = seed % minor_arcana_count + 1;
    }
    char str_card_number[sizeof(card_number)];
    sprintf(str_card_number, "%d", card_number);
    int card_byte_count = strlen(str_card_number);

    // bytes_per_card holds total bytes per card
    //      orientation: 1 byte
    //      suite:       1 byte
    //      card number: card_byte_count byte(s)
    int bytes_per_card = 2 + card_byte_count;

    // total_cards holds number of times to write card to user_buffer
    int total_cards = count / bytes_per_card;

    // allocate kernel buffer
    char* ptr = (char*) vmalloc(count);

    // fill memory with current card
    for (int i = 0; i < total_cards; i++) {
        int cursor = (i * bytes_per_card);
        memset(ptr + cursor, orientation_char, 1);
        cursor++;
        memset(ptr + cursor, suite_char, 1);
        cursor++;
        for (int j = 0; j < card_byte_count; j++) {
            cursor += j;
            memset(ptr + cursor, str_card_number[j], 1);
        }
    }

    // kernel logging
    char card[7];
    sprintf(card, "%c%c%d", orientation_char, suite_char, card_number);
    printk(KERN_NOTICE "[tarot]: filling %zu bytes with %s (%d times)\n", count, card, total_cards);

    char res = copy_to_user(user_buffer, ptr, count);
    if (res != 0) {
        return -EFAULT;
    }

    // return number of bytes read
    return count;
}


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
    // --  res = register_chrdev( 0, device_name, &simple_driver_fops )
    res = alloc_chrdev_region(&first, first_minor, device_count, device_name);
    if( res != 0 ) {
        printk(KERN_WARNING "[tarot]: can\'t register character device with error code = %i\n", res );
        return res;
    }
    device_file_major_number = MAJOR(first);
    printk(KERN_NOTICE "[tarot]: has registered character device with major number = %i and minor numbers %i..%i\n", device_file_major_number, first_minor, first_minor+device_count );

    // create sysfs information:
    printk(KERN_NOTICE "[tarot]: creating device class\n" );
    if ((cl = class_create("chardrv")) == NULL) {
        printk(KERN_ALERT "[tarot]: device class creation failed\n" );
        unregister_chrdev_region(first, 1);
        return -1;
    }

    // create device
    printk(KERN_NOTICE "[tarot]: creating device\n" );
    if (device_create(cl, NULL, first, NULL, device_name) == NULL) {
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

static int tarot_init(void) {
    register_device();
    return 0;
}

static void tarot_exit(void) {
    unregister_device();
    return;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR (AUTHOR);

// declare register and unregister command
module_init(tarot_init);
module_exit(tarot_exit);
