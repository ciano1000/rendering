#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include "utils.h"
#include <stdio.h>
// TODO(Cian): @Strings get rid of this with our own immutable string implementation
#include <string.h>
#include <Windows.h>

#define BUFFER_WIDTH 1280
#define BUFFER_HEIGHT 720
#define BYTES_PER_PIXEL 4

typedef struct win32_buffer{
    BITMAPINFO info;
    void *memory;
    u32 width;
    u32 height;
    u32 stride;
} Win32_Buffer;

typedef struct win32_dimension {
    u32 width;
    u32 height;
} Win32_Dimension;

global Win32_Buffer g_back_buffer;
global b32 g_running;
HBITMAP g_bitmap = null;

internal Win32_Dimension win32_get_window_dimension(HWND window) {
    Win32_Dimension result = {0};
    
    RECT win_rect;
    GetWindowRect(window, &win_rect);
    result.width = win_rect.right - win_rect.left;
    result.height = win_rect.bottom - win_rect.top;
    
    return result;
}

internal LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    LRESULT result = null;
    
    switch(msg) {
        case WM_CLOSE: {
            g_running = false;
        } break;
        case WM_DESTROY: {
            g_running = false;
        } break;
        case WM_PAINT: {
            //TODO blit to screen 
            PAINTSTRUCT paint_s;
            HDC device_ctx = BeginPaint(hwnd, &paint_s);
            Win32_Dimension win_dimension = win32_get_window_dimension(hwnd);
            StretchDIBits(device_ctx, 0, 0, win_dimension.width, win_dimension.height, 0, 0, 1280, 720, g_back_buffer.memory, &g_back_buffer.info, DIB_RGB_COLORS, SRCCOPY);
            EndPaint(hwnd, &paint_s);
            
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

            //since we set our class style to be "CS_OWNDC" we don't need to release this as we don't share it with any other window.
            HDC device_ctx = GetDC(hwnd);
              
            g_back_buffer.width = BUFFER_WIDTH;
            g_back_buffer.height = BUFFER_HEIGHT;
            g_back_buffer.stride = g_back_buffer.width * BYTES_PER_PIXEL;    
            //might need to be global
            g_back_buffer.memory = VirtualAlloc(null, (g_back_buffer.width * g_back_buffer.height * BYTES_PER_PIXEL), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            
            BITMAPINFO bitmap_info = {0};
            
            bitmap_info.bmiHeader.biSize        = sizeof(bitmap_info.bmiHeader); //windows needs this to use as an offset to retrieve the location of the bitmapinfo's color table as different versions of the bmiheader struct with different sizes may exist for different api versions plus some other reasons etc..
            bitmap_info.bmiHeader.biWidth       = 1280; //TODO(Cian): @Bitmap set resolution in a smarter way
            bitmap_info.bmiHeader.biHeight      = -720;//set to negative so that the origin is in the top left as described by msdn, if negative biCompression must be BI_RGB or BI_BITFIELDS, cannot be compressed if top down.
            bitmap_info.bmiHeader.biPlanes      = 1; //must be set to 1 apparently, probably some legacy thing
            bitmap_info.bmiHeader.biBitCount    = 32; //8 bits per channel plus another 8 for alignment.
            bitmap_info.bmiHeader.biCompression = BI_RGB;
            
            g_back_buffer.info = bitmap_info;
            
            CreateDIBSection(device_ctx, &g_back_buffer.info, DIB_RGB_COLORS, &g_back_buffer.memory, null, null); //must DeleteObject when ready with this.
            u8 x_offset = 0;
            u8 y_offset = 0;
            g_running = true;
            while(g_running) {
                u32 stride = 4 * 1280;
                u8 *row = g_back_buffer.memory;
                for(u32 y = 0; y < 720; y++) {
                    u32 *pixel = (u32*)row;
                    for(u32 x = 0; x < 1280; x++) {
                        u8 red = (x + x_offset);
                        u8 blue = (y + y_offset);
                        u8 green = 0;
                        u8 padding = 0;
                        //Memory Layout (bytes): BB GG RR XX
                        //Since this is Little Endian - LSB first
                        //When we take all these bytes together as a 32 bit int the bits are actually in the following order (right to left)
                        // XX RR GG BB - therefore, green is shifted left 8 bits, red left 16 bits, blue does not need to be shifted.
                        *pixel++ = (padding << 24) | (red << 16) | (green << 8) | (blue);
                    }
                    
                    row += stride;
                }
                x_offset++;
                y_offset++;
                
                Win32_Dimension win_dimension = win32_get_window_dimension(hwnd);
                StretchDIBits(device_ctx, 0, 0, win_dimension.width, win_dimension.height, 0, 0, g_back_buffer.width, g_back_buffer.height, g_back_buffer.memory, &g_back_buffer.info, DIB_RGB_COLORS, SRCCOPY);
                
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
