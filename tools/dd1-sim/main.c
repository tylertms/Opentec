#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define D2D_USE_C_DEFINITIONS
#define INITGUID
#if defined(_WIN32)
#include <windows.h>
#endif

#include <commdlg.h>
#include <d2d1.h>
#include <dwrite.h>
#include <math.h>
#include <process.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wincodec.h>
#include <windowsx.h>

#include "firmware.h"
#include "machine.h"

enum {
    CLIENT_WIDTH = 1060,
    CLIENT_HEIGHT = 620,
    DISPLAY_X = 24,
    DISPLAY_Y = 42,
    DISPLAY_SCALE = 2,
    WHEEL_X = 145,
    WHEEL_Y = 420,
    WHEEL_RADIUS = 95,
    BUTTON_X = 285,
    BUTTON_Y = 280,
    BUTTON_WIDTH = 92,
    BUTTON_HEIGHT = 34,
    BUTTON_COLUMNS = 4,
    BUTTON_ROWS = 6,
    BUTTON_GAP = 8,
    FORCE_X = 760,
    FORCE_Y = 280,
    FORCE_WIDTH = 220,
    FORCE_HEIGHT = 220,
    TIMER_ID = 1,
    TIMER_PERIOD_MS = 16,
    WORKER_ROUNDS = 4096,
    ID_BUTTON_FIRST = 100,
    ID_PAUSE = 200,
    ID_LEFT = 201,
    ID_RIGHT = 202,
};

typedef struct {
    Dd1SimMachine *machine;
    Dd1SimSnapshot snapshot;
    CRITICAL_SECTION lock;
    HANDLE worker;
    HWND button_controls[24];
    HWND pause_control;
    HWND status_control;
    HBRUSH background_brush;
    ID2D1Factory *d2d_factory;
    ID2D1HwndRenderTarget *render_target;
    ID2D1SolidColorBrush *brush;
    ID2D1Bitmap *display_bitmap;
    ID2D1Bitmap *wheel_bitmap;
    IWICImagingFactory *wic_factory;
    IDWriteFactory *write_factory;
    IDWriteTextFormat *ui_format;
    IDWriteTextFormat *telemetry_format;
    LONG stop;
    LONG paused;
    int16_t angle_tenths;
    uint32_t buttons;
    UINT dpi;
    uint8_t displayed_frame[DD1_SIM_DISPLAY_SIZE];
    bool displayed_frame_valid;
    bool dragging;
} AppState;

static int scaled(const AppState *state, int value) { return MulDiv(value, (int)state->dpi, 96); }

static D2D1_COLOR_F color(float red, float green, float blue) {
    return (D2D1_COLOR_F){red / 255.0f, green / 255.0f, blue / 255.0f, 1.0f};
}

static void release_render_target(AppState *state) {
    if (state->display_bitmap != NULL) {
        ID2D1Bitmap_Release(state->display_bitmap);
        state->display_bitmap = NULL;
    }
    if (state->wheel_bitmap != NULL) {
        ID2D1Bitmap_Release(state->wheel_bitmap);
        state->wheel_bitmap = NULL;
    }
    if (state->brush != NULL) {
        ID2D1SolidColorBrush_Release(state->brush);
        state->brush = NULL;
    }
    if (state->render_target != NULL) {
        ID2D1HwndRenderTarget_Release(state->render_target);
        state->render_target = NULL;
    }
}

static bool wheel_asset_path(wchar_t *path, size_t path_size) {
    DWORD length = GetModuleFileNameW(NULL, path, (DWORD)path_size);
    if (length == 0 || length >= path_size) {
        return false;
    }
    wchar_t *separator = wcsrchr(path, L'\\');
    if (separator == NULL) {
        return false;
    }
    const wchar_t file_name[] = L"formula-v25.png";
    size_t prefix_length = (size_t)(separator - path) + 1;
    if (prefix_length + sizeof(file_name) / sizeof(file_name[0]) > path_size) {
        return false;
    }
    memcpy(path + prefix_length, file_name, sizeof(file_name));
    return true;
}

static bool load_wheel_bitmap(AppState *state) {
    wchar_t path[MAX_PATH];
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *converter = NULL;
    HRESULT result = wheel_asset_path(path, sizeof(path) / sizeof(path[0]))
                         ? IWICImagingFactory_CreateDecoderFromFilename(
                               state->wic_factory, path, NULL, GENERIC_READ,
                               WICDecodeMetadataCacheOnLoad, &decoder)
                         : E_FAIL;
    if (SUCCEEDED(result)) {
        result = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    }
    if (SUCCEEDED(result)) {
        result = IWICImagingFactory_CreateFormatConverter(state->wic_factory, &converter);
    }
    if (SUCCEEDED(result)) {
        result = IWICFormatConverter_Initialize(
            converter, (IWICBitmapSource *)frame, &GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeMedianCut);
    }
    if (SUCCEEDED(result)) {
        result = ID2D1HwndRenderTarget_CreateBitmapFromWicBitmap(
            state->render_target, (IWICBitmapSource *)converter, NULL, &state->wheel_bitmap);
    }
    if (converter != NULL) {
        IWICFormatConverter_Release(converter);
    }
    if (frame != NULL) {
        IWICBitmapFrameDecode_Release(frame);
    }
    if (decoder != NULL) {
        IWICBitmapDecoder_Release(decoder);
    }
    return SUCCEEDED(result);
}

