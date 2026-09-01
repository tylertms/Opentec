#define WIN32_LEAN_AND_MEAN
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#if defined(_WINDOWS_)
#include <commdlg.h>
#endif

#include "machine.h"

enum {
    WINDOW_WIDTH = 1060,
    WINDOW_HEIGHT = 760,
    DISPLAY_X = 24,
    DISPLAY_Y = 40,
    DISPLAY_SCALE = 3,
    WHEEL_X = 280,
    WHEEL_Y = 500,
    WHEEL_RADIUS = 150,
    BUTTON_X = 500,
    BUTTON_Y = 330,
    BUTTON_WIDTH = 82,
    BUTTON_HEIGHT = 44,
    BUTTON_COLUMNS = 4,
    BUTTON_ROWS = 6,
    BUTTON_GAP = 10,
    TIMER_ID = 1,
    TIMER_PERIOD_MS = 33,
    WORKER_ROUNDS = 5000,
};

typedef struct {
    Dd1SimMachine *machine;
    Dd1SimSnapshot snapshot;
    CRITICAL_SECTION lock;
    HANDLE worker;
    LONG stop;
    LONG paused;
    int16_t angle_tenths;
    uint32_t buttons;
    bool dragging;
} AppState;

static void draw_text(HDC device, int x, int y, const char *format, ...) {
    char text[256];
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    if (length > 0) {
        TextOutA(device, x, y, text, length < (int)sizeof(text) ? length : (int)sizeof(text) - 1);
    }
}

static void draw_display(HDC device, const Dd1SimSnapshot *snapshot) {
    uint32_t pixels[DD1_SIM_DISPLAY_WIDTH * DD1_SIM_DISPLAY_HEIGHT];
    BITMAPINFO info = {0};
    for (int y = 0; y < DD1_SIM_DISPLAY_HEIGHT; ++y) {
        for (int x = 0; x < DD1_SIM_DISPLAY_WIDTH; ++x) {
            uint8_t packed = snapshot->display[y * DD1_SIM_DISPLAY_WIDTH / 2 + x / 2];
            uint8_t level = (x & 1) == 0 ? packed >> 4U : packed & 0x0fU;
            uint8_t shade = (uint8_t)(level * 17U);
            pixels[y * DD1_SIM_DISPLAY_WIDTH + x] =
                (uint32_t)shade | (uint32_t)shade << 8U | (uint32_t)shade << 16U;
        }
    }
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = DD1_SIM_DISPLAY_WIDTH;
    info.bmiHeader.biHeight = -DD1_SIM_DISPLAY_HEIGHT;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    Rectangle(device, DISPLAY_X - 2, DISPLAY_Y - 2,
              DISPLAY_X + DD1_SIM_DISPLAY_WIDTH * DISPLAY_SCALE + 2,
              DISPLAY_Y + DD1_SIM_DISPLAY_HEIGHT * DISPLAY_SCALE + 2);
    StretchDIBits(device, DISPLAY_X, DISPLAY_Y, DD1_SIM_DISPLAY_WIDTH * DISPLAY_SCALE,
                  DD1_SIM_DISPLAY_HEIGHT * DISPLAY_SCALE, 0, 0, DD1_SIM_DISPLAY_WIDTH,
                  DD1_SIM_DISPLAY_HEIGHT, pixels, &info, DIB_RGB_COLORS, SRCCOPY);
    if (!snapshot->display_ready) {
        SetTextColor(device, RGB(220, 220, 220));
        SetBkMode(device, TRANSPARENT);
        draw_text(device, DISPLAY_X + 250, DISPLAY_Y + 85, "Waiting for base display DMA");
        SetTextColor(device, RGB(0, 0, 0));
    }
}

