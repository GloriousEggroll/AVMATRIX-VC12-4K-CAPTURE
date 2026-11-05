/*

*/

#ifndef _HWS_PCIE_H_
#define _HWS_PCIE_H_

#include <linux/module.h>
#include <linux/delay.h>

#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-dv-timings.h>
#include <linux/v4l2-dv-timings.h>	/* V4L2_INIT_BT_TIMINGS */
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-sg.h>
#include <linux/uio_driver.h>

#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>

#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/splice.h>
#include <linux/version.h>
#include <linux/uio.h>
#include <linux/ioctl.h>
#include <linux/semaphore.h>   
#include <linux/wait.h>
#include <linux/kfifo.h>
#include <linux/debugfs.h>
#include <linux/kref.h>
#include <linux/freezer.h>   


//---------------------------------------------
#define MAX_USER_IRQ 1
#define XDMA_CHANNEL_NUM_MAX (1)
#define MAX_NUM_ENGINES (XDMA_CHANNEL_NUM_MAX * 2)

#define  PCIE_BARADDROFSIZE 4


#define PCI_BUS_ACCESS_BASE 0x0
#define PCIEBAR_AXI_BASE 0x20000000

#define CTL_REG_ACC_BASE 0x0
#define PCI_ADDR_TABLE_BASE CTL_REG_ACC_BASE

#define INT_EN_REG_BASE  PCI_BUS_ACCESS_BASE+0x134
#define PCIEBR_EN_REG_BASE  PCI_BUS_ACCESS_BASE+0x148
#define PCIE_INT_DEC_REG_BASE PCI_BUS_ACCESS_BASE+0x138


#define CVBS_IN_BASE 0x04000
#define CBVS_IN_BUF_BASE CVBS_IN_BASE+ 16*PCIE_BARADDROFSIZE
#define CBVS_IN_BUF_BASE2 CVBS_IN_BASE+ 50*PCIE_BARADDROFSIZE

#define MAX_L_VIDEO_SIZE			0x200000	//2M

#define MAX_VIDEO_PKSIZE            2
#define HALF_VIDEO_PKSIZE           1



#define PCI_E_BAR_PAGE_SIZE 0x20000000
#define PCI_E_BAR_ADD_MASK 0xE0000000
#define PCI_E_BAR_ADD_LOWMASK 0x1FFFFFFF

#define MAX_DMA_AUDIO_PK_SIZE      128*16*2
#define HWS_AUDIO_STALE_PKTS       2	/* old packets the card sends after an audio enable */

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif 
/* audio queue slot states (ACAP_STATUS_INFO.byLock) */
#define MEM_LOCK					1	/* holds a packet nobody has read */
#define MEM_UNLOCK					0	/* free */


typedef unsigned int DWORD;
//typedef int LONG;
typedef unsigned int ULONG;
typedef unsigned char BYTE;
typedef unsigned char BOOL;

#define PAGESIZE					4096


#define MAX_AUDIO_CAP_SIZE			10*1024			// 16KHz*2Byte*8channel/4(0.25sec)/4096(pagesize)
#define MAX_AUDIO_QURE_INDEX        8
#define MAX_AUDIO_LINE   4
#define MAX_AUDIO_SUBCHANNEL		24
#define MAX_DMA_PACK_SIZE 160


#define MAX_AUDIO_EVENT_CNT 24
#define MAX_AUDIO_BUF_CNT 50
#define MAX_AUDIO_QUEUE_CNT 20
#define MAX_AUDIO_BUF_SIZE 1920

#define MAX_VID_CHANNELS            4
#define MAX_VIDEO_QUEUE				4
#define MAX_AUDIO_QUEUE				8

#define CUR_AUDIO_PACKET_LENGTH		4096+34						// 250ms,  2*16,000=32,000 -> 1s
#define AUDIO_ONE_QUEUE_SIZE		8*CUR_AUDIO_PACKET_LENGTH

#define MAX_MM_VIDEO_SIZE			0x400000	//4M
/*
 * Each DMA buffer holds one quarter of a frame plus the audio ring at its end.
 * A 4096x2160 quarter is 4,425,728 bytes, so 4 MiB only fits 3840x2160; 4.5 MiB
 * fits both. Larger than MAX_PAGE_ORDER, so it needs IOMMU translation or CMA.
 */
#define MAX_MM_VIDEO_SIZE_DCI		0x480000	//4.5M

#define MAX_VIDEO_HW_W 4096
#define MAX_VIDEO_HW_H 2160

