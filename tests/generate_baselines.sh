#!/bin/sh

# Usage: from the tests subdirectory:
#    ./generate_baselines.sh filename.ncml constraint_expr [output_prefix]
# 
# This script is used to run the besstandalone on a particular
# NcML file and save off the DAP responses { das|dds|dods|ddx} 
# into the baselines directory.  It is assumed the caller
# will check these for accuracy before including them in the testsuite.at
# 
# The optional second argument is the prefix for the output files,
# which are named as $prefix.$response.  If not specified, 
# the input filename is used.
# 
# NOTE: It assumes the existence of ../bes-testsuite/bes.conf for running 
# besstandalone properly!  [TODO perhaps relax this to generate bes.conf within here
# as well as we move away from bes-testsuite]
#
# @author mjohnson <m.johnson@opendap.org>

# output filename for the command
BESCMD_FILENAME="test.bescmd"

# input to process to make $BESCMD_FILENAME
TEMPLATE_FILENAME="template.bescmd.in"

# The directory where the data lives, in terms of the bes.conf
DATADIR="/data/ncml" 

# The directory the baselines live in....
BASELINE_DIR="baselines"

# The autogenerated bes.conf which needs to exist! (run make check)
BES_CONF="./bes.conf"

# Command to run standalone with the autogenerated bes.conf for the 
# dejagnu testsuite on the autogenerated bescmd file.
RUN_BES="besstandalone -c $BES_CONF -i $BESCMD_FILENAME"

# Use the input file as the output prefix if not specified.
OUTPUT_PREFIX=$1;
if test $3 != ""
then OUTPUT_PREFIX=$3
fi
echo "Using output prefix of: $OUTPUT_PREFIX"

# The input
DATA_FILE=$DATADIR/$1

if ! test -e $TEMPLATE_FILENAME
then echo "Fatal error: bescmd template file \"$TEMPLATE_FILENAME\" doesn't exist!"
exit -1
fi

if ! test -e $BES_CONF
then echo "Fatal error: bes conf file \"$BES_CONF\" doesn't exist!"
exit -1
fi

if ! test -d $BASELINE_DIR
then echo "Fatal error: baselines directory \"$BASELINE_DIR\" doesn't exist!"
exit -1;
fi

# Set the constraint to use, can be empty
CONSTRAINT_EXPR=$2;
echo "Using constraint=\"$CONSTRAINT_EXPR\""

# Create baselines for all responses....
for response in "das" "dds" "ddx" "dods"
do
    echo "Generating $BESCMD_FILENAME for $DATA_FILE response $response...";
    # use the helper script
    ./generate_bescmd.sh $1 $response $BESCMD_FILENAME $CONSTRAINT_EXPR

    BASELINE_OUT="$BASELINE_DIR/$OUTPUT_PREFIX.$response";
    echo "Running besstandalone to generate $BASELINE_OUT...";
    echo "$RUN_BES -f $BASELINE_OUT";
    $RUN_BES -f $BASELINE_OUT;
    echo "";
done

# clean up the temp file
echo "Cleaning up temp file $BESCMD_FILENAME..."
#rm -f $BESCMD_FILENAME

