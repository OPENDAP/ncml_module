# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# Configure script for OPeNDAP NCML data handler

AC_PREREQ(2.61)
AC_INIT(ncml_module, 0.9.1, opendap-tech@opendap.org)
AC_CONFIG_AUX_DIR([conf])

AM_INIT_AUTOMAKE

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

# We use perl to replace 'readlink -f' if it doesn't exist...
AC_CHECK_PROG(PERL, perl, perl)

dnl Checks for specific libraries
AC_CHECK_LIBDAP([3.9.2],
 [
  LIBS="$LIBS $DAP_LIBS"
  CPPFLAGS="$CPPFLAGS $DAP_CFLAGS"
 ],
 [ AC_MSG_ERROR([Cannot find libdap])
])
  
AC_CHECK_BES([3.7.2],
[
],
[ AC_MSG_ERROR([Could not find bes library and headers])
])

# Test for a readlink bug, see
# <http://lists.gnu.org/archive/html/bug-coreutils/2008-02/msg00126.html>.
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

OPENDAP_DEBUG_OPTION

AC_CONFIG_FILES([Makefile bes-testsuite/Makefile tests/Makefile tests/atlocal])

AC_OUTPUT
