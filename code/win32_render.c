/*
TODO
    next chapter - shadows and reflections
*/

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <math.h>
#include <float.h>

//Current assumption is that the viewport is a square and distance between origin and viewport is 1, this avoids weird FOV issues
//TODO in future should be able to set any aspect ratio & fov, and then calculate d from that, note that this will affect some of the calculations below that will need to be updated
#define BUFFER_WIDTH 1024
#define BUFFER_HEIGHT 1024
#define HALF_BUFFER_HEIGHT 512
#define HALF_BUFFER_WIDTH 512
#define BYTES_PER_PIXEL 4

#define PROJ_PLANE_D 1

#define VIEWPORT_SIZE 1
#define MAX_REFLECTION_ITERATIONS 3

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

typedef struct v3 {
    f32 x, y, z;
} V3;

global V3 camera_pos = {0, 0, 0};

typedef struct v2 {
    f32 x, y;
} V2;

V3 v3_add(V3 a, V3 b) {
    V3 res = {a.x + b.x, a.y + b.y, a.z + b.z};
    return res;
}

V3 v3_sub(V3 a, V3 b) {
    V3 res = {a.x - b.x, a.y - b.y, a.z - b.z};
    return res;
}

V3 v3_mult(V3 a, V3 b) {
    V3 res = {a.x * b.x, a.y * b.y, a.z * b.z};
    return res;
}

V3 v3_div(V3 a, f32 scalar) {
    V3 res = {a.x / scalar, a.y / scalar, a.z / scalar};
    return res;
}

V3 v3_scalar(V3 a, f32 scalar) {
    V3 res = {a.x * scalar, a.y * scalar, a.z * scalar};
    return res;
}

f32 v3_dot(V3 v1, V3 v2) {
    f32 res = {v1.x * v2.x + v1.y * v2.y + v1.z * v2.z};
    return res;
}

f32 v3_length(V3 v) {
    f32 res = sqrt(v3_dot(v, v));
    return res;
}

V3 v3_normalize(V3 v) {
    f32 length = v3_length(v);    
    return v3_div(v, length);
}

typedef struct color {
    u8 r, g, b;
} Color;

global Color background_color = {0, 0, 0};

f32 clamp(f32 val, f32 min, f32 max) {
    if(val < min)
        val = min;
    if(val > max)
        val = max;
    return val;
}

Color color_scalar(Color color, f32 scalar) {
    Color res = {color.r * scalar, color.g * scalar, color.b * scalar};
    return res;
}

Color color_add(Color a, Color b) {
    Color res = {a.r + b.r, a.g + b.g, a.b + b.b};
    return res;
}

Color color_clamp(Color color) {
    Color res = {clamp(color.r, 0, 255), clamp(color.g, 0, 255), clamp(color.b, 0, 255)};
    return res;
}

