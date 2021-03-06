/*******************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2014 Intel Corporation. All Rights Reserved.

*******************************************************************************/
#include <windows.h>
#include <iostream>
#include "RealSense/SenseManager.h"
#include "RealSense/SampleReader.h"
#include "util_cmdline.h"
#include "util_render.h"
#include "camera_viewer.h"
#include <conio.h>
#include "yuv2rgb.h"

using namespace Intel::RealSense;

SenseManager *pp;
CaptureManager *cm;
Status sts;
PXCProjection * pjn;
// Create stream renders
//UtilRender renderc(L"Color"), renderd(L"Depth"), renderi(L"IR"), renderr(L"Right"), renderl(L"Left");
bool SaveBMP(BYTE* Buffer, int width, int height, long paddedsize, TCHAR* bmpfile);
int initCameraView() {
		/* Creates an instance of the SenseManager */
		pp = SenseManager::CreateInstance();
		if (!pp) {
			wprintf_s(L"Unable to create the SenseManager\n");
			return 3;
		}

		UtilCmdLine cmdl(pp->QuerySession());
		/* Collects command line arguments */

		/*if (!cmdl.Parse(L"-listio-nframes-sdname-csize-dsize-isize-lsize-rsize-file-record-noRender-mirror", argc, argv)) return 3;*/

		/* Sets file recording or playback */
		cm = pp->QueryCaptureManager();
		cm->SetFileName(cmdl.m_recordedFile, cmdl.m_bRecord);
		if (cmdl.m_sdname) cm->FilterByDeviceInfo(cmdl.m_sdname, 0, 0);
		return 0;
	}
	int exitAndDestroy() {
		wprintf_s(L"Exiting\n");

		// Clean Up
		pp->Release();
		return 0;
	}
	int processFrameInit() {
		
		UtilCmdLine cmdl(pp->QuerySession());
		do {

			
			/* Apply command line arguments */
			bool revert = false;
			if (cmdl.m_csize.size() > 0) {
				pp->EnableStream(Capture::STREAM_TYPE_COLOR, cmdl.m_csize.front().first.width, cmdl.m_csize.front().first.height, (pxcF32)cmdl.m_csize.front().second);
			}
			if (cmdl.m_dsize.size() > 0) {
				pp->EnableStream(Capture::STREAM_TYPE_DEPTH, cmdl.m_dsize.front().first.width, cmdl.m_dsize.front().first.height, (pxcF32)cmdl.m_dsize.front().second);
			}
			if (cmdl.m_isize.size() > 0) {
				pp->EnableStream(Capture::STREAM_TYPE_IR, cmdl.m_isize.front().first.width, cmdl.m_isize.front().first.height, (pxcF32)cmdl.m_isize.front().second);
			}
			if (cmdl.m_rsize.size() > 0) {
				pp->EnableStream(Capture::STREAM_TYPE_RIGHT, cmdl.m_rsize.front().first.width, cmdl.m_rsize.front().first.height, (pxcF32)cmdl.m_rsize.front().second);
			}
			if (cmdl.m_lsize.size() > 0) {
				pp->EnableStream(Capture::STREAM_TYPE_LEFT, cmdl.m_lsize.front().first.width, cmdl.m_lsize.front().first.height, (pxcF32)cmdl.m_lsize.front().second);
			}
			if (cmdl.m_csize.size() == 0 && cmdl.m_dsize.size() == 0 && cmdl.m_isize.size() == 0 && cmdl.m_rsize.size() == 0 && cmdl.m_lsize.size() == 0) {
				DataDesc desc = {};
				if (cm->QueryCapture()) {
					cm->QueryCapture()->QueryDeviceInfo(0, &desc.deviceInfo);
				}
				else {
					desc.deviceInfo.streams = Capture::STREAM_TYPE_COLOR | Capture::STREAM_TYPE_DEPTH;
					SizeI32 colorRes;
					colorRes.width = 640;
					colorRes.height = 480;
					desc.streams.color.sizeMax = colorRes;
					revert = true;
				}
				//pp->EnableStream(Capture::STREAM_TYPE_COLOR, 640, 480);
				pp->EnableStreams(&desc);
			}

			/* Initializes the pipeline */
			sts = pp->Init();
			if (sts < Status::STATUS_NO_ERROR) {
				if (revert) {
					/* Enable a single stream */
					pp->Close();
					pp->EnableStream(Capture::STREAM_TYPE_DEPTH);
					sts = pp->Init();
					if (sts < Status::STATUS_NO_ERROR) {
						pp->Close();
						pp->EnableStream(Capture::STREAM_TYPE_COLOR);
						sts = pp->Init();
					}
				}
				if (sts < Status::STATUS_NO_ERROR) {
					wprintf_s(L"Failed to locate any video stream(s)\n");
					pp->Release();
					return sts;
				}
			}

			/* Reset all properties */
			Capture::Device *device = pp->QueryCaptureManager()->QueryDevice();
			device->ResetProperties(Capture::STREAM_TYPE_ANY);

			/* Set mirror mode */
			if (cmdl.m_bMirror) {
				device->SetMirrorMode(Capture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL);
			}
			else {
				device->SetMirrorMode(Capture::Device::MirrorMode::MIRROR_MODE_DISABLED);
			}
			pjn = device->CreateProjection();

		}while (sts == Status::STATUS_STREAM_CONFIG_CHANGED);//if config change
		//exitAndDestroy();
		return 0;
	}


