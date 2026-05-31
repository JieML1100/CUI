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
	auto pos = file_.tellg();
	if (pos == std::streampos(-1)) {
		pos = file_.tellp();
	}
	return pos == std::streampos(-1) ? 0 : static_cast<size_t>(pos);
}
void FileStream::Seek(size_t pos) {
	file_.seekg(pos);
	file_.seekp(pos);
}
void FileStream::SeekToEnd() {
	file_.seekg(0, std::ios_base::end);
	file_.seekp(0, std::ios_base::end);
}
size_t FileStream::Length() {
	auto posG = file_.tellg();
	auto posP = file_.tellp();
	file_.seekg(0, std::ios_base::end);
	auto length = file_.tellg();
	if (length == std::streampos(-1)) {
		file_.clear();
		file_.seekp(0, std::ios_base::end);
		length = file_.tellp();
	}
	file_.clear();
	if (posG != std::streampos(-1)) file_.seekg(posG);
	if (posP != std::streampos(-1)) file_.seekp(posP);
	return length == std::streampos(-1) ? 0 : static_cast<size_t>(length);
}

bool FileStream::IsOpen() const {
	return file_.is_open();
}

void FileStream::Close() {
	if (file_.is_open()) file_.close();
}
