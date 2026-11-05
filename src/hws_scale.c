/*
 * The CPU scalers: produce an out_w x out_h YUYV frame from an in_w x in_h
 * one. Kept apart from hws_video.c so tests/unit can build them in
 * userspace (stage 7.5).
 */
#include "hws_scale.h"

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
void VideoScaler(BYTE *pSrc,BYTE *pOut,int in_w,int in_h,int out_w,int out_h)
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
	/*
	 * No 4096x2160 -> 2560x1440 case: UHDW_To_2560X1440_Scaler() reads 16
	 * source bytes per 6 pixels, runs into the next row and past the frame
	 * on the last one (tests/unit, 7.5); the general scaler below handles it.
	 */
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