int acquireOneFrame(void* textureDataPtrColor, void* textureDataPtrDepth) {
	UtilCmdLine cmdl(pp->QuerySession());

	{
		/* Waits until new frame is available and locks it for application processing */
		sts = pp->AcquireFrame(false);

		if (sts < Status::STATUS_NO_ERROR) {
			if (sts == Status::STATUS_STREAM_CONFIG_CHANGED) {
				wprintf_s(L"Stream configuration was changed, re-initilizing\n");
				pp->Close();
			}
			return 0;
		}


		/* Render streams, unless -noRender is selected */
		if (cmdl.m_bNoRender == false) {
			const Capture::Sample *sample = pp->QuerySample();
			if (sample) {
				//if (sample->color && sample->depth) {
					// image is a PXCImage instance
					PXCImage::ImageData data_color;
					PXCImage::ImageData data_depth;
					PXCImage::ImageData data_mapped;

					PXCImage* mapped = pjn->CreateDepthImageMappedToColor(sample->depth, sample->color);
					sample->color->AcquireAccess(PXCImage::ACCESS_READ, &data_color);
					sample->depth->AcquireAccess(PXCImage::ACCESS_READ, &data_depth);
					mapped->AcquireAccess(PXCImage::ACCESS_READ, &data_mapped); //Depth 16bit
																				// image planes are in data.planes[0-3] with pitch data.pitches[0-3]

					int w_color = sample->color->QueryInfo().width;
					int h_color = sample->color->QueryInfo().height;
					int w_depth = sample->depth->QueryInfo().width;
					int h_depth = sample->depth->QueryInfo().height;

					unsigned char* baseAddr_color = data_color.planes[0];
					unsigned char* baseAddr_depth = data_depth.planes[0];
					unsigned char* baseAddr_mapped = data_mapped.planes[0];

					int currpointercounterIn = 0;
					int currpointercounterOut = 0;
					unsigned char* dst_color = (unsigned char*)textureDataPtrColor;
					unsigned char* dst_depth = (unsigned char*)textureDataPtrDepth;

					for (int i = 0; i < h_color; i++) {
						for (int j = 0; j < w_color / 2; j++) {
							if (i < 480 && 2 * j < 640) {
								int y0 = baseAddr_color[currpointercounterIn];
								int u0 = baseAddr_color[currpointercounterIn + 1];
								int y1 = baseAddr_color[currpointercounterIn + 2];
								int v0 = baseAddr_color[currpointercounterIn + 3];
								currpointercounterIn += 4;
								int c = ((y0 - 16)> 0) ? (y0 - 16) : 0;
								int d = u0 - 128;
								int e = v0 - 128;

								int r = (298 * c + 409 * e + 128) >> 8;
								int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
								int b = (298 * c + 516 * d + 128) >> 8;
								r = (r > 255) ? 255 : ((r > 0) ? r : 0);
								g = (g > 255) ? 255 : ((g > 0) ? g : 0);
								b = (b > 255) ? 255 : ((b > 0) ? b : 0);
								dst_color[currpointercounterOut] = r; // red
								dst_color[currpointercounterOut + 1] = g; // green
								dst_color[currpointercounterOut + 2] = b; // blue
								dst_color[currpointercounterOut + 3] = 255;

								c = ((y1 - 16) > 0) ? (y1 - 16) : 0;
								r = (298 * c + 409 * e + 128) >> 8;
								g = (298 * c - 100 * d - 208 * e + 128) >> 8;
								b = (298 * c + 516 * d + 128) >> 8;
								r = (r > 255) ? 255 : ((r > 0) ? r : 0);
								g = (g > 255) ? 255 : ((g > 0) ? g : 0);
								b = (b > 255) ? 255 : ((b > 0) ? b : 0);
								dst_color[currpointercounterOut + 4] = r; // red
								dst_color[currpointercounterOut + 5] = g; // green
								dst_color[currpointercounterOut + 6] = b; // blue
								dst_color[currpointercounterOut + 7] = 255;

								currpointercounterOut += 8;
							}
							else {
								currpointercounterIn += 4;
							}
						}
					}
					currpointercounterIn = 0;
					currpointercounterOut = 0;
					for (int i = 0; i < h_color; i++) {
						for (int j = 0; j < w_color; j++) {
							if (i < 480 && j < 640) {
								unsigned char upper = baseAddr_mapped[currpointercounterIn];
								unsigned char lower = baseAddr_mapped[currpointercounterIn + 1];
								currpointercounterIn += 2;
								dst_depth[currpointercounterOut] = (unsigned char)upper;
								dst_depth[currpointercounterOut + 1] = (unsigned char)lower;
								
								//dst_depth[currpointercounterOut] = (unsigned char)(j/2)%255;
								//dst_depth[currpointercounterOut + 1] = (unsigned char)i%255;
								currpointercounterOut += 2;
							}
							else {
								currpointercounterIn += 2;
							}

						}
					}
					sample->depth->ReleaseAccess(&data_depth);
					sample->color->ReleaseAccess(&data_color);
					mapped->ReleaseAccess(&data_mapped);
					mapped->Release();
				//}//if color and depth valid
				
			}//if sample valid
		}


		/* Releases lock so pipeline can process next frame */
		pp->ReleaseFrame();
	}
	return 0;
}

