# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# Configure script for OPeNDAP NCML data handler

AC_PREREQ(2.61)

AC_INIT([ncml_moddule], [1.4.4], [opendap-tech@opendap.org])
AC_CONFIG_AUX_DIR([conf])
AC_CONFIG_MACRO_DIR([conf])

AM_INIT_AUTOMAKE

AM_CONDITIONAL([DAP_MODULES], [false])

# The handler is a shared object library, so we don't normally need
# the static lib. Override this default by specifying `--enable-static',
# which can make running unit tests with valgrind far more pleasant. 
AM_DISABLE_STATIC

PACKAGE_MAJOR_VERSION=`echo $PACKAGE_VERSION | sed 's@^\([[0-9]]\)*\.\([[0-9]]*\)\.\([[0-9]]*\)$@\1@'`
PACKAGE_MINOR_VERSION=`echo $PACKAGE_VERSION | sed 's@^\([[0-9]]\)*\.\([[0-9]]*\)\.\([[0-9]]*\)$@\2@'`
PACKAGE_SUBMINOR_VERSION=`echo $PACKAGE_VERSION | sed 's@^\([[0-9]]\)*\.\([[0-9]]*\)\.\([[0-9]]*\)$@\3@'`
AC_MSG_NOTICE(Package Major Version:     $PACKAGE_MAJOR_VERSION)
AC_MSG_NOTICE(Package Minor Version:     $PACKAGE_MINOR_VERSION)
AC_MSG_NOTICE(Package SubMinor Version:  $PACKAGE_SUBMINOR_VERSION)
AC_SUBST(PACKAGE_MAJOR_VERSION)
AC_SUBST(PACKAGE_MINOR_VERSION)
AC_SUBST(PACKAGE_SUBMINOR_VERSION)

AC_CONFIG_SRCDIR([NCMLModule.h])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_TESTDIR(tests, [.])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_SIZE_T
AC_CHECK_TYPES([ptrdiff_t])

# Make sure we have the cctype library
AC_SEARCH_LIBS([isdigit], [cctype])

# Checks for library functions.
AC_CHECK_FUNCS([strchr isdigit])

dnl Checks for specific libraries
AC_CHECK_LIBDAP([3.13.0],
 [
  LIBS="$LIBS $DAP_SERVER_LIBS"
  CPPFLAGS="$CPPFLAGS $DAP_CFLAGS"
 ],
dnl [
dnl  LIBS="$LIBS $DAP_LIBS"
dnl  CPPFLAGS="$CPPFLAGS $DAP_CFLAGS"
dnl ],
 [ AC_MSG_ERROR([Cannot find expected version of libdap])
])

dnl Look for the minimum version of the icu libs and headers
dnl This sets: ICU_CPPFLAGS with -Iicu-include-path and ICU_LIBS with 
dnl -Licu-lib-path -licu-lib-1 ... 
dnl Note: it also handles --with-icu-prefix=path_to_icu_install
dnl in case there are multiple icu development environments on the machine
dnl NOTE: LD_LIBRARY_PATH or DYLD_LIBRARY_PATH may need to be set for this
dnl to work, however.
AC_CHECK_ICU([3.6],
[
AC_MSG_NOTICE([ICU_CPPFLAGS=$ICU_CPPFLAGS])
AC_MSG_NOTICE([ICU_LIBS=$ICU_LIBS])
],
[ AC_MSG_ERROR([Could not find compatible icu (International Components for Unicode) library and headers!])
])

AC_CHECK_BES([3.13.0],
[
],
[ AC_MSG_ERROR([Could not find expected version of bes library and headers])
])

# Test for a readlink bug, see
# <http://lists.gnu.org/archive/html/bug-coreutils/2008-02/msg00126.html>.
# We use perl to replace 'readlink -f' if it doesn't exist...
AC_CHECK_PROG(PERL, perl, perl)
AC_MSG_CHECKING([for a suitable ``readlink -f'' program])
[mkdir readlink-test &&
mkdir readlink-test/a-long &&
: > readlink-test/a-long/f &&
ln -s a-long readlink-test/a &&
for READLINK_F in \
  'readlink -f' \
  realpath \
  false
do
  if
    $READLINK_F > /dev/null 2>&1 \
      readlink-test/a/../a/f 
  then
    break
  fi
done &&
rm -rf readlink-test &&
if [ "$READLINK_F" = false ]
then]
  AC_MSG_RESULT([none found, using own replacement])
  [READLINK_F='perl -e '\''use Cwd qw(abs_path); print abs_path $$ARGV[0];'\'
else]
  AC_MSG_RESULT([$READLINK_F])
[fi]
AC_SUBST([READLINK_F])

## This sets up the special symbols used to build the bes.conf file for
## the tests. Previously, this was done in the Makefile.am down in the
## test directory, but building the bes.conf here works better with
## autoconf overall and esp. with the 'modules' build. jhrg 11/19/14
modulesdir=$BES_MODULE_DIR
AC_SUBST(modulesdir)

dnl The ncml_module files...
AC_CONFIG_FILES([Makefile tests/Makefile tests/atlocal tests/bes.conf tests/bes_no_nc_global.conf])
AC_OUTPUT

