Name:        juise
Version:     0.9.0
Release:     1%{?dist}
Summary:     JUNOS User Interface Scripting Environment

Prefix:      /usr

Vendor:      Juniper Networks, Inc.
Packager:    Phil Shafer <phil@juniper.net>
License:     BSD

Group:       Development/Libraries
URL:         https://github.com/Juniper/libslax
Source0:     https://github.com/Juniper/juise/releases/0.9.0/juise-0.9.0.tar.gz

BuildRequires:  libxml2-devel
BuildRequires:  libxslt-devel
BuildRequires:  curl-devel
BuildRequires:  libedit-devel
BuildRequires:  libslax
BuildRequires:  libssh2
BuildRequires:  bison-devel
BuildRequires:  bison

Requires: libslax
Requires: libxml2
Requires: libxslt
Requires: sqlite
Requires: libssh2

%description
Welcome to juise, the JUNOS User Interface Scripting Environment.
This library adds the JUNOS-specific bits to the base SLAX language.

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
%make_install

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%files
%{_bindir}/*
%{_sbindir}/*
%{_libdir}/*
%{_libdir}/pkgconfig/juise.pc
%{_libdir}/lib*
%{_datarootdir}/juise/*
%{_datadir}/doc/juise/*
%{_libexecdir}/juise/*
%{_datadir}/juise/import/*
%{_mandir}/*/*
%docdir %{_mandir}/*/*

