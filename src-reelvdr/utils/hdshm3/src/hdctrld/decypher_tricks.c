/*
  decypher_tricks.c
  
  Accessing features of the DeCypher in HW

*/

#ifdef CONFIG_MIPS
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include  <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>

#define PERIPHERAL_BASE  0x18000000   
#define DSPC_REG_BASE           (PERIPHERAL_BASE + 0x00600000)
#define DSPC_VF_BASE           (PERIPHERAL_BASE + 0x00600000 + 0x00040000)

#define DAO_REG_BASE           (PERIPHERAL_BASE + 0x00780000)
#define DAOCfgSPDIF_REG        (dao_base + 0x00008)    
#define AO_SPDIF_Valid_REGSHIFT         6                       

static volatile unsigned char *dspc_base;
static volatile unsigned char *dspc_vf_base;
static volatile unsigned char *dao_base;


#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MINMAX(x, min, max) MAX(MIN((x), (max)), (min))

/*------------------------------------------------------------------------*/
//                             FILTERS
/*------------------------------------------------------------------------*/
#define POW2(b) ((1<<(b)))
#define PACK_NUM(x, b) ((x)<0 ? POW2((b) - 1) - (x) : (x))

// 9 coefficients are packed into 64 bits
// The values on the borders have lower data range

#define SCALE_PACK0(c0, c1, c2, c3, c4) ( (PACK_NUM(c0, 5) << (27)) | \
					  (PACK_NUM(c1, 6) << (21)) | \
					  (PACK_NUM(c2, 8) << (13)) | \
					  (PACK_NUM(c3, 9) << (4))  | \
					  (PACK_NUM(c4, 8) >> 4)      \
		)

#define SCALE_PACK1(c8, c7, c6, c5, c4) ( (PACK_NUM(c4, 8) << (28)) | \
					  (PACK_NUM(c5, 9) << (19)) | \
					  (PACK_NUM(c6, 8) << (11)) | \
					  (PACK_NUM(c7, 6) << (5))  | \
					  (PACK_NUM(c8, 5))           \
		)
/*------------------------------------------------------------------------*/
// Only vertical filter
// FIXME: Where's the horizontal filter?

void set_vfilter_raw(int coefs[17][9], int fnum)
{
	int n;
	unsigned int *z=(unsigned int*)dspc_vf_base;
	z+=fnum*2*17;
	for(n=0;n<17;n++) {	
#if 0
		printf("%i %i %i %i %i %i %i %i %i\n",
		       coefs[n][0],coefs[n][1],coefs[n][2],coefs[n][3],coefs[n][4],
		       coefs[n][5],coefs[n][6],coefs[n][7],coefs[n][8]);
#endif
		*z++=SCALE_PACK1(coefs[n][8],coefs[n][7],coefs[n][6],coefs[n][5],coefs[n][4]);
		*z++=SCALE_PACK0(coefs[n][0],coefs[n][1],coefs[n][2],coefs[n][3],coefs[n][4]);
	}
}
/*------------------------------------------------------------------------*/
// Quick and dirty coefficient generation
// mode==1: SDTV-out, disable inter-field filtering
void decypher_set_vfilter(int mode, int val)
{
	int coefs[17][9]={{0}};
	int n,f=val;
	
	if (mode==0) {
		for(n=0;n<16;n++) {
			coefs[n][4]=128-n*f;
			coefs[n][5]=n*f;	
		}
		
		coefs[n][2]=0;	
		coefs[16][3]=128-n*f;
		coefs[16][4]=n*f;
	}
	else if (mode==1) {
		for(n=0;n<16;n++) {
			coefs[n][4]=128-n*f;
			coefs[n][6]=n*f;
		}
		
		coefs[16][2]=128-n*f;
		coefs[16][4]=n*f;

	}
	else if (mode==2) {
		for(n=0;n<16;n++) {
 			coefs[n][3]=64;
 			coefs[n][4]=64;
		}
 		coefs[16][2]=64;
                coefs[16][3]=64;
	}
	set_vfilter_raw(coefs, 0); // Y
	set_vfilter_raw(coefs, 1); // UV
}

/*------------------------------------------------------------------------*/
//                         BRIGHTNESS/CONTRAST/GAMMA
/*------------------------------------------------------------------------*/
/* The DeCypher does its gamma calculation not via a lookup table (as
   usual), but by segmented interpolation.  There are 15 segments from
   0 to 1023 (10Bit input data), these are defined in gamma_segments[].

   Each interpolation needs two values: offset and factor.
   The offsets and factors are packed (3 in an int), first factor also has enable bit.
*/
/*------------------------------------------------------------------------*/
unsigned int gamma_segments[15]={0,16,32,64,96,128,192,
				 256,320,384,512,640,768,896,1023};

