termpic: argparse.c argparse.h termpic.c
	$(CC) argparse.c termpic.c -o termpic -lm
	./termpic test.png --height=16
clean:
	rm termpic
