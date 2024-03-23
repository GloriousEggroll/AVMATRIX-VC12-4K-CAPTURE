/*
*/

#include <linux/pci.h>
#include <linux/kernel.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-vmalloc.h>
#include <media/videobuf2-dma-contig.h>
#include "hws.h"
#include "hws_reg.h"
#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/rawmidi.h>
#include <sound/initval.h>

static void hws_adapters_init(struct hws_pcie_dev *dev);
static void hws_get_video_param(struct hws_pcie_dev *dev,int index);
//static void StartDma(int index);
//static void StopDma(int index);
static int StartVideoCapture(struct hws_pcie_dev *pdx,int index);
static int StartAudioCapture(struct hws_pcie_dev *pdx,int index);
static void StopAudioCapture(struct hws_pcie_dev *pdx,int index);
static void StopVideoCapture(struct hws_pcie_dev *pdx,int index);
static void InitVideoSys(struct hws_pcie_dev *pdx,int set);

//------------------------

static	void VideoScaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void FHD_To_HD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void HD_To_FHD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void FHD_To_800X600_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void HD_To_800X600_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void SD_PAL_To_SD_NTSC_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void SD_NTSC_To_SD_PAL_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void SD_PAL_To_FHD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void SD_PAL_To_HD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void SD_NTSC_To_FHD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void SD_NTSC_To_HD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);	
static	void V1280X1024_To_FHD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void V1280X1024_To_HD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void V1280X1024_To_800X600_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void FHD_To_SD_NTSC_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);	
static	void FHD_To_SD_PAL_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void HD_To_SD_NTSC_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);		
static	void HD_To_SD_PAL_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);		
static	void V1280X1024_NTSC_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void V1280X1024_PAL_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
static	void All_VideoScaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h);
//---------------------------



//struct hws_pcie_dev  *sys_dvrs_hw_pdx=NULL;
//EXPORT_SYMBOL(sys_dvrs_hw_pdx);
//u32 *map_bar0_addr=NULL; //for sys bar0
//EXPORT_SYMBOL(map_bar0_addr);

//------------------------------------------

#define MAKE_ENTRY( __vend, __chip, __subven, __subdev, __configptr) {	\
	.vendor		= (__vend),					\
	.device		= (__chip),					\
	.subvendor	= (__subven),					\
	.subdevice	= (__subdev),					\
	.driver_data	= (unsigned long) (__configptr)			\
}

static const struct pci_device_id hws_pci_table[] = {
	MAKE_ENTRY(0x8888, 0x8581, 0x8888, 0x0007, NULL),
	MAKE_ENTRY(0x1F33, 0x8581, 0x8888, 0x0007, NULL),
	{ }
};
//------------------
static const v4l2_model_timing_t support_videofmt[]= {
	[V4L2_MODEL_VIDEOFORMAT_1920X1080P60]	= V4L2_MODEL_TIMING(1920,1080,60,0),
	[V4L2_MODEL_VIDEOFORMAT_3840X2160P60]	 = V4L2_MODEL_TIMING(3840,2160,60,0),
	[V4L2_MODEL_VIDEOFORMAT_1280X720P60]	= V4L2_MODEL_TIMING(1280,720,60,0),
    [V4L2_MODEL_VIDEOFORMAT_720X480P60]		= V4L2_MODEL_TIMING(720,480,60,0),
    [V4L2_MODEL_VIDEOFORMAT_720X576P50]		= V4L2_MODEL_TIMING(720,480,50,0),
    [V4L2_MODEL_VIDEOFORMAT_800X600P60]		= V4L2_MODEL_TIMING(800,600,60,0),
    [V4L2_MODEL_VIDEOFORMAT_1024X768P60]	= V4L2_MODEL_TIMING(1024,768,60,0),
    [V4L2_MODEL_VIDEOFORMAT_1280X768P60]	= V4L2_MODEL_TIMING(1280,768,60,0),
    [V4L2_MODEL_VIDEOFORMAT_1280X800P60]	= V4L2_MODEL_TIMING(1280,800,60,0),
    [V4L2_MODEL_VIDEOFORMAT_1280X1024P60]	= V4L2_MODEL_TIMING(1280,1024,60,0),
    [V4L2_MODEL_VIDEOFORMAT_1360X768P60]	= V4L2_MODEL_TIMING(1360,768,60,0),
    [V4L2_MODEL_VIDEOFORMAT_1440X900P60]	= V4L2_MODEL_TIMING(1440,900,60,0),
    [V4L2_MODEL_VIDEOFORMAT_1680X1050P60]	= V4L2_MODEL_TIMING(1680,1050,60,0),
    [V4L2_MODEL_VIDEOFORMAT_1920X1200P60]	= V4L2_MODEL_TIMING(1920,1200,60,0),
    [V4L2_MODEL_VIDEOFORMAT_2560X1080P60]	= V4L2_MODEL_TIMING(2560,1080,60,0),
    [V4L2_MODEL_VIDEOFORMAT_2560X1440P60]	= V4L2_MODEL_TIMING(2560,1440,60,0),
    [V4L2_MODEL_VIDEOFORMAT_4096X2160P60]	= V4L2_MODEL_TIMING(4096,2160,60,0),
};
static const framegrabber_pixfmt_t support_pixfmts[] = {
	
	[FRAMEGRABBER_PIXFMT_YUYV]={ //YUYV index=0
		.name     = "4:2:2, packed, YUYV",
		.fourcc   = V4L2_PIX_FMT_YUYV,
		.depth    = 16,
		.is_yuv   = true,
		.pixfmt_out = YUYV,
	},
	#if 0
	[FRAMEGRABBER_PIXFMT_UYVY]={ //UYVY
		.name     = "4:2:2, packed, UYVY",
		.fourcc   = V4L2_PIX_FMT_UYVY,
		.depth    = 16,
		.is_yuv   = true,
		.pixfmt_out = UYVY,
	},
	[FRAMEGRABBER_PIXFMT_YVYU]={ //YVYU
		.name     = "4:2:2, packed, YVYU",
		.fourcc   = V4L2_PIX_FMT_YVYU,
		.depth    = 16,
		.is_yuv   = true,
		.pixfmt_out = YVYU,
	},
	
	[FRAMEGRABBER_PIXFMT_VYUY]={ //VYUY
		.name     = "4:2:2, packed, VYUY",
		.fourcc   = V4L2_PIX_FMT_VYUY,
		.depth    = 16,
		.is_yuv   = true,
		.pixfmt_out = VYUY,
	},

	[FRAMEGRABBER_PIXFMT_RGB565]={ //RGBP
		.name     = "RGB565 (LE)",
		.fourcc   = V4L2_PIX_FMT_RGB565, /* gggbbbbb rrrrrggg */
		.depth    = 16,
		.is_yuv   = false,
		.pixfmt_out = RGBP,
	},
	[FRAMEGRABBER_PIXFMT_RGB565X]={ //RGBR
		.name     = "RGB565 (BE)",
		.fourcc   = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
		.depth    = 16,
		.is_yuv   = false,
		.pixfmt_out = RGBR,
	},
	[FRAMEGRABBER_PIXFMT_RGB555]={ //RGBO
		.name     = "RGB555 (LE)",
		.fourcc   = V4L2_PIX_FMT_RGB555, /* gggbbbbb arrrrrgg */
		.depth    = 16,
		.is_yuv   = false,
		.pixfmt_out = RGBO,
	},
	[FRAMEGRABBER_PIXFMT_RGB555X]={ //RGBQ
		.name     = "RGB555 (BE)",
		.fourcc   = V4L2_PIX_FMT_RGB555X, /* arrrrrgg gggbbbbb */
		.depth    = 16,
		.is_yuv   = false,
		.pixfmt_out = RGBQ,
	},
	[FRAMEGRABBER_PIXFMT_RGB24]={ //RGB3 index=8
		.name     = "RGB24 (LE)",
		.fourcc   = V4L2_PIX_FMT_RGB24, /* rgb */
		.depth    = 24,
		.is_yuv   = false,
		.pixfmt_out = RGB3,
	},

	[FRAMEGRABBER_PIXFMT_BGR24]={ //BGR3
		.name     = "RGB24 (BE)",
		.fourcc   = V4L2_PIX_FMT_BGR24, /* bgr */
		.depth    = 24,
		.is_yuv   = false,
		.pixfmt_out = BGR3,
	},
	[FRAMEGRABBER_PIXFMT_RGB32]={ //RGB4
		.name     = "RGB32 (LE)",
		.fourcc   = V4L2_PIX_FMT_RGB32, /* argb */
		.depth    = 32,
		.is_yuv   = false,
		.pixfmt_out = RGB4,
	},
	[FRAMEGRABBER_PIXFMT_BGR32]={ //BGR4
		.name     = "RGB32 (BE)",
		.fourcc   = V4L2_PIX_FMT_BGR32, /* bgra */
		.depth    = 32,
		.is_yuv   = false,
		.pixfmt_out = BGR4,
	},
	#endif
};

static const int framegrabber_support_refreshrate[]= {
    [REFRESHRATE_15]=15,
    [REFRESHRATE_24]=24,
    [REFRESHRATE_25]=25,
    [REFRESHRATE_30]=30,
    [REFRESHRATE_50]=50,
    [REFRESHRATE_60]=60,
    [REFRESHRATE_100]=100,
    [REFRESHRATE_120]=120,
    [REFRESHRATE_144]=144,
    [REFRESHRATE_240]=240,
};
#define NUM_FRAMERATE_CONTROLS (ARRAY_SIZE(framegrabber_support_refreshrate))

//-------------------------------------------
static int  v4l2_model_get_support_framerate(int index)
{
	if(index <0 ||index >=NUM_FRAMERATE_CONTROLS)
		return -1;

	return (framegrabber_support_refreshrate[index]);
}

static int v4l2_get_suport_VideoFormatIndex(struct v4l2_format *fmt) 
{
	struct v4l2_pix_format *pix = &fmt->fmt.pix;
	int index;
	int videoIndex=-1;
	for(index=0; index<V4L2_MODEL_VIDEOFORMAT_NUM;index++)
	{
		if((pix->width==support_videofmt[index].frame_size.width)&&(pix->height==support_videofmt[index].frame_size.height))
		{
			videoIndex = index;
			break;
		}
	}
	return videoIndex;
}
v4l2_model_timing_t *v4l2_model_get_support_videoformat(int index)
{
	if(index <0 ||index >=V4L2_MODEL_VIDEOFORMAT_NUM)
			return NULL;

	return (v4l2_model_timing_t *)&support_videofmt[index];
}


framegrabber_pixfmt_t *v4l2_model_get_support_pixformat(int index)
{
	if(index <0 ||index >=ARRAY_SIZE(support_pixfmts))
			return NULL;

	return (framegrabber_pixfmt_t *)&support_pixfmts[index];
}
const framegrabber_pixfmt_t *framegrabber_g_support_pixelfmt_by_fourcc(u32 fourcc)
{
	int i;
	int pixfmt_index=-1;
	for(i=0;i<FRAMEGRABBER_PIXFMT_MAX;i++)
	{
		if(support_pixfmts[i].fourcc==fourcc)
		{
			
			pixfmt_index=i;
			break;
		}
	}
	if(pixfmt_index==-1)
			return NULL;

	return &support_pixfmts[pixfmt_index];
}

static int hws_vidioc_querycap(struct file *file, void *priv, struct v4l2_capability *cap)
{
	struct hws_video *videodev = video_drvdata(file);
	struct hws_pcie_dev *dev = videodev->dev;
	int vi_index;
	vi_index = videodev->index+1+dev->m_Device_PortID*dev->m_nCurreMaxVideoChl;
	//printk( "%s\n", __func__);
	strcpy(cap->driver, KBUILD_MODNAME);
	sprintf(cap->card, "%s %d",HWS_VIDEO_NAME,vi_index);
	strcpy(cap->bus_info, "HWS");
	cap->device_caps =	V4L2_CAP_VIDEO_CAPTURE |V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	//printk( "%s(IN END  )\n", __func__);
	return 0;
}
static int hws_vidioc_enum_fmt_vid_cap(struct file *file, void *priv_fh,struct v4l2_fmtdesc *f)
{
	struct hws_video *videodev = video_drvdata(file);
	int index = f->index;
	//printk( "%s(%d)\n", __func__,videodev->index);
	//printk( "%s(f->index = %d)\n", __func__,f->index);
	
	if(f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
	{
		printk("%s.\n",__func__);
		return -EINVAL;
	}
	if(videodev)
	{
		const framegrabber_pixfmt_t *pixfmt;
		if(f->index <0)
		{
			return -EINVAL;
		}
		if(f->index >= FRAMEGRABBER_PIXFMT_MAX)
		{
			return -EINVAL;
		}
		else
		{
			 pixfmt=v4l2_model_get_support_pixformat(f->index);
			 if(pixfmt ==NULL) return -EINVAL;
		    //printk("%s..pixfmt=%d.\n",__func__,f->index);
		    f->index = index;
		    f->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		    strscpy(f->description, pixfmt->name, sizeof(f->description));
		    f->pixelformat=pixfmt->fourcc;
		}
	}
	return 0;
}
void framegrabber_g_Curr_input_framesize(struct hws_video *dev,int *width,int *height)
{	
	struct hws_pcie_dev *pdx = dev->dev;
	int index = dev->index;
	*width= pdx->m_pVCAPStatus[index][0].dwWidth;
	*height=pdx->m_pVCAPStatus[index][0].dwHeight;
}
const framegrabber_pixfmt_t *framegrabber_g_out_pixelfmt(struct hws_video *dev)
{
	return &support_pixfmts[dev->current_out_pixfmt];
}
static int hws_vidioc_g_fmt_vid_cap(struct file *file, void *fh, struct v4l2_format *fmt)
{
	struct hws_video *videodev = video_drvdata(file);
	//struct v4l2_pix_format *pix = &fmt->fmt.pix;
	const framegrabber_pixfmt_t *pixfmt;
	v4l2_model_timing_t *p_SupportmodeTiming;
	//printk( "%s(%d)\n", __func__,videodev->index);
	//printk( "w=%d,h=%d\n",fmt->fmt.pix.width,fmt->fmt.pix.height);
	pixfmt=framegrabber_g_out_pixelfmt(videodev);
	if(pixfmt)
	{
		//framegrabber_g_Curr_input_framesize(videodev,&width,&height);
		p_SupportmodeTiming = v4l2_model_get_support_videoformat(videodev->current_out_size_index);
		if(p_SupportmodeTiming ==NULL) return -EINVAL;
		fmt->fmt.pix.width=p_SupportmodeTiming->frame_size.width;
		fmt->fmt.pix.height=p_SupportmodeTiming->frame_size.height;
		fmt->fmt.pix.field=V4L2_FIELD_NONE; //Field
		fmt->fmt.pix.pixelformat  = pixfmt->fourcc;
		fmt->fmt.pix.bytesperline = (fmt->fmt.pix.width * pixfmt->depth) >> 3;
		fmt->fmt.pix.sizeimage =	fmt->fmt.pix.height * fmt->fmt.pix.bytesperline;
		fmt->fmt.pix.colorspace = V4L2_COLORSPACE_REC709;
		//printk("%s....f->fmt.pix.width=%d.f->fmt.pix.height=%d.\n",__func__,fmt->fmt.pix.width,fmt->fmt.pix.height);
		return 0;
	} 

	return -EINVAL;
}
static v4l2_model_timing_t *Get_input_framesizeIndex(int width,int height)
{	
	int i;
	for(i =0;i<V4L2_MODEL_VIDEOFORMAT_NUM;i++)
	{
		if((support_videofmt[i].frame_size.width ==width)&&(support_videofmt[i].frame_size.height == height))
		{
			return (v4l2_model_timing_t *)&support_videofmt[i];
		}
	}
	return NULL;
}

static int hws_vidioc_try_fmt_vid_cap(struct file *file, void *fh, struct v4l2_format *f)
{
	struct hws_video *videodev = video_drvdata(file);
	v4l2_model_timing_t *pModeTiming;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	const framegrabber_pixfmt_t *fmt;
	//int TimeingIndex = f->index;
	//printk( "%s(%d)\n", __func__,videodev->index);
	//printk( "pix->height =%d  pix->width =%d \n", pix->height,pix->width);
	fmt = framegrabber_g_support_pixelfmt_by_fourcc(pix->pixelformat);
	if(!fmt)
    {
		printk("%s.. format not support \n",__func__);
        return -EINVAL;
	}
	pModeTiming = Get_input_framesizeIndex(pix->width,pix->height);
	if(!pModeTiming)
	{
		printk("%s.. format2 not support  %dX%d\n",__func__,pix->width,pix->height);
		pModeTiming = v4l2_model_get_support_videoformat(videodev->current_out_size_index);
		if(pModeTiming ==NULL) return -EINVAL;
		pix->field = V4L2_FIELD_NONE;
		pix->width=pModeTiming->frame_size.width;
		pix->height=pModeTiming->frame_size.height;
		pix->bytesperline = (pix->width * fmt->depth) >> 3;
		pix->sizeimage =pix->height * pix->bytesperline; 
		pix->colorspace = V4L2_COLORSPACE_REC709;//V4L2_COLORSPACE_SMPTE170M;
		pix->priv = 0;
        //return -EINVAL;
		return 0;
	}
	pix->field = V4L2_FIELD_NONE;
	pix->width=pModeTiming->frame_size.width;
	pix->height=pModeTiming->frame_size.height;
	pix->bytesperline = (pix->width * fmt->depth) >> 3;
	pix->sizeimage =pix->height * pix->bytesperline; 
	pix->colorspace = V4L2_COLORSPACE_REC709;//V4L2_COLORSPACE_SMPTE170M;
	pix->priv = 0;
	
   //printk("%s<<pix->width=%d.pix->height=%d.\n",__func__,pix->width,pix->height);      
	return 0;


//----------------------------------
	return 0;
}
static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,struct v4l2_format *f)
{
	struct hws_video *videodev = video_drvdata(file);
	int nVideoFmtIndex;
	int err;
	unsigned long flags;
	struct hws_pcie_dev *pdx = videodev->dev;
	//printk( "%s()\n", __func__);
	nVideoFmtIndex = v4l2_get_suport_VideoFormatIndex(f);
	if(nVideoFmtIndex ==-1) return -EINVAL;

	err = hws_vidioc_try_fmt_vid_cap(file, priv, f);
	if (0 != err)
		return err;
	spin_lock_irqsave(&pdx->videoslock[videodev->index], flags);	
	videodev->current_out_size_index = nVideoFmtIndex;
	videodev->pixfmt     = f->fmt.pix.pixelformat;
	videodev->current_out_width      = f->fmt.pix.width;
	videodev->curren_out_height     = f->fmt.pix.height;
	//printk("%s<<  current_out_size_index =%d current_out_width=%d.curren_out_height=%d.\n",__func__,videodev->current_out_size_index,videodev->current_out_width ,videodev->curren_out_height );
	spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
		return 0;
}
static int hws_vidioc_g_std(struct file *file, void *priv, v4l2_std_id *tvnorms)
{
	struct hws_video *videodev = video_drvdata(file);
	//printk( "%s()\n", __func__);
	*tvnorms = videodev->std;
	return 0;
}

static int hws_vidioc_s_std(struct file *file, void *priv,v4l2_std_id tvnorms)
{
	struct hws_video *videodev = video_drvdata(file);
	//printk( "%s()\n", __func__);
	videodev->std = tvnorms;
	return 0;
}

int hws_vidioc_g_parm(struct file *file,void *fh, struct v4l2_streamparm *setfps)
{
	struct hws_video *videodev = video_drvdata(file);
	v4l2_model_timing_t *p_SupportmodeTiming;
	
	//printk( "%s(%d) Frame Rate =%d \n", __func__, videodev->index,io_frame_rate);
	if (setfps->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
	{
		printk("%s..\n",__func__);
	    return -EINVAL;
	}
	p_SupportmodeTiming = v4l2_model_get_support_videoformat(videodev->current_out_size_index);
	if(p_SupportmodeTiming ==NULL) return -EINVAL;
    setfps->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    setfps->parm.capture.timeperframe.numerator=1000;
    setfps->parm.capture.timeperframe.denominator=p_SupportmodeTiming->refresh_rate*1000;
	//printk( "%s fps =%d \n", __func__, p_SupportmodeTiming->refresh_rate);
	return 0;	
}
static int hws_vidioc_enum_framesizes(struct file *file, void *fh, struct v4l2_frmsizeenum *fsize)
{
	//struct hws_video *videodev = video_drvdata(file);
	const framegrabber_pixfmt_t *pixfmt;
	v4l2_model_timing_t *p_SupportmodeTiming;
	int width=0,height=0;
	int frameRate;
	//printk( "%s(%d)-FrameIndex=[%d]\n", __func__,videodev->index,fsize->index);
	//----------------------------
	pixfmt=framegrabber_g_support_pixelfmt_by_fourcc(fsize->pixel_format);
	if(pixfmt==NULL)
	{
		//printk("%s..\n",__func__);
		return -EINVAL;
	}
	p_SupportmodeTiming = v4l2_model_get_support_videoformat(fsize->index);
	if(p_SupportmodeTiming == NULL)
	{
		//printk("%s. invalid framesize[%d]\n",__func__,fsize->index);
		return -EINVAL;
	}
	width = p_SupportmodeTiming->frame_size.width;
	height = p_SupportmodeTiming->frame_size.height;
	frameRate = p_SupportmodeTiming->refresh_rate;

	//printk("%s...supportframesize[%d] width=%d height=%d Framerate=%d..\n",__func__,fsize->index,width,height,frameRate); //12
	if((width ==0) || (height ==0))
	{
		//printk("%s. invalid framesize 2\n",__func__);
		return -EINVAL;
	}
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->pixel_format=pixfmt->fourcc;
	fsize->discrete.width=width;
	fsize->discrete.height=height;
	//fsize->discrete.denominator = frameRate;
	//fsize->discrete..numerator =1 ;
	//-------------------------------
	//width = videodev->current_out_width;
	//height = videodev->curren_out_height;
	//fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	//fsize->pixel_format=videodev->pixfmt;
	//fsize->discrete.width=width;
	//fsize->discrete.height=height;
	return 0;

	
}

static int hws_vidioc_enum_input(struct file *file, void *priv,struct v4l2_input *i)
{
	//struct hws_video *videodev = video_drvdata(file);
	int Index;
	Index = i->index; 
	//printk( "%s(%d)-%d Index =%d \n", __func__,videodev->index,i->index,Index);
	if(Index >0)
	{
		return   -EINVAL;
	}
	i->type = V4L2_INPUT_TYPE_CAMERA;
	strcpy(i->name, KBUILD_MODNAME);
	i->std = V4L2_STD_NTSC_M;
	i->capabilities = 0;
	i->status=0;
	
	return 0;
}

static int hws_vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	//struct hws_video *videodev = video_drvdata(file);
	int Index;
	Index = *i;
	//printk( "%s(%d)-index =%d\n", __func__,videodev->index,Index);
	
	#if 0
	if(Index <0 ||Index >=V4L2_MODEL_VIDEOFORMAT_NUM)
	{
		return   -EINVAL;
	}
	else
	{
		*i = Index;
	}
	#else
	if(Index >0)
	{
		return   -EINVAL;
	}
	else
	{
		*i = 0;
	}	
	#endif 
	return 0;
}

static int hws_vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	#if 0
	struct hws_video *videodev = video_drvdata(file);

	int Index;
	v4l2_model_timing_t *p_SupportmodeTiming;
	Index = i;
	if(Index <0 ||Index >=V4L2_MODEL_VIDEOFORMAT_NUM)
	{
		return   -EINVAL;
	}
	p_SupportmodeTiming = v4l2_model_get_support_videoformat(Index);
	videodev->current_out_size_index = Index;
	videodev->current_out_width = p_SupportmodeTiming->frame_size.width;
	videodev->curren_out_height = p_SupportmodeTiming->frame_size.height;
	printk( "%s(%d)- %dx%d \n", __func__,i,videodev->current_out_width,videodev->curren_out_height);
	#endif 
	//printk( "%s(%d)\n", __func__,i);
	return i ? -EINVAL : 0;
}
static int vidioc_log_status(struct file *file, void *priv)
{
	//printk( "%s()\n", __func__);
	return 0;
}

static ssize_t hws_read(struct file *file,char *buf,size_t count, loff_t *ppos)
{
	//printk( "%s()\n", __func__);
	return -1;
		
}

static int hws_open(struct file *file)
{
	struct hws_video *videodev = video_drvdata(file);
	//v4l2_model_timing_t *p_SupportmodeTiming;
	unsigned long flags;
	struct hws_pcie_dev *pdx = videodev->dev;
	//printk( "%s(ch-%d)->%d\n", __func__,videodev->index,videodev->fileindex);
	spin_lock_irqsave(&pdx->videoslock[videodev->index], flags); 
	videodev->fileindex ++;
	spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
	//printk( "%s(ch-%d)END ->%d W=%d H=%d \n", __func__,videodev->index,videodev->fileindex,videodev->current_out_width,videodev->curren_out_height);
	return 0;
		
}
static int hws_release(struct file *file)
{
	struct hws_video *videodev = video_drvdata(file);
	unsigned long flags;
	struct hws_pcie_dev *pdx = videodev->dev;
	//printk( "%s(ch-%d)->%d\n", __func__,videodev->index,videodev->fileindex);
	spin_lock_irqsave(&pdx->videoslock[videodev->index], flags); 
	if(videodev->fileindex>0)
	{
		videodev->fileindex --;
	}
	spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
	//printk( "%s(ch-%d)->%d done\n", __func__,videodev->index,videodev->fileindex);
	
	if(videodev->fileindex==0)
	{
		if(videodev->startstreamIndex >0)
		{
			//printk( "StopVideoCapture %s(%d)->%d [%d]\n", __func__,videodev->index,videodev->fileindex,videodev->startstreamIndex);
			StopVideoCapture(videodev->dev,videodev->index);
			videodev->startstreamIndex =0;
		}
		return(vb2_fop_release(file));
	}
	else
	{
		return 0;
	}
		
}

