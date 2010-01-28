# We use this script as a wrapper for the the recursive configure call
# in order to modify the prefix and add special configure options
# for the ICU build.  
# AC_CONFIG_SUBDIRS will call configure.gnu rather than configure in
# a recursive configure if configure.gnu exists.
# <m.johnson@opendap.org> 28 Jan 2010

echo "**********************************************************************"
echo "configure.gnu found in autoconf recursion, wrapping ./configure...."

ICU_HYRAX_CONFIG_ARGS="--prefix=${prefix}/hyrax_icu --enable-static --disable-shared --disable-extras --disable-icuio --disable-layout --disable-samples --with-data-packaging=library --with-library-suffix=_hyrax"
echo "Running ./configure ${ICU_HYRAX_CONFIG_ARGS}..."
echo "**********************************************************************"
./configure ${ICU_HYRAX_CONFIG_ARGS}

#TODO See if we can pass down all of the other configure options passed in, but not prefix?  



