// RemoteAppServer.cpp : Defines the entry point for the application.
//

#include "SPPCapture.h"
#include "windows.h"
#include <thread>
#include <iostream>
#include <string>
#include <mutex>
#include <chrono>
#include <algorithm>

#include "Windowsx.h"

#define ROUNDUP(x, y) ((x + y - 1) & ~(y - 1))

struct handle_data {
    uint32_t process_id;
    HWND window_handle;
};

BOOL is_main_window(HWND handle)
{
    return GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
}

BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam)
{
    handle_data& data = *(handle_data*)lParam;
    uint32_t process_id = 0;
    GetWindowThreadProcessId(handle, (LPDWORD) &process_id);
    if (data.process_id != process_id || !is_main_window(handle))
        return TRUE;
    data.window_handle = handle;
    return FALSE;
}

HWND find_main_window(uint32_t process_id)
{
    handle_data data;
    data.process_id = process_id;
    data.window_handle = 0;
    EnumWindows(enum_windows_callback, (LPARAM)&data);
    return data.window_handle;
}

namespace SPP
{
    bool CaptureApplicationWindow(uint32_t ProcessID, 
        int32_t& oWidth, int32_t& oHeight, 
        std::vector<uint8_t>& oImageData, uint8_t& oBytesPerPixel)
    {
        auto foundWindow = ProcessID ? find_main_window(ProcessID) : GetDesktopWindow();
        if (foundWindow)
        {
            HDC hdc = GetDC(foundWindow); // get the desktop device context
            HDC hDest = CreateCompatibleDC(hdc); // create a device context to use yourself

            int actualwidth = 0;
            int actualheight = 0;
            RECT rect;
            if (GetClientRect(foundWindow, &rect))
            {
                actualwidth = rect.right - rect.left;
                actualheight = rect.bottom - rect.top;

                // we hackily pad up here cause its easier than later on (this is required for NvEnc alignment)
                oWidth = ROUNDUP(actualwidth,16);
                oHeight = ROUNDUP(actualheight,16);
            }
            else
            {
                return false;
            }

            // create a bitmap
            HBITMAP appBitmap = CreateCompatibleBitmap(hdc, oWidth, oHeight);
            // use the previously created device context with the bitmap
            SelectObject(hDest, appBitmap);

            // copy from the desktop device context to the bitmap device context
            // call this once per 'frame'
            BitBlt(hDest, 0, 0, actualwidth, actualheight, hdc, 0, 0, SRCCOPY);

            oBytesPerPixel = 32;
            oImageData.resize(oWidth * oHeight * 4);

            {
                BITMAP bmp;
                if (GetObject(appBitmap, sizeof(BITMAP), &bmp)) // handle error
                {
                    BITMAPINFO info{ 0 };
                    info.bmiHeader.biSize = sizeof(info.bmiHeader);
                    info.bmiHeader.biWidth = bmp.bmWidth;
                    // pay attention to the sign, you most likely want a 
                    // top-down pixel array as it's easier to use
                    info.bmiHeader.biHeight = -bmp.bmHeight;
                    info.bmiHeader.biPlanes = 1;
                    info.bmiHeader.biBitCount = 32;
                    info.bmiHeader.biCompression = BI_RGB;

                    GetDIBits(hDest, appBitmap, 0, oHeight, oImageData.data(), &info, DIB_RGB_COLORS);
                }
            }

            // after the recording is done, release the desktop context you got..
            ReleaseDC(NULL, hdc);
            // ..delete the bitmap you were using to capture frames..
            DeleteObject(appBitmap);
            // ..and delete the context you created
            DeleteDC(hDest);

            return true;
        }
        return false;
    }
}