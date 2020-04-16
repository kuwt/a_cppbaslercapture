/* Copyright (C) ASTRI - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by kuwingto <ronaldku@astri.org>, Jan 2019
*/

#include <pylon/PylonIncludes.h>
#include <opencv2/opencv.hpp>
#include "baselerCapture.h"
const char cameraModelName[] = "daA1280-54um";

using namespace Pylon;
using namespace GenApi;

static bool	g_bPylonAutoInitTerm = false;


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

	int setRecv(imageRecvInterface *pRecv)
	{
		m_pRecv = pRecv;
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
					m_pRecv->recvMat(outMat);
				}
				else
				{
					Pylon::CPylonImage pylonImage;
					m_ImageConverter.Convert(pylonImage, ptrGrabResult);
					cv::Mat imageBW = cv::Mat(height, width, CV_8UC1, (uint8_t*)pylonImage.GetBuffer());
					cv::Mat outMat = imageBW;
					m_pRecv->recvMat(outMat);
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
	imageRecvInterface* m_pRecv = NULL;
	Pylon::CImageFormatConverter m_ImageConverter;
};


class baslerCapture
{

public:
	baslerCapture();
	~baslerCapture();

	int configurateExposure(float exposureTime); // microsec
	int SetRecv(imageRecvInterface *pRecv);
	int AcquireImages();
	int StopAcquireImages();

	int SetToFreerunMode();
	int SetToTriggerMode();

	int SetToHardwareTrigger();
	int SetToSoftwareTrigger();
	int ExecuteSoftTrig();

private:
	int initBaslerCameras();
	int terminateBaslerCameras();
	bool IsUseDevPresent();
	
	int OpenDevice();
	int CloseDevice();

	int SetRecvToImageEventHandler(ImageEventHandler* p_imageEventHandler, imageRecvInterface *pRecv);
private:
	// Camera Devices
	int m_nTotalDeviceNum = 0;
	Pylon::DeviceInfoList_t m_listDeviceInfo;
	std::vector<std::string> m_vCamSN;
	

	// local 
	int m_UseDevIdx = 0;
	Pylon::CInstantCamera m_InstantCamera;
	ImageEventHandler* m_pimageEventHandler = NULL;

	bool m_trigger_on = false;

	static const int HARDWARE_TRIGGER = 0;
	static const int SOFTWARE_TRIGGER = 1;
	int m_triggerSource = HARDWARE_TRIGGER; // 0: hardware 1: software
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
		SetToTriggerMode();

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

		// Set hardware trigger as default
		CEnumerationPtr triggerMode(m_InstantCamera.GetNodeMap().GetNode("TriggerSource"));
		triggerMode->FromString("Line1");
		m_triggerSource = HARDWARE_TRIGGER;

		// set ImageEventHandler for color/bw camera
		m_pimageEventHandler = new ImageEventHandler(bIsColor);
		if (m_pimageEventHandler != NULL)
		{
			m_InstantCamera.RegisterImageEventHandler(m_pimageEventHandler, RegistrationMode_Append, Cleanup_None);
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

int baslerCapture::SetRecvToImageEventHandler(ImageEventHandler* p_imageEventHandler, imageRecvInterface *pRecv)
{
	if (p_imageEventHandler == NULL)
	{
		std::cout << "ERROR m_pimageEventHandler = NULL" << "\n";
		return -1;
	}

	p_imageEventHandler->setRecv(pRecv);
	return 0;
}


int baslerCapture::SetRecv(imageRecvInterface *pRecv)
{
	if (pRecv == NULL)
	{
		std::cout << "ERROR pRecv = NULL" << "\n";
		return -1;
	}
	
	if (!IsUseDevPresent())
	{
		std::cout << "camera not present" << "\n";
		return -1;
	}

	return SetRecvToImageEventHandler(m_pimageEventHandler, pRecv);
}

int baslerCapture::AcquireImages()
{
	if (IsUseDevPresent())
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
			std::cout << "Camera is not open." << "\n";
		}
	}
	else
	{
		std::cout << "Camera Device Not Present." << "\n";
	}
	return -1;
}

int baslerCapture::StopAcquireImages()
{
	if (IsUseDevPresent())
	{
		if (m_InstantCamera.IsOpen())
		{
			if (m_InstantCamera.IsGrabbing())
			{
				m_InstantCamera.StopGrabbing();
				return 0;
			}
		}
	}
	else
	{
		std::cout << "Camera Device Not Present." << "\n";
	}
	return -1;
}


int baslerCapture::SetToFreerunMode()
{
	if (m_trigger_on == true)
	{
		// Set trigger and expose
		CEnumerationPtr triggerMode(m_InstantCamera.GetNodeMap().GetNode("TriggerMode"));
		triggerMode->FromString("Off");
		m_trigger_on = false;
	}
	return 0;
}

int baslerCapture::SetToTriggerMode()
{
	if (m_trigger_on == false)
	{
		// Set trigger and expose
		CEnumerationPtr triggerMode(m_InstantCamera.GetNodeMap().GetNode("TriggerMode"));
		triggerMode->FromString("On");
		m_trigger_on = true;
	}
	return 0;
}


int baslerCapture::SetToHardwareTrigger()
{
	if (m_triggerSource == SOFTWARE_TRIGGER)
	{
		// Set trigger and expose
		CEnumerationPtr triggerMode(m_InstantCamera.GetNodeMap().GetNode("TriggerSource"));
		triggerMode->FromString("Line1");
		m_triggerSource = HARDWARE_TRIGGER;
	}
	return 0;
}

int baslerCapture::SetToSoftwareTrigger()
{
	if (m_triggerSource == HARDWARE_TRIGGER)
	{
		// Set trigger and expose
		CEnumerationPtr triggerMode(m_InstantCamera.GetNodeMap().GetNode("TriggerSource"));
		triggerMode->FromString("Software");
		m_triggerSource = SOFTWARE_TRIGGER;
	}
	return 0;
}


int baslerCapture::ExecuteSoftTrig()
{

	CEnumerationPtr triggerSelector(m_InstantCamera.GetNodeMap().GetNode("TriggerSelector"));
	CCommandPtr SoftExecute(m_InstantCamera.GetNodeMap().GetNode("TriggerSoftware"));
	if (IsWritable(SoftExecute))
	{
		SoftExecute->Execute();
	}
	return 0;
}


