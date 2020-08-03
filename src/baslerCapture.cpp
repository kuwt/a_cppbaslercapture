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

/****************************************

ImageCache

*****************************************/
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

		bool bStatus = m_con_v_imageCache.wait_for(lk, std::chrono::seconds(10), [&]() {return m_is_condition_ready; });
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
		m_is_condition_ready = false;
		return status;
	}

private:

	bool m_is_condition_ready = false;
	std::mutex m_mu_imageCache;
	std::condition_variable m_con_v_imageCache;

	std::mutex m_mu_grab;
	std::mutex m_mu_imageCacheNumOfImage;

	unsigned int m_NumImages= 1;
	unsigned int m_currentImageCnt = 0;
	std::vector<cv::Mat> m_vMat;
};

/****************************************

ImageEventHandler

*****************************************/
class ImageEventHandler : public  Pylon::CImageEventHandler
{
public:
	ImageEventHandler(){}
	~ImageEventHandler() {}

	int setColor(bool bIsColor)
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
		return 0;
	}

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

/****************************************

baslerCam

*****************************************/
class baslerCam
{
public:
	baslerCam() {};
	~baslerCam();
	int init(CDeviceInfo info);
	std::string getSerial();
	int configurateExposure(float exposureTime); // microsec
	int start();
	int stop();
	int readyHWTrig(int numOfTrig);
	int getHWTrigImgs(std::vector<cv::Mat> &imgs);
	int ExecuteSWTrig(cv::Mat& img);
private:
	int OpenDevice(CDeviceInfo info);
	int CloseDevice();
private:

	int m_UseDevIdx = 0;
	std::string m_CamSN;
	Pylon::CInstantCamera m_InstantCamera;
	ImageEventHandler m_imageEventHandler;
	ImageCache m_Cache;
	bool m_IsHWtriggerRunning = false;
};

int baslerCam::init(CDeviceInfo info)
{
	int status = 0;
	try
	{
		status = OpenDevice(info);
		if (status != 0)
		{
			std::cerr << "init fail.\n";
			return -1;
		}
	}
	catch (GenICam::GenericException &e)
	{
		std::cerr << "init fail. \n";
		std::cerr << e.what();
		return -1;
	}

	return 0;
}

std::string baslerCam::getSerial()
{
	return m_CamSN;
}

baslerCam::~baslerCam()
{
	CloseDevice();
}

int baslerCam::OpenDevice(CDeviceInfo info)
{
	//  prepare m_InstantCamera
	if (m_InstantCamera.IsPylonDeviceAttached())
	{
		m_InstantCamera.DetachDevice();
		m_InstantCamera.DestroyDevice();
	}

	m_InstantCamera.Attach(CTlFactory::GetInstance().CreateDevice(info));
	m_InstantCamera.Open();

	std::string camModelName = std::string( CStringPtr(m_InstantCamera.GetNodeMap().GetNode("DeviceModelName"))->GetValue().c_str());
	std::cout << "camModelName = " << camModelName << "\n";

	//  assign sn
	m_CamSN = info.GetSerialNumber().c_str();

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
	CEnumerationPtr triggerSource(m_InstantCamera.GetNodeMap().GetNode("TriggerSource"));
	triggerSource->FromString("Software");

	// set ImageEventHandler 
	m_InstantCamera.RegisterImageEventHandler(&m_imageEventHandler, RegistrationMode_Append, Cleanup_None);
	m_imageEventHandler.setCache(&m_Cache);
	m_imageEventHandler.setColor(bIsColor);
	

	return 0;
}

int baslerCam::configurateExposure(float time)
{
	CFloatPtr exposureTime(m_InstantCamera.GetNodeMap().GetNode("ExposureTime"));
	exposureTime->SetValue(time);
	return 0;
}


int baslerCam::CloseDevice()
{
	if (m_InstantCamera.IsOpen())
	{
		m_InstantCamera.Close();
	}

	m_InstantCamera.DeregisterImageEventHandler(&m_imageEventHandler);
		

	if (m_InstantCamera.IsPylonDeviceAttached())
	{
		m_InstantCamera.DetachDevice();
		m_InstantCamera.DestroyDevice();
	}

	return 0;
}

