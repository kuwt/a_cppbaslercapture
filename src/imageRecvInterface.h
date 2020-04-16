/* Copyright (C) ASTRI - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by kuwingto <ronaldku@astri.org>, Jan 2019
*/

#pragma once
#include <opencv2/opencv.hpp>
class imageRecvInterface
{
public:
	virtual ~imageRecvInterface() {}
	virtual int setNumOfImage(int num) = 0;
	virtual int recvMat(cv::Mat img) = 0;
};