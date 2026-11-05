/*
 * Pure calculations the driver depends on, kept free of driver state so
 * tests/unit can build them in userspace (stage 7.5). Each one is the
 * driver's own arithmetic, lifted out unchanged.
 */
#ifndef _HWS_CALC_H_
#define _HWS_CALC_H_

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/math64.h>
#include <linux/time64.h>
#else
#include <stdbool.h>
#include <stdint.h>
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
static inline u64 div_u64(u64 a, u32 b) { return a / b; }
static inline u64 div64_u64(u64 a, u64 b) { return a / b; }
#define NSEC_PER_MSEC	1000000ULL
#define NSEC_PER_SEC	1000000000ULL
#endif

/* The card moves video in 2 KiB blocks. */
#define HWS_DMA_BLOCK	2048

/*
 * Quarter lengths for a w x h YUYV input (SetVideoFormteSize()): quarters
 * 0-2 each get a quarter of the frame rounded down to whole blocks, and
 * quarter 3 the rest rounded up to the next block. Returns -1 if the first
 * three already exceed the frame.
 */
static inline int hws_quarter_sizes(int w, int h, u32 len[4])
{
	int frame = w * h * 2;
	int q = frame / (4 * HWS_DMA_BLOCK) * HWS_DMA_BLOCK;
	int rest = frame - q * 3;

	if (rest < 0)
		return -1;
	len[0] = len[1] = len[2] = q;
	len[3] = (rest / HWS_DMA_BLOCK + 1) * HWS_DMA_BLOCK;
	return 0;
}

/*
 * Bytes a zero-copy buffer must hold: quarter 3's offset plus the longest
 * run the card can make there, the largest quarter plus one block (register
 * 9 caps a run at the largest quarter; the block in progress completes).
 */
static inline u32 hws_zc_span(const u32 len[4])
{
	u32 max = 0;
	int q;

	for (q = 0; q < 4; q++)
		if (len[q] > max)
			max = len[q];
	return len[0] + len[1] + len[2] + max + HWS_DMA_BLOCK;
}

/*
 * Zero-copy buffers end this far past the frame, so hws_zc_span() fits for
 * every input size; tests/unit finds 4096 is the least that does. The
 * #ifndef lets tests/unit build with a smaller value to see the check fail.
 */
#ifndef HWS_ZC_SLACK
#define HWS_ZC_SLACK	8192
#endif

/* Standard frame intervals, as numerator/denominator seconds. */
static const u32 hws_std_intervals[][2] = {
	{ 1001, 24000 }, { 1, 24 }, { 1, 25 }, { 1001, 30000 }, { 1, 30 },
	{ 1, 50 }, { 1001, 60000 }, { 1, 60 }, { 1, 100 }, { 1001, 120000 },
	{ 1, 120 }, { 1, 144 }, { 1, 240 },
};
#define HWS_N_STD_INTERVALS (sizeof(hws_std_intervals) / sizeof(hws_std_intervals[0]))

/*
 * The standard interval closest to meas_ns, if it is within 1/tol of that
 * interval (tol 200 = 0.5%, 50 = 2%); -1 otherwise.
 */
static inline int hws_snap_interval(u64 meas_ns, u32 tol)
{
	u64 best_err = ~0ULL, std, err;
	int i, best = -1;

	for (i = 0; i < (int)HWS_N_STD_INTERVALS; i++) {
		std = div_u64((u64)hws_std_intervals[i][0] * NSEC_PER_SEC, hws_std_intervals[i][1]);
		err = meas_ns > std ? meas_ns - std : std - meas_ns;
		if (err < best_err) {
			best_err = err;
			best = i;
		}
	}
	if (best >= 0 && best_err * tol <= div_u64((u64)hws_std_intervals[best][0] * NSEC_PER_SEC,
						    hws_std_intervals[best][1]))
		return best;
	return -1;
}

/*
 * The frame-rate window (hws_frame_end()). Frame ends more than 200 ms
 * apart restart it. Each interval counts as the number of frames it spans,
 * in units of the shortest interval seen, so a dropped interrupt doesn't
 * halve the rate. After a second, the mean interval is returned.
 */
struct hws_rate_win {
	u64 frame_end;	/* last frame end, 0 = none */
	u64 start;	/* window start */
	u64 min_delta;	/* shortest interval in the window */
	u32 frames;	/* frames counted in the window */
};

/* Feed one frame end; returns the measured interval in ns when a window closes, else 0. */
static inline u64 hws_rate_frame_end(struct hws_rate_win *r, u64 now)
{
	u64 prev = r->frame_end, d, elapsed, iv;
	u32 n;

	r->frame_end = now;
	if (!prev || now - prev > 200 * NSEC_PER_MSEC || !r->start) {
		r->start = now;
		r->frames = 0;
		r->min_delta = 0;
		return 0;
	}
	d = now - prev;
	if (!r->min_delta || d < r->min_delta)
		r->min_delta = d;
	n = (u32)div64_u64(d + r->min_delta / 2, r->min_delta);
	r->frames += n ? n : 1;
	elapsed = now - r->start;
	if (elapsed < NSEC_PER_SEC)
		return 0;
	iv = div_u64(elapsed, r->frames);
	r->start = now;
	r->frames = 0;
	return iv;
}

/*
 * Of n candidate frame intervals, the index of the one closest to want_ns,
 * if it is within 1% of want_ns; -1 otherwise (hws_query_timings()).
 */
static inline int hws_closest_interval(const u64 *cand_ns, int n, u64 want_ns)
{
	u64 best_err = ~0ULL, err;
	int i, best = -1;

	for (i = 0; i < n; i++) {
		err = cand_ns[i] > want_ns ? cand_ns[i] - want_ns : want_ns - cand_ns[i];
		if (err < best_err) {
			best_err = err;
			best = i;
		}
	}
	return best >= 0 && best_err * 100 <= want_ns ? best : -1;
}

/*
 * True while event counter ev hasn't reached mark, across wraparound: the
 * zero-copy release check, which keeps a buffer until both quarter pairs
 * have passed the event it was moved away at.
 */
static inline bool hws_ev_before(u32 ev, u32 mark)
{
	return (s32)(ev - mark) < 0;
}

#endif
