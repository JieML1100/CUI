#include "FileStream.h"
#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
FileStream::FileStream(const std::string& filename, FileMode mode) {
	std::ios_base::openmode open_mode = std::ios_base::binary;
	switch (mode) {
	case FileMode::Read:
		open_mode |= std::ios_base::in;
		break;
	case FileMode::Write:
		open_mode |= std::ios_base::out;
		break;
	case FileMode::Append:
		open_mode |= std::ios_base::app;
		break;
	case FileMode::ReadWrite:
		open_mode |= std::ios_base::in | std::ios_base::out;
		break;
	default:
		throw std::invalid_argument("Invalid mode");
	}
	file_.open(filename, open_mode);
	if (!file_) {
		throw std::runtime_error("Failed to open file");
	}
}
FileStream::~FileStream() {
	if (file_.is_open()) file_.close();
}
long long FileStream::Read(void* buffer, size_t size) {
	return (long long)file_.read(static_cast<char*>(buffer), size).gcount();
}
bool FileStream::Write(const void* buffer, size_t size) {
	return file_.write(static_cast<const char*>(buffer), size).good();
}
size_t FileStream::Position() {
	return file_.tellg();
}
void FileStream::Seek(size_t pos) {
	file_.seekg(pos);
}
void FileStream::SeekToEnd() {
	file_.seekg(0, std::ios_base::end);
}
size_t FileStream::Length() {
	auto pos = file_.tellg();
	file_.seekg(0, std::ios_base::end);
	auto length = file_.tellg();
	file_.seekg(pos);
	return length;
}

void FileStream::Close() {
	if (file_.is_open()) file_.close();
}