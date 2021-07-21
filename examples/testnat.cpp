// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPNatTraversal.h"

using namespace SPP;
int main()
{
	IntializeCore(nullptr);

	auto juiceSocket = std::make_shared<UDPJuiceSocket>();

	using namespace std::chrono_literals;
	while (true)
	{
	
		std::this_thread::sleep_for(1s);
	}

	return 0;
}