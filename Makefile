termpic: argparse.c argparse.h termpic.c stb_image.h stb_image_resize2.h
	$(CC) argparse.c termpic.c -o termpic -lm
	./termpic test.png --height=16
clean:
	rm -f termpic
