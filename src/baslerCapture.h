/* Copyright (C) ASTRI - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by kuwingto <ronaldku@astri.org>, Jan 2019
*/


#pragma once
#include <opencv2/opencv.hpp>

class baslerHardwareTriggerImgRecvItf
{
public:
	virtual ~baslerHardwareTriggerImgRecvItf() {}
	virtual int recvMat(cv::Mat img) = 0;
};

class baslerCaptureItf
{
public:
	virtual ~baslerCaptureItf() {}

	int configurateExposure(float exposureTime); // microsec
	int SetToHardwareTrigger();
	int SetToSoftwareTrigger();
	int SetHardwareTriggerImgRecv(baslerHardwareTriggerImgRecvItf *pRecv);

	int StartAcquireImages();
	int StopAcquireImages();
	int ExecuteSoftTrig(cv::Mat& img);
};

baslerCaptureItf* createBaslerCapture();
