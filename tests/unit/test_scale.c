/*
 * Tests for src/hws_scale.c (stage 7.5): every scaler writes exactly
 * out_w x out_h x 2 bytes. Source and output buffers are allocated at
 * exactly their frame size, so ASan stops on any read or write past them;
 * each pair runs twice with different fill bytes, and an output byte equal
 * to the fill both times was never written.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hws_scale.h"

/* the driver's output sizes (support_videofmt in hws_video.c) */
static const int sizes[][2] = {
	{ 1920, 1080 }, { 3840, 2160 }, { 1280, 720 }, { 720, 480 }, { 720, 576 },
	{ 800, 600 }, { 1024, 768 }, { 1280, 768 }, { 1280, 800 }, { 1280, 1024 },
	{ 1360, 768 }, { 1440, 900 }, { 1680, 1050 }, { 1920, 1200 }, { 2560, 1080 },
	{ 2560, 1440 }, { 4096, 2160 },
};
/* input sizes no output size matches */
static const int extra_in[][2] = { { 1600, 900 }, { 1366, 768 }, { 2048, 1080 }, { 640, 480 } };

static int run(int iw, int ih, int ow, int oh)
{
	size_t in_size = (size_t)iw * ih * 2, out_size = (size_t)ow * oh * 2, i, unwritten = 0;
	BYTE *in = malloc(in_size), *a = malloc(out_size), *b = malloc(out_size);

	if (!in || !a || !b) {
		perror("malloc");
		exit(2);
	}
	for (i = 0; i < in_size; i++)
		in[i] = (BYTE)(i * 2654435761u >> 24);
	if (getenv("VERBOSE"))
		fprintf(stderr, "%dx%d -> %dx%d\n", iw, ih, ow, oh);
	memset(a, 0xA5, out_size);
	memset(b, 0x5A, out_size);
	VideoScaler(in, a, iw, ih, ow, oh);
	VideoScaler(in, b, iw, ih, ow, oh);
	for (i = 0; i < out_size; i++)
		if (a[i] == 0xA5 && b[i] == 0x5A)
			unwritten++;
	free(in);
	free(a);
	free(b);
	if (unwritten) {
		fprintf(stderr, "%dx%d -> %dx%d: %zu of %zu output bytes not written\n",
			iw, ih, ow, oh, unwritten, out_size);
		return 1;
	}
	return 0;
}

int main(void)
{
	int n = sizeof(sizes) / sizeof(sizes[0]), x = sizeof(extra_in) / sizeof(extra_in[0]);
	int i, o, pairs = 0, failures = 0;

	for (i = 0; i < n + x; i++) {
		const int *in = i < n ? sizes[i] : extra_in[i - n];

		for (o = 0; o < n; o++) {
			if (in[0] == sizes[o][0] && in[1] == sizes[o][1])
				continue;	/* same size: the driver copies, no scaler */
			failures += run(in[0], in[1], sizes[o][0], sizes[o][1]);
			pairs++;
		}
	}
	if (failures) {
		fprintf(stderr, "test_scale: %d of %d pairs failed\n", failures, pairs);
		return 1;
	}
	printf("test_scale: all %d pairs passed\n", pairs);
	return 0;
}
