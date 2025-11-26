# see: https://stackoverflow.com/questions/15910064/how-to-compile-a-linux-kernel-module-using-std-gnu99

TARGET_MODULE := tarot

# permit a user-defined kernel version with the KVER variable (#6)
KVER ?= $(shell uname -r)
# -- like /lib/modules/5.15.0-57-generic/build/certs/signing_key.x509
BUILDSYSTEM_DIR ?= /lib/modules/$(KVER)/build
EXTRA_DIR ?= /lib/modules/$(KVER)/extra

PWD := $(shell pwd)
obj-m := $(TARGET_MODULE).o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

define title
	@echo -ne '\033[32m'
	@echo -n $1
	@echo -e '\033[0m'
endef

# only compile the module
# -- use: make full
# -- retrocompatibility)
# see (#9 with the ARCH package already using makefile for a package)
all: build


full: \
	check \
	build \
	sign \
	install \
	load \
	test

clean: \
	unbuild \
	uninstall \
	delete

key:
	@$(call title, "creating keys")
	openssl req -new -x509 -newkey rsa:2048 -days 36500 -keyout MOK.priv -outform DER -out MOK.der -nodes -subj "/CN=TinmarinoUnsafe/"
	#
	# TODO: backup if there is one key
	cp MOK.der $(BUILDSYSTEM_DIR)/certs/signing_key.x509
	cp MOK.priv $(BUILDSYSTEM_DIR)/certs/signing_key.pem
	#
	@echo "\e[31;1mplease enter a password you will be asked for on reboot:\e[0m"
	mokutil --import MOK.der
	@echo "\e[31;1mnow you must: 1/ reboot, 2/ select 'Unroll MOK', 3/ enter password you previously gave\e[0m"
	@echo

check:
	@$(call title, "checking")
	@if [ ! -f MOK.der ] || [ ! -f MOK.priv ]; then \
		echo -n '\e[31m'; \
		echo 'error: you must create keys before'; \
		echo 'tip: run: make key'; \
		echo 'tip: read README.md file'; \
		echo '\e[0m'; \
	  exit 1; \
	fi
	@echo

build:
	# Run kernel build system to make module
	@$(call title, "compiling")
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules
	@echo

unbuild:
	@$(call title, "removing local binary")
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) clean
	@echo

sign:
	@$(call title, "signing with the generated self-signed keys")
	cp $(TARGET_MODULE).ko $(TARGET_MODULE).ko.bck
	/usr/src/linux-headers-$(shell uname -r)/scripts/sign-file sha256 MOK.priv MOK.der $(TARGET_MODULE).ko
	@echo

install:
	@$(call title, "installing system wide")
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules_install
	depmod
	@echo

uninstall:
	@$(call title, "removing system binary")
	rm $(EXTRA_DIR)/$(TARGET_MODULE).ko
	@echo

load:
	@$(call title, "loading")
	modprobe $(TARGET_MODULE)
	@echo

unload:
	@$(call title, "unloading")
	modprobe -r $(TARGET_MODULE)
	@echo

local_load:
	insmod ./$(TARGET_MODULE).ko

local_unload:
	rmmod ./$(TARGET_MODULE).ko

create:
	# Not required since (#8 from Dreirund) as load is doing it
	@$(call title, "creating node device /dev/tarot")
	mknod /dev/$(TARGET_MODULE) c $(shell cat /proc/devices | grep $(TARGET_MODULE)$ | cut -d ' ' -f1) 0
	@echo

delete:
	@$(call title, "deleting node device /dev/$(TARGET_MODULE)")
	if lsmod | grep -q '^$(TARGET_MODULE)\b'; then modprobe -r $(TARGET_MODULE); fi
	if [ -e /dev/$(TARGET_MODULE) ]; then rm /dev/$(TARGET_MODULE); fi
	@echo

test:
	@$(call title, "testing")
	@if [ "$(shell sudo ./.generate || false)" = true ]; then \
		echo "\e[32mSUCCESS\e[0m"; \
		exit 0; \
	else \
		echo "\e[31mFAILED\e[0m"; \
		exit 1; \
	fi
	@echo
