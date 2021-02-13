%global buildforkernels akmod
%global debug_package %{nil}

Name:           hdshm3-kmod
Summary:        Kernel module (kmod) for hdshm3
Version:        0.0.1
Release:        1%{?dist}
License:        GPLv2
URL:            http://wiki.reelbox.org/
Source0:        reelbox-hdshm3-src.tar.gz
# get the needed BuildRequires (in parts depending on what we build for)
BuildRequires:  %{_bindir}/kmodtool
%{!?kernels:BuildRequires: buildsys-build-rpmfusion-kerneldevpkgs-%{?buildforkernels:%{buildforkernels}}%{!?buildforkernels:current}-%{_target_cpu} }
ExclusiveArch:  i686 x86_64


# kmodtool does its magic here
%{expand:%(kmodtool --target %{_target_cpu} --repo rpmfusion --kmodname %{name} %{?buildforkernels:--%{buildforkernels}} %{?kernels:--for-kernels "%{?kernels}"} 2>/dev/null) }

%description
ReelBox eHD extra modules.

%prep
# error out if there was something wrong with kmodtool
%{?kmodtool_check}
# print kmodtool output for debugging purposes:
kmodtool  --target %{_target_cpu} --repo rpmfusion --kmodname %{name} %{?buildforkernels:--%{buildforkernels}} %{?kernels:--for-kernels "%{?kernels}"} 2>/dev/null

%setup -q -c
#Copied from kernel sources

#pushd drivers/staging/hdshm3/
#%patch0 -p3
#%patch1 -p4
#popd

# rename directory
mv driver %{name}-%{version}-src

for kernel_version in %{?kernel_versions} ; do
	cp -a %{name}-%{version}-src _kmod_build_${kernel_version%%___*}
done


%build
for kernel_version  in %{?kernel_versions} ; do
%make_build -C ${kernel_version##*___} \
  M=${PWD}/_kmod_build_${kernel_version%%___*} \
  CONFIG_HDSHM3=m \
  V=1\
  modules
done


%install
for kernel_version in %{?kernel_versions}; do
 mkdir -p ${RPM_BUILD_ROOT}%{kmodinstdir_prefix}/${kernel_version%%___*}/%{kmodinstdir_postfix}/
 install -D -m 755 -t ${RPM_BUILD_ROOT}%{kmodinstdir_prefix}/${kernel_version%%___*}/%{kmodinstdir_postfix}/ $(find _kmod_build_${kernel_version%%___*}/ -name '*.ko')
done
%{?akmod_install}


%changelog
* Wed Feb 10 2021 Peter Bieringer <pb@bieringer.de> 0.0.1-1
- Initial build
