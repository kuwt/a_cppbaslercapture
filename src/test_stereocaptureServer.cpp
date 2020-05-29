
#include <string.h>
#include <stdio.h>
#include <chrono>  // for high_resolution_clock
#include <math.h>
#include <opencv2/opencv.hpp>
#include <zmq.hpp>
#include "imagepack.pb.h"

#include "baslerCapture.h"

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


class zmqSocketServerWrapper
{
public:
	zmqSocketServerWrapper(const std::string& server)
	{
		m_context = zmq::context_t(1);
		m_pSock = new zmq::socket_t(m_context, ZMQ_REP); // rep for server
		std::string server_ip = server;
		m_pSock->bind(server_ip);
	};
	~zmqSocketServerWrapper()
	{
		m_pSock->close();
		delete m_pSock;
	};

	int send(const std::string& msgStr)
	{
		zmq::message_t message(msgStr.size());
		memcpy(message.data(), msgStr.data(), msgStr.size());
		m_pSock->send(message);
		return 0;
	}
	int recv(std::string& msgStr)
	{
		zmq::message_t request;
		if (m_pSock->recv(&request, 0))
		{
			//std::cout << "receive message...\n";
			/************ receive message ***************/
			msgStr = std::string((char*)request.data(), request.size());
			return 0;
		}
		return -1;
	}
private:
	zmq::context_t m_context;
	zmq::socket_t* m_pSock;
};

int main(int argc, char *argv[])
{
	float exposuretime = 15550; //ms
	std::string serverport = "5555";
	bool isUsedAllDevices = true;
	std::vector<std::string> snlist;

	if (argc > 1) // 1 parameter
	{
		exposuretime = std::atof(std::string(argv[1]).c_str());
	}
	if (argc > 2)
	{
		serverport = std::string(argv[2]).c_str();
	}
	if (argc > 3)
	{
		isUsedAllDevices = false;
		std::vector<std::string> tmplist = split(std::string(argv[5]), ";");
		for (int i = 0; i < tmplist.size(); ++i)
		{
			snlist.push_back(tmplist[i].c_str());
		}
	}

	std::cout << "exposuretime = " << exposuretime << "\n";
	std::cout << "serverport = " << serverport << "\n";
	std::cout << "isUsedAllDevices = " << isUsedAllDevices << "\n";
	for (int i = 0; i < snlist.size(); ++i)
	{
		std::cout << "sn = " << snlist[i] << "\n";
	}

	/****** start service **********/
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	std::string server_ip = "tcp://*:" + serverport;
	zmqSocketServerWrapper sock(server_ip);
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
	pCapture->configurateExposure(exposuretime);
	pCapture->start();

	/************ service loop ***************/
	while (true)
	{
		std::cout << "\r" <<"streaming...";  //inplace print
		std::string  msgStr;
		if (sock.recv(msgStr) == 0)
		{
			/************ receive message ***************/
			if (msgStr == "imageRequest")
			{
				/************ service ***************/
				std::vector<cv::Mat> capturedImages;
				int status = pCapture->ExecuteSWTrig(capturedImages);
				if (status != 0)
				{
					std::cout << "capture fail\n";
				}
				else
				{
					/************ prepare reply ***************/
					imagepack sendPack;
					for (int i = 0; i < capturedImages.size(); ++i)
					{
						cv::Mat img = capturedImages[i];
						imagepack_Mat* sendMat = sendPack.add_imgs();
						(*sendMat).set_width(img.size().width);
						(*sendMat).set_height(img.size().height);
						(*sendMat).set_image_data((char *)img.data, sizeof(uchar) * img.size().width * img.size().height);
						//std::string s1 = (*sendMat).SerializeAsString();
						//std::cout << "s1.size() = " << s1.size() << "\n";
					}
					std::string s = sendPack.SerializeAsString();

					/************ send reply  ***************/
					sock.send(s);
				}
			}
		}
	}
	std::cout << "press to continue \n";
	getchar();

	return 0;
}