#include "File.h"
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include "StringHelper.h"

#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)

namespace {
	std::wstring PathToWide(const std::string& path) {
		if (path.empty()) return L"";
		int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
			path.data(), static_cast<int>(path.size()), nullptr, 0);
		UINT cp = CP_UTF8;
		DWORD flags = MB_ERR_INVALID_CHARS;
		if (len <= 0) {
			cp = CP_ACP;
			flags = 0;
			len = MultiByteToWideChar(cp, flags, path.data(), static_cast<int>(path.size()), nullptr, 0);
		}
		if (len <= 0) return L"";
		std::wstring out(static_cast<size_t>(len), L'\0');
		MultiByteToWideChar(cp, flags, path.data(), static_cast<int>(path.size()), out.data(), len);
		return out;
	}

	std::filesystem::path ToPath(const std::string& path) {
		return std::filesystem::path(PathToWide(path));
	}

	std::string WideToUtf8(const std::wstring& text) {
		if (text.empty()) return "";
		int len = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
			nullptr, 0, nullptr, nullptr);
		if (len <= 0) return "";
		std::string out(static_cast<size_t>(len), '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
			out.data(), len, nullptr, nullptr);
		return out;
	}

	std::string PathToString(const std::filesystem::path& path) {
		return WideToUtf8(path.wstring());
	}

	HANDLE OpenFileHandle(const std::string& path, DWORD access) {
		std::wstring wpath = PathToWide(path);
		return CreateFileW(wpath.c_str(), access,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}
}

