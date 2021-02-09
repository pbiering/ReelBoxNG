/*****************************************************************************/
/*
  Decypher PCI/U-Boot Loader
  
  (c) 2007-2008 Georg Acher, BayCom GmbH (acher at baycom dot de)

  #include <GPLv2-header>
 
*/
/*****************************************************************************/

#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <asm/mtrr.h>

#include "hdshm.h"

#define HD_MEM_SIZE (128*1024*1024)

#define MAX_IMAGE_SIZE (16*1024*1024)
#define MIN_IMAGE_SIZE (1*1024*1024)
#define IMG_LOAD_ADDR  (0x02001000)
#define U_BOOT_TIMEOUT 10

// Selected Decypher defines

#define PERIPHERAL_BASE         0x0
#define SEM_REG_BASE      (PERIPHERAL_BASE + 0x00080000)
#define SEM_0_SCRATCH_REG (SEM_REG_BASE + 0x00000100)
#define SEM_3_SCRATCH_REG (SEM_REG_BASE + 0x0000010c)
#define CCB_REG_BASE      (PERIPHERAL_BASE + 0x00100000)
#define CCB_REG_CHIPINFO  (CCB_REG_BASE + 0x0c)
#define CCB_REG_CCBCTRL   (CCB_REG_BASE + 0x14)
#define PCI_REG_BASE      (PERIPHERAL_BASE + 0x00180000)

/*------------------------------------------------------------------------*/
int find_decypher(void)
{
	int fd;
	int bar1;
	fd=open("/dev/hdshm",O_RDWR);
	if (fd<0) {
		fprintf(stderr,"Can't open /dev/hdshm to determine Decypher PCI BAR1.\n");
		exit(-1);
	}
	bar1=ioctl(fd, IOCTL_HDSHM_PCIBAR1, 0);
	close(fd);
	printf("Decypher PCI BAR1: %x\n",bar1);
	return bar1;
}
/*------------------------------------------------------------------------*/
void hexdumpi(volatile void *b, int l)
{
        int n;
        int *x=(int*)b;
        for(n=0;n<l;n++)
        {
                if ((n%8)==0) printf("%08x   ",n*4);
                printf("%08x ",*(x+n));
                if ((n%8)==7)
                        puts("");
        }
        puts("");
}
/*------------------------------------------------------------------------*/
unsigned char* pcimap(off_t offset)
{
        int fd1;
	void *base;
		
        if ((fd1 = open("/dev/mem", O_RDWR)) < 0) {
                fprintf(stderr, "Failed to open /dev/mem (%s). Are you root?\n",
                           strerror(errno));
		return 0;
        }

        base = mmap(0, HD_MEM_SIZE,
                                    PROT_READ|PROT_WRITE,
                                    MAP_SHARED, fd1,
#ifndef __x86_64
                                    offset);
#else
                                    (unsigned)offset);
