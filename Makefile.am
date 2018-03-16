EXTRA_DIST = README.devel

bin_PROGRAMS = memtool

noinst_HEADERS = fileaccess.h fileaccpriv.h
memtool_SOURCES = memtool.c fileaccess.c acc_mmap.c
if MDIO
memtool_SOURCES += acc_mdio.c
endif

dist_man_MANS = memtool.1

# clean files "make" has built
CLEANFILES = \
	$(DIST_ARCHIVES)

# clean files "configure" has built
DISTCLEAN = \
	config.log \
	config.status \
	Makefile

# clean all files the maintainer of the package has created
MAINTAINERCLEANFILES = \
	compile \
	Makefile.in \
	configure \
	depcomp \
	install-sh \
	missing \
	aclocal.m4