static bool ensure_render_target(HWND window, AppState *state) {
    if (state->render_target != NULL) {
        return true;
    }
    RECT client;
    GetClientRect(window, &client);
    D2D1_RENDER_TARGET_PROPERTIES target = {
        .type = D2D1_RENDER_TARGET_TYPE_DEFAULT,
        .pixelFormat = {.format = DXGI_FORMAT_UNKNOWN, .alphaMode = D2D1_ALPHA_MODE_UNKNOWN},
        .dpiX = 0,
        .dpiY = 0,
        .usage = D2D1_RENDER_TARGET_USAGE_NONE,
        .minLevel = D2D1_FEATURE_LEVEL_DEFAULT};
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwnd = {
        .hwnd = window,
        .pixelSize = {(UINT32)client.right, (UINT32)client.bottom},
        .presentOptions = D2D1_PRESENT_OPTIONS_NONE};
    if (FAILED(ID2D1Factory_CreateHwndRenderTarget(state->d2d_factory, &target, &hwnd,
                                                   &state->render_target))) {
        return false;
    }
    D2D1_COLOR_F black = color(20, 23, 27);
    if (FAILED(ID2D1HwndRenderTarget_CreateSolidColorBrush(state->render_target, &black, NULL,
                                                           &state->brush))) {
        release_render_target(state);
        return false;
    }
    state->displayed_frame_valid = false;
    load_wheel_bitmap(state);
    return true;
}

static void set_brush(AppState *state, D2D1_COLOR_F value) {
    ID2D1SolidColorBrush_SetColor(state->brush, &value);
}

static void draw_text(AppState *state, IDWriteTextFormat *format, float left, float top,
                      float right, float bottom, const wchar_t *text) {
    D2D1_RECT_F rectangle = {left, top, right, bottom};
    ID2D1HwndRenderTarget_DrawText(state->render_target, text, (UINT32)wcslen(text), format,
                                   &rectangle, (ID2D1Brush *)state->brush,
                                   D2D1_DRAW_TEXT_OPTIONS_CLIP, DWRITE_MEASURING_MODE_NATURAL);
}

static void draw_display(AppState *state, const Dd1SimSnapshot *snapshot) {
    bool changed =
        !state->displayed_frame_valid ||
        memcmp(state->displayed_frame, snapshot->display, sizeof(state->displayed_frame)) != 0;
    if (changed) {
        uint32_t pixels[DD1_SIM_DISPLAY_WIDTH * DD1_SIM_DISPLAY_HEIGHT];
        for (int y = 0; y < DD1_SIM_DISPLAY_HEIGHT; ++y) {
            for (int x = 0; x < DD1_SIM_DISPLAY_WIDTH; ++x) {
                uint8_t packed = snapshot->display[y * DD1_SIM_DISPLAY_WIDTH / 2 + x / 2];
                uint8_t level = (x & 1) == 0 ? packed >> 4U : packed & 0x0fU;
                uint8_t shade = (uint8_t)(level * 17U);
                pixels[y * DD1_SIM_DISPLAY_WIDTH + x] = UINT32_C(0xff000000) | (uint32_t)shade |
                                                        (uint32_t)shade << 8U |
                                                        (uint32_t)shade << 16U;
            }
        }
        if (state->display_bitmap == NULL) {
            D2D1_SIZE_U size = {DD1_SIM_DISPLAY_WIDTH, DD1_SIM_DISPLAY_HEIGHT};
            D2D1_BITMAP_PROPERTIES properties = {
                .pixelFormat = {.format = DXGI_FORMAT_B8G8R8A8_UNORM,
                                .alphaMode = D2D1_ALPHA_MODE_IGNORE},
                .dpiX = 96,
                .dpiY = 96};
            ID2D1HwndRenderTarget_CreateBitmap(state->render_target, size, pixels,
                                               DD1_SIM_DISPLAY_WIDTH * sizeof(uint32_t),
                                               &properties, &state->display_bitmap);
        } else {
            ID2D1Bitmap_CopyFromMemory(state->display_bitmap, NULL, pixels,
                                       DD1_SIM_DISPLAY_WIDTH * sizeof(uint32_t));
        }
        if (state->display_bitmap != NULL) {
            memcpy(state->displayed_frame, snapshot->display, sizeof(state->displayed_frame));
            state->displayed_frame_valid = true;
        }
    }
    D2D1_ROUNDED_RECT bezel = {{DISPLAY_X - 6, DISPLAY_Y - 6,
                                DISPLAY_X + DD1_SIM_DISPLAY_WIDTH * DISPLAY_SCALE + 6,
                                DISPLAY_Y + DD1_SIM_DISPLAY_HEIGHT * DISPLAY_SCALE + 6},
                               5,
                               5};
    set_brush(state, color(25, 27, 30));
    ID2D1HwndRenderTarget_FillRoundedRectangle(state->render_target, &bezel,
                                               (ID2D1Brush *)state->brush);
    if (state->display_bitmap != NULL) {
        D2D1_RECT_F destination = {DISPLAY_X, DISPLAY_Y,
                                   DISPLAY_X + DD1_SIM_DISPLAY_WIDTH * DISPLAY_SCALE,
                                   DISPLAY_Y + DD1_SIM_DISPLAY_HEIGHT * DISPLAY_SCALE};
        ID2D1HwndRenderTarget_DrawBitmap(state->render_target, state->display_bitmap, &destination,
                                         1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                                         NULL);
    }
}

