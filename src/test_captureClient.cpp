
#include <string.h>
#include <stdio.h>
#include <chrono>  // for high_resolution_clock
#include <math.h>
#include <opencv2/opencv.hpp>
#include <zmq.hpp>
#include "imagepack.pb.h"

class zmqSocketClientWrapper
{
public:
	zmqSocketClientWrapper(const std::string & server, float timeoutsec = 5)
	{
		m_server = server;
		m_context = zmq::context_t(1);
		m_pSock = new zmq::socket_t(m_context, ZMQ_REQ);
		m_pSock->connect(m_server);
		int linger = 0;
		m_pSock->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
		m_timoutsec = timeoutsec;
	};
	~zmqSocketClientWrapper()
	{
		m_pSock->close();
		delete m_pSock;
	};

	int send(const std::string &msgStr)
	{
		zmq::message_t message(msgStr.size());
		memcpy(message.data(), msgStr.data(), msgStr.size());
		m_pSock->send(message);
		return 0;
	}
	int recv(std::string &msgStr)
	{
		/****** get reply **********/
		zmq::pollitem_t items[] = { { (void *)*m_pSock, 0, ZMQ_POLLIN, 0 } };	 // support timeout by poll
		zmq::poll(&items[0], 1, m_timoutsec * 1000);

		if (items[0].revents & ZMQ_POLLIN)
		{
			zmq::message_t reply;
			if (m_pSock->recv(&reply, 0))
			{
				msgStr = std::string((char*)reply.data(), reply.size());
			}
			else
			{
				std::cerr << "reply from server malformed! \n";
				return -1;
			}
		}
		else
		{
			char buffer[1024];
			snprintf(buffer, 1024, "Timeout. No response from server %s\n", m_server.c_str());
			std::cerr << buffer;
			m_pSock->close();
			m_pSock = new zmq::socket_t(m_context, ZMQ_REQ);
			m_pSock->connect(m_server);
			int linger = 0;
			m_pSock->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
		}
	}
private:
	zmq::context_t m_context;
	zmq::socket_t *m_pSock;
	std::string m_server;
	int m_timoutsec;
};



int main(int argc, char *argv[])
{
	std::string serverport = "tcp://10.6.65.126:5555";
	if (argc > 1)
	{
		serverport = std::string(argv[1]).c_str();
	}
	std::cout << "serverport = " << serverport << "\n";

	/****** connect to servers**********/
	zmqSocketClientWrapper capture_sock(serverport);

	/******  send material  **********/
	auto starttime = std::chrono::steady_clock::now();
	capture_sock.send("capture");
	std::string msgStr;

	std::vector<cv::Mat> images;
	if (capture_sock.recv(msgStr) == 0)
	{
		imagepack msg_in;
		msg_in.ParseFromString(msgStr);
		for (int i = 0; i < msg_in.imgs_size(); ++i)
		{
			int width = msg_in.imgs(i).width();
			int height = msg_in.imgs(i).height();
			cv::Mat img = cv::Mat(height, width, CV_8UC1);
			memcpy(img.data, &msg_in.imgs(i).image_data()[0], sizeof(uchar) * width * height);
			images.push_back(img);
		}

	}
	auto endtime = std::chrono::steady_clock::now();
	float timeElapsed1 = std::chrono::duration_cast<std::chrono::microseconds>(endtime - starttime).count();
	char buffer[1024];
	snprintf(buffer, 1024, "time = %f ms       ", timeElapsed1 * 0.001);
	std::cout << buffer << "\n";

	for (int i = 0; i < images.size(); ++i)
	{
		char buffer[1024];
		snprintf(buffer, 1024, "./image_%04d.bmp", i);
		cv::imwrite(buffer, images[i]);
	}

	std::cout << "press enter to continue..." << "\n";
	getchar();
	return 0;
}