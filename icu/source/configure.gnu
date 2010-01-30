# We use this script as a wrapper for the the recursive configure call
# in order specify special configure options
# for the ICU build and to override certain ones.
# AC_CONFIG_SUBDIRS will call configure.gnu rather than configure in
# a recursive configure if configure.gnu exists.
# <m.johnson@opendap.org> 28 Jan 2010

# We need to process each argument and: 
# 1) if it is one we will
# override, we want to ignore it.  (like whether to build static or
# shared libraries) 
# 2) If it's unknown arg starting with "--", we pass it through as is.  
# 3) If it doesn't start with "--" we don't pass it through (to avoid a
# problem with 'CXXFLAGS=-g3 -O0' being passed as an arg and causing
# ./configure to crash.  
# This script invocation sets the vars so configure sees them.

echo "***********************************************************************"
echo "* configure.gnu found in autoconf recursion for icu..."
echo "* wrapping ./configure to modify arguments... "

# what to append to the normal icu lib names to make sure we don't collide with standard.
HYRAX_ICU_SUFFIX="_hyrax"

# Start of the arg list...  Modified as we parse the args to this.
HYRAX_ICU_CONFIG_ARGS="--enable-static --disable-shared --disable-extras --disable-icuio --disable-layout --disable-samples --with-data-packaging=library --with-library-suffix=${HYRAX_ICU_SUFFIX}"

while [ $# -ne 0 ]
do
    #echo "Processing arg = $1"
    arg=`echo "$1" | cut -d'=' -f1`
    val=`echo "$1" | cut -s -d'=' -f2`
    #echo "Cut arg = $arg"
    #echo "Cut val = $val"
    case "$arg" in 
        --enable-static) shift ;; # ignore
        --disable-static) shift ;; # ignore 
	--enable-shared) shift ;; # ignore
	--disable-shared) shift ;; # ignore
        --) shift ; break ;; # end case
	*) MATCHES=`expr "$1" : ^--`
	    # i.e. starts with '--'
	    if [ ${MATCHES} == 2 ] 
	    then
		# Pass through as is
		echo "Passing through arg $1"
		HYRAX_ICU_CONFIG_ARGS="${HYRAX_ICU_CONFIG_ARGS} $1" 
	    else 
		echo "Not passing through the arg $1"
	    fi
	    #echo "Passing argument through..."
	    # echo "Args are now: ${HYRAX_ICU_CONFIG_ARGS}"
	    shift ;;
    esac
done

echo "Running:"
echo " ./configure ${HYRAX_ICU_CONFIG_ARGS}"
echo "**********************************************************************"
./configure ${HYRAX_ICU_CONFIG_ARGS}

#echo "./configure $@"
#./configure "$@"

