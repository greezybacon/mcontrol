%{!?python_sitelib: %define python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib()")}

Name:       mcontrol
Version:    1.0
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

%files
%defattr(-,dosis,dosis,-)
/home/dosis/bin/mcontrol
/home/dosis/bin/mcontrol-cli
/home/dosis/lib/libmcontrol.so
/opt/dosis/mcontrol
%{python_sitelib}/_motor.so
%{python_sitelib}/mcontrol
%{python_sitelib}/mcontrol*.egg-info

%doc
