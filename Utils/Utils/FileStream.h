#pragma once
#include "defines.h"
#include <string>
#include <fstream>

enum class FileMode {
	Read,
	Write,
	Append,
	ReadWrite
};
class FileStream {
public:
	FileStream(const std::string& filename, FileMode mode = FileMode::ReadWrite);
	~FileStream();
	long long Read(void* buffer, size_t size);
	template <class T>
	bool Read(T* buffer) {
		return Read(&buffer, sizeof(T)) == sizeof(T);
	}
	bool Write(const void* buffer, size_t size);
	template <class T>
	bool Write(T value) {
		return Write(&value,sizeof(T));
	}
	size_t Position();
	void Seek(size_t pos);
	void SeekToEnd();
	size_t Length();
	void Close();
private:
	std::fstream file_;
};

