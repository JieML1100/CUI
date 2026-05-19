#pragma once
#include "Control.h"

enum class PropertyGridValueType
{
	Text,
	Number,
	Bool,
	Enum,
	Color,
	ReadOnly
};

class PropertyGridItem
{
public:
	std::wstring Category;
	std::wstring Name;
	std::wstring Value;
	std::wstring Description;
	std::vector<std::wstring> Options;
	PropertyGridValueType ValueType = PropertyGridValueType::Text;
	UINT64 Tag = 0;
	bool ReadOnly = false;

	PropertyGridItem() = default;
	PropertyGridItem(std::wstring category, std::wstring name, std::wstring value,
		PropertyGridValueType type = PropertyGridValueType::Text);
};

typedef Event<void(class PropertyGridView*, int index)> PropertyGridItemEvent;
typedef Event<void(class PropertyGridView*, int index, std::wstring oldValue, std::wstring newValue)> PropertyGridValueChangedEvent;

class PropertyGridView : public Control
{
public:
	UIClass Type() override;
	PropertyGridView(int x = 0, int y = 0, int width = 280, int height = 320);
	~PropertyGridView() override;

	std::vector<PropertyGridItem> Items;

	bool ShowHeader = true;
	bool ShowCategories = true;
	bool AlternatingRows = true;
	bool AllowEditing = true;
	float Border = 1.0f;
	float CornerRadius = 6.0f;
	float HeaderHeight = 28.0f;
	float CategoryHeight = 26.0f;
	float RowHeight = 28.0f;
	float NameColumnWidth = 130.0f;
	float SplitterWidth = 5.0f;
	float ScrollBarSize = 8.0f;
	float CellPaddingX = 8.0f;
	int MouseWheelStep = 48;

	int SelectedIndex = -1;
	int HoveredIndex = -1;
	float ScrollYOffset = 0.0f;

	D2D1_COLOR_F HeaderBackColor = D2D1_COLOR_F{ 0.18f, 0.22f, 0.28f, 0.95f };
	D2D1_COLOR_F HeaderForeColor = D2D1_COLOR_F{ 0.90f, 0.93f, 0.98f, 1.0f };
	D2D1_COLOR_F CategoryBackColor = D2D1_COLOR_F{ 0.20f, 0.23f, 0.29f, 0.82f };
	D2D1_COLOR_F CategoryForeColor = D2D1_COLOR_F{ 0.92f, 0.94f, 0.98f, 1.0f };
	D2D1_COLOR_F GridLineColor = D2D1_COLOR_F{ 0.55f, 0.60f, 0.68f, 0.28f };
	D2D1_COLOR_F AlternateRowBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.04f };
	D2D1_COLOR_F SelectedItemBackColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.28f };
	D2D1_COLOR_F UnderMouseItemBackColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.12f };
	D2D1_COLOR_F ReadOnlyForeColor = D2D1_COLOR_F{ 0.58f, 0.62f, 0.70f, 1.0f };
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.95f };
	D2D1_COLOR_F EditBackColor = Colors::White;
	D2D1_COLOR_F EditForeColor = Colors::Black;
	D2D1_COLOR_F EditSelectedBackColor = D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.30f };
	D2D1_COLOR_F EditSelectedForeColor = Colors::Black;
	D2D1_COLOR_F CheckBackColor = Colors::White;
	D2D1_COLOR_F CheckBorderColor = D2D1_COLOR_F{ 0.45f, 0.48f, 0.55f, 1.0f };
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	float EditTextMargin = 3.0f;

	PropertyGridItemEvent SelectionChanged;
	PropertyGridItemEvent OnItemClick;
	PropertyGridValueChangedEvent OnValueChanged;
	ScrollChangedEvent ScrollChanged;

	void Clear();
	int AddItem(const PropertyGridItem& item);
	int AddProperty(const std::wstring& category, const std::wstring& name, const std::wstring& value,
		PropertyGridValueType type = PropertyGridValueType::Text);
	bool RemoveItemAt(int index);
	size_t ItemCount() const;
	PropertyGridItem* SelectedItem();
	const PropertyGridItem* SelectedItem() const;
	bool SetValue(int index, const std::wstring& value);
	std::wstring GetValue(int index) const;
	void CollapseCategory(const std::wstring& category, bool collapsed);
	bool IsCategoryCollapsed(const std::wstring& category) const;
	void ToggleCategory(const std::wstring& category);
	void ExpandAll();
	void CollapseAll();
	void EnsureVisible(int index);
	void SetScrollOffset(float offsetY);
	int HitTestItem(int localX, int localY) const;

	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;

