#!/bin/sh
datadir=../data/ncml/type_validation_tests
outdir=ncml/type_validation_tests
for file in `ls $datadir/*.ncml`
do
    base=`basename $file`;
    outfile=$outdir/$base.ddx.bescmd;
    echo "Generating bescmd file for: $file in $outfile";
    sed s/%test_name%/$base/ < type_validation_test.ddx.bescmd.in > $outfile;
done
