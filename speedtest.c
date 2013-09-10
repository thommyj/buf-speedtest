#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <fcntl.h>		/* open */
#include <unistd.h>		/* exit */
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "memalloc.h"



int main()
{
        struct timeval t1,t2;
	int fd = 0;
	unsigned long pa_dmabuf = 0;
	unsigned long pa_kmbuf = 0;
	char *va_dmabuf = NULL;
	char *va_kmbuf = NULL;
	char *va_mbuf = NULL;
	int i,j;
	unsigned long s=0;

        fd = open("/dev/memalloc",O_RDWR,0);
	if(!fd){
		perror("unable to open device file\r\n");
		goto exito;
	}
	
	/*
	 *alloc memory with dma_alloc_coherent
	*/
	ioctl(fd,DMAMEM,&pa_dmabuf);
	if(pa_dmabuf == 0){
		printf("no dma pa returned\r\n");
		goto exito;
	}else{
		printf("pa_dmabuf = %p\r\n",(void*)pa_dmabuf);
	}

	va_dmabuf = 
		mmap(NULL,BUFSIZE,PROT_READ|PROT_WRITE,MAP_SHARED,fd,pa_dmabuf);
	
	if(!va_dmabuf || va_dmabuf == (char*)0xFFFFFFFF){
		perror("no valid va for dmabuf");
		goto exito;
	}else{
		printf("va_dmabuf = %p\r\n",va_dmabuf);
	}

	/*
	 * alloc memory with kmalloc
	 */
	ioctl(fd,KMEM,&pa_kmbuf);
        if(pa_kmbuf == 0){
                printf("no kmalloc pa returned\r\n");
                goto exito;
        }else{
                printf("pa_kmbuf = %p\r\n",(void*)pa_kmbuf);
        }

        va_kmbuf = 
                mmap(NULL,BUFSIZE,PROT_READ|PROT_WRITE,MAP_SHARED,fd,pa_kmbuf);
        
        if(!va_kmbuf || va_kmbuf == (char*)0xFFFFFFFF){
                perror("no valid va for kmbuf");
                goto exito;
        }else{
                printf("va_kmbuf = %p\r\n",va_kmbuf);
        }

	/*
	 * test speed of dma_alloc_coherent buffer in kernel
	 */
	gettimeofday(&t1,NULL);
	ioctl(fd,DMAMEM_TEST,&s);
        gettimeofday(&t2,NULL);
        printf("dma_alloc_coherent in kernel   %.3fs (s=%lu)\n",
		((t2.tv_sec-t1.tv_sec)*1000000+(t2.tv_usec-t1.tv_usec))/1000000.0,s);

        /*
         * test speed of kmalloc buffer in kernel
         */
        gettimeofday(&t1,NULL);
	ioctl(fd,KMEM_TEST,&s);
        gettimeofday(&t2,NULL);
        printf("kmalloc in kernel              %.3fs (s=%lu)\n",
		((t2.tv_sec-t1.tv_sec)*1000000+(t2.tv_usec-t1.tv_usec))/1000000.0,s);
 



	/*
	 * test speed of dma_alloc_coherent buffer
	 */
	s=0;
	gettimeofday(&t1,NULL);
        for(j=0;j<LOOPCNT;j++){
		for(i=0;i<BUFSIZE;i++){
                	s += va_dmabuf[i];
		}
	}	
        gettimeofday(&t2,NULL);
        printf("dma_alloc_coherent userspace   %.3fs (s=%lu)\n",
		((t2.tv_sec-t1.tv_sec)*1000000+(t2.tv_usec-t1.tv_usec))/1000000.0,s);
 

        /*
         * test speed of kmalloc buffer
         */
	s=0;
        gettimeofday(&t1,NULL);
        for(j=0;j<LOOPCNT;j++){
                for(i=0;i<BUFSIZE;i++){
                        s += va_kmbuf[i];
        	}
	}
        gettimeofday(&t2,NULL);
        printf("kmalloc in userspace          %.3fs (s=%lu)\n",
		((t2.tv_sec-t1.tv_sec)*1000000+(t2.tv_usec-t1.tv_usec))/1000000.0,s);


	/*
	 * test speed of malloc
	 */
	s=0;
	va_mbuf = malloc(BUFSIZE);

        gettimeofday(&t1,NULL);
        for(j=0;j<LOOPCNT;j++){
                for(i=0;i<BUFSIZE;i++){
                        s += va_mbuf[i];
		}
        }
        gettimeofday(&t2,NULL);
        printf("malloc in userspace          %.3fs (s=%lu)\n",
		((t2.tv_sec-t1.tv_sec)*1000000+(t2.tv_usec-t1.tv_usec))/1000000.0,s);

	

exito:
	if(va_dmabuf && va_dmabuf != (char*)0xFFFFFFFF){
		munmap(va_dmabuf,BUFSIZE);
		ioctl(fd,DMAMEM_REL,0);
	}
        if(va_kmbuf && va_kmbuf != (char*)0xFFFFFFFF){
                munmap(va_kmbuf,BUFSIZE);
                ioctl(fd,KMEM_REL,0);
        }
	if(va_mbuf)
		free(va_mbuf);
        if(fd)
                close(fd);
}
