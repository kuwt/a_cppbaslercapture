/* Copyright (C) ASTRI - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by kuwingto <ronaldku@astri.org>, Jan 2019
*/

#include <pylon/PylonIncludes.h>
#include <opencv2/opencv.hpp>
#include <mutex>
#include <thread>
#include <chrono>
#include <condition_variable>

#include "baslerCapture.h"

const char cameraModelName[] = "daA1280-54um";

using namespace Pylon;
using namespace GenApi;

static bool	g_bPylonAutoInitTerm = false;
static std::mutex g_mu_Grab;
static std::mutex g_mu_state;

class ImageCache
{
public:
	~ImageCache() {}

	void setNumOfImage(int num)
	{
		std::lock_guard<std::mutex> lk(m_mu_imageCacheNumOfImage);
		m_NumImages = num;
	}
	int getNumOfImage()
	{
		std::lock_guard<std::mutex> lk(m_mu_imageCacheNumOfImage);
		return m_NumImages;
	}

	void recvMat(cv::Mat img)
	{
		m_vMat.push_back(img.clone());
		// Increment image counter
		m_currentImageCnt++;

		if (m_currentImageCnt >= getNumOfImage())
		{
			// emit signal;
			std::unique_lock<std::mutex> lk(m_mu_imageCache);
			m_is_condition_ready = true;
			m_con_v_imageCache.notify_one();
		}
	}
	
	int getImages(std::vector<cv::Mat> &mats)
	{
		int status = 0;
		std::unique_lock<std::mutex> lk(m_mu_imageCache);

		bool bStatus = m_con_v_imageCache.wait_for(lk, std::chrono::seconds(3), m_is_condition_ready );
		if (bStatus == false)
		{
			std::cerr << "get Images timeout!\n";
			status = -1;
		}

		for (int i = 0; i < m_vMat.size(); ++i)
		{
			mats.push_back(m_vMat.at(i).clone());
		}
		m_currentImageCnt = 0;
		m_vMat.clear();

		return status;
	}

private:

	bool m_is_condition_ready = false;
	std::mutex m_mu_imageCache;
	std::condition_variable m_con_v_imageCache;

	std::mutex m_mu_grab;
	std::mutex m_mu_imageCacheNumOfImage;

	unsigned int m_NumImages= 7;
	unsigned int m_currentImageCnt = 0;
	std::vector<cv::Mat> m_vMat;
};


class ImageEventHandler : public  Pylon::CImageEventHandler
{
public:
	ImageEventHandler(bool bIsColor)
	{
		m_bIsColor = bIsColor;
		if (m_bIsColor)
		{
			m_ImageConverter.OutputPixelFormat = Pylon::PixelType_RGB8packed;
		}
		else
		{
			m_ImageConverter.OutputPixelFormat = Pylon::PixelType_Mono8;
		}
	}

	~ImageEventHandler() {}

	int setCache(ImageCache *pCache)
	{
		m_pCache = pCache;
		return 0;
	}

