/*
 * Tests for src/hws_calc.h, the driver's pure calculations (stage 7.5).
 * Built with ASan and UBSan by the Makefile; exits non-zero on the first
 * failed check.
 */
#include <stdio.h>
#include <stdlib.h>
#include "hws_calc.h"

static int failures;

#define CHECK(cond, ...) do {						\
	if (!(cond)) {							\
		fprintf(stderr, "%s:%d: %s: ", __FILE__, __LINE__, #cond);	\
		fprintf(stderr, __VA_ARGS__);				\
		fputc('\n', stderr);					\
		failures++;						\
	}								\
} while (0)

/* Every input size the driver accepts: up to 4096 x 2160 (MAX_VIDEO_HW_W/H). */
#define MAX_W 4096
#define MAX_H 2160

static void test_quarter_sizes(void)
{
	u32 len[4];
	int w, h;

	/* values read from the card's registers and debugfs on FPGA 122 */
	CHECK(hws_quarter_sizes(3840, 2160, len) == 0, "4K rejected");
	CHECK(len[0] == 4147200 && len[3] == 4149248, "4K: %u %u", len[0], len[3]);
	CHECK(hws_quarter_sizes(1920, 1080, len) == 0, "1080p rejected");
	CHECK(len[0] == 1036288 && len[3] == 1040384, "1080p: %u %u", len[0], len[3]);

	for (w = 2; w <= MAX_W; w += 2)
		for (h = 2; h <= MAX_H; h += 2) {
			u32 frame = (u32)w * h * 2;

			if (hws_quarter_sizes(w, h, len) < 0) {
				CHECK(0, "%dx%d rejected", w, h);
				continue;
			}
			CHECK(len[0] % HWS_DMA_BLOCK == 0 && len[3] % HWS_DMA_BLOCK == 0,
			      "%dx%d: not whole blocks", w, h);
			CHECK(len[0] == len[1] && len[1] == len[2], "%dx%d: quarters 0-2 differ", w, h);
			/* the quarters cover the frame, with less than one extra block */
			CHECK(len[0] * 3 + len[3] >= frame, "%dx%d: quarters miss the frame", w, h);
			CHECK(len[0] * 3 + len[3] - frame <= HWS_DMA_BLOCK,
			      "%dx%d: %u bytes past the frame", w, h, len[0] * 3 + len[3] - frame);
			if (failures > 20)
				return;
		}
}

/*
 * A zero-copy buffer is the frame plus HWS_ZC_SLACK; the card's longest run
 * (hws_zc_span()) must fit in it for every input size.
 */
static void test_zc_slack(void)
{
	u32 len[4], need, worst = 0;
	int w, h, ww = 0, wh = 0;

	for (w = 2; w <= MAX_W; w += 2)
		for (h = 2; h <= MAX_H; h += 2) {
			u32 frame = (u32)w * h * 2;

			if (hws_quarter_sizes(w, h, len) < 0)
				continue;
			need = hws_zc_span(len) - frame;
			if (need > worst) {
				worst = need;
				ww = w;
				wh = h;
			}
		}
	printf("zero-copy slack: largest need %u bytes (%dx%d); HWS_ZC_SLACK is %u\n",
	       worst, ww, wh, (unsigned)HWS_ZC_SLACK);
	CHECK(worst <= HWS_ZC_SLACK, "HWS_ZC_SLACK %u is less than the %u bytes %dx%d needs",
	      (unsigned)HWS_ZC_SLACK, worst, ww, wh);
}

static u64 ns_for(u32 num, u32 den)
{
	return (u64)num * NSEC_PER_SEC / den;
}