b32 definitelyGreaterThan(float a, float b, float epsilon)
{
    return (a - b) > ( (fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

typedef struct sphere {
    f32 radius;
    V3 center;
    Color color;
    f32 specular;
    f32 reflectance;
} Sphere;

global Sphere spheres[] = {
    {1, {0, -1, 3}, {255, 0, 0}, 500, 0.2},
    {1, {2, 0, 4}, {0, 0, 255}, 500, 0.3},
    {1, {-2, 0, 4}, {0, 255, 0}, 10, 0.4},
    {5000, {0, -5001, 0}, {255, 255, 0}, 1000, 0.5}
};

typedef struct light {
    union {
        V3 position;
        V3 direction;
    };
    Color color;
    f32 intensity;
} Light;

//leaving light color at 0 for now
global Light g_point_lights[] = {
    {{2, 1, 0}, {0}, 0.6}
};

global Light g_directional_lights[] = {
    {{1, 4, 4}, {0}, 0.2}
};

global Light g_ambient = {{0}, {0}, 0.2};

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

typedef struct intersection_result {
    Sphere *closest_sphere;
    f32 closest_t;
} Intersection_Result;

internal Intersection_Result find_sphere_intersection_for_ray(V3 ray_dir, V3 origin, f32 t_min, f32 t_max) {
    Intersection_Result result = {0};
    Sphere *closest_sphere = null;
    f32 closest_t = t_max;
    
    u32 array_length = ARRAY_COUNT(spheres);
    Sphere *current_sphere;
    
    //book we're following is using Javascript which defaults to 64-bit numbers so does not suffer as badly from floating-point precision, we need to use a slightly different form of the quadratic formula solver
    for(u32 i = 0; i < array_length; i++) {
        current_sphere = spheres + i;
        
        f32 r = current_sphere->radius;
        V3 oc = v3_sub(origin, current_sphere->center);
        f32 a = v3_dot(ray_dir, ray_dir);
        f32 b = 2 * v3_dot(oc, ray_dir);
        f32 c = v3_dot(oc, oc) - (r * r);
        f32 discriminant = (b * b) - (4 * a * c); //part of the quadratic equation roots formula
        
        if (discriminant >= 0) {
            f32 t1 = ((-b) + sqrt(discriminant)) / (2 * a);
            f32 t2 = ((-b) - sqrt(discriminant)) / (2 * a);
            
            if(t1 > t_min  && t1 <= t_max && t1 < closest_t) {
                closest_t = t1;
                closest_sphere = current_sphere;
            }
            
            if(t2 > t_min && t2 <= t_max && t2 < closest_t) {
                closest_t = t2;
                closest_sphere = current_sphere;
            }
        } 
    }
    
    result.closest_sphere = closest_sphere;
    result.closest_t = closest_t;
    return result;
}

internal Color trace_ray(V3 dir, V3 origin, f32 min_dist, f32 max_dist, u32 iteration) {
    Color final_color = background_color;
    
    Intersection_Result intersection_result = find_sphere_intersection_for_ray(dir, origin, min_dist, max_dist);
    Sphere *closest_sphere = intersection_result.closest_sphere;
    f32 closest_t = intersection_result.closest_t;
    //TODO so far we aren't using the color of the light to do anything....
    if(intersection_result.closest_sphere) {
        //if we have a point on the sphere, lets first calculate it's 
        V3 p = v3_add(origin, v3_scalar(dir, closest_t));
        V3 v = {-dir.x, -dir.y, -dir.z};
        f32 v_len = v3_length(v);
        
        //orgin is 0 so don't need the O in the parametric line formula
        V3 normal = v3_normalize(v3_sub(p, closest_sphere->center)); 
        f32 length_n = 1.0f;
        V3 fixed_p = v3_add(p, v3_scalar(normal, 0.001));
        
        f32 light_intensity_sum = g_ambient.intensity;
        //foreach point light...
        for(u32 i = 0; i < ARRAY_COUNT(g_point_lights); i++) {
            Light point_light = g_point_lights[i];
            
            //diffuse...
            
            //calculate current direction to point light
            V3 l = v3_sub(point_light.position, p);
            f32 length_l = v3_length(l);
            f32 dot = v3_dot(normal, l);
            
            V3 r_vec = v3_sub(v3_scalar(normal, 2 * dot), l);
            f32 r_len = v3_length(r_vec);
            
            //l is already the dir to the light, not the light to the point
            Intersection_Result shadow_result = find_sphere_intersection_for_ray(l, fixed_p, 0.001, 1.0f); //t_max is 1, what we really need is to add attenuation to our point lights, t_min needs to be epsilon-some undefined small number s.t. p itself doesn't count as a hit
            
            if(shadow_result.closest_sphere)
                continue;
            
            //diffuse
            if(dot > 0) {
                light_intensity_sum += (point_light.intensity * dot) / (length_n * length_l);
            }
            
            //specular
            f32 r_dot_v = v3_dot(r_vec, v);
            
            if(r_dot_v > 0) {
                light_intensity_sum += point_light.intensity * pow(r_dot_v / (r_len * v_len), closest_sphere->specular);
            }
        }
        
        for(u32 i = 0; i < ARRAY_COUNT(g_directional_lights); i++) {
            Light directional_light = g_directional_lights[i];
            
            V3 l = directional_light.direction;
            f32 length_l = v3_length(l);
            f32 dot = v3_dot(normal, l);
            
            V3 r_vec = v3_sub(v3_scalar(normal, 2 * dot), l);
            f32 r_len = v3_length(r_vec);
            
            Sphere *shadowing_sphere = null;
            f32 shadow_t = FLT_MAX;
            //l is already the dir to the light, not the light to the point
            Intersection_Result shadow_result = find_sphere_intersection_for_ray(l, fixed_p, 0.001, FLT_MAX);
            
            if(shadow_result.closest_sphere)
                continue;
            
            //see if this can be done without an if...
            if(dot > 0) {
                //think it has to be done in this order, as assuming 1/{something} operations with floats causes imprecision, review that, something to do with associativity/commutavity
                light_intensity_sum += (directional_light.intensity * dot) / (length_n * length_l);
            }
            
            //specular
            f32 r_dot_v = v3_dot(r_vec, v);
            if(r_dot_v > 0) {
                light_intensity_sum += directional_light.intensity * pow(r_dot_v / (r_len * v_len), closest_sphere->specular);
            }
        }
        
        if(light_intensity_sum > 1.0f)
            light_intensity_sum = 1.0f;
        //TODO essentially we are only doing screen space reflections here(mostly) we are missing a lot of info, we need to rerun the entire ray-tracing algorithm for accurate reflections
        //we need to make this entire thing recursive, with a depth value that gets decremented each run
        f32 n_dot_v = v3_dot(normal, v);
        V3 view_reflection_vector = v3_sub(v3_scalar(normal, 2 * n_dot_v), v);
        Color local_color = color_scalar(closest_sphere->color, light_intensity_sum);
        //TODO add a flag for enabling this at build time
        // // //debug normal view 
        // local_color.r = (normal.x * 0.5f + 0.5f) * 255;
        // local_color.g = (normal.y * 0.5f + 0.5f) * 255;
        // local_color.b = (normal.z * 0.5f + 0.5f) * 255;
        
        if(iteration <= 0 || closest_sphere->reflectance <= 0) {
            return local_color;
        }
        
        Color reflected_color = trace_ray(view_reflection_vector, fixed_p, 0.001, FLT_MAX, iteration - 1);
        
        final_color = color_add(color_scalar(local_color, (1 - closest_sphere->reflectance)), color_scalar(reflected_color, closest_sphere->reflectance));
        //foreach directional light...whenever I make a game we can probably always assume this to be 1? Why would you have more than one point light?
        
    }
    
    return final_color;
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
                //aka a point on the ray, the camera origin is another
                V3 viewport_coords = {((f32)canvas_x * VIEWPORT_SIZE / (f32)g_back_buffer.size.width), ((f32)canvas_y * VIEWPORT_SIZE / (f32)g_back_buffer.size.height), PROJ_PLANE_D}; //viewport width/height is 1 so let's ignore it for simplicity sake, z axis is just the viewports distance from the camera
                
                col_at_pixel = trace_ray(viewport_coords, camera_pos, 1, FLT_MAX, MAX_REFLECTION_ITERATIONS);
                color_clamp(col_at_pixel);
            }
            
            u8 red = (u8)col_at_pixel.r;
            u8 green = (u8)col_at_pixel.g;
            u8 blue = (u8)col_at_pixel.b;
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
                                    CW_USEDEFAULT, CW_USEDEFAULT, BUFFER_WIDTH, BUFFER_HEIGHT,
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