bool File::Exists(const std::string path) {
	return std::filesystem::exists(ToPath(path));
}
void File::Delete(const std::string path) {
	std::filesystem::remove(ToPath(path));
}
void File::Copy(const std::string src, const std::string dest) {
	std::filesystem::copy(ToPath(src), ToPath(dest), std::filesystem::copy_options::overwrite_existing);
}
void File::Move(const std::string src, const std::string dest) {
	std::filesystem::rename(ToPath(src), ToPath(dest));
}
void File::Create(const std::string path) {
	std::ofstream fout(ToPath(path), std::ios::binary);
	if (!fout) throw std::runtime_error("Failed to create file: " + path);
}
std::string File::ReadAllText(const std::string path) {
	if (std::ifstream file(ToPath(path), std::ios::binary | std::ios::ate); file) {
		std::string data(static_cast<size_t>(file.tellg()), '\0');
		file.seekg(0);
		file.read(reinterpret_cast<char*>(data.data()), data.size());
		return data;
	}
	return {};
}
std::vector<uint8_t> File::ReadAllBytes(const std::string path) {
	if (std::ifstream file(ToPath(path), std::ios::binary | std::ios::ate); file) {
		std::vector<uint8_t> data(static_cast<size_t>(file.tellg()));
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
	std::ofstream ofs(ToPath(path), std::ios::binary | std::ios::trunc);
	if (!ofs) throw std::runtime_error("Failed to write file: " + path);
	ofs.write(content.data(), content.size());
}
void File::WriteAllBytes(const std::string path, const std::vector<uint8_t> content) {
	std::ofstream ofs(ToPath(path), std::ios::binary | std::ios::trunc);
	if (!ofs) throw std::runtime_error("Failed to write file: " + path);
	ofs.write(reinterpret_cast<const char*>(content.data()), content.size());
}
void File::WriteAllBytes(const std::string path, const uint8_t* content, size_t size) {
	std::ofstream ofs(ToPath(path), std::ios::binary | std::ios::trunc);
	if (!ofs) throw std::runtime_error("Failed to write file: " + path);
	ofs.write(reinterpret_cast<const char*>(content), size);
}
void File::WriteAllLines(const std::string path, const std::vector<std::string> content) {
	auto str = StringHelper::Join(content, "\n");
	File::WriteAllText(path, str);
}
void File::AppendAllText(const std::string path, const std::string content) {
	std::ofstream fout(ToPath(path), std::ios::binary | std::ios::app);
	if (!fout) throw std::runtime_error("Failed to append file: " + path);
	fout.write(content.data(), content.size());
}
void File::AppendAllBytes(const std::string path, const std::vector<uint8_t> content) {
	std::ofstream fout(ToPath(path), std::ios::binary | std::ios::app);
	if (!fout) throw std::runtime_error("Failed to append file: " + path);
	fout.write(reinterpret_cast<const char*>(content.data()), content.size());
}
void File::AppendAllLines(const std::string path, const std::vector<std::string> content) {
	auto str = StringHelper::Join(content, "\n");
	File::AppendAllText(path, str);
}
void File::SetAttributes(const std::string path, FileAttributes attributes) {
	SetFileAttributesW(PathToWide(path).c_str(), (DWORD)attributes);
}
FileAttributes File::GetAttributes(const std::string path) {
	return (FileAttributes)GetFileAttributesW(PathToWide(path).c_str());
}
void File::SetCreationTime(const std::string path, FILETIME time) {
	HANDLE pFile = OpenFileHandle(path, FILE_WRITE_ATTRIBUTES);
	if (pFile == INVALID_HANDLE_VALUE) return;
	SetFileTime(pFile, &time, NULL, NULL);
	CloseHandle(pFile);
}
FILETIME File::GetCreationTime(const std::string path) {
	HANDLE pFile = OpenFileHandle(path, FILE_READ_ATTRIBUTES);
	if (pFile == INVALID_HANDLE_VALUE) return {};
	FILETIME time{};
	GetFileTime(pFile, &time, NULL, NULL);
	CloseHandle(pFile);
	return time;
}
void File::SetLastAccessTime(const std::string path, FILETIME time) {
	HANDLE pFile = OpenFileHandle(path, FILE_WRITE_ATTRIBUTES);
	if (pFile == INVALID_HANDLE_VALUE) return;
	SetFileTime(pFile, NULL, &time, NULL);
	CloseHandle(pFile);
}
FILETIME File::GetLastAccessTime(const std::string path) {
	HANDLE pFile = OpenFileHandle(path, FILE_READ_ATTRIBUTES);
	if (pFile == INVALID_HANDLE_VALUE) return {};
	FILETIME time{};
	GetFileTime(pFile, NULL, &time, NULL);
	CloseHandle(pFile);
	return time;
}
void File::SetLastWriteTime(const std::string path, FILETIME time) {
	HANDLE pFile = OpenFileHandle(path, FILE_WRITE_ATTRIBUTES);
	if (pFile == INVALID_HANDLE_VALUE) return;
	SetFileTime(pFile, NULL, NULL, &time);
	CloseHandle(pFile);
}
FILETIME File::GetLastWriteTime(const std::string path) {
	HANDLE pFile = OpenFileHandle(path, FILE_READ_ATTRIBUTES);
	if (pFile == INVALID_HANDLE_VALUE) return {};
	FILETIME time{};
	GetFileTime(pFile, NULL, NULL, &time);
	CloseHandle(pFile);
	return time;
}
void Directory::Create(std::string dirPath) {
	std::filesystem::create_directories(ToPath(dirPath));
}
bool Directory::Exists(std::string dirPath) {
	return std::filesystem::is_directory(ToPath(dirPath));
}
void Directory::Delete(std::string dirPath, bool recursive) {
	if (recursive) {
		std::filesystem::remove_all(ToPath(dirPath));
	}
	else {
		std::filesystem::remove(ToPath(dirPath));
	}
}
std::vector<FileInfo> Directory::GetFiles(std::string path) {
	std::vector<FileInfo> files;
	for (auto& p : std::filesystem::directory_iterator(ToPath(path))) {
		if (std::filesystem::is_regular_file(p.status())) {
			files.push_back(PathToString(p.path()));
		}
	}
	return files;
}
std::vector<DirectoryInfo> Directory::GetDirectories(std::string path) {
	std::vector<DirectoryInfo> directories;
	for (auto& p : std::filesystem::directory_iterator(ToPath(path))) {
		if (std::filesystem::is_directory(p.status())) {
			directories.push_back(PathToString(p.path()));
		}
	}
	return directories;
}
