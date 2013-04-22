#include "PollingSocket.h"

#include <boost/array.hpp>
#include <cassert>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "Network.h"
#include "Log.h"

#pragma warning(disable:4996) //4996: 'std::copy': Function call with parameters that may be unsafe - this call relies on the caller to check that the passed values are correct. To disable this warning, use -D_SCL_SECURE_NO_WARNINGS. See documentation on how to use Visual C++ 'Checked Iterators'

namespace
{
	const int kMaxDataSize = 1024;
}

PollingSocket::PollingSocket()
	: mSocket(INVALID_SOCKET)
	, mState(kStateClosed)
	, mRecvBuffer(kMaxDataSize)
	, mSendBuffer(kMaxDataSize)
{
}


PollingSocket::~PollingSocket()
{
}


void PollingSocket::Init(OnConnectFunc onConnect, OnRecvFunc onRecv, OnCloseFunc onClose)
{
	mConnectCallback = onConnect;
	mRecvCallback = onRecv;
	mCloseCallback = onClose;

	mSocket = Network::CreateSocket();

	u_long mode = 1; // should be non-zero for non-blocking socket.
	if (NO_ERROR != ioctlsocket(mSocket, FIONBIO, &mode))
	{
		ERROR_CODE(WSAGetLastError(), "PollingSocket::Init() - ioctlsocket failed.");
		Shutdown();
		return;
	}

	std::string address;
	u_short port;
	Network::GetLocalAddress(mSocket, address, port);
	LOG("PollingSocket::Init() - socket created. [%s:%d]", address.c_str(), port);

	mState = kStateWait;
}


void PollingSocket::Shutdown(bool closeCallback)
{
	OnCloseFunc onClose = mCloseCallback;

	Network::CloseSocket(mSocket);
	mSocket = INVALID_SOCKET;

	mState = kStateClosed;

	mConnectCallback.clear();
	mRecvCallback.clear();
	mCloseCallback.clear();

	mRecvBuffer.clear();
	mSendBuffer.clear();

	if (closeCallback)
	{
		// give a chance to re-init this.
		onClose();
	}
}


void PollingSocket::Poll()
{
	if (mState == kStateClosed || mState == kStateWait)
	{
		return;
	}

	fd_set read_set, write_set, except_set;

	memset(&read_set, 0, sizeof(fd_set));
	read_set.fd_array[0] = mSocket;
	read_set.fd_count = 1;

	memcpy(&write_set, &read_set, sizeof(fd_set)); 
	memcpy(&except_set, &read_set, sizeof(fd_set)); 

	TIMEVAL timeout = {0,0}; // immediate

	int result = select(0, &read_set, &write_set, &except_set, &timeout);

	if (SOCKET_ERROR == result)
	{
		ERROR_CODE(WSAGetLastError(), "PollingSocket::Poll() - select failed.");
		Shutdown();
		return;
	}

	if (result == 0)
	{
		// no notifications.
		return;
	}

	if (FD_ISSET(mSocket, &read_set))
	{
		TryRecv();
	}

	if (FD_ISSET(mSocket, &write_set))
	{
		if (mState == kStateConnecting)
		{
			mState = kStateConnected;
			mConnectCallback();

			std::string address;
			u_short port;
			Network::GetRemoteAddress(mSocket, address, port);
			LOG("PollingSocket::Poll() - connected. [%s:%d]", address.c_str(), port);
		}
		else
		{
			TrySend();
		}
	}

	if (FD_ISSET(mSocket, &except_set))
	{
		if (mState == kStateConnecting)
		{			
			LOG("PollingSocket::Poll() - connect failed.");
			Shutdown();
			return;
		}
	}

}


void PollingSocket::AsyncConnect(const char* serverAddress)
{
	if(mState != kStateWait)
	{
		return;
	}

	assert(mSocket != INVALID_SOCKET);
		
    sockaddr_in address;
	int sizeAddr = sizeof(sockaddr_in);
	memset(&address, 0, sizeof(sockaddr_in));

	if (0 != WSAStringToAddressA(const_cast<char*>(serverAddress), AF_INET, NULL, reinterpret_cast<sockaddr*>(&address), &sizeAddr))
	{
		ERROR_CODE(WSAGetLastError(), "PollingSocket::AsyncConnect() - WSAStringToAddress failed");
		Shutdown();
		return;
	}

    int result = connect(mSocket, reinterpret_cast<sockaddr*>(&address), sizeof(sockaddr_in));
	if (SOCKET_ERROR == result)
	{
		if (WSAEWOULDBLOCK != WSAGetLastError())
		{
			ERROR_CODE(WSAGetLastError(), "PollingSocket::AsyncConnect() - connect failed.");
			Shutdown();
			return;
		}
	}	

	mState = kStateConnecting;
}


