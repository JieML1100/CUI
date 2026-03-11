#pragma once

#include "../DesignerTypes.h"
#include <CppUtils/Utils/json.h>
#include <map>
#include <string>
#include <vector>

namespace DesignerModel
{
using Json = JsonLib::json;

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

struct DesignNode
{
	int Id = 0;
	int ParentId = 0;
	std::wstring ParentRef;
	std::wstring Name;
	UIClass Type = UIClass::UI_Base;
	int Order = -1;
	Json Props = Json::object();
	Json Extra = Json::object();
	Json Events = Json::object();

	bool operator==(const DesignNode& other) const;
};

struct DesignDocument
{
	std::string Schema = "cui.designer";
	int SchemaVersion = 1;
	int NextStableId = 1;
	DesignFormModel Form;
	std::vector<DesignNode> Nodes;

	int AllocateNodeId();
	void RecalculateNextStableId();
	void Clear();
	bool operator==(const DesignDocument& other) const;
};
}