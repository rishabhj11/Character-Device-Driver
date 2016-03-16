CONFIG_MODULE_SIG=n

obj-m += chrdriver.o

KDIR = /usr/src/linux-headers-$(shell uname -r)

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

app: 
	gcc -o userapp userapp.c

clean:
	rm -rf *.o *.ko *.mod.* *.symvers *.order
