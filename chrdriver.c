#include <linux/init.h>          
#include <linux/module.h>        
#include <linux/types.h>         
#include <linux/fs.h>            
#include <linux/moduleparam.h>
#include <linux/slab.h>          // kmalloc/kfree
#include <linux/cdev.h>          // struct cdev
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/device.h>		 // device class
#include <linux/semaphore.h>	 // semaphore(struct)
#include <linux/list.h>			 // kernel linked list
#include <asm/uaccess.h>         // copy_to_user and copy_from_user

#define MYDEV_NAME "mycdrv"
#define ramdisk_size (size_t) (16 * PAGE_SIZE) // ramdisk size 
//data access directions
#define REGULAR 0
#define REVERSE 1
#define CDRV_IOC_MAGIC 'Z'

MODULE_LICENSE("GPL");

static int NUM_DEVICES = 3; //default value
module_param(NUM_DEVICES, int, S_IRUGO);

dev_t first = 0;
struct ASP_mycdrv deviceList;
struct class *cl;

struct ASP_mycdrv {
	struct list_head list;
	struct cdev c_dev;
	int direction;
	char *ramdisk;
	struct semaphore sem;
	int devNo;
};

static int mycdrv_open(struct inode *pinode, struct file *pfile){
	struct list_head * pos;
	struct ASP_mycdrv * curr = NULL;
	unsigned int min = iminor(pinode); 
	printk (KERN_DEBUG "Inside the %s function\n", __FUNCTION__);	
	list_for_each(pos,&deviceList.list) {
		curr = list_entry(pos,struct ASP_mycdrv,list);
		if(MINOR((curr->c_dev).dev) == min) break;		
	}
	pfile->private_data = curr; 		
	return 0;
}

static loff_t mycdrv_lseek(struct file *pfile, loff_t offset, int origin) 
{
	loff_t testpos;
	switch (origin) {
	case SEEK_SET:
		testpos = offset;
		break;
	case SEEK_CUR:
		testpos = pfile->f_pos + offset;
		break;
	case SEEK_END:
		testpos = ramdisk_size + offset;
		break;
	default:
		return -EINVAL;
	}
	testpos = testpos < ramdisk_size ? testpos : ramdisk_size;
	testpos = testpos >= 0 ? testpos : 0;
	pfile->f_pos = testpos;
	printk(KERN_DEBUG "Seeking to pos=%ld\n", (long)testpos);
	return testpos;
}

static long mycdrv_ioctl (struct file *pfile, unsigned int cmd, unsigned long arg){	
	struct semaphore *sem=(struct semaphore*)&(((struct ASP_mycdrv *)pfile->private_data)->sem);	
	printk (KERN_DEBUG "Inside the %s function\n", __FUNCTION__);
	down(sem);
	if (_IOC_TYPE(cmd) != CDRV_IOC_MAGIC) return -ENOTTY;	
	((struct ASP_mycdrv *)pfile->private_data)->direction = *((int *)arg);
	printk(KERN_DEBUG "IOCTL completed, new direction: %d\n", ((struct ASP_mycdrv *)pfile->private_data)->direction);
	up(sem);
	return 0;
}