typedef struct tagDATA_BUFFER_MSG
{
	DWORD dwHandle;
	/////////////////////////
	unsigned char m_bVideo;
	int    nSubNum;
	unsigned char   nEvenOrOdd;  // 0x00 even  0x01 odd
	unsigned int    nWidth;
	unsigned int    nHeight;
	unsigned int    nPatch;
	unsigned int     nSize;
	unsigned char   *pDataBuffer;
	unsigned char * pBuffer;
}DATA_BUFFER_MSG,*LPDATA_BUFFER_MSG;

typedef struct tagWRBUF
{
	BYTE    *pDataBuffer;
	int     length;
	int     Lock;
} WRBUF, * PWRBUF;


typedef struct TAG_ACAP_STATUS_INFO
{
	DWORD       dwLength;
	DWORD		byLock;
}ACAP_STATUS_INFO, *PACAP_STATUS_INFO;

typedef struct TAG_VCAP_STATUS_INFO
{
	DWORD		byLock;
	DWORD		byChannel;
	DWORD		bySize;
	DWORD		byField;
	DWORD		byPath;
	DWORD       dwWidth; 
	DWORD       dwHeight;
	DWORD	    dwinterlace;
}VCAP_STATUS_INFO, *PVCAP_STATUS_INFO;
typedef struct 
{
	BYTE		*m_pVideoStaging;	/* raw frame, for interlaced input or scaled output */
	BYTE		*m_pVideoScalerBuf;	/* line-doubled interlaced frame awaiting the scaler */
	DWORD		dwisRuning;
}ACAP_VIDEO_INFO, *PACAP_VIDEO_INFO;

typedef struct 
{
	uint8_t		*m_pAudioBufData[MAX_AUDIO_QUEUE];
	ACAP_STATUS_INFO pStatusInfo[MAX_AUDIO_QUEUE];
	DWORD           m_nAudioIndex;
	DWORD		dwisRuning;
}ACAP_AUDIO_INFO, *PACAP_AUDIO_INFO;




typedef enum 
{
   StandardNone				= 0x80000000,
   StandardNTSC				= 0x00000001,
   StandardPAL				= 0x00000002,
   StandardSECAM			= 0x00000004,
} VideoStandard_t;  

typedef struct tagRECT
{
    int    left;
    int    top;
    int    right;
    int    bottom;
} RECT, *PRECT;


typedef struct tagVIDEO_INFO
{
   	DWORD	nChannel;		//0~3
	DWORD	nStandard;		// 0: NTSC, 1:PAL
	DWORD	bOddOnly;		// TRUE : Odd, Even - FALSE : odd
	DWORD	dwWidth;		// Image Width
	DWORD	dwHeight;		// Image Height
	DWORD	nFrame1;		// NTSC : 1~30, PAL : 101~125(1~25)
	DWORD	HLAF_SIZE;		 
	DWORD	DWON_SIZE;		
}VIDEO_INFO,*PVIDEO_INFO;



//--------------------

#define HWS_PCIE_WRITE(__addr, __offst, __data)	writel((__data), (dev->mmio + (__addr + __offst)))
#define HWS_PCIE_READ(__addr, __offst)		readl((dev->mmio + (__addr + __offst)))


#define	UNSET	(-1U)

struct hws_pcie_dev;
struct hws_adapter;

/* DMA address registers, recorded each time capture is enabled or disabled (debugfs regsnaps) */
#define HWS_REGSNAPS 16
struct hws_regsnap {
	u64	ts;		/* CLOCK_MONOTONIC */
	char	what;		/* 'V' video, 'A' audio */
	u8	en;
	u32	ctl, ven, aen;	/* registers 0, 2, 3 */
	u32	tbl_hi[4], tbl_lo[4];	/* address-translation entries */
	u32	vbuf[4], abuf[4];	/* registers 16 + i and 24 + i */
};

/* one half-frame completion, queued by the hard IRQ handler */
struct hws_half_event {
	u64	ts;	/* CLOCK_MONOTONIC when the interrupt arrived */
	u8	half;	/* 1: top half (DMA buffers 0, 1) done; 0: bottom half (2, 3), frame complete */
};


/* buffer for one video frame */
/* zero-copy buffers end this far past the frame: an over-long last quarter run ends within a quarter + 2 KiB (register 9) */
#define HWS_ZC_SLACK	8192

