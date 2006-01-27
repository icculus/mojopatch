#!/bin/sh

# Build a Universal binary, gcc 3.3 on PowerPC and gcc 4 on Intel...
make BINDIR=bin-ppc CC=gcc-3.3 LD=gcc-3.3 EXTRACFLAGS="-arch ppc" EXTRALDFLAGS="-arch ppc" $* || exit 1
make BINDIR=bin-i386 CC=gcc-4.0 LD=gcc-4.0 EXTRACFLAGS="-arch i386" EXTRALDFLAGS="-arch i386" $* || exit 1

mkdir -p bin
for feh in `ls bin-ppc` ; do
    echo "Gluing bin-ppc/$feh and bin-i386/$feh into bin/$feh ..."
    lipo -create -o bin/$feh bin-ppc/$feh bin-i386/$feh
done

rm -rf bin-ppc bin-i386
exit 0

