AUTOMAKE_OPTIONS = foreign

# Generate the m4 defines from top level configure.ac
# The `:;' works around a Bash 3.2 bug when the output is not writeable.
$(srcdir)/package.m4: $(top_srcdir)/configure.ac
	:;{ \
	echo '# Signature of the current package.' && \
	echo 'm4_define([AT_PACKAGE_NAME],' && \
	echo '  [@PACKAGE_NAME@])' && \
	echo 'm4_define([AT_PACKAGE_TARNAME],' && \
	echo '  [@PACKAGE_TARNAME@])' && \
	echo 'm4_define([AT_PACKAGE_VERSION],' && \
	echo '  [@PACKAGE_VERSION@])' && \
	echo 'm4_define([AT_PACKAGE_STRING],' && \
	echo '  [@PACKAGE_STRING@])' && \
	echo 'm4_define([AT_PACKAGE_BUGREPORT],' && \
	echo '  [@PACKAGE_BUGREPORT@])'; \
	} >'$(srcdir)/package.m4'

TESTSUITE = $(srcdir)/testsuite
# Run the tests in parallel, which speeds things up considerably.
TESTSUITEFLAGS = -j9

TEST_FILES = aggregations.at \
		attribute_tests.at \
		parse_error_misc.at \
		variable_misc.at \
		variable_new_arrays.at \
		variable_new_multi_arrays.at \
		variable_new_scalars.at \
		variable_new_structures.at \
		variable_remove.at \
		variable_rename.at 

EXTRA_DIST = testsuite.at $(TEST_FILES) $(srcdir)/package.m4 \
$(TESTSUITE) atlocal.in template.bescmd.in bes.conf.in \
bes_no_nc_global.conf.in bes.conf.modules.in \
bes_no_nc_global.conf.modules.in baselines

noinst_DATA = bes.conf bes_no_nc_global.conf
DISTCLEANFILES = bes.conf bes_no_nc_global.conf

check-local: atconfig atlocal $(TESTSUITE)
	$(SHELL) '$(TESTSUITE)' $(TESTSUITEFLAGS)

# Technically, we don't need this target now, but it might be useful
# to add it later to be complete.  It would only need to run
# besstandalone on the INSTALLED bes.conf file after it was processed
# with 'make bes-conf' to add the NCML module.  We'd have to figure
# out a way to let the testsuite script know whether to use the local
# bes.conf or the installed one.  
installcheck-local: atconfig atlocal
# $(TESTSUITE) $(SHELL) '$(TESTSUITE)' AUTOTEST_PATH='$(bindir)' \
# $(TESTSUITEFLAGS)

clean-local:
	test ! -f '$(TESTSUITE)' || \
	$(SHELL) '$(TESTSUITE)' --clean

distclean-local:
	-rm atconfig
	-rm bes.conf


# Use the missing script for better error checking on autom4te run.
AUTOM4TE = $(SHELL) $(top_srcdir)/conf/missing --run autom4te
#AUTOM4TE = autom4te
AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): $(srcdir)/testsuite.at $(srcdir)/package.m4 bes.conf $(TEST_FILES)
	$(AUTOTEST) -I '$(srcdir)' -o ${srcdir}/$@.tmp $@.at
	mv ${srcdir}/$@.tmp ${srcdir}/$@

# Resolve the ../ in the path so the BES doesn't complain about them
# canonical_abs_top_srcdir =  `${READLINK_F} ${abs_top_srcdir}`
