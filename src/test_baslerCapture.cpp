// TestCapture.cpp : Defines the entry point for the console application.
//

#include "baslerCapture.h"
#include <iostream>
#include <thread>
#include <chrono>  // for high_resolution_clock


int liveStreamThread(std::shared_ptr<baslerCaptureItf> pCapture)
{
	while (pCapture->getCurrentState() == baslerCaptureItf::RUNNING_STATE)
	{
		auto starttime = std::chrono::steady_clock::now();

		/***** get images *****/
		std::vector<cv::Mat> mats;
		int status = pCapture->ExecuteSWTrig(mats);
		if (status != 0 && mats.empty())
		{
			continue;
		}

		bool isAnyEmopty = false;
		for (int i = 0; i < mats.size(); ++i)
		{
			if (mats[i].empty())
			{
				isAnyEmopty = true;
			}
		}
		if (isAnyEmopty)
		{
			continue;
		}

		std::vector<cv::Mat> resize_mats;
		/***** resize *****/
		int targetHeight = 500;
		for (int i = 0; i < mats.size(); ++i)
		{
			cv::Mat resizeMat;
			float scaleFactor = float(targetHeight) / float(mats[i].size().height);
			int targetWidth = int(mats[i].size().width * scaleFactor);
			cv::Mat mat_hconcat_resize;
			cv::resize(mats[i], resizeMat, cv::Size(targetWidth, targetHeight));
			resize_mats.push_back(resizeMat);
		}

		/***** concat *****/
		cv::Mat mat_hconcat = resize_mats[0];
		for (int i = 1; i < resize_mats.size(); ++i)
		{
			cv::hconcat(mat_hconcat, resize_mats[i], mat_hconcat);
		}

		/***** fps *****/
		auto endtime = std::chrono::steady_clock::now();
		float timeElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endtime - starttime).count();
		float fps = 1.0 * 1000 / timeElapsed;

		char buffer[1024];
		snprintf(buffer, 1024, "fps = %f Hz", fps);
		cv::putText(mat_hconcat, buffer,
			cvPoint(30, 30), cv::FONT_HERSHEY_COMPLEX_SMALL,
			0.8, cvScalar(200, 200, 250), 1, CV_AA);

		/***** show *****/
		cv::imshow("livevideo", mat_hconcat);
		cv::waitKey(1);

	}
	return 0;
}

int main(int argc, char *argv[])
{
	float exposureTime;
	std::string imageSavePath;
	// handling para
	if (argc < 2)
	{
		// default para
		std::cout << "using default exposurePara\n";
		exposureTime = 15550;
		std::cout << "exposureTime = " << exposureTime << "\n";

		imageSavePath = ".";
		std::cout << "imageSavePath = " << imageSavePath << "\n";
	}
	else
	{
		exposureTime = std::atof(std::string(argv[1]).c_str());
		std::cout << "exposureTime = " << exposureTime << "\n";
		imageSavePath = std::string(argv[3]).c_str();
		std::cout << "imageSavePath = " << imageSavePath << "\n";

	}

	std::shared_ptr<baslerCaptureItf> pCapture = createBaslerCapture();
	
	std::vector<std::string> v_sn;
	v_sn.push_back("22080226");
	//pCapture->openDevices(v_sn);
	pCapture->openDevices(pCapture->getAvailableSNs());
	
	pCapture->configurateExposure(exposureTime);
	pCapture->start();
	int status = 0;
	int counter = 0;
	std::thread t(liveStreamThread, pCapture);
	while (1)
	{

		std::cout << "press k to capture, q to quit" << "\n";
		std::string action;
		std::cin >> action;

		if (action == "k")
		{
			std::vector<cv::Mat> mats;
			status = pCapture->ExecuteSWTrig(mats);
			if (status != 0)
			{
				std::cout << "capture fail\n";
			}
			else
			{
				std::cout << "capture successful\n";
				for (int i = 0; i < mats.size(); ++i)
				{
					char buf[1024];
					snprintf(buf, 1024, "%s/cam_%d_%d.bmp", imageSavePath.c_str(), i, counter);
					cv::imwrite(buf, mats[i]);
					counter++;
				}
			}
		}
		else if (action == "q")
		{
			break;
		}
		else
		{
			std::cout << "no such action!\n";
		}
	}

	pCapture->stop();
	t.join();
	return 0;
}

