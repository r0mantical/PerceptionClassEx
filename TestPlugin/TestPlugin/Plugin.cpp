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


// this sends off an async request to the script client to open a process handle
// We will call this whenever OpenProcess is called with non-probe access
// if needed we can also call this if our cached pid ends up not being equal to the current attached pid.
// but that may not be needed so we will wait to see first if reclass handles attachment state fine already for our needs.
void RequestNewProcess(DWORD dwProcessId)
{
    
    Job job;
    job.type = JobType::OpenProcess;
    job.pid = dwProcessId;

    {
        std::lock_guard<std::mutex> lock(g_jobs_mutex);
        g_jobs.push(std::move(job));
    }
}

// we replace "real" handles here with a psudohandle
// returns the pid as a psudohandle
HANDLE PLUGIN_CC MyOpenProcess(IN DWORD dwDesiredAccess, IN BOOL bInheritHandle, IN DWORD dwProcessId)
{
	if (dwProcessId == 0) return NULL;
    // Log for debugging
    LOGV(L"MyOpenProcess called for pid %u, access 0x%08X",dwProcessId, dwDesiredAccess);

    // Detect process browser enumeration
    bool isProbe =
        (dwDesiredAccess == (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ)) ||
        (dwDesiredAccess == PROCESS_QUERY_LIMITED_INFORMATION);

    if (isProbe) {
        // Return a fake handle but DO NOT enqueue a job

		// todo: maybe verify that the process actually exists?
		// not needed until we replace process iteration with a remote request

		return (HANDLE)dwProcessId; // ideally I would like to differentiate this from the psudohandle returned on real attach.
    }

    // Real attach request
	RequestNewProcess(dwProcessId);


	// by returning the pid as a psudohandle we can identify it later in Read/Write operations
    return (HANDLE)dwProcessId;
}

// simply returns the handle as the thread id at the moment
HANDLE PLUGIN_CC MyOpenThread(IN DWORD dwDesiredAccess, IN BOOL bInheritHandle, IN DWORD dwThreadId) {
	if (dwThreadId == 0) return NULL;
    // Log for debugging
    LOGV(L"MyOpenThread called for pid %u, access 0x%08X",dwProcessId, dwDesiredAccess);

    //// Detect process browser enumeration
    //bool isProbe =
    //    (dwDesiredAccess == (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ)) ||
    //    (dwDesiredAccess == PROCESS_QUERY_LIMITED_INFORMATION);

    //if (isProbe) {
    //    // Return a fake handle but DO NOT enqueue a job
    //    return (HANDLE)dwThreadId;
    //}

	// Real attach request
	// todo: decide if I wanna keep track of some kind of identifiers mapped to real handles on the server or just return fake handles always

    // for now we just return 1
    return (HANDLE)dwThreadId;
}


BOOL PLUGIN_CC MyCloseProcess(HANDLE)
{
    Job job;
    job.type = JobType::CloseProcess;
    {
        std::lock_guard<std::mutex> lock(g_jobs_mutex);
        g_jobs.push(std::move(job));
    }
    return TRUE;
}


BOOL 
PLUGIN_CC 
PluginInit( 
    OUT LPRECLASS_PLUGIN_INFO lpRCInfo 
)
{
    wcscpy_s( lpRCInfo->Name, L"Websocket Memory" );
    wcscpy_s( lpRCInfo->Version, L"1.0.0.1" );
    wcscpy_s( lpRCInfo->About, L"Access your memory using websockets" );
    lpRCInfo->DialogId = IDD_SETTINGS_DLG;

    if (!ReClassIsReadMemoryOverriden( ) && !ReClassIsWriteMemoryOverriden( ))
    {
        if (ReClassOverrideMemoryOperations( ReadCallback, WriteCallback ) == FALSE)
        {
            ReClassPrintConsole( L"[WSMem] Failed to register r/w callbacks, failed Plugin Init" );
            return FALSE;
        }
    }

    if (!ReClassOverrideHandleOperations(MyOpenProcess, MyOpenThread)) {
        return FALSE;
    }


    //if (!StartWorker()) {
    //    return FALSE;
    //}


    gTestPluginState = TRUE;

    return TRUE;
}

