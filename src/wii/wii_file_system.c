#include "wii_file_system.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// ===[ Helpers ]===

// The caller must make sure to free the returned string!
static char* buildFullPath(WiiFileSystem* fs, const char* relativePath) {
    if (strstr(relativePath, fs->basePath) != nullptr) return safeStrdup(relativePath);
    size_t baseLen = strlen(fs->basePath);
    size_t relLen = strlen(relativePath);
    char* fullPath = safeMalloc(baseLen + relLen + 1);
    memcpy(fullPath, fs->basePath, baseLen);
    memcpy(fullPath + baseLen, relativePath, relLen);
    fullPath[baseLen + relLen] = '\0';
    return fullPath;
}

// ===[ Vtable Implementations ]===

// The caller must make sure to free the returned string!
static char* wiiResolvePath(FileSystem* fs, const char* relativePath) {
    return buildFullPath((WiiFileSystem*) fs, relativePath);
}

static bool wiiFileExists(FileSystem* fs, const char* relativePath) {
    char* fullPath = buildFullPath((WiiFileSystem*) fs, relativePath);
    struct stat st;
    bool exists = (stat(fullPath, &st) == 0);
    free(fullPath);
    return exists;
}

static char* wiiReadFileText(FileSystem* fs, const char* relativePath) {
    char* fullPath = buildFullPath((WiiFileSystem*) fs, relativePath);
    FILE* f = fopen(fullPath, "rb");
    free(fullPath);
    if (f == nullptr)
        return nullptr;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = safeMalloc((size_t) size + 1);
    size_t bytesRead = fread(content, 1, (size_t) size, f);
    content[bytesRead] = '\0';
    fclose(f);
    return content;
}

static bool wiiWriteFileText(FileSystem* fs, const char* relativePath, const char* contents) {
    char* fullPath = buildFullPath((WiiFileSystem*) fs, relativePath);
    FILE* f = fopen(fullPath, "wb");
    free(fullPath);
    if (f == nullptr)
        return false;

    size_t len = strlen(contents);
    size_t written = fwrite(contents, 1, len, f);
    fclose(f);
    return written == len;
}

static bool wiiDeleteFile(FileSystem* fs, const char* relativePath) {
    char* fullPath = buildFullPath((WiiFileSystem*) fs, relativePath);
    int result = remove(fullPath);
    free(fullPath);
    return result == 0;
}

// ===[ Vtable ]===

static FileSystemVtable wiiFileSystemVtable = {
    .resolvePath = wiiResolvePath,
    .fileExists = wiiFileExists,
    .readFileText = wiiReadFileText,
    .writeFileText = wiiWriteFileText,
    .deleteFile = wiiDeleteFile,
};

// ===[ Lifecycle ]===

WiiFileSystem* WiiFileSystem_create(const char* dataWinPath) {
    WiiFileSystem* fs = safeCalloc(1, sizeof(WiiFileSystem));
    fs->base.vtable = &wiiFileSystemVtable;

    // Derive basePath by stripping the filename from dataWinPath
    const char* lastSlash = strrchr(dataWinPath, '/');
    const char* lastBackslash = strrchr(dataWinPath, '\\');
    if (lastBackslash != nullptr && (lastSlash == nullptr || lastBackslash > lastSlash))
        lastSlash = lastBackslash;
    if (lastSlash != nullptr) {
        size_t dirLen = (size_t) (lastSlash - dataWinPath + 1); // include the trailing /
        fs->basePath = safeMalloc(dirLen + 1);
        memcpy(fs->basePath, dataWinPath, dirLen);
        fs->basePath[dirLen] = '\0';
    } else {
        // data.win is in current directory
        fs->basePath = safeStrdup("./");
    }

    return fs;
}

void WiiFileSystem_destroy(WiiFileSystem* fs) {
    if (fs == nullptr) return;
    free(fs->basePath);
    free(fs);
}
