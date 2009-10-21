
# Automake file for ncml-handler
#
# 5/17/2005 jhrg
#
# $Id: Makefile.am 12972 2006-01-05 15:41:07Z pwest $

AUTOMAKE_OPTIONS = foreign check-news 

ACLOCAL_AMFLAGS = -I conf

lib_besdir=$(libdir)/bes
lib_bes_LTLIBRARIES = libncml_module.la
dist_bin_SCRIPTS = bes-ncml-data.sh

SUBDIRS = tests
#TESTS = tests/testsuite

BES_SRCS:=

BES_HDRS:=

NCML_SRCS = \
		AggregationElement.cc \
		AggregationUtil.cc \
		AttributeElement.cc \
		DDSLoader.cc \
		Dimension.cc \
		DimensionElement.cc \
		ExplicitElement.cc \
		MyBaseTypeFactory.cc \
		NCMLBaseArray.cc \
		NCMLCommonTypes.cc \
		NCMLElement.cc \
		NCMLModule.cc \
		NCMLParser.cc \
		NCMLRequestHandler.cc \
		NCMLUtil.cc \
		NetcdfElement.cc \
		OtherXMLParser.cc \
		RCObject.cc \
		ReadMetadataElement.cc \
		RemoveElement.cc \
		RenamedArrayWrapper.cc \
		SaxParserWrapper.cc \
		SaxParser.cc \
		ScopeStack.cc \
		Shape.cc \
		SimpleLocationParser.cc \
		ValuesElement.cc \
		VariableAggElement.cc \
		VariableElement.cc \
		XMLHelpers.cc 

NCML_HDRS = \
		AggregationElement.h \
		AggregationUtil.h \
		AttributeElement.h \
		DDSLoader.h \
		Dimension.h \
		DimensionElement.h \
		ExplicitElement.h \
		MyBaseTypeFactory.h \
		NCMLArray.h \
		NCMLBaseArray.h \
		NCMLCommonTypes.h \
		NCMLDebug.h \
		NCMLElement.h \
		NCMLModule.h \
		NCMLParser.h \
		NCMLRequestHandler.h \
		NCMLResponseNames.h \
		NCMLUtil.h \
		NetcdfElement.h \
		OtherXMLParser.h \
		RCObject.h \
		ReadMetadataElement.h \
		RemoveElement.h \
		RenamedArrayWrapper.h \
		SaxParserWrapper.h \
		SaxParser.h \
		Shape.h \
		ScopeStack.h \
		SimpleLocationParser.h \
		ValuesElement.h \
		VariableAggElement.h \
		VariableElement.h \
		XMLHelpers.h

libncml_module_la_SOURCES = $(NCML_SRCS) $(BES_SRCS) $(NCML_HDRS) $(BES_HDRS)
libncml_module_la_CPPFLAGS = $(BES_CPPFLAGS)
libncml_module_la_LDFLAGS = -avoid-version -module 
libncml_module_la_LIBADD = $(BES_DAP_LIBS)

EXTRA_PROGRAMS = 

EXTRA_DIST = COPYRIGHT COPYING bes-ncml-data.sh.in ncml_module.spec data OSX_Resources

CLEANFILES = *~ bes-ncml-data.sh

sample_datadir = $(datadir)/hyrax/data/ncml
sample_data_DATA = data/ncml/fnoc1_improved.ncml data/ncml/fnoc1.nc data/ncml/sample_virtual_dataset.ncml

do_subst = sed -e 's,[@]sysconfdir[@],$(sysconfdir),g' \
		   	   -e 's,[@]libdir[@],$(libdir),g' \
		   	   -e 's,[@]datadir[@],$(datadir),g'

bes-ncml-data.sh: bes-ncml-data.sh.in config.status
	$(do_subst) < $(srcdir)/bes-ncml-data.sh.in > bes-ncml-data.sh
	chmod +x bes-ncml-data.sh

# This makes sure that the handler modifies the bes which configure found
# even when distcheck is run.
bes-conf: bes-ncml-data.sh
	(bes_prefix=`bes-config --prefix` ; \
	if test "$$bes_prefix" = "/usr" ; \
	 then \
	    ./bes-ncml-data.sh /etc/bes/bes.conf /usr/lib/bes ;\
	else \
	    ./bes-ncml-data.sh $$bes_prefix/etc/bes/bes.conf $$bes_prefix/lib/bes ; \
	fi)

###########################################################################
#
# Build linux RPMs

srpm: dist
	rpmbuild -ts ncml_module-@PACKAGE_VERSION@.tar.gz

rpm: dist
	rpmbuild -tb ncml_module-@PACKAGE_VERSION@.tar.gz

###########################################################################

# Build OS/X Packages.

PACKAGEMAKER=/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker*

UNIV_ARCH=-arch ppc -arch i386
MAC_SYSROOT=Developer/SDKs/MacOSX10.4u.sdk

PKG_CONF_FLAGS=

clean-pkg:
	-rm -rf mac_osx @PACKAGE@-@PACKAGE_VERSION@.pkg @PACKAGE@-@PACKAGE_VERSION@

pkg-build:
	./configure --prefix=/usr --disable-dependency-tracking $(PKG_CONF_FLAGS)
	make clean all
	DESTDIR=${PWD}/mac_osx make install

pkg-main:
	./OSX_Resources/update_mac_package_contents.pl README "configure.ac" "mac_osx/usr"
	${PACKAGEMAKER} -build -p @PACKAGE@-@PACKAGE_VERSION@.pkg \
		-f mac_osx/usr \
		-ds -r OSX_Resources \
		-i OSX_Resources/Info.plist \
		-d OSX_Resources/Description.plist
	-rm -rf mac_osx

pkg-dmg:
	-rm -rf @PACKAGE@-@PACKAGE_VERSION@
	-rm -rf @PACKAGE@-@PACKAGE_VERSION@.dmg
	mkdir @PACKAGE@-@PACKAGE_VERSION@
	mv @PACKAGE@-@PACKAGE_VERSION@.pkg @PACKAGE@-@PACKAGE_VERSION@
	cp README NEWS @PACKAGE@-@PACKAGE_VERSION@
	dropdmg -i --sanitize-for-servers --format zlib @PACKAGE@-@PACKAGE_VERSION@
	-rm -rf @PACKAGE@-@PACKAGE_VERSION@

pkg: clean-pkg pkg-build pkg-main pkg-dmg

