#pragma once

#include <windows.h>
#include <d2d1helper.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

class DesignerControl;

struct CodeGenInput
{
	std::vector<std::shared_ptr<DesignerControl>> Controls;
	std::shared_ptr<void> RuntimeOwner;
	std::wstring FormText;
	std::wstring FormName = L"MainForm";
	SIZE FormSize{ 800, 600 };
	POINT FormLocation{ 100, 100 };
	D2D1_COLOR_F FormBackColor = D2D1::ColorF(D2D1::ColorF::WhiteSmoke);
	D2D1_COLOR_F FormForeColor = D2D1::ColorF(D2D1::ColorF::Black);
	bool FormShowInTaskBar = true;
	bool FormTopMost = false;
	bool FormEnable = true;
	bool FormVisible = true;
	std::map<std::wstring, std::wstring> FormEventHandlers;
	bool FormVisibleHead = true;
	int FormHeadHeight = 24;
	bool FormMinBox = true;
	bool FormMaxBox = true;
	bool FormCloseBox = true;
	bool FormCenterTitle = true;
	bool FormAllowResize = true;
	std::wstring FormFontName;
	float FormFontSize = 18.0f;
};