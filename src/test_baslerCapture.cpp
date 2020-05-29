// TestCapture.cpp : Defines the entry point for the console application.
//

#include "baslerCapture.h"
#include <iostream>
#include <thread>
#include <chrono>  // for high_resolution_clock


int liveStreamThread(std::shared_ptr<baslerCaptureItf> pCapture, int show_size)
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
		int targetHeight = show_size;
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

inline std::vector<std::string> split(const std::string& s, std::string delimiter)
{
	std::vector<std::string> result;

	std::size_t current = 0;
	std::size_t p = s.find_first_of(delimiter, 0);

	while (p != std::string::npos)
	{
		result.emplace_back(s, current, p - current);
		current = p + 1;
		p = s.find_first_of(delimiter, current);
	}

	result.emplace_back(s, current);

	return result;
}

int main(int argc, char *argv[])
{
	int startImgIdx = 0;
	float exposureTime = 15550;
	std::string imageSavePath = ".";
	int disp_size = 512;
	bool isUsedAllDevices = true;
	std::vector<std::string> snlist;
	
	// handling para
	if (argc > 2) //two parameter
	{
		startImgIdx = std::atof(std::string(argv[1]).c_str());
		imageSavePath = std::string(argv[2]).c_str();
	}
	if (argc > 3)
	{
		exposureTime = std::atof(std::string(argv[3]).c_str());
	}
	if (argc > 4)
	{
		disp_size = std::atof(std::string(argv[4]).c_str());
	}
	if (argc > 5)
	{
		isUsedAllDevices = false;
		std::vector<std::string> tmplist = split(std::string(argv[5]), ";");
		for (int i = 0; i < tmplist.size(); ++i)
		{
			snlist.push_back(tmplist[i].c_str());
		}
	}

	std::cout << "imageSavePath = " << imageSavePath << "\n";
	std::cout << "startImgIdx = " << startImgIdx << "\n";
	std::cout << "exposuretime = " << exposureTime << "\n";
	std::cout << "display_size = " << disp_size << "\n";
	std::cout << "isUsedAllDevices = " << isUsedAllDevices << "\n";
	for (int i = 0; i < snlist.size(); ++i)
	{
		std::cout << "snlist = " << snlist[i] << "\n";
	}

	/****** start camera **********/
	std::shared_ptr<baslerCaptureItf> pCapture = createBaslerCapture();
	
	if (isUsedAllDevices)
	{
		pCapture->openDevices(pCapture->getAvailableSNs());
	}
	else
	{
		pCapture->openDevices(snlist);
	}
	pCapture->configurateExposure(exposureTime);
	pCapture->start();

	/************ service loop ***************/
	int status = 0;
	int counter = startImgIdx;
	std::thread t(liveStreamThread, pCapture, disp_size);
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
				}
				counter++;
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

