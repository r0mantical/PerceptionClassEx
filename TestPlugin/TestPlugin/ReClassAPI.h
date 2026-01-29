#ifndef _RECLASS_API_H_
#define _RECLASS_API_H_

//#include <afxribbonbar.h> // Used for ribbon bar. comment this out if not used

// COMMENTED OUT: We don't have the .lib files, so we'll load functions dynamically
/*
#ifdef _WIN64
#ifdef _DEBUG
#pragma comment(lib, "nothingtoseehere64_dbg.lib")
#else
#pragma comment(lib, "nothingtoseehere64.lib")
#endif
#else
#ifdef _DEBUG
#pragma comment(lib, "nothingtoseehere_dbg.lib")
#else
#pragma comment(lib, "nothingtoseehere.lib")
#endif
#endif
*/

#define PLUGIN_CC __stdcall

//
// Plugin Operation API Prototypes
//
typedef BOOL( PLUGIN_CC *PPLUGIN_READ_MEMORY_OPERATION )(
    IN LPVOID Address,
    IN LPVOID Buffer,
    IN SIZE_T Size,
    OUT PSIZE_T BytesRead
    );
typedef BOOL( PLUGIN_CC *PPLUGIN_WRITE_MEMORY_OPERATION )(
    IN LPVOID Address,
    IN LPVOID Buffer,
    IN SIZE_T Size,
    OUT PSIZE_T BytesWritten
    );
typedef HANDLE( PLUGIN_CC *PPLUGIN_OPEN_PROCESS_OPERATION )(
    IN DWORD dwDesiredAccess,
    IN BOOL bInheritHandle,
    IN DWORD dwProcessId
    );
typedef HANDLE( PLUGIN_CC *PPLUGIN_OPEN_THREAD_OPERATION )(
    IN DWORD dwDesiredAccess,
    IN BOOL bInheritHandle,
    IN DWORD dwThreadId
    );

//
// Plugin info structure to be filled in during initialization
// which is passed back to ReClass to display in the plugins dialog
//
typedef DECLSPEC_ALIGN(16) struct _RECLASS_PLUGIN_INFO {
    wchar_t Name[256];      //< Name of the plugin
    wchar_t Version[256];   //< Plugin version
    wchar_t About[2048];    //< Small snippet about the plugin 
    int DialogId;           //< Identifier for the settings dialog
} RECLASS_PLUGIN_INFO, *PRECLASS_PLUGIN_INFO, *LPRECLASS_PLUGIN_INFO;

//
// Plugin initialization callback to fill in the RECLASS_PLUGIN_INFO struct,
// and initialize any other plugin resources
//
BOOL 
PLUGIN_CC 
PluginInit( 
    OUT LPRECLASS_PLUGIN_INFO lpRCInfo
    );

//
// Callback for when the plugin state is changed (enabled or disabled). 
// Plugins disabled and enabled state are dependent on the implementation inside the plugin. 
// All we do is send a state change to plugins for them to disable or enable their functionality.
//
VOID 
PLUGIN_CC 
PluginStateChange( 
    IN BOOL state 
    );

//
// Window Proc for the settings dialog
//
INT_PTR 
PLUGIN_CC 
PluginSettingsDlg( 
    IN HWND hWnd, 
    IN UINT Msg, 
    IN WPARAM wParam, 
    IN LPARAM lParam 
    );

// =============================================================================
// DYNAMIC LOADING APPROACH - Functions loaded at runtime via GetProcAddress
// =============================================================================