	void OnImageGrabbed(Pylon::CInstantCamera& camera, const Pylon::CGrabResultPtr& ptrGrabResult)
	{
		//std::cout << "Image Grabbed event..." << "\n";

		if (ptrGrabResult->GrabSucceeded())
		{
			//std::cout << "OnImageGrabbed Succeed" << "\n";
			uint8_t *pImageBuffer = (uint8_t *)ptrGrabResult->GetBuffer();
			if (pImageBuffer != NULL)
			{
				//std::cout << "pImageBuffer ok" << "\n";
				uint32_t width = ptrGrabResult->GetWidth();
				uint32_t height = ptrGrabResult->GetHeight();
				//std::cout << "Grabbed image " << ", width = " << width << ", height = " << height << "\n";
				//std::cout << "getting image from camera buffer to ram..." << "\n";
			
				if (m_bIsColor)
				{
					//std::cout << "form image" << "\n";
					Pylon::CPylonImage pylonImage;
					m_ImageConverter.Convert(pylonImage, ptrGrabResult);
					cv::Mat imageRGB = cv::Mat(height, width, CV_8UC3, (uint8_t*)pylonImage.GetBuffer());
					cv::Mat outMat;
					if (!imageRGB.empty() && imageRGB.channels() == 3)
					{
						cv::Mat imageBGR;
						//std::cout << "convert color" << "\n";
						try
						{
							cv::cvtColor(imageRGB, imageBGR, cv::COLOR_RGB2BGR);
							outMat = imageBGR;
							//outMat = imageRGB.clone();
						}
						catch (...)
						{
							std::cerr << "catch at cv::cvtColor(imageRGB, imageBGR, cv::COLOR_RGB2BGR)" << "\n";
							outMat = cv::Mat();
						}
					}
					if (m_pCache)
					{
						m_pCache->recvMat(outMat);
					}
				}
				else
				{
					Pylon::CPylonImage pylonImage;
					m_ImageConverter.Convert(pylonImage, ptrGrabResult);
					cv::Mat imageBW = cv::Mat(height, width, CV_8UC1, (uint8_t*)pylonImage.GetBuffer());
					cv::Mat outMat = imageBW;
					if (m_pCache)
					{
						m_pCache->recvMat(outMat);
					}
				}
				
			}
			else
			{
				std::cout << "ERROR pImageBuffer NULL" << "\n";
			}
		}
		else
		{
			std::cout << "OnImageGrabbed Fail" << "\n";
			uint32_t nError = ptrGrabResult->GetErrorCode();
			Pylon::String_t strError = ptrGrabResult->GetErrorDescription();
			std::cout << "ERROR OnImageGrabbed" << nError << "    " << strError << "\n";
		}
	}
private:
	bool m_bIsColor = false;
	ImageCache* m_pCache = NULL;
	Pylon::CImageFormatConverter m_ImageConverter;
};


class baslerCapture : public baslerCaptureItf
{

public:
	baslerCapture();
	~baslerCapture();

	int configurateExposure(float exposureTime); // microsec

	int Start();
	int Stop();

	int readyHWTrig(int numOfImagesPerTrig);
	int getHWTrigImgs(std::vector<cv::Mat> &imgs);
	int ExecuteSWTrig(cv::Mat& img);

	int getCurrentState();

private:
	
	int initBaslerCameras();
	int terminateBaslerCameras();
	bool IsUseDevPresent();

	int OpenDevice();
	int CloseDevice();

	int setCurrentState(int state);
private:
	// Camera Devices
	int m_nTotalDeviceNum = 0;
	Pylon::DeviceInfoList_t m_listDeviceInfo;
	std::vector<std::string> m_vCamSN;
	
	// local 
	int m_UseDevIdx = 0;
	Pylon::CInstantCamera m_InstantCamera;
	ImageEventHandler* m_pimageEventHandler = NULL;
	ImageCache m_Cache;
	bool m_IsHWtriggerRunning = false;
	int m_currentState = STOP_STATE;

};

baslerCapture::baslerCapture()
{
	try
	{
		initBaslerCameras();
	}
	catch (GenICam::GenericException &e)
	{
		char buff[1024];
		snprintf(buff, sizeof(buff), "Catch Exception InitAll() error: %s", e.GetDescription());
		std::string str;
		str = buff;
		std::cout << str << "\n";
	}

	try
	{
		OpenDevice();
	}
	catch (GenICam::GenericException &e)
	{
		char buff[1024];
		snprintf(buff, sizeof(buff), "Catch Exception OpenDevice: %s", e.GetDescription());
		std::string str;
		str = buff;
		std::cout << str << "\n";
	}
}


baslerCapture::~baslerCapture()
{
	CloseDevice();
	terminateBaslerCameras();
}

int baslerCapture::initBaslerCameras()
{
	std::cout << "initBaslerCameras" << "\n";
	if (g_bPylonAutoInitTerm == false)
	{
		Pylon::PylonInitialize();
		g_bPylonAutoInitTerm = true;
	}

	Pylon::CTlFactory& tlFactory = CTlFactory::GetInstance();

	//Get Device total view
	DeviceInfoList_t listDeviceInfo;
	int nTotalCamNum = tlFactory.EnumerateDevices(listDeviceInfo);
	std::cout << "nTotalCamNum = " << nTotalCamNum << "\n";
	m_nTotalDeviceNum = nTotalCamNum;
	m_listDeviceInfo = listDeviceInfo;

	//Get SN from device list
	for (int i = 0; i < nTotalCamNum; i++)
	{
		DeviceInfoList_t::iterator it = listDeviceInfo.begin() + i;
		std::string strDeviceSN = (*it).GetSerialNumber().c_str();
		m_vCamSN.push_back(strDeviceSN);
		std::cout << "strDeviceSN " << std::to_string(i) << " = " << strDeviceSN << "\n";
	}

	return 0;
}