//-------------------
static const struct v4l2_queryctrl g_no_ctrl = {
	.name  = "42",
	.flags = V4L2_CTRL_FLAG_DISABLED,
};
static struct v4l2_queryctrl g_hws_ctrls[] = 
{
	#if 1
	{
		V4L2_CID_BRIGHTNESS,           //id
		V4L2_CTRL_TYPE_INTEGER,        //type
		"Brightness",                  //name[32]
		MIN_VAMP_BRIGHTNESS_UNITS,     //minimum
		MAX_VAMP_BRIGHTNESS_UNITS,     //maximum
        1,                             //step
		BrightnessDefault,             //default_value
		0,                             //flags
	    { 0, 0 },                      //reserved[2]
	},
	{
		V4L2_CID_CONTRAST,             //id
		V4L2_CTRL_TYPE_INTEGER,        //type
		"Contrast",                    //name[32]
		MIN_VAMP_CONTRAST_UNITS,       //minimum
		MAX_VAMP_CONTRAST_UNITS,       //maximum
        1,                             //step
		ContrastDefault,               //default_value
		0,                             //flags
	    { 0, 0 },                      //reserved[2]
	},
	{
		V4L2_CID_SATURATION,           //id
		V4L2_CTRL_TYPE_INTEGER,        //type
		"Saturation",                  //name[32]
		MIN_VAMP_SATURATION_UNITS,     //minimum
		MAX_VAMP_SATURATION_UNITS,     //maximum
        1,                             //step
		SaturationDefault,             //default_value
		0,                             //flags
	    { 0, 0 },                      //reserved[2]
	},
	{
		V4L2_CID_HUE,                  //id
		V4L2_CTRL_TYPE_INTEGER,        //type
		"Hue",                         //name[32]
		MIN_VAMP_HUE_UNITS,            //minimum
		MAX_VAMP_HUE_UNITS,            //maximum
        1,                             //step
		HueDefault,                    //default_value
		0,                             //flags
	    { 0, 0 },                      //reserved[2]
	},
	#endif
	#if 0
	{
		V4L2_CID_AUTOGAIN,           //id
		V4L2_CTRL_TYPE_INTEGER,        //type
		"Hdcp enable",                 //name[32]
		0,                             //minimum
		1,                             //maximum
		1,                             //step
		0,                             //default_value
		0,                             //flags
		{ 0, 0 },                      //reserved[2]
	},
	{
		V4L2_CID_GAIN,           //id
		V4L2_CTRL_TYPE_INTEGER,        //type
		"Sample rate",                        //name[32]
		48000,                             //minimum
		48000,                             //maximum
		1,                             //step
		48000,                             //default_value
		0,                             //flags
		{ 0, 0 },                      //reserved[2]
	}
	#endif
};

#define ARRAY_SIZE_OF_CTRL		(sizeof(g_hws_ctrls)/sizeof(g_hws_ctrls[0]))

static struct v4l2_queryctrl *find_ctrlByIndex(unsigned int index)
{
	//scan supported queryctrl table
	if(index>=ARRAY_SIZE_OF_CTRL)
	{
		return NULL;
	}
	else
	{
		return &g_hws_ctrls[index];
	}
}

static struct v4l2_queryctrl *find_ctrl(unsigned int id)
{
	int i;
	//scan supported queryctrl table
	for( i=0; i<ARRAY_SIZE_OF_CTRL; i++ )
		if(g_hws_ctrls[i].id==id)
			return &g_hws_ctrls[i];

	return 0;
}
#if 0
static unsigned int find_Next_Ctl_ID(unsigned int id)
{
	int i;
	int nextID =-1;
	int curr_index =-1;
	//scan supported queryctrl table
	for( i=0; i<ARRAY_SIZE_OF_CTRL; i++ )
	{
		if(g_hws_ctrls[i].id==id)
		{
			curr_index = i;
			break;
		}
	}
	if(curr_index != -1)
	{
		if((curr_index +1)<ARRAY_SIZE_OF_CTRL)
		{
			nextID = g_hws_ctrls[curr_index +1].id;
		}
	}
	return nextID;
}
#endif 
int hws_vidioc_g_ctrl(struct file *file, void *fh,struct v4l2_control *a)//
{
	struct hws_video *videodev = video_drvdata(file);
	struct v4l2_control *ctrl = a;
	//struct v4l2_queryctrl *found_ctrl = find_ctrl(ctrl->id);
	int ret = -EINVAL;
	//int bchs_select=0;
	if(ctrl ==NULL)
	{
		printk( "%s(ch-%d)ctrl=NULL\n", __func__,videodev->index);
		return ret;
	}
	//printk( "%s(ch-%d)\n", __func__,videodev->index);
	switch( ctrl->id ) {
		case V4L2_CID_BRIGHTNESS: //0x00980900
			
		    //bchs_select = V4L2_BCHS_TYPE_BRIGHTNESS;
		    //adv7619_get_bchs(v4l2m_context->adv7619_handle,&BCHSinfo,bchs_select);
            ctrl->value = videodev->m_Curr_Brightness;
            //printk("%s...brightness(%d)\n",__func__,ctrl->value);
			ret = 0;
			break;

		case V4L2_CID_CONTRAST:
	
			//bchs_select = V4L2_BCHS_TYPE_CONTRAST;
		    //printk("%s...contrast(%d)\n",__func__,bchs_select);
            ctrl->value = videodev->m_Curr_Contrast;
			ret = 0;
			break;

		case V4L2_CID_SATURATION:
			
			//bchs_select = V4L2_BCHS_TYPE_SATURATION;
		    //printk("%s...saturation(%d)\n",__func__,bchs_select);
            ctrl->value = videodev->m_Curr_Saturation;
			ret = 0;
			break;

		case V4L2_CID_HUE:
		
			//bchs_select = V4L2_BCHS_TYPE_HUE;
		    //printk("%s...hue(%d)\n",__func__,bchs_select);
            ctrl->value = videodev->m_Curr_Hue;
			ret = 0;
			break; // 			
		default:
		    ctrl->value =0;
			printk("control id %d not handled\n", ctrl->id);
		    break;	
		
	}
	//printk("%s...ctrl->value(%d)=%x\n",__func__,bchs_select,ctrl->value);
	return ret;

}

int hws_vidioc_s_ctrl(struct file *file, void *fh,struct v4l2_control *a)
{
	struct hws_video *videodev = video_drvdata(file);
	struct v4l2_control *ctrl = a;
	struct v4l2_queryctrl *found_ctrl;
	int ret = -EINVAL;
	if(ctrl ==NULL)
	{
		printk( "%s(ch-%d)ctrl=NULL\n", __func__,videodev->index);
		return ret;
	}
	//printk( "%s(ch-%d ctrl->id =%X )\n", __func__,videodev->index,ctrl->id);
	found_ctrl = find_ctrl(ctrl->id);
	if( found_ctrl ) {
		switch( found_ctrl->type ) {
		case V4L2_CTRL_TYPE_INTEGER:
			if( ctrl->value >= found_ctrl->minimum 
				|| ctrl->value <= found_ctrl->maximum ) {
					//printk( "%s(ch-%d ctrl->value =%X )\n", __func__,videodev->index,ctrl->value);
					switch( ctrl->id ) {
	                case V4L2_CID_BRIGHTNESS:
						videodev->m_Curr_Brightness = ctrl->value;               
		            break;
	                case V4L2_CID_CONTRAST:
						videodev->m_Curr_Contrast = ctrl->value;					
		            break;
	                case V4L2_CID_HUE:
						videodev->m_Curr_Hue = ctrl->value;                    
		            break;
	                case V4L2_CID_SATURATION:
						videodev->m_Curr_Saturation = ctrl->value;
                    break;

	                default:
		            break;
	            }
				//printk( "%s(Name:%s value =%X )\n", __func__,found_ctrl->name,ctrl->value);
				ret = 0;
			} 
			else 
			{
				//error
				ret = -ERANGE;
				printk("control %s out of range\n", found_ctrl->name);
			}
			break;
		default:
		{
			//error
			printk("control type %d not handled\n", found_ctrl->type);
			}
			
		}
	}
	//printk( "%s(ret=%d)\n", __func__,ret);
	return ret;

}
void mem_model_memset(void *s,int c,unsigned int n)
{
    memset(s,c,n);
}
static int hws_vidioc_queryctrl(struct file *file, void *fh,struct v4l2_queryctrl *a)
{
	struct hws_video *videodev = video_drvdata(file);
	struct v4l2_queryctrl *found_ctrl;
	unsigned int id;
	unsigned int mask_id;
	int ret = -EINVAL;
	//printk( "%s(ch-%d)\n", __func__,videodev->index);
	//printk( "%s(ctrl-id=0x%X[0x%X] )\n", __func__,a->id,(a->id)&(~V4L2_CTRL_FLAG_NEXT_CTRL));
	//-----------------------------------------------
	id = a->id&(~V4L2_CTRL_FLAG_NEXT_CTRL);
	mask_id =  a->id&V4L2_CTRL_FLAG_NEXT_CTRL;
	if(mask_id == V4L2_CTRL_FLAG_NEXT_CTRL)
	{
		if(id ==0)
		{
			 videodev->queryIndex =0;
			 found_ctrl = find_ctrlByIndex(videodev->queryIndex);
			 *a = *found_ctrl;
			 //a->id = a->id|V4L2_CTRL_FLAG_NEXT_CTRL;
			 //printk("queryctrl[1] Get [%s][0x%X]\n",found_ctrl->name,a->id);
			 ret = 0;
		}
		else
		{
				videodev->queryIndex++; 
				found_ctrl = find_ctrlByIndex(videodev->queryIndex);
				 if(found_ctrl!= NULL)
				 {
					*a = *found_ctrl;
					//a->id = a->id|V4L2_CTRL_FLAG_NEXT_CTRL;
					 //printk("queryctrl[2] Get [%s][0x%X]\n",found_ctrl->name,a->id);
					ret = 0;
				 }
				 else
				 {
					*a	= g_no_ctrl;
					ret = -EINVAL;
				 }

		}

		
	}
	else
	{
		found_ctrl = find_ctrl(id);
		if(NULL != found_ctrl)
		{
				*a = *found_ctrl;
				 //printk("queryctrl[3] Get [%s][0x%X]\n",found_ctrl->name,a->id);
				ret = 0;
		}
		else
		{
				*a	= g_no_ctrl;
				ret = -EINVAL;
		}


	}
	return ret;

}
#if 0
static int hws_vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	//struct hws_video *videodev = video_drvdata(file);
	//printk( "%s(ch-%d)\n", __func__,videodev->index); 
	#if 0
	StartVideoCapture(videodev->dev,videodev->index);
	#endif 
	return(vb2_ioctl_streamon(file,priv,i));
	
}
static int hws_vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	//struct hws_video *videodev = video_drvdata(file);
	//printk( "%s(ch-%d)\n", __func__,videodev->index); 
	#if 0
	StopVideoCapture(videodev->dev,videodev->index);
	#endif 
	return(vb2_ioctl_streamoff(file,priv,i));

}
#endif 
static int hws_vidioc_enum_frameintervals(struct file *file, void *fh,
                   struct v4l2_frmivalenum *fival)
{
	//struct hws_video *videodev = video_drvdata(file);
	int Index;
	int FrameRate;
	v4l2_model_timing_t * pModeTiming;
	Index = fival->index;
	//printk( "%s(CH-%d) FrameIndex =%d \n", __func__,videodev->index,Index);
	if(Index <0 ||Index >=NUM_FRAMERATE_CONTROLS)
	{
		return   -EINVAL;
	}

	FrameRate = v4l2_model_get_support_framerate(Index);
	if(FrameRate == -1) return -EINVAL;
	pModeTiming = Get_input_framesizeIndex(fival->width,fival->height);
	if(pModeTiming == NULL) return -EINVAL;
	
    fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator=1000 ;
	fival->discrete.denominator=FrameRate*1000;
	//printk( "%s FrameIndex=%d W=%d H=%d  FrameRate=%d \n", __func__,Index,fival->width,fival->height,FrameRate);
    return 0;
}
int hws_vidioc_s_parm(struct file *file, void *fh,struct v4l2_streamparm *a)
{
	struct hws_video *videodev = video_drvdata(file);
	int io_frame_rate;
	int in_frame_rate;
	//int io_widht;
	//int io_hight;
	//int io_index; 
	v4l2_model_timing_t *p_SupportmodeTiming;
	io_frame_rate = a->parm.capture.timeperframe.denominator/a->parm.capture.timeperframe.numerator;
	//io_index = a->parm.capture.timeperframe.index;
	//printk( "%s(CH-%d) io_index =%d \n", __func__,videodev->index,io_index);
	p_SupportmodeTiming = v4l2_model_get_support_videoformat(videodev->current_out_size_index);
	if(p_SupportmodeTiming ==NULL) return -EINVAL;
	in_frame_rate = p_SupportmodeTiming->refresh_rate;
	//printk( "%s(ch-%d)io_frame_rate =%d  in_frame_rate =%d \n", __func__,videodev->index,io_frame_rate,in_frame_rate);
	return 0;
}
#if 0
static int hws_vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
   struct hws_video *videodev = video_drvdata(file);
   printk( "%s(ch-%d)\n", __func__,videodev->index);
	//vb2_ioctl_dqbuf
    return vb2_ioctl_dqbuf(file,priv,p);
}
#endif 



//----------------------------
static const struct v4l2_file_operations hws_fops = {
	.owner		= THIS_MODULE,
	.open		= hws_open,//v4l2_fh_open,
	.release	= hws_release,//vb2_fop_release,
	.read		= hws_read,//vb2_fop_read,
	.poll		= vb2_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap           = vb2_fop_mmap,
};


static const struct v4l2_ioctl_ops hws_ioctl_fops = {
	.vidioc_querycap = hws_vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = hws_vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap = hws_vidioc_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap = hws_vidioc_try_fmt_vid_cap,
	.vidioc_reqbufs       = vb2_ioctl_reqbufs,
	.vidioc_prepare_buf   = vb2_ioctl_prepare_buf,
	.vidioc_create_bufs   = vb2_ioctl_create_bufs,
	.vidioc_querybuf      = vb2_ioctl_querybuf,
	.vidioc_qbuf          = vb2_ioctl_qbuf,
	.vidioc_dqbuf         = vb2_ioctl_dqbuf,
	.vidioc_streamon      = vb2_ioctl_streamon,
	.vidioc_streamoff     = vb2_ioctl_streamoff,
	.vidioc_g_std = hws_vidioc_g_std,
	.vidioc_s_std = hws_vidioc_s_std,
	.vidioc_enum_framesizes   	= hws_vidioc_enum_framesizes,
	.vidioc_enum_frameintervals = hws_vidioc_enum_frameintervals,
	.vidioc_g_ctrl        		= hws_vidioc_g_ctrl,
	.vidioc_s_ctrl        		= hws_vidioc_s_ctrl,
	.vidioc_queryctrl           = hws_vidioc_queryctrl,
	.vidioc_enum_input = hws_vidioc_enum_input,
	.vidioc_g_input = hws_vidioc_g_input,
	.vidioc_s_input = hws_vidioc_s_input,
	//.vidioc_log_status = vidioc_log_status,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
	.vidioc_g_parm = hws_vidioc_g_parm,
	.vidioc_s_parm = hws_vidioc_s_parm,
};

static int hws_queue_setup(struct vb2_queue *q,
			   unsigned int *num_buffers, unsigned int *num_planes,
			   unsigned int sizes[], struct device *alloc_devs[])
{
	struct hws_video *videodev = q->drv_priv;
	struct hws_pcie_dev *pdx = videodev->dev;
	unsigned long flags;
	unsigned size;
	spin_lock_irqsave(&pdx->videoslock[videodev->index], flags);	
	size = 2* videodev->current_out_width * videodev->curren_out_height; // 16bit
	//printk( "%s(%d)->%d[%d?=%d]\n", __func__,videodev->index,videodev->fileindex,sizes[0],size);
	if(videodev->fileindex >1)
	{
		spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
		return -EINVAL;
	}
	//printk( "q->num_buffers = %d *num_buffers =%d \n", q->num_buffers,*num_buffers);
	//if (tot_bufs < 2)
	//	tot_bufs = 2;
	//tot_bufs = hws_buffer_count(size, tot_bufs);
	//*num_buffers = tot_bufs - q->num_buffers;
	if (*num_planes)
	{
		if(sizes[0] < size)
		{
			spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
			return -EINVAL;
		}
		else
		{
			spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
			return  0;
		}
	}
	//printk( "%s()  num_buffers:%x tot_bufs:%x\n", __func__,*num_buffers,tot_bufs);
	//printk( "%s()  sizes[0]= %d size= %d\n", __func__,sizes[0],size);
	*num_planes = 1;
	sizes[0] = size; 
	spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
	return 0;
}

static int hws_buffer_prepare(struct vb2_buffer *vb)
{

	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct hwsvideo_buffer *buf =
		container_of(vbuf, struct hwsvideo_buffer, vb);
	struct hws_video *videodev = vb->vb2_queue->drv_priv;
	struct hws_pcie_dev *pdx = videodev->dev;
	u32 size;
	unsigned long flags;
	//printk( "%s(W = %d H=%d)\n", __func__,videodev->current_out_width,videodev->curren_out_height);
	spin_lock_irqsave(&pdx->videoslock[videodev->index], flags);	
	size = 2* videodev->current_out_width * videodev->curren_out_height; // 16bit
	if (vb2_plane_size(vb, 0) < size)
	{
		spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
		return -EINVAL;
	}
	vb2_set_plane_payload(vb, 0, size);
	buf->mem = vb2_plane_vaddr(vb,0);
	spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
	return 0;	
}

static void hws_buffer_finish(struct vb2_buffer *vb)
{
	
	//struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	//struct hwsvideo_buffer *buf =
	//	container_of(vbuf, struct hwsvideo_buffer, vb);
	//struct hws_video *videodev = vb->vb2_queue->drv_priv;
	//printk( "%s()\n", __func__);
	return;
}

static void hws_buffer_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct hws_video *videodev = vb->vb2_queue->drv_priv;
	struct hwsvideo_buffer *buf =
		container_of(vbuf, struct hwsvideo_buffer, vb);
	unsigned long flags;
	struct hws_pcie_dev *pdx = videodev->dev;
	
	//printk( "%s(%d)\n", __func__,videodev->index);
	spin_lock_irqsave(&pdx->videoslock[videodev->index], flags);	
	list_add_tail(&buf->queue, &videodev->queue);
	spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
}

static int hws_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct hws_video *videodev = q->drv_priv;
	unsigned long flags;
	struct hws_pcie_dev *pdx = videodev->dev;
	//printk( "%s(%d)->%d\n", __func__,videodev->index,videodev->fileindex);
	#if 0
	if(videodev->fileindex >1)
	{
		return -EINVAL;
	}
	#endif 
	videodev->seqnr = 0;
	mdelay(100);
	//---------------
	//if(videodev->fileindex==1)
	//{
		//printk( "StartVideoCapture %s(%d)->%d\n", __func__,videodev->index,videodev->fileindex);
		StartVideoCapture(videodev->dev,videodev->index);
		videodev->startstreamIndex++;
		//------------------------ reset queue
   	   //printk( "%s(%d)->%d  reset queue \n", __func__,videodev->index,videodev->fileindex);
		
	//}
	spin_lock_irqsave(&pdx->videoslock[videodev->index], flags); 
		while (!list_empty(&videodev->queue)) {
			struct hwsvideo_buffer *buf = list_entry(videodev->queue.next,
				struct hwsvideo_buffer, queue);
			list_del(&buf->queue);
			
			vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
		}
	spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
	//-----------------------
	return 0;
}

static void hws_stop_streaming(struct vb2_queue *q)
{
	struct hws_video *videodev = q->drv_priv;
	unsigned long flags;
	struct hws_pcie_dev *pdx = videodev->dev;
	//printk( "%s(%d)->%d\n", __func__,videodev->index,videodev->fileindex);
	
	//if(videodev->seqnr){
		//vb2_wait_for_all_buffers(q);		
	//	mdelay(100);
		//printk( "%s() vb2_wait_for_all_buffers\n", __func__);
	//}
	#if 1
	//-----------------------------------
	videodev->startstreamIndex --;
	if(videodev->startstreamIndex<0) videodev->startstreamIndex=0;
	if(videodev->startstreamIndex == 0)
	{
		//printk( "StopVideoCapture %s(%d)->%d [%d]\n", __func__,videodev->index,videodev->fileindex,videodev->startstreamIndex);
		StopVideoCapture(videodev->dev,videodev->index);
	}
	//------------------
	spin_lock_irqsave(&pdx->videoslock[videodev->index], flags);	
	while (!list_empty(&videodev->queue)) {
		struct hwsvideo_buffer *buf = list_entry(videodev->queue.next,
			struct hwsvideo_buffer, queue);
		list_del(&buf->queue);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
	//-----------------------------------------------------------------
	#endif 
	
}

static const struct vb2_ops hwspcie_video_qops = {
	.queue_setup    = hws_queue_setup,
	.buf_prepare  = hws_buffer_prepare,
	.buf_finish = hws_buffer_finish,
	.buf_queue    = hws_buffer_queue,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.start_streaming = hws_start_streaming,
	.stop_streaming = hws_stop_streaming,
};
//-----------------------------------------
const unsigned char  g_YUVColors [MAX_COLOR][3] = {
    {128, 16, 128},     // BLACK
    {128, 235 , 128},    // WHITE
    {16, 211, 146},     // YELLOW
    {166, 170, 16},     // CYAN
    {54, 145, 34},      // GREEN
    {202, 106, 222},    // MAGENTA
    {90, 81, 240},      // RED
    {240, 41, 109},     // BLUE
    {128, 125, 128},    // GREY
};
	

static void SetNoVideoMem(uint8_t * pDest, int  w,int h)
{
int x,y;
uint8_t *pST;
uint8_t *pNS;
pST = (uint8_t *)pDest;
//printk("SetNoVideoMem[%d-%d]\n",w,h);

for(x=0;x<w/2;x++)
{
	pST[0] = 41;
	pST[1] = 240;
	pST[2] = 41;
	pST[3] = 109;
	pST +=4;
}

pNS = pDest+w*2;
for(y=1;y<h;y++)
{
	memcpy(pNS,pDest,w*2);
	pNS = pNS+w*2;
}
}
//---------------------------------------------------
static int _deliver_samples(struct hws_audio *drv, void *aud_data, u32 aud_len)
{
    struct snd_pcm_substream *substream = drv->substream;
    int elapsed = 0;
    unsigned int frames = aud_len / (2 * drv->channels);
    int copy_bytes = 0;
    unsigned long flags;

    if (frames == 0)
        return 0;

    if (drv->ring_wpos_byframes + frames > drv->ring_size_byframes) {
        copy_bytes = (drv->ring_size_byframes - drv->ring_wpos_byframes) * 2 * drv->channels;
        memcpy(substream->runtime->dma_area + drv->ring_wpos_byframes * 2 * drv->channels,
                aud_data, copy_bytes);
        memcpy(substream->runtime->dma_area, aud_data + copy_bytes, aud_len - copy_bytes);
    } else {
        memcpy(substream->runtime->dma_area + drv->ring_wpos_byframes * 2 * drv->channels,
                aud_data, aud_len);
    }

    spin_lock_irqsave(&drv->ring_lock, flags);
    drv->ring_wpos_byframes += frames;
    drv->ring_wpos_byframes %= drv->ring_size_byframes;
    /* check if a period available */
    elapsed = (drv->period_used_byframes + frames) / drv->period_size_byframes;
    drv->period_used_byframes += frames;
    drv->period_used_byframes %= drv->period_size_byframes;
    spin_unlock_irqrestore(&drv->ring_lock, flags);

    if (elapsed && substream) {
        /* snd_pcm_period_elapsed will call SNDRV_PCM_TRIGGER_STOP in somecase */
        snd_pcm_period_elapsed(drv->substream);
    }

    return frames * 2 * drv->channels;
}

void audio_data_process(struct work_struct *p_work)
{
	struct hws_audio *drv = container_of(p_work, struct hws_audio, audiowork);
	//struct snd_pcm_substream *substream = drv->substream;
	struct hws_pcie_dev *pdx = drv->dev;
	int nIndex;
	int i;
	int delived = 0;
	//unsigned int frames;
	BYTE *bBuf = NULL;
	//int copysize =0;
	//int rev_size=0;
	int dwAudioCh;
	int aud_len;
	unsigned long flags;
	//int avail = 0;
	dwAudioCh = drv->index;
	nIndex = -1;
	//printk("audio_data_process %d\n",dwAudioCh);
	spin_lock_irqsave(&pdx->audiolock[dwAudioCh], flags);
	for( i = pdx->m_nRDAudioIndex[dwAudioCh];i<MAX_AUDIO_QUEUE;i++)
	{
		if(pdx->m_AudioInfo[dwAudioCh].pStatusInfo[i].byLock== MEM_LOCK)
		{
			nIndex =i;
			bBuf = pdx->m_AudioInfo[dwAudioCh].m_pAudioBufData[i];
			aud_len = pdx->m_AudioInfo[dwAudioCh].pStatusInfo[i].dwLength;
			break;
		}
	}
	if(nIndex == -1)
	{
		for( i = 0 ;i<pdx->m_nRDAudioIndex[dwAudioCh];i++)
		{
				if(pdx->m_AudioInfo[dwAudioCh].pStatusInfo[i].byLock== MEM_LOCK)
				{
						nIndex =i;
						bBuf = pdx->m_AudioInfo[dwAudioCh].m_pAudioBufData[i];
						aud_len = pdx->m_AudioInfo[dwAudioCh].pStatusInfo[i].dwLength;
						break;
				}
				
		}
	}
	if((nIndex != -1)&&(bBuf))
	{
		//--- send data -------------
		 delived = _deliver_samples(drv,bBuf, aud_len);
		#if 0
		 avail = aud_len - delived;
		 if(avail)
		 {
		 	printk("Sample Rate is wrong  =%d \n",avail);
		 }
		#endif 
		//----------------------------------------------
		//spin_lock_irqsave(&pdx->audiolock[dwAudioCh], flags);
		pdx->m_AudioInfo[dwAudioCh].pStatusInfo[nIndex].byLock = MEM_UNLOCK;
		//spin_unlock_irqrestore(&pdx->audiolock[dwAudioCh], flags);
		nIndex ++;
		if(nIndex >=MAX_AUDIO_QUEUE)
		{
			nIndex =0;
		}
		pdx->m_nRDAudioIndex[dwAudioCh] = nIndex;
	}
	spin_unlock_irqrestore(&pdx->audiolock[dwAudioCh], flags);
	
}
//-------------------------------
static void FHD_To_800X600_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int x,y;
   BYTE*pSrcBuf;
//   BYTE*pDestBuf;
   int *pSrcData;
   int *pDestData;
   pSrcBuf = pSrc;
   pDestData = (int *) pOut;
   for(y=0;y<in_h;y++)
   {
	  switch(y%9)
	  {
		case 0:
		case 2:
		case 4:
		case 6:
	    case 8:
		{
		  pSrcBuf = pSrc + (y*in_w*2)+(240*2);
		  for(x=0;x<(in_w-480);)
		  {
			 pSrcData =(int *)pSrcBuf;
			pDestData[0] = pSrcData[0];
			pDestData[1] = pSrcData[2];
			pDestData[2] = pSrcData[4];
			pDestData[3] = pSrcData[6];
			pDestData[4] = pSrcData[8];
			 pSrcBuf +=18*2;
			 pDestData +=5;
			 x= x+18;
		  }
		  break;
	  }
	  case 1:
	  case 3:
	  case 5:
	  case 7:
	  {
			break;
	  }
	  }

   }

}
static void HD_To_800X600_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   //int *pSrcData;
//   int *pDestData;
   pSrcBuf = pSrc;
   pDestBuf = pOut;
   for(y=0;y<(in_h-120);y++)
   {
		memcpy(pDestBuf,(pSrcBuf+60*in_w*2),out_w*2);
		pDestBuf += out_w*2;
		pSrcBuf += in_w*2;
   }

}
static void SD_NTSC_To_SD_PAL_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   //int x,y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   //int *pSrcData;
   //int *pDestData;
   DWORD dwI;
   DWORD dwJ;
   BYTE *pDstYUV;
   
   pSrcBuf = pSrc;
   pDestBuf = pOut;
   
  
   // PAL 720X576  NTSC 720X480
   pDstYUV =  pDestBuf;	
   for(dwI =0; dwI <48; dwI++)
   {
		for(dwJ=0; dwJ<out_w; dwJ++)
		{
				pDstYUV[0] = 0x10;
				pDstYUV[1] = 0x80;
				pDstYUV +=2;
		}
	}
   pDestBuf += 48*in_w*2;
   memcpy(pDestBuf,pSrcBuf,in_h*in_w*2);
   pDestBuf +=in_h*in_w*2; 
   pDstYUV = pDestBuf;
  for(dwI =0; dwI <48; dwI++)
   {
		for(dwJ=0; dwJ<out_w; dwJ++)
		{
				pDstYUV[0] = 0x10;
				pDstYUV[1] = 0x80;
				pDstYUV +=2;
		}
	} 

}

