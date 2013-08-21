Name:        juise
Version:     0.5.8
Release:     1%{?dist}
Summary:     JUNOS User Interface Scripting Environment

Prefix:      /usr

Vender:      Juniper Networks, Inc.
Packager:    Phil Shafer <phil@juniper.net>
License:     BSD

Group:       Development/Libraries
License:     MIT
URL:         https://github.com/Juniper/libslax
Source0:     https://github.com/Juniper/juise/releases/0.5.8/juise-0.5.8.tar.gz

BuildRequires:  libxml2-devel
BuildRequires:  libxslt-devel
BuildRequires:  curl-devel
BuildRequires:  libedit-devel
BuildRequires:  libslax
BuildRequires:  libssh2
BuildRequires:  bison-devel
BuildRequires:  bison

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
%{_libdir}/*
%{_libdir}/pkgconfig/juise.pc
%{_libdir}/lib*
%{_datadir}/doc/juise/*
%{_libexecdir}/juise/*
%{_datadir}/juise/import/*
%{_mandir}/*/*
%docdir %{_mandir}/*/*