// Function pointer types
typedef BOOL (PLUGIN_CC *PFN_ReClassOverrideReadMemoryOperation)(PPLUGIN_READ_MEMORY_OPERATION);
typedef BOOL (PLUGIN_CC *PFN_ReClassOverrideWriteMemoryOperation)(PPLUGIN_WRITE_MEMORY_OPERATION);
typedef BOOL (PLUGIN_CC *PFN_ReClassOverrideMemoryOperations)(PPLUGIN_READ_MEMORY_OPERATION, PPLUGIN_WRITE_MEMORY_OPERATION);
typedef BOOL (PLUGIN_CC *PFN_ReClassRemoveReadMemoryOverride)(VOID);
typedef BOOL (PLUGIN_CC *PFN_ReClassRemoveWriteMemoryOverride)(VOID);
typedef BOOL (PLUGIN_CC *PFN_ReClassIsReadMemoryOverriden)(VOID);
typedef BOOL (PLUGIN_CC *PFN_ReClassIsWriteMemoryOverriden)(VOID);
typedef PPLUGIN_READ_MEMORY_OPERATION (PLUGIN_CC *PFN_ReClassGetCurrentReadMemory)(VOID);
typedef PPLUGIN_WRITE_MEMORY_OPERATION (PLUGIN_CC *PFN_ReClassGetCurrentWriteMemory)(VOID);
typedef BOOL (PLUGIN_CC *PFN_ReClassOverrideOpenProcessOperation)(PPLUGIN_OPEN_PROCESS_OPERATION);
typedef BOOL (PLUGIN_CC *PFN_ReClassOverrideOpenThreadOperation)(PPLUGIN_OPEN_THREAD_OPERATION);
typedef BOOL (PLUGIN_CC *PFN_ReClassOverrideHandleOperations)(PPLUGIN_OPEN_PROCESS_OPERATION, PPLUGIN_OPEN_THREAD_OPERATION);
typedef BOOL (PLUGIN_CC *PFN_ReClassRemoveOpenProcessOverride)(VOID);
typedef BOOL (PLUGIN_CC *PFN_ReClassRemoveOpenThreadOverride)(VOID);
typedef BOOL (PLUGIN_CC *PFN_ReClassIsOpenProcessOverriden)(VOID);
typedef BOOL (PLUGIN_CC *PFN_ReClassIsOpenThreadOverriden)(VOID);
typedef PPLUGIN_OPEN_PROCESS_OPERATION (PLUGIN_CC *PFN_ReClassGetCurrentOpenProcess)(VOID);
typedef PPLUGIN_OPEN_THREAD_OPERATION (PLUGIN_CC *PFN_ReClassGetCurrentOpenThread)(VOID);
typedef VOID (PLUGIN_CC *PFN_ReClassPrintConsole)(const wchar_t*, ...);
typedef HANDLE (PLUGIN_CC *PFN_ReClassGetProcessHandle)(VOID);
typedef DWORD (PLUGIN_CC *PFN_ReClassGetProcessId)(VOID);
typedef HWND (PLUGIN_CC *PFN_ReClassMainWindow)(VOID);

// Global function pointers (in header because of inline functions)
static PFN_ReClassOverrideReadMemoryOperation g_ReClassOverrideReadMemoryOperation = nullptr;
static PFN_ReClassOverrideWriteMemoryOperation g_ReClassOverrideWriteMemoryOperation = nullptr;
static PFN_ReClassOverrideMemoryOperations g_ReClassOverrideMemoryOperations = nullptr;
static PFN_ReClassRemoveReadMemoryOverride g_ReClassRemoveReadMemoryOverride = nullptr;
static PFN_ReClassRemoveWriteMemoryOverride g_ReClassRemoveWriteMemoryOverride = nullptr;
static PFN_ReClassIsReadMemoryOverriden g_ReClassIsReadMemoryOverriden = nullptr;
static PFN_ReClassIsWriteMemoryOverriden g_ReClassIsWriteMemoryOverriden = nullptr;
static PFN_ReClassGetCurrentReadMemory g_ReClassGetCurrentReadMemory = nullptr;
static PFN_ReClassGetCurrentWriteMemory g_ReClassGetCurrentWriteMemory = nullptr;
static PFN_ReClassOverrideOpenProcessOperation g_ReClassOverrideOpenProcessOperation = nullptr;
static PFN_ReClassOverrideOpenThreadOperation g_ReClassOverrideOpenThreadOperation = nullptr;
static PFN_ReClassOverrideHandleOperations g_ReClassOverrideHandleOperations = nullptr;
static PFN_ReClassRemoveOpenProcessOverride g_ReClassRemoveOpenProcessOverride = nullptr;
static PFN_ReClassRemoveOpenThreadOverride g_ReClassRemoveOpenThreadOverride = nullptr;
static PFN_ReClassIsOpenProcessOverriden g_ReClassIsOpenProcessOverriden = nullptr;
static PFN_ReClassIsOpenThreadOverriden g_ReClassIsOpenThreadOverriden = nullptr;
static PFN_ReClassGetCurrentOpenProcess g_ReClassGetCurrentOpenProcess = nullptr;
static PFN_ReClassGetCurrentOpenThread g_ReClassGetCurrentOpenThread = nullptr;
static PFN_ReClassPrintConsole g_ReClassPrintConsole = nullptr;
static PFN_ReClassGetProcessHandle g_ReClassGetProcessHandle = nullptr;
static PFN_ReClassGetProcessId g_ReClassGetProcessId = nullptr;
static PFN_ReClassMainWindow g_ReClassMainWindow = nullptr;

