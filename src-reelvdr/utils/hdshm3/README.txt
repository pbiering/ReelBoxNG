# requirements

- grub linux boot option add
	iomem=relaxed

# RPM/akmods

- create "common" RPM
$ rpmbuild -bb contrib/hdshm3.spec

- create "kmod" RPM
$ pushd x86
$ tar cfzh ~/rpmbuild/SOURCES/reelbox-hdshm3-src.tar.gz driver
$ popd
$ rpmbuild -bb contrib/hdshm3-kmod.spec


- install "common" RPM as user "root"
# version="0.0.1-1.f33.x86_64"
# cd /path/to/RPMS
# rpm -Uhv hdshm3-common-$version.rpm hdshm3-$version.rpm kmod-hdshm3-$version.rpm hdshm3-$version.rpm
