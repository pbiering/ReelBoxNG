/*****************************************************************************/
/*
  Network emulation for Decypher via tun-device + shared memory

  (c) 2007 Georg Acher, BayCom GmbH (acher at baycom dot de)

  #include <GPLv2-header>
*/
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include "hdchannel.h"

#define HDSHM_SHMNET1_RX  20
#define HDSHM_SHMNET1_TX  21

#define MASTER_IP "192.168.99.129"
#define HOST_IP "192.168.99.130"
#define NETMASK "255.255.255.252"

#ifdef CONFIG_MIPS
#define MAX_BUSY_LOOPS 20
#else
#define MAX_BUSY_LOOPS 50
#endif

int tun_alloc(char *dev)
{
      struct ifreq ifr;
      int fd, err;

      if( (fd = open("/dev/net/tun", O_RDWR|O_NONBLOCK)) < 0 )
         return -1;

      memset(&ifr, 0, sizeof(ifr));     
      ifr.ifr_flags = IFF_TUN; 
      if( *dev )
         memcpy(ifr.ifr_name, dev, IFNAMSIZ);

      if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
         close(fd);
         return err;
      }

      return fd;
}

int main(int argc, char** argv)
{
	int master;
	hd_channel_t *ch_rx,*ch_tx;
	int tunfd;
        int l,ll,ret;	
	
	while(1) {
                if(!hd_init(0))
		        break;
		sleep(1);
	}

#ifdef CONFIG_MIPS
	master=1;         // Decypher is master
#else
	master=0;
#endif

	if (master) {
                ch_rx=hd_channel_create(HDSHM_SHMNET1_RX, 65536, HD_CHANNEL_READ);
	        ch_tx=hd_channel_create(HDSHM_SHMNET1_TX, 65536, HD_CHANNEL_WRITE);
	}
	else {
	        while(1) {
        	        ch_tx=hd_channel_open(HDSHM_SHMNET1_RX);
        	        if (ch_tx)
        	                break;
                        sleep(1);
                }
	        while(1) {
        	        ch_rx=hd_channel_open(HDSHM_SHMNET1_TX);
        	        if (ch_tx)
        	                break;
                        sleep(1);
                }
	}
	printf("GOT SHM-CHANNELS\n");
	
	while(1) {
	        tunfd=tun_alloc("tun0");
	        if (tunfd >= 0)
	               break;
	        sleep(1);
	}

	if (master)
        	system("/sbin/ifconfig tun0 " MASTER_IP " netmask " NETMASK " mtu 8300;"
        	       "route add default gw " HOST_IP );
        else
        	system("/sbin/ifconfig tun0 " HOST_IP " netmask " NETMASK " mtu 8300");

	printf("SHMNETD Running...\n");

        while(1) {
                char buffer[10000];
                void *txbuffer;

		struct pollfd pfd={tunfd,POLLIN,0};

		if (!ll) 
			ret=poll(&pfd, 1, 4);
		else
			ret=1;

		if (ret) {
			l=read(tunfd,buffer,10000);
			if (l>=0) {
				hd_channel_write(ch_tx,buffer,l,10*1000);
			
			}
		}

		ll=0;
                if (hd_channel_read_start(ch_rx,&txbuffer,&ll,0)) {
                        write(tunfd, txbuffer, ll);
                        hd_channel_read_finish(ch_rx);
                }
        }        
}
