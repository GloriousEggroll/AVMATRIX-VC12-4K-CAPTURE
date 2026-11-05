#ifndef _HWS_SCALE_H_
#define _HWS_SCALE_H_

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>	/* tests/unit builds hws_scale.c in userspace */
#endif

/* the same types as hws.h */
typedef unsigned int DWORD;
typedef unsigned char BYTE;

/* Scale a YUYV frame; writes out_w x out_h x 2 bytes to pOut. */
void VideoScaler(BYTE *pSrc, BYTE *pOut, int in_w, int in_h, int out_w, int out_h);

#endif
