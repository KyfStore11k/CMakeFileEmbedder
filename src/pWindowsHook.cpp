#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <format>
#include <cstdio>
#include <unordered_map>
#include <string>
#include <mutex>
#include "MinHook.h"
#include "embedded_files.h"

#ifdef CFE_VERBOSE
    #define CFE_PRINTF(...) CFE_PRINTF(__VA_ARGS__)
#else
    #define CFE_PRINTF(...) ((void)0)
#endif

struct VirtualFile {
    const unsigned char* data;
    size_t size;
    size_t offset;
};

std::unordered_map<HANDLE, std::string> g_HandleToPath;
std::unordered_map<HANDLE, VirtualFile> g_VirtualFiles;
std::unordered_map<FILE*, HANDLE> g_FileToHandle;
std::mutex g_MapMutex;

typedef HANDLE(WINAPI* CreateFileA_t)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
CreateFileA_t RealCreateFileA = NULL;
typedef HANDLE(WINAPI* CreateFileW_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
CreateFileW_t RealCreateFileW = NULL;
typedef BOOL(WINAPI* ReadFile_t)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
ReadFile_t RealReadFile = NULL;
typedef LPVOID(WINAPI* MapViewOfFile_t)(HANDLE, DWORD, DWORD, DWORD, SIZE_T);
MapViewOfFile_t RealMapViewOfFile = NULL;
typedef BOOL(WINAPI* CloseHandle_t)(HANDLE);
CloseHandle_t RealCloseHandle = NULL;
typedef FILE* (__cdecl* fsopen_t)(const char*, const char*, int);
fsopen_t RealFsopen = NULL;
typedef FILE* (__cdecl* wfsopen_t)(const wchar_t*, const wchar_t*, int);
wfsopen_t RealWfsopen = NULL;
typedef errno_t (__cdecl* fopen_s_t)(FILE**, const char*, const char*);
fopen_s_t RealFopenS = NULL;
typedef errno_t (__cdecl* wfopen_s_t)(FILE**, const wchar_t*, const wchar_t*);
wfopen_s_t RealWFopenS = NULL;
typedef size_t (*fread_t)(void* ptr, size_t size, size_t count, FILE* stream);
fread_t RealFread = nullptr;
typedef int (*fseek_t)(FILE* stream, long offset, int whence);
fseek_t RealFseek = nullptr;
typedef long (*ftell_t)(FILE* stream);
ftell_t RealFtell = nullptr;
typedef FILE* (__cdecl* fopen_t)(const char*, const char*);
fopen_t RealFopen = nullptr;
typedef int (*feof_t)(FILE* stream);
feof_t RealFeof = nullptr;
typedef int (__cdecl* fclose_t)(FILE*);
fclose_t RealFclose = nullptr;

std::string wcharToUtf8(const wchar_t* wstr) {
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, result.data(), size, nullptr, nullptr);
    return result;
}

void StoreHandle(HANDLE h, const char* path) {
    if (h != INVALID_HANDLE_VALUE && path) {
        std::lock_guard<std::mutex> lock(g_MapMutex);
        g_HandleToPath[h] = path;
    }
}

void StoreHandleW(HANDLE h, const wchar_t* path) {
    if (h != INVALID_HANDLE_VALUE && path) {
        char buffer[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, path, -1, buffer, MAX_PATH, NULL, NULL);
        std::lock_guard<std::mutex> lock(g_MapMutex);
        g_HandleToPath[h] = buffer;
    }
}

std::string GetPath(HANDLE h) {
    std::lock_guard<std::mutex> lock(g_MapMutex);
    auto it = g_HandleToPath.find(h);
    if (it != g_HandleToPath.end())
        return it->second;
    return "<unknown>";
}

void RemoveHandle(HANDLE h) {
    std::lock_guard<std::mutex> lock(g_MapMutex);

    auto it_path = g_HandleToPath.find(h);
    if (it_path != g_HandleToPath.end()) {
        g_HandleToPath.erase(it_path);
    }

    auto it_vf = g_VirtualFiles.find(h);
    if (it_vf != g_VirtualFiles.end()) {
        g_VirtualFiles.erase(it_vf);
    }
}

