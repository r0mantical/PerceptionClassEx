// Plugin.cpp

#include <windows.h>
#include <psapi.h>
#include <string>
#include <codecvt>

#include "Plugin.h"
#include "WebSocketServer.hpp"
#include "resource.h"
#include "ReadCache.hpp"

BOOL gTestPluginState = FALSE;

void StartWebSocketServer();
void StopWebSocketServer();

std::wstring GetProcessNameFromPid(DWORD dwProcessId);

// Small helper to enqueue jobs
static inline void pushJob(Job job)
{
    std::lock_guard<std::mutex> lock(g_jobs_mutex);
    g_jobs.push(std::move(job));
}

// this sends off an async request to the script client to open a process handle
void RequestNewProcess(DWORD dwProcessId)
{
    Job job;
    job.type = JobType::OpenProcess;
    job.pid = dwProcessId;

    pushJob(job);
}

// we replace "real" handles here with a psudohandle
// returns the pid as a psudohandle
HANDLE PLUGIN_CC MyOpenProcess(IN DWORD dwDesiredAccess,
    IN BOOL  bInheritHandle,
    IN DWORD dwProcessId)
{
    if (dwProcessId == 0) return NULL;

    LOGV(L"MyOpenProcess called for pid %u, access 0x%08X",
        dwProcessId, dwDesiredAccess);

    // Detect process browser enumeration
    bool isProbe =
        (dwDesiredAccess ==
            (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ)) ||
        (dwDesiredAccess == PROCESS_QUERY_LIMITED_INFORMATION);

    if (isProbe) {
        // Return a fake handle but DO NOT enqueue a job
        // HACK: By returning 1, which is a valid psudohandle, we don't crash on closehandle but can still indicate success
        return (HANDLE)1;
    }

    // Real attach request
    RequestNewProcess(dwProcessId);

    // HACK: By returning 1, which is a valid psudohandle, we don't crash on closehandle but can still indicate success
    return (HANDLE)1;
}

// simply returns the handle as the thread id at the moment
HANDLE PLUGIN_CC MyOpenThread(IN DWORD dwDesiredAccess,
    IN BOOL  bInheritHandle,
    IN DWORD dwThreadId)
{
    if (dwThreadId == 0) return NULL;

    LOGV(L"MyOpenThread called for tid %u, access 0x%08X",
        dwThreadId, dwDesiredAccess);

    // HACK: By returning 1, which is a valid psudohandle, we don't crash on closehandle but can still indicate success
    return (HANDLE)1;
}

BOOL PLUGIN_CC MyCloseProcess(HANDLE)
{
    Job job;
    job.type = JobType::CloseProcess;
    pushJob(job);
    return TRUE;
}

BOOL PLUGIN_CC
PluginInit(OUT LPRECLASS_PLUGIN_INFO lpRCInfo)
{
    wcscpy_s(lpRCInfo->Name, L"Websocket Memory");
    wcscpy_s(lpRCInfo->Version, L"1.0.0.2");
    wcscpy_s(lpRCInfo->About, L"Access your memory using websockets");
    lpRCInfo->DialogId = IDD_SETTINGS_DLG;

    if (!ReClassIsReadMemoryOverriden() &&
        !ReClassIsWriteMemoryOverriden())
    {
        if (ReClassOverrideMemoryOperations(ReadCallback, WriteCallback) == FALSE)
        {
            ReClassPrintConsole(
                L"[WSMem] Failed to register r/w callbacks, failed Plugin Init");
            return FALSE;
        }
    }

    if (!ReClassOverrideHandleOperations(MyOpenProcess, MyOpenThread)) {
        return FALSE;
    }

    gTestPluginState = TRUE;
    return TRUE;
}

VOID PLUGIN_CC
PluginStateChange(IN BOOL State)
{
    gTestPluginState = State;

    if (State)
    {
        LOG(L"[WSMem] Enabled!");

        StartWebSocketServer();
        StartWorker();
    }
    else
    {
        LOG(L"[WSMem] Disabled!");

        StopWorker();
        StopWebSocketServer();

        if (ReClassGetCurrentReadMemory() == &ReadCallback)
            ReClassRemoveReadMemoryOverride();

        if (ReClassGetCurrentWriteMemory() == &WriteCallback)
            ReClassRemoveWriteMemoryOverride();
    }
}

