// SPPEngine.cpp : Defines the entry point for the application.
//

#include "SPPWeb.h"

SPP_OVERLOAD_ALLOCATORS

#include "CivetServer.h"
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define DOCUMENT_ROOT "../Assets/web"
#define PORT "8081"

/* Exit flag for main loop */
volatile bool exitNow = false;

int main(int argc, char* argv[])
{
	mg_init_library(0);

	const char* options[] = {
		"document_root", DOCUMENT_ROOT, "listening_ports", PORT, 0 };

	std::vector<std::string> cpp_options;
	for (int i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++) {
		cpp_options.push_back(options[i]);
	}

	CivetServer server(cpp_options); // <-- C++ style start

	printf("Browse files at http://localhost:%s/\n", PORT);

	while (!exitNow) 
	{
#ifdef _WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
	}

	printf("Bye!\n");
	mg_exit_library();

	return 0;
}
