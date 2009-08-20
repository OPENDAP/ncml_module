
AUTOMAKE_OPTIONS = foreign dejagnu

DEJATOOL=besstandalone

AM_CPPFLAGS = -I$(top_srcdir)

# AM_CPPFLAGS =
# AM_CXXFLAGS =

# These are not used by automake but are often useful for certain types of
# debugging. The best way to use these is to run configure as:
#     export CXXFLAGS='...'; ./configure --disable-shared
# the --disable-shared is not required, but it seems to help with debuggers.
CXXFLAGS_DEBUG = -g3 -O0 -fno-defer-pop -Wall -W -Wcast-align -Werror
TEST_COV_FLAGS = -ftest-coverage -fprofile-arcs

noinst_DATA = bes.conf

CLEANFILES = *.log *.sum site.exp

EXTRA_DIST = ncml besstandalone.ncml common_tests.exp bes.conf.in

DISTCLEANFILES = bes.conf

# We need to find out where the preinstalled modules are
BES_MODULES_DIR = `bes-config --modulesdir`

# Resolve the ../ in the path so the BES doesn't complain about them
canonical_abs_top_srcdir = `${READLINK_F} ${abs_top_srcdir}`

# this fails on distcheck...
#		-e "s%[@]libdir[@]%${libdir}%" 
bes.conf: bes.conf.in ../config.status
	sed -e "s%[@]pkgdatadir[@]%${pkgdatadir}%" \
		-e "s%[@]sysconfdir[@]%${sysconfdir}%" \
		-e "s%[@]beslibdir[@]%${BES_MODULES_DIR}%" \
		-e "s%[@]datadir[@]%${datadir}%" \
		-e "s%[@]abs_top_srcdir[@]%${canonical_abs_top_srcdir}%" \
		-e "s%[@]abs_top_builddir[@]%${abs_top_builddir}%" \
		-e "s%[@]bindir[@]%${bindir}%" $< > bes.conf


