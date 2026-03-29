#include "Dialog.h"

HWND GetTopMostWindowInCurrentProcess()  {
	struct EnumParams {
		DWORD processID;
		HWND topMostWindow;
	};
	DWORD currentProcessID = GetCurrentProcessId();
	EnumParams params = { currentProcessID, NULL };

	EnumWindows(
		[](HWND hwnd, LPARAM lParam) {
		EnumParams* params = (EnumParams*)lParam;
		DWORD windowProcessID;
		GetWindowThreadProcessId(hwnd, &windowProcessID);
		if (windowProcessID == params->processID) {
			HWND hwndTop = hwnd;
			while (hwndTop != NULL) {
				hwndTop = GetWindow(hwndTop, GW_HWNDPREV);
				if (hwndTop == NULL) {
					params->topMostWindow = hwnd;
					break;
				}
			}
		}
		return TRUE;
	}, (LPARAM)&params);

	return params.topMostWindow;
}

OpenFileDialog::OpenFileDialog()
    : FilterIndex(0),
      Multiselect(false),
      SupportMultiDottedExtensions(false),
      DereferenceLinks(true),
      ValidateNames(true),
      Filter("All Files\0*.*\0"),
      Title("Open File") {
}

DialogResult OpenFileDialog::ShowDialog(HWND owner) {
    Filter.append(2, '\0');
    OPENFILENAMEA ofn;
    CHAR szFile[1024] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.hInstance = GetModuleHandle(NULL);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = Filter.c_str();
    ofn.nFilterIndex = FilterIndex;
    ofn.lpstrTitle = Title.c_str();
    ofn.lpstrInitialDir = InitialDirectory.empty() ? NULL : InitialDirectory.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (Multiselect) {
        ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    }
    if (!DereferenceLinks) {
        ofn.Flags |= OFN_NODEREFERENCELINKS;
    }
    if (!ValidateNames) {
        ofn.Flags |= OFN_NOVALIDATE;
    }

    if (GetOpenFileNameA(&ofn) == TRUE) {
        SelectedPaths.clear();
        if (Multiselect) {
            CHAR* p = ofn.lpstrFile;
            std::string directory = p;
            p += directory.size() + 1;
            while (*p) {
                std::string filename = p;
                std::string fullpath = directory + "\\" + filename;
                SelectedPaths.push_back(fullpath);
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
    else {
        DWORD err = CommDlgExtendedError();
        if (err != 0) {
            return DialogResult::Abort;
        }
        else {
            return DialogResult::Cancel;
        }
    }
}

SaveFileDialog::SaveFileDialog()
    : FilterIndex(0),
      SupportMultiDottedExtensions(false),
      DereferenceLinks(true),
      ValidateNames(true),
      Filter("All Files\0*.*\0"),
      Title("Save File") {
}

DialogResult SaveFileDialog::ShowDialog(HWND owner) {
    Filter.append(2, '\0');
    OPENFILENAMEA ofn;
    CHAR szFile[1024] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.hInstance = GetModuleHandle(NULL);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = Filter.c_str();
    ofn.nFilterIndex = FilterIndex;
    ofn.lpstrTitle = Title.c_str();
    ofn.lpstrInitialDir = InitialDirectory.empty() ? NULL : InitialDirectory.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (SupportMultiDottedExtensions) {
        ofn.Flags |= OFN_EXPLORER | OFN_ENABLESIZING | OFN_ALLOWMULTISELECT | OFN_ENABLEHOOK;
    }

    if (GetSaveFileNameA(&ofn) == TRUE) {
        SelectedPath = ofn.lpstrFile;
        return DialogResult::OK;
    }
    else {
        return DialogResult::Cancel;
    }
}

FolderBrowserDialog::FolderBrowserDialog()
    : ShowNewFolderButton(true),
      Multiselect(false) {
}

DialogResult FolderBrowserDialog::ShowDialog(HWND owner) {
    BROWSEINFOA bi = { 0 };
    bi.hwndOwner = owner;
    bi.pidlRoot = NULL;
    bi.pszDisplayName = NULL;
    bi.lpszTitle = Description.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS;
    if (ShowNewFolderButton) {
        bi.ulFlags |= BIF_NEWDIALOGSTYLE;
    }
    LPITEMIDLIST idl = SHBrowseForFolderA(&bi);
    if (idl == NULL) {
        return DialogResult::Cancel;
    }
    else {
        char path[MAX_PATH] = { 0 };
        SHGetPathFromIDListA(idl, path);
        SelectedPath = path;
        return DialogResult::OK;
    }
}

ColorDialog::ColorDialog()
    : Color(RGB(0, 0, 0)) {
}

DialogResult ColorDialog::ShowDialog(HWND owner) {
    CHOOSECOLORA cc = { 0 };
    static COLORREF acrCustClr[16];
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = owner;
    cc.lpCustColors = (LPDWORD)acrCustClr;
    cc.rgbResult = Color;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorA(&cc)) {
        Color = cc.rgbResult;
        return DialogResult::OK;
    }
    else {
        return DialogResult::Cancel;
    }
}

FontDialog::FontDialog()
    : FontSize(9),
      Color(RGB(0, 0, 0)),
      Bold(false),
      Italic(false),
      Underline(false),
      Strikeout(false) {
}

/**
 * @brief 显示字体选择对话框并获取用户选择的字体信息。
 *
 * @param owner 窗口句柄，表示对话框的拥有者。
 * @return DialogResult 返回对话框的结果，成功时返回 DialogResult::OK，失败时返回 DialogResult::Cancel。
 *
 * @details 此函数使用 Windows API 的 ChooseFontA 函数显示字体选择对话框。
 *          用户选择的字体信息将被存储在 FontName、FontSize、Color 等成员变量中。
 *          函数还会根据用户选择的字体高度和屏幕 DPI 计算字体大小。
 */
DialogResult FontDialog::ShowDialog(HWND owner) {
	CHOOSEFONTA cf = {0};
	static LOGFONTA lf = { 0 };
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = owner;
    cf.lpLogFont = &lf;
    cf.rgbColors = Color;
    cf.Flags = CF_SCREENFONTS | CF_EFFECTS;
    if (ChooseFontA(&cf)) {
        int fontSizeInPixels = abs(lf.lfHeight);
        int dpi = GetDeviceCaps(GetDC(cf.hwndOwner), LOGPIXELSY);
        int fontSize = MulDiv(fontSizeInPixels, 72, dpi);

        FontName = lf.lfFaceName;
        FontSize = fontSize;
        Color = cf.rgbColors;
        Bold = lf.lfWeight == FW_BOLD;
        Italic = lf.lfItalic;
        Underline = lf.lfUnderline;
        Strikeout = lf.lfStrikeOut;
        return DialogResult::OK;
    }
    else {
        return DialogResult::Cancel;
    }
}

DialogResult MessageBox::Show(const std::string& text, const std::string& caption, UINT type) {
    return (DialogResult)MessageBoxA(GetTopMostWindowInCurrentProcess(), text.c_str(), caption.c_str(), type);
}

DialogResult MessageBox::Show(const std::wstring& text, const std::wstring& caption, UINT type) {
    return (DialogResult)MessageBoxW(GetTopMostWindowInCurrentProcess(), text.c_str(), caption.c_str(), type);
}