HANDLE WINAPI HookCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    if (lpFileName) {
        VirtualFile vf{};
        HANDLE h = INVALID_HANDLE_VALUE;
        std::string foundKey;

        {
            std::lock_guard<std::mutex> lock(g_MapMutex);

            auto it = embedded::registry.find(lpFileName);
            if (it != embedded::registry.end()) {
                vf = { it->second.data, it->second.size, 0 };
                foundKey = it->first;
            } else {
                vf.data = nullptr;
            }
        }

        if (vf.data) {
            h = RealCreateFileA("NUL", dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                    dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

            if (h == INVALID_HANDLE_VALUE) {
                return INVALID_HANDLE_VALUE;
            }

            {
                std::lock_guard<std::mutex> lock(g_MapMutex);
                g_VirtualFiles[h] = vf;
                g_HandleToPath[h] = std::format("{} (embedded)", foundKey);
            }

            return h;
        }
    }

    HANDLE h = RealCreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    if (h != INVALID_HANDLE_VALUE) {
        StoreHandle(h, lpFileName);
    }
    return h;
}

HANDLE WINAPI HookCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {

    if (lpFileName) {
        VirtualFile vf{};
        HANDLE h = INVALID_HANDLE_VALUE;
        std::string foundKey;

        {
            std::lock_guard<std::mutex> lock(g_MapMutex);

            auto it = embedded::registry.find(wcharToUtf8(lpFileName));
            if (it != embedded::registry.end()) {
                vf = { it->second.data, it->second.size, 0 };
                foundKey = it->first;
            } else {
                vf.data = nullptr;
            }
        }

        if (vf.data) {
            h = RealCreateFileW(L"NUL", dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                    dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

            if (h == INVALID_HANDLE_VALUE) {
                return INVALID_HANDLE_VALUE;
            }

            {
                std::lock_guard<std::mutex> lock(g_MapMutex);
                g_VirtualFiles[h] = vf;
                g_HandleToPath[h] = std::format("{} (embedded)", foundKey);
            }

            return h;
        }
    }

    HANDLE h = RealCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    if (h != INVALID_HANDLE_VALUE) {
        StoreHandleW(h, lpFileName);
    }
    return h;
}

BOOL WINAPI HookReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
    DWORD toRead = 0;
    const unsigned char* src = nullptr;

    {
        std::lock_guard<std::mutex> lock(g_MapMutex);
        auto it = g_VirtualFiles.find(hFile);
        if (it != g_VirtualFiles.end()) {
            VirtualFile& vf = it->second;

            DWORD remaining = (vf.offset < vf.size)
                ? (DWORD)(vf.size - vf.offset)
                : 0;

            toRead = (std::min)(nNumberOfBytesToRead, remaining);

            if (toRead > 0) {
                src = vf.data + vf.offset;
                vf.offset += toRead;
            }

            if (lpNumberOfBytesRead)
                *lpNumberOfBytesRead = toRead;
        }
    }

    if (toRead > 0 && src) {
        memcpy(lpBuffer, src, toRead);
        return TRUE;
    }

    {
        std::lock_guard<std::mutex> lock(g_MapMutex);
        if (g_VirtualFiles.find(hFile) != g_VirtualFiles.end()) {
            if (lpNumberOfBytesRead)
                *lpNumberOfBytesRead = 0;
            return TRUE;
        }
    }

    std::string path = GetPath(hFile);
    return RealReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
}

LPVOID WINAPI HookMapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap) {
    return RealMapViewOfFile(hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap);
}

BOOL WINAPI HookCloseHandle(HANDLE hObject) {
    RemoveHandle(hObject);
    return RealCloseHandle(hObject);
}

FILE* __cdecl HookFsopen(const char* filename, const char* mode, int shflag)
{
    if (filename) {
        VirtualFile vf{};
        HANDLE h = INVALID_HANDLE_VALUE;
        std::string foundKey;

        {
            std::lock_guard<std::mutex> lock(g_MapMutex);

            auto it = embedded::registry.find(filename);
            if (it != embedded::registry.end()) {
                vf = { it->second.data, it->second.size, 0 };
                foundKey = it->first;
            } else {
                vf.data = nullptr;
            }
        }

        if (vf.data) {
            h = CreateFileA(
                "NUL",
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                0,
                NULL
                );

            if (h == INVALID_HANDLE_VALUE) {
                return nullptr;
            }

            int fd = _open_osfhandle((intptr_t)h, 0);
            if (fd == -1) {
                CloseHandle(h);
                return nullptr;
            }

            FILE* f = _fdopen(fd, mode);
            if (!f) {
                _close(fd);
                CloseHandle(h);
                return nullptr;
            }

            {
                std::lock_guard<std::mutex> lock(g_MapMutex);
                g_VirtualFiles[h] = vf;
                g_HandleToPath[h] = std::format("{} (embedded)", foundKey);
                g_FileToHandle[f] = h;
            }

            return f;
        }
    }

    return RealFsopen(filename, mode, shflag);
}

