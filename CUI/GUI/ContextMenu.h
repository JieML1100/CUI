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

public:
	ContextMenu();
	~ContextMenu();

	float Border = 1.0f;
	int ItemHeight = 28;
	D2D1_COLOR_F PopupBackColor = D2D1_COLOR_F{ 0.12f, 0.12f, 0.12f, 0.94f };
	D2D1_COLOR_F PopupBorderColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.20f };
	D2D1_COLOR_F PopupHoverColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.16f };
	D2D1_COLOR_F PopupTextColor = Colors::WhiteSmoke;
	D2D1_COLOR_F PopupSeparatorColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.14f };

	MenuCommandEvent OnMenuCommand;

	virtual UIClass Type() override;
	bool HitTestChildren() const override { return false; }
	bool ContainsPoint(int xof, int yof) override;
	bool AutoCloseOnOutsideClick() const override { return true; }
	bool AutoCloseOnFormFocusLoss() const override { return true; }
	void ClosePopup() override { Hide(); }
	SIZE ActualSize() override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

	MenuItem* AddItem(std::wstring text, int id = 0);
	MenuItem* AddSeparator();
	MenuItem* FindItemById(int id) const;
	MenuItem* FindItemByText(const std::wstring& text) const;
	bool RemoveItem(MenuItem* item);
	bool RemoveItemById(int id);
	void ClearItems();
	void ShowAt(int x, int y, bool ignoreNextMouseUp = false);
	void ShowAt(class Control* relativeTo, int x, int y, bool ignoreNextMouseUp = false);
	void Hide();
	bool IsOpen() const { return _popupVisible; }
};
