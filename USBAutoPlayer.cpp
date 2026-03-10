// =============================================================================
// USBAutoPlayer.cpp  —  v2.0
//
// Automatically plays MP3 files from any inserted USB flash drive.
// Uses Windows MCI (winmm.dll) for audio — no external player required.
// Runs as a Windows 11 system tray application.
//
// All dependencies are built-in Windows DLLs (always present on Windows):
//   winmm.dll, shell32.dll, user32.dll, gdi32.dll, kernel32.dll
//
// Build (MSVC — Developer Command Prompt):
//   cl USBAutoPlayer.cpp /O2 /W4 /EHsc /std:c++17
//       /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN
//       /link winmm.lib shell32.lib user32.lib gdi32.lib kernel32.lib
//       /SUBSYSTEM:WINDOWS /OUT:USBAutoPlayer.exe
//
// Build (MinGW / MSYS2 — any terminal with mingw64/bin in PATH):
//   g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN
//       -o USBAutoPlayer.exe USBAutoPlayer.cpp
//       -lwinmm -lshell32 -luser32 -lgdi32 -mwindows
// =============================================================================

#include <windows.h>
#include <shellapi.h>   // Shell_NotifyIcon
#include <dbt.h>        // WM_DEVICECHANGE, DEV_BROADCAST_VOLUME
#include <mmsystem.h>   // mciSendStringW, MM_MCINOTIFY
#include <winioctl.h>   // IOCTL_STORAGE_EJECT_MEDIA

#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <random>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define APP_NAME          L"USB AutoPlayer"
#define WM_TRAYICON       (WM_USER + 1)
#define IDI_TRAY          1

// Menu IDs
#define ID_TRAY_PLAYPAUSE 1001
#define ID_TRAY_NEXT      1002
#define ID_TRAY_PREV      1003
#define ID_TRAY_STOP      1004
#define ID_TRAY_SHUFFLE   1005
#define ID_TRAY_ABOUT     1006
#define ID_TRAY_EXIT      1007
#define ID_TRAY_REPEAT    1008
#define ID_TRAY_AUTORUN   1009
#define ID_TRAY_EJECT     1010
#define ID_PLAYLIST_BASE  2000  // menu IDs 2000+ = playlist track items

#define DRIVE_TIMER_BASE  100   // timers 100-125 = drive letters A-Z

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static HWND            g_hWnd         = nullptr;
static HINSTANCE       g_hInstance    = nullptr;
static NOTIFYICONDATAW g_nid          = {};
static HDEVNOTIFY      g_hDevNotify   = nullptr;

static std::vector<std::wstring> g_playlist;
static int             g_currentTrack = -1;
static bool            g_isPaused     = false;
static bool            g_shuffleOn    = false;
static bool            g_repeatAll    = false;
static bool            g_mciOpen      = false;
static wchar_t         g_activeDriveLetter = 0;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void AddTrayIcon(HWND hWnd);
void RemoveTrayIcon();
void UpdateTrayTip();
void ShowBalloon(const wchar_t* title, const wchar_t* text, DWORD icon = NIIF_INFO);
void ShowContextMenu(HWND hWnd);
void RegisterForDeviceNotifications(HWND hWnd);
void OnDriveInserted(wchar_t driveLetter);
void RecursiveScan(const std::wstring& dir, std::vector<std::wstring>& out);
std::vector<std::wstring> ScanForMp3s(const std::wstring& root);
void StartPlaylist(std::vector<std::wstring> tracks);
void MciClose();
void PlayTrack(int index);
void StopPlayback();
void PauseResume();
bool IsAutorunEnabled();
void ToggleAutorun();
void NextTrack();
void PrevTrack();
void EjectUSB();
std::wstring TrackTitle(int index);
std::wstring GetTempDir();
void Log(const std::wstring& msg);