INT_PTR PLUGIN_CC
PluginSettingsDlg(IN HWND hWnd,
    IN UINT Msg,
    IN WPARAM wParam,
    IN LPARAM lParam)
{
    switch (Msg)
    {
    case WM_INITDIALOG:
    {
        if (gTestPluginState)
        {
            BOOL ReadChecked =
                (ReClassGetCurrentReadMemory() == &ReadCallback)
                ? BST_CHECKED : BST_UNCHECKED;
            BOOL WriteChecked =
                (ReClassGetCurrentWriteMemory() == &WriteCallback)
                ? BST_CHECKED : BST_UNCHECKED;

            SendMessage(GetDlgItem(hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE),
                BM_SETCHECK, MAKEWPARAM(ReadChecked, 0), 0);
            EnableWindow(GetDlgItem(hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE), TRUE);

            SendMessage(GetDlgItem(hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE),
                BM_SETCHECK, MAKEWPARAM(WriteChecked, 0), 0);
            EnableWindow(GetDlgItem(hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE), TRUE);
        }
        else
        {
            SendMessage(GetDlgItem(hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE),
                BM_SETCHECK, MAKEWPARAM(BST_UNCHECKED, 0), 0);
            EnableWindow(GetDlgItem(hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE), FALSE);

            SendMessage(GetDlgItem(hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE),
                BM_SETCHECK, MAKEWPARAM(BST_UNCHECKED, 0), 0);
            EnableWindow(GetDlgItem(hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE), FALSE);
        }
    }
    return TRUE;

    case WM_COMMAND:
    {
        WORD NotificationCode = HIWORD(wParam);
        WORD ControlId = LOWORD(wParam);
        HWND hControlWnd = (HWND)lParam;

        if (NotificationCode == BN_CLICKED)
        {
            BOOLEAN bChecked =
                (SendMessage(hControlWnd, BM_GETCHECK, 0, 0) == BST_CHECKED);

            if (ControlId == IDC_CHECK_READ_MEMORY_OVERRIDE)
            {
                if (bChecked)
                {
                    if (!ReClassIsReadMemoryOverriden())
                    {
                        ReClassOverrideReadMemoryOperation(&ReadCallback);
                    }
                    else
                    {
                        if (ReClassGetCurrentReadMemory() != &ReadCallback)
                        {
                            if (MessageBoxW(ReClassMainWindow(),
                                L"Another plugin has already overriden the read operation.\n"
                                L"Would you like to overwrite their read override?",
                                L"Test Plugin", MB_YESNO) == IDYES)
                            {
                                ReClassOverrideReadMemoryOperation(&ReadCallback);
                            }
                            else
                            {
                                SendMessage(GetDlgItem(hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE),
                                    BM_SETCHECK,
                                    MAKEWPARAM(BST_UNCHECKED, 0), 0);
                            }
                        }
                        else
                        {
                            MessageBoxW(ReClassMainWindow(),
                                L"WTF! Plugin memory read operation is already set as the active override!",
                                L"Test Plugin", MB_ICONERROR);
                        }
                    }
                }
                else
                {
                    if (ReClassGetCurrentReadMemory() == &ReadCallback)
                    {
                        ReClassRemoveReadMemoryOverride();
                    }
                }
            }
            else if (ControlId == IDC_CHECK_WRITE_MEMORY_OVERRIDE)
            {
                if (bChecked)
                {
                    if (!ReClassIsWriteMemoryOverriden())
                    {
                        ReClassOverrideWriteMemoryOperation(&WriteCallback);
                    }
                    else
                    {
                        if (ReClassGetCurrentWriteMemory() != &WriteCallback)
                        {
                            if (MessageBoxW(ReClassMainWindow(),
                                L"Another plugin has already overriden the write operation.\n"
                                L"Would you like to overwrite their write override?",
                                L"Test Plugin", MB_YESNO) == IDYES)
                            {
                                ReClassOverrideWriteMemoryOperation(&WriteCallback);
                            }
                            else
                            {
                                SendMessage(GetDlgItem(hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE),
                                    BM_SETCHECK,
                                    MAKEWPARAM(BST_UNCHECKED, 0), 0);
                            }
                        }
                        else
                        {
                            MessageBoxW(ReClassMainWindow(),
                                L"WTF! Plugin memory write operation is already set as the active override!",
                                L"Test Plugin", MB_ICONERROR);
                        }
                    }
                }
                else
                {
                    if (ReClassGetCurrentWriteMemory() == &WriteCallback)
                    {
                        ReClassRemoveWriteMemoryOverride();
                    }
                }
            }
        }
    }
    break;

    case WM_CLOSE:
        EndDialog(hWnd, 0);
        break;
    }
    return FALSE;
}

