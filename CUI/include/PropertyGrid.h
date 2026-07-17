#pragma once
#include "Control.h"
#include "ObservableCollection.h"

enum class PropertyGridValueType
{
	Text,
	Number,
	Bool,
	Enum,
	Color,
	ReadOnly,
	/** Invokes OnItemClick without entering a scalar editor. */
	Action,
	/** Edits a bounded numeric value directly on a native track. */
	Slider,
	/** Edits the four combinable layout anchor edges in a visual popup. */
	Anchor,
	/** Free-form text with a drop-down containing compatible suggestions. */
	EditableEnum
};

class PropertyGridItem
{
public:
	mutable uint32_t CollectionId = 0;
	std::wstring Category;
	std::wstring Name;
	std::wstring Value;
	std::wstring Description;
	std::vector<std::wstring> Options;
	PropertyGridValueType ValueType = PropertyGridValueType::Text;
	UINT64 Tag = 0;
	bool ReadOnly = false;
	/** Draws an indeterminate value until the user establishes one value. */
	bool IsMixed = false;
	/** Exposes the native reset affordance and OnResetRequested event. */
	bool CanReset = false;
	double Minimum = 0.0;
	double Maximum = 1.0;
	double Step = 0.01;

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
	void EnsureBindingPropertiesRegistered() override;
	PropertyGridView(int x = 0, int y = 0, int width = 280, int height = 320);
	~PropertyGridView() override;

	using ItemCollection = ObservableCollection<PropertyGridItem>;
	ItemCollection Items;

#define CUI_PROPERTY_GRID_PROPERTY(type, name) \
	PROPERTY(type, name); \
	GET(type, name); \
	type Get##name() const; \
	SET(type, name)

	CUI_PROPERTY_GRID_PROPERTY(bool, ShowHeader);
	CUI_PROPERTY_GRID_PROPERTY(bool, ShowCategories);
	CUI_PROPERTY_GRID_PROPERTY(bool, AlternatingRows);
	CUI_PROPERTY_GRID_PROPERTY(bool, AllowEditing);
	CUI_PROPERTY_GRID_PROPERTY(float, Border);
	CUI_PROPERTY_GRID_PROPERTY(float, CornerRadius);
	CUI_PROPERTY_GRID_PROPERTY(float, HeaderHeight);
	CUI_PROPERTY_GRID_PROPERTY(float, CategoryHeight);
	CUI_PROPERTY_GRID_PROPERTY(float, RowHeight);
	CUI_PROPERTY_GRID_PROPERTY(float, NameColumnWidth);
	CUI_PROPERTY_GRID_PROPERTY(float, SplitterWidth);
	CUI_PROPERTY_GRID_PROPERTY(float, ScrollBarSize);
	CUI_PROPERTY_GRID_PROPERTY(float, CellPaddingX);
	CUI_PROPERTY_GRID_PROPERTY(int, MouseWheelStep);
	CUI_PROPERTY_GRID_PROPERTY(int, SelectedIndex);
	CUI_PROPERTY_GRID_PROPERTY(int, HoveredIndex);
	CUI_PROPERTY_GRID_PROPERTY(float, ScrollYOffset);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, HeaderBackColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, HeaderForeColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, CategoryBackColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, CategoryForeColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, GridLineColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, AlternateRowBackColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, SelectedItemBackColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, UnderMouseItemBackColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, ReadOnlyForeColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, AccentColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, EditBackColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, EditForeColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, EditSelectedBackColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, EditSelectedForeColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, CheckBackColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, CheckBorderColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, ScrollBackColor);
	CUI_PROPERTY_GRID_PROPERTY(D2D1_COLOR_F, ScrollForeColor);
	CUI_PROPERTY_GRID_PROPERTY(float, EditTextMargin);