static void SD_PAL_To_SD_NTSC_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
  // int x,y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
//   int *pSrcData;
   //int *pDestData;
   pSrcBuf = pSrc;
   pDestBuf = pOut;
   // PAL 720X576  NTSC 720X480
   memcpy(pDestBuf,(pSrcBuf+48*in_w*2),out_h*out_w*2);
   
}
static void SD_NTSC_To_FHD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int x,y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   int dwJ;
   BYTE*pDstYUV;
   
   pSrcBuf = pSrc;
   pDestBuf = pOut;
   
   
   // PAL 720X480   x*2 =1440,(y-36)*2= 1080
  
   //--- 60 line 
   for(y=0;y<60;y++)
   {
		pDstYUV = pDestBuf;
		for(dwJ=0; dwJ<out_w; dwJ++)
		{
				pDstYUV[0] = 0x10;
				pDstYUV[1] = 0x80;
				pDstYUV +=2;
		}
		pDestBuf +=out_w*2;
   }
   //---
    for(y=0;y<(out_h-120);)
   {
		pDstYUV = pDestBuf;
		for(dwJ=0; dwJ<240; dwJ++)
		{
				pDstYUV[0] = 0x10;
				pDstYUV[1] = 0x80;
				pDstYUV +=2;
		}
		pDestBuf +=240*2;
		for(x=0;x<720;)
		{

				pDestBuf[0] = pSrcBuf[0];//y
				pDestBuf[1] = pSrcBuf[1];//cb
						
				pDestBuf[2] = pSrcBuf[0];//y
				pDestBuf[3] = pSrcBuf[3];//cr

				pDestBuf[4] = pSrcBuf[2];//y
				pDestBuf[5] = pSrcBuf[1];//cb
						
				pDestBuf[6] = pSrcBuf[2];//y
				pDestBuf[7] = pSrcBuf[3];//cr

				pSrcBuf +=4;
				pDestBuf +=8;
				x= x+2;
		}
		pDstYUV = pDestBuf;
		for(dwJ=0; dwJ<240; dwJ++)
		{
				pDstYUV[0] = 0x10;
				pDstYUV[1] = 0x80;
				pDstYUV +=2;
		}
		pDestBuf +=240*2;
		//------------copy line 
		memcpy(pDestBuf,pDestBuf-(out_w*2),out_w*2);
		pDestBuf += out_w*2;
		y = y+2;
		//------------------
   }
   //-- 60 line 
   	for(y=0;y<60;y++)
  	 {
		pDstYUV = pDestBuf;
		for(dwJ=0; dwJ<out_w; dwJ++)
		{
				pDstYUV[0] = 0x10;
				pDstYUV[1] = 0x80;
				pDstYUV +=2;
		}
		pDestBuf +=out_w*2;
		
 	}
	//---
}

static void SD_PAL_To_FHD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int x,y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   int dwJ;
   BYTE*pDstYUV;
      pSrcBuf = pSrc;
   pDestBuf = pOut;
   // PAL 720X576   x*2 =1440,(y-36)*2= 1080
   pSrcBuf = pSrcBuf+18*in_w*2;
   for(y=0;y<out_h;)
   {
		pDstYUV = pDestBuf;
		for(dwJ=0; dwJ<240; dwJ++)
		{
				pDstYUV[0] = 0x10;
				pDstYUV[1] = 0x80;
				pDstYUV +=2;
		}
		pDestBuf +=240*2;
		for(x=0;x<720;)
		{

				pDestBuf[0] = pSrcBuf[0];//y
				pDestBuf[1] = pSrcBuf[1];//cb
						
				pDestBuf[2] = pSrcBuf[0];//y
				pDestBuf[3] = pSrcBuf[3];//cr

				pDestBuf[4] = pSrcBuf[2];//y
				pDestBuf[5] = pSrcBuf[1];//cb
						
				pDestBuf[6] = pSrcBuf[2];//y
				pDestBuf[7] = pSrcBuf[3];//cr

				pSrcBuf +=4;
				pDestBuf +=8;
				x= x+2;
		}
		pDstYUV = pDestBuf;
		for(dwJ=0; dwJ<240; dwJ++)
		{
				pDstYUV[0] = 0x10;
				pDstYUV[1] = 0x80;
				pDstYUV +=2;
		}
		pDestBuf +=240*2;
		//------------copy line 
		memcpy(pDestBuf,pDestBuf-(out_w*2),out_w*2);
		pDestBuf += out_w*2;
		y = y+2;
		//------------------
   }
   
   
}

static void SD_PAL_To_HD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
  int x,y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
  // int dwJ;
//   BYTE*pDstYUV;
   pSrcBuf = pSrc;
   pDestBuf = pOut;
 
   
   // PAL 720X576   640X360-> 1280*720
   pSrcBuf = pSrcBuf+108*in_w*2;
    for(y=0;y<out_h;)
   {
		pSrcBuf += 40*2;
		for(x=0;x<640;)
		{

				pDestBuf[0] = pSrcBuf[0];//y
				pDestBuf[1] = pSrcBuf[1];//cb
						
				pDestBuf[2] = pSrcBuf[0];//y
				pDestBuf[3] = pSrcBuf[3];//cr

				pDestBuf[4] = pSrcBuf[2];//y
				pDestBuf[5] = pSrcBuf[1];//cb
						
				pDestBuf[6] = pSrcBuf[2];//y
				pDestBuf[7] = pSrcBuf[3];//cr

				pSrcBuf +=4;
				pDestBuf +=8;
				x= x+2;
		}
		pSrcBuf += 40*2;
		//------------copy line 
		memcpy(pDestBuf,pDestBuf-(out_w*2),out_w*2);
		pDestBuf += out_w*2;
		y= y+2;
		//------------------
   }
   
   
}
static void V1280X1024_To_FHD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{

		BYTE*pSrcBuf;
		BYTE*pDestBuf;
		pSrcBuf = pSrc;
		pDestBuf = pOut;
		pSrcBuf = pSrcBuf+152*in_w*2;
		HD_To_FHD_Scaler(pSrcBuf,pDestBuf,1280,720,out_w,out_h);
}
void V1280X1024_To_HD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
		BYTE*pSrcBuf;
		BYTE*pDestBuf;
		pSrcBuf = pSrc;
		pDestBuf = pOut;
		pSrcBuf = pSrcBuf+152*in_w*2;
		memcpy(pDestBuf,pSrcBuf,out_h*out_w*2);
			
}
void V1280X1024_To_800X600_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
		BYTE*pSrcBuf;
		BYTE*pDestBuf;
		pSrcBuf = pSrc;
		pDestBuf = pOut;
		pSrcBuf = pSrcBuf+152*in_w*2;
		HD_To_800X600_Scaler(pSrcBuf,pDestBuf,1280,720,out_w,out_h);


}

void SD_NTSC_To_HD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int x,y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   pSrcBuf = pSrc;
   pDestBuf = pOut;
//   int dwJ;
   //BYTE*pDstYUV;
   // PAL 720X480   640X360-> 1280*720
   pSrcBuf = pSrcBuf+60*in_w*2;
    for(y=0;y<out_h;)
   {
		pSrcBuf += 40*2;
		for(x=0;x<640;)
		{

				pDestBuf[0] = pSrcBuf[0];//y
				pDestBuf[1] = pSrcBuf[1];//cb
						
				pDestBuf[2] = pSrcBuf[0];//y
				pDestBuf[3] = pSrcBuf[3];//cr

				pDestBuf[4] = pSrcBuf[2];//y
				pDestBuf[5] = pSrcBuf[1];//cb
						
				pDestBuf[6] = pSrcBuf[2];//y
				pDestBuf[7] = pSrcBuf[3];//cr

				pSrcBuf +=4;
				pDestBuf +=8;
				x= x+2;
		}
		pSrcBuf += 40*2;
		//------------copy line 
		memcpy(pDestBuf,pDestBuf-(out_w*2),out_w*2);
		pDestBuf += out_w*2;
		y= y+2;
		//------------------
   }
   
   
}

void FHD_To_HD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)

{
   int x,y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   int *pSrcData;
   int *pDestData;
   pSrcBuf = pSrc;
   pDestData = (int *) pOut;
   for(y=0;y<in_h;y++)
   {
	  if(y%3 != 2)
	  {
		  for(x=0;x<in_w;)
		  {
			pSrcData =(int *)pSrcBuf;
			*pDestData = *pSrcData;
			 if(x%2==1)
			 {
				pDestBuf =(BYTE *)pDestData;
				pDestBuf[1] = pSrcBuf[3];
				pDestBuf [3] = pSrcBuf[1];
			 }
			pDestData +=1;
			pSrcBuf +=6;
			x= x+3;
		  }
	  }
	  else
	  {
		pSrcBuf += in_w*2;	
	  }

   }

}
void HD_To_FHD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
	   
	   	 int x,y;
		 BYTE*pSrcBuf;
		 //BYTE*pDestBuf;
		 DWORD *pSrcData;
		 DWORD *pDestData;
		 int    out_w_size;
		 //BYTE*pSrcTmp;
		 BYTE*pDestTmp;
		 out_w_size = out_w*2;
		 pDestData = (DWORD *) pOut;
		 pSrcData = (DWORD *) pSrc;
		 for(y=0;y<out_h;y++)
		 {
			
				if((y%3)==2)
				{
					pSrcBuf = (BYTE*)pDestData;
					memcpy(pSrcBuf,(BYTE*)(pSrcBuf-out_w_size),out_w_size);
					pDestData += out_w_size/4;
				}
				else
				{
				
					for(x=0;x<out_w;)
					{
						*pDestData = *pSrcData;
						 pDestData ++;
						 pSrcData++;
						 
						*pDestData = *pSrcData;
						 pDestTmp =(BYTE*)pDestData;
						 pDestTmp[2] = pDestTmp[0];
						 pDestData ++;
						
						*pDestData = *pSrcData;
						 pDestTmp =(BYTE*)pDestData;
						 pDestTmp[0] = pDestTmp[2];
						 pSrcData++;
						 pDestData++;
						 x= x+6;
					}
			
				}
				
	   
		 }
	   

}
void FHD_To_SD_NTSC_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int x,y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   BYTE *pSrcData;
//   BYTE *pDestData;
   pSrcBuf = pSrc;
   pDestBuf = pOut;
   //-- 1920X1080 0->720X480
   pSrcBuf = pSrcBuf + 60*in_w*2;
   for(y=0;y<out_h;)
   {
		pSrcData = pSrcBuf + 240*2;
		for(x =0;x<720;)
		{
			pDestBuf[0] = pSrcData[0];//y
			pDestBuf[1] = pSrcData[1];//cb

			pDestBuf[2] = pSrcData[4];//y
			pDestBuf[3] = pSrcData[3];//cr

			x = x+ 2;
			pSrcData +=8;
			pDestBuf +=4;
		}
		pSrcBuf += in_w*4;
		y= y+1;
   }
   
}
void FHD_To_SD_PAL_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int x,y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   BYTE *pSrcData;
   BYTE*pDstYUV;
//   BYTE *pDestData;
   int dwJ;
   pSrcBuf = pSrc;
   pDestBuf = pOut;
   
   //-- 1920X1080 0->720X576
   //-------------------
   pDstYUV = pDestBuf;
   for(dwJ=0; dwJ<18*out_w; dwJ++)
	{
			pDstYUV[0] = 0x10;
			pDstYUV[1] = 0x80;
			pDstYUV +=2;
	}
   //------------------
   pDestBuf += 18*out_w*2;
    for(y=0;y<540;)
   {
		pSrcData = pSrcBuf + 240*2;
		for(x =0;x<720;)
		{
			pDestBuf[0] = pSrcData[0];//y
			pDestBuf[1] = pSrcData[1];//cb

			pDestBuf[2] = pSrcData[4];//y
			pDestBuf[3] = pSrcData[3];//cr

			x = x+ 2;
			pSrcData +=8;
			pDestBuf +=4;
		}
		pSrcBuf += in_w*4;
		y= y+1;
   }
   //------------
    pDstYUV = pDestBuf;
   for(dwJ=0; dwJ<18*out_w; dwJ++)
	{
			pDstYUV[0] = 0x10;
			pDstYUV[1] = 0x80;
			pDstYUV +=2;
	}
   //------------
}

void HD_To_SD_NTSC_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   //int *pSrcData;
  // int *pDestData;
   pSrcBuf = pSrc;
   pDestBuf = pOut;
   //-- 1280X720 ->720X480
   pSrcBuf = pSrcBuf + 120*in_w*2;
   for(y=0;y<out_h;)
   {
		memcpy(pDestBuf,pSrcBuf+280*2,out_w*2);
		pSrcBuf += in_w*2;
		pDestBuf += out_w*2;
		y= y+1;
   }
   
}
void HD_To_SD_PAL_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   //int dwJ;
   pSrcBuf = pSrc;
   pDestBuf = pOut;
   //BYTE*pDstYUV;
   //-- 1920X1080 0->720X576
   pSrcBuf = pSrcBuf + 72*in_w*2;
   for(y=0;y<out_h;)
   {
		memcpy(pDestBuf,pSrcBuf+280*2,out_w*2);
		pSrcBuf += in_w*2;
		pDestBuf += out_w*2;
		y= y+1;
   }
}
void V1280X1024_NTSC_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   pSrcBuf = pSrc;
   pDestBuf = pOut;
//   BYTE*pDstYUV;
   //-- 1280X1024 0->720X480
   pSrcBuf = pSrcBuf + 272*in_w*2; 
   for(y=0;y<out_h;)
   {
		memcpy(pDestBuf,pSrcBuf+280*2,out_w*2);
		pSrcBuf += in_w*2;
		pDestBuf += out_w*2;
		y= y+1;
   }
}

void V1280X1024_PAL_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   pSrcBuf = pSrc;
   pDestBuf = pOut;
//   BYTE*pDstYUV;
   //-- 1280x1024 0->720X576
   pSrcBuf = pSrcBuf + 224*in_w*2;
   for(y=0;y<out_h;)
   {
		memcpy(pDestBuf,pSrcBuf+280*2,out_w*2);
		pSrcBuf += in_w*2;
		pDestBuf += out_w*2;
		y= y+1;
   }
}
void All_VideoScaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   BYTE *pDstYUV;
   int  missX=0;
   int  missY=0;
   int  dumyX=0;
   int  dumyY=0;
   int dwJ; 
   int  y;
   pSrcBuf = pSrc;
   pDestBuf = pOut;
   if(in_w>out_w)
   {
		missX = (in_w-out_w)/2;
   }
   else
   {
   		dumyX = (out_w- in_w)/2;

   }
   if(in_h>out_h)
   {
		missY = (in_h-out_h)/2;
   }
   else
   	{
		dumyY = (out_h - in_h)/2;
   }
   if(dumyY>0)
   {
    pDstYUV = pDestBuf;
	for(dwJ=0; dwJ<(dumyY*out_w); dwJ++)
	{
			pDstYUV[0] = 0x10;
			pDstYUV[1] = 0x80;
			pDstYUV +=2;
	}
	pDestBuf = pDestBuf+dumyY*out_w*2;
   }
   if(missY>0)
   {
	  pSrcBuf += missY*in_w*2;
   }
   //----------
   
   for(y=0;y<(out_h-(dumyY*2));y++)
   {
   		if(dumyX>0)
   		{
   			pDstYUV = pDestBuf;
			for(dwJ=0; dwJ<dumyX; dwJ++)
			{
				pDstYUV[0] = 0x10;
				pDstYUV[1] = 0x80;
				pDstYUV +=2;
			}
			pDestBuf += dumyX*2;
		}
		memcpy(pDestBuf,pSrcBuf+missX*2,(out_w-(dumyX*2))*2);
		pSrcBuf += in_w*2;
		pDestBuf += (out_w-(dumyX*2))*2;
		if(dumyX>0)
   		{
			pDstYUV = pDestBuf;
			for(dwJ=0; dwJ<dumyX; dwJ++)
			{
				pDstYUV[0] = 0x10;
				pDstYUV[1] = 0x80;
				pDstYUV +=2;
			}
			pDestBuf += dumyX*2;
		}
   }
   if(dumyY>0)
  {
  	pDstYUV = pDestBuf;
	for(dwJ=0; dwJ<(dumyY*out_w); dwJ++)
	{
			pDstYUV[0] = 0x10;
			pDstYUV[1] = 0x80;
			pDstYUV +=2;
	}

  }

}

static void UHD_To_800X600_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)

{
   int x,y;
   BYTE*pSrcBuf;
//   BYTE*pDestBuf;
   BYTE *pSrcData;
   BYTE *pDestData;
   pSrcBuf = pSrc;
   pDestData = (BYTE *) pOut;
   for(y=0;y<in_h;y++)
   {
	  switch(y%18)
	  {
		case 0:
		case 4:
		case 8:
		case 12:
	    case 16:
		{
		  pSrcBuf = pSrc + (y*in_w*2)+(480*2);
		  for(x=0;x<(in_w-960);)
		  {
			 pSrcData =(BYTE *)pSrcBuf;
			 
			 pDestData[0] = pSrcData[0];
			 pDestData[1] = pSrcData[1];
			 pDestData[2] = pSrcData[8];
			 pDestData[3] = pSrcData[7];

			 pDestData[4] = pSrcData[16];
			 pDestData[5] = pSrcData[17];
			 pDestData[6] = pSrcData[24];
			 pDestData[7] = pSrcData[23];

			 pDestData[8] = pSrcData[32];
			 pDestData[9] = pSrcData[33];
			 pDestData[10] = pSrcData[36];
			 pDestData[11] = pSrcData[35];

			 pDestData[12] = pSrcData[44];
			 pDestData[13] = pSrcData[45];
			 pDestData[14] = pSrcData[52];
			 pDestData[15] = pSrcData[51];

			 pDestData[16] = pSrcData[60];
			 pDestData[17] = pSrcData[61];
			 pDestData[18] = pSrcData[68];
			 pDestData[19] = pSrcData[67];
			 
			 
			 pSrcBuf +=18*4;
			 pDestData +=5*4;
			 x= x+18*2;
		  }
		  break;
	  }
	  default:
	  {
			break;
	  }
	  }

   }

}
static void UHDW_To_800X600_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)

{
   int x,y;
   BYTE*pSrcBuf;
   //BYTE*pDestBuf;
   BYTE *pSrcData;
   BYTE *pDestData;
   pSrcBuf = pSrc;
   pDestData = (BYTE *) pOut;
   for(y=0;y<in_h;y++)
   {

	  switch(y%18)
	  {
		case 0:
		case 4:
		case 8:
		case 12:
	    case 16:
		{
		  pSrcBuf = pSrc + (y*in_w*2)+((480+128)*2);
		  for(x=0;x<(3840-960);)
		  {
			 pSrcData =(BYTE *)pSrcBuf;
			 
			 pDestData[0] = pSrcData[0];
			 pDestData[1] = pSrcData[1];
			 pDestData[2] = pSrcData[8];
			 pDestData[3] = pSrcData[7];

			 pDestData[4] = pSrcData[16];
			 pDestData[5] = pSrcData[17];
			 pDestData[6] = pSrcData[24];
			 pDestData[7] = pSrcData[23];

			 pDestData[8] = pSrcData[32];
			 pDestData[9] = pSrcData[33];
			 pDestData[10] = pSrcData[36];
			 pDestData[11] = pSrcData[35];

			 pDestData[12] = pSrcData[44];
			 pDestData[13] = pSrcData[45];
			 pDestData[14] = pSrcData[52];
			 pDestData[15] = pSrcData[51];

			 pDestData[16] = pSrcData[60];
			 pDestData[17] = pSrcData[61];
			 pDestData[18] = pSrcData[68];
			 pDestData[19] = pSrcData[67];
			 
			 
			 pSrcBuf +=18*4;
			 pDestData +=5*4;
			 x= x+18*2;
		  }
		  break;
	  }
	  default:
	  {
			break;
	  }
	  }

   }

}

static void V2560X1440_To_800X600_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)

{
   int x,y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   BYTE*pSrcData;
   //BYTE*pDstYUV;
	
   pSrcBuf = pSrc;
   pDestBuf = pOut;
  
   //-- V2560X1440->1280X720
   for(y=0;y<out_h;y++)
   {
		pSrcData = pSrcBuf + 480*2;
		for(x =0;x<out_w;)
		{
			pDestBuf[0] = pSrcData[0];//y
			pDestBuf[1] = pSrcData[1];//cb

			pDestBuf[2] = pSrcData[4];//y
			pDestBuf[3] = pSrcData[3];//cr

			x = x+ 2;
			pSrcData += 8;
			pDestBuf +=4;
		}
		
		pSrcBuf += in_w*4;
   }

}

static  void UHD_TO_FHD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int x,y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
  // BYTE*pDstYUV;
   pSrcBuf = pSrc;
   pDestBuf = pOut;
   
   //-- 3840x2160 0->1920X1080
   for(y=0;y<out_h;y++)
   {
		for(x =0;x<out_w;)
		{
			pDestBuf[0] = pSrcBuf[0];//y
			pDestBuf[1] = pSrcBuf[1];//cb

			pDestBuf[2] = pSrcBuf[4];//y
			pDestBuf[3] = pSrcBuf[3];//cr

			x = x+ 2;
			pSrcBuf +=8;
			pDestBuf +=4;
		}
		pSrcBuf += in_w*2;
   }
}
static void UHD_To_HD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int x,y;
   BYTE*pSrcBuf;
   DWORD *pSrcData;
   DWORD *pDestData;
   BYTE*pSrcTmp;
   BYTE*pDestTmp;
   
   pSrcBuf = pSrc;
   pDestData = (DWORD *) pOut;
   //----------3840x2160->1280x720
   for(y=0;y<in_h;y++)
   {
	  if(y%3 == 0)
	  {
		  for(x=0;x<in_w;)
		  {
			 pSrcData =(DWORD *)pSrcBuf;
			*pDestData = *pSrcData;
			 pDestTmp =(BYTE *)pDestData;
			 pSrcData++;
			 pSrcTmp =(BYTE *)pSrcData;
			 pDestTmp[2] =  pSrcTmp[2];
			 pDestTmp[3] =  pSrcTmp[3];
			 
			pDestData +=1;
			pSrcBuf +=12;
			x= x+6;
		  }
	  }
	  else
	  {
		pSrcBuf += in_w*2;	
	  }

   }

}

static  void UHD_To_2560x1440_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int x,y;
   BYTE*pSrcBuf;
   DWORD *pSrcData;
   DWORD *pDestData;
   BYTE*pSrcTmp;
   BYTE*pDestTmp;
   
   pSrcBuf = pSrc;
   pDestData = (DWORD *) pOut;
   //----------3840x2160->2560X1440
   for(y=0;y<in_h;y++)
   {
	  switch(y%6)
	  {
		case 0:
		case 2:
		case 4:
		case 5:
		{
		  for(x=0;x<in_w;)
		  {
			 pSrcData =(DWORD *)pSrcBuf;
			*pDestData = *pSrcData;
			 pDestTmp =(BYTE *)pDestData;
			 pSrcData++;
			 pSrcTmp =(BYTE *)pSrcData;
			 pDestTmp[2] =  pSrcTmp[0];
			 pDestTmp[3] =  pSrcTmp[3];
			 pDestData +=1;
			 
			 pSrcData++;
			 *pDestData = *pSrcData;
			 pDestData +=1;

			 pSrcBuf +=12;
			 x= x+6;
		  }
		  break;
	  	}
	  	case 1:
		case 3:
	  	{
			pSrcBuf += in_w*2;	
			break;
	  	}
   	}

   }

}
static void UHDW_To_2560X1440_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int x,y;
   BYTE*pSrcBuf;
   DWORD *pSrcData;
   DWORD *pDestData;
   BYTE*pSrcTmp;
   BYTE*pDestTmp;

   pSrcBuf = pSrc;
   pDestData = (DWORD *) pOut;
   //----------4096x2160->2560x1440
   for(y=0;y<in_h;y++)
   {
	   switch(y%6)
	  {
		case 0:
		case 2:
		case 4:
		case 5:
	  	{
		  pSrcData = (DWORD *)(pSrcBuf + 128*2);
		  for(x=0;x<(in_w-256);)
		  {
			*pDestData = *pSrcData;
			 pDestTmp =(BYTE *)pDestData;
			 pSrcData++;
			 pSrcTmp =(BYTE *)pSrcData;
			 pDestTmp[2] =  pSrcTmp[0];
			 pDestTmp[3] =  pSrcTmp[3];
			 pDestData +=1;
			 pSrcData++;
			 *pDestData = *pSrcData;

			pDestData +=1;
			pSrcData +=2;
			x= x+6;
		  }
		  
		  pSrcBuf += in_w*2;	
		  break;
		}
	   case 1:
	   case 3:
	  	{
		 	pSrcBuf += in_w*2;	
			break;
	 	 }
	   }

   }

}
static  void UHDW_To_HD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int x,y;
   BYTE*pSrcBuf;
   DWORD *pSrcData;
   DWORD *pDestData;
   BYTE*pSrcTmp;
   BYTE*pDestTmp;

   pSrcBuf = pSrc;
   pDestData = (DWORD *) pOut;
   //----------4096x2160->1280x720
   for(y=0;y<in_h;y++)
   {
	  if(y%3 == 0)
	  {
		  pSrcData = (DWORD *)(pSrcBuf + 128*2);
		  for(x=0;x<(in_w-256);)
		  {
			*pDestData = *pSrcData;
			 pDestTmp =(BYTE *)pDestData;
			 pSrcData++;
			 pSrcTmp =(BYTE *)pSrcData;
			 pDestTmp[2] =  pSrcTmp[0];
			 pDestTmp[3] =  pSrcTmp[3];
			 
			pDestData +=1;
			pSrcData +=2;
			x= x+6;
		  }
		  pSrcBuf += in_w*2;	
	  }
	  else
	  {
		 pSrcBuf += in_w*2;	
	  }

   }

}

