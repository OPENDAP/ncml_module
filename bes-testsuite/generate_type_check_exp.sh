#!/bin/sh
datadir=../data/ncml/type_validation_tests
outdir=besstandalone.ncml/type_validation_tests
for file in `ls $datadir/*.ncml`
do
    base=`basename $file`;
    outfile=$outdir/$base.ddx.exp;
    echo "Generating .exp file for: $file in $outfile";
    sed s/%test_name%/$base/ < type_validation_test.exp.in > $outfile;
done
