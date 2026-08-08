#pragma once
#include "Menu.h"

class ContextMenu;
using ContextMenuEvent = Event<void(ContextMenu*)>;
enum class PlacementMode : int;

/**
 * WPF-style context menu. Placement is initiated by behavior through ShowAt;
 * IsOpen/PlacementTarget are semantic state, while ItemsPresenter and chrome
 * are supplied by the framework theme ControlTemplate.
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
	ControlWeakReference _servicePlacementTarget;
	bool _servicePlacementTargetActive = false;
	float _horizontalOffset = 0.0f;
	float _verticalOffset = 0.0f;
	cui::core::Rect _placementRectangle{};
	PlacementMode _placement;
	bool _hasDropShadow = false;
	cui::core::Point _anchor{};
	std::vector<int> _hoverPath;
	std::vector<int> _openPath;
	std::vector<Control*> _items;
	std::vector<ControlWeakReference> _generatedItemsRebuildSnapshot;
	bool _generatedItemsRebuildPending = false;

	void AttachItemTree(MenuItem* item);
	void SynchronizeItemCommandHosts();
	void OnPresentationWindowChanged(
		Window* previousWindow, Window* currentWindow) override;
	void OnItemInteractionStateChanged(MenuItem& source);
	void ClearHoverState();
	void SynchronizeInteractionProjection();
	void ApplyIsOpenChange(bool oldValue, bool newValue);
	void ApplyPlacementTarget(const ControlWeakReference& value);
	bool ApplyServicePlacementTarget(Control* value);
	void ClearServicePlacementTarget();
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
	std::unique_ptr<Panel> CreateItemsHost() const override;
	void SynchronizeItems();
	void OnControlTemplatePresentationChanged() override;
	void ArrangePopupSurface();
	MenuItem* HitTopLevelItem(int rootX, int rootY) const noexcept;

public:
	ContextMenu();
	~ContextMenu();

	virtual UIClass Type() override;
	/** WPF dependency-property identities used by generated/native code. */
	static const DependencyProperty& IsOpenProperty();
	static const DependencyProperty& StaysOpenProperty();
	static const DependencyProperty& PlacementTargetProperty();
	static const DependencyProperty& HorizontalOffsetProperty();
	static const DependencyProperty& VerticalOffsetProperty();
	static const DependencyProperty& PlacementRectangleProperty();
	static const DependencyProperty& PlacementProperty();
	static const DependencyProperty& HasDropShadowProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
	bool GetIsOpen() const noexcept { return _isOpen; }
	void SetIsOpen(bool value);
	__declspec(property(get = GetIsOpen, put = SetIsOpen)) bool IsOpen;
	bool GetStaysOpen() const noexcept { return _staysOpen; }
	void SetStaysOpen(bool value);
	__declspec(property(get = GetStaysOpen, put = SetStaysOpen)) bool StaysOpen;
	PROPERTY(class Control*, PlacementTarget);
	GET(class Control*, PlacementTarget);
	SET(class Control*, PlacementTarget);
	PROPERTY(float, HorizontalOffset);
	GET(float, HorizontalOffset);
	SET(float, HorizontalOffset);
	PROPERTY(float, VerticalOffset);
	GET(float, VerticalOffset);
	SET(float, VerticalOffset);
	PROPERTY(cui::core::Rect, PlacementRectangle);
	GET(cui::core::Rect, PlacementRectangle);
	SET(cui::core::Rect, PlacementRectangle);
	PROPERTY(PlacementMode, Placement);
	GET(PlacementMode, Placement);
	SET(PlacementMode, Placement);
	PROPERTY(bool, HasDropShadow);
	GET(bool, HasDropShadow);
	SET(bool, HasDropShadow);
	ContextMenuEvent Opened;
	ContextMenuEvent Closed;
	void Arrange(cui::core::Rect finalRect) override;
	bool ContainsPoint(int localX, int localY) override;
	cui::core::Size GetRenderSizeDip() override;
	bool PresentationSuppressionAffectsLayout() const noexcept override
	{
		return false;
	}
	bool BreaksVisualPresentationInheritance() const noexcept override
	{
		return true;
	}
protected:
	void PreparePresentation() override;
	bool HandlesNavigationKey(Key key) const override;
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