static void UHDW_TO_FHD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int x,y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   BYTE*pSrcData;
   //BYTE*pDstYUV;
   pSrcBuf = pSrc;
   pDestBuf = pOut;
  
   //-- 4096x2160 0->1920X1080
   for(y=0;y<out_h;y++)
   {
		pSrcData = pSrcBuf + 128*2;
		for(x =0;x<out_w;)
		{
			pDestBuf[0] = pSrcData[0];//y
			pDestBuf[1] = pSrcData[1];//cb

			pDestBuf[2] = pSrcData[4];//y
			pDestBuf[3] = pSrcData[3];//cr

			x = x+ 2;
			pSrcData +=8;
			pDestBuf +=4;
		}
		pSrcBuf += in_w*4;
   }
}

static  void V2560X1440_To_FHD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)

{
	   
	   	 int x,y;
		 BYTE*pSrcBuf;
		 BYTE*pDestBuf;
		 DWORD *pSrcData;
		 DWORD *pDestData;
		 int    out_w_size;
		 BYTE*pSrcTmp;
		 BYTE*pDestTmp;
		 out_w_size = out_w*2;
		 pDestData = (DWORD *) pOut;
		 pSrcData = (DWORD *) pSrc;
		 pSrcBuf = pSrc;
		 pDestBuf = pOut;
		 //2560X1440 -> 1920X1080
		 for(y=0;y<in_h;y++)
		 {
			
				if((y%4)==3)
				{
					pSrcBuf += in_w*2;
				}
				else
				{
					 pSrcData =  (DWORD *) pSrcBuf;
					 pDestData = (DWORD *) pDestBuf;
					for(x=0;x<in_w;)
					{
						*pDestData = *pSrcData;
						 pDestData ++;
						 pSrcData++;
						 
						*pDestData = *pSrcData;
						 pDestTmp =(BYTE*)pDestData;
						 pSrcData++;
						 pSrcTmp =(BYTE*)pSrcData;
						 pDestTmp[2] = pSrcTmp[0];
						 
						 pDestData ++;
						 pDestTmp =(BYTE*)pDestData;
						 pDestTmp[0] = pSrcTmp[2];
						 pDestTmp[1] = pSrcTmp[1];
						 pSrcData++;
						 pSrcTmp =(BYTE*)pSrcData;
						 pDestTmp[2] = pSrcTmp[2];
						 pDestTmp[3] = pSrcTmp[3];

						 pSrcData++;
						 pDestData++;
						 
						 x= x+8;
					}
					pDestBuf += out_w*2;
					pSrcBuf += in_w*2; 
				}
				
	   
		 }
	   

}

static  void V2560X1440_To_HD_Scaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
   int x,y;
   BYTE*pSrcBuf;
   BYTE*pDestBuf;
   //BYTE*pDstYUV;
   pSrcBuf = pSrc;
   pDestBuf = pOut;
   
   //-- V2560X1440->1280X720
   for(y=0;y<out_h;y++)
   {
		for(x =0;x<out_w;)
		{
			pDestBuf[0] = pSrcBuf[0];//y
			pDestBuf[1] = pSrcBuf[1];//cb

			pDestBuf[2] = pSrcBuf[4];//y
			pDestBuf[3] = pSrcBuf[3];//cr

			x = x+ 2;
			pSrcBuf +=8;
			pDestBuf +=4;
		}
		pSrcBuf += in_w*2;
   }
}

//---------------------------------
static void VideoScaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
{
	if((in_w == 1920)&&(in_h==1080)&&(out_w==1280)&&(out_h==720))
	{
		FHD_To_HD_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 1920)&&(in_h==1080)&&(out_w==800)&&(out_h==600))
	{
		FHD_To_800X600_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else  if((in_w == 3840)&&(in_h==2160)&&(out_w==800)&&(out_h==600))
	{
		UHD_To_800X600_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else  if((in_w == 4096)&&(in_h==2160)&&(out_w==800)&&(out_h==600))
	{
		UHDW_To_800X600_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 1280)&&(in_h==720)&&(out_w==1920)&&(out_h==1080))
	{
		HD_To_FHD_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 1280)&&(in_h==720)&&(out_w==800)&&(out_h==600))
	{
		HD_To_800X600_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 720)&&(in_h==576)&&(out_w==720)&&(out_h==480))
	{
		SD_PAL_To_SD_NTSC_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 720)&&(in_h==480)&&(out_w==720)&&(out_h==576))
	{
		SD_NTSC_To_SD_PAL_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 720)&&(in_h==576)&&(out_w==1920)&&(out_h==1080))
	{
		SD_PAL_To_FHD_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 720)&&(in_h==480)&&(out_w==1920)&&(out_h==1080))
	{
		SD_NTSC_To_FHD_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 720)&&(in_h==576)&&(out_w==1280)&&(out_h==720))
	{
		SD_PAL_To_HD_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 720)&&(in_h==480)&&(out_w==1280)&&(out_h==720))
	{
		SD_NTSC_To_HD_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 1280)&&(in_h==1024)&&(out_w==1920)&&(out_h==1080))
	{
		V1280X1024_To_FHD_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 1280)&&(in_h==1024)&&(out_w==1280)&&(out_h==720))
	{
		V1280X1024_To_HD_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 1280)&&(in_h==1024)&&(out_w==800)&&(out_h==600))
	{
		V1280X1024_To_800X600_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 1920)&&(in_h==1080)&&(out_w==720)&&(out_h==480))
	{
		FHD_To_SD_NTSC_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 1920)&&(in_h==1080)&&(out_w==720)&&(out_h==576))
	{
		FHD_To_SD_PAL_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 1280)&&(in_h==720)&&(out_w==720)&&(out_h==480))
	{
		HD_To_SD_NTSC_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 1280)&&(in_h==720)&&(out_w==720)&&(out_h==576))
	{
		HD_To_SD_PAL_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 1280)&&(in_h==1024)&&(out_w==720)&&(out_h==480))
	{
		V1280X1024_NTSC_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 1280)&&(in_h==1024)&&(out_w==720)&&(out_h==576))
	{
		V1280X1024_PAL_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 3840)&&(in_h==2160)&&(out_w==1920)&&(out_h==1080))
	{
		UHD_TO_FHD_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 3840)&&(in_h==2160)&&(out_w==2560)&&(out_h==1440))
	{
		UHD_To_2560x1440_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 3840)&&(in_h==2160)&&(out_w==1280)&&(out_h==720))
	{
		UHD_To_HD_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 4096)&&(in_h==2160)&&(out_w==1920)&&(out_h==1080))
	{
		UHDW_TO_FHD_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 4096)&&(in_h==2160)&&(out_w==1280)&&(out_h==720))
	{
		UHDW_To_HD_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 4096)&&(in_h==2160)&&(out_w==2560)&&(out_h==1440))
	{
		UHDW_To_2560X1440_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 2560)&&(in_h==1440)&&(out_w==1920)&&(out_h==1080))
	{
		V2560X1440_To_FHD_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 2560)&&(in_h==1440)&&(out_w==1280)&&(out_h==720))
	{
		V2560X1440_To_HD_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else if((in_w == 2560)&&(in_h==1440)&&(out_w==800)&&(out_h==600))
	{
		V2560X1440_To_800X600_Scaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
	else
	{
		All_VideoScaler(pSrc,pOut,in_w,in_h,out_w,out_h);
	}
}
static int  MemCopyFrame(int nDecoder,BYTE * dest,int nWidth,int nHeight,int interlace,BYTE *src[4],int len[4])
{
	int nCopySize[4];
	int h;
	int line_cnt;
	int res_size;
	BYTE *pSrcBuf;
	int hf_size;
	
	nCopySize[0] = len[0];
	nCopySize[1] = len[1];
	nCopySize[2] = len[2];
	nCopySize[3] = len[3];
	if(interlace ==0)
	{
		nCopySize[3] = nWidth*2*nHeight - nCopySize[0]-nCopySize[1]-nCopySize[2];
		if(nCopySize[3]<0)
		{
			return -1;
		}
		hf_size = (nWidth*nHeight*2)/(4*16*128);
		hf_size = hf_size*16*128;
		if(hf_size != len[0])
		{
			//DbgPrint("[LT] nCopySize[0] = %d ?= %d W=%d H=%d \n",nCopySize[0],hf_size,nWidth,nHeight);
			return -1;
		}
		memcpy(dest,src[0],nCopySize[0]); //
		dest +=nCopySize[0]; 
		memcpy(dest,src[1],nCopySize[1]); //
		dest +=nCopySize[1]; 
		memcpy(dest,src[2],nCopySize[2]); //
		dest +=nCopySize[2]; 	
		memcpy(dest,src[3],nCopySize[3]); //	
		//---------------------------

	}
	else
	{
		//-----copy buf 0---
		nCopySize[3] = nWidth*nHeight - nCopySize[0]-nCopySize[1]-nCopySize[2];
		if(nCopySize[3]<0)
		{
			return -1;
		}
		hf_size = (nWidth*nHeight)/(4*16*128);
		hf_size = hf_size*16*128;
		if(hf_size != len[0])
		{
			//DbgPrint("[LT] nCopySize[0] = %d ?= %d W=%d H=%d \n",nCopySize[0],hf_size,nWidth,nHeight);
			return -1;
		}
		
		pSrcBuf = src[0];
		line_cnt = nCopySize[0]/(nWidth*2);
		res_size = nCopySize[0]- (line_cnt*nWidth*2);
		if(res_size <0)
		{
			return -1;
		}
		//DbgPrint("MemCopyFrame bufer0 nCopySize[0]= %d  res_size=%d line_cnt =%d \n",nCopySize[0],res_size,line_cnt);
		for(h=0; h <line_cnt; h++)
		{
			memcpy(dest,pSrcBuf,nWidth*2);
			dest += nWidth*2;
			memcpy(dest,pSrcBuf,nWidth*2);
			pSrcBuf += nWidth*2;
			dest += nWidth*2;
		}
		if(res_size >0)
		{
			memcpy(dest,pSrcBuf,res_size);
			pSrcBuf = src[1];
			dest +=res_size;
			memcpy(dest,pSrcBuf,(nWidth*2-res_size));
			dest += nWidth*2-res_size;
			memcpy(dest,dest-nWidth*2,nWidth*2);
			dest += nWidth*2;
			pSrcBuf += nWidth*2-res_size;
			nCopySize[1] -= nWidth*2-res_size;
		}
		else
		{
			pSrcBuf = src[1];
		}
		//-----copy buf 1---
		line_cnt = nCopySize[1]/(nWidth*2);
		res_size = nCopySize[1]- (line_cnt*nWidth*2);
		if(res_size <0)
		{
			
			return -1;
		}
		//DbgPrint("MemCopyFrame bufer1 nCopySize[1]= %d  res_size=%d line_cnt =%d \n",nCopySize[1],res_size,line_cnt);
		for(h=0; h <line_cnt; h++)
		{
			memcpy(dest,pSrcBuf,nWidth*2);
			dest += nWidth*2;
			memcpy(dest,pSrcBuf,nWidth*2);
			pSrcBuf += nWidth*2;
			dest += nWidth*2;
		}
		if(res_size >0)
		{
			memcpy(dest,pSrcBuf,res_size);
			pSrcBuf = src[2];
			dest +=res_size;
			memcpy(dest,pSrcBuf,(nWidth*2-res_size));
			dest += nWidth*2-res_size;
			memcpy(dest,dest-nWidth*2,nWidth*2);
			dest += nWidth*2;
			pSrcBuf += nWidth*2-res_size;
			nCopySize[2] -= nWidth*2-res_size;
		}
		else
		{
			pSrcBuf = src[2];
		}

		//-----copy buf 2---
		line_cnt = nCopySize[2]/(nWidth*2);
		res_size = nCopySize[2]- line_cnt*nWidth*2;
		if(res_size <0)
		{
			
			return -1;
		}
		//DbgPrint("MemCopyFrame bufer2 nCopySize[2]= %d  res_size=%d line_cnt =%d \n",nCopySize[2],res_size,line_cnt);
		for(h=0; h <line_cnt; h++)
		{
			memcpy(dest,pSrcBuf,nWidth*2);
			dest += nWidth*2;
			memcpy(dest,pSrcBuf,nWidth*2);
			pSrcBuf += nWidth*2;
			dest += nWidth*2;
		}
		if(res_size >0)
		{
			memcpy(dest,pSrcBuf,res_size);
			pSrcBuf = src[3];
			dest +=res_size;
			memcpy(dest,pSrcBuf,(nWidth*2-res_size));
			dest += nWidth*2-res_size;
			memcpy(dest,dest-nWidth*2,nWidth*2);
			dest += nWidth*2;
			pSrcBuf += nWidth*2-res_size;
			nCopySize[3] -= nWidth*2-res_size;
		}
		else
		{
			pSrcBuf = src[3];
		}
		//-----copy buf 3---
		line_cnt = nCopySize[3]/(nWidth*2);
		res_size = nCopySize[3]- line_cnt*nWidth*2;

		//DbgPrint("MemCopyFrame bufer3 nCopySize[3]= %d  res_size=%d line_cnt =%d \n",nCopySize[3],res_size,line_cnt);
		for(h=0; h <line_cnt; h++)
		{
			memcpy(dest,pSrcBuf,nWidth*2);
			dest += nWidth*2;
			memcpy(dest,(dest-nWidth*2),nWidth*2);
			pSrcBuf += nWidth*2;
			dest += nWidth*2;
		}
		
		//-------------------
		
	}
	return 0;
}

//--------------------------------
void video_data_process(struct work_struct *p_work)
{
	struct hws_video *videodev = container_of(p_work, struct hws_video, videowork);
	struct hwsvideo_buffer *buf;
	//unsigned long flags;
	unsigned long devflags;
	int nVindex=-1;
	int i;
	//int copysize;
	//uint8_t *pSrc;
	int in_width;
	int in_height;
	int in_vsize;
	int out_size=0;
	BYTE *bBuf[4];
	int  nCopySize[4];
	int interlace;
	int miss_freme =0;
	struct hws_pcie_dev *pdx = videodev->dev;
	int nCh;
	bBuf[0] = NULL;
	bBuf[1] = NULL;
	bBuf[2] = NULL;
	bBuf[3] = NULL;
	nCopySize[0] =0;
	nCopySize[1] =0;
	nCopySize[2] =0;
	nCopySize[3] =0;
	nCh = videodev->index;
	spin_lock_irqsave(&pdx->videoslock[nCh], devflags);
	in_width = pdx->m_pVCAPStatus[nCh][0].dwWidth ;
	in_height = pdx->m_pVCAPStatus[nCh][0].dwHeight;
	if(pdx->m_pVCAPStatus[nCh][0].dwinterlace ==1)
	{
		in_height = in_height*2;
	}
	in_vsize = in_width*in_height*2;
	
	//printk("video_data_process [%d]dev->m_curr_No_Video[videodev->index] =%d \n",videodev->index,dev->m_curr_No_Video[videodev->index]);
	//---------------------------
	if(pdx->m_curr_No_Video[nCh]==0 )
	{
		nVindex = -1; 
		if(pdx->m_VideoInfo[nCh].pStatusInfo[pdx->m_nRDVideoIndex[nCh]].byLock == MEM_LOCK)
		{
				nVindex =pdx->m_nRDVideoIndex[nCh];
				bBuf[0]  = pdx->m_VideoInfo[nCh].m_pVideoBufData[nVindex];
				bBuf[1] = pdx->m_VideoInfo[nCh].m_pVideoBufData1[nVindex];
				bBuf[2] = pdx->m_VideoInfo[nCh].m_pVideoBufData2[nVindex];
				bBuf[3] = pdx->m_VideoInfo[nCh].m_pVideoBufData3[nVindex];
				nCopySize[0] = pdx->m_VideoInfo[nCh].m_VideoBufferSize[0];
				nCopySize[1] = pdx->m_VideoInfo[nCh].m_VideoBufferSize[1];
				nCopySize[2] = pdx->m_VideoInfo[nCh].m_VideoBufferSize[2];
				nCopySize[3] = pdx->m_VideoInfo[nCh].m_VideoBufferSize[3];
				interlace = pdx->m_VideoInfo[nCh].pStatusInfo[nVindex].dwinterlace;
		}
		if(nVindex== -1)
		{
			miss_freme = 1;
			if(pdx->m_nRDVideoIndex[nCh] == 0)
			{
				nVindex = MAX_VIDEO_QUEUE-1;
			}
			else
			{
				nVindex = pdx->m_nRDVideoIndex[nCh]-1;
			}
			bBuf[0]  = pdx->m_VideoInfo[nCh].m_pVideoBufData[nVindex];
			bBuf[1] = pdx->m_VideoInfo[nCh].m_pVideoBufData1[nVindex];
			bBuf[2] = pdx->m_VideoInfo[nCh].m_pVideoBufData2[nVindex];
			bBuf[3] = pdx->m_VideoInfo[nCh].m_pVideoBufData3[nVindex];
			nCopySize[0] = pdx->m_VideoInfo[nCh].m_VideoBufferSize[0];
			nCopySize[1] = pdx->m_VideoInfo[nCh].m_VideoBufferSize[1];
			nCopySize[2] = pdx->m_VideoInfo[nCh].m_VideoBufferSize[2];
			nCopySize[3] = pdx->m_VideoInfo[nCh].m_VideoBufferSize[3];
			interlace = pdx->m_VideoInfo[nCh].pStatusInfo[nVindex].dwinterlace;
		}
	}
	else
	{
		//spin_lock_irqsave(&pdx->videoslock[nCh], devflags);
		for( i = 0 ;i<MAX_VIDEO_QUEUE;i++)
		{
			if(pdx->m_VideoInfo[nCh].pStatusInfo[i].byLock == MEM_LOCK)
			{
				pdx->m_VideoInfo[nCh].pStatusInfo[i].byLock = MEM_UNLOCK;		
			}
		}
		//spin_unlock_irqrestore(&pdx->videoslock[nCh], devflags);
	}
	//---------------------------
	//spin_lock_irqsave(&videodev->slock, flags);
	if(list_empty(&videodev->queue)){
		//spin_unlock_irqrestore(&videodev->slock, flags);
		goto vexit;
	}
	
	buf = list_entry(videodev->queue.next, struct hwsvideo_buffer, queue);
	list_del(&buf->queue);	
	
	buf->vb.vb2_buf.timestamp = ktime_get_ns();
	//buf->vb.field = videodev->pixfmt;
	buf->vb.field = V4L2_FIELD_NONE;
	if(buf->mem)
	{
		//----------------------
		// copy data to buffer 
		if(pdx->m_curr_No_Video[nCh]==0 )
		{
			out_size = videodev->current_out_width*videodev->curren_out_height*2;
			if(in_vsize != out_size)
			{
				//printk("in_width =%d  in_height =%d \n",in_width,in_height);
				//printk("out_width =%d out_height =%d \n",videodev->current_out_width,videodev->curren_out_height);
				if(pdx->m_VideoInfo[nCh].m_pVideoScalerBuf)
				{
					MemCopyFrame(nCh,pdx->m_VideoInfo[nCh].m_pVideoScalerBuf,in_width,in_height,interlace,bBuf,nCopySize);
					VideoScaler(pdx->m_VideoInfo[nCh].m_pVideoScalerBuf,buf->mem,in_width,in_height,videodev->current_out_width,videodev->curren_out_height);
				}
			}
			else
			{
				MemCopyFrame(nCh,buf->mem,in_width,in_height,interlace,bBuf,nCopySize);
			}
		}
		else
		{
			SetNoVideoMem(buf->mem,videodev->current_out_width,videodev->curren_out_height);
		}
	}
	
		
	//----------------------------------------
	buf->vb.sequence = videodev->seqnr++;
	vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
	//printk("vb2_buffer_done [%d]\n",videodev->index);
	//spin_unlock_irqrestore(&videodev->slock, flags);
vexit:
	//spin_lock_irqsave(&pdx->videoslock[nCh], devflags);
	if(pdx->m_curr_No_Video[nCh]==0 )
	{
		pdx->m_VideoInfo[nCh].pStatusInfo[nVindex].byLock  = MEM_UNLOCK;
		pdx->m_nRDVideoIndex[nCh] = nVindex+1;
		if(pdx->m_nRDVideoIndex[nCh] >=MAX_VIDEO_QUEUE)
		{
			pdx->m_nRDVideoIndex[nCh] =0;
		}
	}
	spin_unlock_irqrestore(&pdx->videoslock[nCh], devflags);
	return;
}
static void hws_get_video_param(struct hws_pcie_dev *dev,int index)
{
	
	//printk( "%s(): %x \n", __func__, index);
	int width,height;
	width= dev->m_pVCAPStatus[index][0].dwWidth;
	height=dev->m_pVCAPStatus[index][0].dwHeight;
	dev->video[index].current_out_pixfmt =0;
	dev->video[index].current_out_size_index = 0;
	dev->video[index].current_out_width = width;
	dev->video[index].curren_out_height = height;
	dev->video[index].current_out_framerate = 60;
	dev->video[index].Interlaced = 0;
	//printk( "%s(%dx%d):  \n", __func__, width,height);

}

static void hws_adapters_init(struct hws_pcie_dev *dev)
{
  int i;
  for (i = 0; i <MAX_VID_CHANNELS; i++) {
		hws_get_video_param(dev,i);
	}
}
void hws_remove_deviceregister(struct hws_pcie_dev *dev)
{
	int i;
	struct video_device *vdev ;
	for(i=0;i<dev->m_nCurreMaxVideoChl;i++)
	{
		vdev = &(dev->video[i].vdev);
		if(vdev)
		{
			v4l2_device_unregister(&dev->video[i].v4l2_dev);
			vdev = NULL;
		}
	}
}
int hws_video_register(struct hws_pcie_dev *dev)
{
	struct video_device *vdev ;
	struct vb2_queue *q ;
	int i;
	int err=-1;
	//printk("hws_video_register Start\n");
	for(i=0;i<dev->m_nCurreMaxVideoChl;i++)
	{
			//printk("v4l2_device_register[%d]\n",i);
			err = v4l2_device_register(&dev->pdev->dev, &dev->video[i].v4l2_dev);
			if(err<0){
				printk(KERN_ERR " v4l2_device_register 0 error! \n");
				hws_remove_deviceregister(dev);
				return -1;
			}
	}
	//printk("v4l2_device_register end\n");
	//----------------------------------------------------
	for(i=0;i<dev->m_nCurreMaxVideoChl;i++){
		//printk("v4l2_device_register INT[%d]\n",i);
		vdev = &(dev->video[i].vdev);
		q = &(dev->video[i].vq);
		if (NULL == vdev){
			printk(KERN_ERR " video_device_alloc failed !!!!! \n");
			goto fail;
		}
		dev->video[i].index = i;
		dev->video[i].dev = dev;
		dev->video[i].fileindex =0;
		dev->video[i].startstreamIndex=0;
		dev->video[i].std = V4L2_STD_NTSC_M;
		dev->video[i].pixfmt = V4L2_PIX_FMT_YUYV;
		//-------------------
		dev->video[i].m_Curr_Brightness = BrightnessDefault;
		dev->video[i].m_Curr_Contrast   = ContrastDefault;
		dev->video[i].m_Curr_Saturation = SaturationDefault;
		dev->video[i].m_Curr_Hue = HueDefault; 
		//-------------------
		vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
		vdev->v4l2_dev = &(dev->video[i].v4l2_dev);
		vdev->lock = &(dev->video[i].video_lock);
		vdev->fops = &hws_fops;
		strcpy(vdev->name,KBUILD_MODNAME);
		vdev->release = video_device_release_empty;
		vdev->vfl_dir = VFL_DIR_RX;
		vdev->ioctl_ops = &hws_ioctl_fops;
		mutex_init(&(dev->video[i].video_lock));
		mutex_init(&(dev->video[i].queue_lock));
		spin_lock_init(&dev->video[i].slock);
		//printk("v4l2_device_register INT3[%d]\n",i);
		INIT_LIST_HEAD(&dev->video[i].queue);
		//printk("v4l2_device_register INT2[%d]\n",i);
		video_set_drvdata(vdev, &(dev->video[i]));
		
		q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		q->io_modes = VB2_READ | VB2_MMAP | VB2_USERPTR;
		//q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF | VB2_READ;
		q->gfp_flags = GFP_DMA32;
		q->min_queued_buffers = 2;
		q->drv_priv = &(dev->video[i]);
		q->buf_struct_size = sizeof(struct hwsvideo_buffer);
		q->ops = &hwspcie_video_qops;
		
		//q->mem_ops = &vb2_dma_contig_memops;
		//q->mem_ops = &vb2_dma_sg_memops;
		q->mem_ops = &vb2_vmalloc_memops;
		
		q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
		
		q->lock = &(dev->video[i].queue_lock);
		q->dev = &(dev->pdev->dev);
		vdev->queue = q;	
		err = vb2_queue_init(q);
		if(err != 0){
			printk(KERN_ERR " vb2_queue_init failed !!!!! \n");
			goto fail;	
		}
		
		INIT_WORK(&dev->video[i].videowork,video_data_process);
		#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,7,0))
		err = video_register_device(vdev, VFL_TYPE_GRABBER,-1);
		#else
		err = video_register_device(vdev, VFL_TYPE_VIDEO,-1);
		#endif
		if(err!=0){
			printk(KERN_ERR " v4l2_device_register failed !!!!! \n");
			goto fail;
		}else{
			//printk(" video_register_device OK !!!!! \n");
		}
	}
	//printk("hws_video_register End\n");
	return 0;
fail:
	for(i=0;i<dev->m_nCurreMaxVideoChl;i++){
		vdev = &dev->video[i].vdev;
		video_unregister_device(vdev);
		v4l2_device_unregister(&dev->video[i].v4l2_dev);
	}
	return err;
}

/* HDMI 0x39[3:0] - CS_DATA[27:24] 0 for reserved values*/
static const int cs_data_fs[] = {
	44100,
	0,
	48000,
	32000,
	0,
	0,
	0,
	0,
	88200,
	768000,
	96000,
	0,
	176000,
	0,
	192000,
	0,
};
#if 1
static struct snd_pcm_hardware audio_pcm_hardware = {
    .info 				=	(SNDRV_PCM_INFO_MMAP |
                             SNDRV_PCM_INFO_INTERLEAVED |
                             SNDRV_PCM_INFO_BLOCK_TRANSFER |
                             SNDRV_PCM_INFO_RESUME |
                             SNDRV_PCM_INFO_MMAP_VALID),
    .formats 			=	SNDRV_PCM_FMTBIT_S16_LE,
    .rates 				=   SNDRV_PCM_RATE_48000,
    .rate_min 			=	48000,
    .rate_max 			=	48000,
    .channels_min 		=	2,
    .channels_max 		=	2,
    .buffer_bytes_max 	=	64*1024,
    .period_bytes_min 	=	512,
    .period_bytes_max 	=	16*1024,
    .periods_min 		=	2,
    .periods_max 		=	255,
};
#else
static struct snd_pcm_hardware audio_pcm_hardware ={
	.info =  (SNDRV_PCM_INFO_INTERLEAVED |SNDRV_PCM_INFO_BLOCK_TRANSFER ),
	.formats = (SNDRV_PCM_FMTBIT_S16_LE),
	.rates = SNDRV_PCM_RATE_KNOT | SNDRV_PCM_RATE_48000,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max =2,
	.period_bytes_min = HWS_AUDIO_CELL_SIZE,
	.period_bytes_max = HWS_AUDIO_CELL_SIZE,
	.periods_min      = 4,
	.periods_max      = 4,
	.buffer_bytes_max = HWS_AUDIO_CELL_SIZE*4,
};
#endif
int hws_pcie_audio_open(struct snd_pcm_substream *substream)
{
	struct hws_audio *drv = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	
    drv->sample_rate_out        = 48000;
    drv->channels               = 2;
	//printk(KERN_INFO "%s() index:%x\n",__func__,drv->index);
    runtime->hw = audio_pcm_hardware;
    drv->substream = substream;
	//snd_pcm_hw_constraint_minmax(runtime,SNDRV_PCM_HW_PARAM_RATE,setrate,setrate);
	return 0;
}

int hws_pcie_audio_close(struct snd_pcm_substream *substream)
{
//	struct hws_audio *chip = snd_pcm_substream_chip(substream);
	//printk(KERN_INFO "%s() \n",__func__);
	return 0;
} 
int hws_pcie_audio_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params)
{
	//printk(KERN_INFO "%s() \n",__func__);
	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
}  

