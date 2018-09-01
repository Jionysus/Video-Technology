/* *********************************************************************************************************
*
* Copyright (c) 2017-2018
* Supervisor:	Dr. Yui-Lam Chan
* Contributor:	Dr. Sik-Ho Tsang
* Department of Electronic and Information Engineering
* The Hong Kong Polytechnic University
* All rights reserved.
*
* *********************************************************************************************************
*/

#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <process.h>
#include <stdio.h>
#include <time.h>

#pragma region
// Declare and define structure and global variables
int				g_iCurFrame;
int				g_iPlayerStatus;
int				g_iWidth;
int				g_iHeight;
unsigned char * g_pYuvBuf;
unsigned char * g_pBmpBuf;
HWND			g_hwndConsole;
HWND			g_hwndWindowYuv;
HANDLE			g_hloc;
LPBITMAPINFO	g_lpBmpInfo;
double			g_dV2R[256];
double			g_dV2G[256];
double			g_dU2G[256];
double			g_dU2B[256];

// player status for Section D
enum PlayerStatus
{
	STATUS_PAUSE = 0,
	STATUS_PLAY = 1,
	MAX_STATUS_NUM = 2
};

// Get console window handle
HWND getConsoleHwnd(void)
{
	#define MY_BUFSIZE 1024
	HWND hwndFound;
	TCHAR pszNewWindowTitle[MY_BUFSIZE];
	TCHAR pszOldWindowTitle[MY_BUFSIZE];
	GetConsoleTitle(pszOldWindowTitle, MY_BUFSIZE);
	wsprintf(pszNewWindowTitle, L"%d/%d", GetTickCount(), GetCurrentProcessId());
	SetConsoleTitle(pszNewWindowTitle);
	Sleep(40);
	hwndFound = FindWindow(NULL, pszNewWindowTitle);
	SetConsoleTitle(pszOldWindowTitle);

	return(hwndFound);
}

// Window process message loop
LRESULT CALLBACK wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_CREATE:
		return 0;
		break;

	case WM_PAINT:			// paint event
	{
		PAINTSTRUCT ps;
		if (g_pBmpBuf)
		{
			HDC hdcYuv = GetDC(g_hwndWindowYuv);
			StretchDIBits(hdcYuv, 0, 0, g_iWidth, g_iHeight, 0, 0, g_iWidth, g_iHeight, g_pBmpBuf, g_lpBmpInfo, DIB_RGB_COLORS, SRCCOPY);
			ReleaseDC(g_hwndWindowYuv, hdcYuv);
		}
		BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);
		return 0;
	}
	break;

	case WM_LBUTTONDOWN:    // left click
		if(g_iPlayerStatus == STATUS_PLAY)
		{
			g_iPlayerStatus = STATUS_PAUSE;
			printf("\n Pause at frame %d.\n", g_iCurFrame);
		}
		return 0;
		break;

	case WM_NCLBUTTONDOWN:  // NONCLIENT area leftbutton down (click on "title bar" part of window)
		break;

	case WM_CHAR:			// character key
		return 0;
		break;

	case WM_MOVE:			// moving the window
		return 0;
		break;

	case WM_SIZE:
		return 0;
		break;

	case WM_DESTROY:		// killing the window
		PostQuitMessage(0);
		return 0;
		break;
	}

	return DefWindowProc(hwnd, message, wparam, lparam);
}

// Init BMP
LPBITMAPINFO initBmp(int iWidth, int iHeight, int nBitCount)
{
	int i;

	HANDLE hloc1;
	RGBQUAD *argbq;

	g_hloc = GlobalAlloc(GMEM_ZEROINIT | GMEM_MOVEABLE,
		sizeof(BITMAPINFOHEADER) + (sizeof(RGBQUAD) * 256));
	g_lpBmpInfo = (LPBITMAPINFO)GlobalLock(g_hloc);

	hloc1 = LocalAlloc(LMEM_ZEROINIT | LMEM_MOVEABLE, (sizeof(RGBQUAD) * 256));
	argbq = (RGBQUAD *)LocalLock(hloc1);

	for (i = 0; i<256; i++)
	{
		argbq[i].rgbBlue = i;
		argbq[i].rgbGreen = i;
		argbq[i].rgbRed = i;
		argbq[i].rgbReserved = 0;
	}

	g_lpBmpInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	g_lpBmpInfo->bmiHeader.biPlanes = 1;

	g_lpBmpInfo->bmiHeader.biBitCount = nBitCount;
	g_lpBmpInfo->bmiHeader.biCompression = BI_RGB;
	g_lpBmpInfo->bmiHeader.biWidth = iWidth;
	g_lpBmpInfo->bmiHeader.biHeight = iHeight;

	memcpy(g_lpBmpInfo->bmiColors, argbq, sizeof(RGBQUAD) * 256);

	LocalUnlock(hloc1);
	LocalFree(hloc1);

	return g_lpBmpInfo;
}

