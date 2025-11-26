/*
 *  generate.c
 *
 *  dev_tarot - a linux module for tarot.
 *
 *      provides a character device driver at /dev/tarot.
 *
 *      adapted from https://github.com/tinmarino/dev_one.
 *
 */
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