static void draw_wheel(HDC device, const Dd1SimSnapshot *snapshot) {
    HPEN rim_pen = CreatePen(PS_SOLID, 8, RGB(45, 45, 45));
    HPEN marker_pen = CreatePen(PS_SOLID, 5, RGB(220, 45, 45));
    HGDIOBJ previous_pen = SelectObject(device, rim_pen);
    HGDIOBJ previous_brush = SelectObject(device, GetStockObject(HOLLOW_BRUSH));
    Ellipse(device, WHEEL_X - WHEEL_RADIUS, WHEEL_Y - WHEEL_RADIUS, WHEEL_X + WHEEL_RADIUS,
            WHEEL_Y + WHEEL_RADIUS);
    SelectObject(device, marker_pen);
    double angle = (double)snapshot->angle_tenths * 3.14159265358979323846 / 1800.0;
    MoveToEx(device, WHEEL_X, WHEEL_Y, NULL);
    LineTo(device, WHEEL_X + (int)(sin(angle) * (WHEEL_RADIUS - 12)),
           WHEEL_Y - (int)(cos(angle) * (WHEEL_RADIUS - 12)));
    SelectObject(device, previous_brush);
    SelectObject(device, previous_pen);
    DeleteObject(rim_pen);
    DeleteObject(marker_pen);
    draw_text(device, WHEEL_X - 70, WHEEL_Y + WHEEL_RADIUS + 16, "Shaft: %.1f degrees",
              (double)snapshot->angle_tenths / 10.0);
}

static RECT button_rect(int index) {
    int column = index % BUTTON_COLUMNS;
    int row = index / BUTTON_COLUMNS;
    RECT rectangle = {
        .left = BUTTON_X + column * (BUTTON_WIDTH + BUTTON_GAP),
        .top = BUTTON_Y + row * (BUTTON_HEIGHT + BUTTON_GAP),
    };
    rectangle.right = rectangle.left + BUTTON_WIDTH;
    rectangle.bottom = rectangle.top + BUTTON_HEIGHT;
    return rectangle;
}

static void draw_buttons(HDC device, const Dd1SimSnapshot *snapshot) {
    for (int index = 0; index < BUTTON_COLUMNS * BUTTON_ROWS; ++index) {
        RECT rectangle = button_rect(index);
        bool active = (snapshot->buttons & (UINT32_C(1) << index)) != 0;
        HBRUSH brush = CreateSolidBrush(active ? RGB(55, 165, 90) : RGB(225, 225, 225));
        FillRect(device, &rectangle, brush);
        FrameRect(device, &rectangle, GetStockObject(BLACK_BRUSH));
        DeleteObject(brush);
        char label[16];
        int length = snprintf(label, sizeof(label), "Button %d", index + 1);
        SetBkMode(device, TRANSPARENT);
        TextOutA(device, rectangle.left + 10, rectangle.top + 14, label, length);
    }
}

static void draw_force(HDC device, const Dd1SimSnapshot *snapshot) {
    int center = 875;
    int top = 330;
    int width = 150;
    int height = 300;
    Rectangle(device, center - width / 2, top, center + width / 2, top + height);
    int bounded = snapshot->motor_current;
    if (bounded < -32767) {
        bounded = -32767;
    } else if (bounded > 32767) {
        bounded = 32767;
    }
    int middle = top + height / 2;
    int extent = bounded * (height / 2 - 4) / 32767;
    RECT bar = {
        .left = center - width / 2 + 4,
        .right = center + width / 2 - 4,
        .top = extent >= 0 ? middle - extent : middle,
        .bottom = extent >= 0 ? middle : middle - extent,
    };
    HBRUSH brush = CreateSolidBrush(extent >= 0 ? RGB(45, 125, 220) : RGB(220, 100, 45));
    FillRect(device, &bar, brush);
    DeleteObject(brush);
    MoveToEx(device, center - width / 2, middle, NULL);
    LineTo(device, center + width / 2, middle);
    draw_text(device, center - 62, top + height + 12, "Motor current: %d", snapshot->motor_current);
    draw_text(device, center - 62, top + height + 30, "Position: %ld",
              (long)snapshot->motor_position);
}

