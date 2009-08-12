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

EXTRA_DIST = testsuite.at $(srcdir)/package.m4 $(TESTSUITE) atlocal.in \
	template.bescmd.in baselines
TESTSUITE = $(srcdir)/testsuite

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

# Not sure if we need this yet...
#distclean-local:
#	-rm atconfig

TEST_FILES = 	variable_rename.at \
		variable_new_scalars.at \
		variable_new_structures.at \
		variable_remove.at \
		variable_new_arrays.at \
		variable_new_multi_arrays.at

# Use the missing script for better error checking on autom4te run.
AUTOM4TE = $(SHELL) $(top_srcdir)/conf/missing --run autom4te
#AUTOM4TE = autom4te
AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): $(srcdir)/testsuite.at $(srcdir)/package.m4 $(TEST_FILES)
	$(AUTOTEST) -I '$(srcdir)' -o $@.tmp $@.at
	mv $@.tmp $@

