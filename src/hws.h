/*

*/

#ifndef _HWS_PCIE_H_
#define _HWS_PCIE_H_

#include <linux/module.h>
#include <linux/delay.h>

#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-sg.h>
#include <linux/uio_driver.h>

#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/opl3.h>

#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uio_driver.h>
#include <linux/spinlock.h>
#include <linux/uio_driver.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/aio.h>
#include <linux/splice.h>
#include <linux/version.h>
#include <linux/uio.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/semaphore.h>   
#include <linux/sched.h>    
#include <linux/wait.h>   


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

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif 
#define MEM_LOCK					1
#define MEM_UNLOCK					0


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

#define MAX_VIDEO_HW_W 4096
#define MAX_VIDEO_HW_H 2160

struct TMemParam
{
	DWORD   dwIndex;
	DWORD   dwType;
	DWORD   dwStatus;
};
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
	uint8_t		*m_pVideoBufData[MAX_VIDEO_QUEUE];
	uint8_t     *m_pVideoData_area[MAX_VIDEO_QUEUE];
	uint8_t		*m_pVideoBufData1[MAX_VIDEO_QUEUE];
	uint8_t     *m_pVideoData_area1[MAX_VIDEO_QUEUE];
	uint8_t		*m_pVideoBufData2[MAX_VIDEO_QUEUE];
	uint8_t     *m_pVideoData_area2[MAX_VIDEO_QUEUE];
	uint8_t		*m_pVideoBufData3[MAX_VIDEO_QUEUE];
	uint8_t     *m_pVideoData_area3[MAX_VIDEO_QUEUE];
	int         m_VideoBufferSize[4];
	VCAP_STATUS_INFO pStatusInfo[MAX_VIDEO_QUEUE];
	DWORD        m_nVideoIndex;
	BYTE		*m_pVideoScalerBuf;
	DWORD		dwisRuning;
}ACAP_VIDEO_INFO, *PACAP_VIDEO_INFO;