VOID 
PLUGIN_CC 
PluginStateChange( 
    IN BOOL State 
)
{
    //
    // Update global state variable
    //
    gTestPluginState = State;


    if (State)
    {
        LOG( L"[WSMem] Enabled!" );



		// start the server
        StartWebSocketServer(); // sets g_wsRunning, creates g_context
        StartWorker(); // afterwards, now SendWebSocketCommand can succeed 
    }
    else
    {
        LOG( L"[WSMem] Disabled!" );

		// stop the worker
		StopWorker();

		// stop the server
        StopWebSocketServer();

        //
        // Remove our overrides if we're disabling/disabled.
        //
        if (ReClassGetCurrentReadMemory( ) == &ReadCallback)
            ReClassRemoveReadMemoryOverride( );

        if (ReClassGetCurrentWriteMemory( ) == &WriteCallback)
            ReClassRemoveWriteMemoryOverride( );
    }
}

INT_PTR 
PLUGIN_CC 
PluginSettingsDlg( 
    IN HWND hWnd, 
    IN UINT Msg, 
    IN WPARAM wParam, 
    IN LPARAM lParam 
)
{
    switch (Msg)
    {

    case WM_INITDIALOG:
    {
        if (gTestPluginState)
        {
            //
            // Apply checkboxes appropriately if we're in anm enabled state.
            //
            BOOL ReadChecked = (ReClassGetCurrentReadMemory( ) == &ReadCallback) ? BST_CHECKED : BST_UNCHECKED;
            BOOL WriteChecked = (ReClassGetCurrentWriteMemory( ) == &WriteCallback) ? BST_CHECKED : BST_UNCHECKED;

            SendMessage( GetDlgItem( hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE ), BM_SETCHECK, MAKEWPARAM( ReadChecked, 0 ), 0 );
            EnableWindow( GetDlgItem( hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE ), TRUE );

            SendMessage( GetDlgItem( hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE ), BM_SETCHECK, MAKEWPARAM( WriteChecked, 0 ), 0 );
            EnableWindow( GetDlgItem( hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE ), TRUE );
        }
        else
        {
            //
            // Make sure we can't touch the settings if we're in a disabled state.
            //
            SendMessage( GetDlgItem( hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE ), BM_SETCHECK, MAKEWPARAM( BST_UNCHECKED, 0 ), 0 );
            EnableWindow( GetDlgItem( hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE ), FALSE );

            SendMessage( GetDlgItem( hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE ), BM_SETCHECK, MAKEWPARAM( BST_UNCHECKED, 0 ), 0 );
            EnableWindow( GetDlgItem( hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE ), FALSE );
        }
    }
    return TRUE;

    case WM_COMMAND:
    {
        WORD NotificationCode = HIWORD( wParam );
        WORD ControlId = LOWORD( wParam );
        HWND hControlWnd = (HWND)lParam;
        
        if (NotificationCode == BN_CLICKED)
        {
            BOOLEAN bChecked = (SendMessage( hControlWnd, BM_GETCHECK, 0, 0 ) == BST_CHECKED);

            if (ControlId == IDC_CHECK_READ_MEMORY_OVERRIDE)
            {
                if (bChecked)
                {
                    //
                    // Make sure the read memory operation is not already overriden.
                    //
                    if (!ReClassIsReadMemoryOverriden( ))
                    {
                        ReClassOverrideReadMemoryOperation( &ReadCallback );
                    }
                    else
                    {
                        //
                        // Make sure it's not us!
                        //
                        if (ReClassGetCurrentReadMemory( ) != &ReadCallback)
                        {
                            //
                            // Ask the user whether or not they want to overwrite the other operation.
                            //
                            if (MessageBoxW( ReClassMainWindow( ),
                                L"Another plugin has already overriden the read operation.\n"
                                L"Would you like to overwrite their read override?",
                                L"Test Plugin", MB_YESNO ) == IDYES)
                            {
                                ReClassOverrideReadMemoryOperation( &ReadCallback );
                            }
                            else
                            {
                                //
                                // If the user chose no, then make sure our checkbox is unchecked.
                                //
                                SendMessage( GetDlgItem( hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE ), 
                                    BM_SETCHECK, MAKEWPARAM( BST_UNCHECKED, 0 ), 0 );
                            }
                        }
                        else
                        {
                            //
                            // This shouldn't happen!
                            //
                            MessageBoxW( ReClassMainWindow( ), 
                                L"WTF! Plugin memory read operation is already set as the active override!", 
                                L"Test Plugin", MB_ICONERROR );
                        }
                    }
                }
                else
                {
                    //
                    // Only remove the read memory operation if it's ours!
                    //
                    if (ReClassGetCurrentReadMemory( ) == &ReadCallback)
                    {
                        ReClassRemoveReadMemoryOverride( );
                    }
                }			
            }
            else if (ControlId == IDC_CHECK_WRITE_MEMORY_OVERRIDE)
            {
                if (bChecked)
                {
                    //
                    // Make sure the write memory operation is not already overriden.
                    //
                    if (!ReClassIsWriteMemoryOverriden( ))
                    {
                        //
                        // We're all good to set our write memory operation!
                        //
                        ReClassOverrideWriteMemoryOperation( &WriteCallback );
                    }
                    else
                    {
                        //
                        // Make sure it's not us!
                        //
                        if (ReClassGetCurrentWriteMemory( ) != &WriteCallback)
                        {
                            //
                            // Ask the user whether or not they want to overwrite the other operation.
                            //
                            if (MessageBoxW( ReClassMainWindow( ),
                                L"Another plugin has already overriden the write operation.\n"
                                L"Would you like to overwrite their write override?",
                                L"Test Plugin", MB_YESNO ) == IDYES)
                            {
                                ReClassOverrideWriteMemoryOperation( &WriteCallback );
                            }
                            else
                            {
                                //
                                // If the user chose no, then make sure our checkbox is unchecked.
                                //
                                SendMessage( GetDlgItem( hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE ),
                                    BM_SETCHECK, MAKEWPARAM( BST_UNCHECKED, 0 ), 0 );
                            }
                        }
                        else
                        {
                            //
                            // This shouldn't happen!
                            //
                            MessageBoxW( ReClassMainWindow( ),
                                L"WTF! Plugin memory write operation is already set as the active override!",
                                L"Test Plugin", MB_ICONERROR );
                        }
                    }
                }
                else
                {
                    //
                    // Only remove the read memory operation if it's ours!
                    //
                    if (ReClassGetCurrentWriteMemory( ) == &WriteCallback)
                    {
                        ReClassRemoveWriteMemoryOverride( );
                    }
                }
            }
        }	
    }
    break;

    case WM_CLOSE:
    {
        EndDialog( hWnd, 0 );
    }
    break;

    }
    return FALSE;
}

