#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>	/* for put_user */
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include "memalloc.h"

int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static int device_mmap(struct file *filp, struct vm_area_struct *vma);
static long device_ioctl(struct file *file,unsigned int cmd, unsigned long arg);


#define SUCCESS 0
#define DEVICE_NAME "memalloc"	
#define BUF_LEN 80

/* 
 * Global variables are declared as static, so are global within the file. 
 */

static int Major;		/* Major number assigned to our device driver */
static int Device_Open = 0;	/* Is device open?  
				 * Used to prevent multiple access to device */
static char msg[BUF_LEN];	/* The msg the device will give when asked */
static char *msg_Ptr;


static char *va_dmabuf = NULL;
static dma_addr_t pa_dmabuf;
static char *va_kmbuf = NULL;
static dma_addr_t pa_kmbuf;
static dma_addr_t pa_currentbuf;


static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release,
	.mmap = device_mmap,
	.unlocked_ioctl = device_ioctl
};

/*
 * This function is called when the module is loaded
 */
int init_module(void)
{
        Major = register_chrdev(0, DEVICE_NAME, &fops);

	if (Major < 0) {
	  printk(KERN_ALERT "Registering char device failed with %d\n", Major);
	  return Major;
	}

	printk(KERN_INFO "I was assigned major number %d. To talk to\n", Major);
	printk(KERN_INFO "the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
	printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");
	printk(KERN_INFO "the device file.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");

	return SUCCESS;
}

/*
 * This function is called when the module is unloaded
 */
void cleanup_module(void)
{
	/* 
	 * Unregister the device 
	 */
	unregister_chrdev(Major, DEVICE_NAME);
	if(va_dmabuf)
		dma_free_coherent(0,BUFSIZE,va_dmabuf,pa_dmabuf);
        if(va_kmbuf)
		kfree(va_kmbuf);
}




static long device_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg){

        dma_addr_t pa = 0;	
	int i,j;
	unsigned long s=0;

	printk("entering ioctl cmd %d\r\n",cmd);
	switch(cmd)
	{
	case DMAMEM:
		va_dmabuf = dma_alloc_coherent(0,BUFSIZE,&pa,GFP_KERNEL|GFP_DMA);
		//memset(va_dmabuf,0,BUFSIZE);
		//va_dmabuf[15] = 23;
		pa_dmabuf = pa;	
		printk("kernel va_dmabuf: 0x%p, pa_dmabuf 0x%08X\r\n",va_dmabuf,pa_dmabuf);
		break;
	case DMAMEM_TEST:
		for(j=0;j<LOOPCNT;j++){
			for(i=0;i<BUFSIZE;i++){
				s += va_dmabuf[i];
			}
		}
		break;
	case KMEM:
		va_kmbuf = kmalloc(BUFSIZE,GFP_KERNEL);
		//pa = virt_to_phys(va_kmbuf);
		//pa = __pa(va_kmbuf);
		pa = dma_map_single(0,va_kmbuf,BUFSIZE,DMA_FROM_DEVICE);
		pa_kmbuf = pa;
		dma_sync_single_for_cpu(0,pa_kmbuf,BUFSIZE,DMA_FROM_DEVICE);
		//memset(va_kmbuf,0,BUFSIZE);
		//va_kmbuf[10] = 11;
		printk("kernel va_kmbuf: 0x%p, pa_kmbuf 0x%08X\r\n",va_kmbuf,pa_kmbuf);
		break;
	case KMEM_TEST:
		for(j=0;j<LOOPCNT;j++){
			for(i=0;i<BUFSIZE;i++){
				s += va_kmbuf[i];
			}
		}
		break;
	case DMAMEM_REL:
		dma_free_coherent(0,BUFSIZE,va_dmabuf,pa_dmabuf);
		va_dmabuf = 0;
		break;
	case KMEM_REL:
		kfree(va_kmbuf);
		va_kmbuf = 0;
		break;
	default:
		break;
	} 
 
	if(cmd == DMAMEM_TEST || cmd == KMEM_TEST){
		if(copy_to_user((void*)arg, &s, sizeof(s)))
			return -EFAULT;
	}else{
		pa_currentbuf = pa;
 		if(copy_to_user((void*)arg, &pa, sizeof(pa)))
			return -EFAULT;
	}
	return 0;
}

/*
 * Methods
 */

/* 
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file)
{
	static int counter = 0;

	if (Device_Open)
		return -EBUSY;

	Device_Open++;
	sprintf(msg, "I already told you %d times Hello world!\n", counter++);
	msg_Ptr = msg;
	try_module_get(THIS_MODULE);

	return SUCCESS;
}

/* 
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file)
{
	Device_Open--;		/* We're now ready for our next caller */

	/* 
	 * Decrement the usage count, or else once you opened the file, you'll
	 * never get get rid of the module. 
	 */
	module_put(THIS_MODULE);

	return 0;
}


static int device_mmap(struct file *filp, struct vm_area_struct *vma)
 {
	unsigned long size;
	int res = 0;
	size = vma->vm_end - vma->vm_start;
	//vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
//	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
//	vma->vm_page_prot = __pgprot_modify(vma->vm_page_prot,L_PTE_MT_MASK,L_PTE_MT_WRITEBACK);
//	vma->vm_page_prot = __pgprot_modify(vma->vm_page_prot,L_PTE_MT_MASK,L_PTE_MT_DEV_CACHED);
//	vma->vm_page_prot = __pgprot_modify(vma->vm_page_prot,L_PTE_MT_MASK,L_PTE_MT_WRITETHROUGH);

	if (remap_pfn_range(vma, vma->vm_start,
			    pa_currentbuf>>PAGE_SHIFT , size, vma->vm_page_prot)) {
		res = -ENOBUFS;
		goto device_mmap_exit;
	}

	vma->vm_flags &= ~VM_IO;	/* using shared anonymous pages */

device_mmap_exit:
	return res;

 }

/* 
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */

static ssize_t device_read(struct file *filp,	/* see include/linux/fs.h   */
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{
        /*
	 * Number of bytes actually written to the buffer 
	 */
	int bytes_read = 0;

	/*
	 * If we're at the end of the message, 
	 * return 0 signifying end of file 
	 */
	if (*msg_Ptr == 0)
		return 0;

	/* 
	 * Actually put the data into the buffer 
	 */
	while (length && *msg_Ptr) {

		/* 
		 * The buffer is in the user data segment, not the kernel 
		 * segment so "*" assignment won't work.  We have to use 
		 * put_user which copies data from the kernel data segment to
		 * the user data segment. 
		 */
		put_user(*(msg_Ptr++), buffer++);

		length--;
		bytes_read++;
	}

	/* 
	 * Most read functions return the number of bytes put into the buffer
	 */

	return bytes_read;
}

/*  
 * Called when a process writes to dev file: echo "hi" > /dev/hello 
 */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
	return -EINVAL;
}
