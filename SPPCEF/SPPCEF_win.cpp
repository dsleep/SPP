#include <windows.h>
#include "SPPCEFUI.h"


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
