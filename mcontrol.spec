%{!?python_sitelib: %define python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib()")}

Name:       mcontrol
Version:    0.1
Release:    1%{?dist}
Summary:    Comprehensive motor control suite
Group:      Manchac Technologies, LLC
License:    Proprietary
BuildRoot:  %{_topdir}/BUILDROOT
Source:     mcontrol-source.tar

BuildRequires: Cython 

%description
Comprehensive motor control suite with an emphasis on consistent motor
control across motor types and brands and reliable communications to those
motors.

%prep
%setup -q

%build
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%clean
rm -rf %{buildroot}

%package libs
Summary:    mcontrol libraries
Group:      Manchac Technologies, LLC
%description libs
Libraries for the mcontrol software

%package cli
Summary:    Command-line interface for mcontrol
Group:      Manchac Technologies, LLC
Requires:   python >= 2.6
%description cli
Command-line interface for mcontrol

%package drivers-mdrive
Summary:    Drivers and firmware for the MDrive motor suite
Group:      Manchac Technologies, LLC
%description drivers-mdrive
MDrive driver for mcontrol and stock and custom firmwares. This package also
include the microcode for the MDrive motors in the dosis-1.x software

%files
%defattr(-,dosis,dosis,-)
/home/dosis/bin/mcontrold

%files libs
%defattr(-,dosis,dosis,-)
/home/dosis/lib/libmcontrol.so

%files drivers-mdrive
%defattr(-,dosis,dosis,-)
/opt/dosis/mcontrol/drivers
/opt/dosis/mcontrol/firmware/MDI*
/opt/dosis/mcontrol/microcode/dosis*

%files cli
%{python_sitelib}/_motor.so
%{python_sitelib}/mcontrol
%{python_sitelib}/mcontrol*.egg-info
/home/dosis/bin/mcontrol-cli

%doc