#undef CUI_PROPERTY_GRID_PROPERTY

	PropertyGridItemEvent SelectionChanged;
	PropertyGridItemEvent OnItemClick;
	PropertyGridItemEvent OnResetRequested;
	PropertyGridItemEvent OnEditStarted;
	PropertyGridItemEvent OnEditCompleted;
	PropertyGridItemEvent OnEditCanceled;
	PropertyGridValueChangedEvent OnValueChanged;
	ScrollChangedEvent ScrollChanged;

	void Clear();
	/**
	 * Atomically replaces the structural item collection. Category collapse and
	 * scroll state are retained when the corresponding categories/extent remain.
	 */
	void SetItems(std::vector<PropertyGridItem> items);
	int AddItem(const PropertyGridItem& item);
	int AddProperty(const std::wstring& category, const std::wstring& name, const std::wstring& value,
		PropertyGridValueType type = PropertyGridValueType::Text);
	bool RemoveItemAt(int index);
	size_t ItemCount() const;
	PropertyGridItem* SelectedItem();
	const PropertyGridItem* SelectedItem() const;
	bool SelectItem(int index, bool ensureVisible = true);
	bool ClearSelection();
	/** Invokes the row's primary action. Useful for keyboard and automation. */
	bool ActivateItem(int index);
	/** Requests restoring an editable row to its inherited/default value. */
	bool RequestReset(int index);
	/** Customizes the two native header captions without replacing the grid. */
	void SetHeaderLabels(std::wstring nameCaption, std::wstring valueCaption);
	const std::wstring& GetNameHeaderLabel() const noexcept { return _nameHeaderLabel; }
	const std::wstring& GetValueHeaderLabel() const noexcept { return _valueHeaderLabel; }
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
	bool BeginEdit(int index);
	bool CommitEdit();
	bool CancelEdit();
	bool IsEditing() const noexcept { return _editing; }
	int GetEditingIndex() const noexcept { return _editingIndex; }
	const std::wstring& GetEditingText() const noexcept { return _editingText; }
	bool SetEditingText(const std::wstring& text);

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
	bool _showHeader = true;
	bool _showCategories = true;
	bool _alternatingRows = true;
	bool _allowEditing = true;
	float _border = 1.0f;
	float _cornerRadius = 6.0f;
	float _headerHeight = 28.0f;
	float _categoryHeight = 26.0f;
	float _rowHeight = 28.0f;
	float _nameColumnWidth = 130.0f;
	float _splitterWidth = 5.0f;
	float _scrollBarSize = 8.0f;
	float _cellPaddingX = 8.0f;
	int _mouseWheelStep = 48;
	int _selectedIndex = -1;
	int _hoveredIndex = -1;
	float _scrollYOffset = 0.0f;
	D2D1_COLOR_F _headerBackColor = cui::theme::palette::SurfaceMuted;
	D2D1_COLOR_F _headerForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F _categoryBackColor = cui::theme::palette::SurfaceSubtle;
	D2D1_COLOR_F _categoryForeColor = cui::theme::palette::TextSecondary;
	D2D1_COLOR_F _gridLineColor = cui::theme::palette::Border;
	D2D1_COLOR_F _alternateRowBackColor = cui::theme::palette::SurfaceSubtle;
	D2D1_COLOR_F _selectedItemBackColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F _underMouseItemBackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F _readOnlyForeColor = cui::theme::palette::TextMuted;
	D2D1_COLOR_F _accentColor = cui::theme::palette::Accent;
	D2D1_COLOR_F _editBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F _editForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F _editSelectedBackColor = cui::theme::palette::SelectionBack;
	D2D1_COLOR_F _editSelectedForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F _checkBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F _checkBorderColor = cui::theme::palette::BorderStrong;
	D2D1_COLOR_F _scrollBackColor = cui::theme::palette::ScrollTrack;
	D2D1_COLOR_F _scrollForeColor = cui::theme::palette::ScrollThumb;
	float _editTextMargin = 3.0f;

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
	bool _dragSlider = false;
	int _sliderDragIndex = -1;
	float _scrollThumbGrabOffsetY = 0.0f;
	bool _editing = false;
	int _editingIndex = -1;
	bool _dragEditSelection = false;
	std::wstring _editingText;
	std::wstring _editingOriginalText;
	int _editSelectionStart = 0;
	int _editSelectionEnd = 0;
	float _editOffsetX = 0.0f;
	class DropDownPopup* _dropDownPopup = nullptr;
	int _dropDownPopupIndex = -1;
	class ColorPickerPopup* _colorPicker = nullptr;
	int _colorPickerIndex = -1;
	class AnchorPickerPopup* _anchorPicker = nullptr;
	int _anchorPickerIndex = -1;
	std::vector<uint32_t> _knownItemIds;
	std::wstring _nameHeaderLabel = L"Property";
	std::wstring _valueHeaderLabel = L"Value";

	std::vector<RowInfo> BuildRows() const;
	Layout CalcLayout(const std::vector<RowInfo>& rows) const;
	D2D1_RECT_F GetRowRect(const RowInfo& row, const Layout& layout) const;
	D2D1_RECT_F GetVisibleRowRect(const RowInfo& row, const Layout& layout) const;
	D2D1_RECT_F GetNameRect(const D2D1_RECT_F& rowRect) const;
	D2D1_RECT_F GetValueRect(const D2D1_RECT_F& rowRect) const;
	D2D1_RECT_F GetResetRect(const D2D1_RECT_F& rowRect) const;
	void ClampScroll(Layout& layout);
	void DrawHeader(D2DGraphics* d2d, const Layout& layout);
	void DrawRows(D2DGraphics* d2d, const std::vector<RowInfo>& rows, const Layout& layout);
	void DrawCategoryRow(D2DGraphics* d2d, const RowInfo& row, const D2D1_RECT_F& rect);
	void DrawItemRow(D2DGraphics* d2d, const RowInfo& row, const D2D1_RECT_F& rect, int visibleItemOrdinal);
	void DrawCheckBox(D2DGraphics* d2d, const D2D1_RECT_F& rect, bool checked, bool indeterminate = false);
	void DrawScrollBar(D2DGraphics* d2d, const Layout& layout);
	void UpdateHover(int localX, int localY);
	void UpdateScrollByThumb(float localY);
	void SetCurrentSelectedIndex(int value);
	void SetCurrentHoveredIndex(int value);
	void SetCurrentScrollYOffset(float value);
	void SetCurrentNameColumnWidth(float value);
	void InputEditText(std::wstring input);
	void BackspaceEdit();
	void DeleteEdit();
	void MoveEditCaret(int delta);
	bool IsEditingTextAllowed(const std::wstring& text) const;
	void EditEnsureSelectionInRange();
	void EditUpdateScroll(float cellWidth);
	int EditHitTestTextPosition(float cellWidth, float cellHeight, float x, float y);
	bool SetEditingCaretFromMousePoint(int localX, int localY, const D2D1_RECT_F& valueRect);
	bool UpdateEditingSelectionFromMousePoint(int localX, int localY, const D2D1_RECT_F& valueRect);
	std::wstring EditGetSelectedString() const;
	void EditSetImeCompositionWindow();
	void ToggleBool(int index);
	void UpdateSliderFromPoint(int index, float localX, const D2D1_RECT_F& valueRect);
	void CycleEnum(int index, int direction = 1);
	void ToggleDropDownEditor(int index, const D2D1_RECT_F& valueRect);
	void CloseDropDownEditor(bool immediate = false);
	bool IsDropDownEditorOpenFor(int index) const;
	void OpenColorPickerEditor(int index, const D2D1_RECT_F& valueRect);
	void CloseColorPickerEditor();
	void OpenAnchorPickerEditor(int index, const D2D1_RECT_F& valueRect);
	void CloseAnchorPickerEditor();
	bool IsEditableItem(int index) const;
	bool GetValueRectForItem(int index, const std::vector<RowInfo>& rows, const Layout& layout, D2D1_RECT_F& outRect) const;
	bool IsValueCell(int localX, int localY, const std::vector<RowInfo>& rows, const Layout& layout, int& itemIndex) const;
	bool IsOverSplitter(int localX, int localY) const;
	void OnItemsCollectionChanged(const CollectionChangedEventArgs& change);
	void EnsureItemIds();
	void StartCategoryAnimation(const std::wstring& category, bool collapsing);
	bool PruneCategoryAnimations();
	float CategoryContentProgress(const std::wstring& category, bool collapsed) const;
	float CategoryChevronProgress(const std::wstring& category, bool collapsed) const;
};
