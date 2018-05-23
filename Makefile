# my optional module name
MODULE=gpio-counter
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

 
# this two variables, depends where you have you raspberry kernel source and tools installed
 
KERNEL_SRC=/lib/modules/$(shell uname -r)/build/
 
 
obj-m += ${MODULE}.o
 
module_upload=${MODULE}.ko
 
all: ${MODULE}.ko
 
${MODULE}.ko: ${MODULE}.c
	make -C ${KERNEL_SRC} M=$(PWD) modules
 
clean:
	make -C ${KERNEL_SRC} M=$(PWD) clean
 
info:
	modinfo  ${module_upload}
