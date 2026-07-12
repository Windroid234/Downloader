#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <string>
#include <thread>
#include <map>

#define ID_TRAY_APP_ICON    1001
#define ID_TRAY_EXIT        1002
#define WM_TRAYICON         (WM_USER + 1)
#define ID_EDIT_LINK        2001
#define ID_LIST_VIDEOS      2002
#define ID_CHECK_CLIPBOARD  2003
#define ID_BTN_SET_DIR      2004

#define MSG_DL_START        (WM_APP + 2)
#define MSG_DL_PROGRESS     (WM_APP + 3)
#define MSG_DL_FINISH       (WM_APP + 4)

HWND g_hwnd = NULL;
NOTIFYICONDATAW g_nid = {};
std::wstring g_saveDirectory;
bool g_autoClipboard = false;
WNDPROC g_OriginalEditProc;
std::map<int, std::wstring> g_titles;

int Scale(int x) {
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);
    return MulDiv(x, dpi, 96);
}

std::wstring GetAppDir() {
    PWSTR path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path))) {
        std::wstring dir = std::wstring(path) + L"\\YoutubeDownloaderTray";
        CoTaskMemFree(path);
        CreateDirectoryW(dir.c_str(), NULL);
        return dir;
    }
    return L"";
}

void LoadSettings(HWND hwnd) {
    bool dirLoaded = false;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\YoutubeDownloaderTray", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[MAX_PATH];
        DWORD bufSize = sizeof(buf);
        if (RegQueryValueExW(hKey, L"SaveDirectory", NULL, NULL, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
            g_saveDirectory = buf;
            dirLoaded = true;
        }
        DWORD val;
        DWORD valSize = sizeof(val);
        if (RegQueryValueExW(hKey, L"AutoClipboard", NULL, NULL, (LPBYTE)&val, &valSize) == ERROR_SUCCESS) {
            g_autoClipboard = (val != 0);
            HWND hCheck = GetDlgItem(hwnd, ID_CHECK_CLIPBOARD);
            SendMessageW(hCheck, BM_SETCHECK, g_autoClipboard ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        RegCloseKey(hKey);
    }
    
    if (!dirLoaded) {
        PWSTR path = NULL;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, 0, NULL, &path))) {
            g_saveDirectory = path;
            CoTaskMemFree(path);
        }
    }

    std::wstring historyFile = GetAppDir() + L"\\history.txt";
    HANDLE hFile = CreateFileW(historyFile.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD size = GetFileSize(hFile, NULL);
        if (size > 0) {
            char* buf = new char[size + 1];
            DWORD read;
            if (ReadFile(hFile, buf, size, &read, NULL)) {
                buf[read] = 0;
                int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, -1, NULL, 0);
                std::wstring wstr(wlen, 0);
                MultiByteToWideChar(CP_UTF8, 0, buf, -1, &wstr[0], wlen);
                
                HWND hList = GetDlgItem(hwnd, ID_LIST_VIDEOS);
                SendMessageW(hList, WM_SETREDRAW, FALSE, 0);
                
                size_t start = 0;
                while (start < wlen - 1) {
                    size_t end = wstr.find(L'\n', start);
                    if (end == std::wstring::npos) end = wlen - 1;
                    std::wstring line = wstr.substr(start, end - start);
                    if (!line.empty() && line.back() == L'\r') line.pop_back();
                    if (!line.empty() && line[0] != L'\0') {
                        int index = SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
                        g_titles[index] = line;
                    }
                    start = end + 1;
                }
                SendMessageW(hList, WM_SETREDRAW, TRUE, 0);
            }
            delete[] buf;
        }
        CloseHandle(hFile);
    }
}

void SaveSettings(HWND hwnd) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\YoutubeDownloaderTray", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        if (!g_saveDirectory.empty()) {
            RegSetValueExW(hKey, L"SaveDirectory", 0, REG_SZ, (const BYTE*)g_saveDirectory.c_str(), (g_saveDirectory.length() + 1) * sizeof(wchar_t));
        }
        DWORD val = g_autoClipboard ? 1 : 0;
        RegSetValueExW(hKey, L"AutoClipboard", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }
    
    std::wstring historyFile = GetAppDir() + L"\\history.txt";
    HANDLE hFile = CreateFileW(historyFile.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        HWND hList = GetDlgItem(hwnd, ID_LIST_VIDEOS);
        int count = SendMessageW(hList, LB_GETCOUNT, 0, 0);
        for (int i = 0; i < count; ++i) {
            int len = SendMessageW(hList, LB_GETTEXTLEN, i, 0);
            if (len > 0) {
                wchar_t* wbuf = new wchar_t[len + 1];
                SendMessageW(hList, LB_GETTEXT, i, (LPARAM)wbuf);
                std::wstring line = wbuf;
                line += L"\r\n";
                int mlen = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, NULL, 0, NULL, NULL);
                char* mbuf = new char[mlen];
                WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, mbuf, mlen, NULL, NULL);
                DWORD written;
                WriteFile(hFile, mbuf, mlen - 1, &written, NULL);
                delete[] mbuf;
                delete[] wbuf;
            }
        }
        CloseHandle(hFile);
    }
}