struct hwsvideo_buffer {
	/* common v4l buffer stuff -- must be first */
	struct vb2_v4l2_buffer	vb;
	struct list_head		queue;
	void *					mem;
	/* zero-copy (stage 4.4): the card writes the frame here; see hws_zc_program() */
	dma_addr_t				dma;		/* 0 unless the queue is dma-contig */
	bool					zc_in[2];	/* pointed at by quarter pair p (0 = top) in time */
	bool					zc_away_in[2];	/* pair p moved on from it in time */
	u32						zc_free_after[2];	/* card can't write it once video->zc_ev[p] reaches this */
	enum vb2_buffer_state	zc_state;	/* how it completes once the card is done with it */
};

struct hws_dmabuf{
	unsigned int			size;
	__le32					*cpu;
	dma_addr_t				dma;	
};



/*
 * Multi-consumer video streaming context.
 * One ctx per open() file handle, each owns an independent vb2_queue.
 */
struct hws_vfh_ctx {
    struct v4l2_fh      fh;
    struct vb2_queue    vbq;
    struct list_head    buf_queue;   /* queued hwsvideo_buffer for this fh */
    spinlock_t          qlock;       /* protects buf_queue */
    struct mutex        qmutex;      /* vb2 q->lock on kernels >= 7.0 (wait_prepare/finish removed) */
    bool                streaming;
    atomic_t            inflight;    /* buffers the worker popped but has not completed */
    wait_queue_head_t   inflight_wq; /* stop_streaming waits here for inflight == 0 */
    u32                 decimate;    /* deliver every Nth input frame (S_PARM), 1..4 */
    u32                 next_seq;    /* first frame sequence this consumer still wants */
    struct hwsvideo_buffer *held;    /* buffer receiving the current frame, under video->cap_lock */
    u32                 delivered;   /* frames completed into this consumer's buffers */
    u32                 missed;      /* frames it wanted but had no buffer queued for */
    struct hws_video   *video;
    struct list_head    node;        /* link into video->consumers */
};
struct hws_video{
	struct hws_pcie_dev		*dev;
	struct v4l2_device		v4l2_dev;
	struct video_device		vdev;
	int                     fileindex;
	/* B1: multi-consumer */
	spinlock_t			consumers_lock;
	struct mutex			ioctl_lock;	/* non-buffer ioctls (shared format/state) */
	struct mutex			engine_lock;	/* StartVideoCapture/StopVideoCapture */
	struct list_head		consumers;
	atomic_t			engine_users;
	u32						pixfmt; //V4L2_PIX_FMT_YUYV(fourcc)
	int						index;
	/*
	 * Frame assembly. The IRQ thread copies each half of a frame straight
	 * into one buffer per consumer (frame_bufs, linked through
	 * hwsvideo_buffer.queue); cap_lock also serialises the status thread's
	 * no-signal frames and input-size changes, and stop_streaming.
	 */
	struct mutex			cap_lock;
	struct list_head		frame_bufs;
	bool					top_done;	/* the top half of the current frame is in frame_bufs */
	int						frame_w, frame_h, frame_il;	/* geometry at the top half */
	int						frame_len[4];	/* bytes in each DMA quarter */
	bool					frame_hdcp;	/* HDCP was active at this frame's top half */
	bool					hdcp_active;	/* last HDCP state the status thread logged */
	/*
	 * Zero-copy (stage 4.4), under cap_lock. zc_prog[p] is the buffer quarter
	 * pair p (0 = top, 1 = bottom) points at in the card's registers, NULL
	 * for the DMA buffers. zc_cur receives the frame in progress, zc_next the
	 * one after it. A buffer the card may still write waits on zc_release.
	 */
	struct hws_vfh_ctx		*zc_ctx;
	struct hwsvideo_buffer	*zc_prog[2], *zc_cur, *zc_next;
	struct list_head		zc_release;
	u32						zc_ev[2];	/* half-done interrupts seen per pair */
	u32						zc_stable;	/* complete frames since the lengths last changed */
	u64						zc_last_ts, zc_gap_prev;	/* for the in-time margin */
	u32						zc_frames, zc_late, zc_errors, zc_outside;	/* debugfs */
	u32						size_restarts;	/* debugfs */
	u32						zc_held_failed;	/* debugfs: 6.11 path */
	bool					frame_late;	/* the top half's copy ended late (hws_copy_late) */
	u32						late_copies;	/* frames failed for a late copy (debugfs) */
	//------------------------
	struct v4l2_ctrl_handler ctrl_hdl;
	struct v4l2_ctrl *ctrl_brightness;	/* cluster: brightness, contrast, saturation */
	struct v4l2_ctrl *ctrl_contrast;
	struct v4l2_ctrl *ctrl_saturation;
	struct v4l2_ctrl *ctrl_power;		/* V4L2_CID_DV_RX_POWER_PRESENT, read-only */
	u32 m_PictureReg;	/* last value written to the picture register, 0 = never */
	struct v4l2_dv_timings dv_timings;	/* G/S_DV_TIMINGS */
	/* last input state seen by the status thread, for source-change events */
	bool src_signal;
	int src_w, src_h, src_il;
	//------------------------
	int current_out_width;
	int curren_out_height;
	int current_out_pixfmt;
};
	