static void paint_window(HWND window, AppState *state) {
    PAINTSTRUCT paint;
    HDC device = BeginPaint(window, &paint);
    Dd1SimSnapshot snapshot;
    EnterCriticalSection(&state->lock);
    snapshot = state->snapshot;
    LeaveCriticalSection(&state->lock);
    FillRect(device, &paint.rcPaint, GetStockObject(WHITE_BRUSH));
    SelectObject(device, GetStockObject(DEFAULT_GUI_FONT));
    draw_text(device, DISPLAY_X, 16, "DD1 base display");
    draw_display(device, &snapshot);
    draw_text(device, DISPLAY_X, 245, "%s", snapshot.status);
    draw_text(device, DISPLAY_X, 265, "Wheel exchanges: %lu   Display bytes: %llu",
              (unsigned long)snapshot.wheel_exchanges, (unsigned long long)snapshot.display_bytes);
    draw_text(device, DISPLAY_X, 285, "Link bytes: base/WQR %llu/%llu   base/motor %llu/%llu",
              (unsigned long long)snapshot.base_uart_bytes,
              (unsigned long long)snapshot.wqr_uart_bytes,
              (unsigned long long)snapshot.base_motor_words,
              (unsigned long long)snapshot.motor_base_words);
    draw_text(device, DISPLAY_X, 305,
              "Drag the wheel, use Left/Right, click buttons, Space pauses");
    draw_wheel(device, &snapshot);
    draw_buttons(device, &snapshot);
    draw_force(device, &snapshot);
    EndPaint(window, &paint);
}

static void set_angle_from_cursor(AppState *state, int x, int y) {
    double angle = atan2((double)(x - WHEEL_X), (double)(WHEEL_Y - y));
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

static DWORD WINAPI simulation_worker(void *context) {
    AppState *state = context;
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
    }
    return 0;
}