int acquireOneFrame_capture(void* textureDataPtrColor, void* textureDataPtrDepth) {
	UtilCmdLine cmdl(pp->QuerySession());

	{
		/* Waits until new frame is available and locks it for application processing */
		sts = pp->AcquireFrame(false);

		if (sts < Status::STATUS_NO_ERROR) {
			if (sts == Status::STATUS_STREAM_CONFIG_CHANGED) {
				wprintf_s(L"Stream configuration was changed, re-initilizing\n");
				pp->Close();
			}
			return 0;
		}


		if (cmdl.m_bNoRender == false) {
			const Capture::Sample *sample = pp->QuerySample();
			if (sample) {
				PXCImage* mapped = pjn->CreateDepthImageMappedToColor(sample->depth, sample->color);

				PXCImage::ImageData data_mapped;
				PXCImage::ImageData data_color;
				PXCImage::ImageData data_depth;

				sample->color->AcquireAccess(PXCImage::ACCESS_READ, &data_color);
				sample->depth->AcquireAccess(PXCImage::ACCESS_READ, &data_depth);
				mapped->AcquireAccess(PXCImage::ACCESS_READ, &data_mapped);


				int w_color = sample->color->QueryInfo().width;
				int h_color = sample->color->QueryInfo().height;
				int w_depth = sample->depth->QueryInfo().width;
				int h_depth = sample->depth->QueryInfo().height;

				unsigned char* baseAddr_color = data_color.planes[0];
				unsigned char* baseAddr_depth = data_depth.planes[0];
				unsigned char* baseAddr_mapped = data_mapped.planes[0];

				int currpointercounterIn = 0;
				int currpointercounterOut = 0;
				unsigned char* dst_color = (unsigned char*)textureDataPtrColor;
				unsigned char* dst_depth = (unsigned char*)textureDataPtrDepth;

				for (int i = 0; i < h_color; i++) {
					for (int j = 0; j < w_color / 2; j++) {
						if (i < 480 && 2 * j < 640) {
							int y0 = baseAddr_color[currpointercounterIn];
							int u0 = baseAddr_color[currpointercounterIn + 1];
							int y1 = baseAddr_color[currpointercounterIn + 2];
							int v0 = baseAddr_color[currpointercounterIn + 3];
							currpointercounterIn += 4;
							int c = ((y0 - 16)> 0) ? (y0 - 16) : 0;
							int d = u0 - 128;
							int e = v0 - 128;

							int r = (298 * c + 409 * e + 128) >> 8;
							int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
							int b = (298 * c + 516 * d + 128) >> 8;
							r = (r > 255) ? 255 : ((r > 0) ? r : 0);
							g = (g > 255) ? 255 : ((g > 0) ? g : 0);
							b = (b > 255) ? 255 : ((b > 0) ? b : 0);
							dst_color[currpointercounterOut] = r; // red
							dst_color[currpointercounterOut + 1] = g; // green
							dst_color[currpointercounterOut + 2] = b; // blue
							dst_color[currpointercounterOut + 3] = 255;

							c = ((y1 - 16) > 0) ? (y1 - 16) : 0;
							r = (298 * c + 409 * e + 128) >> 8;
							g = (298 * c - 100 * d - 208 * e + 128) >> 8;
							b = (298 * c + 516 * d + 128) >> 8;
							r = (r > 255) ? 255 : ((r > 0) ? r : 0);
							g = (g > 255) ? 255 : ((g > 0) ? g : 0);
							b = (b > 255) ? 255 : ((b > 0) ? b : 0);
							dst_color[currpointercounterOut + 4] = r; // red
							dst_color[currpointercounterOut + 5] = g; // green
							dst_color[currpointercounterOut + 6] = b; // blue
							dst_color[currpointercounterOut + 7] = 255;

							currpointercounterOut += 8;
						}
						else {
							currpointercounterIn += 4;
						}
					}
				}
				currpointercounterIn = 0;
				currpointercounterOut = 0;
				for (int i = 0; i < h_color; i++) {
					for (int j = 0; j < w_color; j++) {
						if (i < 480 && j < 640) {
							unsigned char upper = baseAddr_mapped[currpointercounterIn];
							unsigned char lower = baseAddr_mapped[currpointercounterIn + 1];
							currpointercounterIn += 2;
							dst_depth[currpointercounterOut] = (unsigned char)upper;
							dst_depth[currpointercounterOut + 1] = (unsigned char)lower;

							//dst_depth[currpointercounterOut] = (unsigned char)(j/2)%255;
							//dst_depth[currpointercounterOut + 1] = (unsigned char)i%255;
							currpointercounterOut += 2;
						}
						else {
							currpointercounterIn += 2;
						}

					}
				}

				currpointercounterIn = 0;
				currpointercounterOut = (w_color * h_color * 3) - 1;
				BYTE* dst_color_capture = new BYTE[h_color * w_color * 3];

					for (int i = 0; i < h_color; i++) {
						for (int j = 0; j < w_color / 2; j++) {
							if (i < 480 && 2 * j < 640) {
								int y0 = baseAddr_color[currpointercounterIn];
								int u0 = baseAddr_color[currpointercounterIn + 1];
								int y1 = baseAddr_color[currpointercounterIn + 2];
								int v0 = baseAddr_color[currpointercounterIn + 3];
								currpointercounterIn += 4;
								int c = ((y0 - 16)> 0) ? (y0 - 16) : 0;
								int d = u0 - 128;
								int e = v0 - 128;

								int r = (298 * c + 409 * e + 128) >> 8;
								int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
								int b = (298 * c + 516 * d + 128) >> 8;
								r = (r > 255) ? 255 : ((r > 0) ? r : 0);
								g = (g > 255) ? 255 : ((g > 0) ? g : 0);
								b = (b > 255) ? 255 : ((b > 0) ? b : 0);
								dst_color_capture[currpointercounterOut] = r; // red
								dst_color_capture[currpointercounterOut - 1] = g; // green
								dst_color_capture[currpointercounterOut - 2] = b; // blue

								c = ((y1 - 16) > 0) ? (y1 - 16) : 0;
								r = (298 * c + 409 * e + 128) >> 8;
								g = (298 * c - 100 * d - 208 * e + 128) >> 8;
								b = (298 * c + 516 * d + 128) >> 8;
								r = (r > 255) ? 255 : ((r > 0) ? r : 0);
								g = (g > 255) ? 255 : ((g > 0) ? g : 0);
								b = (b > 255) ? 255 : ((b > 0) ? b : 0);
								dst_color_capture[currpointercounterOut - 3] = r; // red
								dst_color_capture[currpointercounterOut - 4] = g; // green
								dst_color_capture[currpointercounterOut - 5] = b; // blue

								currpointercounterOut -= 6;
							}
							else {
								currpointercounterIn += 4;
							}
						}
					}
					char filename[1024];
					sprintf_s(filename, sizeof(filename), "RSCapture_%d.bmp", 1);

					std::string fn = std::string(filename);
					TCHAR *fn_tchar = new TCHAR[fn.size() + 1];
					fn_tchar[fn.size()] = 0;
					std::copy(fn.begin(), fn.end(), fn_tchar);

				SaveBMP(dst_color_capture, w_color, h_color, w_color*h_color * 3, fn_tchar);

				delete dst_color_capture;
				mapped->ReleaseAccess(&data_mapped);
				sample->color->ReleaseAccess(&data_color);
				sample->depth->ReleaseAccess(&data_depth);

				mapped->Release();

			}

		}

		/* Releases lock so pipeline can process next frame */
		pp->ReleaseFrame();
	}
	return 0;
}
//int wmain(int argc, WCHAR* argv[]) {
//	/* Creates an instance of the SenseManager */
//	pp = SenseManager::CreateInstance();
//	if (!pp) {
//		wprintf_s(L"Unable to create the SenseManager\n");
//		return 3;
//	}
//
//	UtilCmdLine cmdl(pp->QuerySession());
//	/* Collects command line arguments */
//	
//	if (!cmdl.Parse(L"-listio-nframes-sdname-csize-dsize-isize-lsize-rsize-file-record-noRender-mirror", argc, argv)) return 3;
//
//	/* Sets file recording or playback */
//	cm = pp->QueryCaptureManager();
//	cm->SetFileName(cmdl.m_recordedFile, cmdl.m_bRecord);
//	if (cmdl.m_sdname) cm->FilterByDeviceInfo(cmdl.m_sdname, 0, 0);
//
//	/*processFrame();*/
//	
//	wprintf_s(L"Exiting\n");
//
//	// Clean Up
//	pp->Release();
//	return 0;
//}