static void draw_wheel(AppState *state, const Dd1SimSnapshot *snapshot) {
    if (state->wheel_bitmap != NULL) {
        double radians = (double)snapshot->angle_tenths * 3.14159265358979323846 / 1800.0;
        float cosine = (float)cos(radians);
        float sine = (float)sin(radians);
        D2D1_MATRIX_3X2_F rotation = {0};
        rotation._11 = cosine;
        rotation._12 = sine;
        rotation._21 = -sine;
        rotation._22 = cosine;
        rotation._31 = WHEEL_X - WHEEL_X * cosine + WHEEL_Y * sine;
        rotation._32 = WHEEL_Y - WHEEL_X * sine - WHEEL_Y * cosine;
        D2D1_MATRIX_3X2_F identity = {0};
        identity._11 = 1;
        identity._22 = 1;
        D2D1_RECT_F destination = {27, 346, 263, 494};
        D2D1_RECT_F source = {0, 0, 600, 376};
        ID2D1HwndRenderTarget_SetTransform(state->render_target, &rotation);
        ID2D1HwndRenderTarget_DrawBitmap(state->render_target, state->wheel_bitmap, &destination,
                                         1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &source);
        ID2D1HwndRenderTarget_SetTransform(state->render_target, &identity);
    } else {
        set_brush(state, color(120, 124, 130));
        draw_text(state, state->ui_format, 36, 395, 254, 445, L"Formula V2.5 image unavailable");
    }
    wchar_t label[48];
    swprintf(label, sizeof(label) / sizeof(label[0]), L"Shaft  %7.1f%c",
             (double)snapshot->angle_tenths / 10.0, 0x00b0);
    set_brush(state, color(20, 23, 27));
    draw_text(state, state->telemetry_format, WHEEL_X - 78, 546, WHEEL_X + 78, 572, label);
}

static void draw_motor(AppState *state, const Dd1SimSnapshot *snapshot) {
    D2D1_ROUNDED_RECT frame = {
        {FORCE_X, FORCE_Y, FORCE_X + FORCE_WIDTH, FORCE_Y + FORCE_HEIGHT}, 7, 7};
    set_brush(state, color(205, 209, 214));
    ID2D1HwndRenderTarget_DrawRoundedRectangle(state->render_target, &frame,
                                               (ID2D1Brush *)state->brush, 1, NULL);
    int bounded = snapshot->motor_current;
    if (bounded < -32767) {
        bounded = -32767;
    } else if (bounded > 32767) {
        bounded = 32767;
    }
    float middle = FORCE_Y + FORCE_HEIGHT / 2.0f;
    float extent = bounded * (FORCE_HEIGHT / 2.0f - 8.0f) / 32767.0f;
    D2D1_RECT_F bar = {FORCE_X + 8, extent >= 0 ? middle - extent : middle,
                       FORCE_X + FORCE_WIDTH - 8, extent >= 0 ? middle : middle - extent};
    set_brush(state, extent >= 0 ? color(46, 113, 190) : color(205, 91, 45));
    ID2D1HwndRenderTarget_FillRectangle(state->render_target, &bar, (ID2D1Brush *)state->brush);
    set_brush(state, color(125, 130, 136));
    D2D1_POINT_2F line_start = {FORCE_X, middle};
    D2D1_POINT_2F line_end = {FORCE_X + FORCE_WIDTH, middle};
    ID2D1HwndRenderTarget_DrawLine(state->render_target, line_start, line_end,
                                   (ID2D1Brush *)state->brush, 1, NULL);
    wchar_t label[64];
    swprintf(label, sizeof(label) / sizeof(label[0]), L"Drive %7d\nPosition %7ld",
             snapshot->motor_current, (long)snapshot->motor_position);
    set_brush(state, color(20, 23, 27));
    draw_text(state, state->telemetry_format, FORCE_X, FORCE_Y + FORCE_HEIGHT + 10,
              FORCE_X + FORCE_WIDTH, FORCE_Y + FORCE_HEIGHT + 52, label);
}

