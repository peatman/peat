#!/bin/sh

this_script_dir=$(dirname $(readlink -f ${BASH_SOURCE[0]:-$0}))

cwd=`pwd`
directory=$1
template="txt-$2*"
cd $directory
for a in `ls $template` ; do
  if [ -e $a ]; then
    txtfile=$a
    outfile=${a#txt-}
    binfile="bin-$outfile"
    cp $txtfile $outfile
    $this_script_dir/offconv $binfile $outfile
  fi
done
cd $cwd