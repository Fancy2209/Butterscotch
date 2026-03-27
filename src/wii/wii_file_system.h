#pragma once

#include "file_system.h"

typedef struct {
    FileSystem base;
    char* basePath; // directory containing data.win, with trailing separator
} WiiFileSystem;

// Creates a WiiFileSystem from the path to the data.win file
// The basePath is derived by stripping the filename from dataWinPath.
WiiFileSystem* WiiFileSystem_create(const char* dataWinPath);
void WiiFileSystem_destroy(WiiFileSystem* fs);
