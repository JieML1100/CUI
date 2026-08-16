#pragma once

/**
 * CUITest build-time compiled XAML host.
 *
 * DemoWindow.cui.xaml owns the visual tree, layout, persistent properties,
 * styles, and event names. This class owns only runtime services, data, and
 * the C++ implementations for the statically generated event hooks.
 */
#include <DemoWindow.g.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class Button;
class ChartView;
class ComboBox;
class ContextMenu;
class Label;
class MediaElement;
class Menu;
class NotifyIcon;
class ObservableBindingList;
class ObservableObject;
class Image;
class IBindingList;
class ProgressBar;
class ProgressRing;
class RadioButton;
class Slider;
class StatusBar;
class Switch;
class TabControl;
class Taskbar;
class ToolBar;
class TreeView;
class WebBrowser;

class DemoWindow final : public DemoWindowGenerated
{
public:
	enum class InitializationMode
	{
		DeclarativeOnly,
		RuntimeData,
		Full
	};

	explicit DemoWindow(InitializationMode mode = InitializationMode::Full);
	~DemoWindow();

	bool VerifyDeclarativeFeatures(std::wstring* outError = nullptr);
	bool VerifyTextCompositionFeatures(std::wstring* outError = nullptr);
	bool VerifyRuntimeDataFeatures(std::wstring* outError = nullptr);
	bool VerifyPresentationFeatures(std::wstring* outError = nullptr);

private:
	template<typename T>
	T* RequireControl(const wchar_t* name);
	Control* FindGeneratedControlByName(std::wstring_view name) const noexcept;

	void PrepareRuntimeData();
	void AttachStaticBehaviors();
	void RegisterClassCommandBindings();
	void ResolveControls();
	void InitializeBasicPage();
	void InitializeContainerPage();
	void InitializeDataPage();
	void InitializeDataGridPage();
	void InitializeAnalyticsPage();
	void InitializeSystemPage();
	bool EnsureNotifyIconInitialized(bool show);
	void InitializeWebPage();
	void InitializeMediaPage();

	void UpdateStatus(const std::wstring& text);
	std::wstring GetStatusBarItemText(size_t index) const;
	bool SetStatusBarItemText(size_t index, const std::wstring& text);
	void UpdateProgress(float value01);
	void LoadImage(const std::wstring& path);
	std::wstring DescribeCommandTarget(const Control* target) const;
	void RecordCommandTarget(
		bool executed, const RoutedEventArgs& args, const RoutedCommand& command,
		std::uint64_t transactionId);
	void AppendCommandRouteTrace(std::wstring entry);

	void HandleClassCommandCanExecute(
		Control* sender, CanExecuteRoutedEventArgs& args);
	void HandleClassCommandExecuted(
		Control* sender, ExecutedRoutedEventArgs& args);
	void HandleNativeClassCommandCanExecute(
		Control* sender, CanExecuteRoutedEventArgs& args);
	void HandleNativeClassCommandExecuted(
		Control* sender, ExecutedRoutedEventArgs& args);
	void RefreshRoutedInputSummary();
	void RefreshTextCompositionSummary();
	bool RunElementHierarchyProbe(std::wstring* outSummary = nullptr);
	// WPF's handledEventsToo option belongs to AddHandler/Subscribe, not to
	// an event attribute in XAML, so this one handler is attached explicitly.
	void HandleRouteOuterBubble(Control* sender, MouseEventArgs& e);