typedef struct 
{
	uint8_t		*m_pAudioBufData[MAX_AUDIO_QUEUE];
	uint8_t     *m_pAudioData_area[MAX_AUDIO_QUEUE];
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


#define DVRS_HW_IOC_MAGIC	'd'
enum DVRS_HW_TYPES {
	IOCTL_DVRS_MAP_DATA_BUFFER,
	IOCTL_DVRS_MAP_START_DMA,
	IOCTL_DVRS_MAP_STOP_DMA,
	IOCTL_DVRS_MAP_GET_CAPTURE,
	IOCTL_DVRS_MAP_SET_FORMATE
};
#define DVRS_HW_IOCDATABUFFER		_IOWR(DVRS_HW_IOC_MAGIC, IOCTL_DVRS_MAP_DATA_BUFFER, struct TMemParam)
#define DVRS_HW_IOCSTART_DMA	_IOWR(DVRS_HW_IOC_MAGIC, IOCTL_DVRS_MAP_START_DMA,		 struct TMemParam)
#define DVRS_HW_IOCSTOP_DMA	   _IOWR(DVRS_HW_IOC_MAGIC, IOCTL_DVRS_MAP_STOP_DMA,		 struct TMemParam)
#define DVRS_HW_IOCGET_CAPUTURE  _IOWR(DVRS_HW_IOC_MAGIC, IOCTL_DVRS_MAP_GET_CAPTURE,	 struct TMemParam)
#define DVRS_HW_IOCSET_FORMATE  _IOWR(DVRS_HW_IOC_MAGIC, IOCTL_DVRS_MAP_SET_FORMATE,	 struct TMemParam)

//--------------------

#define HWS_PCIE_WRITE(__addr, __offst, __data)	writel((__data), (dev->mmio + (__addr + __offst)))
#define HWS_PCIE_READ(__addr, __offst)		readl((dev->mmio + (__addr + __offst)))


#define	UNSET	(-1U)

struct hws_pcie_dev;
struct hws_adapter;


/* buffer for one video frame */
struct hwsvideo_buffer {
	/* common v4l buffer stuff -- must be first */
	struct vb2_v4l2_buffer	vb;
	struct list_head		queue;
	void *					mem;

};

struct hws_dmabuf{
	unsigned int			size;
	__le32					*cpu;
	dma_addr_t				dma;	
};

struct hws_video{
	struct hws_pcie_dev		*dev;
	struct v4l2_device		v4l2_dev;
	struct video_device		vdev;
	struct vb2_queue		vq;
	struct list_head		queue;
	int                     fileindex;
	int                     startstreamIndex;
	unsigned				seqnr;
	struct mutex			video_lock;
	struct mutex			queue_lock;
	spinlock_t				slock;
	v4l2_std_id				std;  //V4L2_STD_NTSC_M
	u32						pixfmt; //V4L2_PIX_FMT_YUYV(fourcc)
	int                     queryIndex;
	int						index;
	struct work_struct		videowork;
	int						Interlaced;
	//------------------------
	int m_Curr_Brightness;
	int m_Curr_Contrast;   
	int m_Curr_Saturation;
	int m_Curr_Hue;       
	//------------------------
	int current_out_width;
	int curren_out_height;
	int current_out_framerate;
	int current_out_pixfmt;
	int current_out_size_index;
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
	struct workqueue_struct *wq;
	struct workqueue_struct *auwq;
	unsigned long video_data[MAX_VID_CHANNELS];
 	struct tasklet_struct dpc_video_tasklet[MAX_VID_CHANNELS];
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
	uint8_t				m_bChangeVideoSize[MAX_VID_CHANNELS];
	
	struct task_struct *mMain_tsk; 
	int m_curr_No_Video[MAX_VID_CHANNELS];
	//------------------
	dma_addr_t   		m_pbyAudio_phys[MAX_VID_CHANNELS] ;
	uint8_t     *m_pbyAudioBuffer[MAX_VID_CHANNELS];
	uint8_t		*m_pbyUserAudioBuffer[MAX_VID_CHANNELS];
	uint8_t     *m_pAudioData[MAX_VID_CHANNELS];
	uint8_t     *m_pbyAudioBuffer_area[MAX_VID_CHANNELS];
	uint8_t     *m_pAudioData_area[MAX_VID_CHANNELS];
	uint8_t		m_bBufferAllocate;
	u32		m_dwAudioBuffer[MAX_VID_CHANNELS];
	u32		m_dwAudioBufferHigh[MAX_VID_CHANNELS];
	uint8_t m_bVCapStarted[MAX_VID_CHANNELS];
	uint8_t	 m_bACapStarted[MAX_VID_CHANNELS];
	uint8_t     m_nVideoBusy[MAX_VID_CHANNELS];
	uint8_t   m_bVideoStop[MAX_VID_CHANNELS];
	//int       m_nVideoIndex[MAX_VID_CHANNELS];
	int       m_nRDVideoIndex[MAX_VID_CHANNELS];
	int        m_nVideoBufferIndex[MAX_VID_CHANNELS];
	int       m_nVideoHalfDone[MAX_VID_CHANNELS];
	uint8_t	  m_dwAudioField[MAX_VID_CHANNELS];
	uint8_t   m_nAudioBusy[MAX_VID_CHANNELS];
	uint8_t   m_nAudioBufferIndex[MAX_VID_CHANNELS];
	uint8_t	  m_pAudioEvent[MAX_VID_CHANNELS];
	uint8_t		m_pVideoEvent[MAX_VID_CHANNELS];
	uint8_t		m_bVCapIntDone[MAX_VID_CHANNELS];
	uint8_t m_bAudioRun[MAX_VID_CHANNELS];
	uint8_t m_bAudioStop[MAX_VID_CHANNELS];
	int       m_nRDAudioIndex[MAX_VID_CHANNELS];
	u32       m_dwAudioPTKSize;
	//-----------------------------
	
};

#endif
