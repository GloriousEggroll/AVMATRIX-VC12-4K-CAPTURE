/*
 */

#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/iommu.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-vmalloc.h>
#include "hws.h"
#include "hws_reg.h"
#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/rawmidi.h>
#include <sound/initval.h>
#include <media/v4l2-ctrls.h>

static void hws_adapters_init(struct hws_pcie_dev *dev);
static void hws_get_video_param(struct hws_pcie_dev *dev,int index);
//static void StartDma(int index);
//static void StopDma(int index);
static int StartVideoCapture(struct hws_pcie_dev *pdx,int index);
static int StartAudioCapture(struct hws_pcie_dev *pdx,int index);
static void StopAudioCapture(struct hws_pcie_dev *pdx,int index);
static void StopVideoCapture(struct hws_pcie_dev *pdx,int index);
static void hws_video_quiesce(struct hws_pcie_dev *pdx, int index);
static void InitVideoSys(struct hws_pcie_dev *pdx,int set);
struct hwsvideo_buffer;
static void hws_complete_buffer(struct hwsvideo_buffer *buf, enum vb2_buffer_state state);
static void hws_snap_regs(struct hws_pcie_dev *pdx, char what, int en);
static void hws_fill_canary(struct hws_pcie_dev *pdx);
static void hws_check_canary(struct hws_pcie_dev *pdx);
static int Get_Video_Status(struct hws_pcie_dev *pdx,unsigned int  ch);
static bool hws_input_timeperframe(struct hws_pcie_dev *pdx, int ch, struct v4l2_fract *tpf);
static bool hws_hdcp_active(struct hws_pcie_dev *pdx, int ch);
static void WRITE_REGISTER_ULONG(struct hws_pcie_dev *pdx,u32 RegisterOffset,u32 Value);
static u32 READ_REGISTER_ULONG(struct hws_pcie_dev *pdx,u32 RegisterOffset);
static bool hws_any_queue_busy(struct hws_video *videodev);
static void hws_check_source_change(struct hws_video *videodev);
static bool hws_zc_wanted(struct hws_pcie_dev *pdx);
static const char *hws_zc_unavailable(struct hws_pcie_dev *pdx);
static bool zero_copy;	/* module parameter, defined with hws_zc_wanted() */

/*
 * Debug-only DMA checks, read at load: the canary in each DMA buffer's
 * unused gap, the register snapshots in debugfs regsnaps, and the stray-write
 * log. Off by default, since the scan reads up to 14 MiB every 250 ms.
 */
static bool debug_dma;
module_param(debug_dma, bool, 0444);
MODULE_PARM_DESC(debug_dma, "enable the DMA canary, register snapshots and debug knobs (default off)");
static void hws_zc_stop(struct hws_pcie_dev *pdx, bool card_stopped);
static void hws_unregister_video(struct hws_pcie_dev *dev);

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
#define MAKE_ENTRY( __vend, __chip, __subven, __subdev, __configptr) {	\
.vendor		= (__vend),					\
.device		= (__chip),					\
.subvendor	= (__subven),					\
.subdevice	= (__subdev),					\
.driver_data	= (unsigned long) (__configptr)			\
}
//MAKE_ENTRY(0x1F33, 0x9001, 0x1F33, 0x0008, NULL),
static const struct pci_device_id hws_pci_table[] = {
	MAKE_ENTRY(0x8888, 0x8581, 0x8888, 0x0007, NULL),
	MAKE_ENTRY(0x8888, 0x85A1, 0x8888, 0x0007, NULL),
	MAKE_ENTRY(0x1F33, 0x8581, 0x8888, 0x0007, NULL),
	MAKE_ENTRY(0x8888, 0x8591, 0x8888, 0x0007, NULL),
	{ }
};
//------------------
static const v4l2_model_timing_t support_videofmt[]= {
	[V4L2_MODEL_VIDEOFORMAT_1920X1080P60]	= V4L2_MODEL_TIMING(1920,1080,60,0),
	[V4L2_MODEL_VIDEOFORMAT_3840X2160P60]	 = V4L2_MODEL_TIMING(3840,2160,60,0),
	[V4L2_MODEL_VIDEOFORMAT_1280X720P60]	= V4L2_MODEL_TIMING(1280,720,60,0),
	[V4L2_MODEL_VIDEOFORMAT_720X480P60]		= V4L2_MODEL_TIMING(720,480,60,0),
	[V4L2_MODEL_VIDEOFORMAT_720X576P50]		= V4L2_MODEL_TIMING(720,576,50,0),
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
};

//-------------------------------------------

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
	/* unique per card, so apps can tell two cards apart */
	snprintf(cap->bus_info, sizeof(cap->bus_info), "PCI:%s", pci_name(dev->pdev));
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
			memcpy(f->description, pixfmt->name, strlen(pixfmt->name));
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
/*
 * The input as the card sees it now, read straight from the status registers
 * and clamped like Get_Video_Status(). Interlaced fields are line-doubled into
 * frames, so the height reported is the delivered frame height. False when
 * there is no signal or the size is unusable.
 */
static bool hws_read_input(struct hws_pcie_dev *pdx, int ch, int *w, int *h, int *interlace)
{
	u32 st, res;

	if (pdx->m_PciDeviceLost)
		return false;
	st = READ_REGISTER_ULONG(pdx, CVBS_IN_BASE + 5 * PCIE_BARADDROFSIZE);
	if (st == 0xFFFFFFFF || !((st >> ch) & 1))
		return false;
	*interlace = (st >> (8 + ch)) & 1;
	res = READ_REGISTER_ULONG(pdx, CVBS_IN_BASE + (90 + ch * 2) * PCIE_BARADDROFSIZE);
	*w = res & 0xFFFF;
	*h = (res >> 16) & 0xFFFF;
	if (*w > pdx->m_MaxInputWidth)
		*w = pdx->m_MaxInputWidth;
	if (pdx->m_DeviceHW_Version == 0 && *h > 2160)
		*h = 2160;
	if (*interlace)
		*h *= 2;
	return *w >= 320 && *h >= 240 && *w <= MAX_VIDEO_HW_W && *h <= MAX_VIDEO_HW_H && !(*w & 1);
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

/* A size the driver can deliver: the live input size, or a table size the CPU scaler produces. */
static bool hws_size_supported(struct hws_video *videodev, int w, int h)
{
	int nw, nh, il;

	if (Get_input_framesizeIndex(w, h))
		return true;
	return hws_read_input(videodev->dev, videodev->index, &nw, &nh, &il) && w == nw && h == nh;
}

static void hws_fill_pix(struct v4l2_pix_format *pix, const framegrabber_pixfmt_t *fmt)
{
	pix->field = V4L2_FIELD_NONE;
	pix->pixelformat = fmt->fourcc;
	pix->bytesperline = (pix->width * fmt->depth) >> 3;
	pix->sizeimage = pix->height * pix->bytesperline;
	pix->colorspace = V4L2_COLORSPACE_REC709;
	/*
	 * Measured: the card delivers full-range YCbCr (Y uses 1-254) with the
	 * Switch 2 on every RGB Range setting (stage 4.5), so say so instead of
	 * the default, which means limited range for YUV.
	 */
	pix->ycbcr_enc = V4L2_YCBCR_ENC_709;
	pix->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	pix->xfer_func = V4L2_XFER_FUNC_709;
	pix->priv = 0;
}

static int hws_vidioc_g_fmt_vid_cap(struct file *file, void *fh, struct v4l2_format *fmt)
{
	struct hws_video *videodev = video_drvdata(file);

	fmt->fmt.pix.width = videodev->current_out_width;
	fmt->fmt.pix.height = videodev->curren_out_height;
	hws_fill_pix(&fmt->fmt.pix, framegrabber_g_out_pixelfmt(videodev));
	return 0;
}

static int hws_vidioc_try_fmt_vid_cap(struct file *file, void *fh, struct v4l2_format *f)
{
	struct hws_video *videodev = video_drvdata(file);
	struct v4l2_pix_format *pix = &f->fmt.pix;
	const framegrabber_pixfmt_t *fmt;

	fmt = framegrabber_g_support_pixelfmt_by_fourcc(pix->pixelformat);
	if (!fmt)
		fmt = framegrabber_g_support_pixelfmt_by_fourcc(V4L2_PIX_FMT_YUYV);
	/* anything else, including 0 and -1 sizes, gets the current format */
	if (!hws_size_supported(videodev, pix->width, pix->height)) {
		pix->width = videodev->current_out_width;
		pix->height = videodev->curren_out_height;
	}
	hws_fill_pix(pix, fmt);
	return 0;
}
/*
 * All consumers share one output format, and the worker copies frames sized
 * by it into every consumer's buffers. Changing it while any consumer holds
 * buffers would make those copies overrun the smaller buffers.
 */
static bool hws_any_queue_busy(struct hws_video *videodev)
{
	struct hws_vfh_ctx *ctx;
	unsigned long flags;
	bool busy = false;

	spin_lock_irqsave(&videodev->consumers_lock, flags);
	list_for_each_entry(ctx, &videodev->consumers, node) {
		if (vb2_is_busy(&ctx->vbq)) {
			busy = true;
			break;
		}
	}
	spin_unlock_irqrestore(&videodev->consumers_lock, flags);
	return busy;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,struct v4l2_format *f)
{
	struct hws_video *videodev = video_drvdata(file);
	int err;
	unsigned long flags;
	struct hws_pcie_dev *pdx = videodev->dev;
	//printk( "%s()\n", __func__);
  err = hws_vidioc_try_fmt_vid_cap(file, priv, f);
    if (err)
        return err;

	if ((f->fmt.pix.width != videodev->current_out_width ||
	     f->fmt.pix.height != videodev->curren_out_height ||
	     f->fmt.pix.pixelformat != videodev->pixfmt) && hws_any_queue_busy(videodev))
		return -EBUSY;

	spin_lock_irqsave(&pdx->videoslock[videodev->index], flags);
	videodev->pixfmt     = f->fmt.pix.pixelformat;
	videodev->current_out_width      = f->fmt.pix.width;
	videodev->curren_out_height     = f->fmt.pix.height;
	spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
	return 0;
}

/*
 * The live input size comes first (it needs no scaling), then the table sizes
 * the CPU scaler can produce, without repeating the input size.
 */
static int hws_vidioc_enum_framesizes(struct file *file, void *fh, struct v4l2_frmsizeenum *fsize)
{
	struct hws_video *videodev = video_drvdata(file);
	struct hws_pcie_dev *pdx = videodev->dev;
	const framegrabber_pixfmt_t *pixfmt;
	int nw = 0, nh = 0, il, i;
	u32 idx = fsize->index;
	bool native;

	pixfmt = framegrabber_g_support_pixelfmt_by_fourcc(fsize->pixel_format);
	if (!pixfmt)
		return -EINVAL;
	native = hws_read_input(pdx, videodev->index, &nw, &nh, &il);
	if (native) {
		if (idx == 0) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = nw;
			fsize->discrete.height = nh;
			return 0;
		}
		idx--;
	}
	for (i = 0; i < V4L2_MODEL_VIDEOFORMAT_NUM; i++) {
		const struct frame_size *fs = &support_videofmt[i].frame_size;
		if (native && fs->width == nw && fs->height == nh)
			continue;
		if (fs->width > pdx->m_MaxInputWidth)
			continue;
		if (idx-- == 0) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = fs->width;
			fsize->discrete.height = fs->height;
			return 0;
		}
	}
	return -EINVAL;
}

static int hws_vidioc_enum_input(struct file *file, void *priv,struct v4l2_input *i)
{
	struct hws_video *videodev = video_drvdata(file);
	struct hws_pcie_dev *pdx = videodev->dev;
	int w, h, il;
	u32 st;

	if (i->index > 0)
		return -EINVAL;
	i->type = V4L2_INPUT_TYPE_CAMERA;
	strscpy(i->name, "HDMI", sizeof(i->name));
	i->std = 0;
	i->capabilities = V4L2_IN_CAP_DV_TIMINGS;
	i->status = 0;
	if (!hws_read_input(pdx, videodev->index, &w, &h, &il)) {
		st = pdx->m_PciDeviceLost ? 0 :
		     READ_REGISTER_ULONG(pdx, CVBS_IN_BASE + 5 * PCIE_BARADDROFSIZE);
		/* active but unusable (too large, odd width) reads as no sync */
		i->status = (st != 0xFFFFFFFF && ((st >> videodev->index) & 1)) ?
			    V4L2_IN_ST_NO_SYNC : V4L2_IN_ST_NO_SIGNAL;
	} else if (hws_hdcp_active(pdx, videodev->index)) {
		i->status = V4L2_IN_ST_NO_ACCESS;	/* HDCP: frames are delivered black */
	}
	return 0;
}

static int hws_vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	//struct hws_video *videodev = video_drvdata(file);
	int Index;
	Index = *i;
	//printk( "%s(%d)-index =%d\n", __func__,videodev->index,Index);

if(Index >0)
{
	return   -EINVAL;
}
else
{
	*i = 0;
}
return 0;
}

static int hws_vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
//printk( "%s(%d)\n", __func__,i);
return i ? -EINVAL : 0;
}

static inline struct hws_vfh_ctx *hws_ctx_from_file(struct file *file)
{
    return container_of(file->private_data, struct hws_vfh_ctx, fh);
}

/* B1 multi-consumer vb2 ops (per-file queue). Defined later in this file. */
static const struct vb2_ops hwspcie_video_multi_qops;

static int hws_open(struct file *file)
{
    struct hws_video *videodev = video_drvdata(file);
    struct hws_pcie_dev *pdx = videodev->dev;
    unsigned long flags;
    struct hws_vfh_ctx *ctx;
    struct vb2_queue *q;
    int ret;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;

    ctx->video = videodev;
    INIT_LIST_HEAD(&ctx->buf_queue);
    spin_lock_init(&ctx->qlock);
    mutex_init(&ctx->qmutex);
    ctx->streaming = false;
    atomic_set(&ctx->inflight, 0);
    init_waitqueue_head(&ctx->inflight_wq);
    ctx->decimate = 1;
    ctx->next_seq = 0;

    /* v4l2 file-handle */
    v4l2_fh_init(&ctx->fh, &videodev->vdev);
    file->private_data = &ctx->fh;
	#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,18,0))
	v4l2_fh_add(&ctx->fh);
	#else
	v4l2_fh_add(&ctx->fh, file);
	#endif

    /* per-file vb2 queue */
    q = &ctx->vbq;
    memset(q, 0, sizeof(*q));
    q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    q->io_modes = VB2_MMAP | VB2_USERPTR;	/* read() was never implemented */
    q->gfp_flags = GFP_KERNEL | __GFP_ZERO;//GFP_DMA32;
    q->drv_priv = ctx;
    q->buf_struct_size = sizeof(struct hwsvideo_buffer);
    q->ops = &hwspcie_video_multi_qops;
    q->mem_ops = &vb2_vmalloc_memops;
    q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC | V4L2_BUF_FLAG_TSTAMP_SRC_EOF;
    /*
     * Per-open lock for this queue only. vb2 releases it while a blocking
     * DQBUF waits (via wait_prepare/wait_finish before 7.0, by itself since),
     * which the pre-7.0 ops also need: they unlock q->lock and must not see
     * NULL.
     */
    q->lock = &ctx->qmutex;
    q->dev = &pdx->pdev->dev;

    ret = vb2_queue_init(q);
    if (ret) {
		#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,18,0))
		v4l2_fh_del(&ctx->fh);
		#else
		v4l2_fh_del(&ctx->fh, file);
		#endif
        v4l2_fh_exit(&ctx->fh);
        kfree(ctx);
        file->private_data = NULL;
        return ret;
    }
    /* the first app to open an idle device starts at the input's own size */
    if (videodev->fileindex == 0 && !atomic_read(&videodev->engine_users) &&
        !hws_any_queue_busy(videodev)) {
        int nw, nh, il;
        if (hws_read_input(pdx, videodev->index, &nw, &nh, &il)) {
            spin_lock_irqsave(&pdx->videoslock[videodev->index], flags);
            videodev->current_out_width = nw;
            videodev->curren_out_height = nh;
            spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
        }
    }
    spin_lock_irqsave(&pdx->videoslock[videodev->index], flags);
    videodev->fileindex++;
    spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);

    /* add to consumers list */
    spin_lock_irqsave(&videodev->consumers_lock, flags);
    list_add_tail(&ctx->node, &videodev->consumers);
    spin_unlock_irqrestore(&videodev->consumers_lock, flags);

    return 0;
}

static int hws_release(struct file *file)
{
    struct hws_vfh_ctx *ctx;
    struct hws_video *videodev;
    struct hws_pcie_dev *pdx;
    unsigned long flags;

    if (!file->private_data)
        return 0;

    ctx = hws_ctx_from_file(file);
    videodev = ctx->video;
    pdx = videodev->dev;

    /* remove from consumers */
    spin_lock_irqsave(&videodev->consumers_lock, flags);
    list_del(&ctx->node);
    spin_unlock_irqrestore(&videodev->consumers_lock, flags);

    /* release vb2 queue resources */
    mutex_lock(&ctx->qmutex);
    vb2_queue_release(&ctx->vbq);
    mutex_unlock(&ctx->qmutex);

    /*
     * Count the handle as closed only after its queue is gone. Otherwise a
     * new process can open while this one is closing, see no other handle,
     * reset the format to the input's size, and then get -EBUSY from S_FMT
     * because this leftover queue still holds buffers.
     */
    spin_lock_irqsave(&pdx->videoslock[videodev->index], flags);
    if (videodev->fileindex > 0)
        videodev->fileindex--;
    spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);

    /* v4l2 fh cleanup */
	#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,18,0))
	v4l2_fh_del(&ctx->fh);
	#else
	v4l2_fh_del(&ctx->fh, file);
	#endif
    v4l2_fh_exit(&ctx->fh);
    file->private_data = NULL;

    kfree(ctx);
    return 0;
}

//-------------------
/*
 * Picture controls, applied by the card. Register 91 + 2*ch holds
 * saturation, hue, contrast and brightness, one byte each (format from the
 * vendor's commented-out code; it reads 0x80808080 at power-up). The control
 * framework handles QUERYCTRL, G/S_(EXT_)CTRLS and control events.
 *
 * Writing the hue byte has no visible effect on HDMI input (tested on FPGA
 * version 122; the register family comes from the vendor's analog capture
 * designs), so there is no hue control and the byte stays at its neutral 128.
 */
#define HWS_PICTURE_REG(ch)	(CVBS_IN_BASE + (91 + 2 * (ch)) * PCIE_BARADDROFSIZE)

static int hws_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct hws_video *videodev = container_of(ctrl->handler, struct hws_video, ctrl_hdl);
	u32 v = ((u32)videodev->ctrl_saturation->val << 24) |
		((u32)HueDefault << 16) |
		((u32)videodev->ctrl_contrast->val << 8) |
		(u32)videodev->ctrl_brightness->val;

	/* the cluster sets all three together; write once per change */
	videodev->m_PictureReg = v;
	if (!videodev->dev->m_PciDeviceLost)
		WRITE_REGISTER_ULONG(videodev->dev, HWS_PICTURE_REG(videodev->index), v);
	return 0;
}

static const struct v4l2_ctrl_ops hws_ctrl_ops = {
	.s_ctrl = hws_s_ctrl,
};