// Free BMP memory
void freeBmp()
{
	GlobalUnlock(g_hloc);
	GlobalFree(g_hloc);
}

// Convert YUV to RGB, and flip for BMP display
int yuv2bmp(unsigned char* pY, unsigned char* pRGB24, int iWidth, int iHeight)
{
	int nPixelSize = iWidth*iHeight;
	const long nYLen = (long)nPixelSize;
	const int nHfWidth = (iWidth >> 1);
	int nYStride = iWidth;
	int nUVStride = nYStride / 2;

	unsigned char * yData = pY;
	unsigned char * uData = pY + iWidth * iHeight;
	unsigned char * vData = pY + iWidth * iHeight + iWidth * iHeight / 4;

	int rgb[3];
	int i, j, x, y, py;

	HDC hdcYuv;

	for (y = 0; y<iHeight; y++)
	{
		yData = &pY[y*nYStride];
		for (x = 0; x<iWidth; x++)
		{
			i = y*iWidth + x;
			j = (y >> 1)*nUVStride + (x >> 1);

			py = yData[x];
			rgb[0] = (int)((double)py + (double)g_dV2R[uData[j]]);
			rgb[1] = (int)((double)py - (double)g_dV2G[vData[j]] + (double)g_dU2G[uData[j]]);
			rgb[2] = (int)((double)py + (double)g_dU2B[vData[j]]);

			i = (nPixelSize - (y + 1)*iWidth + x) * 3;
			for (j = 0; j<3; j++)
			{
				if (rgb[j] >= 0 && rgb[j] <= 255)
					pRGB24[i + j] = rgb[j];
				else
					pRGB24[i + j] = (rgb[j]<0) ? 0 : 255;
			}
		}
	}

	// display
	hdcYuv = GetDC(g_hwndWindowYuv);
	StretchDIBits(GetDC(g_hwndWindowYuv), 0, 0, iWidth, iHeight, 0, 0, iWidth, iHeight, g_pBmpBuf, g_lpBmpInfo, DIB_RGB_COLORS, SRCCOPY);
	ReleaseDC(g_hwndWindowYuv, hdcYuv);

	return 1;
}