FILE* __cdecl HookWfsopen(const wchar_t* filename, const wchar_t* mode, int shflag) {
    if (filename) {
        VirtualFile vf{};
        HANDLE h = INVALID_HANDLE_VALUE;
        std::string foundKey;

        {
            std::lock_guard<std::mutex> lock(g_MapMutex);

            auto it = embedded::registry.find(wcharToUtf8(filename));
            if (it != embedded::registry.end()) {
                vf = { it->second.data, it->second.size, 0 };
                foundKey = it->first;
            } else {
                vf.data = nullptr;
            }
        }

        if (vf.data) {
            h = CreateFileW(
                L"NUL",
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                0,
                NULL
            );

            if (h == INVALID_HANDLE_VALUE) {
                return nullptr;
            }

            int fd = _open_osfhandle((intptr_t)h, 0);
            if (fd == -1) {
                CloseHandle(h);
                return nullptr;
            }

            FILE* f = _wfdopen(fd, mode);
            if (!f) {
                _close(fd);
                CloseHandle(h);
                return nullptr;
            }

            {
                std::lock_guard<std::mutex> lock(g_MapMutex);
                g_VirtualFiles[h] = vf;
                g_HandleToPath[h] = std::format("{} (embedded)", foundKey);
                g_FileToHandle[f] = h;
            }

            return f;
        }
    }

    return RealWfsopen(filename, mode, shflag);
}

errno_t __cdecl HookFopenS(FILE** pFile, const char* filename, const char* mode)
{
    if (filename) {
        VirtualFile vf{};
        HANDLE h = INVALID_HANDLE_VALUE;
        std::string foundKey;

        {
            std::lock_guard<std::mutex> lock(g_MapMutex);

            auto it = embedded::registry.find(filename);
            if (it != embedded::registry.end()) {
                vf = { it->second.data, it->second.size, 0 };
                foundKey = it->first;
            } else {
                vf.data = nullptr;
            }
        }

        if (vf.data) {
            h = CreateFileA(
                "NUL",
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                0,
                NULL
            );

            if (h == INVALID_HANDLE_VALUE) {
                return -1;
            }

            int fd = _open_osfhandle((intptr_t)h, 0);
            if (fd == -1) {
                CloseHandle(h);
                return -1;
            }

            FILE* f = _fdopen(fd, mode);
            if (!f) {
                _close(fd);
                CloseHandle(h);
                return -1;
            }

            *pFile = f;

            {
                std::lock_guard<std::mutex> lock(g_MapMutex);
                g_VirtualFiles[h] = vf;
                g_HandleToPath[h] = std::format("{} (embedded)", foundKey);
                g_FileToHandle[*pFile] = h;
            }

            return 0;
        }
    }

    return RealFopenS(pFile, filename, mode);
}

errno_t __cdecl HookWFopenS(FILE** pFile, const wchar_t* filename, const wchar_t* mode)
{
    if (filename) {
        VirtualFile vf{};
        HANDLE h = INVALID_HANDLE_VALUE;
        std::string foundKey;

        {
            std::lock_guard<std::mutex> lock(g_MapMutex);

            auto it = embedded::registry.find(wcharToUtf8(filename));
            if (it != embedded::registry.end()) {
                vf = { it->second.data, it->second.size, 0 };
                foundKey = it->first;
            } else {
                vf.data = nullptr;
            }
        }

        if (vf.data) {
            h = CreateFileW(
                L"NUL",
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                0,
                NULL
            );

            if (h == INVALID_HANDLE_VALUE) {
                return -1;
            }

            int fd = _open_osfhandle((intptr_t)h, 0);
            if (fd == -1) {
                CloseHandle(h);
                return -1;
            }

            FILE* f = _wfdopen(fd, mode);
            if (!f) {
                _close(fd);
                CloseHandle(h);
                return -1;
            }

            *pFile = f;

            {
                std::lock_guard<std::mutex> lock(g_MapMutex);
                g_VirtualFiles[h] = vf;
                g_HandleToPath[h] = std::format("{} (embedded)", foundKey);
                g_FileToHandle[*pFile] = h;
            }

            return 0;
        }
    }

    return RealWFopenS(pFile, filename, mode);
}