static void paint_window(HWND window, AppState *state) {
    PAINTSTRUCT paint;
    BeginPaint(window, &paint);
    if (!ensure_render_target(window, state)) {
        EndPaint(window, &paint);
        return;
    }
    Dd1SimSnapshot snapshot;
    EnterCriticalSection(&state->lock);
    snapshot = state->snapshot;
    snapshot.angle_tenths = state->angle_tenths;
    LeaveCriticalSection(&state->lock);
    ID2D1HwndRenderTarget_BeginDraw(state->render_target);
    D2D1_COLOR_F background = color(247, 248, 250);
    ID2D1HwndRenderTarget_Clear(state->render_target, &background);
    set_brush(state, color(20, 23, 27));
    draw_text(state, state->ui_format, DISPLAY_X, 14, 400, 38, L"DD1 base OLED  ·  256 × 64");
    draw_display(state, &snapshot);
    wchar_t stats[512];
    swprintf(
        stats, sizeof(stats) / sizeof(stats[0]),
        L"BASE   %6.2fM  %6.2fMHz\n"
        L"WQR    %6.2fM  %6.2fMHz\n"
        L"MOTOR  %6.2fM  %6.2fMHz\n\n"
        L"OLED   %8llu B\nWHEEL  %8lu",
        (double)snapshot.base_instructions / 1000000.0, (double)snapshot.base_clock_hz / 1000000.0,
        (double)snapshot.wqr_instructions / 1000000.0, (double)snapshot.wqr_clock_hz / 1000000.0,
        (double)snapshot.motor_instructions / 1000000.0,
        (double)snapshot.motor_clock_hz / 1000000.0, (unsigned long long)snapshot.display_bytes,
        (unsigned long)snapshot.wheel_exchanges);
    draw_text(state, state->telemetry_format, 570, 52, 800, 180, stats);
    draw_text(state, state->ui_format, 92, 242, 235, 270, L"Wheel input");
    draw_text(state, state->ui_format, BUTTON_X, 242, 560, 270, L"Wheel buttons");
    draw_text(state, state->ui_format, FORCE_X, 242, 980, 270, L"Motor command");
    draw_wheel(state, &snapshot);
    draw_motor(state, &snapshot);
    HRESULT result = ID2D1HwndRenderTarget_EndDraw(state->render_target, NULL, NULL);
    if (result == D2DERR_RECREATE_TARGET) {
        release_render_target(state);
    }
    EndPaint(window, &paint);
}

static void set_angle_from_cursor(AppState *state, int x, int y) {
    int center_x = scaled(state, WHEEL_X);
    int center_y = scaled(state, WHEEL_Y);
    double angle = atan2((double)(x - center_x), (double)(center_y - y));
    int value = (int)lround(angle * 1800.0 / 3.14159265358979323846);
    if (value < -4500) {
        value = -4500;
    } else if (value > 4500) {
        value = 4500;
    }
    EnterCriticalSection(&state->lock);
    state->angle_tenths = (int16_t)value;
    LeaveCriticalSection(&state->lock);
}

static void change_angle(AppState *state, int delta) {
    EnterCriticalSection(&state->lock);
    int value = state->angle_tenths + delta;
    state->angle_tenths = (int16_t)(value < -4500 ? -4500 : value > 4500 ? 4500 : value);
    LeaveCriticalSection(&state->lock);
}

static unsigned __stdcall simulation_worker(void *context) {
    AppState *state = context;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    while (InterlockedCompareExchange(&state->stop, 0, 0) == 0) {
        int16_t angle;
        uint32_t buttons;
        EnterCriticalSection(&state->lock);
        angle = state->angle_tenths;
        buttons = state->buttons;
        LeaveCriticalSection(&state->lock);
        dd1_sim_machine_set_inputs(state->machine, angle, buttons);
        if (InterlockedCompareExchange(&state->paused, 0, 0) == 0) {
            dd1_sim_machine_run(state->machine, WORKER_ROUNDS);
        } else {
            Sleep(10);
        }
        Dd1SimSnapshot snapshot;
        dd1_sim_machine_snapshot(state->machine, &snapshot);
        EnterCriticalSection(&state->lock);
        state->snapshot = snapshot;
        LeaveCriticalSection(&state->lock);
        if (!snapshot.running) {
            InterlockedExchange(&state->paused, 1);
        }
        Sleep(0);
    }
    return 0;
}

static void update_status(AppState *state) {
    Dd1SimSnapshot snapshot;
    EnterCriticalSection(&state->lock);
    snapshot = state->snapshot;
    LeaveCriticalSection(&state->lock);
    wchar_t status[DD1_SIM_STATUS_SIZE + 16];
    MultiByteToWideChar(CP_UTF8, 0, snapshot.status, -1, status,
                        (int)(sizeof(status) / sizeof(status[0])));
    if (InterlockedCompareExchange(&state->paused, 0, 0) != 0 && snapshot.running) {
        wcscpy(status, L"Paused");
    }
    wchar_t current[DD1_SIM_STATUS_SIZE + 16];
    GetWindowTextW(state->status_control, current, (int)(sizeof(current) / sizeof(current[0])));
    if (wcscmp(current, status) != 0) {
        SetWindowTextW(state->status_control, status);
    }
    const wchar_t *pause_text =
        InterlockedCompareExchange(&state->paused, 0, 0) != 0 ? L"Resume" : L"Pause";
    GetWindowTextW(state->pause_control, current, (int)(sizeof(current) / sizeof(current[0])));
    if (wcscmp(current, pause_text) != 0) {
        SetWindowTextW(state->pause_control, pause_text);
    }
}

