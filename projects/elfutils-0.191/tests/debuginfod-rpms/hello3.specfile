Summary: hello3 -- double hello, world rpm
Name: hello3
Version: 1.0
Release: 2
Group: Utilities
License: GPL
Distribution: RPM ^W Elfutils test suite.
Vendor: Red Hat Software
Packager: Red Hat Software <bugs@redhat.com>
URL: http://www.redhat.com
BuildRequires: gcc make
Source0: hello-1.0.tar.gz

%description
Simple rpm demonstration with an eye to consumption by debuginfod.

%package two
Summary: hello3two
License: GPL

%description two
Dittoish.

%prep
%setup -q -n hello-1.0

%build
mkdir foobar
gcc -g -O1 foobar///./../hello.c -o hello
gcc -g -O2 -D_FORTIFY_SOURCE=2 foobar///./../hello.c -o hello3

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
cp hello $RPM_BUILD_ROOT/usr/local/bin/
cp hello3 $RPM_BUILD_ROOT/usr/local/bin/

%clean
rm -rf $RPM_BUILD_ROOT

%files 
%defattr(-,root,root)
%attr(0751,root,root)   /usr/local/bin/hello

%files two
%defattr(-,root,root)
%attr(0751,root,root)   /usr/local/bin/hello3

%changelog
* Tue Mar 24 2020 Frank Ch. Eigler <fche@redhat.com>
- New variant of hello2, with crazy source file paths

* Thu Nov 14 2019 Frank Ch. Eigler <fche@redhat.com>
- Dropped misc files not relevant to debuginfod testing.

* Wed May 18 2016 Mark Wielaard <mjw@redhat.com>
- Add hello2 for dwz testing support.

* Tue Oct 20 1998 Jeff Johnson <jbj@redhat.com>
- create.
