
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
		AggMemberDatasetDDSWrapper.cc \
		AggMemberDatasetUsingLocationRef.cc \
		AggMemberDatasetWithDimensionCacheBase.cc \
		AggregationElement.cc \
		AggregationException.cc \
		AggregationUtil.cc \
		ArrayAggregateOnOuterDimension.cc \
		ArrayAggregationBase.cc \
		ArrayJoinExistingAggregation.cc \
		AttributeElement.cc \
		DDSAccessInterface.cc \
		DDSLoader.cc \
		Dimension.cc \
		DimensionElement.cc \
		DirectoryUtil.cc \
		ExplicitElement.cc \
		GridAggregationBase.cc \
		GridAggregateOnOuterDimension.cc \
		GridJoinExistingAggregation.cc \
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
		RCObjectInterface.cc \
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
		AggMemberDatasetDDSWrapper.h \
		AggMemberDatasetUsingLocationRef.h \
		AggMemberDatasetWithDimensionCacheBase.h \
		AggregationElement.h \
		AggregationException.h \
		AggregationUtil.h \
		ArrayAggregateOnOuterDimension.h \
		ArrayAggregationBase.h \
		ArrayJoinExistingAggregation.h \
		AttributeElement.h \
		DDSAccessInterface.h \
		DDSLoader.h \
		Dimension.h \
		DimensionElement.h \
		DirectoryUtil.h \
		ExplicitElement.h \
		GridAggregationBase.h \
		GridAggregateOnOuterDimension.h \
		GridJoinExistingAggregation.h \
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
		RCObjectInterface.h \
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
libncml_module_la_CPPFLAGS = $(BES_CPPFLAGS)  $(DAP_CFLAGS) $(ICU_CPPFLAGS) -Werror
libncml_module_la_LDFLAGS = -avoid-version -module 
libncml_module_la_LIBADD =  $(BES_DAP_LIBS)  $(DAP_LIBS) $(ICU_LIBS) 

EXTRA_PROGRAMS = 

EXTRA_DIST = COPYRIGHT COPYING ncml.conf.in ncml_module.spec data OSX_Resources

CLEANFILES = *~ ncml.conf

# Sample data primaries for install
sample_datadir = 		$(datadir)/hyrax/data/ncml
sample_data_DATA = 		data/ncml/fnoc1_improved.ncml \
				data/ncml/fnoc1.nc \
				data/ncml/sample_virtual_dataset.ncml 

# Subdir of data for aggregation samples
sample_data_aggdir = 		$(datadir)/hyrax/data/ncml/agg
sample_data_agg_DATA = 		data/ncml/agg/joinNew_grid.ncml \
				data/ncml/agg/joinNew_scan.ncml \
				data/ncml/agg/joinNew_scan_dfm.ncml \
				data/ncml/agg/aggExisting.ncml

# Two subdirs of agg for the actual aggregation members
sample_data_agg_gridsdir = 	$(datadir)/hyrax/data/ncml/agg/grids
sample_data_agg_grids_DATA = 	data/ncml/agg/grids/f97182070958.hdf \
				data/ncml/agg/grids/f97182183448.hdf \
				data/ncml/agg/grids/f97183065853.hdf \
				data/ncml/agg/grids/f97183182355.hdf 

sample_data_agg_dateddir = 	$(datadir)/hyrax/data/ncml/agg/dated
sample_data_agg_dated_DATA = 	data/ncml/agg/dated/CG2006158_120000h_usfc.nc \
				data/ncml/agg/dated/CG2006158_130000h_usfc.nc \
				data/ncml/agg/dated/CG2006158_140000h_usfc.nc \
				data/ncml/agg/dated/CG2006158_150000h_usfc.nc 

sample_data_ncdir = 		$(datadir)/hyrax/data/nc
sample_data_nc_DATA =		data/nc/jan.nc data/nc/feb.nc


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

# Not nearly as clean as it could be, but this removes .svn directories
# in subdirs.
dist-hook:
	rm -rf `find $(distdir) -name .svn`

C4_DIR=./cccc
.PHONY: cccc
cccc:	
	cccc --outdir=$(C4_DIR) $(libncml_module_la_SOURCES)

###########################################################################
#
# Build linux RPMs

srpm: dist
	rpmbuild -ts --clean $(RPM_OPTIONS) @PACKAGE@-@PACKAGE_VERSION@.tar.gz

rpm: dist
	rpmbuild -tb --clean $(RPM_OPTIONS) @PACKAGE@-@PACKAGE_VERSION@.tar.gz

###########################################################################

# Build OS/X Packages. The strange operations with configure and make
# are there so that the values built into dap-config will match the mac
# osx install dirs and not the temp directory used to build the packages

# PACKAGEMAKER=/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker*

PACKAGEMAKER=/Developer/usr/bin/packagemaker
PKG_CONF_FLAGS=

clean-pkg:
	-rm -rf mac_osx @PACKAGE@-@PACKAGE_VERSION@.pkg

pkg-build:
	./configure --prefix=/usr --disable-dependency-tracking $(PKG_CONF_FLAGS)
	make clean all
	DESTDIR=`pwd`/mac_osx make install

pkg-main:
	./OSX_Resources/update_mac_package_contents.pl README
	${PACKAGEMAKER} --root mac_osx/usr --id org.opendap.libdap \
		--title "@PACKAGE@ @PACKAGE_VERSION@" --version @PACKAGE_VERSION@ \
		--out @PACKAGE@-@PACKAGE_VERSION@.pkg --resources OSX_Resources

pkg-dmg:
	-rm -rf @PACKAGE@-@PACKAGE_VERSION@
	-rm -rf @PACKAGE@-@PACKAGE_VERSION@.dmg
	mkdir @PACKAGE@-@PACKAGE_VERSION@
	mv @PACKAGE@-@PACKAGE_VERSION@.pkg @PACKAGE@-@PACKAGE_VERSION@
	cp README NEWS @PACKAGE@-@PACKAGE_VERSION@
	dropdmg -i --sanitize-for-servers --format zlib @PACKAGE@-@PACKAGE_VERSION@
	-rm -rf @PACKAGE@-@PACKAGE_VERSION@

pkg: clean-pkg pkg-build pkg-main pkg-dmg