int baslerCapture::terminateBaslerCameras()
{
	if (g_bPylonAutoInitTerm == true)
	{
		Pylon::PylonTerminate();
		g_bPylonAutoInitTerm = false;
	}

	return 0;
}

bool baslerCapture::IsUseDevPresent()
{
	if (m_UseDevIdx >= 0 && m_UseDevIdx < m_nTotalDeviceNum)
	{
		return true;
	}
	else
	{
		return false;
	}
}

int baslerCapture::OpenDevice()
{
	if (IsUseDevPresent())
	{
		//  prepare m_InstantCamera
		if (m_InstantCamera.IsPylonDeviceAttached())
		{
			m_InstantCamera.DetachDevice();
			m_InstantCamera.DestroyDevice();
		}

		// locate camera index
		int camIdx = -1;
		for (int i = 0; i < m_listDeviceInfo.size(); ++i)
		{
			if (m_listDeviceInfo[i].GetModelName() == cameraModelName)
			{
				camIdx = i;
			}
		}
		if (camIdx == -1)
		{
			std::cout << "setup camera fail. Camera Installed Model Incorrect." << "\n";
			std::cout << " Close the program and restart.\n";
			return -1;
		}

		m_UseDevIdx = camIdx;
		m_InstantCamera.Attach(CTlFactory::GetInstance().CreateDevice(m_listDeviceInfo[m_UseDevIdx]));
		m_InstantCamera.Open();

		std::string camModelName = CStringPtr(m_InstantCamera.GetNodeMap().GetNode("DeviceModelName"))->GetValue();
		std::cout << "camModelName = " << camModelName << "\n";

		// default trigger mode since it does not waste resources
		// Set trigger and expose
		CEnumerationPtr triggerMode(m_InstantCamera.GetNodeMap().GetNode("TriggerMode"));
		triggerMode->FromString("On");

		// Set color
		bool bIsColor = false;
		const char * pchar = camModelName.c_str();
		if ('m' == pchar[camModelName.size() - 1])
		{
			std::cout << "it is mono camera" << "\n";
			bIsColor = false;
		}
		else if ('c' == pchar[camModelName.size() - 1])
		{
			std::cout << "it is RGB camera = " << "\n";
			bIsColor = true;
		}

		// Set software trigger as default. SHould not use func to avoid the state check
		m_Cache.setNumOfImage(1);
		CEnumerationPtr triggerMode(m_InstantCamera.GetNodeMap().GetNode("TriggerSource"));
		triggerMode->FromString("Software");
		
		// set ImageEventHandler for color/bw camera
		m_pimageEventHandler = new ImageEventHandler(bIsColor);
		if (m_pimageEventHandler != NULL)
		{
			
			m_InstantCamera.RegisterImageEventHandler(m_pimageEventHandler, RegistrationMode_Append, Cleanup_None);
			m_pimageEventHandler->setCache(&m_Cache);
		}
		else
		{
			std::cout << "m_pimageEventHandler NULL" << "\n";
			return -1;
		}
		
	}
	else
	{
		std::cout << "Camera Device Not Present." << "\n";
		return -1;
	}
	return 0;
}

int baslerCapture::configurateExposure(float time)
{
	CFloatPtr exposureTime(m_InstantCamera.GetNodeMap().GetNode("ExposureTime"));
	exposureTime->SetValue(time);
	return 0;
}


int baslerCapture::CloseDevice()
{
	if (m_InstantCamera.IsOpen())
	{
		m_InstantCamera.Close();
	}

	if (m_pimageEventHandler)
	{
		m_InstantCamera.DeregisterImageEventHandler(m_pimageEventHandler);
		delete m_pimageEventHandler;
		m_pimageEventHandler = NULL;
	}

	if (m_InstantCamera.IsPylonDeviceAttached())
	{
		m_InstantCamera.DetachDevice();
		m_InstantCamera.DestroyDevice();
	}

	return 0;
}


