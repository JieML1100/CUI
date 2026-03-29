#pragma once
#include "defines.h"
#include <filesystem>
#include <fstream>
class FileInfo {
private:
    std::filesystem::path m_path;
public:
    FileInfo(const std::string& path);
    std::string Name();
    std::string DirectoryName();
    std::string Extension();
    std::string FullName();
    void CopyTo(std::string dest);
    void MoveTo(std::string dest);
    bool Exists();
    long Length();
    void Create();
    void Delete();
};
class DirectoryInfo {
private:
    std::filesystem::path dirPath;
public:
    DirectoryInfo(const std::string& path);
    bool Exists();
    void Create();
    void Delete(bool recursive = false);
    std::string Name();
    std::string FullName();
    std::string ParentDirectoryName();
    std::vector<FileInfo> GetFiles();
    std::vector<DirectoryInfo> GetDirectories();
};