void PollingSocket::AsyncSend(const char* jsonStr, int total)
{
	if(mState != kStateConnected)
	{
		return;
	}

	int available = mSendBuffer.capacity() - mSendBuffer.size();
	if (available < total)
	{
		mSendBuffer.set_capacity(mSendBuffer.capacity() + total*2);
	}
	assert(mSendBuffer.capacity() - mSendBuffer.size() >= static_cast<size_t>(total));

	mSendBuffer.insert(mSendBuffer.end(), jsonStr, jsonStr + total); 

	TrySend();
}

	
void PollingSocket::AsyncSend(const rapidjson::Document& data)
{
	if(mState != kStateConnected)
	{
		return;
	}

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	data.Accept(writer);

	AsyncSend(buffer.GetString(), buffer.Size()+1);
}


void PollingSocket::TrySend()
{
	if(mState != kStateConnected)
	{
		return;
	}

	while (!mSendBuffer.empty())
	{
		boost::array<char, kMaxDataSize> temp;

		RingBuffer::iterator itorEnd = mSendBuffer.begin();
		size_t total = std::min<size_t>(mSendBuffer.size(), kMaxDataSize);
		std::advance(itorEnd, total);

		assert(std::distance(mSendBuffer.begin(), itorEnd) <= kMaxDataSize);
		std::copy(mSendBuffer.begin(), itorEnd, temp.begin());

		int result = send(mSocket, temp.data(), total, 0);

		if (SOCKET_ERROR == result)
		{
			if (WSAEWOULDBLOCK != WSAGetLastError())
			{
				ERROR_CODE(WSAGetLastError(), "PollingSocket::TrySend() - send failed.");
				Shutdown();				
				return;
			}

			// WSAEWOULDBLOCK.
			return;
		}
		
		assert(result > 0);

		LOG("ClientSocket::OnSend - sending succeeded. %d / %d.", result, total);

		itorEnd = mSendBuffer.begin();
		std::advance(itorEnd, result);
		mSendBuffer.erase(mSendBuffer.begin(), itorEnd);
	}
}


void PollingSocket::TryRecv()
{
	if(mState != kStateConnected)
	{
		return;
	}

	int result = 0;
	char temp[kMaxDataSize];

	while ( (result = recv(mSocket, temp, kMaxDataSize, 0)) > 0 )
	{
		int available = mRecvBuffer.capacity() - mRecvBuffer.size();
		if (available < result)
		{
			mRecvBuffer.set_capacity(mRecvBuffer.capacity() + result*2);
		}
		assert(mRecvBuffer.capacity() - mRecvBuffer.size() >= static_cast<size_t>(result));

		mRecvBuffer.insert(mRecvBuffer.end(), temp, temp + result);

		LOG("PollingSocket::OnReceive - received [%d].", result);

		GenerateJSON();
	}

	if (0 == result)
	{
		LOG("PollingSocket::OnReceive - closed by remote.");
		Shutdown();
		return;
	}
	else if (SOCKET_ERROR == result && WSAEWOULDBLOCK != WSAGetLastError())
	{
		ERROR_CODE(WSAGetLastError(), "PollingSocket::OnReceive - recv failed.");
		Shutdown();
		return;
	}

	assert(WSAEWOULDBLOCK == WSAGetLastError());
}


void PollingSocket::GenerateJSON()
{
	RingBuffer::iterator itorEnd = std::find(mRecvBuffer.begin(), mRecvBuffer.end(), '\0');

	while (itorEnd != mRecvBuffer.end())
	{
		++itorEnd;

		boost::array<char, kMaxDataSize> jsonStr;

		assert(std::distance(mRecvBuffer.begin(), itorEnd) <= kMaxDataSize);
		std::copy(mRecvBuffer.begin(), itorEnd, jsonStr.begin());

		mRecvBuffer.erase(mRecvBuffer.begin(), itorEnd);

		rapidjson::Document jsonData;
		jsonData.Parse<0>(jsonStr.data());

		if (jsonData.HasParseError())
		{
			LOG("PollingSocket::GenerateJSON - parsing failed. %s error[%s]", jsonStr.data(), jsonData.GetParseError());
		}
		else
		{
			LOG("PollingSocket::GenerateJSON - parsing succeeded. %s", jsonStr.data());
		}

		mRecvCallback(jsonData.HasParseError(), jsonData);

		itorEnd = std::find(mRecvBuffer.begin(), mRecvBuffer.end(), '\0');
	}
}