int baslerCapture::Start()
{
	if (IsUseDevPresent())
	{
		if (m_InstantCamera.IsOpen())
		{
			if (!m_InstantCamera.IsGrabbing())
			{
				std::cout << "m_InstantCamera start capture ..." << "\n";
				m_InstantCamera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
				setCurrentState(START_STATE);
				return 0;
			}
			else
			{
				std::cout << "Camera is grabbing already." << "\n";
			}
			
		}
		else
		{
			std::cerr << "Camera is not open." << "\n";
		}
	}
	else
	{
		std::cerr << "Camera Device Not Present." << "\n";
	}
	return -1;
}

int baslerCapture::Stop()
{
	if (IsUseDevPresent())
	{
		if (m_InstantCamera.IsOpen())
		{
			if (m_InstantCamera.IsGrabbing())
			{
				m_InstantCamera.StopGrabbing();
				setCurrentState(STOP_STATE);
				return 0;
			}
		}
	}
	else
	{
		std::cerr << "Camera Device Not Present." << "\n";
	}
	return -1;
}

int baslerCapture::readyHWTrig(int numOfImagesPerTrig)
{
	std::lock_guard<std::mutex> lk(g_mu_Grab);

	//--- set number of image to cache---
	m_Cache.setNumOfImage(numOfImagesPerTrig);

	//--- set hw trigger mode ----
	CEnumerationPtr triggerMode(m_InstantCamera.GetNodeMap().GetNode("TriggerSource"));
	triggerMode->FromString("Line1");
	m_IsHWtriggerRunning = true;
}

int baslerCapture::getHWTrigImgs(std::vector<cv::Mat> &imgs)
{
	std::lock_guard<std::mutex> lk(g_mu_Grab);
	int status = 0;
	
	if (!m_IsHWtriggerRunning)
	{
		std::cerr << "Not hardwareTrigger Ready.Please ReadyHWTrig before calling this function.\n";
		return -1;
	}

	//--- get images ----
	std::vector<cv::Mat> imgs;
	status = m_Cache.getImages(imgs);
	if (status != 0)
	{
		std::cerr << "get images fail.\n";
		return -1;
	}

	if (imgs.size() == 0)
	{
		std::cerr << "image invalid.\n";
		return -1;
	}

	if (imgs[0].empty())
	{
		std::cerr << "image invalid.\n";
		return -1;
	}

	imgs = imgs[0];
	return 0;
}

int baslerCapture::ExecuteSWTrig(cv::Mat& img)
{
	std::lock_guard<std::mutex> lk(g_mu_Grab);
	int status = 0;
	if (m_IsHWtriggerRunning)
	{
		return 0;
	}

	//--- set number of image to cache---
	m_Cache.setNumOfImage(1);

	// ---set softwaretrigger mode ---
	CEnumerationPtr triggerMode(m_InstantCamera.GetNodeMap().GetNode("TriggerSource"));
	triggerMode->FromString("Software");

	// --- trigger execute
	CEnumerationPtr triggerSelector(m_InstantCamera.GetNodeMap().GetNode("TriggerSelector"));
	CCommandPtr SoftExecute(m_InstantCamera.GetNodeMap().GetNode("TriggerSoftware"));
	if (IsWritable(SoftExecute))
	{
		SoftExecute->Execute();
	}

	std::vector<cv::Mat> imgs;
	status = m_Cache.getImages(imgs);
	if (status != 0)
	{
		std::cerr << "get images fail.\n";
		return -1;
	}

	if (imgs.size() == 0)
	{
		std::cerr << "image invalid.\n";
		return -1;
	}

	if (imgs[0].empty())
	{
		std::cerr << "image invalid.\n";
		return -1;
	}
	 
	img = imgs[0];
	return 0;
}

int baslerCapture::setCurrentState(int state)
{
	std::lock_guard<std::mutex> lk(g_mu_state);
	m_currentState = state;
	return 0;
}
int baslerCapture::getCurrentState()
{
	std::lock_guard<std::mutex> lk(g_mu_state);
	return m_currentState;
}


std::shared_ptr<baslerCaptureItf> createBaslerCapture()
{
	return std::make_shared<baslerCaptureItf>();
}