int hws_pcie_audio_hw_free(struct snd_pcm_substream *substream)
{
	//printk(KERN_INFO "%s() \n",__func__);
	return snd_pcm_lib_free_pages(substream);
} 

int hws_pcie_audio_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct hws_audio *drv = snd_pcm_substream_chip(substream);
	//struct hws_pcie_dev *dev= drv->dev;
	//int i;
	unsigned long flags;
	//printk(KERN_INFO "%s() index:%x\n",__func__,drv->index);
	
	spin_lock_irqsave(&drv->ring_lock, flags);
    drv->ring_size_byframes = runtime->buffer_size;
    drv->ring_wpos_byframes = 0;
    drv->period_size_byframes = runtime->period_size;
    drv->period_used_byframes = 0;
	drv->ring_offsize =0;
	drv->ring_over_size =0;
    spin_unlock_irqrestore(&drv->ring_lock, flags);
	
	return 0;
}  
int hws_pcie_audio_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct hws_audio *chip = snd_pcm_substream_chip(substream);
	struct hws_pcie_dev *dev= chip->dev;
	switch(cmd){
		case SNDRV_PCM_TRIGGER_START:
			//HWS_PCIE_READ(HWS_DMA_BASE(chip->index), HWS_DMA_STATUS);
			//start dma
			//HWS_PCIE_WRITE(HWS_INT_BASE, HWS_DMA_MASK(chip->index), 0x00000001); 
			//HWS_PCIE_WRITE(HWS_DMA_BASE(chip->index), HWS_DMA_START, 0x00000001);
			//printk(KERN_INFO "SNDRV_PCM_TRIGGER_START index:%x\n",chip->index);	
	 		chip->ring_wpos_byframes = 0;
        	chip->period_used_byframes = 0;
			StartAudioCapture(dev,chip->index);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
			//stop dma
			//HWS_PCIE_WRITE(HWS_INT_BASE, HWS_DMA_MASK(chip->index), 0x000000000); 
			//HWS_PCIE_WRITE(HWS_DMA_BASE(chip->index), HWS_DMA_START, 0x00000000);
			//printk(KERN_INFO "SNDRV_PCM_TRIGGER_STOP index:%x\n",chip->index);
			StopAudioCapture(dev,chip->index);
			break;
		default:
			return -EINVAL;
			break;
	}
	return 0;
}  
//-------------------------------------------------


//-------------------------------------------------
static snd_pcm_uframes_t hws_pcie_audio_pointer(struct snd_pcm_substream *substream)
{
	struct hws_audio *drv = snd_pcm_substream_chip(substream);
	//struct snd_pcm_runtime *runtime = substream->runtime;
	snd_pcm_uframes_t pos;
	int dwAudioCh;
	unsigned long flags;
	dwAudioCh = drv->index;
	//printk(KERN_INFO "%s() index:%x\n",__func__,dwAudioCh);
	spin_lock_irqsave(&drv->ring_lock,flags);  //spin_lock
    pos = drv->ring_wpos_byframes;
    spin_unlock_irqrestore(&drv->ring_lock,flags); //spin_unlock
	 return pos;
}

struct snd_pcm_ops hws_pcie_pcm_ops ={
	.open =			hws_pcie_audio_open,
	.close = 		hws_pcie_audio_close,
	.ioctl =		snd_pcm_lib_ioctl,
	.hw_params = 	hws_pcie_audio_hw_params,
	.hw_free =		hws_pcie_audio_hw_free,
	.prepare =		hws_pcie_audio_prepare,
	.trigger =		hws_pcie_audio_trigger,
	.pointer =		hws_pcie_audio_pointer
};

int hws_audio_register(struct hws_pcie_dev *dev)
{
	struct snd_pcm		*pcm;
	struct snd_card 	*card;
	int ret;
	int i;
	int ai_index;
	char audioname[100];	  
	//printk("hws_audio_register Start\n");
	ai_index = dev->m_Device_PortID*dev->m_nCurreMaxVideoChl+1;
	for(i=0;i<dev->m_nCurreMaxVideoChl;i++){
		sprintf(audioname,"%s %d",HWS_AUDOI_NAME,i+ai_index);
		//printk("%s\n",audioname);
		ret = snd_card_new(&dev->pdev->dev, -1, audioname, THIS_MODULE,	sizeof(struct hws_audio), &card);
	   // ret = snd_card_new(&dev->pdev->dev, audio_index[i], audio_id[i], THIS_MODULE,	sizeof(struct hws_audio), &card);
		if (ret < 0){
			printk(KERN_ERR "%s() ERROR: snd_card_new failed <%d>\n",__func__, ret);
			goto fail0;
		}
		strcpy(card->driver, KBUILD_MODNAME);
		sprintf(card->shortname, "%s",audioname);
		sprintf(card->longname, "%s",card->shortname);

		ret = snd_pcm_new(card,audioname,0,0,1,&pcm);
		if (ret < 0){
			printk(KERN_ERR "%s() ERROR: snd_pcm_new failed <%d>\n",__func__, ret);
			goto fail1;
		}
		dev->audio[i].index=i;
		dev->audio[i].dev=dev;
		pcm->private_data = &dev->audio[i];	
		strcpy(pcm->name,audioname);
		snd_pcm_set_ops(pcm,SNDRV_PCM_STREAM_CAPTURE,&hws_pcie_pcm_ops);
		//snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,snd_dma_pci_data(dev->pdev), HWS_AUDIO_CELL_SIZE*4, HWS_AUDIO_CELL_SIZE*4);
		 snd_pcm_lib_preallocate_pages_for_all(
            pcm,
            SNDRV_DMA_TYPE_CONTINUOUS,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0))
			card->dev,
#else
            snd_dma_continuous_data(GFP_KERNEL),
#endif
            audio_pcm_hardware.buffer_bytes_max,
            audio_pcm_hardware.buffer_bytes_max
            );
		//----------------------------
		  dev->audio[i].sample_rate_out        = 48000;
    	  dev->audio[i].channels               = 2;
		  dev->audio[i].resampled_buf_size = dev->audio[i].sample_rate_out * 2/* sample bytes */ * dev->audio[i].channels /* channels */;
		  //dev->audio[i].resampled_buf = vmalloc(dev->audio[i].resampled_buf_size);  
		//if(dev->audio[i].resampled_buf == NULL)
		//	goto fail1;
		//-----------------
		spin_lock_init(&dev->audio[i].ring_lock);
		INIT_WORK(&dev->audio[i].audiowork,audio_data_process);
		ret = snd_card_register(card);
		if ( ret < 0) {
			printk(KERN_ERR "%s() ERROR: snd_card_register failed\n",__func__);
			goto fail1;
		}
		dev->audio[i].card =card;
	}
	//printk("hws_audio_register End\n");
	return 0;
fail1:
	for(i=0;i<dev->m_nCurreMaxVideoChl;i++){
		
		if(dev->audio[i].card)
		{
			snd_card_free(dev->audio[i].card);
			dev->audio[i].card=NULL;
		}
		if(dev->audio[i].resampled_buf)
		{
			vfree(dev->audio[i].resampled_buf);
			dev->audio[i].resampled_buf = NULL;
		}
	}
fail0:
	return -1;
}
//-------------------
//static unsigned long video_data[MAX_VID_CHANNELS];
//static struct tasklet_struct dpc_video_tasklet[MAX_VID_CHANNELS];
//static unsigned long audio_data[MAX_VID_CHANNELS];
//static struct tasklet_struct dpc_audio_tasklet[MAX_VID_CHANNELS];



static void  WRITE_REGISTER_ULONG (struct hws_pcie_dev *pdx,u32 RegisterOffset,u32 Value)
{
	//map_bar0_addr[RegisterOffset/4] = Value;
	char *bar0;
	bar0 = (char*)pdx->map_bar0_addr;
	iowrite32(Value,bar0+RegisterOffset);
	//map_bar0_addr[RegisterOffset/4] = Value;

}

static u32 READ_REGISTER_ULONG (struct hws_pcie_dev *pdx,u32 RegisterOffset)
{
	char *bar0;
	bar0 = (char*)pdx->map_bar0_addr;
	//return(map_bar0_addr[RegisterOffset/4]);
	return(ioread32(bar0+RegisterOffset));
}
//----------------------------------------------
static int Check_Busy(struct hws_pcie_dev *pdx)
{
	u32  statusreg;
	u32 TimeOut = 0;
	//DbgPrint(("Check Busy in !!!\n"));
	//WRITE_REGISTER_ULONG((u32)(0x4000), 0x10);
	while (1)
	{
		statusreg = READ_REGISTER_ULONG(pdx,(u32)(CVBS_IN_BASE));
		printk("[MV] Check_Busy!!! statusreg =%X\n", statusreg);
		if (statusreg == 0xFFFFFFFF)
		{
			break;
		}
		if ((statusreg & 0x08) == 0x00)
		{
			break;
		}
		TimeOut++;
		msleep(10);
	}
	//WRITE_REGISTER_ULONG((u32)(0x4000), 0x10);


	//DbgPrint(("Check Busy out !!!\n"));

	return 0;
}

static void StopDsp(struct hws_pcie_dev *pdx)
{
	//int j, i;
	u32  statusreg;
	statusreg = READ_REGISTER_ULONG(pdx,(u32)(CVBS_IN_BASE));
	printk("[MV] Busy!!! statusreg =%X\n", statusreg);
	if (statusreg == 0xFFFFFFFF)
	{
			return;
	}
	WRITE_REGISTER_ULONG(pdx,(u32)(CVBS_IN_BASE), 0x10);
	Check_Busy(pdx);
	WRITE_REGISTER_ULONG(pdx,( CVBS_IN_BASE + (2 * PCIE_BARADDROFSIZE)), 0x00);
		
	
}
static void EnableVideoCapture(struct hws_pcie_dev *pdx,int index,int en)
{
	ULONG status;
	int enable;
	if(pdx->m_PciDeviceLost) return;
	status = READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE+2*PCIE_BARADDROFSIZE));
	if(en)
	{
		enable =1;
		enable = enable<<index;
		status =  status|enable;
	}
	else
	{
		enable =1;
		enable = enable<<index;
		enable = ~enable;
		status =  status&enable;
	}
	pdx->m_bVCapStarted[index] = en;
	WRITE_REGISTER_ULONG(pdx,( CVBS_IN_BASE + (2 * PCIE_BARADDROFSIZE)), status);
	status = READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE+2*PCIE_BARADDROFSIZE));
	//printk("EnableVideoCapture[%d]=%X %d \n",index,status,pdx->m_bVCapStarted[index]);
}
static void EnableAudioCapture(struct hws_pcie_dev *pdx,int index,int en)
{
	ULONG status;
	int enable;
	if(pdx->m_PciDeviceLost) return;
	status =  READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE+3*PCIE_BARADDROFSIZE));

	if(en)
	{
		enable =1;
		enable = enable<<index;
		status =  status|enable;
	}
	else
	{
		enable =1;
		enable = enable<<index;
		enable = ~enable;
		status =  status&enable;
	}
	pdx->m_bACapStarted[index] = en;
	WRITE_REGISTER_ULONG(pdx,( CVBS_IN_BASE + (3 * PCIE_BARADDROFSIZE)), status);
	//printk("EnableAudioCapture =%X",status);
}

static int SetVideoFormteSize(struct hws_pcie_dev *pdx,int ch,int w,int h)
{
	   int hf_size;
	   int hf_size2;
	   //int frame_size;
		if(ch !=0) return -1;
		
		hf_size = (w*h*2)/(4*16*128);
		hf_size = hf_size*16*128;
		
		hf_size2 = (w*h*2)/(4*16*128);
		hf_size2 = w*h*2 -(hf_size2*16*128*3);
		//if((hf_size2 <0) ||(hf_size2 > m_MaxHWVideoBufferSize)||(hf_size >m_MaxHWVideoBufferSize))
		if(hf_size2 <0)
		{
			return -1;
		}
		pdx->m_format[0].dwWidth = w;		 // Image Width	
		pdx->m_format[1].dwWidth = w;		 // Image Width		
		pdx->m_format[2].dwWidth = w;		 // Image Width	
		pdx->m_format[3].dwWidth = w;		 // Image Width	
		
		pdx->m_format[0].dwHeight = h;	
		pdx->m_format[1].dwHeight = h;
		pdx->m_format[2].dwHeight = h;	
		pdx->m_format[3].dwHeight = h;
		
		pdx->m_format[0].HLAF_SIZE = hf_size; 
		pdx->m_format[1].HLAF_SIZE = hf_size; 
		pdx->m_format[2].HLAF_SIZE = hf_size;
		
		hf_size2 = hf_size2/(16*128);
		hf_size2 = (hf_size2+1)*16*128;
		pdx->m_format[3].HLAF_SIZE = hf_size2;
		return 1;
		
}

