#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>

#include "embedded_files.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/memfd.h>

#ifdef CFE_VERBOSE
    #define CFE_PRINTF(...) CFE_PRINTF(__VA_ARGS__)
#else
    #define CFE_PRINTF(...) ((void)0)
#endif

struct VirtualFile {
    const unsigned char* data;
    size_t size;
    off_t offset;
};

static std::unordered_map<int, VirtualFile> g_VirtualFiles;
static std::unordered_map<FILE*, int> g_FileToFd;
static std::mutex g_MapMutex;

#define RESOLVE(func) \
    static auto real_##func = (decltype(&func)) dlsym(RTLD_NEXT, #func)

static int create_memory_fd(const unsigned char* data, size_t size) {
    int fd = memfd_create("embedded", MFD_CLOEXEC);
    if (fd == -1) return -1;

    if (ftruncate(fd, size) != 0) {
        close(fd);
        return -1;
    }

    void* mapped = mmap(nullptr, size, PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        close(fd);
        return -1;
    }

    memcpy(mapped, data, size);
    munmap(mapped, size);

    return fd;
}

extern "C" int open(const char* pathname, int flags, ...) {
    RESOLVE(open);

    std::lock_guard<std::mutex> lock(g_MapMutex);

    auto it = embedded::registry.find(pathname);
    if (it != embedded::registry.end()) {
        int fd = create_memory_fd(it->second.data, it->second.size);
        if (fd == -1) return -1;

        g_VirtualFiles[fd] = { it->second.data, it->second.size, 0 };
        return fd;
    }

    int mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
        return real_open(pathname, flags, mode);
    }

    return real_open(pathname, flags);
}

extern "C" int open64(const char* pathname, int flags, ...) {
    RESOLVE(open64);

    std::lock_guard<std::mutex> lock(g_MapMutex);

    auto it = embedded::registry.find(pathname);
    if (it != embedded::registry.end()) {
        int fd = create_memory_fd(it->second.data, it->second.size);
        if (fd == -1) return -1;

        g_VirtualFiles[fd] = { it->second.data, it->second.size, 0 };
        return fd;
    }

    int mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
        return real_open64(pathname, flags, mode);
    }

    return real_open64(pathname, flags);
}

extern "C" int openat(int dirfd, const char* pathname, int flags, ...) {
    RESOLVE(openat);

    std::lock_guard<std::mutex> lock(g_MapMutex);

    auto it = embedded::registry.find(pathname);
    if (it != embedded::registry.end()) {
        int fd = create_memory_fd(it->second.data, it->second.size);
        if (fd == -1) return -1;

        g_VirtualFiles[fd] = { it->second.data, it->second.size, 0 };
        return fd;
    }

    int mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
        return real_openat(dirfd, pathname, flags, mode);
    }

    return real_openat(dirfd, pathname, flags);
}

extern "C" ssize_t read(int fd, void* buf, size_t count) {
    RESOLVE(read);

    std::lock_guard<std::mutex> lock(g_MapMutex);

    auto it = g_VirtualFiles.find(fd);
    if (it != g_VirtualFiles.end()) {
        VirtualFile& vf = it->second;

        size_t remaining = vf.size - vf.offset;
        size_t toRead = remaining < count ? remaining : count;

        if (toRead > 0) {
            memcpy(buf, vf.data + vf.offset, toRead);
            vf.offset += toRead;
            return toRead;
        }
        return 0;
    }

    return real_read(fd, buf, count);
}

extern "C" off_t lseek(int fd, off_t offset, int whence) {
    RESOLVE(lseek);

    std::lock_guard<std::mutex> lock(g_MapMutex);

    auto it = g_VirtualFiles.find(fd);
    if (it != g_VirtualFiles.end()) {
        VirtualFile& vf = it->second;

        off_t newPos = 0;
        switch (whence) {
            case SEEK_SET: newPos = offset; break;
            case SEEK_CUR: newPos = vf.offset + offset; break;
            case SEEK_END: newPos = vf.size + offset; break;
            default: return -1;
        }

        if (newPos < 0 || newPos > (off_t)vf.size) return -1;

        vf.offset = newPos;
        return newPos;
    }

    return real_lseek(fd, offset, whence);
}

extern "C" int close(int fd) {
    RESOLVE(close);

    std::lock_guard<std::mutex> lock(g_MapMutex);
    g_VirtualFiles.erase(fd);
    return real_close(fd);
}

static void fill_stat(struct stat* buf, size_t size) {
    memset(buf, 0, sizeof(*buf));
    buf->st_mode = S_IFREG | 0444;
    buf->st_nlink = 1;
    buf->st_size = size;
    buf->st_blksize = 512;
    buf->st_blocks = (size + 511) / 512;
}

extern "C" int stat(const char* path, struct stat* buf) {
    RESOLVE(stat);

    std::lock_guard<std::mutex> lock(g_MapMutex);

    auto it = embedded::registry.find(path);
    if (it != embedded::registry.end()) {
        fill_stat(buf, it->second.size);
        return 0;
    }

    return real_stat(path, buf);
}

extern "C" int fstat(int fd, struct stat* buf) {
    RESOLVE(fstat);

    std::lock_guard<std::mutex> lock(g_MapMutex);

    auto it = g_VirtualFiles.find(fd);
    if (it != g_VirtualFiles.end()) {
        fill_stat(buf, it->second.size);
        return 0;
    }

    return real_fstat(fd, buf);
}

extern "C" FILE* fopen(const char* path, const char* mode) {
    RESOLVE(fopen);

    std::lock_guard<std::mutex> lock(g_MapMutex);

    auto it = embedded::registry.find(path);
    if (it != embedded::registry.end()) {
        int fd = create_memory_fd(it->second.data, it->second.size);
        if (fd == -1) return nullptr;

        g_VirtualFiles[fd] = { it->second.data, it->second.size, 0 };

        FILE* f = fdopen(fd, mode);
        if (f) g_FileToFd[f] = fd;

        return f;
    }

    return real_fopen(path, mode);
}

extern "C" size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    RESOLVE(fread);

    std::lock_guard<std::mutex> lock(g_MapMutex);

    auto it_f = g_FileToFd.find(stream);
    if (it_f != g_FileToFd.end()) {
        int fd = it_f->second;
        auto it = g_VirtualFiles.find(fd);

        if (it != g_VirtualFiles.end()) {
            VirtualFile& vf = it->second;

            size_t requested = size * nmemb;
            size_t available = vf.size - vf.offset;
            size_t toRead = available < requested ? available : requested;

            if (toRead > 0) {
                memcpy(ptr, vf.data + vf.offset, toRead);
                vf.offset += toRead;
                return toRead / size;
            }
            return 0;
        }
    }

    return real_fread(ptr, size, nmemb, stream);
}

extern "C" int fclose(FILE* stream) {
    RESOLVE(fclose);

    std::lock_guard<std::mutex> lock(g_MapMutex);

    auto it = g_FileToFd.find(stream);
    if (it != g_FileToFd.end()) {
        g_VirtualFiles.erase(it->second);
        g_FileToFd.erase(it);
    }

    return real_fclose(stream);
}

extern "C" void InstallFileInterceptionHooks() { }