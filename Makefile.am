
# Automake file for ncml-handler
#
# 5/17/2005 jhrg
#
# $Id: Makefile.am 12972 2006-01-05 15:41:07Z pwest $

AUTOMAKE_OPTIONS = foreign check-news 

ACLOCAL_AMFLAGS = -I conf

lib_besdir=$(libdir)/bes
lib_bes_LTLIBRARIES = libncml_module.la

SUBDIRS = . tests

BES_SRCS:=
BES_HDRS:=

NCML_SRCS = \
		AggMemberDataset.cc \
		AggregationElement.cc \
		AggregationUtil.cc \
		AttributeElement.cc \
		DDSLoader.cc \
		Dimension.cc \
		DimensionElement.cc \
		DirectoryUtil.cc \
		ExplicitElement.cc \
		GridAggregateOnOuterDimension.cc \
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
		ScanElement.cc \
		ScopeStack.cc \
		Shape.cc \
		SimpleLocationParser.cc \
		SimpleTimeParser.cc \
		ValuesElement.cc \
		VariableAggElement.cc \
		VariableElement.cc \
		XMLHelpers.cc 

NCML_HDRS = \
		AggMemberDataset.h \
		AggregationElement.h \
		AggregationUtil.h \
		AttributeElement.h \
		DDSLoader.h \
		Dimension.h \
		DimensionElement.h \
		DirectoryUtil.h \
		ExplicitElement.h \
		GridAggregateOnOuterDimension.h \
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
		ScanElement.h \
		Shape.h \
		ScopeStack.h \
		SimpleLocationParser.h \
		SimpleTimeParser.h \
		ValuesElement.h \
		VariableAggElement.h \
		VariableElement.h \
		XMLHelpers.h

# ICU_CPPFLAGS and ICU_LIBS set from configure.ac check for ICU
libncml_module_la_SOURCES = $(NCML_SRCS) $(BES_SRCS) $(NCML_HDRS) $(BES_HDRS)
libncml_module_la_CPPFLAGS = $(BES_CPPFLAGS)  $(DAP_CFLAGS) $(ICU_CPPFLAGS)
libncml_module_la_LDFLAGS = -avoid-version -module 
libncml_module_la_LIBADD =  $(BES_DAP_LIBS)  $(DAP_LIBS) $(ICU_LIBS) 

EXTRA_PROGRAMS = 

EXTRA_DIST = COPYRIGHT COPYING ncml.conf.in ncml_module.spec data OSX_Resources

CLEANFILES = *~ ncml.conf

# Sample data primaries for install
sample_datadir = $(datadir)/hyrax/data/ncml
sample_data_DATA = data/ncml/fnoc1_improved.ncml data/ncml/fnoc1.nc data/ncml/sample_virtual_dataset.ncml 

# Subdir of data for aggregation samples
sample_data_aggdir = $(datadir)/hyrax/data/ncml/agg
sample_data_agg_DATA = data/ncml/agg/joinNew_grid.ncml data/ncml/agg/joinNew_scan.ncml data/ncml/agg/joinNew_scan_dfm.ncml

# Two subdirs of agg for the actual aggregation members
sample_data_agg_gridsdir = $(datadir)/hyrax/data/ncml/agg/grids
sample_data_agg_grids_DATA = 	data/ncml/agg/grids/f97182070958.hdf \
				data/ncml/agg/grids/f97182183448.hdf \
				data/ncml/agg/grids/f97183065853.hdf \
				data/ncml/agg/grids/f97183182355.hdf 

sample_data_agg_dateddir = $(datadir)/hyrax/data/ncml/agg/dated
sample_data_agg_dated_DATA = 	data/ncml/agg/dated/CG2006158_120000h_usfc.nc \
				data/ncml/agg/dated/CG2006158_130000h_usfc.nc \
				data/ncml/agg/dated/CG2006158_140000h_usfc.nc \
				data/ncml/agg/dated/CG2006158_150000h_usfc.nc 

install-data-local: ncml.conf
	@currdate=`date +"%y%m%d%H%M"`; \
	test -d $(DESTDIR)$(sysconfdir)/bes || $(MKDIR_P) $(DESTDIR)$(sysconfdir)/bes; \
	test -d $(DESTDIR)$(sysconfdir)/bes/modules || $(MKDIR_P) $(DESTDIR)$(sysconfdir)/bes/modules; \
	test -f $(DESTDIR)$(sysconfdir)/bes/modules/ncml.conf && mv -f $(DESTDIR)$(sysconfdir)/bes/modules/ncml.conf $(DESTDIR)$(sysconfdir)/bes/modules/ncml.conf.$$currdate; \
	$(INSTALL_DATA) ncml.conf $(DESTDIR)$(sysconfdir)/bes/modules/ncml.conf

uninstall-local:
	rm -f $(DESTDIR)$(sysconfdir)/bes/modules/ncml.conf

ncml.conf: ncml.conf.in config.status
	sed -e "s%[@]bes_modules_dir[@]%${lib_besdir}%" $< > ncml.conf

C4_DIR=./cccc
.PHONY: cccc
cccc:	
	cccc --outdir=$(C4_DIR) $(libncml_module_la_SOURCES)

###########################################################################
#
# Build linux RPMs

srpm: dist
	rpmbuild -ts --clean @PACKAGE@-@PACKAGE_VERSION@.tar.gz

rpm: dist
	rpmbuild -tb --clean @PACKAGE@-@PACKAGE_VERSION@.tar.gz

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

