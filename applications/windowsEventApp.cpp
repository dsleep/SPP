// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include <windows.h>	
#include <GL/gl.h>			
//#include <GL/glu.h>	

#include "SPPCore.h"
#include "json/json.h"
#include "SPPLogging.h"
#include <set>

#include "SPPMemory.h"
#include "SPPApplication.h"
#include "SPPPlatformCore.h"

#include "SPPMath.h"

#include "SPPCapture.h"

using namespace SPP;

#pragma comment(lib, "opengl32.lib")

LogEntry LOG_APP("APP");

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
    GetWindowThreadProcessId(handle, (LPDWORD)&process_id);
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

class OpenGLApp
{
private:
    GLuint tex2D = 0;
    uint32_t ProcessID = 0;
    std::vector<uint8_t> ImageData;
    HWND CurrentLinkedApp = nullptr;
    Matrix3x3 virtualToReal;

public:
    OpenGLApp()
    {
       
    }

    void Update(HWND parentHwnd)
    {
        CurrentLinkedApp = find_main_window(ProcessID);

        int32_t VideoSizeX = 0;
        int32_t VideoSizeY = 0;

        uint8_t BytesPP = 0;
        if (CaptureApplicationWindow(ProcessID, VideoSizeX, VideoSizeY, ImageData, BytesPP) == false)
        {
            return;
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, tex2D);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, VideoSizeX, VideoSizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, ImageData.data());
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        RECT rect;
        GetClientRect(parentHwnd, &rect);

        int32_t WindowSizeX = rect.right - rect.left;
        int32_t WindowSizeY = rect.bottom - rect.top;

        float WindowAspectRatio = (float)WindowSizeX / (float)WindowSizeY;
        float VideoAspectRatio = (float)VideoSizeX / (float)VideoSizeY;
        int32_t VideoDrawWidth = 0;
        int32_t VideoDrawHeight = 0;

        // video wider aspect than window
        if (VideoAspectRatio <= WindowAspectRatio)
        {
            VideoDrawHeight = WindowSizeY;
            VideoDrawWidth = (int32_t)(VideoDrawHeight * VideoAspectRatio);
        }
        else
        {
            VideoDrawWidth = WindowSizeX;
            VideoDrawHeight = (int32_t)(VideoDrawWidth / VideoAspectRatio);
        }

        glViewport(0, 0, WindowSizeX, WindowSizeY);
        glClear(GL_COLOR_BUFFER_BIT);

        float ScaleX = (float)VideoSizeX / (float)VideoDrawWidth;
        float ScaleY = (float)VideoSizeY / (float)VideoDrawHeight;

        int32_t ShiftAmountX = (WindowSizeX - VideoDrawWidth) / 2;
        int32_t ShiftAmountY = (WindowSizeY - VideoDrawHeight) / 2;

        virtualToReal <<
            ScaleX, 0, 0,
            0, ScaleY, 0,
            (float)-ShiftAmountX * ScaleX, (float)-ShiftAmountY * ScaleY, 1.0f;