void write_gamma(volatile unsigned char *z,unsigned int gcoefs[14][2])
{
	unsigned int *x=(unsigned int*)z;
	unsigned int a,b,c,v,n;

	// Offsets
	a=gcoefs[0][0];
	b=gcoefs[1][0];
	c=gcoefs[2][0];
	v=(a<<1)|(b<<11)|(c<<21)|1; // 1: enable custom gamma

	*x++=v;
	x++;
	for(n=3; n<14;n+=3) {
		a=gcoefs[n+0][0];
		b=gcoefs[n+1][0];
		c=gcoefs[n+2][0];

		v=(a<<0)|(b<<10)|(c<<20);
		*x++=v;
		x++;
	}
	// Factors
	for(n=0; n<14;n+=3) {
		a=gcoefs[n+0][1];
		b=gcoefs[n+1][1];
		c=gcoefs[n+2][1];

		v=(a<<0)|(b<<10)|(c<<20);
		*x++=v;
		x++;
	}
}
/*------------------------------------------------------------------------*/
void decypher_disable_bcg (void)
{
	unsigned int *x;
	x=(unsigned int*)(dspc_base+0x420);
	*x=0;
	x=(unsigned int*)(dspc_base+0x420+0x50);
	*x=0;
	x=(unsigned int*)(dspc_base+0x420+0x50*2);
	*x=0;
	printf("disabled bcg\n");
}
/*------------------------------------------------------------------------*/
// brightness -1000...+1000
void decypher_set_bcg(double gamma_val, int brightness, double contrast)
{
	int  n;
        double real_gamma[16];
	double factors[16];
	unsigned int gcoefs[16][2];

	// Calc points for gamma curve
        for (n=0;n<16;n++) {
                real_gamma[n]=MINMAX(brightness+contrast*(1023.0+brightness)*pow(gamma_segments[n]/1023.0,1.0/gamma_val),0,1023);
	}

	// Calc factor for each segment
        for (n=0;n<15;n++) {
                factors[n]=(real_gamma[n+1]-real_gamma[n])/(gamma_segments[n+1]-gamma_segments[n]); 
                // factor=64 is equivalent to 1.0
                gcoefs[n][1]=(unsigned int)(factors[n]*64+0.5);
        }

	// Calc offset for each segment
        gcoefs[0][0]=(unsigned int)real_gamma[0];
        for (n=1; n<15; n++)
                gcoefs[n][0]=MINMAX(gcoefs[n-1][0] + 
				      ((gcoefs[n-1][1]*(gamma_segments[n]-gamma_segments[n-1])+32)>>6),0,1023);

#if 0
	for(n=0;n<15;n++) {
		printf("%i %f %i %i %f\n",n,real_gamma[n],gcoefs[n][0],gcoefs[n][1],factors[n]);
	}
#endif
	// For RGB
	write_gamma(dspc_base+0x420,gcoefs);
	write_gamma(dspc_base+0x420+0x50,gcoefs);
	write_gamma(dspc_base+0x420+2*0x50,gcoefs);
}
/*------------------------------------------------------------------------*/
void spdif_mute(void)
{
	*(volatile int*)DAOCfgSPDIF_REG=(*(volatile int*)DAOCfgSPDIF_REG)|(1<<AO_SPDIF_Valid_REGSHIFT);
}
/*------------------------------------------------------------------------*/
int init_decypher_tricks(void)
{
	int fd1;

        if ((fd1 = open("/dev/mem", O_RDWR)) < 0) {
                printf(" Failed to open /dev/mem (%s)\n",
                           strerror(errno));
                exit(-1);
        }

        dspc_base = (volatile unsigned char*)mmap(0, 4*1024,
						  PROT_READ|PROT_WRITE,
						  MAP_SHARED, fd1,
						  (off_t)DSPC_REG_BASE);
        if ((int)dspc_base==-1) {       
                printf(" Failed to mmap (%s)\n",
                       strerror(errno));
                exit(-1);               
        }

	dspc_vf_base = (volatile unsigned char*)mmap(0, 4*1024,
						  PROT_READ|PROT_WRITE,
						  MAP_SHARED, fd1,
						  (off_t)DSPC_VF_BASE);
        if ((int)dspc_vf_base==-1) {       
                printf(" Failed to mmap (%s)\n",
                       strerror(errno));
                exit(-1);               
        }

	dao_base = (volatile unsigned char*)mmap(0, 4*1024,
						  PROT_READ|PROT_WRITE,
						  MAP_SHARED, fd1,
						  (off_t)DAO_REG_BASE);
        if ((int)dao_base==-1) {       
                printf(" Failed to mmap (%s)\n",
                       strerror(errno));
                exit(-1);               
        }

	return 0;
}
/*------------------------------------------------------------------------*/

#endif
