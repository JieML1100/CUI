#pragma once

#include "../DesignerTypes.h"
#include "../DesignerStyleSheet.h"
#include "DesignValue.h"
#include <map>
#include <string>
#include <vector>

namespace DesignerModel
{
struct DesignFormModel
{
	std::wstring Name = L"MainForm";
	std::wstring Text = L"Form";
	std::wstring FontName;
	float FontSize = 18.0f;
	SIZE Size{ 800, 600 };
	POINT Location{ 100, 100 };
	D2D1_COLOR_F BackColor = Colors::WhiteSmoke;
	D2D1_COLOR_F ForeColor = Colors::Black;
	bool ShowInTaskBar = true;
	bool TopMost = false;
	bool Enable = true;
	bool Visible = true;
	bool VisibleHead = true;
	int HeadHeight = 24;
	bool MinBox = true;
	bool MaxBox = true;
	bool CloseBox = true;
	bool CenterTitle = true;
	bool AllowResize = true;
	std::map<std::wstring, std::wstring> EventHandlers;

	bool operator==(const DesignFormModel& other) const;
};

/**
 * Portable association between a design document and its generated/user C++
 * pair. RelativeBasePath has no extension and is resolved from the design
 * document directory by the Designer shell; runtime loading does not depend on
 * it.
 */
struct DesignCodeBehindModel
{
	std::wstring ClassName;
	std::wstring RelativeBasePath;

	bool Empty() const noexcept
	{
		return ClassName.empty() && RelativeBasePath.empty();
	}
	bool Validate(std::wstring* outError = nullptr) const;
	/** Accepts C++ `::` or XAML-style `.` separators and emits canonical `::`. */
	static bool TryNormalizeClassName(
		const std::wstring& value,
		std::wstring& normalized,
		std::wstring* outError = nullptr);
	static bool TryNormalizeRelativeBasePath(
		const std::wstring& value,
		std::wstring& normalized,
		std::wstring* outError = nullptr);
	bool operator==(const DesignCodeBehindModel& other) const;
};

struct DesignNode
{
	int Id = 0;
	int ParentId = 0;
	std::wstring ParentRef;
	std::wstring Name;
	UIClass Type = UIClass::UI_Base;
	DesignerCustomControlType CustomType;
	std::vector<DesignerCustomEventDescriptor> CustomEvents;
	int Order = -1;
	// Pure design-time placement protection; it is never projected to runtime.
	bool Locked = false;
	DesignValue Props = DesignValue::object();
	DesignValue Extra = DesignValue::object();
	DesignValue Events = DesignValue::object();
	DesignValue Bindings = DesignValue::object();

	bool operator==(const DesignNode& other) const;
};

struct DesignDocument
{
	static constexpr int CurrentSchemaVersion = 5;
	std::string Schema = "cui.designer";
	int SchemaVersion = CurrentSchemaVersion;
	int NextStableId = 1;
	DesignFormModel Form;
	DesignCodeBehindModel CodeBehind;
	DesignerDataContextSchema DataContextSchema;
	DesignerStyleSheet StyleSheet;
	std::vector<DesignNode> Nodes;

	int AllocateNodeId();
	void RecalculateNextStableId();
	void Clear();
	bool operator==(const DesignDocument& other) const;
};
}
