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

	virtual int configurateExposure(float exposureTime) = 0; // microsec

	virtual int Start() = 0;
	virtual int Stop() = 0;

	virtual int readyHWTrig(int numOfImagesPerTrig) = 0;
	virtual int getHWTrigImgs(std::vector<cv::Mat> &imgs) = 0;
	virtual int ExecuteSWTrig(cv::Mat& img) = 0;

	static const int START_STATE = 1;
	static const int STOP_STATE = 0;
	virtual int getCurrentState() = 0;
};

std::shared_ptr<baslerCaptureItf> createBaslerCapture();
