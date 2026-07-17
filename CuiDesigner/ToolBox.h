#pragma once

/**
 * @file ToolBox.h
 * @brief ToolBox：设计器工具箱（控件类型选择）。
 */
#include "../CUI/include/Panel.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/ScrollView.h"
#include "../CUI/include/TextBox.h"
#include "DesignerTypes.h"
#include <functional>
#include <utility>

class ToolBoxItem : public Button
{
public:
	DesignerControlDescriptor Descriptor;
	UIClass ControlType;
	std::wstring TypeName;
	std::wstring Category;
	int BaseY = 0;

	ToolBoxItem(
		DesignerControlDescriptor descriptor,
		int x,
		int y,
		int width = 120,
		int height = 30)
		: Button(descriptor.DisplayName, x, y, width, height),
		Descriptor(std::move(descriptor)),
		ControlType(Descriptor.Type),
		TypeName(Descriptor.Name),
		Category(Descriptor.Category),
		BaseY(y)
	{
		this->Round = 0.15f;
	}

	void Update() override;
};

class ToolBox : public Panel
{
private:
	std::vector<ToolBoxItem*> _items;
	class Label* _titleLabel = nullptr;
	class Label* _filterLabel = nullptr;
	TextBox* _filterBox = nullptr;
	class Label* _emptyLabel = nullptr;
	struct CategoryHeading
	{
		std::wstring Name;
		class Label* LabelPtr = nullptr;
		int BaseY = 0;
	};
	std::vector<CategoryHeading> _categoryHeadings;
	std::wstring _filterText;
	ScrollView* _scrollView = nullptr;
	Panel* _itemsHost = nullptr;
	int _contentTop = 76;
	int _contentBottomPadding = 10;
	int _contentHeight = 0;

	void UpdateScrollLayout();
	void ApplyFilterLayout();
	
public:
	ToolBox(
		int x, int y, int width, int height,
		std::vector<DesignerControlDescriptor> descriptors = {});
	virtual ~ToolBox();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
	void SetFilterText(const std::wstring& value);
	size_t GetItemCount() const noexcept { return _items.size(); }
	size_t GetVisibleItemCount() const noexcept;
	size_t GetVisibleCategoryCount() const noexcept;
	
	Event<void(const DesignerControlDescriptor&)> OnControlSelected;
};
