#include <windows.h>
#include "SPPCEFUI.h"


#include "SPPPlatformCore.h"

SPP_OVERLOAD_ALLOCATORS

// Program entry point function.
int APIENTRY wWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPTSTR lpCmdLine,
	int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	SPP::AddDLLSearchPath("../3rdParty/cef/Release");

	return SPP::RunBrowser(hInstance, "" );
}
