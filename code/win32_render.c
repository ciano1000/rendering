#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include "utils.h"
#include <stdio.h>
// TODO(Cian): @Strings get rid of this with our own immutable string implementation
#include <string.h>
#include <Windows.h>

internal LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    LRESULT result = null;
    
    switch(msg) {
        default: {
            result = DefWindowProcA(hwnd, msg, w_param, l_param);
        }
    }
    
    return result;
}

global b32 g_running = true;
s32 WinMain(HINSTANCE h_instance, HINSTANCE prev_instance, LPSTR cmd, int cmd_show) {
    // TODO(CMS): setup build to be as simple as possible, just get a window showing
    char *class_name = "Render"; 
    
    WNDCLASSA wc = {0};
    
    wc.style         = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    wc.lpfnWndProc   = window_proc;
    wc.hInstance     = h_instance;
    wc.lpszClassName = class_name;
    
    RegisterClassA(&wc);
    
    HWND hwnd = CreateWindowExA(
                                null,
                                class_name,
                                "RenderMcRenderFace",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                null,
                                null,
                                h_instance,
                                null);
    
    if(hwnd == null) {
        return 0;
    }
    
    ShowWindow(hwnd, cmd_show);
    
    for(;;) {
        
        MSG message;
        BOOL msg_result = GetMessage(&message, null, null, null);
        
        if(msg_result > 0) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        } else {
            break;
        }
    }
    
    
    
    return 0;
}