struct hws_audio{
	struct hws_pcie_dev		*dev;
	struct snd_card 		*card;	
	struct snd_pcm_substream *substream;
	struct work_struct		audiowork;
	int						pos;
	int						index;
	int                         ring_offsize;
	int                         ring_over_size;
	void                        *resampled_buf;
	u32                         resampled_buf_size;
	/* silence while the card sends no audio, see hws_audio_silence_work() */
	struct delayed_work         silence_work;
	bool                        running;		/* between trigger start and stop */
	bool                        in_gap;
	u64                         last_pkt_ns;	/* last packet from the card, or the start */
	u64                         silence_frames;	/* silence delivered since last_pkt_ns */
	u32                         silence_gaps;	/* gaps filled (debugfs) */
	u32                         silence_pkts;	/* packets of silence delivered (debugfs) */
	spinlock_t                  ring_lock;
    uint32_t                    ring_wpos_byframes;
    uint32_t                    ring_size_byframes;
    uint32_t                    period_size_byframes;
    uint32_t                    period_used_byframes;
	u32                         sample_rate_out;
    u16                         channels;
    u16                         bits_per_sample;
};
	

struct hws_pcie_dev {
	struct pci_dev			*pdev;
	struct uio_info info;
	struct hws_audio		audio[MAX_VID_CHANNELS];
	struct hws_video		video[MAX_VID_CHANNELS];
	struct work_struct		video_work;
	struct work_struct		audio_work;
	spinlock_t				videoslock[MAX_VID_CHANNELS];
	spinlock_t				audiolock[MAX_VID_CHANNELS];
	//----------------------------
	u32 *map_bar0_addr;
	struct workqueue_struct *auwq;
	/* half-frame completions from the hard IRQ handler to the IRQ thread (channel 0) */
	DECLARE_KFIFO(half_events, struct hws_half_event, 16);
	int video_toggle[MAX_VID_CHANNELS];	/* last half-frame toggle seen, -1 = none */
	/* debugfs (stage 3.5, 3.6) */
	struct dentry *dbg;
	u32 irq_total, irq_video, irq_audio, irq_fifo_full;	/* updated only by irqhandler */
	u32 irq_unknown, irq_unknown_bits;	/* interrupt sources other than bits 0 and 8 */
	spinlock_t snap_lock;
	struct hws_regsnap snaps[HWS_REGSNAPS];
	u32 snap_next;
	u32 canary_start[MAX_VID_CHANNELS];	/* first canary byte in each DMA buffer, 0 = none */
	u32 canary_hits[MAX_VID_CHANNELS];	/* stray writes found by the status thread */
	u64 canary_last_lo[MAX_VID_CHANNELS], canary_last_hi[MAX_VID_CHANNELS];	/* IOVA range of the last */
 	unsigned long audio_data[MAX_VID_CHANNELS];
 	struct tasklet_struct dpc_audio_tasklet[MAX_VID_CHANNELS];
	//-----------------------------
	//spinlock_t lock; /* guards command register accesses */
	int irq_count;	/* interrupt counter */	
	int irq_line;		/* flag if irq allocated successfully */	
	int msi_enabled;	/* flag if msi was enabled for the device */	
	int msix_enabled;	/* flag if msi-x was enabled for the device */	
	int irq_user_count;/* user interrupt count */     
	int m_PciDeviceLost;
	struct msix_entry entry[32];	
	/* msi-x vector/entry table */	
	u32 dwDeviceID;
	u32 dwVendorID;
	u32 m_Device_Version;
	int  m_DeviceHW_Version;
	u32 m_AddreeSpace;
	u32 n_VideoModle;
	u32  m_Device_SupportYV12;
	u32 m_Device_SubVersion;
	u32 m_Device_PortID;
	//int  m_IsHDModel; // 0 SD 1. 720p 2 1080p 3 2K 4 4K
	int  m_MaxHWVideoBufferSize;
	int  m_MaxInputWidth;		/* 4096 with 4.5 MiB DMA buffers, else 3840 */
	u32  m_UnsupportedRes;		/* last unsupported input (w<<16|h), warned once */
	int m_nMaxChl;
	int m_nCurreMaxVideoChl;
	int m_nCurreMaxLineInChl;
	uint8_t m_bStartRun;	// Use for start run for check i2c
	//------------------
	dma_addr_t   		m_pbyVideo_phys[MAX_VID_CHANNELS] ;
    uint8_t		*m_pbyVideoBuffer[MAX_VID_CHANNELS];
	uint8_t     *m_pbyVideoBuffer_area[MAX_VID_CHANNELS];
	u32		    m_dwVideoBuffer[MAX_VID_CHANNELS];
	u32		    m_dwVideoHighBuffer[MAX_VID_CHANNELS];
	//uint8_t     *m_pVideoData[MAX_VID_CHANNELS][MAX_VIDEO_QUEUE];
	//uint8_t     *m_pVideoData_area[MAX_VID_CHANNELS][MAX_VIDEO_QUEUE];
	VCAP_STATUS_INFO    m_pVCAPStatus[MAX_VID_CHANNELS][MAX_VIDEO_QUEUE];
	ACAP_VIDEO_INFO m_VideoInfo[MAX_VID_CHANNELS];