#endif
        if (base==MAP_FAILED) {       
                fprintf(stderr, " Failed to mmap PCI (%s)\n",
                       strerror(errno));
		return 0;
        }
	
	return (unsigned char*)base;
}
/*------------------------------------------------------------------------*/
void disable_pci_burst(unsigned long base)
{
	struct mtrr_gentry gmtrr;
	struct mtrr_sentry mtrr;
	int fd;
	fd=open ("/proc/mtrr", O_RDWR, 0);
	if (fd>=0) {
		for (gmtrr.regnum = 0; ioctl (fd, MTRRIOC_GET_ENTRY, &gmtrr) == 0; ++gmtrr.regnum) {

			if (gmtrr.size>0 && gmtrr.base==base) {
				printf("Removed MTRR reg %i @ %lx\n",gmtrr.regnum, base);
				mtrr.base=base;
				mtrr.size=HD_MEM_SIZE;
				ioctl(fd,MTRRIOC_KILL_ENTRY,&mtrr);
				close(fd);
				return;
			}
		}
		close(fd);
	}
}
/*------------------------------------------------------------------------*/
void enable_pci_burst(unsigned long base, unsigned char *pci_base)
{
	volatile unsigned char * pci_addr;
	struct mtrr_sentry mtrr;
	int fd;
	int chipvers;

	// Don't enable bursts on RevA chips
	chipvers=*(volatile int*)(pci_base+CCB_REG_CHIPINFO);
	if ((chipvers&0xf)==0)
		return;

	pci_addr=pci_base+PCI_REG_BASE+0x158;
	*(int*)pci_addr=0x60; // Enable PCI-bursts

	// Enable write combining
	// FIXME: Just DRAM?
	mtrr.type=MTRR_TYPE_WRCOMB;
	mtrr.base=base;
	mtrr.size=HD_MEM_SIZE;
	fd=open ("/proc/mtrr", O_WRONLY, 0);
	if (fd>=0) {
		printf("Set MTRR @ %lx\n",base);
		ioctl (fd, MTRRIOC_SET_ENTRY, &mtrr);
		close(fd);
	}
}
/*------------------------------------------------------------------------*/
int load(char *fname, unsigned char *pci_base, int jump, int verify, char *cmdline)
{
        volatile unsigned char * load_addr;
        char *buf;
        int fd;
        int n,l;

        load_addr=pci_base+IMG_LOAD_ADDR;

	fd=open("/dev/hdshm",O_RDWR);
	if (fd>=0) {
		ioctl(fd, IOCTL_HDSHM_SHUTDOWN, 0);	// Clear booted-flag
		close(fd);
	}

        fd=open(fname,O_RDONLY);
	if (fd<0) {
		fprintf(stderr,"File <%s> not found\n",fname);
		return 0;
	}

        buf=malloc(MAX_IMAGE_SIZE); // 16MB are enough
	if (!buf) {
		fprintf(stderr,"Out of memory\n");
		return 0;
	}

        l=read(fd,buf,MAX_IMAGE_SIZE);
        printf("Uploading %s, Length %i, virtual %p, MIPS 0x%x\n",fname,l,load_addr,IMG_LOAD_ADDR);
        if (l>MIN_IMAGE_SIZE) {
		if (*(int*)buf==0x12345678 && *(int*)(buf+4)!=0 && !jump) {
			jump=*(int*)(buf+4);
			printf("Detected kernel entry %x\n",jump);
		}

		memcpy((void*)load_addr,buf,(l+3)&~3);
		printf("memcpy done\n");
		if (verify) {
			volatile int *src1,*src2;
			printf("Verify\n");
			src1=(volatile int*)buf;
			src2=(volatile int*)load_addr;
			for(n=0;n<l;n+=4) {
				if (*src1!=*src2) {
					printf("DDR Verify Error %x is %x, should be %x\n",n+IMG_LOAD_ADDR,*src2,*src1);
					puts("IS:");
					hexdumpi(src2,32);
					puts("SHOULD BE:");
					hexdumpi(src1,32);
					exit(-1);
				}
				src1++;
				src2++;
			}
			printf("Verify OK\n");

		}
		// cmdline is inserted after kernelentry (offset 8)
		strcpy((char*)load_addr+8,cmdline);
		
		if (!jump)
			jump=*(int*)(buf+128); 
		if (!jump) {
			fprintf(stderr,"Invalid kernel entry\n");
			return 0;
		}
	

		return jump;		
        }
	else {
		fprintf(stderr,"File too small to be a real boot image\n");
		return 0;
	}
}
/*------------------------------------------------------------------------*/
void warm_reset(unsigned char *pci_base)
{
	int fd;
//	printf("%x\n",CCB_REG_CCBCTRL);
	puts("Warm Reset");
	fd=open("/dev/hdshm",O_RDWR);
	if (fd>=0) {
		ioctl(fd, IOCTL_HDSHM_SHUTDOWN, 0);	// Clear booted-flag
		close(fd);
	}

	*(volatile int*)(pci_base+CCB_REG_CCBCTRL)|=1;
	*(volatile int*)(pci_base+CCB_REG_CCBCTRL)&=~1;
	*(volatile int*)(pci_base+SEM_3_SCRATCH_REG)=0;
}
/*------------------------------------------------------------------------*/
void run_cpu(unsigned char *pci_base, int jump)
{
	printf("Start CPU#0 at %x\n",jump);
	*(volatile int*)(pci_base+SEM_0_SCRATCH_REG)=jump;
	*(volatile int*)(pci_base+SEM_REG_BASE)=0;
}
/*------------------------------------------------------------------------*/
// Wait until u-boot is ready to boot

