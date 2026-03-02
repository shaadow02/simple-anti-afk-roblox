#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>

std::mutex mtx;

std::vector<HWND> GetRoblox() {
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

bool ForceFocusWithRetry(HWND hwnd, int maxRetries = 5) {
    int attempts = 0;
    DWORD lockTimeout = 0;
    SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &lockTimeout, 0);
    SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (PVOID)0, SPIF_SENDWININICHANGE);

    while (attempts < maxRetries) {
        if (GetForegroundWindow() == hwnd) return true;

        DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
        DWORD currentThread = GetCurrentThreadId();

        AttachThreadInput(currentThread, threadId, TRUE);
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
        BringWindowToTop(hwnd);
        SetActiveWindow(hwnd);
        SetFocus(hwnd);
        AttachThreadInput(currentThread, threadId, FALSE);

        Sleep(300);
        if (GetForegroundWindow() == hwnd) {
            SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (PVOID)(DWORD_PTR)lockTimeout, SPIF_SENDWININICHANGE);
            return true;
        }
        attempts++;
    }

    SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (PVOID)(DWORD_PTR)lockTimeout, SPIF_SENDWININICHANGE);
    return false;
}

void SendSpaceToWindow(HWND hwnd, int index) {
    std::lock_guard<std::mutex> lock(mtx);
    if (ForceFocusWithRetry(hwnd)) {
        std::cout << " [" << index << "] hwnd=" << hwnd << " [OK]\n";
        for (int i = 0; i < 2; i++) {
            keybd_event(VK_SPACE, MapVirtualKey(VK_SPACE, MAPVK_VK_TO_VSC), 0, 0);
            Sleep(100);
            keybd_event(VK_SPACE, MapVirtualKey(VK_SPACE, MAPVK_VK_TO_VSC), KEYEVENTF_KEYUP, 0);
            Sleep(200);
        }
    }
    else {
        std::cout << " [" << index << "] hwnd=" << hwnd << " [NO FOCUS]\n";
    }
}

bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

int main() {
    if (!IsRunningAsAdmin()) {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        ShellExecuteA(NULL, "runas", path, NULL, NULL, SW_SHOWNORMAL);
        return 0;
    }

    std::cout << "Anti-AFK for Roblox ( MULTI INSTANCE SUPPORT )\n";
    std::cout << "keyboard and mouse will be blocked during cycle\n";
    std::cout << "if u want to stop just close this app\n\n";
    std::cout << "Starting in 5 seconds\n\n";

    Sleep(5000);

    while (true) {
        auto windows = GetRoblox();

        if (windows.empty()) {
            std::cout << "[!] No RobloxPlayerBeta.exe\n";
        }
        else {
            std::cout << "[+] " << windows.size() << " instances:\n";

            BlockInput(TRUE);
            for (size_t i = 0; i < windows.size(); i++) {
                SendSpaceToWindow(windows[i], (int)i + 1);
                Sleep(800);
            }
            BlockInput(FALSE);

            std::cout << "[*] Done. Next in 10 minutes.\n\n";
        }

        std::this_thread::sleep_for(std::chrono::minutes(10));
    }

    return 0;
}
