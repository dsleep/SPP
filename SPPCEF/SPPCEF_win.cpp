#include <windows.h>
#include "SPPCEFUI.h"

SPP_OVERLOAD_ALLOCATORS

// Program entry point function.
int APIENTRY wWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPTSTR lpCmdLine,
	int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	return SPP::RunBrowser(hInstance, "" );
}