size_t __cdecl HookFread(void* buffer, size_t size, size_t count, FILE* stream)
{
    if (!buffer || size == 0 || count == 0)
        return 0;

    std::lock_guard<std::mutex> lock(g_MapMutex);

    auto it_handle = g_FileToHandle.find(stream);
    if (it_handle == g_FileToHandle.end())
        return RealFread(buffer, size, count, stream);

    HANDLE h = it_handle->second;

    auto it_vf = g_VirtualFiles.find(h);
    if (it_vf == g_VirtualFiles.end())
        return RealFread(buffer, size, count, stream);

    VirtualFile& vf = it_vf->second;

    size_t requested_bytes = size * count;

    if (vf.offset >= vf.size)
        return 0;

    size_t available = vf.size - vf.offset;
    size_t to_read = (requested_bytes < available) ? requested_bytes : available;

    if (to_read == 0)
        return 0;

    memcpy(buffer, vf.data + vf.offset, to_read);
    vf.offset += to_read;

    return to_read / size;
}

int __cdecl HookFseek(FILE* stream, long offset, int whence) {
    std::lock_guard<std::mutex> lock(g_MapMutex);
    auto it_handle = g_FileToHandle.find(stream);
    if (it_handle == g_FileToHandle.end()) {
        return RealFseek(stream, offset, whence);
    }
    HANDLE h = it_handle->second;
    auto it_vf = g_VirtualFiles.find(h);
    if (it_vf == g_VirtualFiles.end()) {
        return RealFseek(stream, offset, whence);
    }
    VirtualFile& vf = it_vf->second;

    long new_pos = 0;
    switch (whence) {
        case SEEK_SET: new_pos = offset; break;
        case SEEK_CUR: new_pos = vf.offset + offset; break;
        case SEEK_END: new_pos = vf.size + offset; break;
        default: return -1;
    }

    if (new_pos < 0 || new_pos > (long)vf.size) {
        return -1;
    }

    vf.offset = new_pos;
    return 0;
}

long __cdecl HookFtell(FILE* stream) {
    std::lock_guard<std::mutex> lock(g_MapMutex);
    auto it_handle = g_FileToHandle.find(stream);
    if (it_handle == g_FileToHandle.end()) {
        return RealFtell(stream);
    }
    HANDLE h = it_handle->second;
    auto it_vf = g_VirtualFiles.find(h);
    if (it_vf == g_VirtualFiles.end()) {
        return RealFtell(stream);
    }
    return it_vf->second.offset;
}

FILE* __cdecl HookFopen(const char* filename, const char* mode) {
    if (filename) {
        VirtualFile vf{};
        HANDLE h = INVALID_HANDLE_VALUE;
        std::string foundKey;

        {
            std::lock_guard<std::mutex> lock(g_MapMutex);

            auto it = embedded::registry.find(filename);
            if (it != embedded::registry.end()) {
                vf = { it->second.data, it->second.size, 0 };
                foundKey = it->first;
            } else {
                vf.data = nullptr;
            }
        }

        if (vf.data) {
            h = CreateFileA(
            "NUL",
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
            );

            if (h == INVALID_HANDLE_VALUE) {
                return nullptr;
            }

            int fd = _open_osfhandle((intptr_t)h, 0);
            if (fd == -1) {
                CloseHandle(h);
                return nullptr;
            }

            FILE* f = _fdopen(fd, mode);
            if (!f) {
                _close(fd);
                CloseHandle(h);
                return nullptr;
            }

            {
                std::lock_guard<std::mutex> lock(g_MapMutex);
                g_VirtualFiles[h] = vf;
                g_HandleToPath[h] = std::format("{} (embedded)", foundKey);
                g_FileToHandle[f] = h;
            }

            return f;
        }
    }

    return RealFopen(filename, mode);
}

