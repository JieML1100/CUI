#pragma once
#include "defines.h"
#include "FileInfo.h"
#include <vector>
#include <string>
enum class FileAttributes {
    ReadOnly = 0x1,
    Hidden = 0x2,
    System = 0x4,
    Directory = 0x10,
    Archive = 0x20,
    Device = 0x40,
    Normal = 0x80,
    Temporary = 0x100,
    SparseFile = 0x200,
    ReparsePoint = 0x400,
    Compressed = 0x800,
    Offline = 0x1000,
    NotContentIndexed = 0x2000,
    Encrypted = 0x4000,
    IntegrityStream = 0x8000,
    NoScrubData = 0x20000
};
class File {
public:
	static bool Exists(const std::string path);
	static void Delete(const std::string path);
	static void Copy(const std::string src, const std::string dest);
	static void Move(const std::string src, const std::string dest);
	static void Create(const std::string path);
	static std::string ReadAllText(const std::string path);
	static std::vector<uint8_t> ReadAllBytes(const std::string path);
	static std::vector<std::string> ReadAllLines(const std::string path);
    static void WriteAllText(const std::string path, const std::string content);
    static void WriteAllBytes(const std::string path, const std::vector<uint8_t> content);
    static void WriteAllBytes(const std::string path, const uint8_t* content,size_t size);
    static void WriteAllLines(const std::string path, const std::vector<std::string> content);
    static void AppendAllText(const std::string path, const std::string content);
    static void AppendAllBytes(const std::string path, const std::vector<uint8_t> content);
    static void AppendAllLines(const std::string path, const std::vector<std::string> content);
    static void SetAttributes(const std::string path, FileAttributes attributes);
    static FileAttributes GetAttributes(const std::string path);
    static void SetCreationTime(const std::string path, FILETIME time);
    static FILETIME GetCreationTime(const std::string path);
    static void SetLastAccessTime(const std::string path, FILETIME time);
    static FILETIME GetLastAccessTime(const std::string path);
    static void SetLastWriteTime(const std::string path, FILETIME time);
    static FILETIME GetLastWriteTime(const std::string path);
};
class Directory {
public:
    static void Create(std::string dirPath);
    static bool Exists(std::string dirPath);
    static void Delete(std::string dirPath, bool recursive = false);
    static std::vector<FileInfo> GetFiles(std::string path);
    static std::vector<DirectoryInfo> GetDirectories(std::string path);
};