// Thread to display Yuv in the window
void threadDisplayYuv(void *param)
{
	HINSTANCE hInstance = NULL;
	HDC hdcWindow = NULL;
	WNDCLASS wc = { 0 };
	MSG msg;

	hInstance = (HINSTANCE)GetWindowLong(g_hwndConsole, GWL_HINSTANCE);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = LoadCursor(hInstance, IDC_ARROW);
	wc.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wc.hInstance = hInstance;
	wc.lpfnWndProc = wndProc;
	wc.lpszClassName = TEXT("Display Image");
	wc.style = CS_HREDRAW | CS_VREDRAW;
	if (!RegisterClass(&wc))
	{
		printf("Problem with your WNDCLASS, foo.\n");
		return;
	}
	g_hwndWindowYuv = CreateWindow(TEXT("Display Image"), TEXT("Display Image"), WS_OVERLAPPEDWINDOW, 600, 20,
		g_iWidth + 16, g_iHeight + 38, g_hwndConsole, NULL, hInstance, NULL);
	ShowWindow(g_hwndWindowYuv, SW_SHOWNORMAL);
	UpdateWindow(g_hwndWindowYuv);
	hdcWindow = GetDC(g_hwndWindowYuv);
	SetStretchBltMode(hdcWindow, HALFTONE);

	while (GetMessage(&msg, g_hwndWindowYuv, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	_endthread();
}

// Init display thread
int initDisplayThread(int iWidth, int iHeight, unsigned char ** pYuvBuf, unsigned char ** pBmpBuf)
{
	int i;

	g_iWidth = iWidth;
	g_iHeight = iHeight;
	g_hwndConsole = getConsoleHwnd();
	*pYuvBuf = (unsigned char *) malloc(iWidth * iHeight * 3 / 2 * sizeof(unsigned char));
	*pBmpBuf = (unsigned char *) malloc(iWidth * iHeight * 3 * sizeof(unsigned char));
	g_pYuvBuf = *pYuvBuf;
	g_pBmpBuf = *pBmpBuf;
	if (pYuvBuf == NULL || pBmpBuf == NULL)
	{
		printf("Cannot allocate memory buffer.\n");
		return -1;
	}
	for (i = 0; i<256; i++)
	{
		g_dV2R[i] = (double) 1.370705 * (i - 128);
		g_dV2G[i] = (double) 0.698001 * (i - 128);
		g_dU2G[i] = (double) 0.703125 * (128 - i);
		g_dU2B[i] = (double) 1.732446 * (i - 128);
	}
	initBmp(iWidth, iHeight, 24);
	g_iCurFrame = 0;
	g_iPlayerStatus = STATUS_PAUSE;
	_beginthread(threadDisplayYuv, 0, 0);
	Sleep(500);

	return 0;
}


// Read image parameter file
int readImageConfigFile(char * szFileName, FILE ** fpInputFile, int * iWidth, int * iHeight)
{
	FILE *fp;
	char szInputFileName[1024];
	// open parameter file
	fp = fopen(szFileName, "rt");
	if (!fp)
	{
		printf("Cannot open parameter file.\n");
		return -1;
	}
	// read parameter file
	fscanf(fp, "%s", szInputFileName);
	fscanf(fp, "%d", iWidth);
	fscanf(fp, "%d", iHeight);
	// close parameter file
	fclose(fp);
	fp = NULL;

	// open input YUV file
	*fpInputFile = fopen(szInputFileName, "rb");
	if (!*fpInputFile)
	{
		printf("Cannot open input YUV file.\n");
		return -1;
	}

	printf("== Configuration File =================================\n");
	printf(" Input File   : %s\n", szInputFileName);
	printf(" Width        : %d\n", *iWidth);
	printf(" Height       : %d\n", *iHeight);
	printf("=======================================================\n");

	return 0;
}

// Read video parameter file
int readVideoConfigFile(char * szFileName, FILE ** fpInputFile, int * iWidth, int * iHeight, int * iFrameRate, int * iMaxFrameNum)
{
	FILE *fp;
	char szInputFileName[1024];
	// open parameter file
	fp = fopen(szFileName, "rt");
	if (!fp)
	{
		printf("Cannot open parameter file.\n");
		return -1;
	}
	// read parameter file
	fscanf(fp, "%s", szInputFileName);
	fscanf(fp, "%d", iWidth);
	fscanf(fp, "%d", iHeight);
	fscanf(fp, "%d", iFrameRate);
	// close parameter file
	fclose(fp);
	fp = NULL;
	// read YUV file to get maximum frame number
	fp = fopen(szInputFileName, "rb");
	if (!fp)
	{
		printf("Cannot open YUV file.\n");
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	*iMaxFrameNum = ftell(fp) / (*iWidth * *iHeight * 3 / 2);
	fclose(fp);
	fp = NULL;

	// open input YUV file
	*fpInputFile = fopen(szInputFileName, "rb");
	if (!*fpInputFile)
	{
		printf("Cannot open input YUV file.\n");
		return -1;
	}

	printf("== Configuration File =================================\n");
	printf("= Input File   : %s\n", szInputFileName);
	printf("= Width        : %d\n", *iWidth);
	printf("= Height       : %d\n", *iHeight);
	printf("= Frame Rate   : %d\n", *iFrameRate);
	printf("= Frame Number : %d\n", *iMaxFrameNum);
	printf("=======================================================\n\n");

	return 0;
}
#pragma endregion

// Display single image
void displayImage(FILE * fp, int iWidth, int iHeight, unsigned char * pYuvBuf, unsigned char * pBmpBuf)
{
	// read YUV
	fread(pYuvBuf, sizeof(unsigned char), iWidth * iHeight * 3 / 2, fp);
	// convert and display
	yuv2bmp(pYuvBuf, pBmpBuf, iWidth, iHeight);
}

// Display single gray-scale image
void displayGrayScaleImage(FILE * fp, int iWidth, int iHeight, unsigned char * pYuvBuf, unsigned char * pBmpBuf)
{
	int i;
	FILE *fpOut;

	// read YUV
	fread(pYuvBuf, sizeof(unsigned char), iWidth * iHeight * 3 / 2, fp);
	// YUV manipulation: set UV to 128
	for (i = iWidth * iHeight; i < iWidth * iHeight * 3 / 2; i++)
		pYuvBuf[i] = 128;
	// convert and display
	yuv2bmp(pYuvBuf, pBmpBuf, iWidth, iHeight);
	// write YUV
	fpOut = fopen("grayscale.raw", "wb");
	fwrite(pYuvBuf, sizeof(unsigned char), iWidth * iHeight * 3 / 2, fpOut);
	fclose(fpOut);
}

// Display single flipped image
void displayFlipImage(FILE * fp, int iWidth, int iHeight, unsigned char * pYuvBuf, unsigned char * pBmpBuf)
{
	int i;
	FILE *fpOut;
	unsigned char * pTempBuf;

	// read YUV
	fread(pYuvBuf, sizeof(unsigned char), iWidth * iHeight * 3 / 2, fp);
	// allocate the temp memory for flipped image
	pTempBuf = (unsigned char * ) malloc(iWidth * iHeight * 3 / 2 * sizeof(unsigned char));
	// YUV manipulation: flip Y
	for (i = 0; i < iWidth * iHeight; i++)
		pTempBuf[i] = pYuvBuf[iWidth * iHeight - i];
	// YUV manipulation: flip U
	for (i = 0; i < iWidth * iHeight / 4; i++)
		pTempBuf[iWidth * iHeight + i] = pYuvBuf[iWidth * iHeight + iWidth * iHeight / 4 - i];
	// YUV manipulation: flip V
	for (i = 0; i < iWidth * iHeight / 4; i++)
		pTempBuf[iWidth * iHeight + iWidth * iHeight / 4 + i] = pYuvBuf[iWidth * iHeight + iWidth * iHeight / 4 + iWidth * iHeight / 4 - i];
	// convert and display
	yuv2bmp(pTempBuf, pBmpBuf, iWidth, iHeight);
	// write YUV
	fpOut = fopen("flip.raw", "wb");
	fwrite(pTempBuf, sizeof(unsigned char), iWidth * iHeight * 3 / 2, fpOut);
	fclose(fpOut);
	// free the temp memory
	if (pTempBuf)
		free(pTempBuf);
}

// Play forward the video, playback
void playVideo(FILE * fp, int iWidth, int iHeight, int iFrameRate, int iMaxFrameNum, unsigned char * pYuvBuf, unsigned char * pBmpBuf)
{
	int i;
	for (i = 0; i<iMaxFrameNum; i++) // loop each frame
	{
		// read YUV
		fread(pYuvBuf, sizeof(unsigned char), iWidth * iHeight * 3 / 2, fp);
		// convert and display
		yuv2bmp(pYuvBuf, pBmpBuf, iWidth, iHeight);
		// sleep 100 ms
		Sleep(100);
	}

	printf("\n Finished playback.\n");
}


// Seek the video, random access
void seekVideo(FILE * fp, int iWidth, int iHeight, int iMaxFrameNum, unsigned char * pYuvBuf, unsigned char * pBmpBuf)
{
	int iSeekFrame;
	// user input
	printf("\n Please enter the [Frame Number] : ");
	scanf(" %d", &iSeekFrame);
	if (iSeekFrame >= iMaxFrameNum)
	{
		printf("\n Seek frame should be smaller than maximum frame number.\n");
		return;
	}
	// seek
	fseek(fp, iSeekFrame * iWidth * iHeight * 3 / 2, SEEK_SET);
	// read YUV
	fread(pYuvBuf, sizeof(unsigned char), iWidth * iHeight * 3 / 2, fp);
	// convert and display
	yuv2bmp(pYuvBuf, pBmpBuf, iWidth, iHeight);
	// update the current frame position
	g_iCurFrame = iSeekFrame;
	
	printf("\n [s] Sought to frame %d.\n", iSeekFrame);
}

// Main function
int main()
{
	char szKey;
	FILE *fpInputFile;
	int iWidth, iHeight, iFrameRate, iMaxFrameNum;
	unsigned char * pYuvBuf, * pBmpBuf;

	// init //////////////////////////////////////////////////////////////////////////////////////////////////
	// read parameter file
	if(readImageConfigFile("image.cfg", &fpInputFile, &iWidth, &iHeight) == -1)
		return -1;
	// init display and thread stuff
	if(initDisplayThread(iWidth, iHeight, &pYuvBuf, &pBmpBuf) == -1)
		return -1;

	// read //////////////////////////////////////////////////////////////////////////////////////////////////
	// display the image
	displayImage(fpInputFile, iWidth, iHeight, pYuvBuf, pBmpBuf);

	// close /////////////////////////////////////////////////////////////////////////////////////////////////
	//printf(" Please press [Enter] to close the window.\n");
	getchar();

	freeBmp();
	if (pYuvBuf)
		free(pYuvBuf);
	if (pBmpBuf)
		free(pBmpBuf);
	if (fpInputFile)
		fclose(fpInputFile);

	return 0;
}