	void HandleAnalyticsAction(Control* sender, RoutedEventArgs& e) override;
	void HandleBasicClick(Control* sender, RoutedEventArgs& e) override;
	void HandleChartKind(Control* sender, RoutedEventArgs& e) override;
	void HandleChartPoint(ChartView* sender, int seriesIndex, int pointIndex) override;
	void HandleDataGridAddingNewItem(
		DataGrid* sender, DataGridAddingNewItemEventArgs& e) override;
	void HandleDataGridCellEditEnding(
		DataGrid* sender, DataGridCellEditEndingEventArgs& e) override;
	void HandleDataGridInitializingNewItem(
		DataGrid* sender, DataGridInitializingNewItemEventArgs& e) override;
	void HandleDataGridLoadingRow(
		DataGrid* sender, DataGridRowEventArgs& e) override;
	void HandleDataGridLoadingRowDetails(
		DataGrid* sender, DataGridRowDetailsEventArgs& e) override;
	void HandleDataGridRowDetailsVisibilityChanged(
		DataGrid* sender, DataGridRowDetailsEventArgs& e) override;
	void HandleDataGridUnloadingRow(
		DataGrid* sender, DataGridRowEventArgs& e) override;
	void HandleDataGridUnloadingRowDetails(
		DataGrid* sender, DataGridRowDetailsEventArgs& e) override;
	void HandleDataGridRowEditEnding(
		DataGrid* sender, DataGridRowEditEndingEventArgs& e) override;
	void HandleDataGridCurrentCellChanged(
		DataGrid* sender, DataGridCurrentCellChangedEventArgs& e) override;
	void HandleDataGridSelectedCellsChanged(
		DataGrid* sender, DataGridSelectedCellsChangedEventArgs& e) override;
	void HandleDataGridSorting(
		DataGrid* sender, DataGridSortingEventArgs& e) override;
	void HandleDataGridScale(Control* sender, RoutedEventArgs& e) override;
	void HandleClosing(Window* sender, CancelEventArgs& e) override;
	void HandleComboSelection(Control* sender, SelectionChangedEventArgs& e) override;
	void HandleCommandAvailabilityToggle(Control* sender, RoutedEventArgs& e) override;
	void HandleCommandCanExecute(Control* sender, CanExecuteRoutedEventArgs& e) override;
	void HandleCommandExecuted(Control* sender, ExecutedRoutedEventArgs& e) override;
	void HandleCommandPreviewCanExecute(Control* sender, CanExecuteRoutedEventArgs& e) override;
	void HandleCommandPreviewExecuted(Control* sender, ExecutedRoutedEventArgs& e) override;
	void HandleCompositionCommit(Control* sender, TextCompositionEventArgs& e) override;
	void HandleCompositionPreviewCommit(Control* sender, TextCompositionEventArgs& e) override;
	void HandleCompositionPreviewStart(Control* sender, TextCompositionEventArgs& e) override;
	void HandleCompositionPreviewUpdate(Control* sender, TextCompositionEventArgs& e) override;
	void HandleCompositionStart(Control* sender, TextCompositionEventArgs& e) override;
	void HandleCompositionUpdate(Control* sender, TextCompositionEventArgs& e) override;
	void HandleContentRendered(Window* sender) override;
	void HandleDispatcherProbe(Control* sender, RoutedEventArgs& e) override;
	void HandleDocsLink(Control* sender, RoutedEventArgs& e) override;
	void HandleDragRoute(Control* sender, DragEventArgs& e) override;
	void HandleDropImage(Control* sender, DragEventArgs& e) override;
	void HandleEnableInput(Control* sender, RoutedEventArgs& e) override;
	void HandleExpander(Control* sender, RoutedEventArgs& e) override;
	void HandleFarButton(Control* sender, RoutedEventArgs& e) override;
	void HandleFeatureBubble(Control* sender, DeclarativeEventArgs& e) override;
	void HandleFeatureInvoked(Control* sender, DeclarativeEventArgs& e) override;
	void HandleGlobalProgress(Control* sender, RoutedPropertyChangedEventArgs<double>& e) override;
	void HandleImageVisibility(Control* sender, RoutedEventArgs& e) override;
	void HandleInvokeWeb(Control* sender, RoutedEventArgs& e) override;
	void HandleNavigationWeb(Control* sender, RoutedEventArgs& e) override;
	void HandleListBoxSelection(Control* sender, SelectionChangedEventArgs& e) override;
	void HandleListViewSelection(Control* sender, SelectionChangedEventArgs& e) override;
	void HandleLocalCommandCanExecute(Control* sender, CanExecuteRoutedEventArgs& e) override;
	void HandleLocalCommandExecuted(Control* sender, ExecutedRoutedEventArgs& e) override;
	void HandleMediaCommand(Control* sender, RoutedEventArgs& e) override;
	void HandleMediaEnded(Control* sender) override;
	void HandleMediaFailed(Control* sender) override;
	void HandleMediaLoop(Control* sender, RoutedEventArgs& e) override;
	void HandleMediaOpened(Control* sender) override;
	void HandleMediaPosition(Control* sender, double position) override;
	void HandleMediaSeek(Control* sender, RoutedPropertyChangedEventArgs<double>& e) override;
	void HandleMediaSpeed(Control* sender, RoutedPropertyChangedEventArgs<double>& e) override;
	void HandleMediaVolume(Control* sender, RoutedPropertyChangedEventArgs<double>& e) override;
	void HandleMouseWheel(Control* sender, MouseEventArgs& e) override;
	void HandleNumericValue(Control* sender, RoutedPropertyChangedEventArgs<double>& e) override;
	void HandleOpenImage(Control* sender, RoutedEventArgs& e) override;
	void HandlePresentationComposition(Control* sender, RoutedEventArgs& e) override;
	void HandlePresentationDeviceLoss(Control* sender, RoutedEventArgs& e) override;
	void HandlePresentationFullFrame(Control* sender, RoutedEventArgs& e) override;
	void HandlePresentationGeometry(Control* sender, RoutedEventArgs& e) override;
	void HandlePresentationRegion(Control* sender, RoutedEventArgs& e) override;
	void HandlePresentationTopology(Control* sender, RoutedEventArgs& e) override;
	void HandleRadio(Control* sender, RoutedEventArgs& e) override;
	void HandleRouteCaptureChanged(Control* sender, RoutedEventArgs& e) override;
	void HandleRouteFocus(Control* sender, RoutedEventArgs& e) override;
	void HandleRouteGotKeyboardFocus(Control* sender, KeyboardFocusChangedEventArgs& e) override;
	void HandleRouteKey(Control* sender, KeyEventArgs& e) override;
	void HandleRouteLostKeyboardFocus(Control* sender, KeyboardFocusChangedEventArgs& e) override;
	void HandleRouteMiddleBubble(Control* sender, MouseEventArgs& e) override;
	void HandleRouteMiddlePreview(Control* sender, MouseEventArgs& e) override;
	void HandleRouteOuterPreview(Control* sender, MouseEventArgs& e) override;
	void HandleRoutePreviewGotKeyboardFocus(Control* sender, KeyboardFocusChangedEventArgs& e) override;
	void HandleRoutePreviewLostKeyboardFocus(Control* sender, KeyboardFocusChangedEventArgs& e) override;
	void HandleRouteSourceBubble(Control* sender, MouseEventArgs& e) override;
	void HandleRouteSourcePreview(Control* sender, MouseEventArgs& e) override;
	void HandleSystemAction(Control* sender, RoutedEventArgs& e) override;
	void HandleSystemSurfaceMouseUp(Control* sender, MouseEventArgs& e) override;
	void HandleTemplateSwap(Control* sender, RoutedEventArgs& e) override;
	void HandleTextCompositionProbe(Control* sender, RoutedEventArgs& e) override;
	void HandleTextOuterBubble(Control* sender, TextCompositionEventArgs& e) override;
	void HandleTextOuterPreview(Control* sender, TextCompositionEventArgs& e) override;
	void HandleTextSourceBubble(Control* sender, TextCompositionEventArgs& e) override;
	void HandleTextSourcePreview(Control* sender, TextCompositionEventArgs& e) override;
	void HandleToolBarAction(Control* sender, RoutedEventArgs& e) override;
	void HandleTreeSelection(Control* sender, RoutedPropertyChangedEventArgs<BindingValue>& e) override;

