#pragma once

#include <winsock2.h>
#include <boost/function.hpp>
#include <boost/circular_buffer.hpp>
#include <rapidjson/document.h>

class PollingSocket
{
public:
	typedef boost::function<void (void)> OnConnectFunc;
	typedef boost::function<void (bool, rapidjson::Document& data)> OnRecvFunc;
	typedef boost::function<void (void)> OnCloseFunc;

public:
	PollingSocket();
	~PollingSocket();

	void Init(OnConnectFunc onConnect, OnRecvFunc onRecv, OnCloseFunc onClose);
	void Shutdown(bool closeCallback = true);

	void Poll();

	void AsyncConnect(const char* serverAddress);
	void AsyncSend(const char* jsonStr, int total);
	void AsyncSend(const rapidjson::Document& data);

private:
	void TrySend();
	void TryRecv();

	void GenerateJSON();

private:
	SOCKET mSocket;

	enum State
	{
		kStateWait,
		kStateConnecting,
		kStateConnected,
		kStateClosed,
	};

	State mState;

	OnConnectFunc mConnectCallback;
	OnRecvFunc mRecvCallback;
	OnCloseFunc mCloseCallback;

	typedef boost::circular_buffer<char> RingBuffer;
	RingBuffer mRecvBuffer;
	RingBuffer mSendBuffer;
};
