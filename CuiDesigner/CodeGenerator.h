#pragma once
#include "DesignerTypes.h"
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <unordered_map>

class CodeGenerator
{
private:
	std::wstring _className;
	std::vector<std::shared_ptr<DesignerControl>> _controls;
	std::wstring _formText;
	SIZE _formSize = { 800, 600 };
	POINT _formLocation = { 100, 100 };
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
		std::wstring formText = L"", SIZE formSize = SIZE{ 800, 600 }, POINT formLocation = POINT{ 100, 100 });
	
	bool GenerateFiles(std::wstring headerPath, std::wstring cppPath);
	std::string GenerateHeader();
	std::string GenerateCpp();
};
