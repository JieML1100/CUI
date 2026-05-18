#pragma once

#include <windows.h>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

template<typename T>
using List = std::vector<T>;

enum class DialogResult {
    None,
    OK,
    Cancel,
    Abort,
    Retry,
    Ignore,
    Yes,
    No
};

class StringHelper {
public:
    static std::wstring Format(const wchar_t* fmt, ...);
    static std::string Format(const char* fmt, ...);
    static std::string ToLower(std::string str);
    static std::wstring ToLower(std::wstring str);
    static bool Contains(const std::string& str, const std::string& substr);
    static bool Contains(const std::wstring& str, const std::wstring& substr);
};

class Convert {
public:
    static std::wstring string_to_wstring(const std::string& str);
    static std::string wstring_to_string(const std::wstring& str);
};

class OpenFileDialog {
public:
    std::vector<std::string> SelectedPaths;
    std::string InitialDirectory;
    std::string Filter;
    int FilterIndex;
    bool Multiselect;
    bool SupportMultiDottedExtensions;
    bool DereferenceLinks;
    bool ValidateNames;
    std::string Title;

    OpenFileDialog();
    DialogResult ShowDialog(HWND owner);
};

class SaveFileDialog {
public:
    std::string SelectedPath;
    std::string InitialDirectory;
    std::string Filter;
    int FilterIndex;
    bool DereferenceLinks;
    bool ValidateNames;
    std::string Title;

    SaveFileDialog();
    DialogResult ShowDialog(HWND owner);
};

class FileInfo {
public:
    explicit FileInfo(const std::string& path);
    std::string Extension() const;
    std::string FullName() const;

private:
    std::filesystem::path _path;
};

class File {
public:
    static std::string ReadAllText(const std::string& path);
};

class Random {
public:
    static int Next();
};

//示例: MakeDialogFilterStrring("图片文件", "*.jpg;*.jpeg;*.png;*.bmp;*.svg;*.webp")
std::string MakeDialogFilterStrring(std::string description, std::string filter);
