#pragma once
#include "Control.h"
#include <vector>

struct FilterBarItem
{
	std::wstring Text;
	std::wstring Value;
	bool Selected = false;
	bool Enabled = true;
	UINT64 Tag = 0;

	FilterBarItem() = default;
	FilterBarItem(std::wstring text, std::wstring value = L"", bool selected = false);
};

typedef Event<void(class FilterBar*, const std::wstring& query)> FilterBarQueryEvent;
typedef Event<void(class FilterBar*, int index, bool selected)> FilterBarItemEvent;
typedef Event<void(class FilterBar*)> FilterBarEvent;

class FilterBar : public Control
{
public:
	UIClass Type() override;
	FilterBar(int x = 0, int y = 0, int width = 640, int height = 48);

	std::wstring Placeholder = L"Search";
	std::wstring QueryText = L"";
	std::vector<FilterBarItem> Items;

	bool ShowSearchBox = true;
	bool ShowActions = true;
	bool ApplyOnFilterChange = false;

	float Border = 1.0f;
	float CornerRadius = 8.0f;
	float SearchBoxWidth = 220.0f;
	float ChipHeight = 28.0f;

	D2D1_COLOR_F SurfaceColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.055f };
	D2D1_COLOR_F InputBackColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.16f };
	D2D1_COLOR_F ChipBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.08f };
	D2D1_COLOR_F ChipSelectedBackColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.34f };
	D2D1_COLOR_F HoverColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.12f };
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.95f };
	D2D1_COLOR_F MutedTextColor = D2D1_COLOR_F{ 0.72f, 0.76f, 0.82f, 1.0f };
	D2D1_COLOR_F ButtonBackColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.88f };
	D2D1_COLOR_F ButtonTextColor = Colors::White;

	FilterBarQueryEvent OnQueryChanged;
	FilterBarItemEvent OnFilterChanged;
	FilterBarEvent OnApply;
	FilterBarEvent OnReset;

	int AddItem(const FilterBarItem& item);
	void ClearItems();
	void ClearSelection();
	std::vector<std::wstring> GetSelectedValues() const;
	void SetQueryText(const std::wstring& text);

	CursorKind QueryCursor(int xof, int yof) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

private:
	enum class HitKind
	{
		None,
		Search,
		Chip,
		Apply,
		Reset
	};

	struct HitRegion
	{
		HitKind Kind = HitKind::None;
		int Index = -1;
		D2D1_RECT_F Rect{ 0,0,0,0 };
	};

	std::vector<HitRegion> _regions;
	D2D1_RECT_F _searchRect{ 0,0,0,0 };
	bool _searchFocused = false;
	int _hoverIndex = -1;
	HitKind _hoverKind = HitKind::None;

	void BuildLayout(float width, float height);
	HitRegion HitTest(int xof, int yof);
	void NotifyQueryChanged(const std::wstring& oldText);
	void ResetFilters();
};