static void DmaMemFreePool(struct hws_pcie_dev *pdx)
	{
		//Trace t("DmaMemFreePool()");
		int	k;
		int index;
		unsigned long phyvirt_addr; 
		if(pdx->m_bBufferAllocate == TRUE)
		{
			//---------------
			for(index=0; index<pdx->m_nMaxChl; index++)
			{
				if(pdx->m_pbyVideoBuffer[index])
				{
					//printk("DmaMemFreePool ::m_pbyVideoBuffer = %p\n",  pdx->m_pbyVideoBuffer[index]);
					#if 0
					for (phyvirt_addr=(unsigned long)pdx->m_pbyVideoBuffer_area[i]; phyvirt_addr < ((unsigned long)pdx->m_pbyVideoBuffer_area[i] + pdx->m_MaxHWVideoBufferSize);phyvirt_addr+=PAGE_SIZE) 
					{
						// clear all pages
						ClearPageReserved(virt_to_page(phyvirt_addr));
					}
					kfree(pdx->m_pbyVideoBuffer[i]);
					#else
						dma_free_coherent(&pdx->pdev->dev, pdx->m_MaxHWVideoBufferSize, pdx->m_pbyVideoBuffer[index], pdx->m_pbyVideo_phys[index]);
					#endif 
					pdx->m_pbyVideoBuffer[index] = NULL;
				}				
			}
			//----------------------------------------------------
			for(index=0; index<pdx->m_nCurreMaxVideoChl;index++)
			{
				//printk("DmaMemFreePool ::m_pVideoScalerBuf = %p\n",  pdx->m_VideoInfo[index].m_pVideoScalerBuf);
				if(pdx->m_VideoInfo[index].m_pVideoScalerBuf !=NULL)
				{
					vfree(pdx->m_VideoInfo[index].m_pVideoScalerBuf);
					pdx->m_VideoInfo[index].m_pVideoScalerBuf = NULL;
				}
				for( k=0; k<MAX_VIDEO_QUEUE;k++)
				{
					//-------------------------
					//printk("DmaMemFreePool ::m_pVideoBufData[%d] = %p\n",k,pdx->m_VideoInfo[index].m_pVideoBufData[k]);
					if(pdx->m_VideoInfo[index].m_pVideoBufData[k])
					{
						for (phyvirt_addr=(unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area[k]; phyvirt_addr < ((unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area[k] + pdx->m_MaxHWVideoBufferSize);phyvirt_addr+=PAGE_SIZE) 
						{
								// clear all pages
							ClearPageReserved(virt_to_page(phyvirt_addr));
						}
						kfree(pdx->m_VideoInfo[index].m_pVideoBufData[k]);  
						pdx->m_VideoInfo[index].m_pVideoBufData[k] = NULL;
					}
					//---------------------------------
					//-------------------------
					//printk("DmaMemFreePool ::m_pVideoBufData1[%d] = %p\n",k,pdx->m_VideoInfo[index].m_pVideoBufData1[k]);
					if(pdx->m_VideoInfo[index].m_pVideoBufData1[k])
					{
						for (phyvirt_addr=(unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area1[k]; phyvirt_addr < ((unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area1[k] + pdx->m_MaxHWVideoBufferSize);phyvirt_addr+=PAGE_SIZE) 
						{
								// clear all pages
								ClearPageReserved(virt_to_page(phyvirt_addr));
						}
						kfree(pdx->m_VideoInfo[index].m_pVideoBufData1[k]);  
						pdx->m_VideoInfo[index].m_pVideoBufData1[k] = NULL;
					}
					//---------------------------------
					//-------------------------
					//printk("DmaMemFreePool ::m_pVideoBufData2[%d] = %p\n",k,pdx->m_VideoInfo[index].m_pVideoBufData2[k]);
					if(pdx->m_VideoInfo[index].m_pVideoBufData2[k])
					{
						for (phyvirt_addr=(unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area2[k]; phyvirt_addr < ((unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area2[k] + pdx->m_MaxHWVideoBufferSize);phyvirt_addr+=PAGE_SIZE) 
						{
								// clear all pages
								ClearPageReserved(virt_to_page(phyvirt_addr));
						}
						kfree(pdx->m_VideoInfo[index].m_pVideoBufData2[k]);  
						pdx->m_VideoInfo[index].m_pVideoBufData2[k] = NULL;
					}
					//---------------------------------
					//-------------------------
					//printk("DmaMemFreePool ::m_pVideoBufData3[%d] = %p\n",k,pdx->m_VideoInfo[index].m_pVideoBufData3[k]);
					if(pdx->m_VideoInfo[index].m_pVideoBufData3[k])
					{
						for (phyvirt_addr=(unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area3[k]; phyvirt_addr < ((unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area3[k] + pdx->m_MaxHWVideoBufferSize);phyvirt_addr+=PAGE_SIZE) 
						{
								// clear all pages
								ClearPageReserved(virt_to_page(phyvirt_addr));
						}
						kfree(pdx->m_VideoInfo[index].m_pVideoBufData3[k]);  
						pdx->m_VideoInfo[index].m_pVideoBufData3[k] = NULL;
					}
					//---------------------------------
					
				}
				//----audio release
				for( k=0; k<MAX_AUDIO_QUEUE;k++)
				{
					if(pdx->m_AudioInfo[index].m_pAudioBufData[k])
					{
						for (phyvirt_addr=(unsigned long)pdx->m_AudioInfo[index].m_pAudioData_area[k]; phyvirt_addr < ((unsigned long)pdx->m_AudioInfo[index].m_pAudioData_area[k] + pdx->m_dwAudioPTKSize);phyvirt_addr+=PAGE_SIZE) 
						{
								// clear all pages
								ClearPageReserved(virt_to_page(phyvirt_addr));
						}
						kfree(pdx->m_AudioInfo[index].m_pAudioBufData[k]);  
						pdx->m_AudioInfo[index].m_pAudioBufData[k] = NULL;
					}
				}
				
			}
			pdx->m_bBufferAllocate = FALSE;
		}
	}


static int  DmaMemAllocPool(struct hws_pcie_dev *pdx)
	{
		u32			status	= 0;
		uint8_t				i,k;
        dma_addr_t phy_addr;
		int  index;
		unsigned long phyvirt_addr; 
		if(pdx->m_bBufferAllocate == TRUE)
		{
			DmaMemFreePool(pdx);
		}
		//------------
		for(i=0; i<pdx->m_nMaxChl; i++)
		{
		
			//printk("kmalloc [%d]size=%X************\n", i,pdx->m_MaxHWVideoBufferSize);
			pdx->m_pbyVideoBuffer[i] = dma_alloc_coherent(&pdx->pdev->dev, pdx->m_MaxHWVideoBufferSize, &pdx->m_pbyVideo_phys[i], GFP_KERNEL);

			
			if(pdx->m_pbyVideoBuffer[i]== NULL)
			{
				printk("m_pbyVideoBuffer[%d] mem Allocate Fail ************\n", i);
				pdx->m_bBufferAllocate = TRUE;
				DmaMemFreePool(pdx);
				pdx->m_bBufferAllocate = FALSE;
				status = -1;
				return status;
			}
			#if 0
			pdx->m_pbyVideoBuffer_area[i] = (char *)(((unsigned long)pdx->m_pbyVideoBuffer[i] + PAGE_SIZE -1) & PAGE_MASK);
			for (phyvirt_addr=(unsigned long)pdx->m_pbyVideoBuffer_area[i]; phyvirt_addr < ((unsigned long)pdx->m_pbyVideoBuffer_area[i] + (pdx->m_MaxHWVideoBufferSize));
			phyvirt_addr+=PAGE_SIZE) 
			{
				// reserve all pages to make them remapable
				SetPageReserved(virt_to_page(phyvirt_addr));
			} 
			memset(pdx->m_pbyVideoBuffer[i] , 0x0,(pdx->m_MaxHWVideoBufferSize) );
			phy_addr= (dma_addr_t)virt_to_phys(pdx->m_pbyVideoBuffer[i]);
			pdx->m_pbyVideo_phys[i] = phy_addr;
			#else 
				phy_addr = 	pdx->m_pbyVideo_phys[i];
			#endif 
			//printk("PHY= %X=%X\n",phy_addr,pdx->m_pbyVideo_phys[i]);
			
			pdx->m_dwVideoBuffer[i] = 	  ((u64)phy_addr)&0xFFFFFFFF;
			pdx->m_dwVideoHighBuffer[i] = ((u64)phy_addr>>32)&0xFFFFFFFF;;

			pdx->m_pbyAudioBuffer[i] = (BYTE *)(pdx->m_pbyVideoBuffer[i] + pdx->m_MaxHWVideoBufferSize -MAX_AUDIO_CAP_SIZE );
			#if 0
				phy_addr= (dma_addr_t)virt_to_phys(pdx->m_pbyAudioBuffer[i]);
			#else
				phy_addr = pdx->m_pbyVideo_phys[i] + (pdx->m_MaxHWVideoBufferSize -MAX_AUDIO_CAP_SIZE);
			#endif 
			pdx->m_pbyAudio_phys[i] = phy_addr;
			
			pdx->m_dwAudioBuffer[i] =    pdx->m_dwVideoBuffer[i]+ pdx->m_MaxHWVideoBufferSize -MAX_AUDIO_CAP_SIZE;
			pdx->m_dwAudioBufferHigh[i] = pdx->m_dwVideoHighBuffer[i];
			//printk("[MV]Mem Video::m_dwVideoBuffer[%d] = %x\n", i, pdx->m_dwVideoBuffer[i]);
			//printk("[MV]Mem Video::m_dwVideoHighBuffer[%d] = %x\n", i, pdx->m_dwVideoHighBuffer[i]);
			//printk("[MV]Mem Audio::m_dwAudioBuffer[%d] = %x\n", i, pdx->m_dwAudioBuffer[i]);
			//printk("[MV]Mem Audio::m_dwAudioBufferHigh[%d] = %x\n", i, pdx->m_dwAudioBufferHigh[i]);

		}

		
         //KdPrint(("Mem allocate::m_dwAudioBuffer[%d] = %x\n", i, pdx->m_dwAudioBuffer));
		//-------------- video buffer 
		for(index=0; index<pdx->m_nCurreMaxVideoChl; index++)
		{
			pdx->m_VideoInfo[index].m_pVideoScalerBuf = vmalloc(MAX_VIDEO_HW_W*MAX_VIDEO_HW_H*2);
			if(pdx->m_VideoInfo[index].m_pVideoScalerBuf ==NULL)
			{
					pdx->m_bBufferAllocate = TRUE;
					DmaMemFreePool(pdx);
					pdx->m_bBufferAllocate = FALSE;
					status = -1;
					return status;
			}
			for( k=0; k<MAX_VIDEO_QUEUE;k++)
			{
				//----------------------------------- buf 
				pdx->m_VideoInfo[index].m_pVideoBufData[k]  = kmalloc((pdx->m_MaxHWVideoBufferSize), GFP_KERNEL);	
				if(!pdx->m_VideoInfo[index].m_pVideoBufData[k])
				{
					
					pdx->m_bBufferAllocate = TRUE;
					DmaMemFreePool(pdx);
					pdx->m_bBufferAllocate = FALSE;
					status = -1;
					return status;

				}
				else
				{

					pdx->m_VideoInfo[index].m_pVideoData_area[k] = (char *)(((unsigned long)pdx->m_VideoInfo[index].m_pVideoBufData[k] + PAGE_SIZE -1) & PAGE_MASK);
					for (phyvirt_addr=(unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area[k]; phyvirt_addr < ((unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area[k] + (pdx->m_MaxHWVideoBufferSize));
						phyvirt_addr+=PAGE_SIZE) 
					{
							// reserve all pages to make them remapable
							SetPageReserved(virt_to_page(phyvirt_addr));
					} 
					memset(pdx->m_VideoInfo[index].m_pVideoBufData[k] ,0x0,pdx->m_MaxHWVideoBufferSize );
				}
				//-------------------------------------------------------
				//----------------------------------- buf1 
				pdx->m_VideoInfo[index].m_pVideoBufData1[k]  = kmalloc((pdx->m_MaxHWVideoBufferSize), GFP_KERNEL);	
				if(!pdx->m_VideoInfo[index].m_pVideoBufData1[k])
				{
					
					pdx->m_bBufferAllocate = TRUE;
					DmaMemFreePool(pdx);
					pdx->m_bBufferAllocate = FALSE;
					status = -1;
					return status;

				}
				else
				{

					pdx->m_VideoInfo[index].m_pVideoData_area1[k] = (char *)(((unsigned long)pdx->m_VideoInfo[index].m_pVideoBufData[k] + PAGE_SIZE -1) & PAGE_MASK);
					for (phyvirt_addr=(unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area1[k]; phyvirt_addr < ((unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area1[k] + (pdx->m_MaxHWVideoBufferSize));
						phyvirt_addr+=PAGE_SIZE) 
					{
							// reserve all pages to make them remapable
							SetPageReserved(virt_to_page(phyvirt_addr));
					} 
					memset(pdx->m_VideoInfo[index].m_pVideoBufData1[k] ,0x0,pdx->m_MaxHWVideoBufferSize );
				}
				//-------------------------------------------------------
				//----------------------------------- buf2 
				pdx->m_VideoInfo[index].m_pVideoBufData2[k]  = kmalloc((pdx->m_MaxHWVideoBufferSize), GFP_KERNEL);	
				if(!pdx->m_VideoInfo[index].m_pVideoBufData2[k])
				{
					
					pdx->m_bBufferAllocate = TRUE;
					DmaMemFreePool(pdx);
					pdx->m_bBufferAllocate = FALSE;
					status = -1;
					return status;

				}
				else
				{

					pdx->m_VideoInfo[index].m_pVideoData_area2[k] = (char *)(((unsigned long)pdx->m_VideoInfo[index].m_pVideoBufData2[k] + PAGE_SIZE -1) & PAGE_MASK);
					for (phyvirt_addr=(unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area2[k]; phyvirt_addr < ((unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area2[k] + (pdx->m_MaxHWVideoBufferSize));
						phyvirt_addr+=PAGE_SIZE) 
					{
							// reserve all pages to make them remapable
							SetPageReserved(virt_to_page(phyvirt_addr));
					} 
					memset(pdx->m_VideoInfo[index].m_pVideoBufData2[k] ,0x0,pdx->m_MaxHWVideoBufferSize );
				}
				//-------------------------------------------------------
				//----------------------------------- buf3 
				pdx->m_VideoInfo[index].m_pVideoBufData3[k]  = kmalloc((pdx->m_MaxHWVideoBufferSize), GFP_KERNEL);	
				if(!pdx->m_VideoInfo[index].m_pVideoBufData3[k])
				{
					
					pdx->m_bBufferAllocate = TRUE;
					DmaMemFreePool(pdx);
					pdx->m_bBufferAllocate = FALSE;
					status = -1;
					return status;

				}
				else
				{

					pdx->m_VideoInfo[index].m_pVideoData_area3[k] = (char *)(((unsigned long)pdx->m_VideoInfo[index].m_pVideoBufData3[k] + PAGE_SIZE -1) & PAGE_MASK);
					for (phyvirt_addr=(unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area3[k]; phyvirt_addr < ((unsigned long)pdx->m_VideoInfo[index].m_pVideoData_area3[k] + (pdx->m_MaxHWVideoBufferSize));
						phyvirt_addr+=PAGE_SIZE) 
					{
							// reserve all pages to make them remapable
							SetPageReserved(virt_to_page(phyvirt_addr));
					} 
					memset(pdx->m_VideoInfo[index].m_pVideoBufData3[k] ,0x0,pdx->m_MaxHWVideoBufferSize );
				}
				//-------------------------------------------------------
				pdx->m_VideoInfo[index].pStatusInfo[k].byLock = MEM_UNLOCK;
				pdx->m_VideoInfo[index].m_nVideoIndex = 0;
			}
		}
		
	   //----------audio alloc 
	   #if 1
		for(i=0; i<pdx->m_nCurreMaxVideoChl; i++)
		{
			for( k=0; k<MAX_AUDIO_QUEUE;k++)
			{
				pdx->m_AudioInfo[i].m_pAudioBufData[k] =kmalloc(pdx->m_dwAudioPTKSize, GFP_KERNEL);
				if(!pdx->m_AudioInfo[i].m_pAudioBufData[k])
				{
					pdx->m_bBufferAllocate = TRUE;
					DmaMemFreePool(pdx);
					pdx->m_bBufferAllocate = FALSE;
		    		status = -1;
					return status;
				}
				else
				{
					pdx->m_AudioInfo[i].pStatusInfo[k].byLock = MEM_UNLOCK;
					pdx->m_AudioInfo[i].m_pAudioData_area[k] = (char *)(((unsigned long)pdx->m_AudioInfo[i].m_pAudioBufData[k] + PAGE_SIZE -1) & PAGE_MASK);
					for (phyvirt_addr=(unsigned long)pdx->m_AudioInfo[i].m_pAudioData_area[k]; phyvirt_addr < ((unsigned long)pdx->m_AudioInfo[i].m_pAudioData_area[k]  + pdx->m_dwAudioPTKSize);
							phyvirt_addr+=PAGE_SIZE) 
					{
						// reserve all pages to make them remapable
						SetPageReserved(virt_to_page(phyvirt_addr));
					} 
				}
			}
		}
	   #endif 
		//------------------------------------------------------------
		//KdPrint(("Mem allocate::m_pAudioData = %x\n",  pdx->m_pAudioData));
		pdx->m_bBufferAllocate = TRUE;
		//KdPrint(("DmaMemAllocPool  ed\n"));
		return 0;
}

static void StopDevice(struct hws_pcie_dev *pdx)
	{							// StopDevice		
		//Trace t("StopDevice()");
		int i;
		//int   device_lost =0;
		u32  statusreg;
        StopDsp(pdx);
		statusreg = READ_REGISTER_ULONG(pdx,(0x4000));
		//DbgPrint("[MV] Busy!!! statusreg =%X\n", statusreg);
		if (statusreg != 0xFFFFFFFF)
		{
			//set to one buffer mode 
	   	   //WRITE_REGISTER_ULONG((u32)(CVBS_IN_BASE + (25*PCIE_BARADDROFSIZE)), 0x00); //Buffer 1 address
		}
		else
		{
			pdx->m_PciDeviceLost = 1;
		}
		pdx->m_bStartRun = 0;
		if(pdx->m_PciDeviceLost ==0)
		{
			for (i = 0; i<MAX_VID_CHANNELS; i++)
			{
				EnableVideoCapture(pdx,i,0);
				EnableAudioCapture(pdx,i,0);
			}
		}
		//if(device_lost) return;
		DmaMemFreePool(pdx);		 
		//printk("StopDevice Done\n");

		
	}	
static void irq_teardown(struct hws_pcie_dev *lro)
{
	//int i;

	//BUG_ON(!lro);

	//if (lro->msix_enabled) {
	//	for (i = 0; i < lro->irq_user_count; i++) {
	//		printk("Releasing IRQ#%d\n", lro->entry[i].vector);
	//		free_irq(lro->entry[i].vector, &lro->user_irq[i]);
	//	}
	//} 
	//else 

	if (lro->irq_line != -1) {
		//printk("Releasing IRQ#%d\n", lro->irq_line);
		free_irq(lro->irq_line, lro);
	}
}
void StopKSThread(struct hws_pcie_dev *pdx)
{
	if(pdx->mMain_tsk)
	{
		kthread_stop(pdx->mMain_tsk);	
	}
}

//----------------------------
static void hws_remove(struct pci_dev *pdev)
{
	int i;
	struct video_device *vdev;
	struct hws_pcie_dev *dev = 
		(struct hws_pcie_dev*) pci_get_drvdata(pdev);
	//----------------------------
	if(dev->map_bar0_addr == NULL) return;
	//StopSys(dev);
	StopDevice(dev);
	/* disable interrupts */
	irq_teardown(dev);
	StopKSThread(dev);
	//printk("hws_remove  0\n");
	for ( i = 0; i<dev->m_nCurreMaxVideoChl; i++)
	{
		tasklet_kill(&dev->dpc_video_tasklet[i]);
		tasklet_kill(&dev->dpc_audio_tasklet[i]);	
	}
	//-------------------------
	//printk("hws_remove  1\n");
	for(i=0;i<dev->m_nCurreMaxVideoChl;i++){
		if(dev->audio[i].resampled_buf)
		{
			vfree(dev->audio[i].resampled_buf);
			dev->audio[i].resampled_buf = NULL;
		}
		if(dev->audio[i].card)
		{
			snd_card_free(dev->audio[i].card);
			dev->audio[i].card=NULL;
		}
	}	
	for(i=0;i<dev->m_nCurreMaxVideoChl;i++){
		vdev = &dev->video[i].vdev;
		video_unregister_device(vdev);
		v4l2_device_unregister(&dev->video[i].v4l2_dev);
	}
	//-----------------
	if(dev->wq)
	{
		destroy_workqueue(dev->wq);
	}
	
	if(dev->auwq)
	{
		destroy_workqueue(dev->auwq);
	}
	dev->wq=NULL;
	dev->auwq=NULL;
    //free_irq(dev->pdev->irq, dev);

	iounmap(dev->info.mem[0].internal_addr);
	
	//pci_disable_device(pdev);
	if (dev->msix_enabled) 
	{		
			pci_disable_msix(pdev); 	
			dev->msix_enabled = 0; 
	}	
	else if (dev->msi_enabled)
	{
			pci_disable_msi(pdev);		
			dev->msi_enabled = 0;	
	}
	kfree(dev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
	//printk("hws_remove  Done\n");
}
//---------------------------------------	
static void CheckCardStatus(struct hws_pcie_dev *pdx)
{
	ULONG status;
	status = READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE+0*PCIE_BARADDROFSIZE));
	//DbgPrint("CheckCardStatus =%X",status);
	if((status&0x01) != 0x01)
	{
		//DbgPrint("CheckCardStatus =%X",status);
		InitVideoSys(pdx,1);
	}
	
}
static int CheckVideoCapture(struct hws_pcie_dev *pdx,int index)
{
	ULONG status;
	int enable;
	status = READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE+2*PCIE_BARADDROFSIZE));
	enable = (status >>index)&0x01;
	return enable;
}
static int CheckAudioCapture(struct hws_pcie_dev *pdx,int index)
{
	ULONG status;
	int enable;
	status = READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE+3*PCIE_BARADDROFSIZE));
	enable = (status >>index)&0x01;
	return enable;
}

static int  StartAudioCapture(struct hws_pcie_dev *pdx,int index)
{
	int j;
	
	if(pdx->m_bACapStarted[index]==1) 
	{
		if(CheckAudioCapture(pdx,index) ==0)
		{
			CheckCardStatus(pdx);
			EnableAudioCapture(pdx,index,1);
		}
		//DbgPrint("Re StartAudioCapture =%d",index);	
		return -1;
	}
	CheckCardStatus(pdx);
	pdx->m_bAudioRun[index] = 1;
	pdx->m_bAudioStop[index] = 0;
	pdx->m_nAudioBufferIndex[index] =0; 
	pdx->audio_data[index]=0;
	pdx->m_nRDAudioIndex[index] =0;
	for(j=0; j<MAX_AUDIO_QUEUE;j++)
	{
		pdx->m_AudioInfo[index].pStatusInfo[j].byLock = MEM_UNLOCK;
	}
	pdx->m_AudioInfo[index].dwisRuning =1;
	EnableAudioCapture(pdx,index,1);
	return 0;
}

static int StartVideoCapture(struct hws_pcie_dev *pdx,int index)
{
	int j;
	//unsigned long flags;
	if(pdx->m_bVCapStarted[index]==1) 
	{

		CheckCardStatus(pdx);
		if(CheckVideoCapture(pdx,index) ==0)
		{
			EnableVideoCapture(pdx,index,1);
		}
		return -1;
	}
	//--------------------
	CheckCardStatus(pdx);
	//--------------------
	//spin_lock_irqsave(&pdx->videoslock[index], flags);
	for (j = 0; j<MAX_VIDEO_QUEUE; j++)
	{
			pdx->m_pVCAPStatus[index][j].byLock = MEM_UNLOCK;
			pdx->m_pVCAPStatus[index][j].byPath = 2;
			
	}
	//spin_unlock_irqrestore(&pdx->videoslock[index], flags);
	//pdx->m_nVideoIndex[index] =0;
	pdx->m_VideoInfo[index].dwisRuning = 1;
	pdx->m_VideoInfo[index].m_nVideoIndex =0;
	pdx->m_nRDVideoIndex[index]=0;
	pdx->m_bChangeVideoSize[index] = 0;
	pdx->m_bVCapIntDone[index] = 1;
	pdx->m_pVideoEvent[index] = 1;
	pdx->m_nVideoBusy[index] =0;
	pdx->video_data[index]  =0;
	EnableVideoCapture(pdx,index,1);
	return 0;
}

static void StopVideoCapture(struct hws_pcie_dev *pdx,int index)
{
	//int inc=0;
	
	if(pdx->m_bVCapStarted[index] ==0) return;	
	//pdx->m_nVideoIndex[index] =0;
	pdx->m_VideoInfo[index].dwisRuning = 0;
	pdx->m_bVideoStop[index] = 1;
	pdx->m_pVideoEvent[index] = 0;
	pdx->m_bChangeVideoSize[index] = 0;
	#if 0
	while(1)
	{
		if(pdx->m_bVideoStop[index] ==0)
		{
			break;
		}
		inc++;
		if(inc >2000)
		{
			break;
		}
		msleep(10);
	}
	#endif 
	EnableVideoCapture(pdx,index,0);
	pdx->m_bVCapIntDone[index] = 0;
}
static void StopAudioCapture(struct hws_pcie_dev *pdx,int index)
{
	//int inc=0;
	if(pdx->m_bAudioRun[index] ==0) return;
	pdx->m_bAudioRun[index] = 0;
	pdx->m_bAudioStop[index] = 1;
	pdx->m_nAudioBufferIndex[index] =0;
	pdx->m_AudioInfo[index].dwisRuning =0;
	#if 0
	while(1)
	{
		if(pdx->m_bAudioStop[index] ==0)
		{
			break;
		}
		inc++;
		if(inc >2000)
		{
			break;
		}
		msleep(10);
	}
	#endif 
	EnableAudioCapture(pdx,index,0);
}
//-----------------------------


//-----------------------------
static int MemCopyVideoToSteam(struct hws_pcie_dev *pdx,int nDecoder)
	{
		int nIndex = -1;
		//int i=0 ;
		int status =-1;
		BYTE *bBuf = NULL;
		BYTE *bBuf1 = NULL;
		BYTE *pSrcBuf= NULL;
		BYTE *pDmaSrcBuf= NULL;
		BYTE *pSrcBuf1= NULL;
		BYTE *pDmaSrcBuf1= NULL;
		int dwSrcPitch;
		//int dwMaskPitch;
		int copysize;
		int copysize1;
		int nw,nh;
		int interlace;
		int mVideoBufIndex;
		//int halfsize;
		int *pMask;
		int  line_cnt=0;
		unsigned long flags;

		//---------------------
		nw = pdx->m_pVCAPStatus[nDecoder][0].dwWidth ;
		nh = pdx->m_pVCAPStatus[nDecoder][0].dwHeight;
		if((nw <0)||(nh<0)||(nw>MAX_VIDEO_HW_W)||(nh>MAX_VIDEO_HW_H))
		{
			return -1;
		}
		mVideoBufIndex = pdx->m_nVideoBufferIndex[nDecoder]; 
		interlace  = pdx->m_pVCAPStatus[nDecoder][0].dwinterlace;
		if(pdx->m_Device_SupportYV12 ==1)
		{
			dwSrcPitch = nw*12/8;
		}
		else
		{
			dwSrcPitch = nw*2;
		}
		if(mVideoBufIndex== 1)
		{
						
			pDmaSrcBuf = pdx->m_pbyVideoBuffer[0];
			pDmaSrcBuf1 = pdx->m_pbyVideoBuffer[1];
			//pci_dma_sync_single_for_cpu(pdx->pdev,pdx->m_pbyVideo_phys[0],pdx->m_MaxHWVideoBufferSize,2);
			//pci_dma_sync_single_for_cpu(pdx->pdev,pdx->m_pbyVideo_phys[1],pdx->m_MaxHWVideoBufferSize,2);
			dma_sync_single_for_cpu(&pdx->pdev->dev,pdx->m_pbyVideo_phys[0],pdx->m_MaxHWVideoBufferSize,2);
			dma_sync_single_for_cpu(&pdx->pdev->dev,pdx->m_pbyVideo_phys[1],pdx->m_MaxHWVideoBufferSize,2);
			
				
			copysize = pdx->m_format[0].HLAF_SIZE;
			copysize1 = pdx->m_format[1].HLAF_SIZE;
			line_cnt = copysize1/dwSrcPitch;
		}
		else
		{
			pDmaSrcBuf = pdx->m_pbyVideoBuffer[2];
			pDmaSrcBuf1 = pdx->m_pbyVideoBuffer[3];
			//pci_dma_sync_single_for_cpu(pdx->pdev,pdx->m_pbyVideo_phys[2],pdx->m_MaxHWVideoBufferSize,2);
			//pci_dma_sync_single_for_cpu(pdx->pdev,pdx->m_pbyVideo_phys[3],pdx->m_MaxHWVideoBufferSize,2);
			dma_sync_single_for_cpu(&pdx->pdev->dev,pdx->m_pbyVideo_phys[2],pdx->m_MaxHWVideoBufferSize,2);
			dma_sync_single_for_cpu(&pdx->pdev->dev,pdx->m_pbyVideo_phys[3],pdx->m_MaxHWVideoBufferSize,2);
			copysize = pdx->m_format[2].HLAF_SIZE;
			copysize1 = pdx->m_format[3].HLAF_SIZE;
			line_cnt = copysize1/dwSrcPitch;

		}
		pMask = (int*)(pDmaSrcBuf1+(copysize1-(line_cnt-1)*dwSrcPitch));
		if(*pMask == 0x55AAAA55)
		{
			//DbgPrint("########-*pMask- [%d]%X[%d-%d]\n",nDecoder, *pMask,nw,nh);
			//------------------------------
			if(pdx->m_nVideoHalfDone[nDecoder] == 1)
			{
				pdx->m_nVideoHalfDone[nDecoder] =0; 
			}
			//------------------------------
			return -1;
		}
		else					
		{							
			if(mVideoBufIndex== 0)							
			{								
				if(pdx->m_nVideoHalfDone[nDecoder]  ==0)
				{									
						//DbgPrint("X1:HLAF ########-*pMask- [%d] [%d]\n",nDecoder,mVideoBufIndex);										
						*pMask = 0x55AAAA55;
						return -1;								
				}
				else
				{
					pdx->m_nVideoHalfDone[nDecoder] =0; 
				}
			}
		}		
		//-------------------------------
		nIndex = -1;
		pSrcBuf = pDmaSrcBuf;
		pSrcBuf1 = pDmaSrcBuf1;
		bBuf =NULL;
		bBuf1 = NULL;
		if(pdx->m_VideoInfo[nDecoder].dwisRuning ==1)
		{
			//--------------			
			if(mVideoBufIndex== 1)
			{
				if(pdx->m_VideoInfo[nDecoder].pStatusInfo[pdx->m_VideoInfo[nDecoder].m_nVideoIndex].byLock== MEM_UNLOCK)
	
				{
						nIndex = pdx->m_VideoInfo[nDecoder].m_nVideoIndex;
						bBuf =	pdx->m_VideoInfo[nDecoder].m_pVideoBufData[nIndex];
						bBuf1 = pdx->m_VideoInfo[nDecoder].m_pVideoBufData1[nIndex];

				}
			}
			else
			{
					   nIndex = pdx->m_VideoInfo[nDecoder].m_nVideoIndex;
					   bBuf =  pdx->m_VideoInfo[nDecoder].m_pVideoBufData2[nIndex];
					   bBuf1 = pdx->m_VideoInfo[nDecoder].m_pVideoBufData3[nIndex];

			}
			if(nIndex== -1)
			{
							
					//if(pdx->m_VideoInfo[nDecoder].pStatusInfo[video_index][pdx->m_VideoInfo[nDecoder].m_nVideoIndex[video_index]].byField== 0)
				//{
					pdx->m_VideoInfo[nDecoder].pStatusInfo[pdx->m_VideoInfo[nDecoder].m_nVideoIndex].byLock= MEM_UNLOCK;
					nIndex = pdx->m_VideoInfo[nDecoder].m_nVideoIndex;
					bBuf =  pdx->m_VideoInfo[nDecoder].m_pVideoBufData[nIndex];
					bBuf1 = pdx->m_VideoInfo[nDecoder].m_pVideoBufData1[nIndex];
				//}
			}

			//-------------------
			if((nIndex!= -1)&& bBuf&&bBuf1)
			{
				memcpy(bBuf,pSrcBuf,copysize);
				memcpy(bBuf1,pSrcBuf1,copysize1);
				//----------------------
					if(mVideoBufIndex== 0)
					{
							status = 0;
							spin_lock_irqsave(&pdx->videoslock[nDecoder], flags);
							
							pdx->m_VideoInfo[nDecoder].m_nVideoIndex = nIndex+1;
							if(pdx->m_VideoInfo[nDecoder].m_nVideoIndex >= MAX_VIDEO_QUEUE)
							{
								pdx->m_VideoInfo[nDecoder].m_nVideoIndex =0;
							}
								pdx->m_VideoInfo[nDecoder].m_VideoBufferSize[2] = copysize;
								pdx->m_VideoInfo[nDecoder].m_VideoBufferSize[3] = copysize1;
								pdx->m_VideoInfo[nDecoder].pStatusInfo[nIndex].dwWidth = pdx->m_pVCAPStatus[nDecoder][0].dwWidth ; 
								pdx->m_VideoInfo[nDecoder].pStatusInfo[nIndex].dwHeight = pdx->m_pVCAPStatus[nDecoder][0].dwHeight;
								pdx->m_VideoInfo[nDecoder].pStatusInfo[nIndex].dwinterlace = interlace;
								pdx->m_VideoInfo[nDecoder].pStatusInfo[nIndex].byLock = MEM_LOCK;

							spin_unlock_irqrestore(&pdx->videoslock[nDecoder], flags);
					}
					else
					{
						pdx->m_VideoInfo[nDecoder].m_VideoBufferSize[0] = copysize;
						pdx->m_VideoInfo[nDecoder].m_VideoBufferSize[1] = copysize1;
						pdx->m_nVideoHalfDone[nDecoder] = 1; 

					}
					 
				}
				else
				{
					 //printk("No Buffer Write %d",nDecoder);
					 //queue_work(pdx->wq,&pdx->video[nDecoder].videowork);
					 pdx->m_nVideoHalfDone[nDecoder] =0; 
				}
		}
		*pMask = 0x55AAAA55;
		return status;
}
static int SetQuene(struct hws_pcie_dev  *pdx,int nDecoder)
	{
		int status =-1;
		//KLOCK_QUEUE_HANDLE  oldirql;
		//DbgPrint("SetQuene %d %d",nDecoder,pdx->m_bStartRun);
		if(!pdx->m_bStartRun)
		{
		  return -1 ;
		}
		//DbgPrint("SetQuene 2 %d %d",nDecoder,pdx->m_bRun[nDecoder]);
		if(!pdx->m_bVCapStarted[nDecoder])
		{
		  	if(pdx->m_bVideoStop[nDecoder] == 1)
		  	{
				pdx->m_bVideoStop[nDecoder] =0;
				//KeSetEvent(& pdx->m_pVideoExitEvent[nDecoder],IO_NO_INCREMENT,FALSE); 
				//DbgPrint("KeSetEvent Exit Event[%d]\n",nDecoder);
			}
		  
		  return -1 ;
		}
		pdx->m_nVideoBusy[nDecoder] = 1; 
		//-------------------------------
		//DbgPrint("SetQuene 3 %d %d",nDecoder,pdx->m_bVCapStarted[nDecoder]);
		if(pdx->m_bVCapStarted[nDecoder] == TRUE)
		{
			status = MemCopyVideoToSteam(pdx,nDecoder);	
		}
		pdx->m_nVideoBusy[nDecoder] = 0;
		return status;
}

//------------------------------------
int MemCopyAudioToSteam( struct hws_pcie_dev  *pdx,int dwAudioCh)
{
	int i=0;
	BYTE *bBuf = NULL;
	BYTE *pSrcBuf= NULL;
	int nIndex = -1;
	unsigned long flags;
	//int status =-1;
	//printk("MemCopyAudioToSteam =%d",dwAudioCh);
	if(pdx->m_nAudioBufferIndex[dwAudioCh]== 0)
	{
							
		pSrcBuf = pdx->m_pbyAudioBuffer[dwAudioCh]+pdx->m_dwAudioPTKSize;
	}
	else
	{
		pSrcBuf =  pdx->m_pbyAudioBuffer[dwAudioCh];
	}
	
	//-----------------------------------------------------
		nIndex = -1;
		if(pdx->m_AudioInfo[dwAudioCh].dwisRuning ==1)
		{
			for( i = pdx->m_AudioInfo[dwAudioCh].m_nAudioIndex;i<MAX_AUDIO_QUEUE;i++)
			{
				if(pdx->m_AudioInfo[dwAudioCh].pStatusInfo[i].byLock== MEM_UNLOCK)
				{
						nIndex =i;
						bBuf = pdx->m_AudioInfo[dwAudioCh].m_pAudioBufData[i];
						break;
				}
			}
			if(nIndex == -1)
			{
				for( i = 0 ;i<pdx->m_AudioInfo[dwAudioCh].m_nAudioIndex;i++)
				{
					if(pdx->m_AudioInfo[dwAudioCh].pStatusInfo[i].byLock== MEM_UNLOCK)
					{
						nIndex =i;
						bBuf = pdx->m_AudioInfo[dwAudioCh].m_pAudioBufData[i];
						break;
					}
				
				}
			}
			
			if((nIndex!= -1)&& bBuf)
			{

					//pci_dma_sync_single_for_cpu(pdx->pdev,pdx->m_pbyAudio_phys[dwAudioCh],MAX_AUDIO_CAP_SIZE,2);
					dma_sync_single_for_cpu(&pdx->pdev->dev,pdx->m_pbyAudio_phys[dwAudioCh],MAX_AUDIO_CAP_SIZE,2);
					memcpy(bBuf, pSrcBuf, pdx->m_dwAudioPTKSize);
					
					pdx->m_AudioInfo[dwAudioCh].m_nAudioIndex = nIndex+1;
					if(pdx->m_AudioInfo[dwAudioCh].m_nAudioIndex>= MAX_AUDIO_QUEUE)
					{
						pdx->m_AudioInfo[dwAudioCh].m_nAudioIndex =0;
					}
					spin_lock_irqsave(&pdx->audiolock[dwAudioCh], flags);
					pdx->m_AudioInfo[dwAudioCh].pStatusInfo[nIndex].dwLength = pdx->m_dwAudioPTKSize ;
			 		pdx->m_AudioInfo[dwAudioCh].pStatusInfo[nIndex].byLock = MEM_LOCK;
					spin_unlock_irqrestore(&pdx->audiolock[dwAudioCh], flags);
					//KeSetEvent(& pdx->m_AudioInfo[dwAudioCh].m_pAudioEvent[audio_index],IO_NO_INCREMENT,FALSE); 
					//printk("Set Audio Event %d\n",dwAudioCh);
					//pdx->audio[dwAudioCh].pos = pdx->m_dwAudioPTKSize;
					 //snd_pcm_period_elapsed(pdx->audio[dwAudioCh].substream);	
					 queue_work(pdx->auwq,&pdx->audio[dwAudioCh].audiowork);
					//pdx->m_AudioInfo[dwAudioCh].pStatusInfo[nIndex].byLock = MEM_UNLOCK;		
				
			}
			else
			{
				printk("No Audio Buffer Write %d",dwAudioCh);

			}
	}
   return 0;
}

int SetAudioQuene( struct hws_pcie_dev *pdx,int dwAudioCh)
{
	int status =-1;
	//int i;
	//BYTE *bBuf = NULL;
	//BYTE *pSrcBuf= NULL;
	//int nIndex = -1;
	//printk("SetAudioQuene =%d",dwAudioCh);
	if(!pdx->m_bACapStarted[dwAudioCh])
	{
		  return -1 ;
	}
	if(!pdx->m_bAudioRun[dwAudioCh])
	{
				if(pdx->m_bAudioStop[dwAudioCh] == 1)
				{
					pdx->m_bAudioStop[dwAudioCh] =0;
					//DbgPrint("DpcForIsr_Audio0 Exit Event[%d]\n",dwAudioCh);
				}
				pdx->m_nAudioBusy[dwAudioCh] =0;
				return status;
	}
	
	pdx->m_nAudioBusy[dwAudioCh]  = 1;

	status = MemCopyAudioToSteam(pdx,dwAudioCh);


	pdx->m_nAudioBusy[dwAudioCh] = 0;	


	return status;

}

static void DpcForIsr_Audio0(unsigned long data)
{
	    
		int index;
	 	struct hws_pcie_dev *pdx;
		//pdx = sys_dvrs_hw_pdx;
		pdx = (struct hws_pcie_dev *)data;
		//unsigned long *pdata = (unsigned long *)data;
		//curr_buf_index = *pdata;
		index =0;
		SetAudioQuene(pdx,index);
		
}
#if 0
static void DpcForIsr_Audio1(unsigned long data)
{
	    
		int index;
	 	struct hws_pcie_dev *pdx;
		//pdx = sys_dvrs_hw_pdx;
		pdx = (struct hws_pcie_dev *)data;
		//unsigned long *pdata = (unsigned long *)data;
		//curr_buf_index = *pdata;
		index =1;
		SetAudioQuene(pdx,index);
		
}
static void DpcForIsr_Audio2(unsigned long data)
{
	    
		int index;
	 	struct hws_pcie_dev *pdx;
		//pdx = sys_dvrs_hw_pdx;
		pdx = (struct hws_pcie_dev *)data;
		//unsigned long *pdata = (unsigned long *)data;
		//curr_buf_index = *pdata;
		index =2;
		SetAudioQuene(pdx,index);
		
}
static void DpcForIsr_Audio3(unsigned long data)
{
	    
		int index;
	 	struct hws_pcie_dev *pdx;
		//pdx = sys_dvrs_hw_pdx;
		pdx = (struct hws_pcie_dev *)data;
		//unsigned long *pdata = (unsigned long *)data;
		//curr_buf_index = *pdata;
		index =3;
		SetAudioQuene(pdx,index);
}
#endif 

static void DpcForIsr_Video0(unsigned long data)
	{
		int i = 0;
	    int ret;
		//int curr_buf_index;
	 	struct hws_pcie_dev *pdx;
		//pdx = sys_dvrs_hw_pdx;
		pdx = (struct hws_pcie_dev *)data;
		//unsigned long *pdata = (unsigned long *)data;
		//curr_buf_index = *pdata;
		//printk("DpcForIsr_Video0\n");
		ret = SetQuene(pdx,i);
		//printk("[%X] pdx->m_bVCapStarted[i]=%d  ret=%d\n", pdx->pdev->device,pdx->m_bVCapStarted[i],ret);
		if(ret != 0 )
		{
			return;

		}
		
		if(pdx->m_bVCapStarted[i] == TRUE)
		{
			//printk("pdx->m_bVCapIntDone[i] = %d\n", pdx->m_bVCapIntDone[i]);
			//printk("pdx->m_pVideoEvent[i] = %d\n", pdx->m_pVideoEvent[i]);
			
			if((pdx->m_bVCapIntDone[i] == TRUE) && pdx->m_pVideoEvent[i])
			{
				pdx->m_bVCapIntDone[i] = FALSE;
				//printk("pdx->m_bChangeVideoSize[i] = %d\n",pdx->m_bChangeVideoSize[i]);
				if((!pdx->m_bChangeVideoSize[i])&&(pdx->m_pVideoEvent[i])) 
				{
					
					 //pdx->wq_flag[i] = 1;
					 //wake_up_interruptible(&pdx->wq_video[i]);  
					 //printk("Set Event\n");
					 queue_work(pdx->wq,&pdx->video[i].videowork);
				}
				else
				{
					 pdx->m_bChangeVideoSize[i] = 0;
				}
				
			}
		}
	}
	#if 0
	static void DpcForIsr_Video1(unsigned long data)
	{
	
		int i = 1;
		int ret;
	    //int curr_buf_index;
	   struct hws_pcie_dev *pdx;
		pdx = (struct hws_pcie_dev *)data;
		//pdx = sys_dvrs_hw_pdx;
		
		//unsigned long *pdata = (unsigned long *)data;
		//curr_buf_index = *pdata;
		
		ret = SetQuene(pdx,i);
		if(ret != 0 ) 
		{
		
			return;
		}
	
		if(pdx->m_bVCapStarted[i] == TRUE)
		{
			
			if(pdx->m_bVCapIntDone[i] == TRUE && pdx->m_pVideoEvent[i])
			{
                pdx->m_bVCapIntDone[i] = FALSE;
				if(!pdx->m_bChangeVideoSize[i]) 
				{
					if((!pdx->m_bChangeVideoSize[i])&&(pdx->m_pVideoEvent[i])) 
					{
					  //pdx->wq_flag[i] = 1;
					  //wake_up_interruptible(&pdx->wq_video[i]); 
					   queue_work(pdx->wq,&pdx->video[i].videowork);
					}
				}
				else
				{
					pdx->m_bChangeVideoSize[i] = 0;
				}
			}
		}
	}
	
	static void DpcForIsr_Video2(unsigned long data)
	{
	
		int i = 2;
		int ret;
		//int curr_buf_index;
		struct hws_pcie_dev *pdx;
		//pdx = sys_dvrs_hw_pdx;
		pdx = (struct hws_pcie_dev *)data;
		//unsigned long *pdata = (unsigned long *)data;
		//curr_buf_index = *pdata;
		 ret = SetQuene(pdx,i);
		if(ret != 0 ) 
		{
			return;
		}
		
		if(pdx->m_bVCapStarted[i] == TRUE)
		{
			
			if(pdx->m_bVCapIntDone[i] == TRUE && pdx->m_pVideoEvent[i])
			{
                pdx->m_bVCapIntDone[i] = FALSE;
				if(!pdx->m_bChangeVideoSize[i]) 
				{
					if((!pdx->m_bChangeVideoSize[i])&&(pdx->m_pVideoEvent[i])) 
					{
					   //pdx->wq_flag[i] = 1;
					   //wake_up_interruptible(&pdx->wq_video[i]); 
					    queue_work(pdx->wq,&pdx->video[i].videowork);
					}
				}
				else
				{
					pdx->m_bChangeVideoSize[i] = 0;
				}
			}
		}

	}
	
	static void DpcForIsr_Video3(unsigned long data)
	{
	
		int i = 3;
		int ret;
		//int curr_buf_index;
		struct hws_pcie_dev *pdx;
		//pdx = sys_dvrs_hw_pdx;
		pdx = (struct hws_pcie_dev *)data;
		//unsigned long *pdata = (unsigned long *)data;
		//curr_buf_index = *pdata;
		//mutex_lock(&pdx->video_mutex[i]); 
		//printk("DpcForIsr_Video3 data = [%d]%d \n",i,curr_buf_index);
		
		ret = SetQuene(pdx,i);
		if(ret != 0 ) 
		{
			//spin_unlock(&pdx->video_lock[i]);
			//mutex_unlock(&pdx->video_mutex[i]);
			return;
		}
		
		if(pdx->m_bVCapStarted[i] == TRUE)
		{
			
			if(pdx->m_bVCapIntDone[i] == TRUE && pdx->m_pVideoEvent[i])
			{
                pdx->m_bVCapIntDone[i] = FALSE;
				if(!pdx->m_bChangeVideoSize[i]) 
				{
					if((!pdx->m_bChangeVideoSize[i])&&(pdx->m_pVideoEvent[i])) 
					{
					//KeSetEvent(pdx->m_pVideoEvent[i], 0, FALSE);
					 //printk("SetEvenT[%d]\n",i);
					 //kill_fasync (&hw_async_video3, SIGIO, POLL_IN);
					 //pdx->wq_flag[i] = 1;
					 //wake_up_interruptible(&pdx->wq_video[i]); 
					  queue_work(pdx->wq,&pdx->video[i].videowork);
					}
				}
				else
				{
					pdx->m_bChangeVideoSize[i] = 0;
				}
			}
		}
	//spin_unlock(&pdx->video_lock[i]);	
	//mutex_unlock(&pdx->video_mutex[i]);
		
}	
#endif
//-----------------------------
/* Interrupt handler. Read/modify/write the command register to disable
 * the interrupt. */
//static irqreturn_t irqhandler(int irq, struct uio_info *info)
static irqreturn_t irqhandler(int irq, void  *info)
{
	struct hws_pcie_dev *pdx = (struct hws_pcie_dev *)(info);
	//struct pci_dev *pdev = pdx->pdev;
	
	u32 dma_status;
	u32 Int_Value =0;
	u32 IntState;
	u32 tmp;
	u32 cnt;
			
			dma_status =  READ_REGISTER_ULONG(pdx,(u32)(CVBS_IN_BASE));
			//printk("dma_status %x\n", dma_status);
			if(((dma_status&0x04)==0x04)&&(dma_status !=0xffffffff))
			{
				IntState= READ_REGISTER_ULONG(pdx,(u32)(CVBS_IN_BASE + 1 * PCIE_BARADDROFSIZE));
				if(IntState>0)
				{
					for(cnt =0; cnt <100; cnt ++)				
					{
					if(IntState == 0) break;
					if((IntState&0x01) == 0x01) // CH0  done
					{
						pdx->m_bVCapIntDone[0] = 1;
					
					Int_Value +=  0x01;
					if(pdx->m_nVideoBusy[0] ==0  )
					{
						tmp = (READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE + (32+0) * PCIE_BARADDROFSIZE)))&0x01;
						if(pdx->video_data[0] != tmp)
						{
							pdx->video_data[0]= tmp;
							pdx->m_nVideoBufferIndex[0]  = tmp;
							tasklet_schedule(&pdx->dpc_video_tasklet[0]);  // tasklet_hi_schedule
							//printk("Set OnInterrupt %x %d %d\n", 0,tmp,tmp2);
						}
					}
							
			 	 	}
					#if 0
			 		if((IntState&0x02) == 0x02) // CH1  done
			 		{
					//printk("OnInterrupt %x\n", 1);
					pdx->m_bVCapIntDone[1] = 1;
		
					Int_Value +=  0x02;
					if(pdx->m_nVideoBusy[1] ==0  )
					{
						 tmp = (READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE + (32+1) * PCIE_BARADDROFSIZE)))&0x01;
						if(pdx->video_data[1] != tmp)
						{
							pdx->m_nVideoBufferIndex[1] =  tmp;
							pdx->video_data[1]= pdx->m_nVideoBufferIndex[1];
							tasklet_schedule(&pdx->dpc_video_tasklet[1]);  
						}
					}
					
					}
				if((IntState&0x04) == 0x04) // CH2  done
				{
				//printk("OnInterrupt %x\n", 2);
				pdx->m_bVCapIntDone[2] = 1;
		
				Int_Value +=  0x04;
				if(pdx->m_nVideoBusy[2] ==0  )
				{
					 tmp = (READ_REGISTER_ULONG(pdx,(+ CVBS_IN_BASE + (32+2) * PCIE_BARADDROFSIZE)))&0x01;
					if(pdx->video_data[2] != tmp)
					{
						pdx->m_nVideoBufferIndex[2] = tmp;
						pdx->video_data[2]= pdx->m_nVideoBufferIndex[2];
						tasklet_schedule(&pdx->dpc_video_tasklet[2]);  
					}
				}
				}
				if((IntState &0x08) == 0x08) // CH1=3  done
				{
				//printk("OnInterrupt %x\n", 3);
				pdx->m_bVCapIntDone[3] = 1;
	
				Int_Value +=  0x08;
			
				if(pdx->m_nVideoBusy[3] ==0  )
				{
					 tmp = (READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE + (32+3) * PCIE_BARADDROFSIZE)))&0x01;
					if(pdx->video_data[3] != tmp)
					{
						pdx->m_nVideoBufferIndex[3] = tmp;
						pdx->video_data[3]= pdx->m_nVideoBufferIndex[3];
						//printk("OnInterrupt-%x [1] %d\n", 3,video_data[3]);
						tasklet_schedule(&pdx->dpc_video_tasklet[3]);  
					}
				}
				}
			#endif
			//-------
			//------------------------------
			
			if((IntState &0x100) == 0x100) // Audio ch0 done
			{
	
				Int_Value +=  0x100;
				//printk("OnInterrupt Audio  %x\n", 0);
				if(pdx->m_nAudioBusy[0] ==0 )
				{
					tmp = (READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE + (40+0) * PCIE_BARADDROFSIZE)))&0x01;
					pdx->m_nAudioBufferIndex[0] = tmp;
					pdx->audio_data[0]= pdx->m_nAudioBufferIndex[0];
					tasklet_schedule(&pdx->dpc_audio_tasklet[0]); 
				}
			}
			#if 0
			if((IntState &0x200) == 0x200) // Audio ch1 done
			{
	
				Int_Value +=  0x200;
				if(pdx->m_nAudioBusy[1] ==0 )
				{
					tmp = (READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE + (40+1) * PCIE_BARADDROFSIZE)))&0x01;
					pdx->m_nAudioBufferIndex[1] = tmp;
					pdx->audio_data[1]= pdx->m_nAudioBufferIndex[1];
					tasklet_schedule(&pdx->dpc_audio_tasklet[1]); 
				}
			}
			if((IntState &0x400) == 0x400) // Audio ch2 done
			{
	
				Int_Value +=  0x400;
				
				if(pdx->m_nAudioBusy[2] ==0 )
				{
					//DbgPrint("OnInterrupt Audio ch-%x pdx->m_nAudioBusy[2] =%d\n", 2,pdx->m_nAudioBusy[2]);
					tmp = (READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE + (40+2) * PCIE_BARADDROFSIZE)))&0x01;
					pdx->m_nAudioBufferIndex[2] = tmp;
					pdx->audio_data[2]= pdx->m_nAudioBufferIndex[2];
					tasklet_schedule(&pdx->dpc_audio_tasklet[2]); 
				}
			}
			if((IntState &0x800) == 0x800) // Audio ch3 done
			{
	
				Int_Value +=  0x800;
				//DbgPrint("OnInterrupt Audio ch-%x pdx->m_nAudioBusy[3] =%d\n", 3,pdx->m_nAudioBusy[3]);
				if(pdx->m_nAudioBusy[3] ==0 )
				{
					tmp = (READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE + (40+3) * PCIE_BARADDROFSIZE)))&0x01;
					pdx->m_nAudioBufferIndex[3] = tmp;
					pdx->audio_data[3]= pdx->m_nAudioBufferIndex[3];
					tasklet_schedule(&pdx->dpc_audio_tasklet[3]); 
				}
			}
			#endif 
			//--------------
			Int_Value = Int_Value&0x00ffffff;				
			WRITE_REGISTER_ULONG(pdx,(u32)(0x4000 + (PCIE_BARADDROFSIZE * 1)), Int_Value);
			IntState= READ_REGISTER_ULONG(pdx,(u32)(CVBS_IN_BASE + 1 * PCIE_BARADDROFSIZE));				
			if(IntState == 0) break;					
			}
					//printk("OnInterrupt IRQ_HANDLED  %X\n", pdev->device);
					return IRQ_HANDLED;
			}
			else
			{
					//printk("OnInterrupt[1] IRQ_NONE  %X\n", pdev->device);
					return IRQ_NONE;
			}
		}
		else
		{
				//printk("OnInterrupt[1] IRQ_NONE  %X\n", pdev->device);
				return IRQ_NONE;
		}

	//-------------------------------
	
}

