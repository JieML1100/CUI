#pragma once

/**
 * @file CodeGenerator.h
 * @brief CodeGenerator：将 Designer 模型导出为 C++ 代码的生成器。
 */
#include "DesignerTypes.h"
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <map>

class CodeGenerator
{
private:
	std::wstring _className;
	std::vector<std::shared_ptr<DesignerControl>> _controls;
	std::wstring _formText;
	std::wstring _formName = L"MainForm";
	SIZE _formSize = { 800, 600 };
	POINT _formLocation = { 100, 100 };
	D2D1_COLOR_F _formBackColor = Colors::WhiteSmoke;
	D2D1_COLOR_F _formForeColor = Colors::Black;
	bool _formShowInTaskBar = true;
	bool _formTopMost = false;
	bool _formEnable = true;
	bool _formVisible = true;
	std::map<std::wstring, std::wstring> _formEventHandlers;
	bool _formVisibleHead = true;
	int _formHeadHeight = 24;
	bool _formMinBox = true;
	bool _formMaxBox = true;
	bool _formCloseBox = true;
	bool _formCenterTitle = true;
	bool _formAllowResize = true;
	std::wstring _formFontName;
	float _formFontSize = 18.0f;
	std::unordered_map<const DesignerControl*, std::string> _varNameOf;
	
	std::string WStringToString(const std::wstring& wstr) const;
	std::wstring StringToWString(const std::string& str) const;
	std::string GetControlTypeName(UIClass type);
	std::string GetIncludeForType(UIClass type);
	void BuildVarNameMap();
	std::string GetVarName(const std::shared_ptr<DesignerControl>& dc) const;
	static std::string SanitizeCppIdentifier(const std::string& raw);
	std::string EscapeWStringLiteral(const std::wstring& s);
	std::string FloatLiteral(float v);
	std::string ColorToString(D2D1_COLOR_F color);
	std::string ThicknessToString(const Thickness& t);
	std::string HorizontalAlignmentToString(::HorizontalAlignment a);
	std::string VerticalAlignmentToString(::VerticalAlignment a);
	std::string DockToString(::Dock d);
	std::string OrientationToString(::Orientation o);
	std::string SizeUnitToString(SizeUnit u);
	std::string GridLengthToCtorString(const GridLength& gl);

	std::string GenerateControlInstantiation(const std::shared_ptr<DesignerControl>& dc, int indent);
	std::string GenerateControlCommonProperties(const std::shared_ptr<DesignerControl>& dc, int indent);
	std::string GenerateContainerProperties(const std::shared_ptr<DesignerControl>& dc, int indent);
	
public:
	CodeGenerator(std::wstring className, const std::vector<std::shared_ptr<DesignerControl>>& controls,
		std::wstring formText = L"", SIZE formSize = SIZE{ 800, 600 }, POINT formLocation = POINT{ 100, 100 }, std::wstring formName = L"MainForm",
		D2D1_COLOR_F formBackColor = Colors::WhiteSmoke, D2D1_COLOR_F formForeColor = Colors::Black,
		bool formShowInTaskBar = true, bool formTopMost = false, bool formEnable = true, bool formVisible = true,
		const std::map<std::wstring, std::wstring>& formEventHandlers = std::map<std::wstring, std::wstring>{},
		bool formVisibleHead = true, int formHeadHeight = 24,
		bool formMinBox = true, bool formMaxBox = true, bool formCloseBox = true,
		bool formCenterTitle = true, bool formAllowResize = true,
		std::wstring formFontName = L"", float formFontSize = 18.0f);
	
	bool GenerateFiles(std::wstring headerPath, std::wstring cppPath);
	std::string GenerateHeader();
	std::string GenerateCpp();
};
