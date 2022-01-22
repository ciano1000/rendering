#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <math.h>

#define MAX_DISTANCE 1000.0f //arbitrary max tracing distance

//Current assumption is that the viewport is a square, this avoids weird FOV issues
#define BUFFER_WIDTH 1024
#define BUFFER_HEIGHT 1024
#define HALF_BUFFER_HEIGHT 512
#define HALF_BUFFER_WIDTH 512
#define BYTES_PER_PIXEL 4

#define PROJ_PLANE_D 1

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))
#define DOT(a, b) ((a.x * b.x) + (a.y * b.y) + (a.z * b.z))

typedef struct v3 {
    f32 x, y, z;
} V3;

typedef struct v2 {
    f32 x, y;
} V2;

typedef struct color {
    u8 r, g, b;
} Color;

global Color background_color = {40, 150, 40};

typedef struct sphere {
    f32 radius;
    V3 center;
    Color color;
} Sphere;

global Sphere spheres[] = {
    {1, {0, -1, 3}, {255, 0, 0}},
    {1, {2, 0, 4}, {0, 0, 255}},
    {1, {-2, 0, 4}, {0, 255, 0}}
};

typedef struct win32_dimension {
    s32 width;
    s32 height;
} Win32_Dimension;

typedef struct win32_buffer{
    BITMAPINFO info;
    void *memory;
    Win32_Dimension size;
    s32 stride;
} Win32_Buffer;

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

internal void win32_primitive_sphere_raytracing() {
    u32 stride = 4 * g_back_buffer.size.width;
    u8 *row = g_back_buffer.memory;
    for(u32 y = 0; y < g_back_buffer.size.height; y++) {
        u32 *pixel = (u32*)row;
        for(u32 x = 0; x < g_back_buffer.size.width; x++) {
        
            Color col_at_pixel = {0};
            {
                f32 canvas_x = (f32)x - (g_back_buffer.size.width / 2);
                f32 canvas_y = -((f32)y - (g_back_buffer.size.height / 2));
                V3 viewport_coords = {((f32)canvas_x / (f32)g_back_buffer.size.width), ((f32)canvas_y / (f32)g_back_buffer.size.height), PROJ_PLANE_D}; //viewport width/height is 1 so let's ignore it for simplicity sake, z axis is just the viewports distance from the camera
            
                u32 array_length = ARRAY_COUNT(spheres);
                Sphere *closest_sphere = null;
                f32 closest_t = MAX_DISTANCE;
                Sphere *current_sphere;
                for(u32 i = 0; i < array_length; i++) {
                    current_sphere = spheres + i;
                    
                    f32 r = current_sphere->radius;
                    V3 CO = {-current_sphere->center.x, -current_sphere->center.y, -current_sphere->center.z}; // equals Origin - Center, origin is all 0's
                    f32 a = DOT(viewport_coords, viewport_coords);
                    f32 b = 2 * DOT(CO, viewport_coords);
                    f32 c = DOT(CO, CO) - (r * r);
                    f32 discriminant = b * b - 4 * a * c; //part of the quadratic equation roots formula
                    
                    if (discriminant >= 0) {
                        f32 t1 = (-b + sqrt(discriminant)) / 2 * a;
                        f32 t2 = (-b - sqrt(discriminant)) / 2 * a;
                        
                        if(t1 > 1 && t1 <= MAX_DISTANCE && t1 < closest_t) {
                            closest_t = t1;
                            closest_sphere = current_sphere;
                        }
                        
                        if(t2 > 1 && t2 <= MAX_DISTANCE && t2 < closest_t) {
                            closest_t = t2;
                            closest_sphere = current_sphere;
                        }
                    } 
                }
                
                if(closest_sphere != null) {
                    col_at_pixel = closest_sphere->color;
                } else {
                    col_at_pixel = background_color;
                }
            }
            
            u8 red = col_at_pixel.r;
            u8 blue = col_at_pixel.g;
            u8 green = col_at_pixel.b;
            u8 padding = 0;
            //Memory Layout (bytes): BB GG RR XX
            //Since this is Little Endian - LSB first
            //When we take all these bytes together as a 32 bit int the bits are actually in the following order (right to left)
            // XX RR GG BB - therefore, green is shifted left 8 bits, red left 16 bits, blue does not need to be shifted.
            *pixel++ = (padding << 24) | (red << 16) | (green << 8) | (blue);
        }
        
        row += stride;
    }
}

internal void win32_render_weird_gradient(u8 x_offset, u8 y_offset) {
    u32 stride = 4 * BUFFER_WIDTH;
    u8 *row = g_back_buffer.memory;
    for(u32 y = 0; y < BUFFER_HEIGHT; y++) {
        u32 *pixel = (u32*)row;
        for(u32 x = 0; x < BUFFER_WIDTH; x++) {
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
            PAINTSTRUCT paint_s;
            HDC device_ctx = BeginPaint(hwnd, &paint_s);
            Win32_Dimension win_dimension = win32_get_window_dimension(hwnd);
            StretchDIBits(device_ctx, 0, 0, g_back_buffer.size.width, g_back_buffer.size.height, 0, 0, g_back_buffer.size.width, g_back_buffer.size.height, g_back_buffer.memory, &g_back_buffer.info, DIB_RGB_COLORS, SRCCOPY);
            EndPaint(hwnd, &paint_s);
            
        } break;
        default: {
            result = DefWindowProcA(hwnd, msg, w_param, l_param);
        } break;
    }
    
    return result;
}


