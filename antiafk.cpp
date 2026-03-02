#include <windows.h>
#include <vector>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <tlhelp32.h>
#include <shellapi.h>

std::mutex mtx;

std::vector<HWND> GetRobloxBetaWindows() {
    std::vector<DWORD> pids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(snap, &pe)) {
            do {
                if (wcscmp(pe.szExeFile, L"RobloxPlayerBeta.exe") == 0) {
                    pids.push_back(pe.th32ProcessID);
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    if (pids.empty()) return {};

    struct SearchData {
        std::vector<DWORD> pids;
        std::vector<HWND> windows;
    } data;
    data.pids = pids;

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* d = reinterpret_cast<SearchData*>(lParam);
        if (!IsWindowVisible(hwnd)) return TRUE;
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        for (DWORD p : d->pids) {
            if (p == pid) {
                char title[256];
                GetWindowTextA(hwnd, title, sizeof(title));
                if (strlen(title) > 0) {
                    d->windows.push_back(hwnd);
                }
                break;
            }
        }
        return TRUE;
        }, reinterpret_cast<LPARAM>(&data));

    return data.windows;
}

void ForceFocus(HWND hwnd) {
    DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
    DWORD currentThread = GetCurrentThreadId();

    DWORD lockTimeout = 0;
    SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &lockTimeout, 0);
    SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (PVOID)0, SPIF_SENDWININICHANGE);

    AttachThreadInput(currentThread, threadId, TRUE);
    ShowWindow(hwnd, SW_RESTORE);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
    AttachThreadInput(currentThread, threadId, FALSE);

    SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (PVOID)(DWORD_PTR)lockTimeout, SPIF_SENDWININICHANGE);
}

void SendSpaceToWindow(HWND hwnd, int index) {
    std::lock_guard<std::mutex> lock(mtx);

    HWND prev = GetForegroundWindow();

    ForceFocus(hwnd);
    Sleep(400);

    HWND focused = GetForegroundWindow();
    std::cout << "  [" << index << "] hwnd=" << hwnd
        << " focused=" << focused
        << (focused == hwnd ? " [OK]" : " [NO FOCUS]") << "\n";

    keybd_event(VK_SPACE, MapVirtualKey(VK_SPACE, MAPVK_VK_TO_VSC), 0, 0);
    Sleep(100);
    keybd_event(VK_SPACE, MapVirtualKey(VK_SPACE, MAPVK_VK_TO_VSC), KEYEVENTF_KEYUP, 0);
    Sleep(300);

    keybd_event(VK_SPACE, MapVirtualKey(VK_SPACE, MAPVK_VK_TO_VSC), 0, 0);
    Sleep(100);
    keybd_event(VK_SPACE, MapVirtualKey(VK_SPACE, MAPVK_VK_TO_VSC), KEYEVENTF_KEYUP, 0);
    Sleep(200);

    if (prev && prev != hwnd) {
        DWORD prevThread = GetWindowThreadProcessId(prev, nullptr);
        DWORD currentThread = GetCurrentThreadId();
        AttachThreadInput(currentThread, prevThread, TRUE);
        SetForegroundWindow(prev);
        SetActiveWindow(prev);
        AttachThreadInput(currentThread, prevThread, FALSE);
    }
}

bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;

    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(
        &NtAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &adminGroup))
    {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin;
}


int main() {
    SetConsoleOutputCP(1250);
    if (!IsRunningAsAdmin()) {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);

        ShellExecuteA(NULL, "runas", path, NULL, NULL, SW_SHOWNORMAL);
        return 0;
    }
    std::cout << "Anti-AFK for Roblox ( MULTI INSTANCE SUPPORT )\n";
    std::cout << "dont click anything on your pc while afking\n";
    std::cout << "if u want to stop just close this app\n\n";
    std::cout << "Starting in 5 seconds\n\n";
    Sleep(5000);
    while (true) {
        auto windows = GetRobloxBetaWindows();

        if (windows.empty()) {
            std::cout << "[!] No RobloxPlayerBeta.exe\n";
        }
        else {
            std::cout << "[+] " << windows.size() << " instances:\n";
            for (size_t i = 0; i < windows.size(); i++) {
                SendSpaceToWindow(windows[i], (int)i + 1);
                Sleep(800);
            }
            std::cout << "[*] Done. Next in 10 minutes.\n\n";
        }

        std::this_thread::sleep_for(std::chrono::minutes(10));
    }

    return 0;
}