static void test_snap(void)
{
	int i;

	/* each standard interval snaps to itself, with either tolerance */
	for (i = 0; i < (int)HWS_N_STD_INTERVALS; i++) {
		u64 ns = ns_for(hws_std_intervals[i][0], hws_std_intervals[i][1]);

		CHECK(hws_snap_interval(ns, 200) == i, "standard %d doesn't snap to itself", i);
		/* 0.02%: the 1000/1001 pairs are only 0.1% apart, so stay well inside that */
		CHECK(hws_snap_interval(ns + ns / 5000, 200) == i, "standard %d +0.02%% lost", i);
	}
	/* 59.94 and 60 stay apart at 0.5% */
	i = hws_snap_interval(16683350, 200);
	CHECK(i >= 0 && hws_std_intervals[i][0] == 1001 && hws_std_intervals[i][1] == 60000, "59.94 -> %d", i);
	i = hws_snap_interval(16666667, 200);
	CHECK(i >= 0 && hws_std_intervals[i][0] == 1 && hws_std_intervals[i][1] == 60, "60 -> %d", i);
	/* nothing standard near 45 fps or 75 fps */
	CHECK(hws_snap_interval(ns_for(1, 45), 200) == -1, "45 fps snapped");
	CHECK(hws_snap_interval(ns_for(1, 75), 50) == -1, "75 fps snapped at 2%%");
	/* the card's whole-fps register, 2%: 59 fps reads as 59.94 */
	i = hws_snap_interval(ns_for(1, 59), 50);
	CHECK(i >= 0 && hws_std_intervals[i][1] == 60000, "59 fps register -> %d", i);
	/* 1% off misses at 0.5% but not at 2% */
	CHECK(hws_snap_interval(ns_for(1, 60) * 101 / 100, 200) == -1, "1%% off snapped at 0.5%%");
	CHECK(hws_snap_interval(ns_for(1, 60) * 101 / 100, 50) >= 0, "1%% off missed at 2%%");
}

static void test_rate_window(void)
{
	struct hws_rate_win r = { 0 };
	u64 t = 1000000000ULL, iv = 0, step = 16666667;
	int i;

	CHECK(hws_rate_frame_end(&r, t) == 0, "first frame gave a rate");
	for (i = 1; i <= 61 && !iv; i++)
		iv = hws_rate_frame_end(&r, t + i * step);
	CHECK(iv >= step - 2 && iv <= step + 2, "steady 60 fps measured %llu", (unsigned long long)iv);

	/* a missed frame end (one 33 ms gap) still counts as two frames */
	r = (struct hws_rate_win){ 0 };
	iv = 0;
	hws_rate_frame_end(&r, t);
	for (i = 1; i <= 70 && !iv; i++) {
		if (i == 30)
			continue;
		iv = hws_rate_frame_end(&r, t + i * step);
	}
	CHECK(iv >= step - 2 && iv <= step + 2, "60 fps with a gap measured %llu", (unsigned long long)iv);

	/* more than 200 ms between frame ends restarts the window */
	r = (struct hws_rate_win){ 0 };
	hws_rate_frame_end(&r, t);
	hws_rate_frame_end(&r, t + step);
	CHECK(hws_rate_frame_end(&r, t + step + 250000000ULL) == 0, "250 ms gap gave a rate");
	CHECK(r.frames == 0 && r.start == t + step + 250000000ULL, "250 ms gap didn't restart");

	/* 240 fps */
	r = (struct hws_rate_win){ 0 };
	iv = 0;
	step = 4166667;
	hws_rate_frame_end(&r, t);
	for (i = 1; i <= 250 && !iv; i++)
		iv = hws_rate_frame_end(&r, t + i * step);
	CHECK(iv >= step - 2 && iv <= step + 2, "240 fps measured %llu", (unsigned long long)iv);
}

static void test_closest(void)
{
	u64 cand[] = { 16683333, 16666667, 20000000 };	/* 59.94, 60, 50 */

	CHECK(hws_closest_interval(cand, 3, 16666667) == 1, "60 not picked");
	CHECK(hws_closest_interval(cand, 3, 16683350) == 0, "59.94 not picked");
	/* the 1% is of the wanted interval */
	CHECK(hws_closest_interval(cand, 3, 20000000ULL * 102 / 100) == -1, "2%% off accepted");
	CHECK(hws_closest_interval(cand, 3, 20000000ULL * 199 / 200) == 2, "0.5%% off rejected");
	CHECK(hws_closest_interval(cand, 0, 16666667) == -1, "no candidates gave a match");
}

static void test_ev_before(void)
{
	CHECK(hws_ev_before(5, 6), "5 < 6");
	CHECK(!hws_ev_before(6, 6), "6 < 6");
	CHECK(!hws_ev_before(7, 6), "7 < 6");
	CHECK(hws_ev_before(0xFFFFFFFFu, 1), "wrap: 0xFFFFFFFF < 1");
	CHECK(!hws_ev_before(1, 0xFFFFFFFFu), "wrap: 1 < 0xFFFFFFFF");
	CHECK(hws_ev_before(0x7FFFFFFFu, 0x80000000u), "0x7FFFFFFF < 0x80000000");
}

int main(void)
{
	test_quarter_sizes();
	test_zc_slack();
	test_snap();
	test_rate_window();
	test_closest();
	test_ev_before();
	if (failures) {
		fprintf(stderr, "test_calc: %d failed\n", failures);
		return 1;
	}
	puts("test_calc: all passed");
	return 0;
}
