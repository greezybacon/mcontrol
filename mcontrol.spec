%{!?python_sitelib: %define python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib()")}
%{!?python_sitearch: %define python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)")}

# The GIT_VERSION token below is automatically changed to the value of
# `git describe` by the top-level Makefile's "rpm" build target
%define git_version GIT_VERSION

#
# The git version is <tag>-#-sha1, where the <tag> is the most recent
# annotated git tag, # is the number of commits since the tag, and the
# sha1 is 'g' plus the first 7 chars of the most recent commit sha1-id
# on the branch being compiled
#
# This will split the version information back into the <tag> (version)
# and the #-sha1 (post_tag) parts
#
%define version %(echo %{git_version} | cut -d- -f1)
%define post_tag %(echo %{git_version} | cut -s -d- -f2- | sed -e s/-/_/g)
%if x%{post_tag} == "x" 
    %undefine post_tag
%endif
 
Name:       mcontrol
Version:    %{version}
Release:    %{post_tag}%{?dist}
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
make -j1 #%{?_smp_mflags}

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
%{python_sitearch}/_motor.so
%{python_sitearch}/mcontrol
%{python_sitearch}/mcontrol*.egg-info
/home/dosis/bin/mcontrol-cli

%doc
