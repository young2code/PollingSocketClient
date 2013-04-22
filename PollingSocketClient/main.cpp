
#include "Log.h"
#include "Network.h"
#include "PollingSocket.h"

#include <boost/bind.hpp>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

class Test
{
public:
	Test(PollingSocket& pollingSocket) : mPollingSocket(pollingSocket), mClosed(false)
	{
	}

	void OnConnect()
	{
		LOG("Test::OnConnect()");
	}


	void OnRecv(bool, const rapidjson::Document& data)
	{
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		data.Accept(writer);

		LOG("Test::OnRecv() - %s", buffer.GetString());
	}


	void OnClose()
	{
		LOG("Test::OnClose()");
		mClosed = true;
	}


	bool IsClosed() const { return mClosed; }

private:
	PollingSocket& mPollingSocket;
	bool mClosed;
};


void main(int argc, char* argv[])
{
	Log::Init();
	Network::Init();

	if(argc != 2)
	{
		LOG("Please add server address");
		LOG("(ex) 127.0.0.1:1234");
		return;
	}

	const char* address = argv[1];

	LOG("Input : Server address: %s", address);

	PollingSocket pollingSocket;

	Test test(pollingSocket);
	PollingSocket::OnConnectFunc onConnect = boost::bind(&Test::OnConnect, &test);
	PollingSocket::OnRecvFunc onRecv = boost::bind(&Test::OnRecv, &test, _1, _2);
	PollingSocket::OnCloseFunc onClose = boost::bind(&Test::OnClose, &test);

	pollingSocket.Init(onConnect, onRecv, onClose);
	pollingSocket.AsyncConnect(address);

	while (!test.IsClosed())
	{
		pollingSocket.Poll();

		if (rand() % 1000 < 10)
		{
			rapidjson::Document data;
			data.SetObject();
			data.AddMember("test", rand(), data.GetAllocator());
			pollingSocket.AsyncSend(data);
		}
	}

	Network::Shutdown();
	Log::Shutdown();
}