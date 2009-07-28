#!/bin/sh
datadir=../data/ncml/types
outdir=ncml/types
for file in `ls $datadir/*.ncml`
do
    base=`basename $file`;
    outfile=$outdir/$base.ddx.bescmd;
    echo "Generating bescmd file for: $file in $outfile";
    sed s/%test_name%/$base/ < types.ddx.bescmd.in > $outfile;
done