// Load all ReClass API functions
static inline BOOL LoadReClassAPI()
{
    static BOOL loaded = FALSE;
    if (loaded) return TRUE;

    HMODULE hReClass = GetModuleHandleW(nullptr);
    if (!hReClass) return FALSE;

    #define LOAD_FUNC(name) g_##name = (PFN_##name)GetProcAddress(hReClass, #name)

    LOAD_FUNC(ReClassOverrideReadMemoryOperation);
    LOAD_FUNC(ReClassOverrideWriteMemoryOperation);
    LOAD_FUNC(ReClassOverrideMemoryOperations);
    LOAD_FUNC(ReClassRemoveReadMemoryOverride);
    LOAD_FUNC(ReClassRemoveWriteMemoryOverride);
    LOAD_FUNC(ReClassIsReadMemoryOverriden);
    LOAD_FUNC(ReClassIsWriteMemoryOverriden);
    LOAD_FUNC(ReClassGetCurrentReadMemory);
    LOAD_FUNC(ReClassGetCurrentWriteMemory);
    LOAD_FUNC(ReClassOverrideOpenProcessOperation);
    LOAD_FUNC(ReClassOverrideOpenThreadOperation);
    LOAD_FUNC(ReClassOverrideHandleOperations);
    LOAD_FUNC(ReClassRemoveOpenProcessOverride);
    LOAD_FUNC(ReClassRemoveOpenThreadOverride);
    LOAD_FUNC(ReClassIsOpenProcessOverriden);
    LOAD_FUNC(ReClassIsOpenThreadOverriden);
    LOAD_FUNC(ReClassGetCurrentOpenProcess);
    LOAD_FUNC(ReClassGetCurrentOpenThread);
    LOAD_FUNC(ReClassPrintConsole);
    LOAD_FUNC(ReClassGetProcessHandle);
    LOAD_FUNC(ReClassGetProcessId);
    LOAD_FUNC(ReClassMainWindow);

    #undef LOAD_FUNC

    loaded = TRUE;
    return TRUE;
}

// Inline wrapper functions
static inline BOOL ReClassOverrideReadMemoryOperation(PPLUGIN_READ_MEMORY_OPERATION op) {
    LoadReClassAPI();
    return g_ReClassOverrideReadMemoryOperation ? g_ReClassOverrideReadMemoryOperation(op) : FALSE;
}

static inline BOOL ReClassOverrideWriteMemoryOperation(PPLUGIN_WRITE_MEMORY_OPERATION op) {
    LoadReClassAPI();
    return g_ReClassOverrideWriteMemoryOperation ? g_ReClassOverrideWriteMemoryOperation(op) : FALSE;
}

static inline BOOL ReClassOverrideMemoryOperations(PPLUGIN_READ_MEMORY_OPERATION rop, PPLUGIN_WRITE_MEMORY_OPERATION wop) {
    LoadReClassAPI();
    return g_ReClassOverrideMemoryOperations ? g_ReClassOverrideMemoryOperations(rop, wop) : FALSE;
}

static inline BOOL ReClassRemoveReadMemoryOverride() {
    LoadReClassAPI();
    return g_ReClassRemoveReadMemoryOverride ? g_ReClassRemoveReadMemoryOverride() : FALSE;
}

static inline BOOL ReClassRemoveWriteMemoryOverride() {
    LoadReClassAPI();
    return g_ReClassRemoveWriteMemoryOverride ? g_ReClassRemoveWriteMemoryOverride() : FALSE;
}

static inline BOOL ReClassIsReadMemoryOverriden() {
    LoadReClassAPI();
    return g_ReClassIsReadMemoryOverriden ? g_ReClassIsReadMemoryOverriden() : FALSE;
}