static struct hws_pcie_dev *alloc_dev_instance(struct pci_dev *pdev)
{
	//int i;
	struct hws_pcie_dev *lro;

	BUG_ON(!pdev);

	/* allocate zeroed device book keeping structure */
	lro = kzalloc(sizeof(struct hws_pcie_dev), GFP_KERNEL);
	if (!lro) {
		printk("Could not kzalloc(hws_pcie_dev).\n");
		return NULL;
	}

	//lro->magic = MAGIC_DEVICE;
	//lro->config_bar_idx = -1;
	//lro->user_bar_idx = -1;
	//lro->bypass_bar_idx = -1;
	lro->irq_line = -1;

	/* create a device to driver reference */
	dev_set_drvdata(&pdev->dev, lro);
	/* create a driver to device reference */
	lro->pdev = pdev;
	//printk("probe() lro = 0x%p\n", lro);

	/* Set up data user IRQ data structures */
	//for (i = 0; i < MAX_USER_IRQ; i++) {
	//	lro->user_irq[i].lro = lro;
	//	spin_lock_init(&lro->user_irq[i].events_lock);
	//	init_waitqueue_head(&lro->user_irq[i].events_wq);
	//}

	return lro;
}


static void SetDMAAddress(struct hws_pcie_dev *pdx)
{
	//-------------------------------------

	u32 Addrmsk;
	u32 AddrLowmsk;
	//u32 AddrPageSize;
	//u32 Addr2PageSize;
	u32 PhyAddr_A_Low;
	u32 PhyAddr_A_High;
	
	//u32 PhyAddr_A_Low2;
	//u32 PhyAddr_A_High2;
	//u32 PCI_Addr2;
	
	u32 PCI_Addr;
	//u32 AVALON_Addr;
	u32 cnt;
	//u64 m_tmp64cnt = 0;
	//u32 RDAvalon = 0;
	//u32 m_AddreeSpace = 0;
	int i = 0;
	u32 m_ReadTmp;
	u32 m_ReadTmp2;
	//u32 m_ReadTmp3;
	//u32 m_ReadTmp4;
	DWORD halfframeLength=0;
	//DWORD m_Valude;
	PhyAddr_A_High = 0;
	PhyAddr_A_Low=0;
	PCI_Addr =0;
    
	
	//------------------------------------------ // re write dma register 

	Addrmsk = PCI_E_BAR_ADD_MASK;
	AddrLowmsk = PCI_E_BAR_ADD_LOWMASK;

	//printk("[MV]1DispatchCreate :Addrmsk = %X  AddrPageSize =%X Addr2PageSize =%X \n", Addrmsk, AddrPageSize, Addr2PageSize);
	
	cnt = 0x208;  // Table address
	for (i = 0; i< pdx->m_nMaxChl; i++)
	{
		//printk("[MV] pdx->m_pbyVideoBuffer[%d]=%x\n", i, pdx->m_pbyVideoBuffer[i]);
		if (pdx->m_pbyVideoBuffer[i])
		{
				PhyAddr_A_Low = pdx->m_dwVideoBuffer[i];
				PhyAddr_A_High = pdx->m_dwVideoHighBuffer[i];
				
				PCI_Addr = (PhyAddr_A_Low&AddrLowmsk);
				PhyAddr_A_Low = (PhyAddr_A_Low&Addrmsk);
				
				
				//printk("[MV]1-pdx->m_dwVideoBuffer[%d]-%X\n",i,pdx->m_dwVideoBuffer[i]);
				//-------------------------------------------------------------------------------
				WRITE_REGISTER_ULONG(pdx,(PCI_ADDR_TABLE_BASE + cnt),PhyAddr_A_High);
				WRITE_REGISTER_ULONG(pdx,(PCI_ADDR_TABLE_BASE + cnt+PCIE_BARADDROFSIZE),PhyAddr_A_Low);  //Entry 0
				//----------------------------------------
				m_ReadTmp =  READ_REGISTER_ULONG(pdx,(PCI_ADDR_TABLE_BASE + cnt));
				m_ReadTmp2 = READ_REGISTER_ULONG(pdx,(PCI_ADDR_TABLE_BASE + cnt+PCIE_BARADDROFSIZE));
				//printk("[MV]1-PCI_Addr[%d] :PhyAddr_A_Low  %X=%X  PhyAddr_A_High %X=%X\n", i, PhyAddr_A_Low, m_ReadTmp2, PhyAddr_A_High, m_ReadTmp);
			   	

				//--------------------------
				WRITE_REGISTER_ULONG(pdx,( CBVS_IN_BUF_BASE + (i*PCIE_BARADDROFSIZE)), ((i+1)*PCIEBAR_AXI_BASE)+PCI_Addr); //Buffer 1 address
				halfframeLength = pdx->m_format[i].HLAF_SIZE/16;
				WRITE_REGISTER_ULONG(pdx,( CBVS_IN_BUF_BASE2 + (i*PCIE_BARADDROFSIZE)),halfframeLength); //Buffer 1 address

				
				m_ReadTmp =  READ_REGISTER_ULONG(pdx,(  CBVS_IN_BUF_BASE + (i*PCIE_BARADDROFSIZE)));
				m_ReadTmp2 = READ_REGISTER_ULONG(pdx,(  CBVS_IN_BUF_BASE2 + (i*PCIE_BARADDROFSIZE)));
				//printk("[MV]1-Avalone [X64]BUF[%d]:BUF1=%X  BUF2=%X\n", i,  m_ReadTmp,  m_ReadTmp2);
			
				//---------------------------
				
		}
		cnt +=8;	
		#if 1
		if(pdx->m_pbyAudioBuffer[i])
		{
				PhyAddr_A_Low = pdx->m_dwAudioBuffer[i];
				PhyAddr_A_High = pdx->m_dwAudioBufferHigh[i];
				PCI_Addr = (PhyAddr_A_Low&AddrLowmsk);
				PhyAddr_A_Low = (PhyAddr_A_Low&Addrmsk);
				//printk("[X1]Audio:PCI_Addr =%X\n",PCI_Addr);
				//printk("[X1]Audio:-------- - LOW=%X  HIGH =%X\n",pdx->m_dwAudioBuffer[i],pdx->m_dwAudioBufferHigh[i]);
				WRITE_REGISTER_ULONG(pdx,(CBVS_IN_BUF_BASE + ((8+i)*PCIE_BARADDROFSIZE)), ((i+1)*PCIEBAR_AXI_BASE+PCI_Addr)); //Buffer 1 address
				m_ReadTmp = READ_REGISTER_ULONG(pdx,(CBVS_IN_BUF_BASE + ((8+i)*PCIE_BARADDROFSIZE)));
		    	//printk("[X1]Audio:[%d] :--------BUF1: %X=%X\n",i,(PCIEBAR_AXI_BASE+PCI_Addr),m_ReadTmp);
		}
		#endif 
	}
	WRITE_REGISTER_ULONG(pdx,INT_EN_REG_BASE, 0x3ffff); //enable PCI Interruput		
	//WRITE_REGISTER_ULONG(PCIEBR_EN_REG_BASE, 0xFFFFFFFF);	
	
}

