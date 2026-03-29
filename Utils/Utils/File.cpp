#include "File.h"
#include <fstream>
#include <filesystem>
#include "StringHelper.h"
#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
bool File::Exists(const std::string path) {
	return std::filesystem::exists(path);
}
void File::Delete(const std::string path) {
	std::filesystem::remove(path);
}
void File::Copy(const std::string src, const std::string dest) {
	std::filesystem::copy(src, dest);
}
void File::Move(const std::string src, const std::string dest) {
	std::filesystem::rename(src, dest);
}
void File::Create(const std::string path) {
	std::ofstream fout(path);
	fout.close();
}
std::string File::ReadAllText(const std::string path) {
	if (std::ifstream file(path, std::ios::binary | std::ios::ate); file) {
		std::string data(file.tellg(), '\0');
		file.seekg(0);
		file.read(reinterpret_cast<char*>(data.data()), data.size());
		return data;
	}
	return {};
}
std::vector<uint8_t> File::ReadAllBytes(const std::string path) {
	if (std::ifstream file(path, std::ios::binary | std::ios::ate); file) {
		std::vector<uint8_t> data(file.tellg());
		file.seekg(0);
		file.read(reinterpret_cast<char*>(data.data()), data.size());
		return data;
	}
	return {};
}
std::vector<std::string> File::ReadAllLines(const std::string path) {
	std::string str = File::ReadAllText(path);
	return StringHelper::Split(str, { '\r','\n' });
}
void File::WriteAllText(const std::string path, const std::string content) {
	std::ofstream ofs(path, std::ios::binary);
	if (ofs) {
		ofs.write((char*)content.data(), content.size());
		ofs.close();
	}
}
void File::WriteAllBytes(const std::string path, const std::vector<uint8_t> content) {
	std::ofstream ofs(path, std::ios::binary);
	if (ofs) {
		ofs.write((char*)content.data(), content.size());
		ofs.close();
	}
}

void File::WriteAllBytes(const std::string path, const uint8_t* content, size_t size) {
	std::ofstream ofs(path, std::ios::binary);
	if (ofs) {
		ofs.write((char*)content, size);
		ofs.close();
	}
}
void File::WriteAllLines(const std::string path, const std::vector<std::string> content) {
	auto str = StringHelper::Join(content, "\n");
	File::WriteAllText(path, str);
}
void File::AppendAllText(const std::string path, const std::string content) {
	std::ofstream fout(path, std::ios::app);
	if (fout) {
		fout.write((char*)content.data(), content.size());
		fout.close();
	}
}
void File::AppendAllBytes(const std::string path, const std::vector<uint8_t> content) {
	std::ofstream fout(path, std::ios::app);
	if (fout) {
		fout.write((char*)content.data(), content.size());
		fout.close();
	}
}
void File::AppendAllLines(const std::string path, const std::vector<std::string> content) {
	auto str = StringHelper::Join(content, "\n");
	File::AppendAllText(path, str);
}
void File::SetAttributes(const std::string path, FileAttributes attributes) {
	SetFileAttributesA(path.c_str(), (DWORD)attributes);
}
FileAttributes File::GetAttributes(const std::string path) {
	return (FileAttributes)GetFileAttributesA(path.c_str());
}
void File::SetCreationTime(const std::string path, FILETIME time) {
	HANDLE pFile = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (pFile == INVALID_HANDLE_VALUE) {
		CloseHandle(pFile);
		return;
	}
	SetFileTime(pFile, &time, NULL, NULL);
	CloseHandle(pFile);
}
FILETIME File::GetCreationTime(const std::string path) {
	HANDLE pFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (pFile == INVALID_HANDLE_VALUE) {
		CloseHandle(pFile);
		return {};
	}
	FILETIME time;
	GetFileTime(pFile, &time, NULL, NULL);
	CloseHandle(pFile);
	return time;
}
void File::SetLastAccessTime(const std::string path, FILETIME time) {
	HANDLE pFile = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (pFile == INVALID_HANDLE_VALUE) {
		CloseHandle(pFile);
		return;
	}
	SetFileTime(pFile, NULL, &time, NULL);
	CloseHandle(pFile);
}
FILETIME File::GetLastAccessTime(const std::string path) {
	HANDLE pFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (pFile == INVALID_HANDLE_VALUE) {
		CloseHandle(pFile);
		return {};
	}
	FILETIME time;
	GetFileTime(pFile, NULL, &time, NULL);
	CloseHandle(pFile);
	return time;
}
void File::SetLastWriteTime(const std::string path, FILETIME time) {
	HANDLE pFile = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (pFile == INVALID_HANDLE_VALUE) {
		CloseHandle(pFile);
		return;
	}
	SetFileTime(pFile, NULL, NULL, &time);
	CloseHandle(pFile);
}
FILETIME File::GetLastWriteTime(const std::string path) {
	HANDLE pFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (pFile == INVALID_HANDLE_VALUE) {
		CloseHandle(pFile);
		return {};
	}
	FILETIME time;
	GetFileTime(pFile, NULL, NULL, &time);
	CloseHandle(pFile);
	return time;
}
void Directory::Create(std::string dirPath) {
	std::filesystem::create_directory(dirPath);
}
bool Directory::Exists(std::string dirPath) {
	return std::filesystem::exists(dirPath);
}
void Directory::Delete(std::string dirPath, bool recursive) {
	if (recursive) {
		std::filesystem::remove_all(dirPath);
	}
	else {
		std::filesystem::remove(dirPath);
	}
}
std::vector<FileInfo> Directory::GetFiles(std::string path) {
	std::vector<FileInfo> files;
	for (auto& p : std::filesystem::directory_iterator(path)) {
		if (std::filesystem::is_regular_file(p.status())) {
			files.push_back(p.path().string());
		}
	}
	return files;
}
std::vector<DirectoryInfo> Directory::GetDirectories(std::string path) {
	std::vector<DirectoryInfo> directories;
	for (auto& p : std::filesystem::directory_iterator(path)) {
		if (std::filesystem::is_directory(p.status())) {
			directories.push_back(p.path().string());
		}
	}
	return directories;
}