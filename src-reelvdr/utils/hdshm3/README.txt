### requirements

- grub linux boot option add
	iomem=relaxed

	FedoraLinux:
	- edit /etc/sysconfig/grub
		- add option to GRUB_CMDLINE_LINUX
	- update grub configuration
		# grub2-mkconfig -o /boot/grub2/grub.cfg

## RPM/akmods

- prepare basic RPM build
$ cd ~
$ rpmdev-setuptree

- create "common" RPM
$ rpmbuild -bb contrib/hdshm3.spec

- create "kmod" RPM
$ pushd x86
$ tar cfzh ~/rpmbuild/SOURCES/reelbox-hdshm3-src.tar.gz driver
$ popd
$ cp contrib/hdshm3-kmod.spec ~/rpmbuild/SPECS	#required for akmods RPM build
$ rpmbuild -bb contrib/hdshm3-kmod.spec


- install "common" RPM as user "root"
# version="0.0.1-1.fc35.x86_64"
# cd /path/to/RPMS
# rpm -Uhv hdshm3-$version.rpm kmod-hdshm3-$version.rpm akmod-hdshm3-$version.rpm

- run "akmods"
akmods

- insert module

with all debugging enabled:
# insmod /lib/modules/$(uname -r)/extra/hdshm3/hdshm.ko has_fb=0 hd_dbg_mask=0xffffffff

no debugging enabled
# insmod /lib/modules/$(uname -r)/extra/hdshm3/hdshm.ko

no debugging enabled and framebuffer device created
# insmod /lib/modules/$(uname -r)/extra/hdshm3/hdshm.ko has_fb=1