static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM word, LPARAM number) {
    AppState *state = (AppState *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (message == WM_NCCREATE) {
        CREATESTRUCTW *create = (CREATESTRUCTW *)number;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
        return TRUE;
    }
    if (state == NULL) {
        return DefWindowProcW(window, message, word, number);
    }
    switch (message) {
    case WM_PAINT:
        paint_window(window, state);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        if (state->render_target != NULL) {
            D2D1_SIZE_U size = {LOWORD(number), HIWORD(number)};
            ID2D1HwndRenderTarget_Resize(state->render_target, &size);
        }
        return 0;
    case WM_CTLCOLORSTATIC:
        SetTextColor((HDC)word, RGB(0, 0, 0));
        SetBkColor((HDC)word, RGB(247, 248, 250));
        return (LRESULT)state->background_brush;
    case WM_TIMER:
        update_status(state);
        InvalidateRect(window, NULL, FALSE);
        return 0;
    case WM_COMMAND: {
        int identifier = LOWORD(word);
        if (identifier >= ID_BUTTON_FIRST && identifier < ID_BUTTON_FIRST + 24) {
            int index = identifier - ID_BUTTON_FIRST;
            bool checked =
                SendMessageW(state->button_controls[index], BM_GETCHECK, 0, 0) == BST_CHECKED;
            EnterCriticalSection(&state->lock);
            if (checked) {
                state->buttons |= UINT32_C(1) << index;
            } else {
                state->buttons &= ~(UINT32_C(1) << index);
            }
            LeaveCriticalSection(&state->lock);
        } else if (identifier == ID_PAUSE) {
            LONG paused = InterlockedCompareExchange(&state->paused, 0, 0);
            InterlockedExchange(&state->paused, !paused);
            update_status(state);
        } else if (identifier == ID_LEFT) {
            change_angle(state, -50);
        } else if (identifier == ID_RIGHT) {
            change_angle(state, 50);
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(number);
        int y = GET_Y_LPARAM(number);
        int dx = x - scaled(state, WHEEL_X);
        int dy = y - scaled(state, WHEEL_Y);
        int radius = scaled(state, WHEEL_RADIUS);
        if (dx * dx + dy * dy <= radius * radius) {
            state->dragging = true;
            SetCapture(window);
            set_angle_from_cursor(state, x, y);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (state->dragging) {
            set_angle_from_cursor(state, GET_X_LPARAM(number), GET_Y_LPARAM(number));
        }
        return 0;
    case WM_LBUTTONUP:
        state->dragging = false;
        ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        state->dragging = false;
        return 0;
    case WM_DESTROY:
        KillTimer(window, TIMER_ID);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, word, number);
    }
}

static int run_headless(uint64_t rounds, const char *base, const char *wqr, const char *motor) {
    char error[256] = {0};
    Dd1SimMachine *machine = dd1_sim_machine_create(base, wqr, motor, error, sizeof(error));
    if (machine == NULL) {
        fprintf(stderr, "%s\n", error);
        return EXIT_FAILURE;
    }
    while (rounds != 0) {
        uint32_t batch = rounds > UINT32_C(100000) ? UINT32_C(100000) : (uint32_t)rounds;
        if (!dd1_sim_machine_run(machine, batch)) {
            break;
        }
        rounds -= batch;
    }
    Dd1SimSnapshot snapshot;
    dd1_sim_machine_snapshot(machine, &snapshot);
    printf("%s\ndisplay=%s wheel-exchanges=%lu current=%d position=%ld\n"
           "pc base=%08lx wqr=%08lx motor=%08lx\n",
           snapshot.status, snapshot.display_ready ? "ready" : "pending",
           (unsigned long)snapshot.wheel_exchanges, snapshot.motor_current,
           (long)snapshot.motor_position, (unsigned long)snapshot.base_program_counter,
           (unsigned long)snapshot.wqr_program_counter,
           (unsigned long)snapshot.motor_program_counter);
    printf("link display=%llu base-wqr=%llu/%llu base-motor=%llu/%llu\n",
           (unsigned long long)snapshot.display_bytes, (unsigned long long)snapshot.base_uart_bytes,
           (unsigned long long)snapshot.wqr_uart_bytes,
           (unsigned long long)snapshot.base_motor_words,
           (unsigned long long)snapshot.motor_base_words);
    bool passed = snapshot.running;
    dd1_sim_machine_destroy(machine);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

static double benchmark_mode(uint64_t rounds, const Dd1FirmwarePaths *paths, bool parallel,
                             Dd1SimSnapshot *snapshot, char *error, size_t error_size) {
    Dd1SimMachine *machine =
        dd1_sim_machine_create(paths->base, paths->wqr, paths->motor, error, error_size);
    if (machine == NULL) {
        return -1.0;
    }
    dd1_sim_machine_set_parallel(machine, parallel);
    LARGE_INTEGER frequency;
    LARGE_INTEGER start;
    LARGE_INTEGER finish;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    uint64_t remaining = rounds;
    while (remaining != 0) {
        uint32_t batch = remaining > UINT32_C(100000) ? UINT32_C(100000) : (uint32_t)remaining;
        if (!dd1_sim_machine_run(machine, batch)) {
            break;
        }
        remaining -= batch;
    }
    QueryPerformanceCounter(&finish);
    dd1_sim_machine_snapshot(machine, snapshot);
    dd1_sim_machine_destroy(machine);
    if (!snapshot->running) {
        snprintf(error, error_size, "%s", snapshot->status);
        return -1.0;
    }
    return (double)(finish.QuadPart - start.QuadPart) / (double)frequency.QuadPart;
}

static int run_benchmark(uint64_t rounds, const Dd1FirmwarePaths *paths) {
    char error[256] = {0};
    Dd1SimSnapshot sequential_snapshot;
    Dd1SimSnapshot parallel_snapshot;
    double sequential_samples[3];
    double parallel_samples[3];
    for (size_t index = 0; index < 3; ++index) {
        sequential_samples[index] =
            benchmark_mode(rounds, paths, false, &sequential_snapshot, error, sizeof(error));
        if (sequential_samples[index] < 0.0) {
            fprintf(stderr, "sequential benchmark failed: %s\n", error);
            return EXIT_FAILURE;
        }
        parallel_samples[index] =
            benchmark_mode(rounds, paths, true, &parallel_snapshot, error, sizeof(error));
        if (parallel_samples[index] < 0.0) {
            fprintf(stderr, "parallel benchmark failed: %s\n", error);
            return EXIT_FAILURE;
        }
    }
    for (size_t left = 0; left < 2; ++left) {
        for (size_t right = left + 1; right < 3; ++right) {
            if (sequential_samples[right] < sequential_samples[left]) {
                double value = sequential_samples[left];
                sequential_samples[left] = sequential_samples[right];
                sequential_samples[right] = value;
            }
            if (parallel_samples[right] < parallel_samples[left]) {
                double value = parallel_samples[left];
                parallel_samples[left] = parallel_samples[right];
                parallel_samples[right] = value;
            }
        }
    }
    double sequential = sequential_samples[1];
    double parallel = parallel_samples[1];
    printf("median of 3: sequential %.3f s, three-worker %.3f s, speedup %.2fx\n", sequential,
           parallel, sequential / parallel);
    printf("sequential instructions base=%llu WQR=%llu motor=%llu\n",
           (unsigned long long)sequential_snapshot.base_instructions,
           (unsigned long long)sequential_snapshot.wqr_instructions,
           (unsigned long long)sequential_snapshot.motor_instructions);
    printf("three-worker instructions base=%llu WQR=%llu motor=%llu\n",
           (unsigned long long)parallel_snapshot.base_instructions,
           (unsigned long long)parallel_snapshot.wqr_instructions,
           (unsigned long long)parallel_snapshot.motor_instructions);
    return EXIT_SUCCESS;
}

static void create_controls(HWND window, AppState *state) {
    HFONT font = GetStockObject(DEFAULT_GUI_FONT);
    for (int index = 0; index < 24; ++index) {
        int column = index % BUTTON_COLUMNS;
        int row = index / BUTTON_COLUMNS;
        wchar_t label[12];
        swprintf(label, sizeof(label) / sizeof(label[0]), L"Button %d", index + 1);
        HWND control = CreateWindowExW(
            0, L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX | BS_PUSHLIKE,
            scaled(state, BUTTON_X + column * (BUTTON_WIDTH + BUTTON_GAP)),
            scaled(state, BUTTON_Y + row * (BUTTON_HEIGHT + BUTTON_GAP)),
            scaled(state, BUTTON_WIDTH), scaled(state, BUTTON_HEIGHT), window,
            (HMENU)(INT_PTR)(ID_BUTTON_FIRST + index), GetModuleHandleW(NULL), NULL);
        state->button_controls[index] = control;
        SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
    }
    state->pause_control =
        CreateWindowExW(0, L"BUTTON", L"Pause", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                        scaled(state, 950), scaled(state, 12), scaled(state, 80), scaled(state, 26),
                        window, (HMENU)(INT_PTR)ID_PAUSE, GetModuleHandleW(NULL), NULL);
    state->status_control =
        CreateWindowExW(0, L"STATIC", L"Booting DD1 firmware", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        scaled(state, DISPLAY_X), scaled(state, 190), scaled(state, 956),
                        scaled(state, 26), window, NULL, GetModuleHandleW(NULL), NULL);
    SendMessageW(state->pause_control, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(state->status_control, WM_SETFONT, (WPARAM)font, TRUE);
}

static bool initialize_graphics(AppState *state) {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL,
                                 (void **)&state->d2d_factory)) ||
        FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IWICImagingFactory, (void **)&state->wic_factory)) ||
        FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory,
                                   (IUnknown **)&state->write_factory)) ||
        FAILED(IDWriteFactory_CreateTextFormat(state->write_factory, L"Segoe UI", NULL,
                                               DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                               DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                               14, L"en-us", &state->ui_format)) ||
        FAILED(IDWriteFactory_CreateTextFormat(state->write_factory, L"Consolas", NULL,
                                               DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, 13, L"en-us",
                                               &state->telemetry_format))) {
        return false;
    }
    return true;
}