//-----------------------------------
static void ChangeVideoSize(struct hws_pcie_dev *pdx,int ch,int w,int h,int interlace)
{
	int j;
	int halfframeLength[4];
	unsigned long flags;
	if(ch != 0) return;
	if(SetVideoFormteSize(pdx,ch,w,h) != 1)
	{
		return;		
	}
	spin_lock_irqsave(&pdx->videoslock[ch], flags);
	for (j = 0; j<MAX_VIDEO_QUEUE; j++)
	{
		pdx->m_pVCAPStatus[ch][j].dwWidth = w ;
		pdx->m_pVCAPStatus[ch][j].dwHeight = h;
		pdx->m_pVCAPStatus[ch][j].dwinterlace = interlace;
				
	}
	spin_unlock_irqrestore(&pdx->videoslock[ch], flags);
	halfframeLength[0] = pdx->m_format[0].HLAF_SIZE/16;
	halfframeLength[1] = pdx->m_format[1].HLAF_SIZE/16;
	halfframeLength[2] = pdx->m_format[2].HLAF_SIZE/16;
	halfframeLength[3] = pdx->m_format[3].HLAF_SIZE/16;
	WRITE_REGISTER_ULONG(pdx,(DWORD)(CBVS_IN_BUF_BASE2 + (0*PCIE_BARADDROFSIZE)), halfframeLength[0]); //Buffer 1 address
	WRITE_REGISTER_ULONG(pdx,(DWORD)(CBVS_IN_BUF_BASE2 + (1*PCIE_BARADDROFSIZE)), halfframeLength[1]); //Buffer 1 address
	WRITE_REGISTER_ULONG(pdx,(DWORD)(CBVS_IN_BUF_BASE2 + (2*PCIE_BARADDROFSIZE)), halfframeLength[2]); //Buffer 1 address
	WRITE_REGISTER_ULONG(pdx,(DWORD)(CBVS_IN_BUF_BASE2 + (3*PCIE_BARADDROFSIZE)), halfframeLength[3]); //Buffer 1 address

	
}

static int Get_Video_Status(struct hws_pcie_dev *pdx,unsigned int  ch)
{
	int value; 
	int res_w=0;
	int res_h=0;
//	int frame_rate=0;
	int active_video=1;
	int interlace=0;
	int offset;
	int no_video;
	value =  READ_REGISTER_ULONG(pdx,(DWORD)(CVBS_IN_BASE + (5*PCIE_BARADDROFSIZE)));
	//printk("[MV]check NoVideo End: [%d] %X\n",ch,value);
	active_video = ((value&0xFF)>>ch)&0x01;
	interlace = value>>8;
	interlace = ((interlace&0xFF)>>ch)&0x01;
	//printk("[MV][%d] active_video %d\n",ch,active_video);
	if(active_video >0)
	{
			offset = 90 + ch*2;
			//DbgPrint("[MV][%d] active_video %d\n",ch,interlace);
			value =  READ_REGISTER_ULONG(pdx,(DWORD)(CVBS_IN_BASE + (offset*PCIE_BARADDROFSIZE)));
			res_w = value&0xFFFF;
			res_h = (value>>16)&0xFFFF;
			if(pdx->m_DeviceHW_Version==0)
			{
				if(res_w>3840) res_w = 3840;
				if(res_h>2160) res_h = 2160;
			}
			if(((res_w <=MAX_VIDEO_HW_W) &&(res_h<=MAX_VIDEO_HW_H)&&(interlace==0))||((res_w <=MAX_VIDEO_HW_W) &&(res_h*2<=MAX_VIDEO_HW_H)&&(interlace==1)))
			{
				if((res_w !=pdx->m_pVCAPStatus[ch][0].dwWidth )||(res_h!= pdx->m_pVCAPStatus[ch][0].dwHeight)||(pdx->m_pVCAPStatus[ch][0].dwinterlace!=interlace))
				{
					ChangeVideoSize(pdx,ch,res_w,res_h,interlace);
				
				}
			}
			
		no_video = 0;
		//printk("[MV-X1]-[ch-%d]W=%d H=%d interlace =%d %dx%d \n",ch,res_w,res_h,interlace,pdx->m_pVCAPStatus[ch][0].dwWidth, pdx->m_pVCAPStatus[ch][0].dwHeight);	
	}
	else
	{
		no_video = 1;
	}
	
	return no_video;
	
}

static void CheckVideFmt (struct hws_pcie_dev *pdx)
{
	//PAGED_CODE();
	int i;
	//DWORD value;
	//DWORD SetData;
//	int ret=0;
	//int nNeed_ReInit =0;
//	unsigned char mark=1;


		for(i =0; i<pdx->m_nCurreMaxVideoChl;i++)
		{
			#if 0
			value =  ReadDevReg((DWORD)(CVBS_IN_BASE + ((91+i*2)*PCIE_BARADDROFSIZE)));
			m_brightness[i] = value&0xFF;
			m_contrast[i] =  (value>>8)&0xFF;
			m_hue[i] = (value>>16)&0xFF;
			m_saturation[i] = (value>>24)&0xFF;
			//DbgPrint("[MV]value[%d]= %X\n",i,value);
			if((g_contrast[i] != m_contrast[i])||(g_brightness[i] != m_brightness[i])||(g_saturation[i] != m_saturation[i])||(g_hue[i] != m_hue[i]))
			{
				
				//DbgPrint("[MV]m_brightness[%d]= %d %d \n",i,m_brightness[i],g_brightness[i]);
				//DbgPrint("[MV]m_contrast[%d]= %d %d\n",i,m_contrast[i],g_contrast[i]);
				//DbgPrint("[MV]m_hue[%d]= %d %d \n",i,m_hue[i],g_hue[i]);
				//DbgPrint("[MV]m_saturation[%d]= %d %d \n",i,m_saturation[i],g_saturation[i] );
				SetData = g_saturation[i]<<24;
				SetData  |=g_hue[i]<<16;
				SetData  |=g_contrast[i]<<8;
				SetData  |=g_brightness[i];
				//DbgPrint("[MV]value[%d]= %X %X\n",i,value,SetData);
				WriteDevReg((DWORD)(CVBS_IN_BASE + ((91+i*2)*PCIE_BARADDROFSIZE)), SetData);
				
			}
			#endif 
			pdx->m_curr_No_Video[i] = Get_Video_Status(pdx,i);
			//---------------
			if((pdx->m_curr_No_Video[i] ==0x1)&&(pdx->m_bVCapStarted[i]==TRUE))
			{
				 //printk("[MV]check NoVideo End: [%d]\n",i);
				 queue_work(pdx->wq,&pdx->video[i].videowork);	
			}
			//-----------------
			
		}
		
}

int MainKsThreadHandle(void *arg)
{
        int need_check=0;
		int i=0;
		struct hws_pcie_dev *pdx = (struct hws_pcie_dev *)(arg);
        while(1)
        {
              
			need_check=0;
			for(i=0; i<pdx->m_nMaxChl; i++)
			{
					if(pdx->m_bVCapStarted[i] ==1)
					{
						need_check = 1;
						break;
					}
			}
			if(need_check==1)
			{
				CheckVideFmt(pdx);
			}
            ssleep(1);
       		if(kthread_should_stop())
            {
                        break;
             }

        }
		//printk("MainKsThreadHandle Exit");
        return 0;
}
static void StartKSThread(struct hws_pcie_dev *pdx)
{
	    pdx->mMain_tsk = kthread_run(MainKsThreadHandle,(void*)pdx,"StartKSThread task"); 
	
}



//------------------------------


#ifndef arch_msi_check_device
int arch_msi_check_device(struct pci_dev *dev, int nvec, int type)
{
	return 0;
}
#endif

/* type = PCI_CAP_ID_MSI or PCI_CAP_ID_MSIX */
static int msi_msix_capable(struct pci_dev *dev, int type)
{
	struct pci_bus *bus;
	int ret;
    //printk("msi_msix_capable in \n");
	if (!dev || dev->no_msi)
	{
		 printk("msi_msix_capable no_msi exit \n");
		return 0;
	}

	for (bus = dev->bus; bus; bus = bus->parent)
	{
		if (bus->bus_flags & PCI_BUS_FLAGS_NO_MSI)
		{
			printk("msi_msix_capable PCI_BUS_FLAGS_NO_MSI \n");
			return 0;
		}
	}
	ret = arch_msi_check_device(dev, 1, type);
	if (ret)
	{
		return 0;
	}
	ret = pci_find_capability(dev, type);
	if (!ret)
	{
		printk("msi_msix_capable pci_find_capability =%d\n",ret);
		return 0;
	}

	return 1;
}


static int probe_scan_for_msi(struct hws_pcie_dev *lro, struct pci_dev *pdev)
{
	//int i;
	int rc = 0;
	//int req_nvec = MAX_NUM_ENGINES + MAX_USER_IRQ;

	//BUG_ON(!lro);
	//BUG_ON(!pdev);
	//if (msi_msix_capable(pdev, PCI_CAP_ID_MSIX)) {
	//		printk("Enabling MSI-X\n");
	//		for (i = 0; i < req_nvec; i++)
	//			lro->entry[i].entry = i;
	//
	//		rc = pci_enable_msix(pdev, lro->entry, req_nvec);
	//		if (rc < 0)
	//			printk("Couldn't enable MSI-X mode: rc = %d\n", rc);
	
	//		lro->msix_enabled = 1;
	//		lro->msi_enabled = 0;
	//	} 
	//else  

	if (msi_msix_capable(pdev, PCI_CAP_ID_MSI)) {
		/* enable message signalled interrupts */
		//printk("pci_enable_msi()\n");
		rc = pci_enable_msi(pdev);
		if (rc < 0)
		{
			printk("Couldn't enable MSI mode: rc = %d\n", rc);
		}
		lro->msi_enabled = 1;
		lro->msix_enabled = 0;
	} else {
		//printk("MSI/MSI-X not detected - using legacy interrupts\n");
		lro->msi_enabled = 0;
		lro->msix_enabled = 0;
	}

	return rc;
}



static int irq_setup(struct hws_pcie_dev *lro, struct pci_dev *pdev)
{
	int rc = 0;
	u32 irq_flag;
	u8 val;
	//void *reg;
	//u32 w;

	//BUG_ON(!lro);

	//if (lro->msix_enabled) {
	//	rc = msix_irq_setup(lro);
	//} 
	//else 
	{
		if (!lro->msi_enabled){
			pci_read_config_byte(pdev, PCI_INTERRUPT_PIN, &val);
			//printk("Legacy Interrupt register value = %d\n", val);
		}
		//irq_flag = lro->msi_enabled ? 0 : IRQF_SHARED;
		irq_flag = lro->msi_enabled ? IRQF_SHARED:0;
		//irq_flag = IRQF_SHARED;
		
		rc = request_irq(pdev->irq, irqhandler, irq_flag, pci_name(pdev), lro); // IRQF_TRIGGER_HIGH 
		if (rc)
		{
			//printk("Couldn't use IRQ#%d, rc=%d\n", pdev->irq, rc);
		}
		else
		{
			lro->irq_line = (int)pdev->irq;
			//printk("Using IRQ#%d with  MSI_EN=%d \n", pdev->irq,lro->msi_enabled);
		}
	}

	return rc;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
static void enable_pcie_relaxed_ordering(struct pci_dev *dev)
{
	pcie_capability_set_word(dev, PCI_EXP_DEVCTL, PCI_EXP_DEVCTL_RELAX_EN);
}
#else
static void __devinit enable_pcie_relaxed_ordering(struct pci_dev *dev)
{
	u16 v;
	int pos;

	pos = pci_pcie_cap(dev);
	if (pos > 0) {
		pci_read_config_word(dev, pos + PCI_EXP_DEVCTL, &v);
		v |= PCI_EXP_DEVCTL_RELAX_EN;
		pci_write_config_word(dev, pos + PCI_EXP_DEVCTL, v);
	}
}
#endif
//--------------------------------------
static void InitVideoSys(struct hws_pcie_dev *pdx,int set)
{
	// init decoder 
	int i,j;
//	DWORD dwRest=0;
	DWORD m_Valude;
	if(pdx->m_bStartRun&&(set==0)) return;
	WRITE_REGISTER_ULONG(pdx,( CVBS_IN_BASE + (0 * PCIE_BARADDROFSIZE)), 0X00);
	SetDMAAddress(pdx);
	if(set ==0)
	{
		for(i=0; i<pdx->m_nMaxChl;i++)
		{
			for (j = 0; j<MAX_VIDEO_QUEUE; j++)
			{
				pdx->m_pVCAPStatus[i][j].byLock = MEM_UNLOCK;
				pdx->m_pVCAPStatus[i][j].byPath = 2;
				pdx->m_pVCAPStatus[i][j].byField = 0;
				pdx->m_pVCAPStatus[i][j].dwinterlace =0;
			}
			//pdx->m_nVideoIndex[i] =0;
			pdx->m_nAudioBufferIndex[i] =0; 
			EnableVideoCapture(pdx,i,0);
			EnableAudioCapture(pdx,i,0);
	
		}
	}

	WRITE_REGISTER_ULONG(pdx,INT_EN_REG_BASE, 0x3ffff);
	//start Run
	//---------------------------------------------
	WRITE_REGISTER_ULONG(pdx,CVBS_IN_BASE, 0x80000000);
	//DelayUs(500);	
    m_Valude= 0x00FFFFFF;
	m_Valude |= 0x80000000;		  
	WRITE_REGISTER_ULONG(pdx,CVBS_IN_BASE, m_Valude);
	WRITE_REGISTER_ULONG(pdx,(CVBS_IN_BASE + (0 * PCIE_BARADDROFSIZE)), 0X13);
	pdx->m_bStartRun = 1;
	//--------------------------------------------------
	
}

//-------------------------------------
static void SetHardWareInfo(struct hws_pcie_dev *pdx)
{
	switch (pdx->dwDeviceID)
	 {
		 default:
		 {
			   pdx->m_nCurreMaxVideoChl = 1;
			   pdx->m_nCurreMaxLineInChl =0;
			   pdx->m_MaxHWVideoBufferSize = MAX_MM_VIDEO_SIZE;
		   break;
		 }
	   }
	 //-----------------------
		if(pdx->m_Device_Version>121)
	   {
	   		pdx->m_DeviceHW_Version =1;
	   }
	   else
	   {
		   pdx->m_DeviceHW_Version =0;
	   }

}
static int ReadChipId(struct hws_pcie_dev *pdx)
{
	//  CCIR_PACKET      reg;
	//int Chip_id1 = 0;
	int ret=0;
	//int reg_vaule = 0;
	//int nResult;
	//------read Dvice Version
	ULONG m_dev_ver;
	ULONG m_tmpVersion;
	ULONG m_tmpHWKey;
	//ULONG m_OEM_code_data;
	m_dev_ver= READ_REGISTER_ULONG(pdx,CVBS_IN_BASE+(88*PCIE_BARADDROFSIZE));
  
		m_tmpVersion = m_dev_ver>>8;
		pdx->m_Device_Version =  (m_tmpVersion&0xFF);
		m_tmpVersion = m_dev_ver>>16;
		pdx->m_Device_SubVersion = (m_tmpVersion&0xFF);
		pdx->m_Device_SupportYV12 = ((m_dev_ver>>28)&0x0F);
		m_tmpHWKey =  m_dev_ver>>24;
		m_tmpHWKey = m_tmpHWKey&0x0F;
		pdx->m_Device_PortID = m_tmpHWKey&0x03;
		//n_VideoModle =	READ_REGISTER_ULONG(pdx,0x4000+(4*PCIE_BARADDROFSIZE));
		//n_VideoModle = (n_VideoModle>>8)&0xFF;
		//pdx->m_IsHDModel = 1;
	 	pdx->m_MaxHWVideoBufferSize = MAX_MM_VIDEO_SIZE;
	 	pdx->m_nMaxChl  = 4;
	 	pdx->m_bBufferAllocate = FALSE;
		pdx->mMain_tsk = NULL;
		pdx->m_dwAudioPTKSize = MAX_DMA_AUDIO_PK_SIZE; //128*16*4;
		pdx->m_bStartRun = 0;
		pdx->m_PciDeviceLost =0;
		
		WRITE_REGISTER_ULONG(pdx,CVBS_IN_BASE,0x0);
		//ssleep(100);
		WRITE_REGISTER_ULONG(pdx,CVBS_IN_BASE,0x10);
   		//ssleep(500);	
		//-------
		SetHardWareInfo(pdx);
		printk("************[HW]-[VIDV]=[%d]-[%d]-[%d] ************\n",  pdx->m_Device_Version, pdx->m_Device_SubVersion,pdx->m_Device_PortID );	
	return ret;

}

static int hws_probe(struct pci_dev *pdev, const struct pci_device_id *pci_id)
{
	struct hws_pcie_dev *gdev=NULL;
	int err = 0, ret = -ENODEV;
	//u8 val=0;
	//u32 m_dev_ver=0;
	//u32 m_dev_vid_ver=0;
	//u32 m_dev_supportYV12=0;
	//u32 m_Device_Version=0;
	//ULONG m_tmpHWKey;
	//ULONG m_tmpVersion;
	//u32 m_tmpVersion=0;
	//u32 m_Device_SubVersion=0;
	//u32 m_Device_SupportYV12=0;
	//ULONG n_VideoModle =0;
	//u64 *mem64_ptr;
	int j, i;
	//---------------------------
	//printk("hws_probe  probe\n");
	//------------------------
	gdev = alloc_dev_instance(pdev);
	//sys_dvrs_hw_pdx = gdev;
	gdev->pdev = pdev;
	
	gdev->dwDeviceID = gdev->pdev->device;
	gdev->dwVendorID = gdev->pdev->vendor;
	printk("MV360: Device =%X VID =%X\n",gdev->dwDeviceID,gdev->dwVendorID); 
	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "%s: pci_enable_device failed: %d\n",
				__func__, err);
		goto  err_alloc;
	}
	//printk("hws_probe  probe 2\n"); 
	#if 0
	if (pci_request_regions(pdev, "longtimetech"))
		goto err_disable;
	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) {
		printk(KERN_EMERG "fail pci_set_dma_mask\n");
       	goto err_release;
    }
	#endif 
	
	//printk("hws_probe  probe 3\n"); 
	enable_pcie_relaxed_ordering(pdev);
	//printk("hws_probe  probe 4\n"); 
	pci_set_master(pdev);
	//------------------------------------	
	ret = probe_scan_for_msi(gdev, pdev);	
	if (ret < 0)		
	  goto disable_msi;	
	//------------------------
	//printk("hws_probe  probe 5\n"); 
	/* known root complex's max read request sizes */
#ifdef CONFIG_ARCH_TI816X
	//dbg_init("TI816X RC detected: limit MaxReadReq size to 128 bytes.\n");
	pcie_set_readrq(pdev, 128);
#endif
 #if 0
	gdev->info.mem[0].addr = pci_resource_start(pdev, 0);
	if (!gdev->info.mem[0].addr)
		goto err_release;
  #endif 
  //printk("hws_probe  ioremap_nocache\n"); 
  #if 0
	gdev->info.mem[0].internal_addr = ioremap_nocache(pci_resource_start(pdev, 0), 
		pci_resource_len(pdev, 0));
  #else
        //gdev->info.mem[0].internal_addr = ioremap_cache(pci_resource_start(pdev, 0), 
  		//gdev->info.mem[0].internal_addr = ioremap_nocache(pci_resource_start(pdev, 0), 
		//pci_resource_len(pdev, 0));

		gdev->info.mem[0].internal_addr = ioremap(pci_resource_start(pdev, 0), 
		pci_resource_len(pdev, 0));
  
  #endif
  	gdev->wq=NULL;
	gdev->auwq=NULL;
	gdev->map_bar0_addr = (u32 *)gdev->info.mem[0].internal_addr;

	if (!gdev->info.mem[0].internal_addr)
		goto err_release;

	gdev->info.mem[0].size = pci_resource_len(pdev, 0);
	gdev->info.mem[0].memtype = UIO_MEM_PHYS;

  
	
	//printk(" pdev->irq = %d \n",pdev->irq); 
	ret = irq_setup(gdev, pdev);
	if (ret)
		goto err_register;

	//printk("pci_set_drvdata \n"); 
	pci_set_drvdata(pdev, gdev);
	//enable irq
	//enable_irq(gdev->info.irq);
	//------
	ReadChipId(gdev);
		//---------------
		for (i = 0; i<MAX_VID_CHANNELS; i++)
		{
			//gdev->m_nVideoIndex[i] =0;
			gdev->m_nRDVideoIndex[i] =0;
			gdev->m_bVCapIntDone[i] =0;
			gdev->m_nVideoBusy[i] = 0;
			gdev->m_bChangeVideoSize[i] =0;
			gdev->m_nVideoBufferIndex[i] = 0;
			gdev->m_nVideoHalfDone[i] =0;
			gdev->m_pVideoEvent[i] = 0;
			SetVideoFormteSize(gdev,i,1920,1080);
			gdev->m_bVCapStarted[i] = 0;
			gdev->m_bVideoStop[i]=0;
			gdev->video_data[i] =0;
			//----------------------

			gdev->m_pbyVideoBuffer[i] = NULL;
			gdev->m_VideoInfo[i].dwisRuning= 0;
			gdev->m_VideoInfo[i].m_nVideoIndex= 0;
			gdev->m_VideoInfo[i].m_pVideoScalerBuf = NULL;
			for (j = 0; j<MAX_VIDEO_QUEUE; j++)
			{
				gdev->m_pVCAPStatus[i][j].byLock = MEM_UNLOCK;
				gdev->m_pVCAPStatus[i][j].byField = 0;
				gdev->m_pVCAPStatus[i][j].byPath = 2;
				gdev->m_pVCAPStatus[i][j].dwWidth = 1920 ;
				gdev->m_pVCAPStatus[i][j].dwHeight = 1080;
				gdev->m_pVCAPStatus[i][j].dwinterlace =0;
				//gdev->m_pVideoData[i][j] = NULL;
				//------------------
				gdev->m_VideoInfo[i].m_pVideoBufData[j] = NULL;
				gdev->m_VideoInfo[i].m_pVideoBufData1[j] = NULL;
				gdev->m_VideoInfo[i].m_pVideoBufData2[j] = NULL;
				gdev->m_VideoInfo[i].m_pVideoBufData3[j] = NULL;
				gdev->m_VideoInfo[i].pStatusInfo[j].byLock= MEM_UNLOCK;
				//----------------
			}
			//--------audio
			gdev->m_pAudioEvent[i] = 0;
			gdev->m_bACapStarted[i]=0;
			gdev->m_bAudioRun[i] = 0;
			gdev->m_bAudioStop[i] = 0;
			gdev->m_nAudioBusy[i] = 0;
			gdev->m_nRDAudioIndex[i] =0;
			//sema_init(&gdev->sem_video[i],1);  
			//spin_lock_init(&gdev->video_lock[i]); 
			spin_lock_init(&gdev->videoslock[i]);
			spin_lock_init(&gdev->audiolock[i]);
			//mutex_init(&gdev->video_mutex[i]); 
			//init_waitqueue_head(&gdev->wq_video[i]);  
			//gdev->wq_flag[i]=0;
			gdev->m_AudioInfo[i].dwisRuning =0;
			gdev->m_AudioInfo[i].m_nAudioIndex =0;
			gdev->audio[i].resampled_buf =NULL;
			for(j=0; j<MAX_AUDIO_QUEUE;j++)
			{
				gdev->m_AudioInfo[i].m_pAudioBufData[j] =NULL;
				gdev->m_AudioInfo[i].pStatusInfo[j].byLock = MEM_UNLOCK;
				gdev->m_AudioInfo[i].m_pAudioBufData[j] = NULL;
			}
			//gdev->video[i].v4l2_dev = NULL;
		}
		//---------------------
	 	 tasklet_init(&gdev->dpc_video_tasklet[0],DpcForIsr_Video0,(unsigned long)gdev);
		 //tasklet_init(&gdev->dpc_video_tasklet[1],DpcForIsr_Video1,(unsigned long)gdev);
		 //tasklet_init(&gdev->dpc_video_tasklet[2],DpcForIsr_Video2,(unsigned long)gdev);
		 //tasklet_init(&gdev->dpc_video_tasklet[3],DpcForIsr_Video3,(unsigned long)gdev);

		 tasklet_init(&gdev->dpc_audio_tasklet[0],DpcForIsr_Audio0,(unsigned long)gdev);
		 //tasklet_init(&gdev->dpc_audio_tasklet[1],DpcForIsr_Audio1,(unsigned long)gdev);
		 //tasklet_init(&gdev->dpc_audio_tasklet[2],DpcForIsr_Audio2,(unsigned long)gdev);
		 //tasklet_init(&gdev->dpc_audio_tasklet[3],DpcForIsr_Audio3,(unsigned long)gdev);
		 
		//----------------------
	 	ret = DmaMemAllocPool(gdev);
		 if(ret !=0)
	  	{
			goto err_mem_alloc;
	   }
	   //SetDMAAddress(gdev);
	   InitVideoSys(gdev,0);
	   StartKSThread(gdev);
	 // just test
	 //StartVideoCapture(gdev,0);
	//-------------------
	//printk("hws_probe probe exit \n"); 
	//--------------------------------------
	//--------------------
	hws_adapters_init(gdev);
	gdev->wq =   create_singlethread_workqueue("hwsuhdx1");
	gdev->auwq = create_singlethread_workqueue("hwsuhdx1-audio");
	//----------------
	if( hws_video_register(gdev) )
		goto err_mem_alloc;
#if 1
		if(hws_audio_register(gdev))
		goto err_mem_alloc;
#endif	
	return 0;
err_mem_alloc:
	
		 gdev->m_bBufferAllocate = TRUE;
		 DmaMemFreePool(gdev);
		 gdev->m_bBufferAllocate = FALSE;
err_register:
		iounmap(gdev->info.mem[0].internal_addr);
		irq_teardown(gdev);
		kfree(gdev);
disable_msi:	
		if (gdev->msix_enabled) 
		{		
		pci_disable_msix(pdev); 	
		gdev->msix_enabled = 0; 
		}	
		else if (gdev->msi_enabled)
		{
			pci_disable_msi(pdev);		
			gdev->msi_enabled = 0;	
		}
err_release:
		pci_release_regions(pdev);
		pci_disable_device(pdev);
		return err;
err_alloc:
			kfree(gdev);
			
	return	-1;


}

MODULE_DEVICE_TABLE(pci, hws_pci_table);

static struct pci_driver hws_pci_driver = {
	.name        = KBUILD_MODNAME,
	.id_table    = hws_pci_table,
	.probe       = hws_probe,
	.remove      = hws_remove,
};

static __init int pcie_hws_init(void)
{

	return pci_register_driver(&hws_pci_driver);
}

static __exit void pcie_hws_exit(void)
{
	pci_unregister_driver(&hws_pci_driver);
}

module_init(pcie_hws_init);
module_exit(pcie_hws_exit);

MODULE_DESCRIPTION("HWS driver");
MODULE_AUTHOR("Sales <sales@avmatrix.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
