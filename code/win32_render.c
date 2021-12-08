#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include "utils.h"
#include <stdio.h>
// TODO(Cian): @Strings get rid of this with our own immutable string implementation
#include <string.h>
#include <Windows.h>

global b32 g_running;
HBITMAP g_bitmap = null;

internal LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    LRESULT result = null;
    
    switch(msg) {
        case WM_CLOSE: {
            g_running = false;
        } break;
        case WM_DESTROY: {
            g_running = false;
        } break;
        default: {
            result = DefWindowProcA(hwnd, msg, w_param, l_param);
        } break;
    }
    
    return result;
}


int WinMain(HINSTANCE h_instance, HINSTANCE prev_instance, LPSTR cmd, int cmd_show) {
    // TODO(CMS): setup build to be as simple as possible, just get a window showing
    char *class_name = "Render"; 
    
    WNDCLASSA wc = {0};
    
    wc.style         = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    wc.lpfnWndProc   = window_proc;
    wc.hInstance     = h_instance;
    wc.lpszClassName = class_name;
    
    if(RegisterClassA(&wc)) {
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
        
        if(hwnd) {
            ShowWindow(hwnd, cmd_show);
       
            //TODO: move this up into the WM_RESIZE message and only set it there
            RECT win_rect;
            GetWindowRect(hwnd, &win_rect);
            u32 win_width = win_rect.right - win_rect.left;
            u32 win_height = win_rect.bottom - win_rect.top;
            
            //since we set our class style to be "CS_OWNDC" we don't need to release this as we don't share it with any other window.
            HDC device_ctx = GetDC(hwnd);
            
            //might need to be global
            void *bitmap_memory = VirtualAlloc(null, (1280 * 720 * 4), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            
            BITMAPINFO bitmap_info = {0};
            
            bitmap_info.bmiHeader.biSize        = sizeof(bitmap_info.bmiHeader); //windows needs this to use as an offset to retrieve the location of the bitmapinfo's color table as different versions of the bmiheader struct with different sizes may exist for different api versions plus some other reasons etc..
            bitmap_info.bmiHeader.biWidth       = 1280; //TODO(Cian): @Bitmap set resolution in a smarter way
            bitmap_info.bmiHeader.biHeight      = -720;//set to negative so that the origin is in the top left as described by msdn, if negative biCompression must be BI_RGB or BI_BITFIELDS, cannot be compressed if top down.
            bitmap_info.bmiHeader.biPlanes      = 1; //must be set to 1 apparently, probably some legacy thing
            bitmap_info.bmiHeader.biBitCount    = 32; //8 bits per channel plus another 8 for alignment.
            bitmap_info.bmiHeader.biCompression = BI_RGB;
            
            g_bitmap = CreateDIBSection(device_ctx, &bitmap_info, DIB_RGB_COLORS, &bitmap_memory, null, null); //must DeleteObject when ready with this.
            
            g_running = true;
            while(g_running) {
                u32 stride = 4 * 1280;
                u8 *row = bitmap_memory;
                for(u32 y = 0; y < 720; y++) {
                    u8 *pixel = row;
                    for(u32 x = 0; x < 1280; x++) {
                        *pixel = x;
                        pixel++;
                        
                        *pixel = y;
                        pixel++;
                        
                        *pixel = 0;
                        pixel++;
                        
                        *pixel = 0;
                        pixel++;
                    }
                    
                    row += stride;
                }
                
                StretchDIBits(device_ctx, 0, 0, win_width, win_height, 0, 0, 1280, 720, bitmap_memory, &bitmap_info, DIB_RGB_COLORS, SRCCOPY);
                
                // TODO(Cian): handle all win messages here and send them to a seperate thread to prevent blocking
                MSG message;
                while(PeekMessage(&message, null, null, null, PM_REMOVE)) {
                    if(message.message == WM_QUIT) {
                        g_running = false;
                    }
                    
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }
            }
            
        } else {
            // TODO(Cian): logging
        }
    } else {
        // TODO(Cian): logging
    } 
    
    return 0;
}
