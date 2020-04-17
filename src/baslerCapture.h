/* Copyright (C) ASTRI - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by kuwingto <ronaldku@astri.org>, Jan 2019
*/


#pragma once
#include <opencv2/opencv.hpp>
#include <memory>


class baslerCaptureItf
{
public:
	virtual ~baslerCaptureItf() {}
	static const int RUNNING_STATE = 1;
	static const int STOP_STATE = 0;

	virtual std::vector<std::string> getAvailableSNs() = 0;
	virtual int openDevices(const std::vector<std::string> &camSNs) = 0;
	virtual int getNumOfWorkingDevices() = 0;
	virtual int configurateExposure(float exposureTime) = 0; // microsec
	virtual int start() = 0;
	virtual int stop() = 0;
	virtual int readyHWTrig(int numOfTrig) = 0;
	virtual int getHWTrigImgs(std::vector<cv::Mat> &imgs) = 0;
	virtual int ExecuteSWTrig(std::vector<cv::Mat> &imgs) = 0;
	virtual int getCurrentState() = 0;

};

std::shared_ptr<baslerCaptureItf> createBaslerCapture();