int WinMain(HINSTANCE h_instance, HINSTANCE prev_instance, LPSTR cmd, int cmd_show) {
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
              
            g_back_buffer.size.width = BUFFER_WIDTH;
            g_back_buffer.size.height = BUFFER_HEIGHT;
            g_back_buffer.stride = g_back_buffer.size.width * BYTES_PER_PIXEL;
            g_back_buffer.memory = VirtualAlloc(0, (g_back_buffer.size.width * g_back_buffer.size.height * BYTES_PER_PIXEL), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            
            BITMAPINFO bitmap_info = {0};
            
            bitmap_info.bmiHeader.biSize        = sizeof(bitmap_info.bmiHeader); //windows needs this to use as an offset to retrieve the location of the bitmapinfo's color table as different versions of the bmiheader struct with different sizes may exist for different api versions plus some other reasons etc..
            bitmap_info.bmiHeader.biWidth        = g_back_buffer.size.width; 
            bitmap_info.bmiHeader.biHeight        = -g_back_buffer.size.height; //set to negative so that the origin is in the top left as described by msdn, if negative biCompression must be BI_RGB or BI_BITFIELDS, cannot be compressed if top down.
            bitmap_info.bmiHeader.biPlanes      = 1; //must be set to 1 apparently, probably some legacy thing
            bitmap_info.bmiHeader.biBitCount    = 32; //8 bits per channel plus another 8 for alignment.
            bitmap_info.bmiHeader.biCompression = BI_RGB;
            
            g_back_buffer.info = bitmap_info;
            
            u8 x_offset = 0;
            u8 y_offset = 0;
            
            LARGE_INTEGER perf_counter_frequency_result;
            QueryPerformanceFrequency(&perf_counter_frequency_result);
            s64 perf_counter_frequency = perf_counter_frequency_result.QuadPart;
            
            //Set scheduler granularity and check if succesful
            b32 is_granular_sleep = (timeBeginPeriod(1) == TIMERR_NOERROR);
            
            g_running = true;
            
            u32 refresh_rate = 60;
            f32 target_seconds_per_frame = 1.0f / refresh_rate;
             
            LARGE_INTEGER start_counter;
            QueryPerformanceCounter(&start_counter);
            
            u64 start_cycle_count = __rdtsc();
            while(g_running) {
                //win32_render_weird_gradient(x_offset, y_offset);
                win32_primitive_sphere_raytracing();
                x_offset++;
                y_offset++;
                
                Win32_Dimension win_dimension = win32_get_window_dimension(hwnd);
                StretchDIBits(device_ctx, 0, 0, g_back_buffer.size.width, g_back_buffer.size.height, 0, 0, g_back_buffer.size.width, g_back_buffer.size.height, g_back_buffer.memory, &g_back_buffer.info, DIB_RGB_COLORS, SRCCOPY);
                
                MSG message;
                while(PeekMessage(&message, null, null, null, PM_REMOVE)) {
                    if(message.message == WM_QUIT) {
                        g_running = false;
                    }
                    
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }
                
                //TODO Cian: pull out this into functions for getting Clock time and elapsed seconds etc.
                LARGE_INTEGER end_counter;
                QueryPerformanceCounter(&end_counter);
                s64 counter_elapsed = end_counter.QuadPart - start_counter.QuadPart;
                f32 actual_seconds_for_frame = (f32)counter_elapsed / (f32)perf_counter_frequency;
#if 0 //TODO Cian: pull this stuff into proper debug timing helpers later
                s32 time_elapsed_ms = (s32)((counter_elapsed * 1000) / perf_counter_frequency); 
                s32 frames_per_second = perf_counter_frequency / counter_elapsed;
                char buffer[256];
                wsprintfA(buffer, "ms/frame: %dms, fps: %dfps, %dcycles per frame\n", time_elapsed_ms, frames_per_second, elapsed_cycles);
                OutputDebugStringA(buffer);
#endif               

                //TODO Cian: check opposite case where we miss our framerate
                while(actual_seconds_for_frame < target_seconds_per_frame) {
                    if(is_granular_sleep) {
                        DWORD ms_to_sleep = (DWORD)((target_seconds_per_frame - actual_seconds_for_frame) * 1000.0f);
                        if(ms_to_sleep > 0) {
                            Sleep(ms_to_sleep);
                        }
                    }
                    
                    QueryPerformanceCounter(&end_counter);
                    counter_elapsed = end_counter.QuadPart - start_counter.QuadPart;
                    actual_seconds_for_frame = (f32)counter_elapsed / (f32)perf_counter_frequency;
                }
                start_counter = end_counter;
                
                u64 end_cycle_count = __rdtsc();
                u64 elapsed_cycles  = end_cycle_count - start_cycle_count;
                start_cycle_count = end_cycle_count;
            }
        } else {
        }
    } else {
    } 
    
    
    timeEndPeriod(1);
    return 0;
}