// ----------------- Read / Write callbacks -----------------

BOOL PLUGIN_CC ReadCallback(
    IN LPVOID Address,
    IN LPVOID Buffer,
    IN SIZE_T Size,
    OUT PSIZE_T BytesRead
)
{
    DWORD pid = ReClassGetProcessId();
    if (pid == 0 || Address == nullptr || Buffer == nullptr || Size == 0) {
        if (BytesRead) *BytesRead = 0;
        return FALSE;
    }

    uintptr_t addr = (uintptr_t)Address;
    uintptr_t orig_addr = addr;
    SIZE_T    remaining = Size;
    SIZE_T    total_copied = 0;
    uint8_t* out = static_cast<uint8_t*>(Buffer);

    bool need_remote = false;

    while (remaining) {

        uintptr_t page_base = PAGE_BASE(addr);
        size_t    page_offset = static_cast<size_t>(addr - page_base);
        size_t    chunk = PAGE_SIZE - page_offset;
        if (chunk > remaining)
            chunk = remaining;

        bool satisfied_from_cache = false;
        bool refreshRequestedThisPage = false;

        {
            std::shared_lock<std::shared_mutex> lock(g_cache_mutex);

            ReadKey key(page_base);
            auto it = g_cache.find(key);
            if (it != g_cache.end()) {
                CachedBlock& block = it->second;

                bool expired = DEFAULT_CACHE_EXPIRY_HANDLER(block.timestamp);
                if (expired) {
                    refreshRequestedThisPage = true;
                }

                size_t needed_start = page_offset;
                size_t needed_end = page_offset + chunk;
                size_t valid_end = block.valid_start + block.valid_length;

                if (needed_start >= block.valid_start &&
                    needed_end <= valid_end)
                {
                    const uint8_t* src = block.data + page_offset;
                    memcpy(out, src, chunk);
                    satisfied_from_cache = true;
                    total_copied += chunk;
                }
            }
        }

        if (!satisfied_from_cache) {
            // Not in cache: zero now, and request remote
            ZeroMemory(out, chunk);
            total_copied += chunk;
            need_remote = true;
        }
        else if (refreshRequestedThisPage) {
            // Cached but expired: return old data, but request refresh
            need_remote = true;
        }

        addr += chunk;
        out += chunk;
        remaining -= chunk;
    }

    if (need_remote) {
        Job job;
        job.type = JobType::Read;
        job.pid = pid;
        job.read.address = orig_addr;
        job.read.size = Size;
        pushJob(job);
    }

    if (BytesRead)
        *BytesRead = total_copied;

    return TRUE;
}

BOOL PLUGIN_CC WriteCallback(
    IN LPVOID Address,
    IN LPVOID Buffer,
    IN SIZE_T Size,
    OUT PSIZE_T BytesWritten
)
{
    DWORD pid = ReClassGetProcessId();
    if (pid == 0 || Address == nullptr || Buffer == nullptr || Size == 0) {
        if (BytesWritten) *BytesWritten = 0;
        return FALSE;
    }

    uintptr_t addr = (uintptr_t)Address;

    // TODO: update cache optimistically per-page here if you want

    // Enqueue async write
    Job job;
    job.type = JobType::Write;
    job.write.address = addr;
    job.pid = pid;
    job.data.assign((uint8_t*)Buffer, (uint8_t*)Buffer + Size);

    pushJob(job);

    if (BytesWritten)
        *BytesWritten = Size;

    return TRUE;
}

// Helper function to get process name from PID
std::wstring GetProcessNameFromPid(DWORD dwProcessId)
{
    std::wstring processName;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        FALSE, dwProcessId);
    if (hProcess)
    {
        WCHAR szProcessName[MAX_PATH] = L"<unknown>";
        if (GetModuleBaseNameW(hProcess, NULL, szProcessName, MAX_PATH))
        {
            processName = szProcessName;
        }
        CloseHandle(hProcess);
    }
    else
    {
        processName = L"<unknown>";
    }
    return processName;
}
