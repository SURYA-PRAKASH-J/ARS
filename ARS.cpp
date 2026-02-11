#include <windows.h>

HHOOK hook;

// Restart using Windows shutdown command (reliable, no privilege issues)
void restart()
{
    system("shutdown /r /t 0");
}

// Low-level keyboard hook
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION &&
        (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN))
    {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;

        // CTRL + ESC
        if (kb->vkCode == VK_ESCAPE &&
            (GetAsyncKeyState(VK_CONTROL) & 0x8000))
        {
            restart();
            return 1; // block Start menu
        }

        // SHIFT + HOME
        if (kb->vkCode == VK_HOME &&
            (GetAsyncKeyState(VK_SHIFT) & 0x8000))
        {
            restart();
            return 1;
        }

        // ALT + SPACE
        if (kb->vkCode == VK_SPACE &&
            (GetAsyncKeyState(VK_MENU) & 0x8000))
        {
            restart();
            return 1; // block window menu
        }
    }

    return CallNextHookEx(hook, nCode, wParam, lParam);
}

int main()
{
    // Install global keyboard hook
    hook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        KeyboardProc,
        GetModuleHandle(NULL),
        0
    );

    if (!hook)
    {
        MessageBox(NULL, "Hook installation failed", "Error", MB_OK);
        return 1;
    }

    // Message loop (keeps hook alive)
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hook);
    return 0;
}