static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM word, LPARAM number) {
    AppState *state = (AppState *)GetWindowLongPtrA(window, GWLP_USERDATA);
    if (message == WM_NCCREATE) {
        CREATESTRUCTA *create = (CREATESTRUCTA *)number;
        SetWindowLongPtrA(window, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
        return TRUE;
    }
    if (state == NULL) {
        return DefWindowProcA(window, message, word, number);
    }
    switch (message) {
    case WM_PAINT:
        paint_window(window, state);
        return 0;
    case WM_TIMER:
        InvalidateRect(window, NULL, FALSE);
        return 0;
    case WM_LBUTTONDOWN: {
        int x = (int)(short)LOWORD(number);
        int y = (int)(short)HIWORD(number);
        int dx = x - WHEEL_X;
        int dy = y - WHEEL_Y;
        if (dx * dx + dy * dy <= WHEEL_RADIUS * WHEEL_RADIUS) {
            state->dragging = true;
            SetCapture(window);
            set_angle_from_cursor(state, x, y);
            return 0;
        }
        for (int index = 0; index < BUTTON_COLUMNS * BUTTON_ROWS; ++index) {
            RECT rectangle = button_rect(index);
            POINT point = {.x = x, .y = y};
            if (PtInRect(&rectangle, point)) {
                EnterCriticalSection(&state->lock);
                state->buttons ^= UINT32_C(1) << index;
                LeaveCriticalSection(&state->lock);
                return 0;
            }
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (state->dragging) {
            set_angle_from_cursor(state, (int)(short)LOWORD(number), (int)(short)HIWORD(number));
        }
        return 0;
    case WM_LBUTTONUP:
        state->dragging = false;
        ReleaseCapture();
        return 0;
    case WM_KEYDOWN:
        if (word == VK_SPACE) {
            LONG paused = InterlockedCompareExchange(&state->paused, 0, 0);
            InterlockedExchange(&state->paused, !paused);
        } else if (word == VK_LEFT || word == VK_RIGHT) {
            EnterCriticalSection(&state->lock);
            int value = state->angle_tenths + (word == VK_LEFT ? -50 : 50);
            state->angle_tenths = (int16_t)(value < -4500 ? -4500 : value > 4500 ? 4500 : value);
            LeaveCriticalSection(&state->lock);
        }
        return 0;
    case WM_DESTROY:
        KillTimer(window, TIMER_ID);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(window, message, word, number);
    }
}

static int run_headless(uint64_t rounds, const char *base, const char *wqr, const char *motor) {
    char error[256];
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

static int run_window(const char *base, const char *wqr, const char *motor) {
    char error[256];
    AppState state = {0};
    state.machine = dd1_sim_machine_create(base, wqr, motor, error, sizeof(error));
    if (state.machine == NULL) {
        MessageBoxA(NULL, error, "DD1 Simulator", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
    InitializeCriticalSection(&state.lock);
    dd1_sim_machine_snapshot(state.machine, &state.snapshot);
    HINSTANCE instance = GetModuleHandleA(NULL);
    WNDCLASSA window_class = {
        .lpfnWndProc = window_procedure,
        .hInstance = instance,
        .hCursor = LoadCursorA(NULL, IDC_ARROW),
        .hbrBackground = GetStockObject(WHITE_BRUSH),
        .lpszClassName = "OpentecDd1Simulator",
    };
    if (RegisterClassA(&window_class) == 0) {
        DeleteCriticalSection(&state.lock);
        dd1_sim_machine_destroy(state.machine);
        return EXIT_FAILURE;
    }
    RECT bounds = {.left = 0, .top = 0, .right = WINDOW_WIDTH, .bottom = WINDOW_HEIGHT};
    AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowExA(0, window_class.lpszClassName, "Opentec DD1 Simulator",
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                                  bounds.right - bounds.left, bounds.bottom - bounds.top, NULL,
                                  NULL, instance, &state);
    if (window == NULL) {
        DeleteCriticalSection(&state.lock);
        dd1_sim_machine_destroy(state.machine);
        return EXIT_FAILURE;
    }
    SetTimer(window, TIMER_ID, TIMER_PERIOD_MS, NULL);
    state.worker = CreateThread(NULL, 0, simulation_worker, &state, 0, NULL);
    if (state.worker == NULL) {
        DestroyWindow(window);
        DeleteCriticalSection(&state.lock);
        dd1_sim_machine_destroy(state.machine);
        return EXIT_FAILURE;
    }
    MSG message;
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    InterlockedExchange(&state.stop, 1);
    WaitForSingleObject(state.worker, INFINITE);
    CloseHandle(state.worker);
    DeleteCriticalSection(&state.lock);
    dd1_sim_machine_destroy(state.machine);
    return EXIT_SUCCESS;
}

static bool select_firmware(char *path, size_t path_size, const char *title) {
    OPENFILENAMEA dialog = {
        .lStructSize = sizeof(dialog),
        .lpstrFilter = "ELF firmware\0*.elf\0All files\0*.*\0",
        .lpstrFile = path,
        .nMaxFile = (DWORD)path_size,
        .lpstrTitle = title,
        .Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
    };
    return GetOpenFileNameA(&dialog) != FALSE;
}

int main(int argument_count, char **arguments) {
    if (argument_count == 6 && strcmp(arguments[1], "--headless") == 0) {
        return run_headless(strtoull(arguments[2], NULL, 10), arguments[3], arguments[4],
                            arguments[5]);
    }
    if (argument_count == 1) {
        char base[MAX_PATH] = {0};
        char wqr[MAX_PATH] = {0};
        char motor[MAX_PATH] = {0};
        if (!select_firmware(base, sizeof(base), "Select DD1 base firmware") ||
            !select_firmware(wqr, sizeof(wqr), "Select WQR firmware") ||
            !select_firmware(motor, sizeof(motor), "Select motor firmware")) {
            return EXIT_SUCCESS;
        }
        return run_window(base, wqr, motor);
    }
    if (argument_count != 4) {
        fprintf(stderr, "usage: dd1-sim BASE_FIRMWARE.elf WQR_FIRMWARE.elf MOTOR_FIRMWARE.elf\n"
                        "       dd1-sim --headless ROUNDS BASE_FIRMWARE.elf WQR_FIRMWARE.elf "
                        "MOTOR_FIRMWARE.elf\n");
        return EXIT_FAILURE;
    }
    return run_window(arguments[1], arguments[2], arguments[3]);
}