static ssize_t mycdrv_read(struct file *pfile, char __user *buffer, size_t length, loff_t *ppos){
	int i=0, nbytes = 0;
	struct semaphore *sem=(struct semaphore*)&(((struct ASP_mycdrv *)pfile->private_data)->sem);
	char *str = kmalloc(length,GFP_KERNEL);
	down(sem);
	printk (KERN_DEBUG "Inside the %s function\n", __FUNCTION__);
	if ((((struct ASP_mycdrv *)pfile->private_data)->direction) == REGULAR){		
		if ((int)(length + *ppos) > ramdisk_size) {	
			printk(KERN_DEBUG "trying to read past end of device,"
				"aborting because this is just a stub!\n");
			goto out;
		}	
		nbytes = length - copy_to_user(buffer, ((struct ASP_mycdrv *)pfile->private_data)->ramdisk + *ppos, length);
		*ppos += nbytes;		
	}
	else if ((((struct ASP_mycdrv *)pfile->private_data)->direction) == REVERSE){		
		if ((int)(*ppos - length) < 0) {	
			printk(KERN_DEBUG "trying to read before start of device,"
				"aborting because this is just a stub!\n");
			goto out;
		}
		for(i = 0;i < length;i++){
			str[i] = ((struct ASP_mycdrv *)pfile->private_data)->ramdisk[*ppos - 1 - i];
		}
		str[length] = '\0';		
		nbytes = length - copy_to_user(buffer, str, length);;
		*ppos-=nbytes;		
	}
	out:
	printk(KERN_DEBUG "\n READING function, nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
	up(sem);
	return nbytes;	
}

static ssize_t mycdrv_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *ppos){
	int i=0, strlen = 0, nbytes = 0;
	struct semaphore *sem=(struct semaphore*)&(((struct ASP_mycdrv *)pfile->private_data)->sem);
	char *str = kmalloc(length,GFP_KERNEL);
	down(sem);
	printk (KERN_DEBUG "Inside the %s function\n", __FUNCTION__);
	for (strlen = 0; strlen < length && buffer[strlen]!='\0'; strlen++);
	printk(KERN_DEBUG "strlen: %d length: %d", strlen, (int)length);
	if ((((struct ASP_mycdrv *)pfile->private_data)->direction) == REGULAR){
		if ((int)(strlen + *ppos) > ramdisk_size) {
			printk(KERN_ALERT "trying to read past end of device,"
				"aborting because this is just a stub!\n");
			goto out;
		}	
		nbytes = strlen - copy_from_user(((struct ASP_mycdrv *)pfile->private_data)->ramdisk + *ppos, buffer, strlen);
		*ppos += nbytes;		
	}
	else if ((((struct ASP_mycdrv *)pfile->private_data)->direction) == REVERSE){		
		if ((int)(*ppos - length) < 0) {
			printk(KERN_ALERT "trying to read before start of device,"
				"aborting because this is just a stub!\n");
			goto out;
		}
		nbytes = length - copy_from_user(str,buffer,length);
		for(i = 0;i < length ;i++){	
			((struct ASP_mycdrv *)pfile->private_data)->ramdisk[*ppos - i] = str[i];
		}			
		*ppos -= nbytes;
		kfree(str);	
	}
	out:
	printk(KERN_DEBUG "\n WRITING function, nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
	up(sem);
	return nbytes;
}

static int mycdrv_release(struct inode *pinode, struct file *pfile){
	printk (KERN_ALERT "Inside the %s function\n", __FUNCTION__);
	return 0;
}

static const struct file_operations mycdrv_fops = {
	.owner = THIS_MODULE,
	.read = mycdrv_read,
	.write = mycdrv_write,
	.open = mycdrv_open,
	.release = mycdrv_release,
	.llseek = mycdrv_lseek,
	.unlocked_ioctl = mycdrv_ioctl,
};

static int mydriver_init(void){
		int err = 0, i;		
		struct ASP_mycdrv * curr = NULL;
		struct list_head * pos;
		printk (KERN_ALERT "Inside the %s function\n", __FUNCTION__);				
		
		/* dynamically allocating major number and registering a range of device numbers = NUM_DEVICES */		
		err = alloc_chrdev_region (&first, 0, NUM_DEVICES, MYDEV_NAME);
		if (err < 0) {
			printk(KERN_WARNING "[target] alloc_chrdev_region() failed\n");
			return err;
		}		
		printk(KERN_ALERT "Major number: %d\n", MAJOR(first));
		printk(KERN_ALERT "Minor number: %d\n", MINOR(first));
		cl = class_create(THIS_MODULE,"mycdrrv11");
		
		INIT_LIST_HEAD(&deviceList.list); // creating list head
		
		//creating list nodes		
		for	(i = 0; i < NUM_DEVICES; i++){
			struct ASP_mycdrv* newDevice;
			newDevice = kmalloc(sizeof(*newDevice), GFP_KERNEL);	
			(newDevice->devNo) = i;								
			cdev_init(&(newDevice->c_dev), &mycdrv_fops);
			newDevice->c_dev.owner = THIS_MODULE;
			cdev_add(&(newDevice->c_dev), MKDEV(MAJOR(first),MINOR(first) + newDevice->devNo), 1);			
			newDevice->direction = REGULAR;
			newDevice->ramdisk = kmalloc(ramdisk_size, GFP_KERNEL);
			memset(newDevice->ramdisk,0,ramdisk_size);
			sema_init(&(newDevice->sem), 1);			
			list_add(&(newDevice->list), &(deviceList.list));					
			printk(KERN_INFO "Device %d structure initialized\n", newDevice->devNo);
		}		
		
		/*Using device class to automate the device creation */
		list_for_each(pos,&deviceList.list) {
			curr = list_entry(pos,struct ASP_mycdrv,list);
			device_create(cl, NULL, MKDEV(MAJOR(first), MINOR(first) + curr->devNo), NULL, "mycdrv%i", curr->devNo);
		}
		
		return 0;
	}
	
static void mydriver_exit(void){	
    struct ASP_mycdrv * curr = NULL;
    struct list_head * pos , *q;
    int i = 0;
    printk (KERN_ALERT "Inside the %s function\n", __FUNCTION__);
    list_for_each(pos,&deviceList.list) {
		curr = list_entry(pos,struct ASP_mycdrv,list);
		if(curr && curr->ramdisk){
			kfree(curr->ramdisk);
		}
       	cdev_del(&(curr->c_dev));
       	device_destroy(cl,MKDEV(MAJOR(first), curr->devNo));
       	i++;
    }	

    class_unregister(cl);
    class_destroy(cl);
    unregister_chrdev_region(first,NUM_DEVICES);    
    list_for_each_safe(pos, q, &(deviceList.list)){
		curr = list_entry(pos, struct ASP_mycdrv, list);
		list_del(pos);
		kfree(curr);
    }
    printk(KERN_ALERT "%s : Exiting\n",__FUNCTION__);
}

module_init(mydriver_init);
module_exit(mydriver_exit);

		
