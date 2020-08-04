// TestCapture.cpp : Defines the entry point for the console application.
//

#include "baslerCapture.h"
#include <iostream>
#include <thread>
#include <chrono>  // for high_resolution_clock


int main(int argc, char *argv[])
{
	std::string imageSavePath = ".";
	
	// handling para
	if (argc > 1) //1 parameter
	{
		imageSavePath = std::string(argv[1]).c_str();
	}

	std::cout << "imageSavePath = " << imageSavePath << "\n";

	/****** start camera **********/
	std::shared_ptr<baslerCaptureItf> pCapture = createBaslerCapture();
	
	pCapture->openDevices(pCapture->getAvailableSNs());
	pCapture->start();

	/************ service loop ***************/
	int status = 0;
    int counter = 0;
	while (1)
	{

		std::cout << "press k to capture, q to quit" << "\n";
		std::string action;
		std::cin >> action;

		if (action == "k")
		{
            pCapture->readyHWTrig(45);
            
			std::vector<cv::Mat> mats;
            // hardware trigger , not complete
           
            status = pCapture->getHWTrigImgs(mats);
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
	return 0;
}