// ---------------------------------------------------------------------------
// WinMain
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    g_hInstance = hInstance;

    // Single-instance guard
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"USBAutoPlayer_v2_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxW(nullptr,
            L"USB AutoPlayer is already running.\nCheck the system tray.",
            APP_NAME, MB_ICONINFORMATION);
        return 0;
    }

    // Register window class
    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"USBAutoPlayerClass";
    RegisterClassExW(&wc);

    // Real top-level window (hidden) — HWND_MESSAGE blocks WM_DEVICECHANGE,
    // so we use a normal window and simply never show it.
    g_hWnd = CreateWindowExW(0,
        L"USBAutoPlayerClass", APP_NAME,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_hWnd)
    {
        MessageBoxW(nullptr, L"Failed to create window.", APP_NAME, MB_ICONERROR);
        return 1;
    }

    // Keep the window hidden — it only exists to receive messages
    ShowWindow(g_hWnd, SW_HIDE);

    AddTrayIcon(g_hWnd);
    RegisterForDeviceNotifications(g_hWnd);
    Log(L"USB AutoPlayer v2.0 started. Waiting for USB drives...");

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    StopPlayback();
    RemoveTrayIcon();
    if (g_hDevNotify) UnregisterDeviceNotification(g_hDevNotify);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    return static_cast<int>(msg.wParam);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    // ---- Tray icon mouse events ----
    case WM_TRAYICON:
    {
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU)
        {
            ShowContextMenu(hWnd);
        }
        else if (lParam == WM_LBUTTONDBLCLK)
        {
            if (g_currentTrack >= 0 && !g_playlist.empty())
            {
                wchar_t info[256];
                swprintf_s(info,
                    L"Track %d / %d\n%s",
                    g_currentTrack + 1,
                    static_cast<int>(g_playlist.size()),
                    TrackTitle(g_currentTrack).c_str());
                ShowBalloon(g_isPaused ? L"Paused" : L"Now Playing", info);
            }
            else
            {
                ShowBalloon(APP_NAME, L"Idle — insert a USB drive with MP3 files.");
            }
        }
        break;
    }

    // ---- MCI track-finished notification ----
    case MM_MCINOTIFY:
    {
        // MCI_NOTIFY_SUCCESSFUL means the track played to the end
        if (wParam == MCI_NOTIFY_SUCCESSFUL && g_currentTrack >= 0)
        {
            int next = g_currentTrack + 1;
            if (next < static_cast<int>(g_playlist.size()))
            {
                PlayTrack(next);
            }
            else if (g_repeatAll)
            {
                PlayTrack(0);
                Log(L"Repeat All — restarting playlist.");
            }
            else
            {
                // End of playlist
                MciClose();
                g_currentTrack = -1;
                UpdateTrayTip();
                ShowBalloon(APP_NAME, L"Playlist finished.");
                Log(L"Playlist finished.");
            }
        }
        break;
    }

    // ---- USB device arrival ----
    case WM_DEVICECHANGE:
    {
        if (wParam == DBT_DEVICEARRIVAL)
        {
            auto* pHdr = reinterpret_cast<DEV_BROADCAST_HDR*>(lParam);
            if (pHdr && pHdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
            {
                auto* pVol = reinterpret_cast<DEV_BROADCAST_VOLUME*>(lParam);
                DWORD mask = pVol->dbcv_unitmask;
                for (int i = 0; i < 26; ++i)
                    if (mask & (1u << i))
                        // 1.8s delay to let the filesystem fully mount
                        SetTimer(hWnd, DRIVE_TIMER_BASE + i, 1800, nullptr);
            }
        }
        break;
    }

    // ---- Drive mount delay ----
    case WM_TIMER:
    {
        if (wParam >= DRIVE_TIMER_BASE && wParam < DRIVE_TIMER_BASE + 26)
        {
            KillTimer(hWnd, wParam);
            OnDriveInserted(L'A' + static_cast<wchar_t>(wParam - DRIVE_TIMER_BASE));
        }
        break;
    }

    // ---- Context menu commands ----
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case ID_TRAY_PLAYPAUSE: PauseResume();  break;
        case ID_TRAY_NEXT:      NextTrack();    break;
        case ID_TRAY_PREV:      PrevTrack();    break;
        case ID_TRAY_STOP:      StopPlayback(); break;
        case ID_TRAY_EJECT:     EjectUSB();     break;
        case ID_TRAY_SHUFFLE:
            g_shuffleOn = !g_shuffleOn;
            Log(g_shuffleOn ? L"Shuffle ON." : L"Shuffle OFF.");
            ShowBalloon(APP_NAME,
                g_shuffleOn ? L"Shuffle enabled." : L"Shuffle disabled.");
            break;
        case ID_TRAY_REPEAT:
            g_repeatAll = !g_repeatAll;
            Log(g_repeatAll ? L"Repeat All ON." : L"Repeat All OFF.");
            ShowBalloon(APP_NAME,
                g_repeatAll ? L"Repeat All enabled." : L"Repeat All disabled.");
            break;
        case ID_TRAY_AUTORUN:
            ToggleAutorun();
            if (IsAutorunEnabled())
            {
                Log(L"Start with Windows ON.");
                ShowBalloon(APP_NAME, L"Start with Windows enabled.");
            }
            else
            {
                Log(L"Start with Windows OFF.");
                ShowBalloon(APP_NAME, L"Start with Windows disabled.");
            }
            break;
        case ID_TRAY_ABOUT:
            MessageBoxW(hWnd,
                L"USB AutoPlayer v2.0\n\n"
                L"Plays MP3 files automatically from any USB drive.\n"
                L"No media player required — built-in audio engine.\n\n"
                L"  Double-click tray icon  =  track info\n"
                L"  Right-click tray icon   =  controls",
                APP_NAME, MB_ICONINFORMATION);
            break;
        case ID_TRAY_EXIT:
            StopPlayback();
            PostQuitMessage(0);
            break;
        default:
        {
            WORD id = LOWORD(wParam);
            if (id >= ID_PLAYLIST_BASE && id < ID_PLAYLIST_BASE + static_cast<int>(g_playlist.size()))
                PlayTrack(id - ID_PLAYLIST_BASE);
            break;
        }
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Tray icon helpers
// ---------------------------------------------------------------------------
void AddTrayIcon(HWND hWnd)
{
    g_nid                  = {};
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = hWnd;
    g_nid.uID              = IDI_TRAY;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = LoadIconW(g_hInstance, MAKEINTRESOURCEW(1));
    wcscpy_s(g_nid.szTip, L"USB AutoPlayer — waiting for USB drive");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void RemoveTrayIcon()
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

void UpdateTrayTip()
{
    g_nid.uFlags = NIF_TIP;
    if (g_currentTrack >= 0 && !g_playlist.empty())
    {
        wchar_t tip[128];
        swprintf_s(tip, L"%s  [%d/%d]%s%s",
            TrackTitle(g_currentTrack).c_str(),
            g_currentTrack + 1,
            static_cast<int>(g_playlist.size()),
            g_isPaused ? L" (paused)" : L"",
            g_repeatAll ? L" \u27F3" : L"");
        wcscpy_s(g_nid.szTip, tip);
    }
    else
    {
        wcscpy_s(g_nid.szTip, L"USB AutoPlayer — idle");
    }
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void ShowBalloon(const wchar_t* title, const wchar_t* text, DWORD icon)
{
    g_nid.uFlags      = NIF_INFO;
    g_nid.dwInfoFlags = icon;
    g_nid.uTimeout    = 4000;
    wcscpy_s(g_nid.szInfoTitle, title);
    wcscpy_s(g_nid.szInfo, text);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// ---------------------------------------------------------------------------
// Autorun (Start with Windows) helpers
// ---------------------------------------------------------------------------
static const wchar_t* AUTORUN_KEY = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* AUTORUN_VALUE = L"USBGroove";

bool IsAutorunEnabled()
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTORUN_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    bool exists = (RegQueryValueExW(hKey, AUTORUN_VALUE, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return exists;
}

void ToggleAutorun()
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTORUN_KEY, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return;

    if (IsAutorunEnabled())
    {
        RegDeleteValueW(hKey, AUTORUN_VALUE);
    }
    else
    {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        RegSetValueExW(hKey, AUTORUN_VALUE, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(exePath),
            static_cast<DWORD>((wcslen(exePath) + 1) * sizeof(wchar_t)));
    }
    RegCloseKey(hKey);
}

void ShowContextMenu(HWND hWnd)
{
    const bool active = (g_currentTrack >= 0 && !g_playlist.empty());

    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();

    UINT pos = 0;

    // "Now playing" header (informational, greyed out)
    if (active)
    {
        wchar_t header[128];
        swprintf_s(header, L"  %s  [%d/%d]",
            TrackTitle(g_currentTrack).c_str(),
            g_currentTrack + 1,
            static_cast<int>(g_playlist.size()));
        InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_STRING | MF_GRAYED, 0, header);
        InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    }

    InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_STRING,
        ID_TRAY_PLAYPAUSE, g_isPaused ? L"Resume" : L"Pause");
    InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_STRING, ID_TRAY_PREV, L"Previous track");
    InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_STRING, ID_TRAY_NEXT, L"Next track");
    InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_STRING, ID_TRAY_STOP, L"Stop");
    InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_STRING, ID_TRAY_EJECT, L"Eject USB");
    InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);

    UINT shuffleFlags = MF_BYPOSITION | MF_STRING | (g_shuffleOn ? MF_CHECKED : 0);
    InsertMenuW(hMenu, pos++, shuffleFlags, ID_TRAY_SHUFFLE, L"Shuffle");
    UINT repeatFlags = MF_BYPOSITION | MF_STRING | (g_repeatAll ? MF_CHECKED : 0);
    InsertMenuW(hMenu, pos++, repeatFlags, ID_TRAY_REPEAT, L"Repeat All");
    UINT autorunFlags = MF_BYPOSITION | MF_STRING | (IsAutorunEnabled() ? MF_CHECKED : 0);
    InsertMenuW(hMenu, pos++, autorunFlags, ID_TRAY_AUTORUN, L"Start with Windows");
    InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);

    // Playlist submenu — list all tracks, current track gets a bullet
    if (!g_playlist.empty())
    {
        HMENU hPlaylist = CreatePopupMenu();
        int limit = static_cast<int>(g_playlist.size());
        if (limit > 50) limit = 50;  // cap submenu length
        for (int i = 0; i < limit; ++i)
        {
            UINT flags = MF_STRING;
            if (i == g_currentTrack)
                flags |= MF_CHECKED;
            std::wstring label = std::to_wstring(i + 1) + L". " + TrackTitle(i);
            AppendMenuW(hPlaylist, flags, ID_PLAYLIST_BASE + i, label.c_str());
        }
        if (static_cast<int>(g_playlist.size()) > 50)
            AppendMenuW(hPlaylist, MF_STRING | MF_GRAYED, 0,
                (L"... and " + std::to_wstring(g_playlist.size() - 50) + L" more").c_str());
        InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_POPUP,
            reinterpret_cast<UINT_PTR>(hPlaylist), L"Playlist");
    }

    InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_STRING, ID_TRAY_ABOUT, L"About");
    InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT,  L"Exit");

    if (!active)
    {
        EnableMenuItem(hMenu, ID_TRAY_PLAYPAUSE, MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem(hMenu, ID_TRAY_PREV,      MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem(hMenu, ID_TRAY_NEXT,      MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem(hMenu, ID_TRAY_STOP,      MF_BYCOMMAND | MF_GRAYED);
    }
    if (g_activeDriveLetter == 0)
        EnableMenuItem(hMenu, ID_TRAY_EJECT, MF_BYCOMMAND | MF_GRAYED);

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        pt.x, pt.y, 0, hWnd, nullptr);
    DestroyMenu(hMenu);
}

// ---------------------------------------------------------------------------
// Device notifications
// ---------------------------------------------------------------------------
void RegisterForDeviceNotifications(HWND hWnd)
{
    DEV_BROADCAST_DEVICEINTERFACE_W dbi = {};
    dbi.dbcc_size       = sizeof(dbi);
    dbi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    g_hDevNotify = RegisterDeviceNotificationW(
        hWnd, &dbi,
        DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
}

// ---------------------------------------------------------------------------
// USB drive handler
// ---------------------------------------------------------------------------
void OnDriveInserted(wchar_t driveLetter)
{
    wchar_t root[8];
    swprintf_s(root, L"%c:\\", driveLetter);

    UINT driveType = GetDriveTypeW(root);
    // Skip only network drives and RAM disks; allow removable, fixed USB, and optical
    if (driveType == DRIVE_REMOTE || driveType == DRIVE_RAMDISK || driveType == DRIVE_NO_ROOT_DIR)
    {
        Log(std::wstring(L"Drive ") + driveLetter + L": skipped (network/ramdisk).");
        return;
    }

    Log(std::wstring(L"USB drive detected: ") + root);

    auto mp3s = ScanForMp3s(root);
    if (mp3s.empty())
    {
        wchar_t msg[64];
        swprintf_s(msg, L"No MP3 files found on drive %c:", driveLetter);
        ShowBalloon(APP_NAME, msg, NIIF_WARNING);
        Log(msg);
        return;
    }

    wchar_t info[128];
    swprintf_s(info, L"Found %zu MP3 file(s) on drive %c: — starting playback.",
        mp3s.size(), driveLetter);
    ShowBalloon(APP_NAME, info);
    Log(info);

    g_activeDriveLetter = driveLetter;
    StartPlaylist(std::move(mp3s));
}

// ---------------------------------------------------------------------------
// MP3 file scanner
// ---------------------------------------------------------------------------
void RecursiveScan(const std::wstring& dir, std::vector<std::wstring>& out)
{
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW((dir + L"*").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;

        std::wstring full = dir + name;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            RecursiveScan(full + L"\\", out);
        }
        else
        {
            // Case-insensitive .mp3 check
            std::wstring low = name;
            std::transform(low.begin(), low.end(), low.begin(), ::towlower);
            if (low.size() >= 4 && low.compare(low.size() - 4, 4, L".mp3") == 0)
                out.push_back(full);
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

std::vector<std::wstring> ScanForMp3s(const std::wstring& root)
{
    std::vector<std::wstring> results;
    RecursiveScan(root, results);
    std::sort(results.begin(), results.end());
    return results;
}

// ---------------------------------------------------------------------------
// Playback engine (Windows MCI — winmm.dll)
// ---------------------------------------------------------------------------
void StartPlaylist(std::vector<std::wstring> tracks)
{
    StopPlayback();
    g_playlist = std::move(tracks);

    if (g_shuffleOn)
    {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::shuffle(g_playlist.begin(), g_playlist.end(), rng);
    }

    PlayTrack(0);
}

void MciClose()
{
    if (g_mciOpen)
    {
        mciSendStringW(L"close player", nullptr, 0, nullptr);
        g_mciOpen = false;
    }
}

void PlayTrack(int index)
{
    if (index < 0 || index >= static_cast<int>(g_playlist.size())) return;

    MciClose();

    // MCI requires the path wrapped in quotes if it contains spaces
    std::wstring openCmd = L"open \"" + g_playlist[index] + L"\" type mpegvideo alias player";
    MCIERROR err = mciSendStringW(openCmd.c_str(), nullptr, 0, nullptr);

    if (err != 0)
    {
        wchar_t errBuf[256] = {};
        mciGetErrorStringW(err, errBuf, 256);
        Log(std::wstring(L"MCI open error (") + errBuf + L"): " + g_playlist[index]);

        // Skip bad file and try next
        int next = index + 1;
        if (next < static_cast<int>(g_playlist.size()))
            PlayTrack(next);
        return;
    }

    g_mciOpen = true;

    // "notify" causes MM_MCINOTIFY to be sent to g_hWnd when playback ends
    mciSendStringW(L"play player notify", nullptr, 0, g_hWnd);

    g_currentTrack = index;
    g_isPaused     = false;

    UpdateTrayTip();

    Log(L"Playing [" + std::to_wstring(index + 1) + L"/" +
        std::to_wstring(g_playlist.size()) + L"]: " + g_playlist[index]);
}

void StopPlayback()
{
    MciClose();
    g_playlist.clear();
    g_currentTrack = -1;
    g_isPaused     = false;
    g_activeDriveLetter = 0;
    UpdateTrayTip();
    Log(L"Playback stopped.");
}

void PauseResume()
{
    if (g_currentTrack < 0) return;

    if (!g_isPaused)
    {
        mciSendStringW(L"pause player", nullptr, 0, nullptr);
        g_isPaused = true;
        Log(L"Paused.");
    }
    else
    {
        // Resume — re-attach notify so MM_MCINOTIFY still fires at end
        mciSendStringW(L"play player notify", nullptr, 0, g_hWnd);
        g_isPaused = false;
        Log(L"Resumed.");
    }
    UpdateTrayTip();
}

void NextTrack()
{
    if (g_playlist.empty()) return;
    int next = g_currentTrack + 1;
    if (next < static_cast<int>(g_playlist.size()))
        PlayTrack(next);
    else
        ShowBalloon(APP_NAME, L"Already at the last track.", NIIF_INFO);
}

void PrevTrack()
{
    if (g_playlist.empty()) return;
    int prev = g_currentTrack - 1;
    if (prev >= 0)
        PlayTrack(prev);
    else
        ShowBalloon(APP_NAME, L"Already at the first track.", NIIF_INFO);
}

void EjectUSB()
{
    if (g_activeDriveLetter == 0)
    {
        ShowBalloon(APP_NAME, L"No USB drive to eject.", NIIF_WARNING);
        return;
    }

    wchar_t driveLetter = g_activeDriveLetter;

    // Stop playback first to release MCI file handles on the drive
    StopPlayback();

    // Open the drive volume as \\.\X:
    wchar_t devicePath[8];
    swprintf_s(devicePath, L"\\\\.\\%c:", driveLetter);

    HANDLE hDrive = CreateFileW(devicePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);

    if (hDrive == INVALID_HANDLE_VALUE)
    {
        // Retry with read-only access
        hDrive = CreateFileW(devicePath,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, 0, nullptr);
    }

    if (hDrive == INVALID_HANDLE_VALUE)
    {
        wchar_t msg[64];
        swprintf_s(msg, L"Failed to eject drive %c: — could not open device.", driveLetter);
        ShowBalloon(APP_NAME, msg, NIIF_ERROR);
        Log(msg);
        return;
    }

    DWORD bytesReturned = 0;

    // Lock the volume (best effort — not fatal if it fails)
    DeviceIoControl(hDrive, FSCTL_LOCK_VOLUME,
        nullptr, 0, nullptr, 0, &bytesReturned, nullptr);

    // Dismount the volume
    DeviceIoControl(hDrive, FSCTL_DISMOUNT_VOLUME,
        nullptr, 0, nullptr, 0, &bytesReturned, nullptr);

    // Eject the media
    BOOL ejected = DeviceIoControl(hDrive, IOCTL_STORAGE_EJECT_MEDIA,
        nullptr, 0, nullptr, 0, &bytesReturned, nullptr);

    CloseHandle(hDrive);

    if (ejected)
    {
        wchar_t msg[64];
        swprintf_s(msg, L"Drive %c: safely ejected.", driveLetter);
        ShowBalloon(APP_NAME, msg);
        Log(msg);
    }
    else
    {
        wchar_t msg[64];
        swprintf_s(msg, L"Failed to eject drive %c: — it may be in use.", driveLetter);
        ShowBalloon(APP_NAME, msg, NIIF_ERROR);
        Log(msg);
    }
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
std::wstring TrackTitle(int index)
{
    if (index < 0 || index >= static_cast<int>(g_playlist.size()))
        return L"(none)";

    const std::wstring& path = g_playlist[index];
    auto slash = path.find_last_of(L"\\/");
    std::wstring name = (slash != std::wstring::npos) ? path.substr(slash + 1) : path;

    // Strip .mp3
    if (name.size() > 4) name = name.substr(0, name.size() - 4);

    // Truncate for tray tip
    if (name.size() > 60) name = name.substr(0, 57) + L"...";

    return name;
}

std::wstring GetTempDir()
{
    wchar_t buf[MAX_PATH];
    GetTempPathW(MAX_PATH, buf);
    return buf;
}

void Log(const std::wstring& msg)
{
    std::wstring logPath = GetTempDir() + L"USBAutoPlayer.log";
    std::ofstream ofs(logPath.c_str(), std::ios::app | std::ios::binary);
    if (!ofs) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    char ts[32];
    snprintf(ts, sizeof(ts), "[%04d-%02d-%02d %02d:%02d:%02d] ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    int len = WideCharToMultiByte(CP_UTF8, 0, msg.c_str(), -1,
        nullptr, 0, nullptr, nullptr);
    std::string narrow(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, msg.c_str(), -1,
        &narrow[0], len, nullptr, nullptr);
    if (!narrow.empty() && narrow.back() == '\0') narrow.pop_back();

    ofs << ts << narrow << "\r\n";
}
