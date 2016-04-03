#!/bin/sh


TARGET="armv6 vfp neon armv7 x86 mips"

rm -rf libsplayer.tar.gz
rm -rf ./build/*
rm -rf ./build
mkdir ./build

for version in $TARGET; do

rm -rf ./libs/armeabi*
rm -rf ./libs
rm -rf ./obj/


export PALTFORM_DIR=$version    #vfp armv6 armv7 neon
export BUILD_TYPE=prebuild

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


mkdir ./build/$dstdir
if [ "$version" = "vfp" ]  || [ "$version" = "armv6" ]
then
    mv ./libs/armeabi/libsplayer.so  ./build/$dstdir/libsplayer.so
elif [ "$version" = "neon" ]  || [ "$version" = "armv7" ]
then   
    mv ./libs/armeabi-v7a/libsplayer.so  ./build/$dstdir/libsplayer.so
elif [ "$version" = "x86" ]
then
    mv ./libs/x86/libsplayer.so  ./build/$dstdir/libsplayer.so
elif [ "$version" = "mips" ]
then
    mv ./libs/mips/libsplayer.so  ./build/$dstdir/libsplayer.so
fi


done

#tar -zcvf libsplayer.tar.gz build/
cd build/
chmod -R 777 .
7zr a -t7z libsplayer.7z *
chmod 777 libsplayer.7z
mv libsplayer.7z ../
cd ../