const auto pushJob = [](Job job) {
    std::lock_guard<std::mutex> lock(g_jobs_mutex);
    g_jobs.push(std::move(job));
};

BOOL
PLUGIN_CC ReadCallback(
    IN LPVOID Address,
    IN LPVOID Buffer,
    IN SIZE_T Size,
    OUT PSIZE_T BytesRead
)
{
    //ReClassGetProcessHandle() returns the pid also
    DWORD pid = ReClassGetProcessId();
    if(pid == 0 || Address == 0 || Buffer == 0 || Size == 0) {
        if (BytesRead) *BytesRead = 0;
        return FALSE;
	}

    uintptr_t addr = (uintptr_t)Address;
    uint8_t* out = static_cast<uint8_t*>(Buffer);

    SIZE_T remaining = Size;

    SIZE_T total_copied = 0;

	bool refreshRequested = false;

    while (remaining) {

        uintptr_t page_base = PAGE_BASE(addr);
        size_t page_offset = static_cast<size_t>(addr - page_base);
        size_t chunk = PAGE_SIZE - page_offset;
        if (chunk > remaining)
            chunk = remaining;

        bool satisfied_from_cache = false;

        {
            std::shared_lock<std::shared_mutex> lock(g_cache_mutex);

            ReadKey key{ page_base };
            auto it = g_cache.find(key);
            if (it != g_cache.end()) {
                CachedBlock& block = it->second;

                // Expired?
                if (!DEFAULT_CACHE_EXPIRY_HANDLER(block.timestamp)) {
                    // force a refresh but return the expired data anyways
					refreshRequested = true;
                }
				// we will also return the expired data for now, as it is better than nothing
                
                // Check if this chunk is fully covered by valid region
                size_t needed_start = page_offset;
                size_t needed_end = page_offset + chunk;
#if PARANOID
                if (block.valid_start + chunk > PAGE_SIZE) {
                    LOG("Cache logic error: valid region too small for requested chunk");
                    break;
                }
#endif
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

        if (!satisfied_from_cache || refreshRequested) {
            // Cache miss or stale: enqueue async read for this page
            Job job;
            job.type = JobType::Read;

#if FULL_PAGE_FETCH
            job.read.address = page_base;
            job.read.size = PAGE_SIZE;
#else
			job.read.address = addr;
			job.read.size = chunk;
#endif
            job.pid = pid; // we include the pid with every request now to avoid desync of the attached process


            //{
            //    std::lock_guard<std::mutex> lock(g_jobs_mutex);
            //    g_jobs.push(std::move(job));
            //}
            pushJob(job);

            // Todo: improve this behaviour for more "live" use cases than reclass
            // it's kind of hard to do an async read properly without blocking the caller here since our interface with reclass is sync.
            // maybe there is something clever to do
            // 
            // But for now:
            // Cache miss: enqueue async read, return zeros
            // on cache miss we return null at the moment.
            // With reclass this is fine because it will probably come in next frame
            ZeroMemory(out, chunk);
            total_copied += chunk;
        }

        addr += chunk;
        out += chunk;
        remaining -= chunk;
    }



    if (BytesRead) *BytesRead = total_copied;
    return TRUE;
}


BOOL
PLUGIN_CC
WriteCallback(
    IN LPVOID Address,
    IN LPVOID Buffer,
    IN SIZE_T Size,
    OUT PSIZE_T BytesWritten
)
{
    //ReClassGetProcessHandle() returns the pid also
    DWORD pid = ReClassGetProcessId();
    if (pid == 0 || Address == 0 || Buffer == 0 || Size == 0) {
        if (BytesWritten) *BytesWritten = 0;
        return FALSE;
    }

    uintptr_t addr = (uintptr_t)Address;

    // todo: update this for paged cache for better responsiveness
  //  // Update cache optimistically
  //  {
		//std::unique_lock<std::shared_mutex> lock(g_cache_mutex); // write enabled lock
  //      ReadKey key(addr);
  //      CachedBlock block;
  //      block.data.assign((uint8_t*)Buffer, (uint8_t*)Buffer + Size);
  //      g_cache[key] = std::move(block);
  //  }

    // Enqueue async write
    {
        Job job;
        job.type = JobType::Write;
        job.write.address = addr;
        job.pid = pid; // we include the pid with every request now to avoid desync of the attached process
        job.data.assign((uint8_t*)Buffer, (uint8_t*)Buffer + Size);

		pushJob(job);
    }

    if (BytesWritten) *BytesWritten = Size;
    return TRUE;
}

// Helper function to get process name from PID
std::wstring GetProcessNameFromPid(DWORD dwProcessId)
{
    std::wstring processName;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcessId);
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