static int hws_ctrls_init(struct hws_video *videodev)
{
	struct v4l2_ctrl_handler *hdl = &videodev->ctrl_hdl;

	v4l2_ctrl_handler_init(hdl, 4);
	videodev->ctrl_brightness = v4l2_ctrl_new_std(hdl, &hws_ctrl_ops, V4L2_CID_BRIGHTNESS,
		MIN_VAMP_BRIGHTNESS_UNITS, MAX_VAMP_BRIGHTNESS_UNITS, 1, BrightnessDefault);
	videodev->ctrl_contrast = v4l2_ctrl_new_std(hdl, &hws_ctrl_ops, V4L2_CID_CONTRAST,
		MIN_VAMP_CONTRAST_UNITS, MAX_VAMP_CONTRAST_UNITS, 1, ContrastDefault);
	videodev->ctrl_saturation = v4l2_ctrl_new_std(hdl, &hws_ctrl_ops, V4L2_CID_SATURATION,
		MIN_VAMP_SATURATION_UNITS, MAX_VAMP_SATURATION_UNITS, 1, SaturationDefault);
	/* HDMI input present, kept current by the status thread */
	videodev->ctrl_power = v4l2_ctrl_new_std(hdl, NULL, V4L2_CID_DV_RX_POWER_PRESENT,
						 0, 1, 0, videodev->src_signal);
	if (hdl->error) {
		int err = hdl->error;
		v4l2_ctrl_handler_free(hdl);
		return err;
	}
	/* one s_ctrl call writes the shared register for all three */
	v4l2_ctrl_cluster(3, &videodev->ctrl_brightness);
	videodev->vdev.ctrl_handler = hdl;
	return v4l2_ctrl_handler_setup(hdl);
}

#define HWS_MAX_DECIMATE 4

/* The input's time per frame: measured if possible, else the table's nominal rate. */
static void hws_input_tpf(struct hws_video *videodev, struct v4l2_fract *tpf)
{
	v4l2_model_timing_t *t;

	if (hws_input_timeperframe(videodev->dev, videodev->index, tpf))
		return;
	t = Get_input_framesizeIndex(videodev->current_out_width, videodev->curren_out_height);
	tpf->numerator = 1;
	tpf->denominator = t ? t->refresh_rate : 60;
}

/*
 * Rates are the input rate divided by 1..HWS_MAX_DECIMATE: the card always
 * captures at the input rate, and a consumer that asks for less gets every
 * Nth frame, which also saves the copy for the frames it skips.
 */
static int hws_vidioc_enum_frameintervals(struct file *file, void *fh,
					  struct v4l2_frmivalenum *fival)
{
	struct hws_video *videodev = video_drvdata(file);
	struct v4l2_fract tpf;

	if (fival->index >= HWS_MAX_DECIMATE)
		return -EINVAL;
	if (!framegrabber_g_support_pixelfmt_by_fourcc(fival->pixel_format))
		return -EINVAL;
	if (!hws_size_supported(videodev, fival->width, fival->height))
		return -EINVAL;
	hws_input_tpf(videodev, &tpf);
	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator = tpf.numerator * (fival->index + 1);
	fival->discrete.denominator = tpf.denominator;
	return 0;
}

int hws_vidioc_g_parm(struct file *file,void *fh, struct v4l2_streamparm *a)
{
	struct hws_video *videodev = video_drvdata(file);
	struct hws_vfh_ctx *ctx = hws_ctx_from_file(file);
	struct v4l2_fract tpf;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	hws_input_tpf(videodev, &tpf);
	memset(&a->parm.capture, 0, sizeof(a->parm.capture));
	a->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
	a->parm.capture.timeperframe.numerator = tpf.numerator * ctx->decimate;
	a->parm.capture.timeperframe.denominator = tpf.denominator;
	return 0;
}

int hws_vidioc_s_parm(struct file *file, void *fh,struct v4l2_streamparm *a)
{
	struct hws_video *videodev = video_drvdata(file);
	struct hws_vfh_ctx *ctx = hws_ctx_from_file(file);
	struct v4l2_fract *want = &a->parm.capture.timeperframe;
	struct v4l2_fract tpf;
	u64 want_ns, in_ns;
	u32 n;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	/* 0/x or x/0 means "no preference": keep the current rate (this used to divide by zero) */
	if (want->numerator && want->denominator) {
		hws_input_tpf(videodev, &tpf);
		want_ns = div_u64((u64)want->numerator * NSEC_PER_SEC, want->denominator);
		in_ns = div_u64((u64)tpf.numerator * NSEC_PER_SEC, tpf.denominator);
		n = in_ns ? (u32)div64_u64(want_ns + in_ns / 2, in_ns) : 1;
		ctx->decimate = clamp_t(u32, n, 1, HWS_MAX_DECIMATE);
	}
	return hws_vidioc_g_parm(file, fh, a);
}

/* What the card can lock to: HDMI 2.0 up to 4096x2160 (3840 if DMA buffers had to stay at 4 MiB). */
static const struct v4l2_dv_timings_cap hws_timings_cap = {
	.type = V4L2_DV_BT_656_1120,
	V4L2_INIT_BT_TIMINGS(640, 4096, 480, 2160, 13500000, 600000000,
		V4L2_DV_BT_STD_CEA861 | V4L2_DV_BT_STD_DMT | V4L2_DV_BT_STD_CVT | V4L2_DV_BT_STD_GTF,
		V4L2_DV_BT_CAP_PROGRESSIVE | V4L2_DV_BT_CAP_INTERLACED |
		V4L2_DV_BT_CAP_REDUCED_BLANKING | V4L2_DV_BT_CAP_CUSTOM)
};

/*
 * v4l2_*_dv_timings_cap filter: progressive timings whose size the driver can
 * deliver. Interlaced input is delivered line-doubled as progressive frames, so
 * listing interlaced timings would promise a field format the driver never
 * produces; the live input's own timings are accepted separately.
 */
static bool hws_timings_supported(const struct v4l2_dv_timings *t, void *handle)
{
	return !t->bt.interlaced && hws_size_supported(handle, t->bt.width, t->bt.height);
}

static u64 hws_tpf_ns(const struct v4l2_fract *f)
{
	return f->denominator ? div_u64((u64)f->numerator * NSEC_PER_SEC, f->denominator) : 0;
}

/*
 * Describe the current input as DV timings. The card reports only the active
 * size and interlacing, so take the standard preset with that size and the
 * closest frame rate; for a size no preset has, synthesize timings without
 * blanking whose pixel clock still gives the measured rate.
 */
static int hws_query_timings(struct hws_video *videodev, struct v4l2_dv_timings *t)
{
	struct hws_pcie_dev *pdx = videodev->dev;
	struct v4l2_fract tpf, ptpf;
	u64 want_ns, best_err = U64_MAX, err;
	int w, h, il, i, best = -1;
	u32 st;

	if (!hws_read_input(pdx, videodev->index, &w, &h, &il)) {
		st = pdx->m_PciDeviceLost ? 0 :
		     READ_REGISTER_ULONG(pdx, CVBS_IN_BASE + 5 * PCIE_BARADDROFSIZE);
		return (st != 0xFFFFFFFF && ((st >> videodev->index) & 1)) ? -ERANGE : -ENOLINK;
	}
	if (!hws_input_timeperframe(pdx, videodev->index, &tpf)) {
		tpf.numerator = 1;	/* not measured yet (rate is counted while capturing) */
		tpf.denominator = 60;
	}
	want_ns = hws_tpf_ns(&tpf);

	for (i = 0; v4l2_dv_timings_presets[i].bt.width; i++) {
		const struct v4l2_bt_timings *bt = &v4l2_dv_timings_presets[i].bt;
		if (bt->width != w || bt->height != h || !!bt->interlaced != !!il)
			continue;
		ptpf = v4l2_calc_timeperframe(&v4l2_dv_timings_presets[i]);
		err = hws_tpf_ns(&ptpf);
		err = err > want_ns ? err - want_ns : want_ns - err;
		if (err < best_err) {
			best_err = err;
			best = i;
		}
	}
	if (best >= 0 && best_err * 100 <= want_ns) {	/* within 1% */
		*t = v4l2_dv_timings_presets[best];
		return 0;
	}
	memset(t, 0, sizeof(*t));
	t->type = V4L2_DV_BT_656_1120;
	t->bt.width = w;
	t->bt.height = h;
	t->bt.interlaced = il ? V4L2_DV_INTERLACED : V4L2_DV_PROGRESSIVE;
	t->bt.pixelclock = div_u64((u64)w * h * tpf.denominator, tpf.numerator);
	return 0;
}

static int hws_query_dv_timings(struct file *file, void *fh, struct v4l2_dv_timings *t)
{
	return hws_query_timings(video_drvdata(file), t);
}

static int hws_g_dv_timings(struct file *file, void *fh, struct v4l2_dv_timings *t)
{
	struct hws_video *videodev = video_drvdata(file);

	if (!videodev->dv_timings.bt.width && hws_query_timings(videodev, &videodev->dv_timings))
		return -ENODATA;
	*t = videodev->dv_timings;
	return 0;
}

/*
 * The card follows its input, so these timings choose the output format: the
 * usual sequence is QUERY_DV_TIMINGS, S_DV_TIMINGS, G_FMT.
 */
static int hws_s_dv_timings(struct file *file, void *fh, struct v4l2_dv_timings *t)
{
	struct hws_video *videodev = video_drvdata(file);
	struct hws_pcie_dev *pdx = videodev->dev;
	struct v4l2_dv_timings live;
	unsigned long flags;

	/* the input's own timings always work, interlaced ones included (QUERY then S) */
	if (!(hws_query_timings(videodev, &live) == 0 && v4l2_match_dv_timings(t, &live, 0, false)) &&
	    !v4l2_valid_dv_timings(t, &hws_timings_cap, hws_timings_supported, videodev))
		return -EINVAL;
	if (v4l2_match_dv_timings(t, &videodev->dv_timings, 0, false))
		return 0;
	if (hws_any_queue_busy(videodev))
		return -EBUSY;
	videodev->dv_timings = *t;
	spin_lock_irqsave(&pdx->videoslock[videodev->index], flags);
	videodev->current_out_width = t->bt.width;
	videodev->curren_out_height = t->bt.height;
	spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);
	return 0;
}

static int hws_enum_dv_timings(struct file *file, void *fh, struct v4l2_enum_dv_timings *t)
{
	return v4l2_enum_dv_timings_cap(t, &hws_timings_cap, hws_timings_supported,
					video_drvdata(file));
}

static int hws_dv_timings_cap(struct file *file, void *fh, struct v4l2_dv_timings_cap *cap)
{
	*cap = hws_timings_cap;
	return 0;
}

static int hws_subscribe_event(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subscribe(fh, sub);
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subscribe_event(fh, sub);
	}
	return -EINVAL;
}

/*
 * Called by the status thread for every channel with an open file handle:
 * signal gained or lost, or the size or interlacing changed, raises
 * V4L2_EVENT_SOURCE_CHANGE so apps can renegotiate.
 */
static void hws_check_source_change(struct hws_video *videodev)
{
	static const struct v4l2_event ev = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION,
	};
	int w = 0, h = 0, il = 0;
	bool sig = hws_read_input(videodev->dev, videodev->index, &w, &h, &il);

	if (sig == videodev->src_signal && w == videodev->src_w &&
	    h == videodev->src_h && il == videodev->src_il)
		return;
	videodev->src_signal = sig;
	videodev->src_w = w;
	videodev->src_h = h;
	videodev->src_il = il;
	videodev->dv_timings.bt.width = 0;	/* G_DV_TIMINGS re-queries */
	v4l2_ctrl_s_ctrl(videodev->ctrl_power, sig);
	v4l2_event_queue(&videodev->vdev, &ev);
}

static int hws_log_status(struct file *file, void *fh)
{
	struct hws_video *videodev = video_drvdata(file);
	struct hws_pcie_dev *pdx = videodev->dev;
	struct v4l2_dv_timings t;
	struct v4l2_fract tpf;
	struct hws_vfh_ctx *ctx;
	unsigned long flags;
	int open = 0, streaming = 0;

	/*
	 * OBS calls LOG_STATUS on every read timeout (5 frame periods), which
	 * during a signal change is about 12 times a second, each a dozen
	 * kernel log lines. Print at most once every 2 s.
	 */
	static DEFINE_RATELIMIT_STATE(rs, 2 * HZ, 1);

	if (!__ratelimit(&rs))
		return 0;
	if (hws_query_timings(videodev, &t) == 0)
		v4l2_print_dv_timings(videodev->v4l2_dev.name, "input: ", &t, true);
	else
		v4l2_info(&videodev->v4l2_dev, "input: no usable signal\n");
	if (hws_input_timeperframe(pdx, videodev->index, &tpf))
		v4l2_info(&videodev->v4l2_dev, "input frame interval: %u/%u s\n",
			  tpf.numerator, tpf.denominator);
	v4l2_info(&videodev->v4l2_dev, "HDCP: %s\n",
		  hws_hdcp_active(pdx, videodev->index) ? "active (frames delivered black)" : "not active");
	v4l2_info(&videodev->v4l2_dev, "output: %dx%d, frames: %u, dropped: %u, DMA buffers: %d KiB\n",
		  videodev->current_out_width, videodev->curren_out_height,
		  pdx->m_FrameSeq[videodev->index], pdx->m_FramesDropped[videodev->index],
		  pdx->m_MaxHWVideoBufferSize / 1024);
	spin_lock_irqsave(&videodev->consumers_lock, flags);
	list_for_each_entry(ctx, &videodev->consumers, node) {
		open++;
		streaming += ctx->streaming;
		if (ctx->streaming)
			v4l2_info(&videodev->v4l2_dev, "consumer %d: every %u frame(s), delivered %u, no buffer queued %u\n",
				  open, ctx->decimate, ctx->delivered, ctx->missed);
	}
	spin_unlock_irqrestore(&videodev->consumers_lock, flags);
	v4l2_info(&videodev->v4l2_dev, "file handles: %d open, %d streaming\n", open, streaming);
	v4l2_ctrl_handler_log_status(&videodev->ctrl_hdl, videodev->v4l2_dev.name);
	return 0;
}

/*
 * Multi-consumer locking. Each open() owns a vb2 queue guarded by its own
 * ctx->qmutex, which is also that queue's q->lock, so vb2 releases it while a
 * blocking DQBUF waits. Buffer ioctls, poll and mmap touch only the caller's
 * queue. Every other ioctl changes or reads state shared by all consumers and
 * runs under videodev->ioctl_lock.
 *
 * This used to hold ioctl_lock across every ioctl while swapping vdev->queue to
 * the caller's queue. A blocking DQBUF then kept the device lock while it
 * waited, so every other ioctl on the device, including the one that would
 * have woken it, blocked forever (v4l2-compliance hangs in its blocking-wait
 * test).
 */
static bool hws_is_buffer_ioctl(unsigned int cmd)
{
	switch (cmd) {
	case VIDIOC_REQBUFS:
	case VIDIOC_CREATE_BUFS:
	case VIDIOC_QUERYBUF:
	case VIDIOC_PREPARE_BUF:
	case VIDIOC_QBUF:
	case VIDIOC_DQBUF:
	case VIDIOC_STREAMON:
	case VIDIOC_STREAMOFF:
	case VIDIOC_DQEVENT:		/* per file handle, and may block like DQBUF */
#ifdef VIDIOC_DQEVENT_TIME32
	case VIDIOC_DQEVENT_TIME32:
#endif
#ifdef VIDIOC_DQBUF_TIME32
	case VIDIOC_QUERYBUF_TIME32:
	case VIDIOC_QBUF_TIME32:
	case VIDIOC_DQBUF_TIME32:
	case VIDIOC_PREPARE_BUF_TIME32:
#endif
		return true;
	}
	return false;
}

static long hws_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct hws_vfh_ctx *ctx;
	long ret;

	if (!file->private_data)
		return -EINVAL;
	if (hws_is_buffer_ioctl(cmd))
		return video_ioctl2(file, cmd, arg);	/* takes ctx->qmutex itself */

	ctx = hws_ctx_from_file(file);
	mutex_lock(&ctx->video->ioctl_lock);
	ret = video_ioctl2(file, cmd, arg);
	mutex_unlock(&ctx->video->ioctl_lock);
	return ret;
}

static __poll_t hws_poll(struct file *file, struct poll_table_struct *wait)
{
	struct hws_vfh_ctx *ctx;
	__poll_t ret;

	if (!file->private_data)
		return EPOLLERR;
	ctx = hws_ctx_from_file(file);
	mutex_lock(&ctx->qmutex);
	ret = vb2_poll(&ctx->vbq, file, wait);
	mutex_unlock(&ctx->qmutex);
	return ret;
}

static int hws_mmap(struct file *file, struct vm_area_struct *vma)
{
	if (!file->private_data)
		return -EINVAL;
	return vb2_mmap(&hws_ctx_from_file(file)->vbq, vma);
}

/*
 * An idle queue's memory type is picked when buffers are requested: MMAP
 * buffers are dma-contig when zero-copy is possible (stage 4.4), so the card
 * can write into them; otherwise, and for USERPTR, vmalloc as before.
 */
static void hws_pick_mem_ops(struct hws_vfh_ctx *ctx, u32 memory)
{
	struct hws_pcie_dev *pdx = ctx->video->dev;
	const char *why;

	if (vb2_is_busy(&ctx->vbq))
		return;
	why = hws_zc_unavailable(pdx);
	if (memory == V4L2_MEMORY_MMAP && READ_ONCE(zero_copy) && why)
		dev_info_once(&pdx->pdev->dev, "zero_copy is on but unavailable: %s; using the copy path\n", why);
	ctx->vbq.mem_ops = memory == V4L2_MEMORY_MMAP && hws_zc_wanted(pdx) ?
			   &vb2_dma_contig_memops : &vb2_vmalloc_memops;
}

/* buffer ioctls: the caller's own queue, under its own lock */
static int hws_reqbufs(struct file *file, void *priv, struct v4l2_requestbuffers *p)
{
	struct hws_vfh_ctx *ctx = hws_ctx_from_file(file);
	int ret;
	mutex_lock(&ctx->qmutex);
	hws_pick_mem_ops(ctx, p->memory);
	ret = vb2_reqbufs(&ctx->vbq, p);
	mutex_unlock(&ctx->qmutex);
	return ret;
}

static int hws_create_bufs(struct file *file, void *priv, struct v4l2_create_buffers *p)
{
	struct hws_vfh_ctx *ctx = hws_ctx_from_file(file);
	int ret;
	mutex_lock(&ctx->qmutex);
	hws_pick_mem_ops(ctx, p->memory);
	ret = vb2_create_bufs(&ctx->vbq, p);
	mutex_unlock(&ctx->qmutex);
	return ret;
}

static int hws_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct hws_vfh_ctx *ctx = hws_ctx_from_file(file);
	int ret;
	mutex_lock(&ctx->qmutex);
	ret = vb2_querybuf(&ctx->vbq, p);
	mutex_unlock(&ctx->qmutex);
	return ret;
}

