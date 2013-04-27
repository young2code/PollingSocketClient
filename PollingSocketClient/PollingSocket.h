#pragma once

#include <winsock2.h>
#include <boost/function.hpp>
#include <boost/circular_buffer.hpp>
#include <rapidjson/document.h>

class PollingSocket
{
public:
	typedef boost::function<void (PollingSocket*)> OnConnectFunc;
	typedef boost::function<void (PollingSocket*, bool, rapidjson::Document& data)> OnRecvFunc;
	typedef boost::function<void (PollingSocket*)> OnCloseFunc;
	typedef boost::function<void (PollingSocket*)> OnAcceptFunc;

public:
	PollingSocket();
	~PollingSocket();

	bool InitWait(OnConnectFunc onConnect, OnRecvFunc onRecv, OnCloseFunc onClose);
	bool InitListen(unsigned short port, OnAcceptFunc onAccept, OnCloseFunc onClose);
	void InitAccept(SOCKET socketAccpted, OnRecvFunc onRecv, OnCloseFunc onClose);

	void Shutdown(bool closeCallback = true);

	void Poll();


	void AsyncConnect(const char* serverAddress);
	void AsyncSend(const char* jsonStr, int total);
	void AsyncSend(const rapidjson::Document& data);

	SOCKET GetSocket() const { return mSocket; }

private:
	bool CreateSocket(unsigned short port);

	void TrySend();
	void TryRecv();

	void GenerateJSON();

private:
	SOCKET mSocket;

	enum State
	{
		kStateWait,
		kStateListening,
		kStateConnecting,
		kStateConnected,
		kStateClosed,
	};

	State mState;

	OnConnectFunc mConnectCallback;
	OnRecvFunc mRecvCallback;
	OnCloseFunc mCloseCallback;
	OnAcceptFunc mAcceptCallback;

	typedef boost::circular_buffer<char> RingBuffer;
	RingBuffer mRecvBuffer;
	RingBuffer mSendBuffer;
};
