#include "FileInfo.h"

#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
FileInfo::FileInfo(const std::string& path) {
    m_path = std::filesystem::path(path);
}
std::string FileInfo::Name() {
    return m_path.filename().string();
}
std::string FileInfo::DirectoryName() {
    return m_path.parent_path().string();
}
std::string FileInfo::Extension() {
    return m_path.extension().string();
}
std::string FileInfo::FullName() {
    return m_path.string();
}
void FileInfo::CopyTo(std::string dest) {
    std::filesystem::copy(m_path, dest, std::filesystem::copy_options::overwrite_existing);
}
void FileInfo::MoveTo(std::string dest) {
    std::filesystem::rename(m_path, dest);
}
bool FileInfo::Exists() {
    return std::filesystem::exists(m_path);
}
long FileInfo::Length() {
    if (Exists()) {
        return std::filesystem::file_size(m_path);
    }
    return NULL;
}
void FileInfo::Create() {
    std::ofstream fout(m_path);
    fout.close();
}
void FileInfo::Delete() {
    if (Exists()) {
        std::filesystem::remove(m_path);
    }
}
DirectoryInfo::DirectoryInfo(const std::string& path) {
    dirPath = std::filesystem::path(path);
}
bool DirectoryInfo::Exists() {
    return std::filesystem::exists(dirPath);
}
void DirectoryInfo::Create() {
    std::filesystem::create_directory(dirPath);
}
void DirectoryInfo::Delete(bool recursive) {
    if (recursive) {
        std::filesystem::remove_all(dirPath);
    }
    else {
        std::filesystem::remove(dirPath);
    }
}
std::string DirectoryInfo::Name() {
	return dirPath.filename().string();
}
std::string DirectoryInfo::FullName() {
	return dirPath.string();
}
std::string DirectoryInfo::ParentDirectoryName() {
    return dirPath.parent_path().string();
}
std::vector<FileInfo> DirectoryInfo::GetFiles() {
    std::vector<FileInfo> files;
    for (auto& p : std::filesystem::directory_iterator(dirPath)) {
        if (std::filesystem::is_regular_file(p.status())) {
            files.push_back(p.path().string());
        }
    }
    return files;
}
std::vector<DirectoryInfo> DirectoryInfo::GetDirectories() {
    std::vector<DirectoryInfo> directories;
    for (auto& p : std::filesystem::directory_iterator(dirPath)) {
        if (std::filesystem::is_directory(p.status())) {
            directories.push_back(p.path().string());
        }
    }
    return directories;
}