int baslerCam::start()
{
	if (m_InstantCamera.IsOpen())
	{
		if (!m_InstantCamera.IsGrabbing())
		{
			std::cout << "m_InstantCamera start capture ..." << "\n";
			m_InstantCamera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
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
	return -1;
}

int baslerCam::stop()
{
	if (m_InstantCamera.IsOpen())
	{
		if (m_InstantCamera.IsGrabbing())
		{
			m_InstantCamera.StopGrabbing();
			return 0;
		}
	}
	return -1;
}

int baslerCam::readyHWTrig(int numOfTrig)
{
	std::lock_guard<std::mutex> lk(g_mu_Grab);

	//--- set number of image to cache---
	m_Cache.setNumOfImage(numOfTrig);

	//--- set hw trigger mode ----
	CEnumerationPtr triggerMode(m_InstantCamera.GetNodeMap().GetNode("TriggerSource"));
	triggerMode->FromString("Line1");
	m_IsHWtriggerRunning = true;

	return 0;
}

int baslerCam::getHWTrigImgs(std::vector<cv::Mat> &imgs)
{
	std::lock_guard<std::mutex> lk(g_mu_Grab);
	int status = 0;

	if (!m_IsHWtriggerRunning)
	{
		std::cerr << "Not hardwareTrigger Ready.Please ReadyHWTrig before calling this function.\n";
		return -1;
	}

	//--- get images ----
	std::vector<cv::Mat> _imgs;
	status = m_Cache.getImages(_imgs);
	if (status != 0)
	{
		std::cerr << "get images fail.\n";
		return -1;
	}

	if (_imgs.size() == 0)
	{
		std::cerr << "image invalid.\n";
		return -1;
	}

	if (_imgs[0].empty())
	{
		std::cerr << "image invalid.\n";
		return -1;
	}

	imgs = _imgs;
	m_IsHWtriggerRunning = false;
	return 0;
}

int baslerCam::ExecuteSWTrig(cv::Mat& img)
{
	std::lock_guard<std::mutex> lk(g_mu_Grab);
	int status = 0;
	if (m_IsHWtriggerRunning)
	{
		return 1;
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


/****************************************

baslerCapture

*****************************************/
class baslerCapture : public baslerCaptureItf
{
public:
	baslerCapture();
	virtual ~baslerCapture();

	std::vector<std::string> getAvailableSNs();
	int openDevices(const std::vector<std::string> &camSNs);
	int getNumOfWorkingDevices();
	int configurateExposure(float exposureTime); // microsec
	int start();
	int stop();
	int readyHWTrig(int numOfTrig);
	int getHWTrigImgs(std::vector<cv::Mat> &imgs);
	int ExecuteSWTrig(std::vector<cv::Mat> &imgs);
	int getCurrentState();
	
private:
	int initBaslerCameras();
	int terminateBaslerCameras();

	int addDevice(const std::string &camSN);
	int setCurrentState(int state);
private:
	// Camera Devices
	bool m_isInited = false;
	int m_nTotalDeviceNum = 0;
	int m_currentState = STOP_STATE;
	Pylon::DeviceInfoList_t m_listDeviceInfo;

	std::vector<baslerCam*> m_vpWorkingCameras;

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
}


int baslerCapture::openDevices(const std::vector<std::string> &camSNs)
{
	try
	{
		for (int i = 0; i < camSNs.size(); ++i)
		{
			addDevice(camSNs[i]);
		}
	}
	catch (GenICam::GenericException &e)
	{
		char buff[1024];
		snprintf(buff, sizeof(buff), "Catch Exception OpenDevice: %s", e.GetDescription());
		std::string str;
		str = buff;
		std::cout << str << "\n";
		return -1;
	}
	return 0;
}

baslerCapture::~baslerCapture()
{
	for (int i = 0; i < m_vpWorkingCameras.size(); ++i)
	{
		delete m_vpWorkingCameras[i];
		m_vpWorkingCameras[i] = NULL;
	}
	terminateBaslerCameras();
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

int baslerCapture::getNumOfWorkingDevices()
{
	return m_vpWorkingCameras.size();
}

std::vector<std::string> baslerCapture::getAvailableSNs()
{
	std::vector<std::string> SNlist;
	for (int i = 0; i < m_nTotalDeviceNum; i++)
	{
		DeviceInfoList_t::iterator it = m_listDeviceInfo.begin() + i;
		std::string strDeviceSN = (*it).GetSerialNumber().c_str();
		std::cout << "strDeviceSN " << std::to_string(i) << " = " << strDeviceSN << "\n";
		SNlist.push_back(strDeviceSN);
	}
	return SNlist;
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
	int nTotalDeviceNum = tlFactory.EnumerateDevices(listDeviceInfo);
	std::cout << "m_nTotalAvailableDeviceNum = " << nTotalDeviceNum << "\n";
	m_nTotalDeviceNum = nTotalDeviceNum;
	m_listDeviceInfo = listDeviceInfo;

	//Print SN from device list
	getAvailableSNs();

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


int baslerCapture::addDevice(const std::string &camSN)
{
	int status = 0;
	// locate camera index
	int camIdx = -1;
	for (int i = 0; i < m_listDeviceInfo.size(); ++i)
	{
		std::string s1(m_listDeviceInfo[i].GetSerialNumber().c_str());
		if (s1 == camSN)
		{
			camIdx = i;
		}
	}
	if (camIdx == -1)
	{
		std::cerr << "camSN = " << camSN << "not found.\n";
		return -1;
	}

	baslerCam* p_cam = new baslerCam();
	if (p_cam != NULL)
	{
		status = p_cam->init(m_listDeviceInfo[camIdx]);
		if (status == 0)
		{
			m_vpWorkingCameras.push_back(p_cam);
		}
	}
	
	return 0;
}

int baslerCapture::configurateExposure(float exposureTime)
{
	for (int i = 0; i < m_vpWorkingCameras.size(); ++i)
	{
		m_vpWorkingCameras[i]->configurateExposure(exposureTime);
	}
	return 0;
}
int baslerCapture::start()
{
	for (int i = 0; i < m_vpWorkingCameras.size(); ++i)
	{
		int status = m_vpWorkingCameras[i]->start();
		if (status != 0)
		{
			std::cerr << m_vpWorkingCameras[i]->getSerial() << " fails to start.\n";
		}
	}
	setCurrentState(RUNNING_STATE);
	return 0;
}
int baslerCapture::stop()
{
	for (int i = 0; i < m_vpWorkingCameras.size(); ++i)
	{
		int status = m_vpWorkingCameras[i]->stop();
		if (status != 0)
		{
			std::cerr << m_vpWorkingCameras[i]->getSerial() << " fails to stop.\n";
		}
	}
	setCurrentState(STOP_STATE);
	return 0;
}
int baslerCapture::readyHWTrig(int numOfTrig)
{
	for (int i = 0; i < m_vpWorkingCameras.size(); ++i)
	{
		int status = m_vpWorkingCameras[i]->readyHWTrig(numOfTrig);
		if (status != 0)
		{
			std::cerr << m_vpWorkingCameras[i]->getSerial() << " fails to readyHWTrig.\n";
		}
	}
	return 0;
}
int baslerCapture::getHWTrigImgs(std::vector<cv::Mat> &imgs)
{
	imgs.clear();
	for (int i = 0; i < m_vpWorkingCameras.size(); ++i)
	{
		std::vector<cv::Mat> imgs_per_cam;
		int status = m_vpWorkingCameras[i]->getHWTrigImgs(imgs_per_cam);
		if (status != 0)
		{
			std::cerr << m_vpWorkingCameras[i]->getSerial() << " fails to getHWTrigImgs.\n";
			return -1;
		}
		imgs.insert(imgs.end(), imgs_per_cam.begin(), imgs_per_cam.end());
	}
	return 0;
}
int baslerCapture::ExecuteSWTrig(std::vector<cv::Mat> &imgs)
{
	imgs.clear();
	for (int i = 0; i < m_vpWorkingCameras.size(); ++i)
	{
		cv::Mat img_per_cam;
		int status = m_vpWorkingCameras[i]->ExecuteSWTrig(img_per_cam);
		if (status != 0)
		{
			std::cerr << m_vpWorkingCameras[i]->getSerial() << " fails to ExecuteSWTrig.\n";
			return -1;
		}
		imgs.push_back(img_per_cam);
	}
	return 0;
}

std::shared_ptr<baslerCaptureItf> createBaslerCapture()
{
	return std::make_shared<baslerCapture>();
}