static int hws_prepare_buf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct hws_vfh_ctx *ctx = hws_ctx_from_file(file);
	int ret;
	mutex_lock(&ctx->qmutex);
	ret = vb2_prepare_buf(&ctx->vbq, ctx->fh.vdev->v4l2_dev->mdev, p);
	mutex_unlock(&ctx->qmutex);
	return ret;
}

static int hws_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct hws_vfh_ctx *ctx = hws_ctx_from_file(file);
	int ret;
	mutex_lock(&ctx->qmutex);
	ret = vb2_qbuf(&ctx->vbq, ctx->fh.vdev->v4l2_dev->mdev, p);
	mutex_unlock(&ctx->qmutex);
	return ret;
}

static int hws_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct hws_vfh_ctx *ctx = hws_ctx_from_file(file);
	int ret;
	mutex_lock(&ctx->qmutex);
	ret = vb2_dqbuf(&ctx->vbq, p, file->f_flags & O_NONBLOCK);
	mutex_unlock(&ctx->qmutex);
	return ret;
}

static int hws_streamon(struct file *file, void *priv, enum v4l2_buf_type type)
{
	struct hws_vfh_ctx *ctx = hws_ctx_from_file(file);
	int ret;
	mutex_lock(&ctx->qmutex);
	ret = vb2_streamon(&ctx->vbq, type);
	mutex_unlock(&ctx->qmutex);
	return ret;
}

static int hws_streamoff(struct file *file, void *priv, enum v4l2_buf_type type)
{
	struct hws_vfh_ctx *ctx = hws_ctx_from_file(file);
	int ret;
	mutex_lock(&ctx->qmutex);
	ret = vb2_streamoff(&ctx->vbq, type);
	mutex_unlock(&ctx->qmutex);
	return ret;
}

/* forward declaration */
static const struct vb2_ops hwspcie_video_multi_qops;
static const struct v4l2_file_operations hws_fops = {
    .owner          = THIS_MODULE,
    .open           = hws_open,
    .release        = hws_release,
    .poll           = hws_poll,
    .unlocked_ioctl = hws_unlocked_ioctl,
    .mmap           = hws_mmap,
};


static const struct v4l2_ioctl_ops hws_ioctl_fops = {
	.vidioc_querycap = hws_vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = hws_vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap = hws_vidioc_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap = hws_vidioc_try_fmt_vid_cap,
	.vidioc_reqbufs       = hws_reqbufs,
	.vidioc_prepare_buf   = hws_prepare_buf,
	.vidioc_create_bufs   = hws_create_bufs,
	.vidioc_querybuf      = hws_querybuf,
	.vidioc_qbuf          = hws_qbuf,
	.vidioc_dqbuf         = hws_dqbuf,
	.vidioc_streamon      = hws_streamon,
	.vidioc_streamoff     = hws_streamoff,
	.vidioc_enum_framesizes   	= hws_vidioc_enum_framesizes,
	.vidioc_enum_frameintervals = hws_vidioc_enum_frameintervals,
	.vidioc_g_parm 	   = hws_vidioc_g_parm,
	.vidioc_s_parm     = hws_vidioc_s_parm,

	.vidioc_enum_input = hws_vidioc_enum_input,
	.vidioc_g_input = hws_vidioc_g_input,
	.vidioc_s_input = hws_vidioc_s_input,
	.vidioc_query_dv_timings = hws_query_dv_timings,
	.vidioc_g_dv_timings = hws_g_dv_timings,
	.vidioc_s_dv_timings = hws_s_dv_timings,
	.vidioc_enum_dv_timings = hws_enum_dv_timings,
	.vidioc_dv_timings_cap = hws_dv_timings_cap,
	.vidioc_log_status = hws_log_status,
	.vidioc_subscribe_event = hws_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,

};

/*
 * B1 multi-consumer vb2 ops: q->drv_priv is struct hws_vfh_ctx
 */
static int hws_queue_setup_multi(struct vb2_queue *q,
               unsigned int *num_buffers, unsigned int *num_planes,
               unsigned int sizes[], struct device *alloc_devs[])
{
    struct hws_vfh_ctx *ctx = q->drv_priv;
    struct hws_video *videodev = ctx->video;
    struct hws_pcie_dev *pdx = videodev->dev;
    unsigned long flags;
    unsigned int size;

    spin_lock_irqsave(&pdx->videoslock[videodev->index], flags);
    size = 2 * videodev->current_out_width * videodev->curren_out_height;
    spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);

    if (*num_planes) {
        if (sizes[0] < size)
            return -EINVAL;
        return 0;
    }

    *num_planes = 1;
    sizes[0] = size;
    /* room for an over-long last quarter when the card writes the buffer itself */
    if (q->mem_ops == &vb2_dma_contig_memops)
        sizes[0] += HWS_ZC_SLACK;
    return 0;
}

static int hws_buffer_prepare_multi(struct vb2_buffer *vb)
{
    struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
    struct hwsvideo_buffer *buf = container_of(vbuf, struct hwsvideo_buffer, vb);
    struct hws_vfh_ctx *ctx = vb->vb2_queue->drv_priv;
    struct hws_video *videodev = ctx->video;
    struct hws_pcie_dev *pdx = videodev->dev;
    unsigned long flags;
    u32 size;

    spin_lock_irqsave(&pdx->videoslock[videodev->index], flags);
    size = 2 * videodev->current_out_width * videodev->curren_out_height;
    spin_unlock_irqrestore(&pdx->videoslock[videodev->index], flags);

    if (vb2_plane_size(vb, 0) < size)
        return -EINVAL;

    vb2_set_plane_payload(vb, 0, size);
    buf->mem = vb2_plane_vaddr(vb, 0);
    buf->dma = vb->vb2_queue->mem_ops == &vb2_dma_contig_memops ?
               vb2_dma_contig_plane_dma_addr(vb, 0) : 0;
    return 0;
}

static void hws_buffer_queue_multi(struct vb2_buffer *vb)
{
    struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
    struct hwsvideo_buffer *buf = container_of(vbuf, struct hwsvideo_buffer, vb);
    struct hws_vfh_ctx *ctx = vb->vb2_queue->drv_priv;
    unsigned long flags;

    spin_lock_irqsave(&ctx->qlock, flags);
    list_add_tail(&buf->queue, &ctx->buf_queue);
    spin_unlock_irqrestore(&ctx->qlock, flags);
}

static int hws_start_streaming_multi(struct vb2_queue *q, unsigned int count)
{
    struct hws_vfh_ctx *ctx = q->drv_priv;
    struct hws_video *videodev = ctx->video;

    ctx->next_seq = 0;
    ctx->streaming = true;

    /* Start hardware engine only once, on first streamer. engine_lock keeps a
     * start from one consumer from interleaving with a stop from another. */
    mutex_lock(&videodev->engine_lock);
    if (atomic_inc_return(&videodev->engine_users) == 1) {
        mutex_lock(&videodev->cap_lock);
        videodev->dev->m_FrameSeq[videodev->index] = 0;
        videodev->top_done = false;
        hws_zc_stop(videodev->dev, true);	/* the engine is off: nothing in flight */
        videodev->zc_last_ts = 0;
        videodev->zc_gap_prev = 0;
        /*
         * Program the DMA lengths for the live input now, not at the next
         * status poll: until then the card DMAs the frame with the old
         * lengths and runs past them (see HWS_REG_DMA_MAX_SIZE).
         */
        videodev->dev->m_curr_No_Video[videodev->index] =
            Get_Video_Status(videodev->dev, videodev->index);
        mutex_unlock(&videodev->cap_lock);
        StartVideoCapture(videodev->dev, videodev->index);
    }
    mutex_unlock(&videodev->engine_lock);
    return 0;
}

static void hws_stop_streaming_multi(struct vb2_queue *q)
{
    struct hws_vfh_ctx *ctx = q->drv_priv;
    struct hws_video *videodev = ctx->video;
    struct hwsvideo_buffer *buf;
    unsigned long flags;

    ctx->streaming = false;

    /* Return all pending buffers for this ctx */
    spin_lock_irqsave(&ctx->qlock, flags);
    while (!list_empty(&ctx->buf_queue)) {
        buf = list_first_entry(&ctx->buf_queue, struct hwsvideo_buffer, queue);
        list_del(&buf->queue);
        vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
    }
    spin_unlock_irqrestore(&ctx->qlock, flags);

    /*
     * vb2 requires every buffer back before stop_streaming returns; it then
     * frees the queue. A buffer holding half a frame is not on buf_queue:
     * take it back under cap_lock, so the IRQ thread is not copying into it.
     */
    mutex_lock(&videodev->cap_lock);
    if (ctx->held) {
        list_del(&ctx->held->queue);
        hws_complete_buffer(ctx->held, VB2_BUF_STATE_ERROR);
        ctx->held = NULL;
    }
    if (videodev->zc_ctx == ctx)
        hws_zc_stop(videodev->dev, false);
    mutex_unlock(&videodev->cap_lock);
    /*
     * Zero-copy buffers come back once the card has moved past them, at the
     * next half-frame interrupts. With no frames arriving (no signal) that
     * can't be shown, so stop the engine briefly and let them go.
     */
    if (!wait_event_timeout(ctx->inflight_wq, atomic_read(&ctx->inflight) == 0,
                            msecs_to_jiffies(250))) {
        mutex_lock(&videodev->engine_lock);
        hws_video_quiesce(videodev->dev, videodev->index);
        mutex_lock(&videodev->cap_lock);
        hws_zc_stop(videodev->dev, true);
        mutex_unlock(&videodev->cap_lock);
        if (atomic_read(&videodev->engine_users) > 1)
            StartVideoCapture(videodev->dev, videodev->index);
        mutex_unlock(&videodev->engine_lock);
        wait_event(ctx->inflight_wq, atomic_read(&ctx->inflight) == 0);
    }

    /* Stop engine when the last streamer goes away */
    mutex_lock(&videodev->engine_lock);
    if (atomic_dec_return(&videodev->engine_users) == 0)
        StopVideoCapture(videodev->dev, videodev->index);
    mutex_unlock(&videodev->engine_lock);
}

