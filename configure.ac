# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# Configure script for OPeNDAP NCML data handler

AC_PREREQ(2.62)
AC_INIT(ncml_module, 1.0.0, support@opendap.org)
AC_CONFIG_AUX_DIR([conf])
AC_CONFIG_MACRO_DIR([conf])

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

dnl Checks for specific libraries
AC_CHECK_LIBDAP([3.10.0],
 [
  LIBS="$LIBS $DAP_LIBS"
  CPPFLAGS="$CPPFLAGS $DAP_CFLAGS"
 ],
 [ AC_MSG_ERROR([Cannot find libdap])
])
  
AC_CHECK_BES([3.8.0],
[
],
[ AC_MSG_ERROR([Could not find bes library and headers])
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

OPENDAP_DEBUG_OPTION

dnl The ncml_module files...
AC_CONFIG_FILES([Makefile tests/Makefile tests/atlocal])
AC_OUTPUT

