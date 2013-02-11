Name:       mcontrol
Version:    1.0
Release:    1%{?dist}
Summary:    Comprehensive motor control suite
Group:      Manchac Technologies, LLC
License:    Proprietary
Source0:    
BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: Cython 
Requires:   

%description
Comprehensive motor control suite with an emphasis on consistent motor
control across motor types and brands and reliable communications to those
motors.

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%clean
rm -rf %{buildroot}

%files
%defattr(-,dosis,dosis,-)
%doc