int __cdecl HookFeof(FILE* stream) {
    std::lock_guard<std::mutex> lock(g_MapMutex);
    auto it_handle = g_FileToHandle.find(stream);
    if (it_handle == g_FileToHandle.end()) {
        return RealFeof(stream);
    }
    HANDLE h = it_handle->second;
    auto it_vf = g_VirtualFiles.find(h);
    if (it_vf == g_VirtualFiles.end()) {
        return RealFeof(stream);
    }
    if (it_vf->second.offset >= it_vf->second.size) {
        return 1;
    }
    return 0;
}

int __cdecl HookFclose(FILE* stream) {
    {
        std::lock_guard<std::mutex> lock(g_MapMutex);

        auto it = g_FileToHandle.find(stream);
        if (it != g_FileToHandle.end()) {
            HANDLE h = it->second;

            g_FileToHandle.erase(it);

            g_VirtualFiles.erase(h);
            g_HandleToPath.erase(h);
        }
    }

    return RealFclose(stream);
}

void InitHooks() {
    if (MH_Initialize() != MH_OK) {
        printf("MinHook init failed\n");
        return;
    }

    MH_CreateHook(reinterpret_cast<LPVOID*>(&CreateFileA), reinterpret_cast<LPVOID*>(&HookCreateFileA), reinterpret_cast<LPVOID*>(&RealCreateFileA));
    MH_CreateHook(reinterpret_cast<LPVOID*>(&CreateFileW), reinterpret_cast<LPVOID*>(&HookCreateFileW), reinterpret_cast<LPVOID*>(&RealCreateFileW));
    MH_CreateHook(reinterpret_cast<LPVOID*>(&ReadFile), reinterpret_cast<LPVOID*>(&HookReadFile), reinterpret_cast<LPVOID*>(&RealReadFile));
    MH_CreateHook(reinterpret_cast<LPVOID*>(&MapViewOfFile), reinterpret_cast<LPVOID*>(&HookMapViewOfFile), reinterpret_cast<LPVOID*>(&RealMapViewOfFile));
    MH_CreateHook(reinterpret_cast<LPVOID*>(&CloseHandle), reinterpret_cast<LPVOID*>(&HookCloseHandle), reinterpret_cast<LPVOID*>(&RealCloseHandle));
	MH_CreateHook(reinterpret_cast<LPVOID*>(&_fsopen), reinterpret_cast<LPVOID*>(&HookFsopen), reinterpret_cast<LPVOID*>(&RealFsopen));
    MH_CreateHook(reinterpret_cast<LPVOID*>(&_wfsopen), reinterpret_cast<LPVOID*>(&HookWfsopen), reinterpret_cast<LPVOID*>(&RealWfsopen));
	MH_CreateHook(reinterpret_cast<LPVOID*>(&fopen_s), reinterpret_cast<LPVOID*>(&HookFopenS), reinterpret_cast<LPVOID*>(&RealFopenS));
    MH_CreateHook(reinterpret_cast<LPVOID*>(&_wfopen_s), reinterpret_cast<LPVOID*>(&HookWFopenS), reinterpret_cast<LPVOID*>(&RealWFopenS));
    MH_CreateHook(reinterpret_cast<LPVOID*>(&fopen), reinterpret_cast<LPVOID*>(&HookFopen), reinterpret_cast<LPVOID*>(&RealFopen));
    MH_CreateHook(reinterpret_cast<LPVOID*>(&fread), reinterpret_cast<LPVOID*>(&HookFread), reinterpret_cast<LPVOID*>(&RealFread));
    MH_CreateHook(reinterpret_cast<LPVOID*>(&feof), reinterpret_cast<LPVOID*>(&HookFeof), reinterpret_cast<LPVOID*>(&RealFeof));
    MH_CreateHook(reinterpret_cast<LPVOID*>(&fseek), reinterpret_cast<LPVOID*>(&HookFseek), reinterpret_cast<LPVOID*>(&RealFseek));
    MH_CreateHook(reinterpret_cast<LPVOID*>(&ftell), reinterpret_cast<LPVOID*>(&HookFtell), reinterpret_cast<LPVOID*>(&RealFtell));
    MH_CreateHook(reinterpret_cast<LPVOID*>(&fclose), reinterpret_cast<LPVOID*>(&HookFclose), reinterpret_cast<LPVOID*>(&RealFclose));

    MH_EnableHook(MH_ALL_HOOKS);
}

extern "C" void InstallFileInterceptionHooks() {
    InitHooks();
}