bool SaveBMP(BYTE* Buffer, int width, int height, long paddedsize, TCHAR* bmpfile)
//bool SaveBMP(BYTE* Buffer, int width, int height, long paddedsize, LPCTSTR bmpfile)
{
	// declare bmp structures 
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER info;

	// andinitialize them to zero
	memset(&bmfh, 0, sizeof(BITMAPFILEHEADER));
	memset(&info, 0, sizeof(BITMAPINFOHEADER));

	// fill the fileheader with data
	bmfh.bfType = 0x4d42;       // 0x4d42 = 'BM'
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + paddedsize;
	bmfh.bfOffBits = 0x36;      // number of bytes to start of bitmap bits

								// fill the infoheader

	info.biSize = sizeof(BITMAPINFOHEADER);
	info.biWidth = width;
	info.biHeight = height;
	info.biPlanes = 1;          // we only have one bitplane
	info.biBitCount = 24;       // RGB mode is 24 bits
	info.biCompression = BI_RGB;
	info.biSizeImage = 0;       // can be 0 for 24 bit images
	info.biXPelsPerMeter = 0x0ec4;     // paint and PSP use this values
	info.biYPelsPerMeter = 0x0ec4;
	info.biClrUsed = 0;         // we are in RGB mode and have no palette
	info.biClrImportant = 0;    // all colors are important

								// now we open the file to write to
	HANDLE file = CreateFile(bmpfile, GENERIC_WRITE, FILE_SHARE_READ,
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == NULL)
	{
		CloseHandle(file);
		return false;
	}

	// write file header
	unsigned long bwritten;
	if (WriteFile(file, &bmfh, sizeof(BITMAPFILEHEADER), &bwritten, NULL) == false)
	{
		CloseHandle(file);
		return false;
	}
	// write infoheader
	if (WriteFile(file, &info, sizeof(BITMAPINFOHEADER), &bwritten, NULL) == false)
	{
		CloseHandle(file);
		return false;
	}
	// write image data
	if (WriteFile(file, Buffer, paddedsize, &bwritten, NULL) == false)
	{
		CloseHandle(file);
		return false;
	}

	// and close file
	CloseHandle(file);

	return true;
}

