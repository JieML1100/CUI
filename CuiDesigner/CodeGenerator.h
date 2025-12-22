#pragma once
#include "DesignerTypes.h"
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>

class CodeGenerator
{
private:
	std::wstring _className;
	std::vector<std::shared_ptr<DesignerControl>> _controls;
	
	std::string WStringToString(const std::wstring& wstr);
	std::wstring StringToWString(const std::string& str);
	std::string GetControlTypeName(UIClass type);
	std::string GetIncludeForType(UIClass type);
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
	CodeGenerator(std::wstring className, const std::vector<std::shared_ptr<DesignerControl>>& controls);
	
	bool GenerateFiles(std::wstring headerPath, std::wstring cppPath);
	std::string GenerateHeader();
	std::string GenerateCpp();
};