	VIDEO_INFO      	m_format[MAX_VID_CHANNELS];
	
	ACAP_AUDIO_INFO     m_AudioInfo[MAX_VID_CHANNELS];
	
	struct task_struct *mMain_tsk; 
	int m_curr_No_Video[MAX_VID_CHANNELS];
	//------------------
	dma_addr_t   		m_pbyAudio_phys[MAX_VID_CHANNELS] ;
	uint8_t     *m_pbyAudioBuffer[MAX_VID_CHANNELS];
	uint8_t		*m_pbyUserAudioBuffer[MAX_VID_CHANNELS];
	uint8_t     *m_pbyAudioBuffer_area[MAX_VID_CHANNELS];
	uint8_t		m_bBufferAllocate;
	u32		m_dwAudioBuffer[MAX_VID_CHANNELS];
	u32		m_dwAudioBufferHigh[MAX_VID_CHANNELS];
	uint8_t m_bVCapStarted[MAX_VID_CHANNELS];
	uint8_t	 m_bACapStarted[MAX_VID_CHANNELS];
	u64       m_FrameEndNs[MAX_VID_CHANNELS];	/* set by irqhandler at the frame-end half */
	u32       m_FrameSeq[MAX_VID_CHANNELS];	/* next frame number, under video->cap_lock */
	u32       m_FramesDropped[MAX_VID_CHANNELS];	/* captured frames lost: a half missing or stale */
	/* input frame rate, measured from frame-end interrupts over ~1 s windows */
	u64       m_RateWinStartNs[MAX_VID_CHANNELS];
	u32       m_RateWinFrames[MAX_VID_CHANNELS];
	u64       m_RateMinDeltaNs[MAX_VID_CHANNELS];	/* shortest frame interval in the window */
	u64       m_FrameIntervalNs[MAX_VID_CHANNELS];	/* last measured, 0 = unknown */
	u64       m_RateUpdatedNs[MAX_VID_CHANNELS];	/* when m_FrameIntervalNs was last written */
	struct kref ref;	/* see hws_dev_release() */
	bool      m_Removed;	/* remove() unmapped BAR0 */
	int       m_NeedReinit;	/* set by hws_request_reinit(), handled by the status thread */
	bool      zc_iommu;	/* the IOMMU translates for the card: zero-copy buffers possible (stage 4.4) */
	uint8_t   m_ResumeVideo[MAX_VID_CHANNELS];	/* video capture state saved across system sleep; ALSA restarts audio */
	uint8_t	  m_dwAudioField[MAX_VID_CHANNELS];
	uint8_t   m_nAudioBusy[MAX_VID_CHANNELS];
	uint8_t   m_nAudioBufferIndex[MAX_VID_CHANNELS];
	uint8_t	  m_pAudioEvent[MAX_VID_CHANNELS];
	uint8_t m_bAudioRun[MAX_VID_CHANNELS];
	uint8_t m_bAudioStop[MAX_VID_CHANNELS];
	uint8_t m_AudioSkip[MAX_VID_CHANNELS];	/* stale packets still to drop */
	unsigned int dbg_stale_bottom;	/* debug_dma test knob: the bottom half, counted from now, to treat as stale */
	int       m_nRDAudioIndex[MAX_VID_CHANNELS];
	u32       m_dwAudioPTKSize;
	//-----------------------------
	
};

#endif
