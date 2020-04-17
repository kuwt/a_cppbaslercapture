// TestCapture.cpp : Defines the entry point for the console application.
//

#include "baslerCapture.h"
#include <iostream>
#include <Windows.h>
#include <thread>

int liveStreamThread(std::shared_ptr<baslerCaptureItf> pCapture)
{
	while (pCapture->getCurrentState() == baslerCaptureItf::RUNNING_STATE)
	{
		std::vector<cv::Mat> mats;
		int status = pCapture->ExecuteSWTrig(mats);
		if (status == 0 && !mats.empty())
		{
			cv::imshow("livevideo", mats[0]);
			cv::waitKey(1);
		}
		Sleep(50);
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
	pCapture->openDevices(v_sn);
	//pCapture->openDevices(pCapture->getAvailableSNs());
	
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

