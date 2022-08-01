#ifndef MY_SDCARD_CPP
#define MY_SDCARD_CPP

#include "MySdCard.hpp"

using namespace MyLOG;

const String MySdCard::TAG = "MySdCard";

void MySdCard::listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    LOGD(TAG, "Listing directory: " + String(dirname));

    File root = fs.open(dirname);
    if (!root)
    {
        LOGD(TAG, "Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        LOGD(TAG, "Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            // Serial.print("  DIR : ");
            LOGD(TAG, "DIR : " + String(file.name()));
            if (levels)
            {
                listDir(fs, file.name(), levels - 1);
            }
        }
        else
        {
            // Serial.print("  FILE: ");
            // Serial.print(file.name());
            // Serial.print("  SIZE: ");
            LOGD(TAG, "FILE: " + String(file.name()) + "  SIZE: " + String(file.size()));
        }
        file = root.openNextFile();
    }
}

void MySdCard::createDir(fs::FS &fs, const char *path)
{
   LOGD(TAG, "Creating Dir: " + String(path));
    if (fs.mkdir(path))
    {
        LOGD(TAG, "Directry created");
    }
    else
    {
        LOGD(TAG, "mkdir failed");
    }
}

void MySdCard::removeDir(fs::FS &fs, const char *path)
{
    LOGD(TAG, "Removing an empty Dir: " + String(path));
    if (fs.rmdir(path))
    {
        LOGD(TAG, "Directry removed");
    }
    else
    {
        LOGD(TAG, "rmdir failed");
    }
}

void MySdCard::removeDirR(fs::FS &fs, const char *path)
{
    LOGD(TAG, "Removing directory recursively: " + String(path));
    File root = fs.open(path);
    if (!root)
    {
        LOGD(TAG, "Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        LOGD(TAG, "Not a directory");
        return;
    }
    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            LOGD(TAG, "DIR : " + String(file.name()));
            removeDirR(fs, file.name());
        }
        else
        {
            LOGD(TAG, "removing FILE: " + String(file.name()) + "  SIZE: " + String(file.size()));
            fs.remove(file.name());
        }
        file = root.openNextFile();
    }
    //LOGD(TAG, "Removing directory: " + String(path));
    removeDir(fs, path);
    //LOGD(TAG, "Removed directory: " + String(path));
}

void MySdCard::readFile(fs::FS &fs, const char *path)
{
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if (!file)
    {
        LOGD(TAG, "Failed to open file for reading");
        return;
    }

    LOGD(TAG, "Read from file: ");
    while (file.available())
    {
        Serial.write(file.read());
    }
    file.close();
}

void MySdCard::writeFile(fs::FS &fs, const char *path, const char *message)
{
    LOGD(TAG, "Writing file: " + String(path));

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        LOGD(TAG, "Failed to open file for writing");
        return;
    }
    if (file.print(message))
    {
        LOGD(TAG, "File written");
    }
    else
    {
        LOGD(TAG, "Write failed");
    }
    file.close();
}

void MySdCard::appendFile(fs::FS &fs, const char *path, const char *message)
{
    LOGD(TAG, "Appending to file: " + String(path));

    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        LOGD(TAG, "Failed to open file for appending");
        return;
    }
    if (file.print(message))
    {
        LOGD(TAG, "Message appended");
    }
    else
    {
        LOGD(TAG, "Append failed");
    }
    file.close();
}

void MySdCard::renameFile(fs::FS &fs, const char *path1, const char *path2)
{
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2))
    {
        LOGD(TAG, "File renamed");
    }
    else
    {
        LOGD(TAG, "Rename failed");
    }
}

void MySdCard::deleteFile(fs::FS &fs, const char *path)
{
    Serial.printf("Deleting file: %s\n", path);
    if (fs.remove(path))
    {
        LOGD(TAG, "File deleted");
    }
    else
    {
        LOGD(TAG, "Delete failed");
    }
}

void MySdCard::testFileIO(fs::FS &fs, const char *path)
{
    File file = fs.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if (file)
    {
        len = file.size();
        size_t flen = len;
        start = millis();
        while (len)
        {
            size_t toRead = len;
            if (toRead > 512)
            {
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %u ms\n", flen, end);
        file.close();
    }
    else
    {
        LOGD(TAG, "Failed to open file for reading");
    }

    file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        LOGD(TAG, "Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for (i = 0; i < 2048; i++)
    {
        file.write(buf, 512);
    }
    end = millis() - start;
    Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
    file.close();
}

#endif /* MY_SDCARD_CPP */