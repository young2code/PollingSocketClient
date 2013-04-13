
#include "Log.h"
#include "Network.h"


void main()
{
	Log::Init();
	Network::Init();


	Network::Shutdown();
	Log::Shutdown();
}