#pragma once
#include "defines.h"
#include <string>
#include <vector>
#include <commdlg.h>
#include <Shlobj.h>
#undef MessageBox

HWND GetTopMostWindowInCurrentProcess();

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
    bool SupportMultiDottedExtensions;
    bool DereferenceLinks;
    bool ValidateNames;
    std::string Title;

    SaveFileDialog();
    DialogResult ShowDialog(HWND owner);
};

class FolderBrowserDialog {
public:
    std::string SelectedPath;
    std::string Description;
    bool ShowNewFolderButton;
    bool Multiselect; 

    FolderBrowserDialog();
    DialogResult ShowDialog(HWND owner);
};

class ColorDialog {
public:
    COLORREF Color;

    ColorDialog();
    DialogResult ShowDialog(HWND owner);
};

class FontDialog {
public:
    std::string FontName;
    int FontSize;
    COLORREF Color;
    bool Bold;
    bool Italic;
    bool Underline;
    bool Strikeout;

    FontDialog();
    DialogResult ShowDialog(HWND owner);
};

class MessageBox {
public:
    static DialogResult Show(const std::string& text, const std::string& caption = "", UINT type = MB_OK);
    static DialogResult Show(const std::wstring& text, const std::wstring& caption = L"", UINT type = MB_OK);
};
