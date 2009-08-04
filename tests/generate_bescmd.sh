#!/bin/sh

# Usage: from the tests subdirectory:
#    ./generate_bescmd.sh filename.ncml responseType outputFilename
# 
# Generates an XML bes command file that can be loaded with -i 
# Useful for creating test files for debugging.
# 
# @author mjohnson <m.johnson@opendap.org>

# input to process to make $BESCMD_FILENAME
TEMPLATE_FILENAME="template.bescmd.in"

# The directory where the data lives, in terms of the bes.conf
DATADIR="/data/ncml" 

DATA_FILENAME=$1
RESPONSE=$2
OUTPUT_FILENAME=$3

echo "Generating $OUTPUT_FILENAME for $DATADIR/$DATA_FILENAME response $RESPONSE..."
sed -e "s:%ncml_filename%:$DATADIR/$DATA_FILENAME:" -e "s:%response_type%:$RESPONSE:" < $TEMPLATE_FILENAME > $OUTPUT_FILENAME;