        glViewport(ShiftAmountX, ShiftAmountY, WindowSizeX, WindowSizeY);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, WindowSizeX, 0, WindowSizeY, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glBindTexture(GL_TEXTURE_2D, tex2D);
        glEnable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);

        glTexCoord2i(0, 0); glVertex2i(0, VideoDrawHeight);  //you should probably change these vertices.
        glTexCoord2i(0, 1); glVertex2i(0, 0);
        glTexCoord2i(1, 1); glVertex2i(VideoDrawWidth, 0);
        glTexCoord2i(1, 0); glVertex2i(VideoDrawWidth, VideoDrawHeight);

        glEnd();
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        glFlush(); //don't need this with GLUT_DOUBLE and glutSwapBuffers
    }


    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        const auto pApp = reinterpret_cast<OpenGLApp*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

        if (pApp)
        {
            return pApp->LocalWindowProc(hWnd, uMsg, wParam, lParam);
        }

        switch (uMsg)
        {
        case WM_CREATE:
        {
            // Save the DXSample* passed in to CreateWindow.
            LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
        }
        return 0;
        }

        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    void SetupOpenglGLAssets()
    {
        // Process Rest
        ProcessID = CreateChildProcess("C:/SurfLab/SOFA/bin/runSofa.exe", "");

        glGenTextures(1, &tex2D);
        glBindTexture(GL_TEXTURE_2D, tex2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        uint32_t ImageData = 0;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &ImageData);
        glBindTexture(GL_TEXTURE_2D, 0);

        glDisable(GL_DEPTH);
        glDisable(GL_LIGHTING);
        glDepthFunc(GL_ALWAYS);
    }

    HWND CreateOpenGLWindow(char* title, int x, int y, int width, int height, BYTE type, DWORD flags)
    {
        int         pf;
        HDC         hDC;
        HWND        hWnd;
        WNDCLASS    wc;
        PIXELFORMATDESCRIPTOR pfd;
        static HINSTANCE hInstance = 0;

        /* only register the window class once - use hInstance as a flag. */
        if (!hInstance)
        {
            hInstance = GetModuleHandle(NULL);
            wc.style = CS_OWNDC;
            wc.lpfnWndProc = (WNDPROC)WindowProc;
            wc.cbClsExtra = 0;
            wc.cbWndExtra = 0;
            wc.hInstance = hInstance;
            wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
            wc.hCursor = LoadCursor(NULL, IDC_ARROW);
            wc.hbrBackground = NULL;
            wc.lpszMenuName = NULL;
            wc.lpszClassName = L"RemoteWindow";

            if (!RegisterClass(&wc))
            {
                MessageBoxA(NULL, "RegisterClass() failed: Cannot register window class.", "Error", MB_OK);
                return NULL;
            }
        }

        hWnd = CreateWindowA("RemoteWindow", 
            title, 
            WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            x, y, width, height, NULL, NULL, hInstance, this);

        if (hWnd == NULL) {
            MessageBoxA(NULL, "CreateWindow() failed:  Cannot create a window.", "Error", MB_OK);
            return NULL;
        }

        hDC = GetDC(hWnd);

        /* there is no guarantee that the contents of the stack that become
           the pfd are zeroed, therefore _make sure_ to clear these bits. */
        memset(&pfd, 0, sizeof(pfd));
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | flags;
        pfd.iPixelType = type;
        pfd.cColorBits = 32;

        pf = ChoosePixelFormat(hDC, &pfd);
        if (pf == 0)
        {
            MessageBoxA(NULL, "ChoosePixelFormat() failed: Cannot find a suitable pixel format.", "Error", MB_OK);
            return 0;
        }
        if (SetPixelFormat(hDC, pf, &pfd) == FALSE)
        {
            MessageBoxA(NULL, "SetPixelFormat() failed: Cannot set format specified.", "Error", MB_OK);
            return 0;
        }

        DescribePixelFormat(hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);
        ReleaseDC(hWnd, hDC);

        return hWnd;
    }

    LRESULT LocalWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        static PAINTSTRUCT ps;

        if (uMsg >= WM_KEYDOWN && uMsg <= WM_KEYUP)
        {
            if (CurrentLinkedApp)
            {
                PostMessage(CurrentLinkedApp, uMsg, wParam, lParam);
            }

            //return SendMessage(message, wParam, lParam)
            SPP_LOG(LOG_APP, LOG_INFO, "%u : %u : %u", uMsg, wParam, lParam);

        }
        else if (uMsg >= WM_LBUTTONDOWN && uMsg <= WM_MOUSELAST)
        {
            uint16_t X = (uint16_t)(lParam & 0xFFFFF);
            uint16_t Y = (uint16_t)((lParam >> 16) & 0xFFFFF);

            Vector3 remap(X, Y, 1);
            Vector3 RemappedPosition = remap * virtualToReal;

            if (RemappedPosition[0] >= 0 && RemappedPosition[1] >= 0)
            {              
                lParam = (lParam & ~LPARAM(0xFFFFFFFF)) | (uint16_t)RemappedPosition[0] | LPARAM((uint16_t)(RemappedPosition[1])) << 16;

                if (CurrentLinkedApp)
                {
                    PostMessage(CurrentLinkedApp, uMsg, wParam, lParam);
                }
            }            
                        
            //SendMessage 
            SPP_LOG(LOG_APP, LOG_INFO, "%u : %u : %u", uMsg, wParam, lParam);
        }

        switch (uMsg)
        {
        case WM_PAINT:
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            return 0;

        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
};


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    IntializeCore(nullptr);

    HDC hDC;				/* device context */
    HGLRC hRC;				/* opengl context */
    HWND  hWnd;				/* window */

    OpenGLApp curApp;

    hWnd = curApp.CreateOpenGLWindow("minimal", 0, 0, 1280, 720, PFD_TYPE_RGBA, 0);
    if (hWnd == NULL)
    {
        exit(1);
    }

    hDC = GetDC(hWnd);
    hRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hRC);

    curApp.SetupOpenglGLAssets();

    MSG msg = { 0 };
    {
        ShowWindow(hWnd, nCmdShow);

        while (true)
        {
            if (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
            {
                if (WM_QUIT == msg.message)
                {
                    break;
                }
                else
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
           
            curApp.Update(hWnd);
        }
    }

    wglMakeCurrent(NULL, NULL);
    ReleaseDC(hWnd, hDC);
    wglDeleteContext(hRC);
    DestroyWindow(hWnd);

    return static_cast<char>(msg.wParam);
}
