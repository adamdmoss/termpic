
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#include "argparse.h"

int main(int argc, const char* argv[])
{
	int reqwidth=0, reqheight=0;
	int no_alpha_output = 0;
	int whitebg = 0;
	int blackbg = 0;
	int full_alpha_range = 0;

	/* process command-line */
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_INTEGER('\0', "width", &reqwidth, "Desired width in characters", NULL, 0, 0),
        OPT_INTEGER('\0', "height", &reqheight, "Desired height in characters", NULL, 0, 0),
        OPT_BOOLEAN('\0', "no-alpha", &no_alpha_output, "Ignore transparency in image", NULL, 0, 0),
        OPT_BOOLEAN('\0', "full-alpha", &full_alpha_range, "Use full alpha range from source image (may increase fringing)", NULL, 0, 0),
        OPT_BOOLEAN('\0', "white", &whitebg, "Assume a white terminal background instead of black", NULL, 0, 0),
        OPT_BOOLEAN('\0', "gray", &blackbg, "Assume a dark gray terminal background instead of black", NULL, 0, 0),
        OPT_END(),
    };
    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argparse_describe(&argparse, "\ttermpic [opts] filename\n\nDisplays an image rendered full-color in modern terminals.  Most common image formats are supported.", NULL);
    argc = argparse_parse(&argparse, argc, argv);
	reqheight *= 2; /* from this point onwards we assume half-height characters, i.e. square pixels */

	if (argc != 1)
	{
		argparse_usage(&argparse);
		return -1;
	}

	/* prep data */
	uint8_t reqbg[4] = {0, 0, 0, 0/* 4th channel must be 0 */};
	if (whitebg)
		reqbg[0] = reqbg[1] = reqbg[2] = 255;
	if (blackbg)
		reqbg[0] = reqbg[1] = reqbg[2] = 32;

	/* load image */
	int rawwidth,rawheight,rawchannels;
	unsigned char *rawdata = stbi_load(argv[0], &rawwidth, &rawheight, &rawchannels, 4);
	if (!rawdata)
	{
		fprintf(stderr, "Could not load '%s'\n", argv[0]);
		return -2;
	}

	int had_alpha = (rawchannels == 2 || rawchannels == 4);

	/* figure out ideal sizing */
	int max_magic_width = 79;
	int max_magic_height = 100;

    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
	max_magic_width = w.ws_col;
	max_magic_height = w.ws_row * /*demicharacters*/ 2;
	if (max_magic_height > 1) max_magic_height-=2; /* leave a line for the command-line */

	if (reqwidth == 0 && reqheight == 0)
	{
		reqwidth = rawwidth;
		reqheight = rawheight;

		if (reqwidth > max_magic_width)
		{
			reqwidth = max_magic_width;
			reqheight = rawheight * ((float)max_magic_width / rawwidth);
		}
		if (reqheight > max_magic_height)
		{
			reqheight = max_magic_height;
			reqwidth = rawwidth * ((float)max_magic_height / rawheight);
		}
	}
	else if (reqheight == 0)
	{
		reqheight = rawheight * ((float)reqwidth / rawwidth);
	}
	else if (reqwidth == 0)
	{
		reqwidth = rawwidth * ((float)reqheight / rawheight);
	}
	if (reqheight <= 1) reqheight = 1;
	if (reqwidth <= 1) reqwidth = 1;

	unsigned char *resizeddata = NULL;
	int width, height;

	/* perform resize, if necessary */
	if (rawwidth != reqwidth || rawheight != reqheight)
	{
		resizeddata = malloc(4 * reqwidth * reqheight);
		stbir_resize_uint8_srgb(
			rawdata, rawwidth, rawheight, 0,
			resizeddata, reqwidth, reqheight, 0,
			4, (had_alpha)? 3: STBIR_ALPHA_CHANNEL_NONE, 0
		);
		free(rawdata); rawdata = NULL;
		width = reqwidth;
		height = reqheight;

	}
	else
	{
		width = rawwidth;
		height = rawheight;
		resizeddata = rawdata; rawdata = NULL;
	}

	/* if input genuinely contained alpha then blend image color with solid color */
	if (had_alpha)
	{
		for (int offset=0; offset<width*height; ++offset)
		{
			float alpha = resizeddata[offset*4 + 3] / 255.f;
			resizeddata[offset*4 + 0] = resizeddata[offset*4 + 0] * alpha + reqbg[0] * (1.f - alpha);
			resizeddata[offset*4 + 1] = resizeddata[offset*4 + 1] * alpha + reqbg[1] * (1.f - alpha);
			resizeddata[offset*4 + 2] = resizeddata[offset*4 + 2] * alpha + reqbg[2] * (1.f - alpha);
		}
	}

	/* output image */
	for (int y=0; y<height; y+=2)
	{
		for (int x=0; x<width; ++x)	
		{
			uint8_t* fg = &resizeddata[(x + y * width) * 4];
			uint8_t* bg = (y+1 < height) ? &resizeddata[(x + (y+1) * width) * 4] : reqbg;
			if (((bg[3] <= 128 && fg[3] <= 128 && !full_alpha_range) ||
				(bg[3] == 0 && fg[3] == 0))
				&& !no_alpha_output) /* 100% transparent? print one 'normal' space */
				printf("\e[0m ");
			else
				printf("\e[38;2;%d;%d;%dm\e[48;2;%d;%d;%dm▄", bg[0], bg[1], bg[2], fg[0], fg[1], fg[2]);
		}
		puts("\e[0m"); /* reset + newline */
	}
	
	free(resizeddata);
	return 0;
}
