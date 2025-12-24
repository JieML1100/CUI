#pragma once
#include "Control.h"
#include <vector>

typedef Event<void(class Control*, int)> MenuCommandEvent;

class MenuItem : public Control
{
public:
	virtual UIClass Type() override;
	int Id = 0;
	bool Separator = false;
	std::wstring Shortcut;
	std::vector<MenuItem*> SubItems;

	MenuItem(std::wstring text = L"", int id = 0);
	~MenuItem();

	MenuItem* AddSubItem(std::wstring text, int id = 0);
	MenuItem* AddSeparator();
	static MenuItem* CreateSeparator();

	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

	D2D1_COLOR_F HoverBackColor = D2D1_COLOR_F{ 1,1,1,0.20f };
};

class Menu : public Control
{
private:
	bool _expand = false;
	int _expandIndex = -1;
	int _hoverTopIndex = -1;
	std::vector<int> _hoverPath;
	std::vector<int> _openPath;

	float ItemPaddingX = 10.0f;
	float DropPaddingY = 6.0f;

	float DropLeftLocal();
	float DropTopLocal() { return (float)BarHeight; }
	float DropWidthLocal();
	float DropHeightLocal();
	int DropCount();
	bool HasSubMenu(int dropIndex);

public:
	virtual UIClass Type() override;

	MenuCommandEvent OnMenuCommand;

	int BarHeight = 28;
	int DropItemHeight = 26;
	float Boder = 1.0f;

	D2D1_COLOR_F BarBackColor = D2D1_COLOR_F{ 1,1,1,0.10f };
	D2D1_COLOR_F BarBorderColor = D2D1_COLOR_F{ 1,1,1,0.18f };
	D2D1_COLOR_F DropBackColor = D2D1_COLOR_F{ 0.12f,0.12f,0.12f,0.92f };
	D2D1_COLOR_F DropBorderColor = D2D1_COLOR_F{ 1,1,1,0.22f };
	D2D1_COLOR_F DropHoverColor = D2D1_COLOR_F{ 1,1,1,0.18f };
	D2D1_COLOR_F DropTextColor = Colors::WhiteSmoke;
	D2D1_COLOR_F DropSeparatorColor = D2D1_COLOR_F{ 1,1,1,0.16f };

	Menu(int x, int y, int width, int height = 28);

	MenuItem* AddItem(std::wstring text);

	SIZE ActualSize() override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};

