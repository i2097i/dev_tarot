/*
 *  tarot.c
 *
 *  dev_tarot - a linux module for tarot.
 *
 *      provides a character device driver at /dev/tarot.
 *
 *      adapted from https://github.com/tinmarino/dev_one.
 *
 */
static const char device_n[] = "tarot";
static const char class_n[] = "tarot_cls";
#include "generator/module.c"
