#pragma once
#include "Menu.h"

/* Popup context menu shown explicitly by app code via ShowAt(). */
class ContextMenu : public Control
{
private:
	bool _popupVisible = false;
	bool _ignoreNextMouseUp = false;
	POINT _anchor = { 0, 0 };
	std::vector<int> _hoverPath;
	std::vector<int> _openPath;
	std::vector<MenuItem*> _items;
	float _popupProgress = 0.0f;
	float _popupStartProgress = 0.0f;
	float _popupTargetProgress = 0.0f;
	ULONGLONG _popupAnimStartTick = 0;
	bool _popupAnimating = false;

	float ItemPaddingX = 12.0f;
	float DropPaddingY = 6.0f;

	struct PopupPanel
	{
		const std::vector<MenuItem*>* Items = nullptr;
		float X = 0;
		float Y = 0;
		float W = 0;
		float H = 0;
		bool OpenedToLeft = false;
	};

	float CalcPanelWidth(const std::vector<MenuItem*>& items);
	std::vector<PopupPanel> BuildPanels();
	void ClearHoverState();
	float CurrentPopupProgress();
	void BeginPopupReveal(float startProgress = 0.08f);

public:
	ContextMenu();
	~ContextMenu();

	float Border = 1.0f;
	int ItemHeight = 28;
	D2D1_COLOR_F PopupBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F PopupBorderColor = cui::theme::palette::Border;
	D2D1_COLOR_F PopupHoverColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F PopupTextColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F PopupSeparatorColor = cui::theme::palette::Border;
	float PopupCornerRadius = 8.0f;
	float ItemCornerRadius = 6.0f;
	float ItemHorizontalInset = 6.0f;
	UINT PopupAnimationDurationMs = 95;

	MenuCommandEvent OnMenuCommand;

	virtual UIClass Type() override;
	bool HitTestChildren() const override { return false; }
	bool ContainsPoint(int localX, int localY) override;
	bool AutoCloseOnOutsideClick() const override { return true; }
	bool AutoCloseOnFormFocusLoss() const override { return true; }
	void ClosePopup() override { Hide(); }
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	SIZE ActualSize() override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;

	MenuItem* AddItem(std::wstring text, int id = 0);
	MenuItem* AddItem(std::unique_ptr<MenuItem> item);
	MenuItem* InsertItem(
		int index, std::unique_ptr<MenuItem> item);
	MenuItem* AddSeparator();
	int ItemCount() const noexcept
	{
		return static_cast<int>(_items.size());
	}
	MenuItem* GetItem(int index) const noexcept;
	int IndexOfItem(const MenuItem* item) const noexcept;
	MenuItem* FindItemById(int id, bool recursive = true) const noexcept;
	MenuItem* FindItemByText(
		const std::wstring& text, bool recursive = true) const noexcept;
	std::unique_ptr<MenuItem> DetachItemAt(int index);
	std::unique_ptr<MenuItem> DetachItem(MenuItem* item);
	bool RemoveItemAt(int index);
	bool RemoveItem(MenuItem* item);
	bool RemoveItemById(int id, bool recursive = true);
	void ClearItems();
	void ShowAt(int x, int y, bool ignoreNextMouseUp = false);
	void ShowAt(class Control* relativeTo, int x, int y, bool ignoreNextMouseUp = false);
	void Hide();
	bool IsOpen() const { return _popupVisible; }
};
