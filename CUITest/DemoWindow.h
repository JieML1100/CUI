#pragma once

/**
 * CUITest dynamic-XAML host.
 *
 * DemoWindow.cui.xaml owns the visual tree, layout, persistent properties,
 * styles, and event names. This class owns only runtime services, data, and
 * the C++ implementations registered for those event names.
 */
#include <CuiRuntime.h>
#include <Window.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class BitmapSource;
class Button;
class ChartView;
class ComboBox;
class ContextMenu;
class Label;
class MediaPlayer;
class Menu;
class NotifyIcon;
class ObservableBindingList;
class ObservableObject;
class Image;
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

class DemoWindow final : public Window
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

	static std::wstring XamlFilePath();
	static bool ValidateXaml(std::wstring* outError = nullptr);
	bool VerifyDeclarativeFeatures(std::wstring* outError = nullptr);
	bool VerifyTextCompositionFeatures(std::wstring* outError = nullptr);
	bool VerifyRuntimeDataFeatures(std::wstring* outError = nullptr);
	bool VerifyPresentationFeatures(std::wstring* outError = nullptr);

private:
	template<typename T>
	T* RequireControl(const wchar_t* name);

	void PrepareDeclarativeRuntime(
		const DesignerModel::DesignDocument& document);
	void RegisterClassCommandBindings();
	void RegisterXamlHandlers(
		const DesignerModel::DesignDocument& document);
	void MountXaml(const DesignerModel::DesignDocument& document);
	void ResolveControls();
	void LoadImages();
	void InitializeBasicPage();
	void InitializeContainerPage();
	void InitializeDataPage();
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

	void HandleContentRendered(Window* sender);
	void HandleClosing(Window* sender, CancelEventArgs& args);
	void HandleCommandPreviewCanExecute(
		Control* sender, CanExecuteRoutedEventArgs& args);
	void HandleCommandCanExecute(
		Control* sender, CanExecuteRoutedEventArgs& args);
	void HandleCommandPreviewExecuted(
		Control* sender, ExecutedRoutedEventArgs& args);
	void HandleCommandExecuted(
		Control* sender, ExecutedRoutedEventArgs& args);
	void HandleLocalCommandCanExecute(
		Control* sender, CanExecuteRoutedEventArgs& args);
	void HandleLocalCommandExecuted(
		Control* sender, ExecutedRoutedEventArgs& args);
	void HandleClassCommandCanExecute(
		Control* sender, CanExecuteRoutedEventArgs& args);
	void HandleClassCommandExecuted(
		Control* sender, ExecutedRoutedEventArgs& args);
	void HandleNativeClassCommandCanExecute(
		Control* sender, CanExecuteRoutedEventArgs& args);
	void HandleNativeClassCommandExecuted(
		Control* sender, ExecutedRoutedEventArgs& args);
	void HandleCommandAvailabilityToggle(Control* sender, RoutedEventArgs& e);
	void HandleToolBarAction(Control* sender, RoutedEventArgs& e);
	void HandleGlobalProgress(Control* sender, double oldValue, double newValue);
	void HandleMouseWheel(Control* sender, MouseEventArgs& e);
	void HandleBasicClick(Control* sender, RoutedEventArgs& e);
	void HandleEnableInput(Control* sender);
	void HandleRadio(Control* sender);
	void HandleComboSelection(ComboBox* sender);
	void HandleNumericValue(Control* sender, double oldValue, double newValue);
	void HandleDocsLink(Control* sender, RoutedEventArgs& e);
	void HandleExpander(class Expander* sender, bool expanded);
	void HandleOpenImage(Control* sender, RoutedEventArgs& e);
	void HandleDragRoute(Control* sender, DragEventArgs& e);
	void HandleDropImage(Control* sender, DragEventArgs& e);
	void HandleImageVisibility(Control* sender);
	void HandleListViewSelection(class Selector* sender);
	void HandleListBoxSelection(class Selector* sender);
	void HandleTreeSelection(TreeView* sender);
	void HandleFeatureInvoked(
		Control* sender, DeclarativeEventArgs& args);
	void HandleFeatureBubble(
		Control* sender, DeclarativeEventArgs& args);
	void HandleDispatcherProbe(Control* sender, RoutedEventArgs& e);
	void HandleRouteOuterPreview(Control* sender, MouseEventArgs& e);
	void HandleRouteMiddlePreview(Control* sender, MouseEventArgs& e);
	void HandleRouteSourcePreview(Control* sender, MouseEventArgs& e);
	void HandleRouteSourceBubble(Control* sender, MouseEventArgs& e);
	void HandleRouteMiddleBubble(Control* sender, MouseEventArgs& e);
	void HandleRouteOuterBubble(Control* sender, MouseEventArgs& e);
	void HandleRouteKey(Control* sender, KeyEventArgs& e);
	void HandleRouteCaptureChanged(Control* sender, RoutedEventArgs& e);
	void HandleRouteFocus(Control* sender, RoutedEventArgs& e);
	void HandleRoutePreviewGotKeyboardFocus(
		Control* sender, KeyboardFocusChangedEventArgs& e);
	void HandleRouteGotKeyboardFocus(
		Control* sender, KeyboardFocusChangedEventArgs& e);
	void HandleRoutePreviewLostKeyboardFocus(
		Control* sender, KeyboardFocusChangedEventArgs& e);
	void HandleRouteLostKeyboardFocus(
		Control* sender, KeyboardFocusChangedEventArgs& e);
	void HandleTextOuterPreview(Control* sender, TextCompositionEventArgs& e);
	void HandleTextSourcePreview(Control* sender, TextCompositionEventArgs& e);
	void HandleTextSourceBubble(Control* sender, TextCompositionEventArgs& e);
	void HandleTextOuterBubble(Control* sender, TextCompositionEventArgs& e);
	void RefreshRoutedInputSummary();
	void HandleCompositionPreviewStart(
		Control* sender, TextCompositionEventArgs& e);
	void HandleCompositionStart(Control* sender, TextCompositionEventArgs& e);
	void HandleCompositionPreviewUpdate(
		Control* sender, TextCompositionEventArgs& e);
	void HandleCompositionUpdate(Control* sender, TextCompositionEventArgs& e);
	void HandleCompositionPreviewCommit(
		Control* sender, TextCompositionEventArgs& e);
	void HandleCompositionCommit(Control* sender, TextCompositionEventArgs& e);
	void HandleTextCompositionProbe(Control* sender, RoutedEventArgs& e);
	void RefreshTextCompositionSummary();
	void HandlePresentationRegion(Control* sender, RoutedEventArgs& e);
	void HandlePresentationGeometry(Control* sender, RoutedEventArgs& e);
	void HandlePresentationComposition(Control* sender, RoutedEventArgs& e);
	void HandlePresentationFullFrame(Control* sender, RoutedEventArgs& e);
	void HandlePresentationTopology(Control* sender, RoutedEventArgs& e);
	void HandlePresentationDeviceLoss(Control* sender, RoutedEventArgs& e);
	bool RunElementHierarchyProbe(std::wstring* outSummary = nullptr);
	void HandleAnalyticsAction(Control* sender, RoutedEventArgs& e);
	void HandleChartKind(Control* sender, RoutedEventArgs& e);
	void HandleChartPoint(ChartView* sender, int seriesIndex, int pointIndex);
	void HandleFarButton(Control* sender, RoutedEventArgs& e);
	void HandleSystemAction(Control* sender, RoutedEventArgs& e);
	void HandleSystemSurfaceMouseUp(Control* sender, MouseEventArgs& e);
	void HandleInvokeWeb(Control* sender, RoutedEventArgs& e);
	void HandleMediaCommand(Control* sender, RoutedEventArgs& e);
	void HandleMediaVolume(Control* sender, double oldValue, double newValue);
	void HandleMediaSpeed(Control* sender, double oldValue, double newValue);
	void HandleMediaLoop(Control* sender);
	void HandleMediaSeek(Control* sender, double oldValue, double newValue);
	void HandleMediaOpened(MediaPlayer* sender);
	void HandleMediaEnded(MediaPlayer* sender);
	void HandleMediaFailed(MediaPlayer* sender);
	void HandleMediaPosition(MediaPlayer* sender, double position);

	DesignerModel::RuntimeDocumentSession _xamlSession;
	std::shared_ptr<ObservableObject> _dataContext;
	bool _runtimeDataInitialized = false;
	std::shared_ptr<ObservableBindingList> _treeRoots;
	std::shared_ptr<ObservableBindingList> _listViewEntries;
	std::shared_ptr<ObservableBindingList> _wpfLabPeople;
	std::shared_ptr<ObservableObject> _wpfLabSettings;
	std::shared_ptr<DesignerModel::DeclarativeComponentBehaviorRegistry>
		_componentBehaviors;
	std::shared_ptr<DesignerModel::NativeSurfaceBehaviorRegistry>
		_nativeSurfaceBehaviors;
	std::wstring _schemaSummary;
	int _featureInvocations = 0;
	int _featureBubbleInvocations = 0;
	int _treeSelectionChanges = 0;
	std::vector<std::wstring> _routedInputTrace;
	std::wstring _routedInputDetail;
	std::vector<std::wstring> _textCompositionTrace;
	bool _compositionPreviewHandled = false;
	bool _cancelNextKeyboardFocus = false;
	Control* _lastKeyboardFocusOld = nullptr;
	Control* _lastKeyboardFocusNew = nullptr;
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

	std::shared_ptr<BitmapSource> _images[10]{};

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
	MediaPlayer* _media = nullptr;
	Slider* _mediaProgress = nullptr;
	Label* _mediaTime = nullptr;
	Label* _mediaSpeedText = nullptr;

	std::unique_ptr<Taskbar> _taskbar;
	std::unique_ptr<NotifyIcon> _notify;
	ContextMenu* _systemContextMenu = nullptr;
	bool _updatingMediaProgress = false;
};