	std::shared_ptr<ObservableObject> _dataContext;
	bool _runtimeDataInitialized = false;
	std::shared_ptr<ObservableBindingList> _treeRoots;
	std::shared_ptr<ObservableBindingList> _listViewEntries;
	std::shared_ptr<ObservableBindingList> _wpfLabPeople;
	std::shared_ptr<IBindingList> _dataGridDefaultItems;
	bool _dataGridMillionMode = false;
	std::shared_ptr<ObservableObject> _wpfLabSettings;
	int _featureInvocations = 0;
	int _featureBubbleInvocations = 0;
	int _featureInputCount = 0;
	int _dataGridSortEvents = 0;
	int _dataGridAddingNewItemEvents = 0;
	int _dataGridInitializingNewItemEvents = 0;
	int _dataGridEditEndingEvents = 0;
	int _dataGridRowEditEndingEvents = 0;
	int _dataGridCurrentCellEvents = 0;
	int _dataGridLoadingRowEvents = 0;
	int _dataGridUnloadingRowEvents = 0;
	int _dataGridLoadingRowDetailsEvents = 0;
	int _dataGridUnloadingRowDetailsEvents = 0;
	int _dataGridRowDetailsVisibilityEvents = 0;
	int _treeSelectionChanges = 0;
	std::vector<std::wstring> _routedInputTrace;
	std::wstring _routedInputDetail;
	std::vector<std::wstring> _textCompositionTrace;
	bool _compositionPreviewHandled = false;
	bool _cancelNextKeyboardFocus = false;
	ControlWeakReference _lastKeyboardFocusOld;
	ControlWeakReference _lastKeyboardFocusNew;
	std::vector<std::wstring> _commandRouteTrace;
	std::vector<std::wstring> _classCommandTrace;
	std::vector<EventConnection> _classCommandBindingConnections;
	bool _classCommandEnabled = true;
	bool _copyInfoCommandEnabled = true;
	int _classCommandCanExecuteCount = 0;
	int _classCommandExecutedCount = 0;
	int _nativeClassCommandCanExecuteCount = 0;
	int _nativeClassCommandExecutedCount = 0;
	int _commandPreviewCanExecuteCount = 0;
	int _commandCanExecuteCount = 0;
	int _commandPreviewExecutedCount = 0;
	int _commandExecutedCount = 0;
	int _localCommandCanExecuteCount = 0;
	int _localCommandExecutedCount = 0;
	std::wstring _lastCommandParameter;
	std::wstring _lastCommandCanExecuteTarget;
	std::wstring _lastCommandExecutedTarget;
	std::wstring _lastCommandTargetCommand;
	std::wstring _pendingCommandTargetCommand;
	std::wstring _displayedCommandCanExecuteTarget;
	std::uint64_t _pendingCommandTransactionId = 0;
	std::uint64_t _lastCommandExecutedTransactionId = 0;

	Menu* _menu = nullptr;
	ToolBar* _toolBar = nullptr;
	StatusBar* _statusBar = nullptr;
	Slider* _globalProgress = nullptr;
	Label* _statusText = nullptr;
	TabControl* _tabs = nullptr;
	Button* _basicButton = nullptr;
	Button* _dialogCancelButton = nullptr;
	RadioButton* _radioA = nullptr;
	RadioButton* _radioB = nullptr;
	Image* _image = nullptr;
	ProgressBar* _progress = nullptr;
	ProgressRing* _progressRing = nullptr;
	ChartView* _chart = nullptr;
	Label* _toastMessage = nullptr;
	WebBrowser* _web = nullptr;
	MediaElement* _media = nullptr;
	Slider* _mediaProgress = nullptr;
	Label* _mediaTime = nullptr;
	Label* _mediaSpeedText = nullptr;

	std::unique_ptr<Taskbar> _taskbar;
	std::unique_ptr<NotifyIcon> _notify;
	ContextMenu* _systemContextMenu = nullptr;
	bool _updatingMediaProgress = false;
};
