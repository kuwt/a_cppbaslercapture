// TestCapture.cpp : Defines the entry point for the console application.
//

#include "baslerCapture.h"
#include <iostream>
#include <Windows.h>
#include <thread>

int liveStreamThread(std::shared_ptr<baslerCaptureItf> pCapture)
{
	while (pCapture->getCurrentState() == baslerCaptureItf::START_STATE)
	{
		cv::Mat mat;
		int status = pCapture->captureStreaming(mat);
		if (status == 0 && !mat.empty())
		{
			cv::imshow("livevideo", mat);
			cv::waitKey(1);
		}
		Sleep(50);
	}
	return 0;
}



int main(int argc, char *argv[])
{
	float exposureTime;
	int numberOfImages;
	std::string imageSavePath;
	// handling para
	if (argc < 2)
	{
		// default para
		std::cout << "using default exposurePara\n";
		exposureTime = 15550;
		std::cout << "exposureTime = " << exposureTime << "\n";

		numberOfImages = 27;
		std::cout << "numberOfImages = " << numberOfImages << "\n";

		imageSavePath = ".";
		std::cout << "imageSavePath = " << imageSavePath << "\n";
	}
	else
	{
		exposureTime = std::atof(std::string(argv[1]).c_str());
		std::cout << "exposureTime = " << exposureTime << "\n";
		numberOfImages = std::atof(std::string(argv[2]).c_str());
		std::cout << "numberOfImages = " << numberOfImages << "\n";
		imageSavePath = std::string(argv[3]).c_str();
		std::cout << "imageSavePath = " << imageSavePath << "\n";

	}

	std::shared_ptr<projectorCameraCaptureInterface> pCapture = createProjectorCameraCapture();

	int status = pCapture->init("COM1");
	if (status != 0)
	{
		std::cout << "projectorCameraCaptureInterface init error\n";
		getchar();
		return -1;
	}
	pCapture->configurateNumOfSeriesImage(numberOfImages);
	//pCapture->configurateExposure(exposureTime);
	pCapture->start();

	std::thread t(liveStreamThread, pCapture);
	while (1)
	{

		std::cout << "press k to capture, q to quit" << "\n";
		std::string action;
		std::cin >> action;

		if (action == "k")
		{
			std::vector<cv::Mat> mats;
			status = pCapture->captureProSeries(mats);
			if (status != 0)
			{
				std::cout << "capture fail\n";
			}
			else
			{
				std::cout << "capture successful\n";
				for (int i = 0; i < mats.size(); ++i)
				{
					std::cout << "save images " << i << "\n";

					char buf[1024];
					snprintf(buf, 1024, "%s/%04d.bmp", imageSavePath, i);
					cv::imwrite(buf, mats.at(i));
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

