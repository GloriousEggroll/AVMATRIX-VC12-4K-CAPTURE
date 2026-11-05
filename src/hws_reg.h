/*

*/

#ifndef _HWS_PCIE_REG_H
#define _HWS_PCIE_REG_H

#define HWS_VIDEO_NAME "HUHDVideo"
#define HWS_AUDOI_NAME "HUHDAudio"




#define MIN_VAMP_BRIGHTNESS_UNITS   0
#define MAX_VAMP_BRIGHTNESS_UNITS   0xff

#define MIN_VAMP_CONTRAST_UNITS     0
#define MAX_VAMP_CONTRAST_UNITS     0xff

#define MIN_VAMP_SATURATION_UNITS   0
#define MAX_VAMP_SATURATION_UNITS   0xff

#define MIN_VAMP_HUE_UNITS          0
#define MAX_VAMP_HUE_UNITS          0xff


#define BrightnessDefault  0x80
#define ContrastDefault    0x80
#define SaturationDefault  0x80
#define HueDefault         0x80	/* hardware reset value (register 91 reads 0x80808080) */

typedef struct frame_size
{
	int width;
	int height;
}framegrabber_frame_size_t;
typedef enum
{
	YUYV = 0,
	UYVY,
	YVYU,
	VYUY,
	RGBP,
	RGBR,
	RGBO,
	RGBQ,
	RGB3,
	BGR3,
	RGB4,
	BGR4,	
}framegrabber_pixfmt_enum_t;

typedef struct  {
	const char *name;
	int   fourcc;          /* v4l2 format id */
	char  depth;
	char  is_yuv;
	framegrabber_pixfmt_enum_t pixfmt_out;
}framegrabber_pixfmt_t;

typedef enum {
    V4L2_MODEL_VIDEOFORMAT_UNSUPPORTED = -1,
	V4L2_MODEL_VIDEOFORMAT_1920X1080P60,
	V4L2_MODEL_VIDEOFORMAT_3840X2160P60,
	V4L2_MODEL_VIDEOFORMAT_1280X720P60,
    V4L2_MODEL_VIDEOFORMAT_720X480P60,
    V4L2_MODEL_VIDEOFORMAT_720X576P50,
    V4L2_MODEL_VIDEOFORMAT_800X600P60,
    V4L2_MODEL_VIDEOFORMAT_1024X768P60,
    V4L2_MODEL_VIDEOFORMAT_1280X768P60,
    V4L2_MODEL_VIDEOFORMAT_1280X800P60,
    V4L2_MODEL_VIDEOFORMAT_1280X1024P60,
    V4L2_MODEL_VIDEOFORMAT_1360X768P60,
    V4L2_MODEL_VIDEOFORMAT_1440X900P60,
    V4L2_MODEL_VIDEOFORMAT_1680X1050P60,
    V4L2_MODEL_VIDEOFORMAT_1920X1200P60,
    V4L2_MODEL_VIDEOFORMAT_2560X1080P60,
    V4L2_MODEL_VIDEOFORMAT_2560X1440P60,
    V4L2_MODEL_VIDEOFORMAT_4096X2160P60, 
    V4L2_MODEL_VIDEOFORMAT_NUM,
} v4l2_model_videoformat_e;
	
typedef struct
{
	struct frame_size frame_size;
	int refresh_rate;
	bool  is_interlace;
} v4l2_model_timing_t;


#define V4L2_MODEL_TIMING(w,h,f,i) \
	{ \
		.frame_size = { \
			.width = w, \
			.height = h, \
		}, \
		.refresh_rate = f, \
		.is_interlace = i, \
	}
typedef enum
{
	FRAMEGRABBER_PIXFMT_YUYV=0,
	#if 0
	FRAMEGRABBER_PIXFMT_UYVY,
	FRAMEGRABBER_PIXFMT_YVYU,
	FRAMEGRABBER_PIXFMT_VYUY,
	FRAMEGRABBER_PIXFMT_RGB565,
	FRAMEGRABBER_PIXFMT_RGB565X,
	FRAMEGRABBER_PIXFMT_RGB555,
	FRAMEGRABBER_PIXFMT_RGB555X,
	FRAMEGRABBER_PIXFMT_RGB24,
	FRAMEGRABBER_PIXFMT_BGR24,
	FRAMEGRABBER_PIXFMT_RGB32,
	FRAMEGRABBER_PIXFMT_BGR32,
	#endif
	FRAMEGRABBER_PIXFMT_MAX,
}framegrabber_pixfmt_e;
	
typedef enum
{
    REFRESHRATE_15=0,
    REFRESHRATE_24,
    REFRESHRATE_25,
    REFRESHRATE_30,
    REFRESHRATE_50,
    REFRESHRATE_60,
    REFRESHRATE_100,
    REFRESHRATE_120,
    REFRESHRATE_144,
    REFRESHRATE_240,
    FRAMEGRABBER_SUPPORT_REFERSHRATE_NUM,
}framegrabber_refreshrate_e;
//---------------------------------
//osd
typedef enum {

    BLACK = 0,
    WHITE,
    YELLOW,
    CYAN,
    GREEN,
    MAGENTA,
    RED,
    BLUE,
    GREY,

    MAX_COLOR,
    TRANSPARENT,

} COLOR;

//----------------------------
/*
 * Register 9: DMA size limit in 16-byte units. A quarter DMA'd with a length
 * shorter than the frame (the first frames after a resolution change) runs
 * on up to this offset and ends with a 2 KiB block there. Reset value 0x44000
 * (4.25 MiB), past the end of a 4 MiB buffer. The driver sets it to the
 * largest programmed quarter (hws_set_dma_max).
 */
#define HWS_REG_DMA_MAX_SIZE	(CVBS_IN_BASE + 9 * PCIE_BARADDROFSIZE)
/* Register 8: bit ch set while the source uses HDCP (stage 4.1) */
#define HWS_REG_HDCP_STATUS	(CVBS_IN_BASE + 8 * PCIE_BARADDROFSIZE)
/* Register 110 + ch: input frame rate in whole fps, FPGA 122 and later (stage 4.2) */
#define HWS_REG_IN_FPS(ch)	(CVBS_IN_BASE + (110 + (ch)) * PCIE_BARADDROFSIZE)

#endif