private:
	struct RowInfo
	{
		bool IsCategory = false;
		int ItemIndex = -1;
		std::wstring Category;
		float Top = 0.0f;
		float Height = 0.0f;
		bool HasClip = false;
		float ClipTop = 0.0f;
		float ClipBottom = 0.0f;
	};

	struct Layout
	{
		D2D1_RECT_F HeaderRect{ 0,0,0,0 };
		D2D1_RECT_F ContentRect{ 0,0,0,0 };
		D2D1_RECT_F ScrollTrackRect{ 0,0,0,0 };
		D2D1_RECT_F ScrollThumbRect{ 0,0,0,0 };
		float ContentHeight = 0.0f;
		float MaxScrollY = 0.0f;
		float ScrollBarSize = 8.0f;
		bool NeedVScroll = false;
	};

	struct CategoryAnimation
	{
		std::wstring Category;
		bool Collapsing = false;
		float StartProgress = 0.0f;
		float TargetProgress = 0.0f;
		UINT64 StartTick = 0;
		UINT DurationMs = 180;
	};

	std::vector<std::wstring> _collapsedCategories;
	std::vector<CategoryAnimation> _categoryAnimations;
	bool _dragVScroll = false;
	bool _dragSplitter = false;
	float _scrollThumbGrabOffsetY = 0.0f;
	bool _editing = false;
	int _editingIndex = -1;
	bool _dragEditSelection = false;
	std::wstring _editingText;
	std::wstring _editingOriginalText;
	int _editCaret = 0;
	int _editSelectionStart = 0;
	int _editSelectionEnd = 0;
	float _editOffsetX = 0.0f;
	std::wstring _imeCommittedTextToSuppress;
	UINT64 _imeCommitSuppressTick = 0;
	class DropDownPopup* _dropDownPopup = nullptr;
	int _dropDownPopupIndex = -1;
	class ColorPickerPopup* _colorPicker = nullptr;
	int _colorPickerIndex = -1;

	std::vector<RowInfo> BuildRows() const;
	Layout CalcLayout(const std::vector<RowInfo>& rows) const;
	D2D1_RECT_F GetRowRect(const RowInfo& row, const Layout& layout) const;
	D2D1_RECT_F GetVisibleRowRect(const RowInfo& row, const Layout& layout) const;
	D2D1_RECT_F GetNameRect(const D2D1_RECT_F& rowRect) const;
	D2D1_RECT_F GetValueRect(const D2D1_RECT_F& rowRect) const;
	void ClampScroll(Layout& layout);
	void DrawHeader(D2DGraphics* d2d, const Layout& layout);
	void DrawRows(D2DGraphics* d2d, const std::vector<RowInfo>& rows, const Layout& layout);
	void DrawCategoryRow(D2DGraphics* d2d, const RowInfo& row, const D2D1_RECT_F& rect);
	void DrawItemRow(D2DGraphics* d2d, const RowInfo& row, const D2D1_RECT_F& rect, int visibleItemOrdinal);
	void DrawCheckBox(D2DGraphics* d2d, const D2D1_RECT_F& rect, bool checked);
	void DrawScrollBar(D2DGraphics* d2d, const Layout& layout);
	void UpdateHover(int localX, int localY);
	void UpdateScrollByThumb(float localY);
	void SelectItem(int index);
	void BeginEdit(int index);
	void CommitEdit();
	void CancelEdit();
	void InsertEditChar(wchar_t ch);
	void BackspaceEdit();
	void DeleteEdit();
	void MoveEditCaret(int delta);
	void EditEnsureSelectionInRange();
	void EditUpdateScroll(float cellWidth);
	int EditHitTestTextPosition(float cellWidth, float cellHeight, float x, float y);
	bool SetEditingCaretFromMousePoint(int localX, int localY, const D2D1_RECT_F& valueRect);
	bool UpdateEditingSelectionFromMousePoint(int localX, int localY, const D2D1_RECT_F& valueRect);
	std::wstring EditGetSelectedString() const;
	void EditSetImeCompositionWindow();
	void HandleImeComposition(LPARAM lParam);
	void ToggleBool(int index);
	void CycleEnum(int index, int direction = 1);
	void ToggleDropDownEditor(int index, const D2D1_RECT_F& valueRect);
	void CloseDropDownEditor(bool immediate = false);
	bool IsDropDownEditorOpenFor(int index) const;
	void OpenColorPickerEditor(int index, const D2D1_RECT_F& valueRect);
	void CloseColorPickerEditor();
	bool IsEditableItem(int index) const;
	bool GetValueRectForItem(int index, const std::vector<RowInfo>& rows, const Layout& layout, D2D1_RECT_F& outRect) const;
	bool IsValueCell(int localX, int localY, const std::vector<RowInfo>& rows, const Layout& layout, int& itemIndex) const;
	bool IsOverSplitter(int localX, int localY) const;
	void StartCategoryAnimation(const std::wstring& category, bool collapsing);
	bool PruneCategoryAnimations();
	float CategoryContentProgress(const std::wstring& category, bool collapsed) const;
	float CategoryChevronProgress(const std::wstring& category, bool collapsed) const;
};
