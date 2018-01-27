all: data2img

data2img: data2img.c
	gcc -O2 data2img.c -I/usr/include/ImageMagick-6 `pkg-config --cflags --libs MagickWand` -o data2img