static inline BOOL ReClassIsWriteMemoryOverriden() {
    LoadReClassAPI();
    return g_ReClassIsWriteMemoryOverriden ? g_ReClassIsWriteMemoryOverriden() : FALSE;
}

static inline PPLUGIN_READ_MEMORY_OPERATION ReClassGetCurrentReadMemory() {
    LoadReClassAPI();
    return g_ReClassGetCurrentReadMemory ? g_ReClassGetCurrentReadMemory() : nullptr;
}

static inline PPLUGIN_WRITE_MEMORY_OPERATION ReClassGetCurrentWriteMemory() {
    LoadReClassAPI();
    return g_ReClassGetCurrentWriteMemory ? g_ReClassGetCurrentWriteMemory() : nullptr;
}

static inline BOOL ReClassOverrideOpenProcessOperation(PPLUGIN_OPEN_PROCESS_OPERATION op) {
    LoadReClassAPI();
    return g_ReClassOverrideOpenProcessOperation ? g_ReClassOverrideOpenProcessOperation(op) : FALSE;
}

static inline BOOL ReClassOverrideOpenThreadOperation(PPLUGIN_OPEN_THREAD_OPERATION op) {
    LoadReClassAPI();
    return g_ReClassOverrideOpenThreadOperation ? g_ReClassOverrideOpenThreadOperation(op) : FALSE;
}

static inline BOOL ReClassOverrideHandleOperations(PPLUGIN_OPEN_PROCESS_OPERATION pop, PPLUGIN_OPEN_THREAD_OPERATION top) {
    LoadReClassAPI();
    return g_ReClassOverrideHandleOperations ? g_ReClassOverrideHandleOperations(pop, top) : FALSE;
}

static inline BOOL ReClassRemoveOpenProcessOverride() {
    LoadReClassAPI();
    return g_ReClassRemoveOpenProcessOverride ? g_ReClassRemoveOpenProcessOverride() : FALSE;
}

static inline BOOL ReClassRemoveOpenThreadOverride() {
    LoadReClassAPI();
    return g_ReClassRemoveOpenThreadOverride ? g_ReClassRemoveOpenThreadOverride() : FALSE;
}

static inline BOOL ReClassIsOpenProcessOverriden() {
    LoadReClassAPI();
    return g_ReClassIsOpenProcessOverriden ? g_ReClassIsOpenProcessOverriden() : FALSE;
}

static inline BOOL ReClassIsOpenThreadOverriden() {
    LoadReClassAPI();
    return g_ReClassIsOpenThreadOverriden ? g_ReClassIsOpenThreadOverriden() : FALSE;
}

static inline PPLUGIN_OPEN_PROCESS_OPERATION ReClassGetCurrentOpenProcess() {
    LoadReClassAPI();
    return g_ReClassGetCurrentOpenProcess ? g_ReClassGetCurrentOpenProcess() : nullptr;
}

static inline PPLUGIN_OPEN_THREAD_OPERATION ReClassGetCurrentOpenThread() {
    LoadReClassAPI();
    return g_ReClassGetCurrentOpenThread ? g_ReClassGetCurrentOpenThread() : nullptr;
}

static inline VOID ReClassPrintConsole(const wchar_t* fmt, ...) {
    LoadReClassAPI();
    if (g_ReClassPrintConsole) {
        va_list args;
        va_start(args, fmt);
        // Call with format string and args
        wchar_t buffer[2048];
        vswprintf_s(buffer, 2048, fmt, args);
        g_ReClassPrintConsole(L"%s", buffer);
        va_end(args);
    }
}

static inline HANDLE ReClassGetProcessHandle() {
    LoadReClassAPI();
    return g_ReClassGetProcessHandle ? g_ReClassGetProcessHandle() : nullptr;
}

static inline DWORD ReClassGetProcessId() {
    LoadReClassAPI();
    return g_ReClassGetProcessId ? g_ReClassGetProcessId() : 0;
}

static inline HWND ReClassMainWindow() {
    LoadReClassAPI();
    return g_ReClassMainWindow ? g_ReClassMainWindow() : nullptr;
}

#endif // _RECLASS_API_H_
