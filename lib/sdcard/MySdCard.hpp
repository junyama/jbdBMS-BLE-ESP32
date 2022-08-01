#ifndef MY_SDCARD_HPP
#define MY_SDCARD_HPP

#include "MyDebug.hpp"
#include <Arduino.h>

#include "FS.h"
#include "SD.h"
#include "SPI.h"

class MySdCard
{
private:
	static const String TAG;

public:
static void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
static void createDir(fs::FS &fs, const char *path);
static void removeDir(fs::FS &fs, const char *path);
static void removeDirR(fs::FS &fs, const char *path);
static void readFile(fs::FS &fs, const char *path);
static void writeFile(fs::FS &fs, const char *path, const char *message);
static void appendFile(fs::FS &fs, const char *path, const char *message);
static void renameFile(fs::FS &fs, const char *path1, const char *path2);
static void deleteFile(fs::FS &fs, const char *path);
static void testFileIO(fs::FS &fs, const char *path);
};

#endif /* MY_SDCARD_HPP */