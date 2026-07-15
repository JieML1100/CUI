#include "FileInfo.h"

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

	std::string ToString(const std::filesystem::path& path) {
		return WideToUtf8(path.wstring());
	}
}

FileInfo::FileInfo(const std::string& path) {
	m_path = ToPath(path);
}
std::string FileInfo::Name() {
	return ToString(m_path.filename());
}
std::string FileInfo::DirectoryName() {
	return ToString(m_path.parent_path());
}
std::string FileInfo::Extension() {
	return ToString(m_path.extension());
}
std::string FileInfo::FullName() {
	return ToString(m_path);
}
void FileInfo::CopyTo(std::string dest) {
	std::filesystem::copy(m_path, ToPath(dest), std::filesystem::copy_options::overwrite_existing);
}
void FileInfo::MoveTo(std::string dest) {
	std::filesystem::rename(m_path, ToPath(dest));
}
bool FileInfo::Exists() {
	return std::filesystem::exists(m_path);
}
long FileInfo::Length() {
	if (Exists()) {
		return static_cast<long>(std::filesystem::file_size(m_path));
	}
	return 0;
}
void FileInfo::Create() {
	std::ofstream fout(m_path, std::ios::binary);
	fout.close();
}
void FileInfo::Delete() {
	if (Exists()) {
		std::filesystem::remove(m_path);
	}
}
DirectoryInfo::DirectoryInfo(const std::string& path) {
	dirPath = ToPath(path);
}
bool DirectoryInfo::Exists() {
	return std::filesystem::is_directory(dirPath);
}
void DirectoryInfo::Create() {
	std::filesystem::create_directories(dirPath);
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
	return ToString(dirPath.filename());
}
std::string DirectoryInfo::FullName() {
	return ToString(dirPath);
}
std::string DirectoryInfo::ParentDirectoryName() {
	return ToString(dirPath.parent_path());
}
std::vector<FileInfo> DirectoryInfo::GetFiles() {
	std::vector<FileInfo> files;
	for (auto& p : std::filesystem::directory_iterator(dirPath)) {
		if (std::filesystem::is_regular_file(p.status())) {
			files.push_back(ToString(p.path()));
		}
	}
	return files;
}
std::vector<DirectoryInfo> DirectoryInfo::GetDirectories() {
	std::vector<DirectoryInfo> directories;
	for (auto& p : std::filesystem::directory_iterator(dirPath)) {
		if (std::filesystem::is_directory(p.status())) {
			directories.push_back(ToString(p.path()));
		}
	}
	return directories;
}
