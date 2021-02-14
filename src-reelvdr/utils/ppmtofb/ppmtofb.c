/*****************************************************************************
 * 
 * Tool to display PPM-pics on the framebuffer
 *
 * (c) Tobias Bratfisch, tobias (at) reel-multimedia (dot) com
 *
 * #include <GPL-header>
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/fb.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <string.h>

struct osd_s
{
    int fd; // fd for framebuffer-device
    int bpp;
    int width;
    int height;
    unsigned char * data; // fb mmaped
    unsigned char * buffer;
};

typedef struct osd_s osd_t;

osd_t *osd;
char fb[256]="";

void print_usage(void) {
   printf("ppmtofb: Displays PPM-pics on the framebuffer\n");
   printf("Usage: ppmtofb [-f <framebuffer device>] -p <picture filename>\n");
   printf("PPM pic has to be in plain-format and may only have max. 255 colours per channel\n");
}

osd_t *new_osd(void) {
    osd = (osd_t*)malloc(sizeof(osd_t));

    if(!osd) {
       printf("ERROR: couldn't malloc in %s: %i\n", __FILE__, __LINE__);
       abort();
    }

    osd->fd = open(fb, O_RDWR|O_NDELAY);

    if(osd->fd==-1) {
       printf("couldn't open %s, error: %s\n", fb, strerror(errno));
		 return NULL;
	 };
    struct fb_var_screeninfo screeninfo;
    ioctl(osd->fd, FBIOGET_VSCREENINFO, &screeninfo);

    osd->bpp = screeninfo.bits_per_pixel/8; 
    osd->width = screeninfo.xres;
    osd->height = screeninfo.yres;

    osd->data = (unsigned char*) mmap(0, osd->bpp* osd->width * osd->height, PROT_READ|PROT_WRITE, MAP_SHARED, osd->fd, 0);

    assert(osd->data!=(void*)-1);
    /* clear osd */
    memset(osd->data,0,osd->bpp*osd->width*osd->height);

    osd->buffer = (unsigned char*) malloc(osd->bpp * osd->width * osd->height);
    if(!osd->buffer) {
       printf("ERROR: couldn't malloc in %s: %i\n", __FILE__, __LINE__);
       abort();
    }

    printf("FB: xres: %i, yres: %i, bpp: %i, osd->data: %p\n", osd->width, osd->height, osd->bpp, osd->data);

    return osd;
}

int main(int argc, char** argv){
   char *buf = (char*)malloc(64);
   int w,h;
	int i;
   char pict[16384]="";

   while ((i = getopt(argc, argv, "f:p:h?")) != -1) {
      switch(i) {
         case 'f':
				snprintf(fb, sizeof(fb), "%s", optarg);
				printf("INFO : given framebuffer device: %s\n", fb);
				break;
         case 'p':
				snprintf(pict, sizeof(pict), "%s", optarg);
				printf("INFO : given picture file: %s\n", pict);
				break;
         case '?':
         case 'h':
         default:
				print_usage();
				exit(1);
				break;
      };
   };

	if (strlen(pict) == 0) {
      printf("ERROR: no picture file given\n");
		print_usage();
		exit(1);
	};

   FILE *fd;
   if(strcmp(pict, "-")){
     fd = fopen(pict, "r");
   } else
     fd = stdin; 
  
   printf("opening: %s\n", pict);

   if(fd==NULL){
      printf("ERROR: couldn't open %s\n", pict);
		exit(1);
   }

  do {
      buf[0]=0;
      fgets(buf, 20, fd);
  } while (buf[0]=='#');

  if (strcmp(buf,"P6\n")) {
    printf("ERROR: pic %s isn't a PPM in P6 format\n",pict);
	 exit(1);
  }

  do {
      buf[0]=0;
      fgets(buf, 20, fd);
  } while (buf[0]=='#');
          
  osd = new_osd();
  if (osd == NULL) {
    printf("ERROR: can't open OSD\n");
	 exit(1);
  };
             
  sscanf(buf,"%i %i",&w,&h);
  if (w!=osd->width || h!=osd->height) {
    printf("ERROR: size mismatch, framebuffer %ix%i, pic %ix%i\n",osd->width,osd->height,w,h);
    exit(1);
  }
  
  do {
      buf[0]=0;
      fgets(buf, 20, fd);
  } while (buf[0]=='#');
  
  
   int counter = 0;
   size_t res=1;
   /* now: we are at the beginnig of the pic */
   while(res){
     res = fread(buf, 1, 3, fd); /* got the RGB-values */
     if(res==3){
       osd->buffer[counter]=buf[2];   // B
       osd->buffer[counter+1]=buf[1]; // G
       osd->buffer[counter+2]=buf[0]; // R
       int sum=2*(buf[0]+buf[1]+buf[2]);
       if (sum>240)
         osd->buffer[counter+3]=240;
      else
         osd->buffer[counter+3]=sum;
       counter+=4;
     }
   }

   memcpy(osd->data, osd->buffer, osd->width*osd->height*osd->bpp);

   free(buf);
   fclose(fd);
   return 0;
}

/* vim: set tabstop=3 shiftwidth=3 noexpandtab: */
