#!/bin/sh
mkdir -p build

#MAJ=1
MAJ=0
#MIN=12
MIN=1

#ALL="MAINLH DOTCLH MKIILH SARULH"
ALL="WIV1LH"

for V in $ALL
do
    VERSION_MAJOR=${MAJ} VERSION_MINOR=${MIN} VARIANT=${V} make clean all > build/${V}.${MAJ}.${MIN}.log
done