static void release_graphics(AppState *state) {
    release_render_target(state);
    if (state->ui_format != NULL) {
        IDWriteTextFormat_Release(state->ui_format);
    }
    if (state->telemetry_format != NULL) {
        IDWriteTextFormat_Release(state->telemetry_format);
    }
    if (state->write_factory != NULL) {
        IDWriteFactory_Release(state->write_factory);
    }
    if (state->wic_factory != NULL) {
        IWICImagingFactory_Release(state->wic_factory);
    }
    if (state->d2d_factory != NULL) {
        ID2D1Factory_Release(state->d2d_factory);
    }
    if (state->background_brush != NULL) {
        DeleteObject(state->background_brush);
        state->background_brush = NULL;
    }
}

static int run_window(const char *base, const char *wqr, const char *motor);

static int run_gui(const char *base, const char *wqr, const char *motor) {
    HRESULT result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(result) && result != RPC_E_CHANGED_MODE) {
        MessageBoxW(NULL, L"Windows imaging could not be initialized.", L"DD1 Simulator",
                    MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
    int exit_code = run_window(base, wqr, motor);
    if (SUCCEEDED(result)) {
        CoUninitialize();
    }
    return exit_code;
}

static int run_window(const char *base, const char *wqr, const char *motor) {
    char error[256] = {0};
    AppState state = {0};
    state.dpi = 96;
    state.machine = dd1_sim_machine_create(base, wqr, motor, error, sizeof(error));
    if (state.machine == NULL) {
        MessageBoxA(NULL, error, "DD1 Simulator", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
    InitializeCriticalSection(&state.lock);
    dd1_sim_machine_snapshot(state.machine, &state.snapshot);
    SetProcessDPIAware();
    HDC screen = GetDC(NULL);
    state.dpi = screen != NULL ? (UINT)GetDeviceCaps(screen, LOGPIXELSX) : 96;
    if (screen != NULL) {
        ReleaseDC(NULL, screen);
    }
    state.background_brush = CreateSolidBrush(RGB(247, 248, 250));
    if (!initialize_graphics(&state)) {
        release_graphics(&state);
        DeleteCriticalSection(&state.lock);
        dd1_sim_machine_destroy(state.machine);
        return EXIT_FAILURE;
    }
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW window_class = {.lpfnWndProc = window_procedure,
                              .hInstance = instance,
                              .hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(32512)),
                              .hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512)),
                              .hbrBackground = GetSysColorBrush(COLOR_WINDOW),
                              .lpszClassName = L"OpentecDd1Simulator"};
    if (RegisterClassW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        release_graphics(&state);
        DeleteCriticalSection(&state.lock);
        dd1_sim_machine_destroy(state.machine);
        return EXIT_FAILURE;
    }
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT bounds = {.left = 0,
                   .top = 0,
                   .right = scaled(&state, CLIENT_WIDTH),
                   .bottom = scaled(&state, CLIENT_HEIGHT)};
    AdjustWindowRect(&bounds, style, FALSE);
    HWND window = CreateWindowExW(0, window_class.lpszClassName, L"Opentec DD1 Simulator", style,
                                  CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left,
                                  bounds.bottom - bounds.top, NULL, NULL, instance, &state);
    if (window == NULL) {
        release_graphics(&state);
        DeleteCriticalSection(&state.lock);
        dd1_sim_machine_destroy(state.machine);
        return EXIT_FAILURE;
    }
    SetWindowTextW(window, L"Opentec DD1 Simulator");
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    create_controls(window, &state);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    if (SetTimer(window, TIMER_ID, TIMER_PERIOD_MS, NULL) == 0) {
        DestroyWindow(window);
        release_graphics(&state);
        DeleteCriticalSection(&state.lock);
        dd1_sim_machine_destroy(state.machine);
        return EXIT_FAILURE;
    }
    uintptr_t worker = _beginthreadex(NULL, 0, simulation_worker, &state, 0, NULL);
    state.worker = (HANDLE)worker;
    if (state.worker == NULL) {
        DestroyWindow(window);
        release_graphics(&state);
        DeleteCriticalSection(&state.lock);
        dd1_sim_machine_destroy(state.machine);
        return EXIT_FAILURE;
    }
    ACCEL accelerators[] = {{FVIRTKEY, VK_SPACE, ID_PAUSE},
                            {FVIRTKEY, VK_LEFT, ID_LEFT},
                            {FVIRTKEY, VK_RIGHT, ID_RIGHT}};
    HACCEL accelerator_table = CreateAcceleratorTableW(accelerators, 3);
    MSG message;
    int message_result;
    while ((message_result = GetMessageW(&message, NULL, 0, 0)) > 0) {
        if (accelerator_table == NULL ||
            !TranslateAcceleratorW(window, accelerator_table, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    InterlockedExchange(&state.stop, 1);
    WaitForSingleObject(state.worker, INFINITE);
    CloseHandle(state.worker);
    if (accelerator_table != NULL) {
        DestroyAcceleratorTable(accelerator_table);
    }
    DeleteCriticalSection(&state.lock);
    release_graphics(&state);
    dd1_sim_machine_destroy(state.machine);
    return message_result == -1 ? EXIT_FAILURE : EXIT_SUCCESS;
}

static bool select_firmware(char *path, size_t path_size, const char *title) {
    OPENFILENAMEA dialog = {.lStructSize = sizeof(dialog),
                            .lpstrFilter = "Encrypted Intel HEX firmware\0*.hex\0",
                            .lpstrFile = path,
                            .nMaxFile = (DWORD)path_size,
                            .lpstrTitle = title,
                            .Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST};
    return GetOpenFileNameA(&dialog) != FALSE;
}

static bool select_firmware_set(Dd1FirmwarePaths *paths) {
    return select_firmware(paths->base, sizeof(paths->base),
                           "Select encrypted DD1 base firmware") &&
           select_firmware(paths->wqr, sizeof(paths->wqr), "Select encrypted WQR firmware") &&
           select_firmware(paths->motor, sizeof(paths->motor),
                           "Select encrypted DD1 motor firmware");
}

static bool parse_rounds(const char *text, uint64_t *rounds) {
    char *end = NULL;
    *rounds = text != NULL ? strtoull(text, &end, 10) : 0;
    return *rounds != 0 && end != text && *end == 0;
}

int main(int argument_count, char **arguments) {
    const char default_directory[] = "C:\\Program Files\\Fanatec\\Fanatec Wheel\\fw";
    Dd1FirmwarePaths paths = {0};
    char error[256] = {0};
    if (argument_count >= 2 && strcmp(arguments[1], "--benchmark") == 0) {
        uint64_t rounds;
        if (argument_count < 3 || argument_count > 4 || !parse_rounds(arguments[2], &rounds)) {
            fprintf(stderr, "benchmark rounds must be a positive integer\n");
            return EXIT_FAILURE;
        }
        const char *directory = argument_count == 4 ? arguments[3] : default_directory;
        if (!dd1_firmware_find(directory, &paths, error, sizeof(error))) {
            fprintf(stderr, "%s\n", error);
            return EXIT_FAILURE;
        }
        return run_benchmark(rounds, &paths);
    }
    if (argument_count >= 2 && strcmp(arguments[1], "--headless") == 0) {
        uint64_t rounds;
        if (argument_count < 3 || !parse_rounds(arguments[2], &rounds)) {
            fprintf(stderr, "headless rounds must be a positive integer\n");
            return EXIT_FAILURE;
        }
        if (argument_count == 3) {
            if (!dd1_firmware_find(default_directory, &paths, error, sizeof(error))) {
                fprintf(stderr, "%s\n", error);
                return EXIT_FAILURE;
            }
        } else if (argument_count == 4) {
            if (!dd1_firmware_find(arguments[3], &paths, error, sizeof(error))) {
                fprintf(stderr, "%s\n", error);
                return EXIT_FAILURE;
            }
        } else if (argument_count == 6) {
            snprintf(paths.base, sizeof(paths.base), "%s", arguments[3]);
            snprintf(paths.wqr, sizeof(paths.wqr), "%s", arguments[4]);
            snprintf(paths.motor, sizeof(paths.motor), "%s", arguments[5]);
        } else {
            fprintf(stderr, "invalid headless firmware arguments\n");
            return EXIT_FAILURE;
        }
        return run_headless(rounds, paths.base, paths.wqr, paths.motor);
    }
    if (argument_count == 1) {
        if (!dd1_firmware_find(default_directory, &paths, error, sizeof(error)) &&
            !select_firmware_set(&paths)) {
            return EXIT_SUCCESS;
        }
        return run_gui(paths.base, paths.wqr, paths.motor);
    }
    if (argument_count == 2) {
        if (!dd1_firmware_find(arguments[1], &paths, error, sizeof(error))) {
            MessageBoxA(NULL, error, "DD1 Simulator", MB_OK | MB_ICONERROR);
            return EXIT_FAILURE;
        }
        return run_gui(paths.base, paths.wqr, paths.motor);
    }
    if (argument_count == 4) {
        return run_gui(arguments[1], arguments[2], arguments[3]);
    }
    fprintf(stderr, "usage: dd1-sim [FIRMWARE_DIRECTORY]\n"
                    "       dd1-sim BASE.hex WQR.hex MOTOR.hex\n"
                    "       dd1-sim --headless ROUNDS [FIRMWARE_DIRECTORY]\n"
                    "       dd1-sim --headless ROUNDS BASE.hex WQR.hex MOTOR.hex\n"
                    "       dd1-sim --benchmark ROUNDS [FIRMWARE_DIRECTORY]\n");
    return EXIT_FAILURE;
}
