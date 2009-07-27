Summary:         NCML module for the OPeNDAP Data server
Name:            ncml_module
Version:         0.9.0
Release:         1
License:         LGPLv2+
Group:           System Environment/Daemons 
Source0:         http://www.opendap.org/pub/source/%{name}-%{version}.tar.gz
URL:             http://www.opendap.org/

BuildRoot:       %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:   libdap-devel >= 3.9.3
BuildRequires:   bes-devel >= 3.7.2

%description
This is the NcML module for our data server.  It parses NcML files to
add metadata to other local datasets on the local Hyrax server. It
returns DAP2 responses.

%prep 
%setup -q

%build
%configure --disable-static --disable-dependency-tracking
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install INSTALL="install -p"

rm $RPM_BUILD_ROOT%{_libdir}/*.la
rm $RPM_BUILD_ROOT%{_libdir}/*.so
rm $RPM_BUILD_ROOT%{_libdir}/bes/*.la

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_bindir}/bes-ncml-data.sh
%{_libdir}/bes/libncml_module.so
%{_datadir}/hyrax/
%doc COPYING COPYRIGHT NEWS README 

%changelog
