#pragma once

#include "../CUI/include/ComboBox.h"
#include "../CUI/include/Canvas.h"
#include "../CUI/include/ContextMenu.h"
#include "../CUI/include/Menu.h"
#include "../CUI/include/TabControl.h"
#include "../CUI/include/DependencyPropertyInfrastructure.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cui::designer
{
	inline void ApplyProgrammaticBounds(
		Control& control, float x, float y)
	{
		Canvas::SetLeft(control, x);
		Canvas::SetTop(control, y);
	}

	inline void ApplyProgrammaticBounds(
		Control& control, float x, float y, float width, float height)
	{
		ApplyProgrammaticBounds(control, x, y);
		control.Width = width;
		control.Height = height;
	}

	inline void ApplyProgrammaticDisplayValue(
		Control& control, const std::wstring& value)
	{
		const BindingValue authored(value);
		if (control.TrySetPropertyValue(L"Text", authored)
			|| control.TrySetPropertyValue(L"Content", authored)
			|| control.TrySetPropertyValue(L"Header", authored))
			return;
		throw std::logic_error(
			"programmatic designer control has no display content property");
	}

	inline void ApplyProgrammaticTypography(
		Control& control, std::wstring family, double size)
	{
		if (!cui::framework::DependencyPropertyAccess::SetValue(
			control, L"FontFamily", BindingValue(std::move(family)),
			DependencyPropertyValueSource::Theme)
			|| !cui::framework::DependencyPropertyAccess::SetValue(
				control, L"FontSize", BindingValue(size),
				DependencyPropertyValueSource::Theme))
			throw std::logic_error(
				"programmatic designer control rejected typography");
	}

	template<typename TControl>
	TControl* NewControl()
	{
		return new TControl();
	}

	template<typename TControl>
	TControl* NewControl(float x, float y)
	{
		auto* control = new TControl();
		ApplyProgrammaticBounds(*control, x, y);
		return control;
	}

	template<typename TControl>
	TControl* NewControl(float x, float y, float width, float height)
	{
		auto* control = new TControl();
		ApplyProgrammaticBounds(*control, x, y, width, height);
		return control;
	}

	template<typename TControl>
	TControl* NewControl(std::wstring value, float x, float y)
	{
		auto* control = NewControl<TControl>(x, y);
		ApplyProgrammaticDisplayValue(*control, value);
		return control;
	}

	template<typename TControl>
	TControl* NewControl(
		std::wstring value,
		float x, float y, float width, float height)
	{
		auto* control = NewControl<TControl>(x, y, width, height);
		ApplyProgrammaticDisplayValue(*control, value);
		return control;
	}

	template<typename TControl, typename... TArgs>
	std::unique_ptr<TControl> MakeControl(TArgs&&... args)
	{
		return std::unique_ptr<TControl>(
			NewControl<TControl>(std::forward<TArgs>(args)...));
	}

	inline std::unique_ptr<ComboBoxItem> MakeComboBoxItem(std::wstring text)
	{
		auto item = std::make_unique<ComboBoxItem>();
		item->SetContent(BindingValue(std::move(text)));
		return item;
	}

	inline ComboBoxItem* AddComboBoxItem(
		ComboBox& owner, std::wstring text)
	{
		return owner.AddItem(MakeComboBoxItem(std::move(text)));
	}

	inline std::wstring ComboBoxItemText(
		const ComboBox& owner, size_t index)
	{
		auto* item = owner.GetItem(static_cast<int>(index));
		return item ? item->GetContent().ToString() : std::wstring{};
	}

	inline std::vector<std::wstring> ComboBoxItems(const ComboBox& owner)
	{
		std::vector<std::wstring> result;
		result.reserve(owner.ItemCount());
		for (size_t index = 0; index < owner.ItemCount(); ++index)
			result.push_back(ComboBoxItemText(owner, index));
		return result;
	}

	inline int FindComboBoxItem(
		const ComboBox& owner, const std::wstring& text)
	{
		for (size_t index = 0; index < owner.ItemCount(); ++index)
			if (ComboBoxItemText(owner, index) == text)
				return static_cast<int>(index);
		return -1;
	}

	inline void SetComboBoxItems(
		ComboBox& owner, std::vector<std::wstring> values)
	{
		if (owner.GetItemsSource())
			throw std::logic_error(
				"designer ComboBox Items are unavailable while ItemsSource is set");
		owner.ClearItems();
		for (auto& value : values)
			(void)AddComboBoxItem(owner, std::move(value));
	}

	inline std::unique_ptr<MenuItem> MakeMenuItem(
		std::wstring header, std::wstring command = {})
	{
		auto item = std::make_unique<MenuItem>();
		item->SetHeader(BindingValue(std::move(header)));
		item->Command = std::move(command);
		return item;
	}

	inline MenuItem* AddMenuItem(
		Menu& owner, std::wstring header, std::wstring command = {})
	{
		return owner.AddItem(MakeMenuItem(
			std::move(header), std::move(command)));
	}

	inline MenuItem* AddMenuItem(
		ContextMenu& owner, std::wstring header, std::wstring command = {})
	{
		return owner.AddItem(MakeMenuItem(
			std::move(header), std::move(command)));
	}

	inline MenuItem* AddMenuItem(
		MenuItem& owner, std::wstring header, std::wstring command = {})
	{
		return owner.AddSubItem(MakeMenuItem(
			std::move(header), std::move(command)));
	}

	inline TabItem* AddTabItem(TabControl& owner, std::wstring header)
	{
		auto item = std::make_unique<TabItem>();
		item->SetHeader(BindingValue(std::move(header)));
		return owner.AddItem(std::move(item));
	}
}
