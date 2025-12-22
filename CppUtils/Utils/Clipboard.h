#pragma once
#include "defines.h"
#include <string>
#include <vector>
class Clipboard {
public:
	static std::string GetText();
	static void SetText(std::string str);
	static void Clear();
	static void SetFile(std::string files);
	static void SetFiles(std::vector<std::string> files);
	static std::string GetFile();
	static std::vector<std::string> GetFiles();
	static void SetImage(HBITMAP bmp);
	static HBITMAP GetImage();
	static UINT GetFormat();
};

