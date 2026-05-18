#include "TestUtils.h"

#include <algorithm>
#include <cwctype>
#include <cstdarg>
#include <windows.h>

namespace {
    std::mt19937& TestRandomEngine()
    {
        static std::mt19937 engine(std::random_device{}());
        return engine;
    }
}

std::wstring StringHelper::Format(const wchar_t* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    va_list argsCopy;
    va_copy(argsCopy, args);

    const int size = _vscwprintf(fmt, args);
    std::wstring result(size, L'\0');
    vswprintf_s(result.data(), result.size() + 1, fmt, argsCopy);

    va_end(argsCopy);
    va_end(args);
    return result;
}

std::string StringHelper::Format(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    va_list argsCopy;
    va_copy(argsCopy, args);

    const int size = _vscprintf(fmt, args);
    std::string result(size, '\0');
    vsprintf_s(result.data(), result.size() + 1, fmt, argsCopy);

    va_end(argsCopy);
    va_end(args);
    return result;
}

std::string StringHelper::ToLower(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return str;
}

std::wstring StringHelper::ToLower(std::wstring str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return str;
}

bool StringHelper::Contains(const std::string& str, const std::string& substr)
{
    return str.find(substr) != std::string::npos;
}

bool StringHelper::Contains(const std::wstring& str, const std::wstring& substr)
{
    return str.find(substr) != std::wstring::npos;
}

std::wstring Convert::string_to_wstring(const std::string& str)
{
    if (str.empty()) {
        return {};
    }

    const int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), static_cast<int>(str.size()), result.data(), len);
    return result;
}

std::string Convert::wstring_to_string(const std::wstring& str)
{
    if (str.empty()) {
        return {};
    }

    const int len = WideCharToMultiByte(CP_ACP, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_ACP, 0, str.c_str(), static_cast<int>(str.size()), result.data(), len, nullptr, nullptr);
    return result;
}

OpenFileDialog::OpenFileDialog()
    : Filter("All Files\0*.*\0"),
      FilterIndex(0),
      Multiselect(false),
      SupportMultiDottedExtensions(false),
      DereferenceLinks(true),
      ValidateNames(true),
      Title("Open File")
{
}

DialogResult OpenFileDialog::ShowDialog(HWND owner)
{
    std::string filter = Filter;
    filter.append(2, '\0');

    OPENFILENAMEA ofn{};
    CHAR buffer[4096] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.hInstance = GetModuleHandle(nullptr);
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = static_cast<DWORD>(sizeof(buffer));
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = FilterIndex;
    ofn.lpstrTitle = Title.empty() ? nullptr : Title.c_str();
    ofn.lpstrInitialDir = InitialDirectory.empty() ? nullptr : InitialDirectory.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (Multiselect) {
        ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    }
    if (SupportMultiDottedExtensions) {
        ofn.Flags |= OFN_EXPLORER;
    }
    if (!DereferenceLinks) {
        ofn.Flags |= OFN_NODEREFERENCELINKS;
    }
    if (!ValidateNames) {
        ofn.Flags |= OFN_NOVALIDATE;
    }

    if (!GetOpenFileNameA(&ofn)) {
        return CommDlgExtendedError() != 0 ? DialogResult::Abort : DialogResult::Cancel;
    }

    SelectedPaths.clear();
    if (Multiselect) {
        const char* p = ofn.lpstrFile;
        std::string directory = p;
        p += directory.size() + 1;
        while (*p) {
            std::string filename = p;
            SelectedPaths.push_back(directory + "\\" + filename);
            p += filename.size() + 1;
        }
        if (SelectedPaths.empty()) {
            SelectedPaths.push_back(directory);
        }
    }
    else {
        SelectedPaths.push_back(ofn.lpstrFile);
    }

    return DialogResult::OK;
}

SaveFileDialog::SaveFileDialog()
    : Filter("All Files\0*.*\0"),
      FilterIndex(0),
      DereferenceLinks(true),
      ValidateNames(true),
      Title("Save File")
{
}

DialogResult SaveFileDialog::ShowDialog(HWND owner)
{
    std::string filter = Filter;
    filter.append(2, '\0');

    OPENFILENAMEA ofn{};
    CHAR buffer[4096] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.hInstance = GetModuleHandle(nullptr);
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = static_cast<DWORD>(sizeof(buffer));
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = FilterIndex;
    ofn.lpstrTitle = Title.empty() ? nullptr : Title.c_str();
    ofn.lpstrInitialDir = InitialDirectory.empty() ? nullptr : InitialDirectory.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (!DereferenceLinks) {
        ofn.Flags |= OFN_NODEREFERENCELINKS;
    }
    if (!ValidateNames) {
        ofn.Flags |= OFN_NOVALIDATE;
    }

    if (!GetSaveFileNameA(&ofn)) {
        return CommDlgExtendedError() != 0 ? DialogResult::Abort : DialogResult::Cancel;
    }

    SelectedPath = ofn.lpstrFile;
    return DialogResult::OK;
}

FileInfo::FileInfo(const std::string& path)
    : _path(path)
{
}

std::string FileInfo::Extension() const
{
    return _path.extension().string();
}

std::string FileInfo::FullName() const
{
    return _path.string();
}

std::string File::ReadAllText(const std::string& path)
{
    if (std::ifstream file(path, std::ios::binary | std::ios::ate); file) {
        std::string data(static_cast<size_t>(file.tellg()), '\0');
        file.seekg(0);
        file.read(data.data(), static_cast<std::streamsize>(data.size()));
        return data;
    }
    return {};
}

int Random::Next()
{
    std::uniform_int_distribution<int> dist(0, INT_MAX);
    return dist(TestRandomEngine());
}

std::string MakeDialogFilterStrring(std::string description, std::string filter)
{
    std::string result = std::move(description);
    result.append("(");
    result.append(filter);
    result.append(")");
    result.push_back('\0');
    result.append(filter);
    result.append(2, '\0');
    return result;
}
