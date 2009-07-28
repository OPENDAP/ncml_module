#!/bin/sh
datadir=../data/ncml/types
outdir=besstandalone.ncml/types
for file in `ls $datadir/*.ncml`
do
    base=`basename $file`;
    outfile=$outdir/$base.ddx.exp;
    echo "Generating .exp file for: $file in $outfile";
    sed s/%test_name%/$base/ < types.exp.in > $outfile;
done