int wait_for_pciboot(unsigned char *pci_base, int timeout_s)
{
	int n;
	for(n=0;n<1+timeout_s*10;n++) {
//		printf("%x\n",*(volatile int*)(pci_base+SEM_3_SCRATCH_REG));
		if (*(volatile int*)(pci_base+SEM_3_SCRATCH_REG)==0xb001abcd)
			return 0;
		usleep(100*1000);
	}
	return 1;
}
/*------------------------------------------------------------------------*/
void upload_start(unsigned char *pci_base, char *fname, int jump, int verify, char *cmdline)
{		
	if (wait_for_pciboot(pci_base,U_BOOT_TIMEOUT)) {
		fprintf(stderr,"Timeout: U-Boot not ready for PCI boot\n");
		exit(-1);
	}
	jump=load(fname, pci_base,jump, verify, cmdline);

	if (!jump)
		exit(-1);
	run_cpu(pci_base,jump);
}
/*------------------------------------------------------------------------*/
int wait_kernel(int timeout)
{
	int n,fd,stat;
	fd=open("/dev/hdshm",O_RDWR);
	if (fd<0)
		return -1;


	for(n=0;n<timeout;n++) {
		stat=ioctl(fd, IOCTL_HDSHM_GET_STATUS);
		if (!(stat&HDSHM_STATUS_NOT_BOOTED)) {
			close(fd);
			return 0;
		}
		printf("WAIT %i  \r",n);
		fflush(stdout);
		sleep(1);
	}
	close(fd);
	return 1;
}
/*------------------------------------------------------------------------*/
void usage(void)
{
	fprintf(stderr,
		"Usage: hdboot <options>\n"
		"       -r        Force warm reset\n"
		"       -p <hex>  Manually set PCI BAR1 (DANGEROUS!!!)\n"
		"       -i <file> Upload file (default: linux.bin)\n"
		"       -e <hex>  Manually set kernel entry address (default: auto detect)\n"
		"       -v        Verify DDR-RAM after upload\n"
		"       -w <timeout>  Wait until kernel is up\n"
		"       -c <string>   Commandline\n"
		"       -M         Disable MTRR speedup\n");
	exit(1);
}
/*------------------------------------------------------------------------*/
int main(int argc, char ** argv)
{
	unsigned char *pci_base;
	int bar1=0;
	int do_reset=0;
	int i;
	char fname[256]="";
	char cmdline[256]="";
	int entry= 0;
	int verify=0;
	int wait=-1;
	int no_mtrr=0;

	while ((i = getopt(argc, argv, "c:e:i:p:rvw:M")) != -1) {
		switch (i) 
		{
		case 'c':
			strncpy(cmdline,optarg,255);
			cmdline[255]=0;
			break;
		case 'e':
			entry=strtoul(optarg,NULL,16);
			break;
		case 'i':
			strcpy(fname,optarg);
			break;
		case 'p':
			bar1=strtoul(optarg,NULL,16);
			printf("Overriding PCI BAR1 to %08x\n",bar1);
			break;
		case 'r':
			do_reset=1;
			break;
		case 'v':
			verify=1;
			break;
		case 'w':
			wait=atoi(optarg);
			break;
		case 'M':
			no_mtrr=1;
			break;
		default:
			usage();
			break;
		}
	}

	if (bar1==0)
		bar1=find_decypher();

	pci_base=pcimap(bar1);

	if (strlen(fname)) {
		disable_pci_burst(bar1);
		if (wait_for_pciboot(pci_base,0) || do_reset) {
			warm_reset(pci_base);
			sleep(2);
		}
		upload_start(pci_base, fname, entry, verify, cmdline);
		if (!no_mtrr)
			enable_pci_burst(bar1, pci_base);
	}

	if (wait>=0)
		exit(wait_kernel(wait));

	exit(0);
}