void SelectSaveDirectory(HWND hwnd) {
    IFileDialog* pfd = NULL;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
        DWORD dwOptions;
        if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
            pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
        }
        if (SUCCEEDED(pfd->Show(hwnd))) {
            IShellItem* psi;
            if (SUCCEEDED(pfd->GetResult(&psi))) {
                PWSTR path;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    g_saveDirectory = path;
                    CoTaskMemFree(path);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }
}

void ToggleWindowVisibility() {
    if (IsWindowVisible(g_hwnd)) {
        ShowWindow(g_hwnd, SW_HIDE);
    } else {
        RECT workArea;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        int width = Scale(350);
        int height = Scale(250);
        int x = workArea.right - width - Scale(10);
        int y = workArea.bottom - height - Scale(10);
        SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);
        SetForegroundWindow(g_hwnd);
    }
}

void DownloadVideo(std::wstring url, int index) {
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return;
    
    std::wstring cmd = L"cmd.exe /c yt-dlp.exe --newline -o \"%(title)s.%(ext)s\" -P \"" + g_saveDirectory + L"\" \"" + url + L"\"";
    
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    PROCESS_INFORMATION pi;
    
    if (CreateProcessW(NULL, &cmd[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWritePipe);
        
        std::wstring title = L"Unknown";
        std::wstring finalPath;
        char buffer[1024];
        DWORD bytesRead;
        std::string line;
        
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            for (DWORD i = 0; i < bytesRead; ++i) {
                if (buffer[i] == '\n' || buffer[i] == '\r') {
                    if (!line.empty()) {
                        if (line.find("[download] Destination:") != std::string::npos) {
                            std::string path = line.substr(line.find(":") + 2);
                            while (!path.empty() && path[0] == ' ') path.erase(0, 1);
                            int sz = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
                            std::wstring wpath(sz, 0);
                            MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], sz);
                            finalPath = wpath.c_str();
                            size_t lastSlash = finalPath.find_last_of(L"\\/");
                            if (lastSlash != std::wstring::npos) title = finalPath.substr(lastSlash + 1);
                            std::wstring* pTitle = new std::wstring(title);
                            SendMessageW(g_hwnd, MSG_DL_START, index, (LPARAM)pTitle);
                        } else if (line.find("[download]") != std::string::npos && line.find("%") != std::string::npos) {
                            size_t pctPos = line.find("%");
                            size_t startPos = pctPos;
                            while (startPos > 0 && (isdigit(line[startPos - 1]) || line[startPos - 1] == '.' || line[startPos - 1] == ' ')) startPos--;
                            std::string pctStr = line.substr(startPos, pctPos - startPos);
                            int pct = (int)atof(pctStr.c_str());
                            SendMessageW(g_hwnd, MSG_DL_PROGRESS, index, pct);
                        } else if (line.find("has already been downloaded") != std::string::npos) {
                            size_t start = line.find("[download] ") + 11;
                            size_t end = line.find(" has already");
                            if (start != std::string::npos && end != std::string::npos) {
                                std::string path = line.substr(start, end - start);
                                int sz = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
                                std::wstring wpath(sz, 0);
                                MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], sz);
                                finalPath = wpath.c_str();
                                size_t lastSlash = finalPath.find_last_of(L"\\/");
                                if (lastSlash != std::wstring::npos) title = finalPath.substr(lastSlash + 1);
                                std::wstring* pTitle = new std::wstring(title);
                                SendMessageW(g_hwnd, MSG_DL_START, index, (LPARAM)pTitle);
                                SendMessageW(g_hwnd, MSG_DL_PROGRESS, index, 100);
                            }
                        }
                        line.clear();
                    }
                } else {
                    line += buffer[i];
                }
            }
        }
        
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);
        
        std::wstring* pPath = new std::wstring(finalPath);
        SendMessageW(g_hwnd, MSG_DL_FINISH, index, (LPARAM)pPath);
    } else {
        CloseHandle(hWritePipe);
        CloseHandle(hReadPipe);
    }
}

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        wchar_t buf[1024];
        GetWindowTextW(hwnd, buf, 1024);
        if (wcslen(buf) > 0) {
            SendMessageW(GetParent(hwnd), WM_APP + 1, 0, (LPARAM)buf);
            SetWindowTextW(hwnd, L"");
        }
        return 0;
    }
    return CallWindowProcW(g_OriginalEditProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hwnd = hwnd;
            
            HFONT hFont = CreateFontW(Scale(-14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HFONT hBtnFont = CreateFontW(Scale(-12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, Scale(10), Scale(10), Scale(330), Scale(25), hwnd, (HMENU)ID_EDIT_LINK, NULL, NULL);
            SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(hEdit, EM_SETCUEBANNER, FALSE, (LPARAM)L"Paste YouTube or Instagram link here...");
            g_OriginalEditProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

            HWND hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", NULL, WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL, Scale(10), Scale(45), Scale(330), Scale(160), hwnd, (HMENU)ID_LIST_VIDEOS, NULL, NULL);
            SendMessageW(hList, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hCheck = CreateWindowExW(0, L"BUTTON", L"Copy to clipboard", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_VCENTER, Scale(10), Scale(215), Scale(210), Scale(25), hwnd, (HMENU)ID_CHECK_CLIPBOARD, NULL, NULL);
            SendMessageW(hCheck, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hBtn = CreateWindowExW(0, L"BUTTON", L"Set Save Directory", WS_CHILD | WS_VISIBLE, Scale(225), Scale(215), Scale(115), Scale(25), hwnd, (HMENU)ID_BTN_SET_DIR, NULL, NULL);
            SendMessageW(hBtn, WM_SETFONT, (WPARAM)hBtnFont, TRUE);
            LoadSettings(hwnd);
            break;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == ID_BTN_SET_DIR) {
                SelectSaveDirectory(hwnd);
            } else if (wmId == ID_CHECK_CLIPBOARD) {
                g_autoClipboard = (SendMessageW((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            break;
        }
        case WM_APP + 1: {
            std::wstring url = (const wchar_t*)lParam;
            HWND hList = GetDlgItem(hwnd, ID_LIST_VIDEOS);
            int index = SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)L"Starting download...");
            std::thread(DownloadVideo, url, index).detach();
            break;
        }
        case MSG_DL_START: {
            int index = (int)wParam;
            std::wstring* pTitle = (std::wstring*)lParam;
            g_titles[index] = *pTitle;
            std::wstring text = L"[0%] " + *pTitle;
            HWND hList = GetDlgItem(hwnd, ID_LIST_VIDEOS);
            SendMessageW(hList, WM_SETREDRAW, FALSE, 0);
            SendMessageW(hList, LB_DELETESTRING, index, 0);
            SendMessageW(hList, LB_INSERTSTRING, index, (LPARAM)text.c_str());
            SendMessageW(hList, WM_SETREDRAW, TRUE, 0);
            delete pTitle;
            break;
        }
        case MSG_DL_PROGRESS: {
            int index = (int)wParam;
            int pct = (int)lParam;
            std::wstring text = L"[" + std::to_wstring(pct) + L"%] " + g_titles[index];
            HWND hList = GetDlgItem(hwnd, ID_LIST_VIDEOS);
            SendMessageW(hList, WM_SETREDRAW, FALSE, 0);
            SendMessageW(hList, LB_DELETESTRING, index, 0);
            SendMessageW(hList, LB_INSERTSTRING, index, (LPARAM)text.c_str());
            SendMessageW(hList, WM_SETREDRAW, TRUE, 0);
            break;
        }
        case MSG_DL_FINISH: {
            int index = (int)wParam;
            std::wstring* pPath = (std::wstring*)lParam;
            std::wstring text = g_titles[index];
            if (text.empty()) text = L"Download Complete";
            HWND hList = GetDlgItem(hwnd, ID_LIST_VIDEOS);
            SendMessageW(hList, WM_SETREDRAW, FALSE, 0);
            SendMessageW(hList, LB_DELETESTRING, index, 0);
            SendMessageW(hList, LB_INSERTSTRING, index, (LPARAM)text.c_str());
            SendMessageW(hList, WM_SETREDRAW, TRUE, 0);
            
            if (g_autoClipboard && !pPath->empty()) {
                if (OpenClipboard(hwnd)) {
                    EmptyClipboard();
                    size_t size = sizeof(DROPFILES) + (pPath->length() + 2) * sizeof(wchar_t);
                    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, size);
                    if (hg) {
                        DROPFILES* df = (DROPFILES*)GlobalLock(hg);
                        df->pFiles = sizeof(DROPFILES);
                        df->fWide = TRUE;
                        wchar_t* dst = (wchar_t*)((char*)df + sizeof(DROPFILES));
                        wcscpy_s(dst, pPath->length() + 1, pPath->c_str());
                        GlobalUnlock(hg);
                        SetClipboardData(CF_HDROP, hg);
                    }
                    CloseClipboard();
                }
            }
            delete pPath;
            break;
        }
        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONUP) {
                ToggleWindowVisibility();
            } else if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();
                InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit");
                SetForegroundWindow(hwnd);
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
                if (cmd == ID_TRAY_EXIT) {
                    PostQuitMessage(0);
                }
            }
            break;
        }
        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE) {
                ShowWindow(hwnd, SW_HIDE);
            }
            break;
        }
        case WM_DESTROY: {
            SaveSettings(hwnd);
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    SetProcessDPIAware();
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wcex.lpszClassName = L"YTDL_Tray_App";

    RegisterClassExW(&wcex);

    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"YTDL_Tray_App", L"Downloader", WS_POPUP | WS_BORDER, 0, 0, Scale(350), Scale(250), NULL, NULL, hInstance, NULL);

    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = ID_TRAY_APP_ICON;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"Video Downloader");

    Shell_NotifyIconW(NIM_ADD, &g_nid);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}
