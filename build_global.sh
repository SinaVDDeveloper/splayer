#!/bin/sh


TARGET="armv6 vfp neon armv7 x86 mips"

rm -rf libsplayer.tar.gz
rm -rf ./global/*
rm -rf ./global
mkdir ./global

for version in $TARGET; do

rm -rf ./libs/armeabi*
rm -rf ./libs
rm -rf ./obj/


export PALTFORM_DIR=$version    #vfp armv6 armv7 neon
export BUILD_TYPE=build

#$ANDROID_NDK_ROOT/ndk-build NDK_DEBUG=1
$ANDROID_NDK_ROOT/ndk-build

if [ "$version" = "armv6" ]
then
dstdir=60
fi
if [ "$version" = "vfp" ]
then
dstdir=61
fi
if [ "$version" = "armv7" ]
then
dstdir=70
fi
if [ "$version" = "neon" ]
then
dstdir=71
fi
if [ "$version" = "x86" ]
then
dstdir=80
fi

if [ "$version" = "mips" ]
then
dstdir=90
fi


mkdir ./global/$dstdir
if [ "$version" = "vfp" ]  || [ "$version" = "armv6" ]
then
mv ./libs/armeabi/libsplayer.so  ./global/$dstdir/libsplayer.so
elif [ "$version" = "neon" ]  || [ "$version" = "armv7" ]
then
mv ./libs/armeabi-v7a/libsplayer.so  ./global/$dstdir/libsplayer.so
elif [ "$version" = "x86" ]
then
mv ./libs/x86/libsplayer.so  ./global/$dstdir/libsplayer.so
elif [ "$version" = "mips" ]
then
mv ./libs/mips/libsplayer.so  ./global/$dstdir/libsplayer.so
fi


done

#tar -zcvf libsplayer.tar.gz build/
cd global/
chmod -R 777 .
7zr a -t7z libsplayer_global.7z *
chmod 777 libsplayer_global.7z
mv libsplayer_global.7z ../
cd ../
