EXTRA_DIST = README.devel

bin_PROGRAMS = memtool

memtool_SOURCES = memtool.c

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
