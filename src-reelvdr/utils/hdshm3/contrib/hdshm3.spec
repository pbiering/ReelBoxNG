Name:           hdshm3
Summary:        Kernel module (kmod) for hdshm3
Version:        0.0.1
Release:        1%{?dist}
License:        GPLv2
URL:            http://wiki.reelbox.org/

ExclusiveArch:  i686 x86_64
Provides:       %{name}-kmod-common = %{version}
Requires:       %{name}-kmod >= %{version}


%description
ReelBox eHD common files.

%prep
#setup -q -c


%build
#Nothing to build


%install
#Nothing to prep

%files
#no files

%changelog
* Wed Feb 10 2021 Peter Bieringer <pb@bieringer.de> 0.0.1-1
- Initial build
