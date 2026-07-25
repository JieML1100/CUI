#pragma once
#include "Menu.h"

class ContextMenu;
using ContextMenuEvent = Event<void(ContextMenu*)>;

/**
 * WPF-style context menu. Placement is initiated by behavior through ShowAt;
 * IsOpen/PlacementTarget are semantic state, while popup chrome is private to
 * the current fallback presenter.
 */
class ContextMenu : public ItemsControl
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Menu, L"ContextMenu");
	}

private:
	bool _isOpen = false;
	bool _isPresented = false;
	bool _staysOpen = false;
	bool _ignoreNextMouseUp = false;
	ControlWeakReference _placementTarget;
	cui::core::Point _anchor{};
	std::vector<int> _hoverPath;
	std::vector<int> _openPath;
	std::vector<Control*> _items;

	struct PopupPanel
	{
		std::span<Control* const> Items;
		float X = 0;
		float Y = 0;
		float W = 0;
		float H = 0;
		bool OpenedToLeft = false;
	};

	float CalcPanelWidth(std::span<Control* const> items);
	std::vector<PopupPanel> BuildPanels();
	void AttachItemTree(MenuItem* item);
	void SynchronizeItemCommandHosts();
	void OnPresentationWindowChanged(
		Window* previousWindow, Window* currentWindow) override;
	void OnItemInteractionStateChanged(MenuItem& source);
	void ClearHoverState();
	void SynchronizeInteractionProjection();
	void ApplyIsOpenChange(bool oldValue, bool newValue);
	void PresentCore();
	void DismissPresentationCore();
	void ShowAtCore(
		Control* placementTarget,
		int x, int y,
		bool ignoreNextMouseUp);
	bool ValidateAuthoredItemControl(
		const Control& item, std::string& error) const override;
	void OnAuthoredItemsChanged() noexcept override;
	void OnBeforeGeneratedItemsRebuilt() override;
	void OnGeneratedItemsRebuilt() override;
	std::unique_ptr<Control> WrapGeneratedItem(
		std::unique_ptr<Control> visual,
		const BindingSourceReference& item,
		size_t index) override;
	void SynchronizeItems();
	void SuppressItemsPresentation();
	void OnControlTemplatePresentationChanged() override;

public:
	ContextMenu();
	~ContextMenu();

	virtual UIClass Type() override;
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
	bool GetIsOpen() const noexcept { return _isOpen; }
	void SetIsOpen(bool value);
	__declspec(property(get = GetIsOpen, put = SetIsOpen)) bool IsOpen;
	bool GetStaysOpen() const noexcept { return _staysOpen; }
	void SetStaysOpen(bool value);
	__declspec(property(get = GetStaysOpen, put = SetStaysOpen)) bool StaysOpen;
	READONLY_PROPERTY(class Control*, PlacementTarget);
	GET(class Control*, PlacementTarget);
	ContextMenuEvent Opened;
	ContextMenuEvent Closed;
	bool HitTestChildren() const override { return false; }
	bool ContainsPoint(int localX, int localY) override;
	cui::core::Size GetRenderSizeDip() override;
protected:
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
public:
	MenuItem* AddItem(std::unique_ptr<MenuItem> item);
	MenuItem* InsertItem(
		int index, std::unique_ptr<MenuItem> item);
	Separator* AddSeparator();
	MenuItem* GetItem(int index) const noexcept;
	int IndexOfItem(const MenuItem* item) const noexcept;
	MenuItem* FindItemByCommand(
		const std::wstring& command, bool recursive = true) const noexcept;
	MenuItem* FindItemByText(
		const std::wstring& text, bool recursive = true) const noexcept;
	std::unique_ptr<Control> DetachItemAt(int index);
	std::unique_ptr<MenuItem> DetachItem(MenuItem* item);
	bool RemoveItemAt(int index);
	bool RemoveItem(MenuItem* item);
	bool RemoveItemByCommand(
		const std::wstring& command, bool recursive = true);
	void ClearItems();
	void ShowAt(int x, int y, bool ignoreNextMouseUp = false);
	void ShowAt(class Control* relativeTo, int x, int y, bool ignoreNextMouseUp = false);
	void Hide();
};