static const struct vb2_ops hwspcie_video_multi_qops = {
    .queue_setup    = hws_queue_setup_multi,
    .buf_prepare    = hws_buffer_prepare_multi,
    .buf_queue      = hws_buffer_queue_multi,
    #if (LINUX_VERSION_CODE < KERNEL_VERSION(7,0,0))
    .wait_prepare   = vb2_ops_wait_prepare,
    .wait_finish    = vb2_ops_wait_finish,
    #endif
    .start_streaming= hws_start_streaming_multi,
    .stop_streaming = hws_stop_streaming_multi,
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

/*
 * Silence while the card sends no audio. During a source mode change
 * (3-4 s on a PS5 resolution change), with no source, or with an HDMI
 * source that sends no audio, the card raises no audio interrupts, so the
 * ALSA pointer stands still. PipeWire then re-delivers its last few
 * buffers of the card's audio over and over (a loop of 4864-5120 frames),
 * heard as a buzz. Once no packet has arrived for HWS_AUD_GAP_NS, this
 * delivers whole packets of silence at 48 kHz from the last packet's time
 * until the card's packets resume.
 */
#define HWS_AUD_GAP_NS		(40 * NSEC_PER_MSEC)	/* packets come every 21.3 ms */
#define HWS_AUD_GAP_POLL_MS	5

static const u8 hws_silence[MAX_DMA_AUDIO_PK_SIZE];

/* With pdx->audiolock[ch] held. */
static void hws_audio_card_packet(struct hws_audio *drv)
{
	drv->last_pkt_ns = ktime_get_ns();
	drv->silence_frames = 0;
	drv->in_gap = false;
}

static void hws_audio_silence_work(struct work_struct *work)
{
	struct hws_audio *drv = container_of(to_delayed_work(work), struct hws_audio, silence_work);
	struct hws_pcie_dev *pdx = drv->dev;
	const u32 pkt_frames = MAX_DMA_AUDIO_PK_SIZE / (2 * 2);
	unsigned long flags;
	unsigned int next_ms = HWS_AUD_GAP_POLL_MS;
	u64 gap, due;

	spin_lock_irqsave(&pdx->audiolock[drv->index], flags);
	if (!READ_ONCE(drv->running)) {
		spin_unlock_irqrestore(&pdx->audiolock[drv->index], flags);
		return;
	}
	gap = ktime_get_ns() - drv->last_pkt_ns;
	if (gap > HWS_AUD_GAP_NS) {
		if (!drv->in_gap) {
			drv->in_gap = true;
			drv->silence_gaps++;
		}
		due = div_u64(gap * 48, NSEC_PER_MSEC);	/* frames at 48 kHz */
		while (drv->silence_frames + pkt_frames <= due) {
			_deliver_samples(drv, (void *)hws_silence, sizeof(hws_silence));
			drv->silence_frames += pkt_frames;
			drv->silence_pkts++;
		}
	} else {
		/* packets are flowing: look again when the next would be late */
		next_ms = div_u64(HWS_AUD_GAP_NS - gap, NSEC_PER_MSEC) + 1;
	}
	spin_unlock_irqrestore(&pdx->audiolock[drv->index], flags);
	queue_delayed_work(pdx->auwq, &drv->silence_work,
			   max_t(unsigned long, 1, msecs_to_jiffies(next_ms)));
}

void audio_data_process(struct work_struct *p_work)
{
	struct hws_audio *drv = container_of(p_work, struct hws_audio, audiowork);
	//struct snd_pcm_substream *substream = drv->substream;
	struct hws_pcie_dev *pdx = drv->dev;
	int nIndex;
	int i;
	bool deliver;
	//unsigned int frames;
	BYTE *bBuf = NULL;
	//int copysize =0;
	//int rev_size=0;
	int dwAudioCh;
	int aud_len;
	unsigned long flags;
	//int avail = 0;
	dwAudioCh = drv->index;
	//printk("audio_data_process %d\n",dwAudioCh);
	spin_lock_irqsave(&pdx->audiolock[dwAudioCh], flags);
	/*
	 * Deliver only into a running stream's buffer. A packet queued just
	 * before trigger STOP can run after hw_free has dropped the buffer, and
	 * after close the runtime is gone; such packets are dropped.
	 */
	deliver = READ_ONCE(drv->running) && drv->ring_size_byframes &&
			  drv->substream && drv->substream->runtime &&
			  drv->substream->runtime->dma_area;
	/*
	 * queue_work() does nothing if the work is already pending, so several
	 * IRQs can map to one run. Drain every filled packet here; delivering only
	 * one per run left packets waiting for the next IRQ, adding up to a
	 * packet of jitter to the ALSA pointer.
	 */
	for(;;)
	{
	nIndex = -1;
	bBuf = NULL;
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
	if((nIndex == -1)||(!bBuf))
		break;
	{
		//--- send data -------------
		if (deliver) {
			hws_audio_card_packet(drv);
			_deliver_samples(drv,bBuf, aud_len);
		}
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
	BYTE*pSrcTmp;
	BYTE*pDestTmp;
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

/*
 * Frame delivery. The card DMAs each frame as four quarters, one per DMA
 * buffer. When the half-frame toggle reads 1, buffers 0 and 1 (the top half)
 * are complete; when it reads 0, buffers 2 and 3 are, which ends the frame.
 * A half stays in place for about half a frame period before the card starts
 * overwriting it. The IRQ thread copies each half straight into one buffer
 * per streaming consumer, so each frame is copied once per consumer; only
 * interlaced input or a scaled output goes through a staging frame first.
 * Everything here runs under video->cap_lock.
 */

/* Register 8: the source is using HDCP on channel @ch (stage 4.1). */
static bool hws_hdcp_active(struct hws_pcie_dev *pdx, int ch)
{
	u32 v;

	if (pdx->m_PciDeviceLost)
		return false;
	v = READ_REGISTER_ULONG(pdx, HWS_REG_HDCP_STATUS);
	return v != 0xFFFFFFFF && ((v >> ch) & 1);	/* all ones: the card isn't answering */
}

/* the driver writes this word after reading a half; the card overwrites it when it refreshes the half */
#define HWS_HALF_MARK 0x55AAAA55

static u32 *hws_half_mark(struct hws_pcie_dev *pdx, int half, int pitch)
{
	int b = half ? 1 : 3;
	int len = pdx->m_format[b].HLAF_SIZE;

	return (u32 *)(pdx->m_pbyVideoBuffer[b] + (len - (len / pitch - 1) * pitch));
}

/* Complete a buffer taken by hws_hold_buffers() and drop its in-flight count. */
static void hws_complete_buffer(struct hwsvideo_buffer *buf, enum vb2_buffer_state state)
{
	struct hws_vfh_ctx *ctx = vb2_get_drv_priv(buf->vb.vb2_buf.vb2_queue);

	vb2_buffer_done(&buf->vb.vb2_buf, state);
	if (atomic_dec_and_test(&ctx->inflight))
		wake_up(&ctx->inflight_wq);
}

/*
 * Give each streaming consumer that wants frame @seq one of its queued
 * buffers, unless it still holds one from a frame that never completed. A
 * consumer that asked for a lower rate (S_PARM) skips frames. The buffers
 * go on video->frame_bufs. @skip, if set, gets the frame by zero-copy.
 */
static void hws_hold_buffers(struct hws_video *v, u32 seq, u32 out_size,
			     struct hws_vfh_ctx *skip)
{
	struct hws_vfh_ctx *ctx;
	struct hwsvideo_buffer *buf, *tmp;
	unsigned long flags, qflags;
	LIST_HEAD(too_small);

	spin_lock_irqsave(&v->consumers_lock, flags);
	list_for_each_entry(ctx, &v->consumers, node) {
		if (!ctx->streaming || ctx->held || ctx == skip)
			continue;
		if (seq < ctx->next_seq && ctx->next_seq - seq <= ctx->decimate)
			continue;
		spin_lock_irqsave(&ctx->qlock, qflags);
		buf = list_first_entry_or_null(&ctx->buf_queue, struct hwsvideo_buffer, queue);
		if (buf) {
			list_del(&buf->queue);
			atomic_inc(&ctx->inflight);
			ctx->next_seq = seq + ctx->decimate;
		} else {
			ctx->missed++;
		}
		spin_unlock_irqrestore(&ctx->qlock, qflags);
		if (!buf)
			continue;
		if (!buf->mem || vb2_plane_size(&buf->vb.vb2_buf, 0) < out_size) {
			list_add_tail(&buf->queue, &too_small);
			continue;
		}
		ctx->held = buf;
		list_add_tail(&buf->queue, &v->frame_bufs);
	}
	spin_unlock_irqrestore(&v->consumers_lock, flags);

	list_for_each_entry_safe(buf, tmp, &too_small, queue) {
		list_del(&buf->queue);
		hws_complete_buffer(buf, VB2_BUF_STATE_ERROR);
	}
}

/* Complete every buffer on video->frame_bufs as frame @seq. */
static void hws_finish_frame(struct hws_video *v, enum vb2_buffer_state state,
			     u64 ts, u32 seq, u32 out_size)
{
	struct hwsvideo_buffer *buf, *tmp;

	list_for_each_entry_safe(buf, tmp, &v->frame_bufs, queue) {
		struct hws_vfh_ctx *ctx = vb2_get_drv_priv(buf->vb.vb2_buf.vb2_queue);

		list_del(&buf->queue);
		ctx->held = NULL;
		buf->vb.vb2_buf.timestamp = ts;
		buf->vb.field = V4L2_FIELD_NONE;
		buf->vb.sequence = seq;
		if (state == VB2_BUF_STATE_DONE) {
			vb2_set_plane_payload(&buf->vb.vb2_buf, 0, out_size);
			ctx->delivered++;
		}
		hws_complete_buffer(buf, state);
	}
}

/*
 * Zero-copy (stage 4.4). Probe 4.4 showed that the card takes a new address
 * in registers 16-19 for every frame: a value written after a quarter pair's
 * half-done interrupt applies to that pair in the next frame. So one
 * consumer's buffer can receive the frame directly, and other consumers copy
 * from it. Only for a progressive input captured at its own size, without
 * HDCP, into an MMAP buffer allocated dma-contig through a translating IOMMU
 * whose quarters fall inside the card's address windows (the table is never
 * rewritten: entry 0 also maps the audio ring). Anything else uses the DMA
 * buffers and the copy path, as before.
 *
 * The card must never write a buffer after it is returned. A pair's next run
 * starts about one half-frame after its half-done interrupt, so a register
 * write within half of the shortest recent gap between half interrupts is in
 * time. A buffer is released once no pair points at it and, for a pair that
 * moved on late, after that pair's next half-done interrupt; a buffer touched
 * by a late write completes with an error.
 */
static bool zero_copy;
module_param(zero_copy, bool, 0644);
MODULE_PARM_DESC(zero_copy, "let the card write frames into an app's buffers (default off; needs FPGA 122 or later "
		 "and a translating IOMMU; takes effect for buffers allocated afterwards)");


static bool hws_iommu_translates(struct device *dev)
{
	struct iommu_domain *d = iommu_get_domain_for_dev(dev);

	if (!d)
		return false;
#ifdef IOMMU_DOMAIN_DMA_FQ
	if (d->type == IOMMU_DOMAIN_DMA_FQ)
		return true;
#endif
	return d->type == IOMMU_DOMAIN_DMA;
}

/*
 * Why zero-copy can't run on this card, or NULL if it can. Its safety rests
 * on register 9 capping an over-long run at the largest quarter plus 2 KiB
 * (HWS_ZC_SLACK), and hws_set_dma_max() programs register 9 only on FPGA
 * 122 and later. The reset value (4.25 MiB) would let a run end far past
 * an app's buffer.
 */
static const char *hws_zc_unavailable(struct hws_pcie_dev *pdx)
{
	if (!pdx->m_DeviceHW_Version || !pdx->m_MaxHWVideoBufferSize)
		return "needs FPGA 122 or later";
	if (pdx->pdev->vendor != 0x8888 || pdx->pdev->device != 0x8581)
		return "tested only on 8888:8581";
	if (!pdx->zc_iommu)
		return "IOMMU not translating";
	return NULL;
}

static bool hws_zc_wanted(struct hws_pcie_dev *pdx)
{
	return READ_ONCE(zero_copy) && !hws_zc_unavailable(pdx);
}

static u32 hws_zc_off(struct hws_pcie_dev *pdx, int q)
{
	u32 off = 0;
	int i;

	for (i = 0; i < q; i++)
		off += pdx->m_format[i].HLAF_SIZE;
	return off;
}

/* The card's address for quarter @q of @buf, or of its own DMA buffer if @buf is NULL. */
static u32 hws_zc_reg(struct hws_pcie_dev *pdx, struct hwsvideo_buffer *buf, int q)
{
	u64 a = buf ? buf->dma + hws_zc_off(pdx, q) : pdx->m_pbyVideo_phys[q];

	return (q + 1) * PCIEBAR_AXI_BASE + (u32)(a & PCI_E_BAR_ADD_LOWMASK);
}

/* the stale-half mark (HWS_HALF_MARK) for pair @p, at the place hws_half_mark() uses in a DMA buffer */
static u32 *hws_zc_mark(struct hws_pcie_dev *pdx, struct hwsvideo_buffer *buf, int p, int pitch)
{
	int q = p ? 3 : 1;
	int len = pdx->m_format[q].HLAF_SIZE;

	return (u32 *)((u8 *)buf->mem + hws_zc_off(pdx, q) + (len - (len / pitch - 1) * pitch));
}

/*
 * A run of any quarter can reach up to the largest quarter plus 2 KiB past
 * its start (register 9), so everything from the buffer's start to that far
 * past the last quarter must be inside the buffer and inside every quarter's
 * window: a run through window q past its end would land elsewhere.
 */
static bool hws_zc_buffer_ok(struct hws_pcie_dev *pdx, struct hwsvideo_buffer *buf)
{
	u32 max = 0, end;
	u64 first, last;
	int q;

	for (q = 0; q < 4; q++)
		max = max_t(u32, max, pdx->m_format[q].HLAF_SIZE);
	end = hws_zc_off(pdx, 3) + max + 2048;
	if (!buf->dma || !buf->mem || vb2_plane_size(&buf->vb.vb2_buf, 0) < end)
		return false;
	first = buf->dma & ~(u64)PCI_E_BAR_ADD_LOWMASK;
	last = (buf->dma + end - 1) & ~(u64)PCI_E_BAR_ADD_LOWMASK;
	for (q = 0; q < 4; q++) {
		u64 win = ((u64)pdx->m_dwVideoHighBuffer[q] << 32) | (pdx->m_dwVideoBuffer[q] & PCI_E_BAR_ADD_MASK);

		if (first != win || last != win)
			return false;
	}
	return true;
}

/* Complete every buffer on zc_release the card can no longer write. */
static void hws_zc_release(struct hws_video *v)
{
	struct hwsvideo_buffer *buf, *tmp;

	list_for_each_entry_safe(buf, tmp, &v->zc_release, queue) {
		if (buf == v->zc_prog[0] || buf == v->zc_prog[1] ||
		    (s32)(v->zc_ev[0] - buf->zc_free_after[0]) < 0 ||
		    (s32)(v->zc_ev[1] - buf->zc_free_after[1]) < 0)
			continue;
		list_del(&buf->queue);
		if (buf->zc_state == VB2_BUF_STATE_DONE)
			((struct hws_vfh_ctx *)vb2_get_drv_priv(buf->vb.vb2_buf.vb2_queue))->delivered++;
		hws_complete_buffer(buf, buf->zc_state);
	}
}

static void hws_zc_retire(struct hws_video *v, struct hwsvideo_buffer *buf, enum vb2_buffer_state state)
{
	buf->zc_state = state;
	list_add_tail(&buf->queue, &v->zc_release);
}

/*
 * Point quarter pair @p at @buf (NULL: the DMA buffers). @ts is the half-done
 * interrupt this follows and @margin how long after it a write is in time;
 * @ts 0 means a write at an arbitrary moment, never in time.
 */
static void hws_zc_program(struct hws_pcie_dev *pdx, int p, struct hwsvideo_buffer *buf,
			   u64 ts, u64 margin, int pitch)
{
	struct hws_video *v = &pdx->video[0];
	struct hwsvideo_buffer *old = v->zc_prog[p];
	bool in_time;
	int q;

	if (!buf && !old)
		return;		/* already on the DMA buffers */
	if (buf)
		*hws_zc_mark(pdx, buf, p, pitch) = HWS_HALF_MARK;
	for (q = 2 * p; q < 2 * p + 2; q++)
		WRITE_REGISTER_ULONG(pdx, CBVS_IN_BUF_BASE + q * PCIE_BARADDROFSIZE, hws_zc_reg(pdx, buf, q));
	in_time = ts && ktime_get_ns() - ts < margin;
	if (!in_time && ts)
		v->zc_late++;
	if (old) {
		old->zc_away_in[p] = in_time;
		old->zc_free_after[p] = v->zc_ev[p] + !in_time;
	}
	if (buf)
		buf->zc_in[p] = in_time;
	v->zc_prog[p] = buf;
}

/*
 * Take the zero-copy consumer's next buffer for the frame after this one, or
 * NULL to use the DMA buffers.
 */
static struct hwsvideo_buffer *hws_zc_pick(struct hws_pcie_dev *pdx, bool direct, int pitch, int w)
{
	struct hws_video *v = &pdx->video[0];
	struct hws_vfh_ctx *ctx, *found = NULL;
	struct hwsvideo_buffer *buf;
	unsigned long flags, qflags;

	if (!hws_zc_wanted(pdx) || v->zc_stable < 2 || !direct ||
	    pitch != w * 2 || hws_hdcp_active(pdx, 0))
		return NULL;
	spin_lock_irqsave(&v->consumers_lock, flags);
	list_for_each_entry(ctx, &v->consumers, node) {
		if (ctx->streaming && ctx->decimate == 1 && ctx->vbq.mem_ops == &vb2_dma_contig_memops &&
		    (!found || ctx == v->zc_ctx))
			found = ctx;
	}
	buf = NULL;
	if (found) {
		spin_lock_irqsave(&found->qlock, qflags);
		buf = list_first_entry_or_null(&found->buf_queue, struct hwsvideo_buffer, queue);
		if (buf && hws_zc_buffer_ok(pdx, buf)) {
			list_del(&buf->queue);
			atomic_inc(&found->inflight);
		} else {
			if (buf)
				v->zc_outside++;	/* stays queued for the copy path */
			buf = NULL;
		}
		spin_unlock_irqrestore(&found->qlock, qflags);
	}
	spin_unlock_irqrestore(&v->consumers_lock, flags);
	if (!buf)
		return NULL;
	v->zc_ctx = found;
	memset(buf->zc_in, 0, sizeof(buf->zc_in));
	memset(buf->zc_away_in, 0, sizeof(buf->zc_away_in));
	memset(buf->zc_free_after, 0, sizeof(buf->zc_free_after));
	buf->zc_free_after[0] = v->zc_ev[0];
	buf->zc_free_after[1] = v->zc_ev[1];
	return buf;
}

/* The frame in zc_cur has ended (bottom half): hand it on, and move to the next. */
static void hws_zc_frame_end(struct hws_video *v, enum vb2_buffer_state state, u64 ts, u32 seq, u32 out_size)
{
	struct hwsvideo_buffer *buf = v->zc_cur;

	if (buf) {
		if (state == VB2_BUF_STATE_DONE &&
		    !(buf->zc_in[0] && buf->zc_in[1] && buf->zc_away_in[0] && buf->zc_away_in[1]))
			state = VB2_BUF_STATE_ERROR;
		if (state == VB2_BUF_STATE_DONE) {
			buf->vb.vb2_buf.timestamp = ts;
			buf->vb.field = V4L2_FIELD_NONE;
			buf->vb.sequence = seq;
			vb2_set_plane_payload(&buf->vb.vb2_buf, 0, out_size);
			v->zc_frames++;
		} else {
			v->zc_errors++;
		}
		hws_zc_retire(v, buf, state);
	}
	v->zc_cur = v->zc_next;
	v->zc_next = NULL;
	hws_zc_release(v);
}

/*
 * Back to the DMA buffers now (input size change, consumer stop, engine
 * start). In-flight buffers complete with an error once the card is past
 * them; with @card_stopped they go at once.
 */
static void hws_zc_stop(struct hws_pcie_dev *pdx, bool card_stopped)
{
	struct hws_video *v = &pdx->video[0];
	struct hwsvideo_buffer *buf;
	int p;

	for (p = 0; p < 2; p++)
		hws_zc_program(pdx, p, NULL, 0, 0, 0);
	if (v->zc_cur)
		hws_zc_retire(v, v->zc_cur, VB2_BUF_STATE_ERROR);
	if (v->zc_next)
		hws_zc_retire(v, v->zc_next, VB2_BUF_STATE_ERROR);
	v->zc_cur = v->zc_next = NULL;
	v->zc_ctx = NULL;
	v->zc_stable = 0;
	if (card_stopped) {
		list_for_each_entry(buf, &v->zc_release, queue) {
			buf->zc_free_after[0] = v->zc_ev[0];
			buf->zc_free_after[1] = v->zc_ev[1];
		}
	}
	hws_zc_release(v);
}

/*
 * Produce the output frame from the staging copy: line-double interlaced
 * fields, then scale if the output size differs from the input.
 */
static int hws_render_staged(struct hws_pcie_dev *pdx, int ch, BYTE *dest, int out_w, int out_h)
{
	struct hws_video *v = &pdx->video[ch];
	ACAP_VIDEO_INFO *vi = &pdx->m_VideoInfo[ch];
	int w = v->frame_w, h = v->frame_h;
	BYTE *src[4];
	int len[4], i, off = 0;

	if (!v->frame_il) {
		VideoScaler(vi->m_pVideoStaging, dest, w, h, out_w, out_h);
		return 0;
	}
	for (i = 0; i < 4; i++) {
		src[i] = vi->m_pVideoStaging + off;
		len[i] = v->frame_len[i];
		off += len[i];
	}
	h *= 2;	/* MemCopyFrame line-doubles each field */
	if (w == out_w && h == out_h)
		return MemCopyFrame(ch, dest, w, h, 1, src, len);
	if (MemCopyFrame(ch, vi->m_pVideoScalerBuf, w, h, 1, src, len) < 0)
		return -1;
	VideoScaler(vi->m_pVideoScalerBuf, dest, w, h, out_w, out_h);
	return 0;
}

/*
 * One half of a frame has landed (@half as in struct hws_half_event). First,
 * that quarter pair is pointed at the next frame's destination (zero-copy,
 * above). Then the top half takes a buffer from each consumer and fills its
 * first two quarters; the bottom half fills the rest and completes the
 * buffers with the frame-end time @ts. The halves come from the DMA buffers
 * or from the zero-copy buffer zc_cur, which already holds the frame. A half
 * the card did not refresh, or a bottom half without its top half, loses the
 * frame; held buffers then wait for the next one.
 */
/*
 * A half copied from the DMA buffers is safe only until the card starts the
 * next frame's run into that quarter pair, about one half-frame interval
 * after the half-done interrupt. A copy that ends later may be torn.
 */
static bool hws_copy_late(struct hws_video *v, struct hwsvideo_buffer *zf, u64 ts)
{
	return !zf && v->zc_gap_prev && ktime_get_ns() - ts > v->zc_gap_prev;
}

/* debug_dma test knob (6.11): treat the K-th bottom half from now as not written. */
static bool hws_dbg_stale(struct hws_pcie_dev *pdx, int half)
{
	unsigned int k = READ_ONCE(pdx->dbg_stale_bottom);

	if (half || !k)
		return false;
	WRITE_ONCE(pdx->dbg_stale_bottom, k - 1);
	return k == 1;
}

static void hws_video_half(struct hws_pcie_dev *pdx, int ch, int half, u64 ts)
{
	struct hws_video *v = &pdx->video[ch];
	ACAP_VIDEO_INFO *vi = &pdx->m_VideoInfo[ch];
	int w = pdx->m_pVCAPStatus[ch][0].dwWidth;
	int h = pdx->m_pVCAPStatus[ch][0].dwHeight;
	int il = pdx->m_pVCAPStatus[ch][0].dwinterlace;
	int out_w = v->current_out_width, out_h = v->curren_out_height;
	u32 out_size = out_w * out_h * 2;
	int pitch = pdx->m_Device_SupportYV12 == 1 ? w * 12 / 8 : w * 2;
	int p = half ? 0 : 1;	/* quarter pair */
	int b = half ? 0 : 2;	/* first quarter of this half */
	bool direct = !il && w == out_w && h == out_h;
	struct hwsvideo_buffer *zf = v->zc_cur;	/* the zero-copy buffer this frame went to, if any */
	enum vb2_buffer_state state = VB2_BUF_STATE_DONE;
	struct hwsvideo_buffer *buf;
	BYTE *src0, *src1, *first = NULL;
	u64 gap, margin;
	u32 *mark;
	int i, off, len2, len3;
	bool late = false;

	if (w <= 0 || h <= 0 || w > MAX_VIDEO_HW_W || h > MAX_VIDEO_HW_H || !vi->dwisRuning)
		return;

	/* the pair's next run starts about a half-frame after this interrupt */
	gap = v->zc_last_ts && ts > v->zc_last_ts ? ts - v->zc_last_ts : 0;
	margin = min(gap, v->zc_gap_prev) / 2;
	v->zc_gap_prev = gap;
	v->zc_last_ts = ts;
	v->zc_ev[p]++;
	if (half) {
		if (v->zc_next)		/* its bottom half never came */
			hws_zc_retire(v, v->zc_next, VB2_BUF_STATE_ERROR);
		v->zc_next = hws_zc_pick(pdx, direct, pitch, w);
	}
	hws_zc_program(pdx, p, v->zc_next, ts, margin, pitch);

	if (zf) {
		src0 = (BYTE *)zf->mem + hws_zc_off(pdx, b);
		src1 = (BYTE *)zf->mem + hws_zc_off(pdx, b + 1);
		mark = hws_zc_mark(pdx, zf, p, pitch);
	} else {
		src0 = pdx->m_pbyVideoBuffer[b];
		src1 = pdx->m_pbyVideoBuffer[b + 1];
		mark = hws_half_mark(pdx, half, pitch);
	}
	/* the halves are coherent memory: no sync call, but order the reads after the interrupt */
	dma_rmb();
	if (*mark == HWS_HALF_MARK || (zf && !zf->zc_in[p]) || hws_dbg_stale(pdx, half)) {
		/* stale: the card did not write this half here */
		if (v->top_done) {
			v->top_done = false;
			pdx->m_FramesDropped[ch]++;
			pdx->m_FrameSeq[ch]++;
		}
		if (!half)
			hws_zc_frame_end(v, VB2_BUF_STATE_ERROR, 0, 0, 0);
		return;
	}

	if (half) {
		if (v->top_done) {	/* the previous frame's bottom half never came */
			pdx->m_FramesDropped[ch]++;
			pdx->m_FrameSeq[ch]++;
		}
		v->frame_w = w;
		v->frame_h = h;
		v->frame_il = il;
		for (i = 0; i < 4; i++)
			v->frame_len[i] = pdx->m_format[i].HLAF_SIZE;
		/*
		 * Like the vendor's Windows driver, never pass on video while the
		 * source uses HDCP: the frame is delivered black instead.
		 */
		v->frame_hdcp = hws_hdcp_active(pdx, ch);
		/*
		 * The zero-copy consumer gets this frame in zf. A copy buffer it
		 * still holds from a frame whose bottom half was lost would get
		 * the same frame too, so fail that buffer instead.
		 */
		if (zf) {
			struct hws_vfh_ctx *zctx = vb2_get_drv_priv(zf->vb.vb2_buf.vb2_queue);

			if (zctx->held) {
				list_del(&zctx->held->queue);
				hws_complete_buffer(zctx->held, VB2_BUF_STATE_ERROR);
				zctx->held = NULL;
				v->zc_held_failed++;
			}
		}
		hws_hold_buffers(v, pdx->m_FrameSeq[ch], out_size,
				 zf ? vb2_get_drv_priv(zf->vb.vb2_buf.vb2_queue) : NULL);
		if (v->frame_hdcp) {
			/* nothing copied; the bottom half blanks the buffers */
		} else if (direct) {
			list_for_each_entry(buf, &v->frame_bufs, queue) {
				memcpy(buf->mem, src0, v->frame_len[0]);
				memcpy((BYTE *)buf->mem + v->frame_len[0], src1, v->frame_len[1]);
			}
		} else if (!list_empty(&v->frame_bufs)) {
			memcpy(vi->m_pVideoStaging, src0, v->frame_len[0]);
			memcpy(vi->m_pVideoStaging + v->frame_len[0], src1, v->frame_len[1]);
		}
		v->top_done = true;
		v->frame_late = hws_copy_late(v, zf, ts);
		goto out;
	}

	if (!v->top_done || v->frame_w != w || v->frame_h != h || v->frame_il != il) {
		/* no top half for this frame, or the input changed in between */
		v->top_done = false;
		pdx->m_FramesDropped[ch]++;
		pdx->m_FrameSeq[ch]++;
		hws_zc_frame_end(v, VB2_BUF_STATE_ERROR, 0, 0, 0);
		goto out;
	}
	v->top_done = false;
	off = v->frame_len[0] + v->frame_len[1];
	len2 = v->frame_len[2];
	len3 = w * h * 2 - off - len2;	/* the last quarter's length is rounded up; copy only the frame */
	if (len3 < 0) {
		state = VB2_BUF_STATE_ERROR;
	} else if (v->frame_hdcp || hws_hdcp_active(pdx, ch)) {
		if (zf)		/* the card wrote the frame there: blank it like the others */
			memset32(zf->mem, 0x80108010, out_size / 4);
		list_for_each_entry(buf, &v->frame_bufs, queue) {
			if (first) {
				memcpy(buf->mem, first, out_size);
			} else {
				memset32(buf->mem, 0x80108010, out_size / 4);	/* YUYV black: Y 16, U/V 128 */
				first = buf->mem;
			}
		}
	} else if (direct) {
		list_for_each_entry(buf, &v->frame_bufs, queue) {
			memcpy((BYTE *)buf->mem + off, src0, len2);
			memcpy((BYTE *)buf->mem + off + len2, src1, len3);
		}
		late = hws_copy_late(v, zf, ts);
	} else if (!list_empty(&v->frame_bufs)) {
		memcpy(vi->m_pVideoStaging + off, src0, len2);
		memcpy(vi->m_pVideoStaging + off + len2, src1, len3);
		late = hws_copy_late(v, zf, ts);	/* the scaler below reads only the staging copy */
		list_for_each_entry(buf, &v->frame_bufs, queue) {
			if (first) {
				memcpy(buf->mem, first, out_size);
			} else if (hws_render_staged(pdx, ch, buf->mem, out_w, out_h) < 0) {
				state = VB2_BUF_STATE_ERROR;
				break;
			} else {
				first = buf->mem;
			}
		}
	}
	if (state == VB2_BUF_STATE_DONE && (v->frame_late || late)) {
		state = VB2_BUF_STATE_ERROR;	/* the card may have overwritten a half mid-copy */
		v->late_copies++;
	}
	if (state == VB2_BUF_STATE_DONE)
		v->zc_stable++;
	hws_zc_frame_end(v, state, ts, pdx->m_FrameSeq[ch], out_size);
	hws_finish_frame(v, state, ts, pdx->m_FrameSeq[ch]++, out_size);
out:
	if (!zf)	/* never in a zero-copy buffer: that word is part of the picture */
		*mark = HWS_HALF_MARK;
}

/* No usable signal: give each streaming consumer a blue frame (status thread, each poll). */
static void hws_deliver_no_video(struct hws_pcie_dev *pdx, int ch)
{
	struct hws_video *v = &pdx->video[ch];
	int out_w = v->current_out_width, out_h = v->curren_out_height;
	u32 out_size = out_w * out_h * 2;
	struct hwsvideo_buffer *buf;
	BYTE *first = NULL;

	v->top_done = false;
	hws_hold_buffers(v, pdx->m_FrameSeq[ch], out_size, NULL);
	list_for_each_entry(buf, &v->frame_bufs, queue) {
		if (first) {
			memcpy(buf->mem, first, out_size);
		} else {
			SetNoVideoMem(buf->mem, out_w, out_h);
			first = buf->mem;
		}
	}
	hws_finish_frame(v, VB2_BUF_STATE_DONE, ktime_get_ns(), pdx->m_FrameSeq[ch]++, out_size);
}

/* Threaded half of the interrupt: assemble frames from the halves irqhandler queued. */
static irqreturn_t hws_irq_thread(int irq, void *info)
{
	struct hws_pcie_dev *pdx = info;
	struct hws_video *v = &pdx->video[0];
	struct hws_half_event ev;

	while (kfifo_get(&pdx->half_events, &ev)) {
		mutex_lock(&v->cap_lock);
		if (pdx->m_bVCapStarted[0] && !pdx->m_curr_No_Video[0])
			hws_video_half(pdx, 0, ev.half, ev.ts);
		mutex_unlock(&v->cap_lock);
	}
	return IRQ_HANDLED;
}

static void hws_get_video_param(struct hws_pcie_dev *dev,int index)
{

	//printk( "%s(): %x \n", __func__, index);
	int width,height;
	width= dev->m_pVCAPStatus[index][0].dwWidth;
	height=dev->m_pVCAPStatus[index][0].dwHeight;
	dev->video[index].current_out_pixfmt =0;
	dev->video[index].current_out_width = width;
	dev->video[index].curren_out_height = height;
	//printk( "%s(%dx%d):  \n", __func__, width,height);

}

static void hws_adapters_init(struct hws_pcie_dev *dev)
{
	int i;
	for (i = 0; i <MAX_VID_CHANNELS; i++) {
		hws_get_video_param(dev,i);
	}
}
/*
 * Lifetime. Open V4L2 and ALSA handles keep pointers into hws_pcie_dev after
 * remove(), so it is freed only when the last reference goes: probe holds
 * one (dropped at the end of remove()), each channel's v4l2_device holds one
 * (dropped by hws_v4l2_release() after its last video handle closes), and
 * each sound card holds one (dropped by its private_free).
 */
static void hws_dev_release(struct kref *ref)
{
	kfree(container_of(ref, struct hws_pcie_dev, ref));
}

static void hws_put_dev(struct hws_pcie_dev *pdx)
{
	kref_put(&pdx->ref, hws_dev_release);
}

/* The last handle of this channel's video node is closed. */
static void hws_v4l2_release(struct v4l2_device *v4l2_dev)
{
	struct hws_video *video = container_of(v4l2_dev, struct hws_video, v4l2_dev);

	/* control events can use the handler until the last close */
	v4l2_ctrl_handler_free(&video->ctrl_hdl);
	hws_put_dev(video->dev);
}

static void hws_card_private_free(struct snd_card *card)
{
	hws_put_dev(card->private_data);
}

int hws_video_register(struct hws_pcie_dev *dev)
{
	struct video_device *vdev ;
	int i;
	int err=-1;
	//printk("hws_video_register Start\n");
	for(i=0;i<dev->m_nCurreMaxVideoChl;i++)
	{
		//printk("v4l2_device_register[%d]\n",i);
		dev->video[i].dev = dev;
		dev->video[i].v4l2_dev.release = hws_v4l2_release;
		kref_get(&dev->ref);
		err = v4l2_device_register(&dev->pdev->dev, &dev->video[i].v4l2_dev);
		if(err<0){
			printk(KERN_ERR " v4l2_device_register 0 error! \n");
			hws_put_dev(dev);	/* not registered: no release callback */
			while (--i >= 0) {
				v4l2_device_unregister(&dev->video[i].v4l2_dev);
				v4l2_device_put(&dev->video[i].v4l2_dev);
			}
			return -1;
		}
	}
	//printk("v4l2_device_register end\n");
	//----------------------------------------------------
	for(i=0;i<dev->m_nCurreMaxVideoChl;i++){
		//printk("v4l2_device_register INT[%d]\n",i);
		vdev = &(dev->video[i].vdev);
		if (NULL == vdev){
			printk(KERN_ERR " video_device_alloc failed !!!!! \n");
			goto fail;
		}
		dev->video[i].index = i;
		dev->video[i].dev = dev;
		dev->video[i].fileindex =0;
		dev->video[i].pixfmt = V4L2_PIX_FMT_YUYV;
		//-------------------
		//-------------------
		vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
		vdev->v4l2_dev = &(dev->video[i].v4l2_dev);
		vdev->lock = NULL;	/* locking is per queue, see hws_unlocked_ioctl() */
		vdev->fops = &hws_fops;
		strcpy(vdev->name,KBUILD_MODNAME);
		vdev->release = video_device_release_empty;
		vdev->vfl_dir = VFL_DIR_RX;
		vdev->ioctl_ops = &hws_ioctl_fops;
		spin_lock_init(&dev->video[i].consumers_lock);
		mutex_init(&dev->video[i].ioctl_lock);
		INIT_LIST_HEAD(&dev->video[i].consumers);
		atomic_set(&dev->video[i].engine_users, 0);

		//printk("v4l2_device_register INT3[%d]\n",i);
		//printk("v4l2_device_register INT2[%d]\n",i);
		video_set_drvdata(vdev, &(dev->video[i]));

		vdev->queue = NULL;	/* each open() has its own queue */

		/* seed the status thread's view so the first poll raises no event */
		dev->video[i].src_signal = hws_read_input(dev, i, &dev->video[i].src_w,
							  &dev->video[i].src_h, &dev->video[i].src_il);
		err = hws_ctrls_init(&dev->video[i]);
		if(err){
			printk(KERN_ERR " control handler init failed !!!!! \n");
			goto fail;
		}
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
	hws_unregister_video(dev);	/* the control handlers go in hws_v4l2_release() */
	return err;
}

static struct snd_pcm_hardware audio_pcm_hardware = {
	/*
	 * BATCH: the pointer only moves when a whole DMA packet
	 * (MAX_DMA_AUDIO_PK_SIZE, 1024 frames / 21.3 ms) lands, so tell
	 * userspace it is coarse. Without it PipeWire's timer scheduling reads
	 * the jumps as clock drift and resamples the audio (robotic sound).
	 */
	.info 				=	(SNDRV_PCM_INFO_MMAP |
	SNDRV_PCM_INFO_INTERLEAVED |
	SNDRV_PCM_INFO_BLOCK_TRANSFER |
	SNDRV_PCM_INFO_BATCH |
	SNDRV_PCM_INFO_MMAP_VALID),
	.formats 			=	SNDRV_PCM_FMTBIT_S16_LE,
	.rates 				=   SNDRV_PCM_RATE_48000,
	.rate_min 			=	48000,
	.rate_max 			=	48000,
	.channels_min 		=	2,
	.channels_max 		=	2,
	.buffer_bytes_max 	=	64*1024,
	/* a period shorter than one DMA packet can never complete on its own */
	.period_bytes_min 	=	MAX_DMA_AUDIO_PK_SIZE,
	.period_bytes_max 	=	16*1024,
	.periods_min 		=	2,
	.periods_max 		=	255,
};
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
	/* periods are whole DMA packets, so period_elapsed fires on packet boundaries */
	return snd_pcm_hw_constraint_step(runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
					  MAX_DMA_AUDIO_PK_SIZE);
}

int hws_pcie_audio_close(struct snd_pcm_substream *substream)
{
	struct hws_audio *drv = snd_pcm_substream_chip(substream);
	unsigned long flags;

	cancel_work_sync(&drv->audiowork);
	spin_lock_irqsave(&drv->dev->audiolock[drv->index], flags);
	drv->substream = NULL;	/* the audio work checks it under audiolock */
	spin_unlock_irqrestore(&drv->dev->audiolock[drv->index], flags);
	return 0;
}
int hws_pcie_audio_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params)
{
	//printk(KERN_INFO "%s() \n",__func__);
	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
}

int hws_pcie_audio_hw_free(struct snd_pcm_substream *substream)
{
	struct hws_audio *drv = snd_pcm_substream_chip(substream);

	//printk(KERN_INFO "%s() \n",__func__);
	cancel_delayed_work_sync(&drv->silence_work);	/* both write the buffer */
	cancel_work_sync(&drv->audiowork);
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
			/*
			 * No audiolock here: the audio work holds it while
			 * snd_pcm_period_elapsed() takes the stream lock that
			 * trigger runs under.
			 */
			hws_audio_card_packet(chip);
			WRITE_ONCE(chip->running, true);
			queue_delayed_work(dev->auwq, &chip->silence_work, 0);
			StartAudioCapture(dev,chip->index);
			break;
		case SNDRV_PCM_TRIGGER_SUSPEND:	/* no RESUME: the app prepares and starts again */
		case SNDRV_PCM_TRIGGER_STOP:
			//stop dma
			//HWS_PCIE_WRITE(HWS_INT_BASE, HWS_DMA_MASK(chip->index), 0x000000000);
			//HWS_PCIE_WRITE(HWS_DMA_BASE(chip->index), HWS_DMA_START, 0x00000000);
			//printk(KERN_INFO "SNDRV_PCM_TRIGGER_STOP index:%x\n",chip->index);
			WRITE_ONCE(chip->running, false);
			cancel_delayed_work(&chip->silence_work);
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
	unsigned long flags;
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
		ret = snd_card_new(&dev->pdev->dev, -1, audioname, THIS_MODULE, 0, &card);
		// ret = snd_card_new(&dev->pdev->dev, audio_index[i], audio_id[i], THIS_MODULE,	sizeof(struct hws_audio), &card);
		if (ret < 0){
			printk(KERN_ERR "%s() ERROR: snd_card_new failed <%d>\n",__func__, ret);
			goto fail0;
		}
		/* stored now so the error path frees it; the card holds a device reference */
		dev->audio[i].card = card;
		card->private_data = dev;
		card->private_free = hws_card_private_free;
		kref_get(&dev->ref);
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
			#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,19,0))
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
		INIT_DELAYED_WORK(&dev->audio[i].silence_work, hws_audio_silence_work);
		ret = snd_card_register(card);
		if ( ret < 0) {
			printk(KERN_ERR "%s() ERROR: snd_card_register failed\n",__func__);
			goto fail1;
		}
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
	if (pdx->m_Removed)	/* BAR0 is unmapped once remove() has run */
		return;
	bar0 = (char*)pdx->map_bar0_addr;
	iowrite32(Value,bar0+RegisterOffset);
	//map_bar0_addr[RegisterOffset/4] = Value;

}

static u32 READ_REGISTER_ULONG (struct hws_pcie_dev *pdx,u32 RegisterOffset)
{
	char *bar0;
	if (pdx->m_Removed)
		return 0xFFFFFFFF;	/* reads like a card that is gone */
	bar0 = (char*)pdx->map_bar0_addr;
	//return(map_bar0_addr[RegisterOffset/4]);
	return(ioread32(bar0+RegisterOffset));
}
//----------------------------------------------
/*
 * Wait for register 0 bit 3 (mainline: DMA busy) to clear after a stop, at
 * most 200 ms. It used to wait forever and print a line every 10 ms.
 */
static int Check_Busy(struct hws_pcie_dev *pdx)
{
	u32 statusreg;
	int tries;

	for (tries = 0; tries < 20; tries++) {
		statusreg = READ_REGISTER_ULONG(pdx, CVBS_IN_BASE);
		if (statusreg == 0xFFFFFFFF || !(statusreg & 0x08))
			return 0;
		msleep(10);
	}
	dev_warn(&pdx->pdev->dev, "card still busy 200 ms after stop (status %#x)\n", statusreg);
	return -ETIMEDOUT;
}

static void StopDsp(struct hws_pcie_dev *pdx)
{
	//int j, i;
	u32  statusreg;
	statusreg = READ_REGISTER_ULONG(pdx,(u32)(CVBS_IN_BASE));
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
	hws_snap_regs(pdx, 'V', en);
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
	hws_snap_regs(pdx, 'A', en);
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
	if(hf_size2 <0)
	{
		return -1;
	}
	hf_size2 = hf_size2/(16*128);
	hf_size2 = (hf_size2+1)*16*128;
	/*
	 * The card DMAs each quarter into one buffer whose last
	 * MAX_AUDIO_CAP_SIZE bytes hold the audio ring. A larger quarter would
	 * be written past the buffer, so never program it.
	 */
	if(hf_size > pdx->m_MaxHWVideoBufferSize - MAX_AUDIO_CAP_SIZE ||
	   hf_size2 > pdx->m_MaxHWVideoBufferSize - MAX_AUDIO_CAP_SIZE)
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
	pdx->m_format[3].HLAF_SIZE = hf_size2;
	return 1;

}

static void DmaMemFreePool(struct hws_pcie_dev *pdx)
{
	int k, index;
	if(pdx->m_bBufferAllocate != TRUE)
		return;

	for(index=0; index<pdx->m_nMaxChl; index++)
	{
		if(pdx->m_pbyVideoBuffer[index])
		{
			dma_free_coherent(&pdx->pdev->dev, pdx->m_MaxHWVideoBufferSize, pdx->m_pbyVideoBuffer[index], pdx->m_pbyVideo_phys[index]);
			pdx->m_pbyVideoBuffer[index] = NULL;
		}
	}
	for(index=0; index<pdx->m_nCurreMaxVideoChl; index++)
	{
		vfree(pdx->m_VideoInfo[index].m_pVideoScalerBuf);
		pdx->m_VideoInfo[index].m_pVideoScalerBuf = NULL;
		vfree(pdx->m_VideoInfo[index].m_pVideoStaging);
		pdx->m_VideoInfo[index].m_pVideoStaging = NULL;
		for(k=0; k<MAX_AUDIO_QUEUE; k++)
		{
			kfree(pdx->m_AudioInfo[index].m_pAudioBufData[k]);
			pdx->m_AudioInfo[index].m_pAudioBufData[k] = NULL;
		}
	}
	pdx->m_bBufferAllocate = FALSE;
}

/*
 * Allocate the m_nMaxChl DMA buffers of @size bytes. The card reaches host
 * memory through a table of 512 MiB windows (SetDMAAddress), so a buffer must
 * not cross a window boundary. On any failure, frees what it allocated.
 */
static int dma_alloc_video_buffers(struct hws_pcie_dev *pdx, int size)
{
	int i;
	for(i=0; i<pdx->m_nMaxChl; i++)
	{
		pdx->m_pbyVideoBuffer[i] = dma_alloc_coherent(&pdx->pdev->dev, size, &pdx->m_pbyVideo_phys[i], GFP_KERNEL | __GFP_NOWARN);
		if(!pdx->m_pbyVideoBuffer[i] ||
		   ((u64)pdx->m_pbyVideo_phys[i] >> 29) != (((u64)pdx->m_pbyVideo_phys[i] + size - 1) >> 29))
		{
			if(pdx->m_pbyVideoBuffer[i])
				dma_free_coherent(&pdx->pdev->dev, size, pdx->m_pbyVideoBuffer[i], pdx->m_pbyVideo_phys[i]);
			pdx->m_pbyVideoBuffer[i] = NULL;
			while(--i >= 0)
			{
				dma_free_coherent(&pdx->pdev->dev, size, pdx->m_pbyVideoBuffer[i], pdx->m_pbyVideo_phys[i]);
				pdx->m_pbyVideoBuffer[i] = NULL;
			}
			return -1;
		}
	}
	pdx->m_MaxHWVideoBufferSize = size;
	return 0;
}

static int DmaMemAllocPool(struct hws_pcie_dev *pdx)
{
	uint8_t i, k;
	int index;
	dma_addr_t phy_addr;

	if(pdx->m_bBufferAllocate == TRUE)
	{
		DmaMemFreePool(pdx);
	}
	/* from here on a failure frees whatever was allocated */
	pdx->m_bBufferAllocate = TRUE;
	if(dma_alloc_video_buffers(pdx, MAX_MM_VIDEO_SIZE_DCI) == 0)
	{
		pdx->m_MaxInputWidth = pdx->m_DeviceHW_Version ? 4096 : 3840;
		dev_info(&pdx->pdev->dev, "DMA buffers: %d x %d KiB, input up to %dx2160\n",
			 pdx->m_nMaxChl, MAX_MM_VIDEO_SIZE_DCI/1024, pdx->m_MaxInputWidth);
	}
	else if(dma_alloc_video_buffers(pdx, MAX_MM_VIDEO_SIZE) == 0)
	{
		pdx->m_MaxInputWidth = 3840;
		dev_warn(&pdx->pdev->dev, "4.5 MiB DMA buffers unavailable (no IOMMU translation or CMA); "
			 "4096-wide input will be captured at 3840\n");
	}
	else
	{
		printk("m_pbyVideoBuffer mem Allocate Fail ************\n");
		goto fail;
	}
	for(i=0; i<pdx->m_nMaxChl; i++)
	{
		phy_addr = pdx->m_pbyVideo_phys[i];
		pdx->m_dwVideoBuffer[i] = 	  ((u64)phy_addr)&0xFFFFFFFF;
		pdx->m_dwVideoHighBuffer[i] = ((u64)phy_addr>>32)&0xFFFFFFFF;

		/* the audio ring lives in the last MAX_AUDIO_CAP_SIZE bytes of each video buffer */
		pdx->m_pbyAudioBuffer[i] = (BYTE *)(pdx->m_pbyVideoBuffer[i] + pdx->m_MaxHWVideoBufferSize -MAX_AUDIO_CAP_SIZE );
		pdx->m_pbyAudio_phys[i] = pdx->m_pbyVideo_phys[i] + (pdx->m_MaxHWVideoBufferSize -MAX_AUDIO_CAP_SIZE);
		pdx->m_dwAudioBuffer[i] =    pdx->m_dwVideoBuffer[i]+ pdx->m_MaxHWVideoBufferSize -MAX_AUDIO_CAP_SIZE;
		pdx->m_dwAudioBufferHigh[i] = pdx->m_dwVideoHighBuffer[i];
	}

	/*
	 * Frames are copied from the DMA buffers straight into app buffers.
	 * Only interlaced input or a scaled output needs CPU-side frames: the
	 * staging copy and the line-doubled frame for the scaler.
	 */
	for(index=0; index<pdx->m_nCurreMaxVideoChl; index++)
	{
		pdx->m_VideoInfo[index].m_pVideoStaging = vmalloc(MAX_VIDEO_HW_W*MAX_VIDEO_HW_H*2);
		pdx->m_VideoInfo[index].m_pVideoScalerBuf = vmalloc(MAX_VIDEO_HW_W*MAX_VIDEO_HW_H*2);
		if(!pdx->m_VideoInfo[index].m_pVideoStaging || !pdx->m_VideoInfo[index].m_pVideoScalerBuf)
			goto fail;
	}

	//----------audio alloc
	for(i=0; i<pdx->m_nCurreMaxVideoChl; i++)
	{
		for( k=0; k<MAX_AUDIO_QUEUE;k++)
		{
			pdx->m_AudioInfo[i].m_pAudioBufData[k] = kzalloc(pdx->m_dwAudioPTKSize, GFP_KERNEL);
			if(!pdx->m_AudioInfo[i].m_pAudioBufData[k])
				goto fail;
			pdx->m_AudioInfo[i].pStatusInfo[k].byLock = MEM_UNLOCK;
		}
	}
	return 0;

fail:
	DmaMemFreePool(pdx);
	return -1;
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
		WRITE_REGISTER_ULONG(pdx, INT_EN_REG_BASE, 0);	/* before free_irq, for a shared line */
	}


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
		pdx->mMain_tsk = NULL;
	}
}

static void hws_unregister_video(struct hws_pcie_dev *dev)
{
	int i;

	for (i = 0; i < dev->m_nCurreMaxVideoChl; i++) {
		video_unregister_device(&dev->video[i].vdev);
		v4l2_device_unregister(&dev->video[i].v4l2_dev);
		v4l2_device_put(&dev->video[i].v4l2_dev);
	}
}

/*
 * Teardown, in the reverse order of probe: stop everything that touches the
 * card or its buffers (status thread, capture engines, interrupt handlers,
 * audio work) before the devices go away and the DMA buffers are freed.
 * Freeing the IRQ also waits for the IRQ thread.
 */
static void hws_remove(struct pci_dev *pdev)
{
	int i;
	struct hws_pcie_dev *dev = pci_get_drvdata(pdev);

	if (!dev || dev->map_bar0_addr == NULL)
		return;
	/* first, no new opens or ioctls; running PCM streams stop now */
	for (i = 0; i < dev->m_nCurreMaxVideoChl; i++)
		if (dev->audio[i].card)
			snd_card_disconnect(dev->audio[i].card);
	for (i = 0; i < dev->m_nCurreMaxVideoChl; i++) {
		struct hws_vfh_ctx *ctx;
		unsigned long flags;

		video_unregister_device(&dev->video[i].vdev);
		/* wake readers blocked in DQBUF or poll: no frame will come */
		spin_lock_irqsave(&dev->video[i].consumers_lock, flags);
		list_for_each_entry(ctx, &dev->video[i].consumers, node)
			vb2_queue_error(&ctx->vbq);
		spin_unlock_irqrestore(&dev->video[i].consumers_lock, flags);
	}
	debugfs_remove_recursive(dev->dbg);
	/* then everything that touches the card or its buffers */
	StopKSThread(dev);
	for (i = 0; i < dev->m_nCurreMaxVideoChl; i++)
		hws_video_quiesce(dev, i);	/* before the DMA buffers are freed */
	StopDevice(dev);
	irq_teardown(dev);
	tasklet_kill(&dev->dpc_audio_tasklet[0]);
	for (i = 0; i < dev->m_nCurreMaxVideoChl; i++) {
		WRITE_ONCE(dev->audio[i].running, false);
		cancel_delayed_work_sync(&dev->audio[i].silence_work);
		cancel_work_sync(&dev->audio[i].audiowork);
	}
	destroy_workqueue(dev->auwq);
	dev->auwq = NULL;
	DmaMemFreePool(dev);
	dev->m_PciDeviceLost = 1;
	dev->m_Removed = true;	/* register accessors do nothing from here on */
	iounmap(dev->info.mem[0].internal_addr);
	if (dev->msi_enabled)
		pci_disable_msi(pdev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
	/* last, drop the references; open handles keep dev until they close */
	for (i = 0; i < dev->m_nCurreMaxVideoChl; i++) {
		v4l2_device_unregister(&dev->video[i].v4l2_dev);
		v4l2_device_put(&dev->video[i].v4l2_dev);
	}
	for (i = 0; i < dev->m_nCurreMaxVideoChl; i++) {
		if (dev->audio[i].card) {
			snd_card_free_when_closed(dev->audio[i].card);
			dev->audio[i].card = NULL;
		}
	}
	hws_put_dev(dev);
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

/*
 * The PCM trigger runs in atomic context, and InitVideoSys() sleeps (the
 * 500 us start pause), so StartAudioCapture() can't call CheckCardStatus().
 * It only notes that the card lost its run state; the status thread
 * re-initialises it at its next poll (hws_do_reinit()).
 */
static void hws_request_reinit(struct hws_pcie_dev *pdx)
{
	u32 st;

	if (pdx->m_PciDeviceLost)
		return;
	st = READ_REGISTER_ULONG(pdx, CVBS_IN_BASE);
	if (st != 0xFFFFFFFF && !(st & 0x01))
		WRITE_ONCE(pdx->m_NeedReinit, 1);
}

/*
 * Status thread: re-initialise a card that lost its run state, then
 * re-enable the engines that were running. zero-copy goes back to the DMA
 * buffers first, since InitVideoSys() reprograms the buffer registers.
 */
static void hws_do_reinit(struct hws_pcie_dev *pdx)
{
	int i;

	if (!READ_ONCE(pdx->m_NeedReinit))
		return;
	WRITE_ONCE(pdx->m_NeedReinit, 0);
	mutex_lock(&pdx->video[0].engine_lock);
	mutex_lock(&pdx->video[0].cap_lock);
	hws_zc_stop(pdx, false);
	InitVideoSys(pdx, 1);
	for (i = 0; i < pdx->m_nCurreMaxVideoChl; i++) {
		if (pdx->m_bVCapStarted[i])
			EnableVideoCapture(pdx, i, 1);
		if (pdx->m_bACapStarted[i])
			EnableAudioCapture(pdx, i, 1);
	}
	mutex_unlock(&pdx->video[0].cap_lock);
	mutex_unlock(&pdx->video[0].engine_lock);
	dev_info(&pdx->pdev->dev, "card had lost its run state; re-initialised\n");
}

static int  StartAudioCapture(struct hws_pcie_dev *pdx,int index)
{
	int j;

	if(pdx->m_bACapStarted[index]==1)
	{
		if(CheckAudioCapture(pdx,index) ==0)
		{
			hws_request_reinit(pdx);
			EnableAudioCapture(pdx,index,1);
		}
		//DbgPrint("Re StartAudioCapture =%d",index);
		return -1;
	}
	hws_request_reinit(pdx);
	pdx->m_bAudioRun[index] = 1;
	pdx->m_bAudioStop[index] = 0;
	pdx->m_nAudioBufferIndex[index] =0;
	pdx->audio_data[index]=0;
	pdx->m_nRDAudioIndex[index] =0;
	for(j=0; j<MAX_AUDIO_QUEUE;j++)
	{
		pdx->m_AudioInfo[index].pStatusInfo[j].byLock = MEM_UNLOCK;
	}
	/*
	 * After an enable the card first sends one or two packets it captured
	 * before the last disable (6.20), heard as a blip of old sound. Drop
	 * them; the silence fill covers the 43 ms.
	 */
	pdx->m_AudioSkip[index] = HWS_AUDIO_STALE_PKTS;
	pdx->m_AudioInfo[index].dwisRuning =1;
	EnableAudioCapture(pdx,index,1);
	return 0;
}

/*
 * Fill the unused gap of each DMA buffer, between its quarter and the audio
 * ring, with a known pattern; debugfs dma reports bytes the card changed
 * there. The card should never write it, so changes are stray or oversized
 * DMA writes (stage 3.5). Filled at probe and again when the input size
 * moves the gap; the status thread checks it every poll (hws_check_canary),
 * logs each write and re-arms. Filled at probe, otherwise only under cap_lock.
 */
#define HWS_CANARY 0xC0FFEE11u
static void hws_fill_canary(struct hws_pcie_dev *pdx)
{
	int i, start, end = pdx->m_MaxHWVideoBufferSize - MAX_AUDIO_CAP_SIZE;

	if (!debug_dma)
		return;

	for (i = 0; i < pdx->m_nMaxChl && i < MAX_VID_CHANNELS; i++) {
		start = pdx->m_pbyVideoBuffer[i] ? round_up(pdx->m_format[i].HLAF_SIZE, 64) : end;
		if (start >= end) {
			pdx->canary_start[i] = 0;
			continue;
		}
		if (pdx->canary_start[i] == start)
			continue;	/* same gap: keep what earlier captures left, to cover many start/stop cycles */
		memset32((u32 *)(pdx->m_pbyVideoBuffer[i] + start), HWS_CANARY, (end - start) / 4);
		pdx->canary_start[i] = start;
	}
}

/*
 * Status thread, every fifth poll with debug_dma: log any canary words the
 * card overwrote, with enough context to tell when it happens, then re-arm
 * them. The scan runs without cap_lock, which the IRQ thread needs; the lock
 * is held only to read where the gap starts and to re-arm it, and the re-arm
 * is skipped if an input-size change moved the gap meanwhile.
 *
 * An over-long quarter run (stale lengths after a mode change) ends with a
 * 2 KiB block at register 9 x 16. That block is expected, so it is logged at
 * debug level as an over-long run rather than as a stray write.
 */
static void hws_check_canary(struct hws_pcie_dev *pdx)
{
	int i, k, words, lo, hi, start, end = pdx->m_MaxHWVideoBufferSize - MAX_AUDIO_CAP_SIZE;
	struct hws_regsnap last = { 0 };
	unsigned long flags;
	u32 exp_lo, exp_hi;
	u32 *c;

	if (!debug_dma || pdx->m_PciDeviceLost)
		return;
	exp_lo = READ_REGISTER_ULONG(pdx, HWS_REG_DMA_MAX_SIZE) * 16;
	exp_hi = exp_lo + 2048;
	for (i = 0; i < pdx->m_nMaxChl && i < MAX_VID_CHANNELS; i++) {
		mutex_lock(&pdx->video[0].cap_lock);
		start = pdx->canary_start[i];
		mutex_unlock(&pdx->video[0].cap_lock);
		if (!start)
			continue;
		c = (u32 *)(pdx->m_pbyVideoBuffer[i] + start);
		words = (end - start) / 4;
		lo = -1;
		hi = -1;
		for (k = 0; k < words; k++) {
			if (READ_ONCE(c[k]) != HWS_CANARY) {
				if (lo < 0)
					lo = k;
				hi = k;
			}
		}
		if (lo < 0)
			continue;
		if (start + lo * 4 >= exp_lo && start + hi * 4 + 4 <= exp_hi) {
			dev_dbg(&pdx->pdev->dev, "over-long DMA run ended at buffer %d offset %#x (register 9), as expected\n",
				i, start + lo * 4);
		} else {
			pdx->canary_hits[i]++;
			pdx->canary_last_lo[i] = (u64)pdx->m_pbyVideo_phys[i] + start + lo * 4;
			pdx->canary_last_hi[i] = (u64)pdx->m_pbyVideo_phys[i] + start + hi * 4 + 3;
			spin_lock_irqsave(&pdx->snap_lock, flags);
			if (pdx->snap_next)
				last = pdx->snaps[(pdx->snap_next - 1) % HWS_REGSNAPS];
			spin_unlock_irqrestore(&pdx->snap_lock, flags);
			dev_info_ratelimited(&pdx->pdev->dev,
				"stray DMA write #%u: buffer %d, offset %#x-%#x (IOVA %llx-%llx); video %s, audio %s, frame %u; %llu ms after %s %s\n",
				pdx->canary_hits[i], i, start + lo * 4, start + hi * 4 + 3,
				pdx->canary_last_lo[i], pdx->canary_last_hi[i],
				pdx->m_bVCapStarted[0] ? "on" : "off", pdx->m_bACapStarted[0] ? "on" : "off",
				pdx->m_FrameSeq[0], last.ts ? div_u64(ktime_get_ns() - last.ts, NSEC_PER_MSEC) : 0,
				last.what == 'A' ? "audio" : "video", last.en ? "enable" : "disable");
		}
		mutex_lock(&pdx->video[0].cap_lock);
		if (pdx->canary_start[i] == start)
			memset32(c + lo, HWS_CANARY, hi - lo + 1);
		mutex_unlock(&pdx->video[0].cap_lock);
	}
}

static int StartVideoCapture(struct hws_pcie_dev *pdx,int index)
{
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
	pdx->m_VideoInfo[index].dwisRuning = 1;
	pdx->video_toggle[index] = 0;	/* the first event is then a top half */
	EnableVideoCapture(pdx,index,1);
	return 0;
}

/*
 * Stop video DMA before a buffer the card may be writing is returned or
 * freed. Probe 6.9 found that once the cleared enable bit has been read
 * back (EnableVideoCapture() reads register 2 after writing it), the card
 * writes nothing more: it finishes the 2 KiB burst in progress before
 * that. Register 0 bit 3 doesn't show DMA in progress, so there is
 * nothing to poll; the pause is margin, not a measured need. Sleeps.
 */
static void hws_video_quiesce(struct hws_pcie_dev *pdx, int index)
{
	StopVideoCapture(pdx, index);
	usleep_range(100, 200);
}

static void StopVideoCapture(struct hws_pcie_dev *pdx,int index)
{
	//int inc=0;

	if(pdx->m_bVCapStarted[index] ==0) return;
	//pdx->m_nVideoIndex[index] =0;
	pdx->m_VideoInfo[index].dwisRuning = 0;
	EnableVideoCapture(pdx,index,0);
}
static void StopAudioCapture(struct hws_pcie_dev *pdx,int index)
{
	//int inc=0;
	if(pdx->m_bAudioRun[index] ==0) return;
	pdx->m_bAudioRun[index] = 0;
	pdx->m_bAudioStop[index] = 1;
	pdx->m_nAudioBufferIndex[index] =0;
	pdx->m_AudioInfo[index].dwisRuning =0;
EnableAudioCapture(pdx,index,0);
}
//-----------------------------


//-----------------------------
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
	if (pdx->m_AudioSkip[dwAudioCh]) {
		pdx->m_AudioSkip[dwAudioCh]--;	/* stale, see StartAudioCapture() */
		return 0;
	}
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

			dma_rmb();	/* the ring is coherent memory: order the reads, no sync call */
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

//-----------------------------
/* Interrupt handler. Read/modify/write the command register to disable
 * the interrupt. */
//static irqreturn_t irqhandler(int irq, struct uio_info *info)
/*
 * Called from the interrupt handler when a frame completes. Besides the frame
 * timestamp, measures the input frame rate: frame periods are counted over
 * windows of about a second, which resolves 60.00 from 59.94 fps (0.1% apart)
 * despite interrupt jitter. A frame-end interrupt can still be missed (two
 * halves can complete before the handler reads the toggle), so each interval counts as
 * however many periods it spans, rounded, against the shortest interval seen in
 * the window; a plain count read one missed interrupt per second as 59 fps.
 * A gap longer than 200 ms (signal loss, capture restart) starts a new window.
 */
static void hws_frame_end(struct hws_pcie_dev *pdx, int ch, u64 now)
{
	u64 prev = pdx->m_FrameEndNs[ch];
	u64 elapsed, d, unit;

	pdx->m_FrameEndNs[ch] = now;
	if (!prev || now - prev > 200 * NSEC_PER_MSEC || !pdx->m_RateWinStartNs[ch]) {
		pdx->m_RateWinStartNs[ch] = now;
		pdx->m_RateWinFrames[ch] = 0;
		pdx->m_RateMinDeltaNs[ch] = 0;
		return;
	}
	d = now - prev;
	if (!pdx->m_RateMinDeltaNs[ch] || d < pdx->m_RateMinDeltaNs[ch])
		pdx->m_RateMinDeltaNs[ch] = d;
	unit = pdx->m_RateMinDeltaNs[ch];
	pdx->m_RateWinFrames[ch] += max_t(u32, 1, (u32)div64_u64(d + unit / 2, unit));
	elapsed = now - pdx->m_RateWinStartNs[ch];
	if (elapsed >= NSEC_PER_SEC) {
		WRITE_ONCE(pdx->m_FrameIntervalNs[ch], div_u64(elapsed, pdx->m_RateWinFrames[ch]));
		WRITE_ONCE(pdx->m_RateUpdatedNs[ch], now);
		pdx->m_RateWinStartNs[ch] = now;
		pdx->m_RateWinFrames[ch] = 0;
	}
}

/* The card's own input rate in whole fps (register 110 + ch), 0 if unavailable. */
static u32 hws_hw_fps(struct hws_pcie_dev *pdx, int ch)
{
	u32 fps;

	if (!pdx->m_DeviceHW_Version || pdx->m_PciDeviceLost)
		return 0;
	fps = READ_REGISTER_ULONG(pdx, HWS_REG_IN_FPS(ch));
	return fps >= 1 && fps <= 300 ? fps : 0;
}

/*
 * The input frame interval as a time-per-frame fraction. While capture runs
 * and the measurement is current, it comes from frame-end interrupts and is
 * snapped to a standard rate within 0.5%, which tells 59.94 from 60. When
 * nothing captures, or the measurement is older than 2 s, the measured value
 * would be stale after any mode change, so the card's whole-fps register is
 * used, snapped within 2%. Returns false if neither is available.
 */
static bool hws_input_timeperframe(struct hws_pcie_dev *pdx, int ch, struct v4l2_fract *tpf)
{
	static const struct v4l2_fract standard[] = {
		{ 1001, 24000 }, { 1, 24 }, { 1, 25 }, { 1001, 30000 }, { 1, 30 },
		{ 1, 50 }, { 1001, 60000 }, { 1, 60 }, { 1, 100 }, { 1001, 120000 },
		{ 1, 120 }, { 1, 144 }, { 1, 240 },
	};
	u64 meas = READ_ONCE(pdx->m_FrameIntervalNs[ch]);
	u64 at = READ_ONCE(pdx->m_RateUpdatedNs[ch]);
	u64 best_err = U64_MAX, std;
	u32 fps, tol = 200;	/* 1/200 = 0.5% */
	int i, best = -1;

	if (!meas || !pdx->m_bVCapStarted[ch] || ktime_get_ns() - at > 2 * NSEC_PER_SEC) {
		fps = hws_hw_fps(pdx, ch);
		if (fps) {
			meas = div_u64(NSEC_PER_SEC, fps);
			tol = 50;	/* 2%: the register is a whole number */
		} else if (!meas) {
			return false;
		}
	}
	for (i = 0; i < ARRAY_SIZE(standard); i++) {
		std = div_u64((u64)standard[i].numerator * NSEC_PER_SEC, standard[i].denominator);
		if ((meas > std ? meas - std : std - meas) < best_err) {
			best_err = meas > std ? meas - std : std - meas;
			best = i;
		}
	}
	if (best >= 0 && best_err * tol <= div_u64((u64)standard[best].numerator * NSEC_PER_SEC,
						    standard[best].denominator)) {
		*tpf = standard[best];
	} else {
		tpf->numerator = (u32)div_u64(meas, 1000);	/* microseconds */
		tpf->denominator = 1000000;
	}
	return true;
}

static irqreturn_t irqhandler(int irq, void  *info)
{
	struct hws_pcie_dev *pdx = (struct hws_pcie_dev *)(info);
	//struct pci_dev *pdev = pdx->pdev;

	u32 dma_status;
	u32 Int_Value =0;
	u32 IntState;
	u32 tmp;
	u32 cnt;
	bool wake_thread = false;

	dma_status =  READ_REGISTER_ULONG(pdx,(u32)(CVBS_IN_BASE));
	//printk("dma_status %x\n", dma_status);
	if(((dma_status&0x04)==0x04)&&(dma_status !=0xffffffff))
	{
		IntState= READ_REGISTER_ULONG(pdx,(u32)(CVBS_IN_BASE + 1 * PCIE_BARADDROFSIZE));
		if(IntState>0)
		{
			pdx->irq_total++;
			for(cnt =0; cnt <100; cnt ++)
			{
				if(IntState == 0) break;
				if((IntState&0x01) == 0x01) // CH0  done
				{
					Int_Value +=  0x01;
					/*
					 * Always sample the toggle: it used to be read only
					 * while the copy tasklet was idle, which missed
					 * frame ends. The IRQ thread does the copying.
					 */
					tmp = (READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE + (32+0) * PCIE_BARADDROFSIZE)))&0x01;
					if(pdx->video_toggle[0] != tmp)
					{
						struct hws_half_event ev = { .ts = ktime_get_ns(), .half = tmp };

						if(tmp == 0)	/* second half done: the frame is complete */
							hws_frame_end(pdx, 0, ev.ts);
						pdx->video_toggle[0] = tmp;
						pdx->irq_video++;
						/* a full queue means the thread is far behind; those halves are stale anyway */
						if (kfifo_put(&pdx->half_events, ev))
							wake_thread = true;
						else
							pdx->irq_fifo_full++;
					}
				}
			//-------
			//------------------------------

			if((IntState &0x100) == 0x100) // Audio ch0 done
			{

				Int_Value +=  0x100;
				pdx->irq_audio++;
				//printk("OnInterrupt Audio  %x\n", 0);
				if(pdx->m_nAudioBusy[0] ==0 )
				{
					tmp = (READ_REGISTER_ULONG(pdx,(CVBS_IN_BASE + (40+0) * PCIE_BARADDROFSIZE)))&0x01;
					pdx->m_nAudioBufferIndex[0] = tmp;
					pdx->audio_data[0]= pdx->m_nAudioBufferIndex[0];
					tasklet_schedule(&pdx->dpc_audio_tasklet[0]);
				}
			}
			//--------------
			/*
			 * Acknowledge exactly the bits that are set. Int_Value was
			 * summed across passes (0x01 + 0x01 = 0x02 on a second pass),
			 * which acknowledged the wrong bit, and bits other than 0 and 8
			 * (bit 12 is line-in audio on the 8591) were never acknowledged.
			 */
			if (IntState & ~0x101u) {
				pdx->irq_unknown++;
				pdx->irq_unknown_bits |= IntState & ~0x101u;
			}
			Int_Value = IntState & 0x00ffffff;
			WRITE_REGISTER_ULONG(pdx,(u32)(0x4000 + (PCIE_BARADDROFSIZE * 1)), Int_Value);
			IntState= READ_REGISTER_ULONG(pdx,(u32)(CVBS_IN_BASE + 1 * PCIE_BARADDROFSIZE));
			if(IntState == 0) break;
			}
			//printk("OnInterrupt IRQ_HANDLED  %X\n", pdev->device);
			return wake_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;
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
	kref_init(&lro->ref);	/* probe's reference, see hws_dev_release() */

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


/*
 * Register 9 caps an over-long quarter run (HWS_REG_DMA_MAX_SIZE) at the
 * largest programmed quarter, so a run with stale lengths ends at most a
 * quarter plus the 2 KiB block past its start: inside the 4.5 MiB DMA
 * buffers, and within the slack of a buffer the card writes directly
 * (stage 4.4). Never above the old limit, which kept clear of the audio
 * ring. Called wherever the quarter lengths are programmed.
 */
static void hws_set_dma_max(struct hws_pcie_dev *pdx)
{
	u32 max = 0, cap;
	int i;

	if (!pdx->m_DeviceHW_Version || !pdx->m_MaxHWVideoBufferSize)
		return;
	for (i = 0; i < 4; i++)
		max = max_t(u32, max, pdx->m_format[i].HLAF_SIZE);
	cap = pdx->m_MaxHWVideoBufferSize - MAX_AUDIO_CAP_SIZE - 2048;
	if (!max || max > cap)
		max = cap;
	WRITE_REGISTER_ULONG(pdx, HWS_REG_DMA_MAX_SIZE, max / 16);
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
			/* read back to flush the posted writes; the values aren't used */
			READ_REGISTER_ULONG(pdx,(PCI_ADDR_TABLE_BASE + cnt));
			READ_REGISTER_ULONG(pdx,(PCI_ADDR_TABLE_BASE + cnt+PCIE_BARADDROFSIZE));


			//--------------------------
			WRITE_REGISTER_ULONG(pdx,( CBVS_IN_BUF_BASE + (i*PCIE_BARADDROFSIZE)), ((i+1)*PCIEBAR_AXI_BASE)+PCI_Addr); //Buffer 1 address
			halfframeLength = pdx->m_format[i].HLAF_SIZE/16;
			WRITE_REGISTER_ULONG(pdx,( CBVS_IN_BUF_BASE2 + (i*PCIE_BARADDROFSIZE)),halfframeLength); //Buffer 1 address


			READ_REGISTER_ULONG(pdx,(  CBVS_IN_BUF_BASE + (i*PCIE_BARADDROFSIZE)));
			READ_REGISTER_ULONG(pdx,(  CBVS_IN_BUF_BASE2 + (i*PCIE_BARADDROFSIZE)));

			//---------------------------

		}
		cnt +=8;
		if(pdx->m_pbyAudioBuffer[i])
		{
			PhyAddr_A_Low = pdx->m_dwAudioBuffer[i];
			PhyAddr_A_High = pdx->m_dwAudioBufferHigh[i];
			PCI_Addr = (PhyAddr_A_Low&AddrLowmsk);
			PhyAddr_A_Low = (PhyAddr_A_Low&Addrmsk);
			//printk("[X1]Audio:PCI_Addr =%X\n",PCI_Addr);
			//printk("[X1]Audio:-------- - LOW=%X  HIGH =%X\n",pdx->m_dwAudioBuffer[i],pdx->m_dwAudioBufferHigh[i]);
			WRITE_REGISTER_ULONG(pdx,(CBVS_IN_BUF_BASE + ((8+i)*PCIE_BARADDROFSIZE)), ((i+1)*PCIEBAR_AXI_BASE+PCI_Addr)); //Buffer 1 address
			READ_REGISTER_ULONG(pdx,(CBVS_IN_BUF_BASE + ((8+i)*PCIE_BARADDROFSIZE)));
		}
	}
	WRITE_REGISTER_ULONG(pdx,INT_EN_REG_BASE, 0x3ffff); //enable PCI Interruput
	//WRITE_REGISTER_ULONG(PCIEBR_EN_REG_BASE, 0xFFFFFFFF);

}

//-----------------------------------
static int ChangeVideoSize(struct hws_pcie_dev *pdx,int ch,int w,int h,int interlace)
{
	struct hws_video *v = &pdx->video[ch];
	bool restart = false;
	int j;
	int halfframeLength[4];
	unsigned long flags;
	if(ch != 0) return -1;
	/*
	 * The card applies new lengths and register 9 to a run already in
	 * progress (6.8): a zero-copy buffer sized for the old input could
	 * overrun, and a DMA buffer gets the rest of an old-size quarter
	 * after the canary is refilled. Stop the engine first, so zero-copy
	 * buffers go back while it is off and the canary is filled at rest.
	 */
	if (pdx->m_bVCapStarted[ch]) {
		hws_video_quiesce(pdx, ch);
		restart = true;
		v->size_restarts++;
	}
	hws_zc_stop(pdx, restart);
	if(SetVideoFormteSize(pdx,ch,w,h) != 1)
	{
		if (restart)
			StartVideoCapture(pdx, ch);
		return -1;
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
	hws_set_dma_max(pdx);
	hws_fill_canary(pdx);
	if (restart)
		StartVideoCapture(pdx, ch);	/* the frame in flight is lost */
	return 0;
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
		/*
		 * Version-0 cards, and cards whose DMA buffers had to fall back to
		 * 4 MiB, capture 4096-wide input as 3840 wide (the vendor's v0
		 * behaviour) so a quarter frame always fits its buffer.
		 */
		if(res_w > pdx->m_MaxInputWidth) res_w = pdx->m_MaxInputWidth;
		if(pdx->m_DeviceHW_Version==0 && res_h > 2160) res_h = 2160;
		no_video = 0;
		if(((res_w <=MAX_VIDEO_HW_W) &&(res_h<=MAX_VIDEO_HW_H)&&(interlace==0))||((res_w <=MAX_VIDEO_HW_W) &&(res_h*2<=MAX_VIDEO_HW_H)&&(interlace==1)))
		{
			if((res_w !=pdx->m_pVCAPStatus[ch][0].dwWidth )||(res_h!= pdx->m_pVCAPStatus[ch][0].dwHeight)||(pdx->m_pVCAPStatus[ch][0].dwinterlace!=interlace))
			{
				if(ChangeVideoSize(pdx,ch,res_w,res_h,interlace) < 0)
					no_video = 1;
			}
		}
		else
		{
			no_video = 1;
		}
		if(no_video && pdx->m_UnsupportedRes != (((u32)res_w<<16)|res_h))
		{
			pdx->m_UnsupportedRes = ((u32)res_w<<16)|res_h;
			dev_warn(&pdx->pdev->dev, "unsupported input %dx%d%s, showing no-signal frames\n",
				 res_w, res_h, interlace ? "i" : "p");
		}
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
	/*
	 * The IRQ thread reads the geometry and quarter sizes this may change.
	 * A size change may stop and restart the engine, so engine_lock comes
	 * first, in the order hws_start_streaming_multi() takes them.
	 */
	mutex_lock(&pdx->video[i].engine_lock);
	mutex_lock(&pdx->video[i].cap_lock);
	pdx->m_curr_No_Video[i] = Get_Video_Status(pdx,i);
	if((pdx->m_curr_No_Video[i] ==0x1)&&(pdx->m_bVCapStarted[i]==TRUE))
		hws_deliver_no_video(pdx, i);
	mutex_unlock(&pdx->video[i].cap_lock);
	mutex_unlock(&pdx->video[i].engine_lock);
	//-----------------

	}

}

int MainKsThreadHandle(void *arg)
{
	int need_check=0;
	int i=0;
	unsigned int poll = 0;
	struct hws_pcie_dev *pdx = (struct hws_pcie_dev *)(arg);

	set_freezable();	/* don't poll a card that system sleep has powered down */
	while(!kthread_should_stop())
	{
		try_to_freeze();
		/* a card that lost its run state while capturing is re-initialised here too, not only at an audio start */
		for (i = 0; i < pdx->m_nCurreMaxVideoChl; i++)
			if (pdx->m_bVCapStarted[i] || pdx->m_bACapStarted[i])
				hws_request_reinit(pdx);
		hws_do_reinit(pdx);

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
		for(i=0; i<pdx->m_nCurreMaxVideoChl; i++)
		{
			bool hdcp = hws_hdcp_active(pdx, i);

			if(pdx->video[i].fileindex > 0)
				hws_check_source_change(&pdx->video[i]);
			if (hdcp != pdx->video[i].hdcp_active) {
				pdx->video[i].hdcp_active = hdcp;
				dev_info(&pdx->pdev->dev, "input %d: HDCP %s\n", i,
					 hdcp ? "active, delivering black frames" : "no longer active");
			}
		}
		if (poll++ % 5 == 0)	/* up to 14 MiB at 1080p: every 250 ms is enough */
			hws_check_canary(pdx);
		/*
		 * TASK_IDLE: ssleep() slept uninterruptibly, which counts toward
		 * the load average and kept it 1.0 higher for as long as the
		 * module was loaded. kthread_stop() still wakes this early.
		 * Every 50 ms, as the Windows driver polls: resolution changes
		 * are seen sooner, and with no signal the blue frames come
		 * faster than OBS's read timeout (5 frame periods, 83 ms at
		 * 60 fps), which it otherwise hit repeatedly at every signal
		 * change. Each poll is a few register reads.
		 */
		schedule_timeout_idle(max_t(long, 1, msecs_to_jiffies(50)));
	}
	//printk("MainKsThreadHandle Exit");
	return 0;
}
static int StartKSThread(struct hws_pcie_dev *pdx)
{
	struct task_struct *t = kthread_run(MainKsThreadHandle,(void*)pdx,"StartKSThread task");

	if (IS_ERR(t))
		return PTR_ERR(t);
	pdx->mMain_tsk = t;
	return 0;
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
			dev_warn(&pdev->dev, "MSI unavailable (%d), using a legacy interrupt\n", rc);
		lro->msi_enabled = rc >= 0;
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
		irq_flag = lro->msi_enabled ? 0 : IRQF_SHARED;

		rc = request_threaded_irq(pdev->irq, irqhandler, hws_irq_thread, irq_flag, pci_name(pdev), lro);
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
	/* re-applied here after suspend, which resets it */
	hws_set_dma_max(pdx);
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
	usleep_range(500, 600);	/* the Windows driver waits 500 us here (stage 4.9) */
	//DelayUs(500);
	m_Valude= 0x00FFFFFF;
	m_Valude |= 0x80000000;
	WRITE_REGISTER_ULONG(pdx,CVBS_IN_BASE, m_Valude);
	WRITE_REGISTER_ULONG(pdx,(CVBS_IN_BASE + (0 * PCIE_BARADDROFSIZE)), 0X13);
	pdx->m_bStartRun = 1;
	/* a re-init (CheckCardStatus) must not drop the user's picture settings */
	for(i=0; i<pdx->m_nCurreMaxVideoChl; i++)
	{
		if(pdx->video[i].m_PictureReg)
			WRITE_REGISTER_ULONG(pdx, HWS_PICTURE_REG(i), pdx->video[i].m_PictureReg);
	}
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

/*
 * debugfs, in <debugfs>/hws-<pci name>/:
 *   status    input, frame and interrupt counters, per-consumer counters
 *   dma       each DMA buffer's IOVA next to the address the card's
 *             registers point at; a mismatch means the card writes elsewhere
 *   regsnaps  the address registers at each capture enable and disable, to
 *             match against IO_PAGE_FAULT times in the kernel log
 * Reading them only reads address and status registers.
 */

/* Record the DMA address registers (called after each capture enable or disable). */
static void hws_snap_regs(struct hws_pcie_dev *pdx, char what, int en)
{
	struct hws_regsnap s;
	unsigned long flags;
	int i;

	if (!debug_dma || pdx->m_PciDeviceLost || !pdx->map_bar0_addr)
		return;
	s.ts = ktime_get_ns();
	s.what = what;
	s.en = en;
	s.ctl = READ_REGISTER_ULONG(pdx, CVBS_IN_BASE);
	s.ven = READ_REGISTER_ULONG(pdx, CVBS_IN_BASE + 2 * PCIE_BARADDROFSIZE);
	s.aen = READ_REGISTER_ULONG(pdx, CVBS_IN_BASE + 3 * PCIE_BARADDROFSIZE);
	for (i = 0; i < 4; i++) {
		s.tbl_hi[i] = READ_REGISTER_ULONG(pdx, PCI_ADDR_TABLE_BASE + 0x208 + 8 * i);
		s.tbl_lo[i] = READ_REGISTER_ULONG(pdx, PCI_ADDR_TABLE_BASE + 0x208 + 8 * i + 4);
		s.vbuf[i] = READ_REGISTER_ULONG(pdx, CBVS_IN_BUF_BASE + i * PCIE_BARADDROFSIZE);
		s.abuf[i] = READ_REGISTER_ULONG(pdx, CBVS_IN_BUF_BASE + (8 + i) * PCIE_BARADDROFSIZE);
	}
	spin_lock_irqsave(&pdx->snap_lock, flags);
	pdx->snaps[pdx->snap_next % HWS_REGSNAPS] = s;
	pdx->snap_next++;
	spin_unlock_irqrestore(&pdx->snap_lock, flags);
}

/*
 * The bus address a card-side buffer register value reaches: bits 31:29
 * pick translation window n + 1, whose table entry n supplies the upper
 * bits; the low 29 bits are the offset. Returns false for an address
 * outside windows 1-4.
 */
static bool hws_card_addr(const u32 *hi, const u32 *lo, u32 reg, u64 *bus)
{
	u32 win = reg >> 29;

	if (win < 1 || win > 4)
		return false;
	*bus = ((u64)hi[win - 1] << 32) | (lo[win - 1] & PCI_E_BAR_ADD_MASK) | (reg & PCI_E_BAR_ADD_LOWMASK);
	return true;
}

static int hws_dbg_status_show(struct seq_file *m, void *unused)
{
	struct hws_pcie_dev *pdx = m->private;
	struct hws_video *v = &pdx->video[0];
	struct hws_vfh_ctx *ctx;
	unsigned long flags;
	int w, h, il, n = 0;

	if (hws_read_input(pdx, 0, &w, &h, &il))
		seq_printf(m, "input: %dx%d%s\n", w, h, il ? " interlaced" : "");
	else
		seq_puts(m, "input: no usable signal\n");
	seq_printf(m, "measured frame interval: %llu ns, card register 110: %u fps, HDCP: %s\n",
		   READ_ONCE(pdx->m_FrameIntervalNs[0]), hws_hw_fps(pdx, 0),
		   hws_hdcp_active(pdx, 0) ? "active" : "not active");
	seq_printf(m, "capture: %s, output %dx%d\n", pdx->m_bVCapStarted[0] ? "running" : "stopped",
		   v->current_out_width, v->curren_out_height);
	seq_printf(m, "frames: %u, lost: %u\n", READ_ONCE(pdx->m_FrameSeq[0]), READ_ONCE(pdx->m_FramesDropped[0]));
	seq_printf(m, "interrupts: %u, video halves: %u, audio packets: %u, half queue full: %u, other sources: %u (bits %#x)\n",
		   READ_ONCE(pdx->irq_total), READ_ONCE(pdx->irq_video),
		   READ_ONCE(pdx->irq_audio), READ_ONCE(pdx->irq_fifo_full),
		   READ_ONCE(pdx->irq_unknown), READ_ONCE(pdx->irq_unknown_bits));
	seq_printf(m, "copy path: frames failed because a copy ended late: %u\n", READ_ONCE(v->late_copies));
	seq_printf(m, "engine restarts for a size change: %u\n", v->size_restarts);
	seq_printf(m, "zero-copy: %s; frames %u, failed %u, late writes %u, buffers outside the window %u, held copy buffers failed %u%s\n",
		   hws_zc_unavailable(pdx) ?: (zero_copy ? "on" : "off"),
		   v->zc_frames, v->zc_errors, v->zc_late, v->zc_outside, v->zc_held_failed,
		   v->zc_cur || v->zc_next ? ", in use" : "");
	seq_printf(m, "audio: gaps filled with silence: %u, silence packets: %u%s\n",
		   READ_ONCE(pdx->audio[0].silence_gaps), READ_ONCE(pdx->audio[0].silence_pkts),
		   READ_ONCE(pdx->audio[0].in_gap) ? ", in a gap now" : "");
	spin_lock_irqsave(&v->consumers_lock, flags);
	list_for_each_entry(ctx, &v->consumers, node) {
		n++;
		seq_printf(m, "consumer %d: %s, every %u frame(s), delivered %u, no buffer queued %u%s\n",
			   n, ctx->streaming ? "streaming" : "idle", ctx->decimate,
			   ctx->delivered, ctx->missed, ctx->held ? ", holds half a frame" : "");
	}
	spin_unlock_irqrestore(&v->consumers_lock, flags);
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(hws_dbg_status);

static int hws_dbg_dma_show(struct seq_file *m, void *unused)
{
	struct hws_pcie_dev *pdx = m->private;
	u32 hi[4], lo[4], vreg, areg;
	u64 bus;
	int i;

	if (pdx->m_PciDeviceLost) {
		seq_puts(m, "card not responding\n");
		return 0;
	}
	for (i = 0; i < 4; i++) {
		hi[i] = READ_REGISTER_ULONG(pdx, PCI_ADDR_TABLE_BASE + 0x208 + 8 * i);
		lo[i] = READ_REGISTER_ULONG(pdx, PCI_ADDR_TABLE_BASE + 0x208 + 8 * i + 4);
	}
	seq_printf(m, "buffer size %d KiB, audio ring %d KiB at the end of each, register 9 %#x (limit at offset %#x)\n",
		   pdx->m_MaxHWVideoBufferSize / 1024, MAX_AUDIO_CAP_SIZE / 1024,
		   READ_REGISTER_ULONG(pdx, HWS_REG_DMA_MAX_SIZE), READ_REGISTER_ULONG(pdx, HWS_REG_DMA_MAX_SIZE) * 16);
	for (i = 0; i < pdx->m_nMaxChl; i++) {
		if (!pdx->m_pbyVideoBuffer[i])
			continue;
		vreg = READ_REGISTER_ULONG(pdx, CBVS_IN_BUF_BASE + i * PCIE_BARADDROFSIZE);
		areg = READ_REGISTER_ULONG(pdx, CBVS_IN_BUF_BASE + (8 + i) * PCIE_BARADDROFSIZE);
		seq_printf(m, "buffer %d: IOVA %pad-%llx, table %08x:%08x, quarter %u bytes\n", i,
			   &pdx->m_pbyVideo_phys[i],
			   (u64)pdx->m_pbyVideo_phys[i] + pdx->m_MaxHWVideoBufferSize - 1,
			   hi[i], lo[i], READ_REGISTER_ULONG(pdx, CBVS_IN_BUF_BASE2 + i * PCIE_BARADDROFSIZE) * 16);
		if (hws_card_addr(hi, lo, vreg, &bus))
			seq_printf(m, "  video register %08x -> %llx%s\n", vreg, bus,
				   bus == (u64)pdx->m_pbyVideo_phys[i] ? "" : "  MISMATCH");
		else
			seq_printf(m, "  video register %08x -> outside the windows\n", vreg);
		if (hws_card_addr(hi, lo, areg, &bus))
			seq_printf(m, "  audio register %08x -> %llx%s\n", areg, bus,
				   bus == (u64)pdx->m_pbyAudio_phys[i] ? "" : "  MISMATCH");
		else
			seq_printf(m, "  audio register %08x -> outside the windows\n", areg);
		if (i < MAX_VID_CHANNELS && pdx->canary_start[i]) {
			u32 *c = (u32 *)(pdx->m_pbyVideoBuffer[i] + pdx->canary_start[i]);
			int words = (pdx->m_MaxHWVideoBufferSize - MAX_AUDIO_CAP_SIZE - pdx->canary_start[i]) / 4;
			int k, changed = 0, lo_k = -1, hi_k = -1;

			for (k = 0; k < words; k++) {
				if (READ_ONCE(c[k]) != HWS_CANARY) {
					changed++;
					if (lo_k < 0)
						lo_k = k;
					hi_k = k;
				}
			}
			if (pdx->canary_hits[i])
				seq_printf(m, "  stray writes caught: %u, last at IOVA %llx-%llx\n", pdx->canary_hits[i],
					   pdx->canary_last_lo[i], pdx->canary_last_hi[i]);
			if (!changed)
				seq_printf(m, "  gap %#x-%#x: untouched\n", pdx->canary_start[i],
					   pdx->m_MaxHWVideoBufferSize - MAX_AUDIO_CAP_SIZE);
			else
				seq_printf(m, "  gap %#x-%#x: %d words written, IOVA %llx-%llx  STRAY WRITE\n",
					   pdx->canary_start[i], pdx->m_MaxHWVideoBufferSize - MAX_AUDIO_CAP_SIZE, changed,
					   (u64)pdx->m_pbyVideo_phys[i] + pdx->canary_start[i] + lo_k * 4,
					   (u64)pdx->m_pbyVideo_phys[i] + pdx->canary_start[i] + hi_k * 4 + 3);
		}
	}
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(hws_dbg_dma);

static int hws_dbg_regsnaps_show(struct seq_file *m, void *unused)
{
	struct hws_pcie_dev *pdx = m->private;
	struct hws_regsnap *snaps;
	unsigned long flags;
	u32 next, i, j, first;

	snaps = kmalloc_array(HWS_REGSNAPS, sizeof(*snaps), GFP_KERNEL);
	if (!snaps)
		return -ENOMEM;
	spin_lock_irqsave(&pdx->snap_lock, flags);
	memcpy(snaps, pdx->snaps, HWS_REGSNAPS * sizeof(*snaps));
	next = pdx->snap_next;
	spin_unlock_irqrestore(&pdx->snap_lock, flags);
	first = next > HWS_REGSNAPS ? next - HWS_REGSNAPS : 0;
	for (i = first; i < next; i++) {
		struct hws_regsnap *s = &snaps[i % HWS_REGSNAPS];

		seq_printf(m, "[%5llu.%06llu] %s %s: reg0 %08x video-en %08x audio-en %08x\n",
			   s->ts / NSEC_PER_SEC, (s->ts % NSEC_PER_SEC) / NSEC_PER_USEC,
			   s->what == 'V' ? "video" : "audio", s->en ? "enable " : "disable",
			   s->ctl, s->ven, s->aen);
		for (j = 0; j < 4; j++)
			seq_printf(m, "    %u: table %08x:%08x video %08x audio %08x\n", j,
				   s->tbl_hi[j], s->tbl_lo[j], s->vbuf[j], s->abuf[j]);
	}
	kfree(snaps);
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(hws_dbg_regsnaps);

/*
 * debug_dma only: writing anything stops the card's run state (register 0 =
 * 0x10, as StopDsp() does), to test the re-init path of hws_request_reinit()
 * by starting audio afterwards.
 */
static ssize_t hws_dbg_reset_write(struct file *f, const char __user *buf, size_t n, loff_t *pos)
{
	struct hws_pcie_dev *pdx = file_inode(f)->i_private;

	if (!pdx->m_PciDeviceLost)
		WRITE_REGISTER_ULONG(pdx, CVBS_IN_BASE, 0x10);
	return n;
}

/* Treat the K-th bottom half from now as stale (6.11 test), as if the card hadn't written it. */
static ssize_t hws_dbg_stale_write(struct file *f, const char __user *buf, size_t n, loff_t *pos)
{
	struct hws_pcie_dev *pdx = file_inode(f)->i_private;
	unsigned int v;
	int ret = kstrtouint_from_user(buf, n, 0, &v);

	if (ret)
		return ret;
	WRITE_ONCE(pdx->dbg_stale_bottom, v);
	return n;
}

static const struct file_operations hws_dbg_stale_fops = {
	.owner = THIS_MODULE,
	.write = hws_dbg_stale_write,
	.llseek = noop_llseek,
};

static const struct file_operations hws_dbg_reset_fops = {
	.owner = THIS_MODULE,
	.write = hws_dbg_reset_write,
	.llseek = noop_llseek,
};

static void hws_debugfs_init(struct hws_pcie_dev *pdx)
{
	char name[32];

	snprintf(name, sizeof(name), "hws-%s", pci_name(pdx->pdev));
	pdx->dbg = debugfs_create_dir(name, NULL);
	debugfs_create_file("status", 0444, pdx->dbg, pdx, &hws_dbg_status_fops);
	debugfs_create_file("dma", 0444, pdx->dbg, pdx, &hws_dbg_dma_fops);
	if (debug_dma) {
		debugfs_create_file("regsnaps", 0444, pdx->dbg, pdx, &hws_dbg_regsnaps_fops);
		debugfs_create_file("simulate_reset", 0200, pdx->dbg, pdx, &hws_dbg_reset_fops);
		debugfs_create_file("stale_bottom_halves", 0200, pdx->dbg, pdx, &hws_dbg_stale_fops);
	}
}

static int hws_probe(struct pci_dev *pdev, const struct pci_device_id *pci_id)
{
	struct hws_pcie_dev *gdev;
	int i, j, ret;

	gdev = alloc_dev_instance(pdev);
	if (!gdev)
		return -ENOMEM;
	gdev->dwDeviceID = pdev->device;
	gdev->dwVendorID = pdev->vendor;
	printk("MV360: Device =%X VID =%X\n",gdev->dwDeviceID,gdev->dwVendorID);
	if (pdev->vendor != 0x8888 || pdev->device != 0x8581)
		dev_warn(&pdev->dev, "model %04x:%04x is untested with this driver; only 8888:8581 is tested\n",
			 pdev->vendor, pdev->device);

	ret = pci_enable_device(pdev);
	if (ret) {
		dev_err(&pdev->dev, "pci_enable_device failed: %d\n", ret);
		goto err_free;
	}
	pci_set_master(pdev);
	probe_scan_for_msi(gdev, pdev);	/* falls back to INTx */
#ifdef CONFIG_ARCH_TI816X
	pcie_set_readrq(pdev, 128);	/* known root complex's max read request size */
#endif
	gdev->info.mem[0].internal_addr = ioremap(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
	if (!gdev->info.mem[0].internal_addr) {
		ret = -ENOMEM;
		goto err_msi;
	}
	gdev->map_bar0_addr = (u32 *)gdev->info.mem[0].internal_addr;
	gdev->info.mem[0].size = pci_resource_len(pdev, 0);
	gdev->info.mem[0].memtype = UIO_MEM_PHYS;

	/* everything the interrupt handlers and the status thread use exists before they can run */
	INIT_KFIFO(gdev->half_events);
	spin_lock_init(&gdev->snap_lock);
	ReadChipId(gdev);
	for (i = 0; i<MAX_VID_CHANNELS; i++)
	{
		mutex_init(&gdev->video[i].cap_lock);
		mutex_init(&gdev->video[i].engine_lock);	/* the status thread's re-init takes it */
		INIT_LIST_HEAD(&gdev->video[i].frame_bufs);
		INIT_LIST_HEAD(&gdev->video[i].zc_release);
		SetVideoFormteSize(gdev,i,1920,1080);
		for (j = 0; j<MAX_VIDEO_QUEUE; j++)
		{
			gdev->m_pVCAPStatus[i][j].byLock = MEM_UNLOCK;
			gdev->m_pVCAPStatus[i][j].byPath = 2;
			gdev->m_pVCAPStatus[i][j].dwWidth = 1920 ;
			gdev->m_pVCAPStatus[i][j].dwHeight = 1080;
		}
		spin_lock_init(&gdev->videoslock[i]);
		spin_lock_init(&gdev->audiolock[i]);
		for(j=0; j<MAX_AUDIO_QUEUE;j++)
			gdev->m_AudioInfo[i].pStatusInfo[j].byLock = MEM_UNLOCK;
	}
	tasklet_init(&gdev->dpc_audio_tasklet[0],DpcForIsr_Audio0,(unsigned long)gdev);
	gdev->auwq = alloc_ordered_workqueue("hwsuhdx1-audio", 0);
	if (!gdev->auwq) {
		ret = -ENOMEM;
		goto err_unmap;
	}
	if (DmaMemAllocPool(gdev) != 0) {
		ret = -ENOMEM;
		goto err_wq;
	}
	ret = irq_setup(gdev, pdev);
	if (ret) {
		dev_err(&pdev->dev, "could not get IRQ %d: %d\n", pdev->irq, ret);
		goto err_dma;
	}
	InitVideoSys(gdev,0);
	hws_fill_canary(gdev);
	ret = StartKSThread(gdev);
	if (ret)
		goto err_irq;
	hws_adapters_init(gdev);
	if (hws_video_register(gdev)) {	/* unregisters what it registered on failure */
		ret = -ENODEV;
		goto err_thread;
	}
	if (hws_audio_register(gdev)) {	/* likewise */
		ret = -ENODEV;
		goto err_video;
	}
	gdev->zc_iommu = hws_iommu_translates(&pdev->dev);
	hws_debugfs_init(gdev);
	return 0;

err_video:
	StopKSThread(gdev);	/* it updates the video controls; a no-op when reached again below */
	hws_unregister_video(gdev);
err_thread:
	StopKSThread(gdev);
err_irq:
	StopDevice(gdev);
	irq_teardown(gdev);
err_dma:
	DmaMemFreePool(gdev);
err_wq:
	tasklet_kill(&gdev->dpc_audio_tasklet[0]);
	destroy_workqueue(gdev->auwq);
err_unmap:
	gdev->m_Removed = true;
	iounmap(gdev->info.mem[0].internal_addr);
err_msi:
	if (gdev->msi_enabled)
		pci_disable_msi(pdev);
	pci_disable_device(pdev);
err_free:
	pci_set_drvdata(pdev, NULL);
	hws_put_dev(gdev);
	return ret;
}

MODULE_DEVICE_TABLE(pci, hws_pci_table);

/*
 * System sleep. The card loses its registers in S3 (run state, DMA address
 * table, buffer addresses and lengths), and re-running InitVideoSys() alone
 * does not bring it back, so resume replays the probe-time reset (ReadChipId)
 * and init before restarting whatever was capturing. The PCI core saves and
 * restores config space (BARs, bus mastering, MSI) around these callbacks.
 */
static int hws_suspend(struct device *d)
{
	struct hws_pcie_dev *pdx = dev_get_drvdata(d);
	int i;

	for (i = 0; i < pdx->m_nCurreMaxVideoChl; i++) {
		pdx->m_ResumeVideo[i] = pdx->m_bVCapStarted[i];
		if (pdx->m_bVCapStarted[i])
			hws_video_quiesce(pdx, i);	/* zero-copy buffers go back below */
		if (pdx->m_bACapStarted[i])
			EnableAudioCapture(pdx, i, 0);
	}
	if (pdx->irq_line != -1)
		synchronize_irq(pdx->irq_line);
	mutex_lock(&pdx->video[0].cap_lock);
	hws_zc_stop(pdx, true);		/* capture is off: no zero-copy buffer stays in flight */
	mutex_unlock(&pdx->video[0].cap_lock);
	if (pdx->auwq)
		flush_workqueue(pdx->auwq);
	return 0;
}

static int hws_resume(struct device *d)
{
	struct hws_pcie_dev *pdx = dev_get_drvdata(d);
	int i;

	WRITE_REGISTER_ULONG(pdx, CVBS_IN_BASE, 0x0);
	WRITE_REGISTER_ULONG(pdx, CVBS_IN_BASE, 0x10);
	pdx->m_bStartRun = 0;
	InitVideoSys(pdx, 0);
	for (i = 0; i < pdx->m_nCurreMaxVideoChl; i++) {
		pdx->m_FrameEndNs[i] = 0;	/* restart the frame-rate window */
		if (pdx->m_ResumeVideo[i]) {
			pdx->m_bVCapStarted[i] = 0;
			StartVideoCapture(pdx, i);
		}
	}
	dev_info(d, "resumed, card re-initialised\n");
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0)
static DEFINE_SIMPLE_DEV_PM_OPS(hws_pm_ops, hws_suspend, hws_resume);
#define HWS_PM_OPS pm_sleep_ptr(&hws_pm_ops)
#else
static SIMPLE_DEV_PM_OPS(hws_pm_ops, hws_suspend, hws_resume);
#define HWS_PM_OPS (&hws_pm_ops)
#endif

static struct pci_driver hws_pci_driver = {
	.name        = KBUILD_MODNAME,
	.id_table    = hws_pci_table,
	.probe       = hws_probe,
	.remove      = hws_remove,
	.driver.pm   = HWS_PM_OPS,
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
MODULE_AUTHOR("Alex Liu <alex.liu@longtimetech.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
