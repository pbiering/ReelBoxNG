#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "hdshmlib.h"
#include "hdshm_user_structs.h"

/* --------------------------------------------------------------------- */
void usage(void)
{
	fprintf(stderr,
		"hdfbctl <options>\n"
		" -i      Increase transparency\n"
		" -d      Decrement transparency\n"
		" -a <n>  Set alpha\n"
		" -f      Full screen\n"
		" -c      Center\n"
		" -F      Switch full/center\n");
}
/* --------------------------------------------------------------------- */
void set_alpha(hd_data_t *hdd, int val)
{
	if (val<0)
		val=0;
	if (val>255)
		val=255;
	hdd->plane[2].alpha=val;
}
/* --------------------------------------------------------------------- */
int main(int argc, char **argv)
{
	hdshm_area_t *hsa;
	hd_data_t *hdd;

	if (hd_init(0)) {
                fprintf(stderr,"hdctrld: hdshm driver not loaded!\n");
                exit(0);
        }
	hsa=hd_get_area(HDID_HDA);
	if (hsa) {
		int changed=0;
		hdd=(hd_data_t*)hsa->mapped;
		if (!hdd)
			exit(0);
		while (1)
		{
			char c = getopt(argc, argv, "ida:fcFh?");
			if (c == -1)
				break;
			switch(c) {
			case 'd':
				set_alpha(hdd,hdd->plane[2].alpha+32);
				changed=1;
				break;
			case 'i':
				set_alpha(hdd,hdd->plane[2].alpha-32);
				changed=1;
				break;
			case 'a':
				set_alpha(hdd,atoi(optarg));
				changed=1;
				break;
			case 'f':
				hdd->plane[2].ws=0;
				hdd->plane[2].hs=0;
				changed++;
				break;
			case 'c':
				hdd->plane[2].ws=-1;
				changed++;
				break;
			case 'F':
				if (hdd->plane[2].ws==-1)
					hdd->plane[2].ws=0;
				else
					hdd->plane[2].ws=-1;
				changed++;
				break;
			case '?':
			case 'h':
			default:
				usage();
				exit(0);
				break;
			}
		}

		if (changed)
			hdd->plane[2].changed++;		

	}
	exit(0);
}
