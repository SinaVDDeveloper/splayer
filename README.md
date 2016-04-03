#setup 1 
#	unzip ffmpeg source package 
#	es. ~/ffmpeg/
#	unzip splayer package in ffmpeg dir
#	es. ~/ffmpeg/splayer/

#setup 2
#complie ffmpeg library
	cd ~/ffmpeg/
	mv ~/ffmpeg/splayer/ffmpeg/build_ffmpeg ~/ffmpeg/
#default build for arm
	./build_ffmpeg 
#build for x86
	./build_ffmpeg x86
#build for mips
	./build_ffmpeg mips

#build result in ~/prebuild/ dir


#setup 3
#complie splayer library
cd ~/ffmpeg/splayer/
./build.sh
