#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include "memalloc.h"

int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static int device_mmap(struct file *filp, struct vm_area_struct *vma);
static long device_ioctl(struct file *file,unsigned int cmd, unsigned long arg);


#define SUCCESS 0
#define DEVICE_NAME "memalloc"	

/* 
 * Global variables are declared as static, so are global within the file. 
 */

static int major;


static char *va_dmabuf = NULL;
static dma_addr_t pa_dmabuf;
static char *va_kmbuf = NULL;
static dma_addr_t pa_kmbuf;
static dma_addr_t pa_currentbuf;
static unsigned long bufsize = 0;

static struct file_operations fops = {
	.open = device_open,
	.release = device_release,
	.mmap = device_mmap,
	.unlocked_ioctl = device_ioctl
};

int init_module(void)
{
        major = register_chrdev(0, DEVICE_NAME, &fops);

	if (major < 0) {
	  printk("%s: Failed to register\r\n",DEVICE_NAME);
	  return major;
	}
	printk("major number %d, dont forget to mknod\r\n",major);

	return SUCCESS;
}

void cleanup_module(void)
{
	/* 
	 * Unregister the device 
	 */
	unregister_chrdev(major, DEVICE_NAME);
	if(va_dmabuf)
		dma_free_coherent(0,bufsize,va_dmabuf,pa_dmabuf);
        if(va_kmbuf)
		kfree(va_kmbuf);
}


static long device_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg){

        dma_addr_t pa = 0;	
	int i,j;
	unsigned long s=0;

	printk("entering ioctl cmd %u with arg %lu\r\n",cmd,arg);
	if (bufsize == 0 && cmd != SET_MEM_SIZE){
		printk("memalloc: first set memsize!\r\n");
		return -EFAULT;
	}else if (bufsize != 0 && cmd == SET_MEM_SIZE){
		printk("memalloc: not possible to change mem size\r\n");
		return -EFAULT;
	}

 
	switch(cmd)
	{
	case DMAMEM:
		va_dmabuf = dma_alloc_coherent(0,bufsize,&pa,GFP_KERNEL|GFP_DMA);
		pa_dmabuf = pa;	
		printk("kernel va_dmabuf: 0x%p, pa_dmabuf 0x%08X\r\n",va_dmabuf,pa_dmabuf);
		break;
	case DMAMEM_TEST:
		for(j=0;j<LOOPCNT;j++){
			for(i=0;i<bufsize;i++){
				s += va_dmabuf[i];
			}
		}
		break;
	case KMEM:
		va_kmbuf = kmalloc(bufsize,GFP_KERNEL);
		pa = dma_map_single(0,va_kmbuf,bufsize,DMA_FROM_DEVICE);
		pa_kmbuf = pa;
		dma_sync_single_for_cpu(0,pa_kmbuf,bufsize,DMA_FROM_DEVICE);
		printk("kernel va_kmbuf: 0x%p, pa_kmbuf 0x%08X\r\n",va_kmbuf,pa_kmbuf);
		break;
	case KMEM_TEST:
		for(j=0;j<LOOPCNT;j++){
			for(i=0;i<bufsize;i++){
				s += va_kmbuf[i];
			}
		}
		break;
	case DMAMEM_REL:
		dma_free_coherent(0,bufsize,va_dmabuf,pa_dmabuf);
		va_dmabuf = 0;
		break;
	case KMEM_REL:
		kfree(va_kmbuf);
		va_kmbuf = 0;
		break;
	case SYNC_KMEM:
		dma_sync_single_for_cpu(NULL, pa_kmbuf,bufsize,DMA_FROM_DEVICE);
		break;
	case SET_MEM_SIZE:
		//if(copy_from_user(&bufsize,arg,sizeof(bufsize))!=0){
		//	printk("memalloc: error copying to userspace\r\n");
		//	return -EFAULT;
		//}
		bufsize=arg;
		break;
	case USELESS:
		//test speed of ioctl call itself
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

static int device_open(struct inode *inode, struct file *file)
{
	try_module_get(THIS_MODULE);

	return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
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
//	vma->vm_page_prot = pgprot_dmacoherent(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start,
			    pa_currentbuf>>PAGE_SHIFT , size, vma->vm_page_prot)) {
		res = -ENOBUFS;
		goto device_mmap_exit;
	}

device_mmap_exit:
	return res;

 }
