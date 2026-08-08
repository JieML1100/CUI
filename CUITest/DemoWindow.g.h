#pragma once
#include "Binding.h"
#include "BindingList.h"
#include "Border.h"
#include "Button.h"
#include "Calendar.h"
#include "Canvas.h"
#include "ChartView.h"
#include "CheckBox.h"
#include "CollectionViewSource.h"
#include "ComboBox.h"
#include "ContentControl.h"
#include "ContextMenu.h"
#include "Control.h"
#include "DatePicker.h"
#include "Expander.h"
#include "GroupBox.h"
#include "GroupStyle.h"
#include "Image.h"
#include "ItemsControl.h"
#include "ItemsPanelTemplate.h"
#include "Label.h"
#include "Layout/DockPanel.h"
#include "Layout/Grid.h"
#include "Layout/LayoutTypes.h"
#include "Layout/RelativePanel.h"
#include "Layout/StackPanel.h"
#include "Layout/WrapPanel.h"
#include "ListBox.h"
#include "ListView.h"
#include "LoadingRing.h"
#include "MediaElement.h"
#include "Menu.h"
#include "NativeSurface.h"
#include "NumericUpDown.h"
#include "PasswordBox.h"
#include "ProgressBar.h"
#include "ProgressRing.h"
#include "RadioButton.h"
#include "RichTextBox.h"
#include "RoutedCommand.h"
#include "ScrollViewer.h"
#include "Separator.h"
#include "Slider.h"
#include "StatusBar.h"
#include "Switch.h"
#include "TabControl.h"
#include "TextBox.h"
#include "ToolBar.h"
#include "TreeView.h"
#include "WebBrowser.h"
#include "Window.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/** Build-time C++ projection of XAML ComponentDefinition FeatureCard. */
class DemoWindowGeneratedFeatureCard final : public Canvas
{
protected:
	ComponentTypeToken GetCompiledComponentTypeTokenCore() const noexcept override;
	const DependencyPropertyMetadata* FindCompiledComponentPropertyCore(
		ComponentPropertyToken property) const override;
	bool IsCompiledComponentPropertyCore(
		const DependencyPropertyMetadata& metadata) const noexcept override;

private:
	static const DependencyPropertyKey& StatePropertyKey();
	bool InitializeGeneratedTemplate(std::wstring* outError = nullptr);
	Border* _part_PART_Root = nullptr;
	StackPanel* _part_stackPanel1 = nullptr;
	Label* _part_PART_Caption = nullptr;
	Label* _part_PART_State = nullptr;
	StackPanel* _part_PART_Content = nullptr;
	Button* _part_PART_Invoke = nullptr;
	StackPanel* _part_PART_Actions = nullptr;
	Control* _presenter_Content = nullptr;
	Control* _content_Content = nullptr;
	Control* _presenter_Actions = nullptr;

public:
	DemoWindowGeneratedFeatureCard();
	~DemoWindowGeneratedFeatureCard() override = default;
	[[nodiscard]] static ComponentTypeToken ComponentTypeId() noexcept;
	[[nodiscard]] static const DependencyProperty& CaptionProperty();
	[[nodiscard]] std::wstring GetCaption() const;
	void SetCaption(std::wstring value);
	[[nodiscard]] static const DependencyProperty& StateProperty();
	[[nodiscard]] std::wstring GetState() const;
	bool PublishState(std::wstring value);
	[[nodiscard]] static const DependencyProperty& IsActiveProperty();
	[[nodiscard]] bool GetIsActive() const;
	void SetIsActive(bool value);
	[[nodiscard]] static const DependencyProperty& AccentColorProperty();
	[[nodiscard]] D2D1_COLOR_F GetAccentColor() const;
	void SetAccentColor(D2D1_COLOR_F value);
	[[nodiscard]] static const DependencyProperty& ContentPaddingProperty();
	[[nodiscard]] Thickness GetContentPadding() const;
	void SetContentPadding(Thickness value);
	[[nodiscard]] static const DeclarativeEventDefinition& InvokedEvent() noexcept;
	EventConnection SubscribeInvoked(DeclarativeEvent::std_function_type handler,
		bool handledEventsToo = false);
	bool RaiseInvoked();
	[[nodiscard]] static const DeclarativeEventDefinition& PulseEvent() noexcept;
	EventConnection SubscribePulse(DeclarativeEvent::std_function_type handler,
		bool handledEventsToo = false);
	bool RaisePulse();
	[[nodiscard]] static const DeclarativeEventDefinition& StopPulseEvent() noexcept;
	EventConnection SubscribeStopPulse(DeclarativeEvent::std_function_type handler,
		bool handledEventsToo = false);
	bool RaiseStopPulse();
	bool SetContent(std::unique_ptr<Control> value);
	bool AddActions(std::unique_ptr<Control> value);
	[[nodiscard]] Border* GetPART_Root() noexcept { return _part_PART_Root; }
	[[nodiscard]] const Border* GetPART_Root() const noexcept { return _part_PART_Root; }
	[[nodiscard]] Label* GetPART_Caption() noexcept { return _part_PART_Caption; }
	[[nodiscard]] const Label* GetPART_Caption() const noexcept { return _part_PART_Caption; }
	[[nodiscard]] Label* GetPART_State() noexcept { return _part_PART_State; }
	[[nodiscard]] const Label* GetPART_State() const noexcept { return _part_PART_State; }
	[[nodiscard]] StackPanel* GetPART_Content() noexcept { return _part_PART_Content; }
	[[nodiscard]] const StackPanel* GetPART_Content() const noexcept { return _part_PART_Content; }
	[[nodiscard]] Button* GetPART_Invoke() noexcept { return _part_PART_Invoke; }
	[[nodiscard]] const Button* GetPART_Invoke() const noexcept { return _part_PART_Invoke; }
	[[nodiscard]] StackPanel* GetPART_Actions() noexcept { return _part_PART_Actions; }
	[[nodiscard]] const StackPanel* GetPART_Actions() const noexcept { return _part_PART_Actions; }
};

class DemoWindowGenerated : public Window
{
protected:
	Grid* windowContent = nullptr;
	Menu* mainMenu = nullptr;
	MenuItem* menuItem1 = nullptr;
	MenuItem* menuItem2 = nullptr;
	Separator* separator1 = nullptr;
	MenuItem* menuItem3 = nullptr;
	MenuItem* menuItem4 = nullptr;
	MenuItem* menuItem5 = nullptr;
	ToolBar* mainToolBar = nullptr;
	Button* toolBasic = nullptr;
	Button* toolData = nullptr;
	Button* toolAnalytics = nullptr;
	Button* toolSystem = nullptr;
	Border* toolSeparator = nullptr;
	Button* toolIcon1 = nullptr;
	Image* toolIconImage1 = nullptr;
	Button* toolIcon2 = nullptr;
	Image* toolIconImage2 = nullptr;
	Button* toolIcon3 = nullptr;
	Image* toolIconImage3 = nullptr;
	Border* border1 = nullptr;
	Grid* grid1 = nullptr;
	Slider* globalProgress = nullptr;
	Label* statusText = nullptr;
	Label* runtimeBadge = nullptr;
	TabControl* mainTabs = nullptr;
	TabItem* tabItem1 = nullptr;
	Border* border2 = nullptr;
	Grid* basicSurface = nullptr;
	Label* basicTitle = nullptr;
	Label* frameworkThemeHint = nullptr;
	Grid* grid2 = nullptr;
	StackPanel* stackPanel1 = nullptr;
	Label* textBlock1 = nullptr;
	StackPanel* stackPanel2 = nullptr;
	Button* basicButton = nullptr;
	CheckBox* enableInput = nullptr;
	StackPanel* stackPanel3 = nullptr;
	RadioButton* radioA = nullptr;
	RadioButton* radioB = nullptr;
	Label* textBlock2 = nullptr;
	TextBox* nameInput = nullptr;
	PasswordBox* passwordInput = nullptr;
	ComboBox* basicCombo = nullptr;
	TextBox* dateInput = nullptr;
	StackPanel* stackPanel4 = nullptr;
	NumericUpDown* numberInput = nullptr;
	Button* dialogCancelButton = nullptr;
	Button* docsLink = nullptr;
	StackPanel* stackPanel5 = nullptr;
	Label* textBlock3 = nullptr;
	StackPanel* stackPanel6 = nullptr;
	Slider* verticalThemeSlider = nullptr;
	ProgressBar* verticalThemeProgress = nullptr;
	StackPanel* stackPanel7 = nullptr;
	Label* textBlock4 = nullptr;
	TextBox* gradientInput = nullptr;
	Label* gradientLabel = nullptr;
	DemoWindowGeneratedFeatureCard* featureCard = nullptr;
	Label* featureCardContent = nullptr;
	Button* featureActionA = nullptr;
	Button* featureActionB = nullptr;
	StackPanel* stackPanel8 = nullptr;
	Label* textBlock5 = nullptr;
	GroupBox* basicGroup = nullptr;
	StackPanel* basicGroupContent = nullptr;
	Label* groupHint = nullptr;
	TextBox* groupName = nullptr;
	CheckBox* groupEnabled = nullptr;
	StackPanel* stackPanel9 = nullptr;
	Button* themeNormalButton = nullptr;
	Button* themeDisabledButton = nullptr;
	Expander* basicExpander = nullptr;
	StackPanel* basicExpanderContent = nullptr;
	Label* expanderText = nullptr;
	ContentControl* themeContentControlProbe = nullptr;
	ItemsControl* themeItemsControlProbe = nullptr;
	Label* textBlock6 = nullptr;
	Separator* themeSeparatorProbe = nullptr;
	TabItem* tabItem2 = nullptr;
	Border* border3 = nullptr;
	Grid* grid3 = nullptr;
	Label* textBlock7 = nullptr;
	Calendar* calendarDemo = nullptr;
	StackPanel* stackPanel10 = nullptr;
	Label* textBlock8 = nullptr;
	Label* textBlock9 = nullptr;
	DatePicker* longDatePickerDemo = nullptr;
	Label* textBlock10 = nullptr;
	DatePicker* shortDatePickerDemo = nullptr;
	Border* border4 = nullptr;
	StackPanel* stackPanel11 = nullptr;
	Label* textBlock11 = nullptr;
	Label* textBlock12 = nullptr;
	TabItem* tabItem3 = nullptr;
	Border* border5 = nullptr;
	Grid* containerSurface = nullptr;
	Label* textBlock13 = nullptr;
	StackPanel* stackPanel12 = nullptr;
	Button* openImageButton = nullptr;
	Border* border6 = nullptr;
	Image* demoImage = nullptr;
	Label* textBlock14 = nullptr;
	ProgressBar* demoProgress = nullptr;
	Label* textBlock15 = nullptr;
	ProgressBar* indeterminateProgress = nullptr;
	StackPanel* stackPanel13 = nullptr;
	Label* textBlock16 = nullptr;
	WrapPanel* wrapPanel1 = nullptr;
	LoadingRing* loadingRing = nullptr;
	ProgressRing* progressRing = nullptr;
	StackPanel* stackPanel14 = nullptr;
	Switch* imageVisible = nullptr;
	Label* imageVisibleLabel = nullptr;
	NativeSurface* demoScene = nullptr;
	Label* textBlock17 = nullptr;
	Grid* grid4 = nullptr;
	Grid* detailGrid = nullptr;
	StackPanel* navigationComposition = nullptr;
	Label* textBlock18 = nullptr;
	ListBox* sideNavigationList = nullptr;
	Border* border7 = nullptr;
	StackPanel* detailComposition = nullptr;
	StackPanel* stackPanel15 = nullptr;
	Label* textBlock19 = nullptr;
	Label* textBlock20 = nullptr;
	Label* textBlock21 = nullptr;
	Label* textBlock22 = nullptr;
	Label* textBlock23 = nullptr;
	RichTextBox* splitNotes = nullptr;
	GroupBox* containerGroup = nullptr;
	Label* containerGroupText = nullptr;
	TabItem* tabItem4 = nullptr;
	Border* border8 = nullptr;
	Grid* dataSurface = nullptr;
	Label* textBlock24 = nullptr;
	TreeView* demoTree = nullptr;
	ListBox* demoListBox = nullptr;
	ListView* demoList = nullptr;
	GroupBox* composedPropertyEditor = nullptr;
	Grid* grid5 = nullptr;
	Label* textBlock25 = nullptr;
	TextBox* composedTitleEditor = nullptr;
	Label* textBlock26 = nullptr;
	CheckBox* composedEnabledEditor = nullptr;
	Label* textBlock27 = nullptr;
	ComboBox* composedDensityEditor = nullptr;
	ComboBoxItem* comboBoxItem1 = nullptr;
	ComboBoxItem* comboBoxItem2 = nullptr;
	ComboBoxItem* comboBoxItem3 = nullptr;
	Label* textBlock28 = nullptr;
	Slider* composedScaleEditor = nullptr;
	Label* textBlock29 = nullptr;
	StackPanel* stackPanel16 = nullptr;
	Label* textBlock30 = nullptr;
	TreeView* authoredStateTree = nullptr;
	TreeViewItem* treeViewItem1 = nullptr;
	TreeViewItem* treeViewItem2 = nullptr;
	TreeViewItem* treeViewItem3 = nullptr;
	TreeViewItem* treeViewItem4 = nullptr;
	Label* textBlock31 = nullptr;
	TabItem* tabItem5 = nullptr;
	Border* border9 = nullptr;
	Grid* analyticsSurface = nullptr;
	Label* textBlock32 = nullptr;
	Border* border10 = nullptr;
	Grid* analyticsFilterSurface = nullptr;
	TextBox* analyticsQuery = nullptr;
	CheckBox* analyticsClosed = nullptr;
	CheckBox* analyticsContract = nullptr;
	CheckBox* analyticsHighMargin = nullptr;
	Button* analyticsApply = nullptr;
	Button* analyticsReset = nullptr;
	Grid* grid6 = nullptr;
	GroupBox* groupBox1 = nullptr;
	StackPanel* stackPanel17 = nullptr;
	Label* textBlock33 = nullptr;
	Label* textBlock34 = nullptr;
	GroupBox* groupBox2 = nullptr;
	StackPanel* stackPanel18 = nullptr;
	Label* textBlock35 = nullptr;
	ProgressBar* progressBar1 = nullptr;
	GroupBox* groupBox3 = nullptr;
	StackPanel* stackPanel19 = nullptr;
	Label* textBlock36 = nullptr;
	Label* textBlock37 = nullptr;
	Label* textBlock38 = nullptr;
	WrapPanel* wrapPanel2 = nullptr;
	Button* chartBar = nullptr;
	Button* chartPie = nullptr;
	Button* chartLine = nullptr;
	Grid* grid7 = nullptr;
	ChartView* salesChart = nullptr;
	GroupBox* analyticsReport = nullptr;
	Grid* grid8 = nullptr;
	StackPanel* stackPanel20 = nullptr;
	Label* textBlock39 = nullptr;
	Label* textBlock40 = nullptr;
	Label* textBlock41 = nullptr;
	Label* textBlock42 = nullptr;
	Label* textBlock43 = nullptr;
	ListView* analyticsRows = nullptr;
	Label* textBlock44 = nullptr;
	TabItem* tabItem6 = nullptr;
	Border* border11 = nullptr;
	Grid* layoutSurface = nullptr;
	Label* layoutTitle = nullptr;
	Canvas* canvasSemanticsProbe = nullptr;
	Border* border12 = nullptr;
	Label* canvasLeftWins = nullptr;
	Label* canvasRightBottom = nullptr;
	Border* border13 = nullptr;
	StackPanel* demoStack = nullptr;
	Label* textBlock45 = nullptr;
	Button* stackA = nullptr;
	Button* stackB = nullptr;
	Button* stackC = nullptr;
	Border* border14 = nullptr;
	Grid* demoGrid = nullptr;
	Button* gridHeader = nullptr;
	Label* gridLeft = nullptr;
	TextBox* gridEditor = nullptr;
	Button* gridFooter = nullptr;
	Border* border15 = nullptr;
	DockPanel* demoDock = nullptr;
	Label* textBlock46 = nullptr;
	Button* dockTop = nullptr;
	Button* dockLeft = nullptr;
	Label* dockFill = nullptr;
	Border* border16 = nullptr;
	WrapPanel* demoWrap = nullptr;
	Button* wrap1 = nullptr;
	Button* wrap2 = nullptr;
	Button* wrap3 = nullptr;
	Button* wrap4 = nullptr;
	Button* wrap5 = nullptr;
	Button* wrap6 = nullptr;
	Border* border17 = nullptr;
	RelativePanel* demoRelative = nullptr;
	StackPanel* relativeCenter = nullptr;
	Label* naturalTextProbe = nullptr;
	Label* wrappedTextProbe = nullptr;
	Label* trimmedTextProbe = nullptr;
	Button* relativeCenterButton = nullptr;
	Border* border18 = nullptr;
	ScrollViewer* demoScroll = nullptr;
	Grid* demoScrollContent = nullptr;
	Border* border19 = nullptr;
	StackPanel* scrollCard1 = nullptr;
	Label* scrollCard1Text = nullptr;
	Label* textBlock47 = nullptr;
	Border* border20 = nullptr;
	StackPanel* scrollCard2 = nullptr;
	Label* scrollCard2Text = nullptr;
	Label* textBlock48 = nullptr;
	Label* textBlock49 = nullptr;
	Button* farButton = nullptr;
	TabItem* tabItem7 = nullptr;
	Border* border21 = nullptr;
	Grid* systemSurface = nullptr;
	Label* systemTitle = nullptr;
	StackPanel* stackPanel21 = nullptr;
	WrapPanel* wrapPanel3 = nullptr;
	Button* notifyToggle = nullptr;
	Button* notifyBalloon = nullptr;
	Button* showDialog = nullptr;
	Button* showToast = nullptr;
	Label* systemHint = nullptr;
	Border* border22 = nullptr;
	Grid* grid9 = nullptr;
	Label* textBlock50 = nullptr;
	Button* commandTargetButton = nullptr;
	Label* textBlock51 = nullptr;
	Label* commandTargetTrace = nullptr;
	Label* textBlock52 = nullptr;
	Label* textBlock53 = nullptr;
	Label* textBlock54 = nullptr;
	GroupBox* notificationPanel = nullptr;
	Grid* grid10 = nullptr;
	Label* textBlock55 = nullptr;
	Label* toastMessage = nullptr;
	ProgressBar* progressBar2 = nullptr;
	Button* dismissToast = nullptr;
	Label* textBlock56 = nullptr;
	TabItem* tabItem8 = nullptr;
	Border* border23 = nullptr;
	Grid* webSurface = nullptr;
	Grid* grid11 = nullptr;
	Button* invokeWeb = nullptr;
	Button* navigationWeb = nullptr;
	WebBrowser* webBrowser = nullptr;
	TabItem* tabItem9 = nullptr;
	Border* border24 = nullptr;
	Grid* mediaSurface = nullptr;
	MediaElement* mediaElement = nullptr;
	Grid* grid12 = nullptr;
	Button* mediaOpen = nullptr;
	Button* mediaPlay = nullptr;
	Button* mediaPause = nullptr;
	Button* mediaStop = nullptr;
	Label* volumeLabel = nullptr;
	Slider* mediaVolume = nullptr;
	Label* speedTitle = nullptr;
	Slider* mediaSpeed = nullptr;
	Label* mediaSpeedText = nullptr;
	CheckBox* mediaLoop = nullptr;
	Grid* grid13 = nullptr;
	Slider* mediaProgress = nullptr;
	Label* mediaTime = nullptr;
	TabItem* tabItem10 = nullptr;
	Border* border25 = nullptr;
	Grid* wpfLabSurface = nullptr;
	Label* wpfLabTitle = nullptr;
	ContentControl* wpfBindingScope = nullptr;
	StackPanel* stackPanel22 = nullptr;
	Label* wpfTypographyOverride = nullptr;
	TextBox* wpfTwoWayEditor = nullptr;
	Label* wpfElementMirror = nullptr;
	Label* wpfSelfValue = nullptr;
	Label* wpfAncestorValue = nullptr;
	Label* wpfFallbackValue = nullptr;
	Label* wpfNullValue = nullptr;
	Label* wpfIndexerValue = nullptr;
	Label* wpfKeyedIndexerValue = nullptr;
	Label* wpfConvertedValue = nullptr;
	Label* wpfMultiValue = nullptr;
	StackPanel* wpfTemplateAndStyleScope = nullptr;
	Label* textBlock57 = nullptr;
	Button* wpfTemplateButton = nullptr;
	Button* wpfTriggerButton = nullptr;
	Label* wpfScopeResourceValue = nullptr;
	StackPanel* wpfInnerResourceScope = nullptr;
	Label* wpfInnerResourceValue = nullptr;
	Label* textBlock58 = nullptr;
	Grid* wpfItemsScope = nullptr;
	Label* textBlock59 = nullptr;
	ListBox* wpfTemplateList = nullptr;
	Border* wpfRouteOuter = nullptr;
	Grid* grid14 = nullptr;
	Label* textBlock60 = nullptr;
	Grid* wpfRouteMiddle = nullptr;
	Button* wpfRouteSource = nullptr;
	Button* wpfFocusPeerB = nullptr;
	Button* wpfFocusPeerC = nullptr;
	Button* wpfNoFocusPeer = nullptr;
	TextBox* wpfTextInputSource = nullptr;
	Label* wpfRouteTrace = nullptr;
	Label* wpfInputStats = nullptr;
	Grid* wpfHierarchyScope = nullptr;
	Label* wpfHierarchyChain = nullptr;
	Button* wpfDispatcherProbe = nullptr;
	Label* wpfDispatcherResult = nullptr;
	TabItem* tabItem11 = nullptr;
	Border* border26 = nullptr;
	Grid* textCompositionLabSurface = nullptr;
	Label* textBlock61 = nullptr;
	Border* border27 = nullptr;
	Grid* grid15 = nullptr;
	Label* textBlock62 = nullptr;
	Label* textBlock63 = nullptr;
	TextBox* compositionTextBox = nullptr;
	Label* textBlock64 = nullptr;
	RichTextBox* compositionRichTextBox = nullptr;
	Label* textBlock65 = nullptr;
	PasswordBox* compositionPasswordBox = nullptr;
	Label* textBlock66 = nullptr;
	Border* border28 = nullptr;
	Grid* grid16 = nullptr;
	Label* textBlock67 = nullptr;
	WrapPanel* wrapPanel4 = nullptr;
	Button* compositionStartProbe = nullptr;
	Button* compositionUpdateProbe = nullptr;
	Button* compositionCommitProbe = nullptr;
	Button* compositionCancelProbe = nullptr;
	Button* compositionSurrogateProbe = nullptr;
	Button* compositionUnicharProbe = nullptr;
	Button* compositionFocusProbe = nullptr;
	Button* compositionPreviewHandledProbe = nullptr;
	Button* compositionResetProbe = nullptr;
	Label* compositionState = nullptr;
	Label* compositionStats = nullptr;
	Border* border29 = nullptr;
	Grid* grid17 = nullptr;
	Label* textBlock68 = nullptr;
	Label* compositionTrace = nullptr;
	TabItem* tabItem12 = nullptr;
	Border* border30 = nullptr;
	Grid* presentationLabSurface = nullptr;
	Label* textBlock69 = nullptr;
	Grid* grid18 = nullptr;
	NativeSurface* presentationProbeSurface = nullptr;
	Canvas* canvas1 = nullptr;
	Label* presentationTopologyTile = nullptr;
	StackPanel* stackPanel23 = nullptr;
	Label* textBlock70 = nullptr;
	Label* textBlock71 = nullptr;
	Label* textBlock72 = nullptr;
	Label* textBlock73 = nullptr;
	Label* textBlock74 = nullptr;
	Label* textBlock75 = nullptr;
	Grid* grid19 = nullptr;
	WrapPanel* wrapPanel5 = nullptr;
	Button* presentationRegionButton = nullptr;
	Button* presentationGeometryButton = nullptr;
	Button* presentationCompositionButton = nullptr;
	Button* presentationFullButton = nullptr;
	Button* presentationTopologyButton = nullptr;
	Button* presentationDeviceLossButton = nullptr;
	Label* presentationStatus = nullptr;
	Label* textBlock76 = nullptr;
	ContextMenu* systemContextMenu = nullptr;
	MenuItem* menuItem6 = nullptr;
	MenuItem* menuItem7 = nullptr;
	Separator* separator2 = nullptr;
	MenuItem* menuItem8 = nullptr;
	MenuItem* menuItem9 = nullptr;
	MenuItem* menuItem10 = nullptr;
	StatusBar* mainStatusBar = nullptr;
	std::vector<EventConnection> _generatedEventConnections;
	bool _componentInitialized = false;
	void InitializeComponent();

	virtual void HandleClosing(Window* sender, CancelEventArgs& e) = 0;
	virtual void HandleContentRendered(Window* sender) = 0;
	virtual void HandleCommandPreviewExecuted(Control* sender, ExecutedRoutedEventArgs& e) = 0;
	virtual void HandleCommandPreviewCanExecute(Control* sender, CanExecuteRoutedEventArgs& e) = 0;
	virtual void HandleCommandCanExecute(Control* sender, CanExecuteRoutedEventArgs& e) = 0;
	virtual void HandleCommandExecuted(Control* sender, ExecutedRoutedEventArgs& e) = 0;
	virtual void HandleToolBarAction(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleGlobalProgress(Control* sender, RoutedPropertyChangedEventArgs<double>& e) = 0;
	virtual void HandleMouseWheel(Control* sender, MouseEventArgs& e) = 0;
	virtual void HandleFeatureBubble(Control* sender, DeclarativeEventArgs& e) = 0;
	virtual void HandleBasicClick(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleEnableInput(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleRadio(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleComboSelection(Control* sender, SelectionChangedEventArgs& e) = 0;
	virtual void HandleNumericValue(Control* sender, RoutedPropertyChangedEventArgs<double>& e) = 0;
	virtual void HandleDocsLink(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleFeatureInvoked(Control* sender, DeclarativeEventArgs& e) = 0;
	virtual void HandleCommandAvailabilityToggle(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleExpander(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleDragRoute(Control* sender, DragEventArgs& e) = 0;
	virtual void HandleOpenImage(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleDropImage(Control* sender, DragEventArgs& e) = 0;
	virtual void HandleImageVisibility(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleListBoxSelection(Control* sender, SelectionChangedEventArgs& e) = 0;
	virtual void HandleTreeSelection(Control* sender, RoutedPropertyChangedEventArgs<BindingValue>& e) = 0;
	virtual void HandleListViewSelection(Control* sender, SelectionChangedEventArgs& e) = 0;
	virtual void HandleAnalyticsAction(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleChartKind(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleChartPoint(ChartView* sender, int seriesIndex, int pointIndex) = 0;
	virtual void HandleFarButton(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleSystemSurfaceMouseUp(Control* sender, MouseEventArgs& e) = 0;
	virtual void HandleSystemAction(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleInvokeWeb(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleNavigationWeb(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleMediaEnded(Control* sender) = 0;
	virtual void HandleMediaFailed(Control* sender) = 0;
	virtual void HandleMediaOpened(Control* sender) = 0;
	virtual void HandleMediaPosition(Control* sender, double position) = 0;
	virtual void HandleMediaCommand(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleMediaVolume(Control* sender, RoutedPropertyChangedEventArgs<double>& e) = 0;
	virtual void HandleMediaSpeed(Control* sender, RoutedPropertyChangedEventArgs<double>& e) = 0;
	virtual void HandleMediaLoop(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleMediaSeek(Control* sender, RoutedPropertyChangedEventArgs<double>& e) = 0;
	virtual void HandleTemplateSwap(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleRouteKey(Control* sender, KeyEventArgs& e) = 0;
	virtual void HandleRouteOuterPreview(Control* sender, MouseEventArgs& e) = 0;
	virtual void HandleTextOuterPreview(Control* sender, TextCompositionEventArgs& e) = 0;
	virtual void HandleTextOuterBubble(Control* sender, TextCompositionEventArgs& e) = 0;
	virtual void HandleRouteMiddleBubble(Control* sender, MouseEventArgs& e) = 0;
	virtual void HandleRouteMiddlePreview(Control* sender, MouseEventArgs& e) = 0;
	virtual void HandleRouteFocus(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleRouteGotKeyboardFocus(Control* sender, KeyboardFocusChangedEventArgs& e) = 0;
	virtual void HandleRouteCaptureChanged(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleRouteLostKeyboardFocus(Control* sender, KeyboardFocusChangedEventArgs& e) = 0;
	virtual void HandleRouteSourceBubble(Control* sender, MouseEventArgs& e) = 0;
	virtual void HandleRoutePreviewGotKeyboardFocus(Control* sender, KeyboardFocusChangedEventArgs& e) = 0;
	virtual void HandleRoutePreviewLostKeyboardFocus(Control* sender, KeyboardFocusChangedEventArgs& e) = 0;
	virtual void HandleRouteSourcePreview(Control* sender, MouseEventArgs& e) = 0;
	virtual void HandleTextSourcePreview(Control* sender, TextCompositionEventArgs& e) = 0;
	virtual void HandleTextSourceBubble(Control* sender, TextCompositionEventArgs& e) = 0;
	virtual void HandleLocalCommandCanExecute(Control* sender, CanExecuteRoutedEventArgs& e) = 0;
	virtual void HandleLocalCommandExecuted(Control* sender, ExecutedRoutedEventArgs& e) = 0;
	virtual void HandleDispatcherProbe(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleCompositionPreviewCommit(Control* sender, TextCompositionEventArgs& e) = 0;
	virtual void HandleCompositionPreviewStart(Control* sender, TextCompositionEventArgs& e) = 0;
	virtual void HandleCompositionPreviewUpdate(Control* sender, TextCompositionEventArgs& e) = 0;
	virtual void HandleCompositionCommit(Control* sender, TextCompositionEventArgs& e) = 0;
	virtual void HandleCompositionStart(Control* sender, TextCompositionEventArgs& e) = 0;
	virtual void HandleCompositionUpdate(Control* sender, TextCompositionEventArgs& e) = 0;
	virtual void HandleTextCompositionProbe(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandlePresentationRegion(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandlePresentationGeometry(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandlePresentationComposition(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandlePresentationFullFrame(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandlePresentationTopology(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandlePresentationDeviceLoss(Control* sender, RoutedEventArgs& e) = 0;

public:
	// Name-free identities for the authored root DataContext contract.
	struct DataContextProperties final
	{
		static constexpr BindingSourcePropertyToken DemoListEntries{ 13985105387377820950ULL };
		static constexpr BindingSourcePropertyToken TreeRoots{ 2602425173560606198ULL };
		static constexpr BindingSourcePropertyToken WpfFirst{ 9835099465518400830ULL };
		static constexpr BindingSourcePropertyToken WpfIsAdmin{ 15197647781564744993ULL };
		static constexpr BindingSourcePropertyToken WpfLast{ 2002927140446823006ULL };
		static constexpr BindingSourcePropertyToken WpfMissing{ 9378076335107128704ULL };
		static constexpr BindingSourcePropertyToken WpfNullable{ 16822651602587833845ULL };
		static constexpr BindingSourcePropertyToken WpfPadded{ 4327259317505158084ULL };
		static constexpr BindingSourcePropertyToken WpfPeople{ 3015263015765239239ULL };
		static constexpr BindingSourcePropertyToken WpfSettings{ 13417529444444422561ULL };
		static constexpr BindingSourcePropertyToken WpfStatus{ 13692166754359878880ULL };
	};

	// Type-safe x:Name accessors; ownership remains with the generated Window.
	[[nodiscard]] Grid* GetWindowContent() noexcept { return windowContent; }
	[[nodiscard]] const Grid* GetWindowContent() const noexcept { return windowContent; }
	[[nodiscard]] Menu* GetMainMenu() noexcept { return mainMenu; }
	[[nodiscard]] const Menu* GetMainMenu() const noexcept { return mainMenu; }
	[[nodiscard]] ToolBar* GetMainToolBar() noexcept { return mainToolBar; }
	[[nodiscard]] const ToolBar* GetMainToolBar() const noexcept { return mainToolBar; }
	[[nodiscard]] Button* GetToolBasic() noexcept { return toolBasic; }
	[[nodiscard]] const Button* GetToolBasic() const noexcept { return toolBasic; }
	[[nodiscard]] Button* GetToolData() noexcept { return toolData; }
	[[nodiscard]] const Button* GetToolData() const noexcept { return toolData; }
	[[nodiscard]] Button* GetToolAnalytics() noexcept { return toolAnalytics; }
	[[nodiscard]] const Button* GetToolAnalytics() const noexcept { return toolAnalytics; }
	[[nodiscard]] Button* GetToolSystem() noexcept { return toolSystem; }
	[[nodiscard]] const Button* GetToolSystem() const noexcept { return toolSystem; }
	[[nodiscard]] Border* GetToolSeparator() noexcept { return toolSeparator; }
	[[nodiscard]] const Border* GetToolSeparator() const noexcept { return toolSeparator; }
	[[nodiscard]] Button* GetToolIcon1() noexcept { return toolIcon1; }
	[[nodiscard]] const Button* GetToolIcon1() const noexcept { return toolIcon1; }
	[[nodiscard]] Image* GetToolIconImage1() noexcept { return toolIconImage1; }
	[[nodiscard]] const Image* GetToolIconImage1() const noexcept { return toolIconImage1; }
	[[nodiscard]] Button* GetToolIcon2() noexcept { return toolIcon2; }
	[[nodiscard]] const Button* GetToolIcon2() const noexcept { return toolIcon2; }
	[[nodiscard]] Image* GetToolIconImage2() noexcept { return toolIconImage2; }
	[[nodiscard]] const Image* GetToolIconImage2() const noexcept { return toolIconImage2; }
	[[nodiscard]] Button* GetToolIcon3() noexcept { return toolIcon3; }
	[[nodiscard]] const Button* GetToolIcon3() const noexcept { return toolIcon3; }
	[[nodiscard]] Image* GetToolIconImage3() noexcept { return toolIconImage3; }
	[[nodiscard]] const Image* GetToolIconImage3() const noexcept { return toolIconImage3; }
	[[nodiscard]] Slider* GetGlobalProgress() noexcept { return globalProgress; }
	[[nodiscard]] const Slider* GetGlobalProgress() const noexcept { return globalProgress; }
	[[nodiscard]] Label* GetStatusText() noexcept { return statusText; }
	[[nodiscard]] const Label* GetStatusText() const noexcept { return statusText; }
	[[nodiscard]] Label* GetRuntimeBadge() noexcept { return runtimeBadge; }
	[[nodiscard]] const Label* GetRuntimeBadge() const noexcept { return runtimeBadge; }
	[[nodiscard]] TabControl* GetMainTabs() noexcept { return mainTabs; }
	[[nodiscard]] const TabControl* GetMainTabs() const noexcept { return mainTabs; }
	[[nodiscard]] Grid* GetBasicSurface() noexcept { return basicSurface; }
	[[nodiscard]] const Grid* GetBasicSurface() const noexcept { return basicSurface; }
	[[nodiscard]] Label* GetBasicTitle() noexcept { return basicTitle; }
	[[nodiscard]] const Label* GetBasicTitle() const noexcept { return basicTitle; }
	[[nodiscard]] Label* GetFrameworkThemeHint() noexcept { return frameworkThemeHint; }
	[[nodiscard]] const Label* GetFrameworkThemeHint() const noexcept { return frameworkThemeHint; }
	[[nodiscard]] Button* GetBasicButton() noexcept { return basicButton; }
	[[nodiscard]] const Button* GetBasicButton() const noexcept { return basicButton; }
	[[nodiscard]] CheckBox* GetEnableInput() noexcept { return enableInput; }
	[[nodiscard]] const CheckBox* GetEnableInput() const noexcept { return enableInput; }
	[[nodiscard]] RadioButton* GetRadioA() noexcept { return radioA; }
	[[nodiscard]] const RadioButton* GetRadioA() const noexcept { return radioA; }
	[[nodiscard]] RadioButton* GetRadioB() noexcept { return radioB; }
	[[nodiscard]] const RadioButton* GetRadioB() const noexcept { return radioB; }
	[[nodiscard]] TextBox* GetNameInput() noexcept { return nameInput; }
	[[nodiscard]] const TextBox* GetNameInput() const noexcept { return nameInput; }
	[[nodiscard]] PasswordBox* GetPasswordInput() noexcept { return passwordInput; }
	[[nodiscard]] const PasswordBox* GetPasswordInput() const noexcept { return passwordInput; }
	[[nodiscard]] ComboBox* GetBasicCombo() noexcept { return basicCombo; }
	[[nodiscard]] const ComboBox* GetBasicCombo() const noexcept { return basicCombo; }
	[[nodiscard]] TextBox* GetDateInput() noexcept { return dateInput; }
	[[nodiscard]] const TextBox* GetDateInput() const noexcept { return dateInput; }
	[[nodiscard]] NumericUpDown* GetNumberInput() noexcept { return numberInput; }
	[[nodiscard]] const NumericUpDown* GetNumberInput() const noexcept { return numberInput; }
	[[nodiscard]] Button* GetDialogCancelButton() noexcept { return dialogCancelButton; }
	[[nodiscard]] const Button* GetDialogCancelButton() const noexcept { return dialogCancelButton; }
	[[nodiscard]] Button* GetDocsLink() noexcept { return docsLink; }
	[[nodiscard]] const Button* GetDocsLink() const noexcept { return docsLink; }
	[[nodiscard]] Slider* GetVerticalThemeSlider() noexcept { return verticalThemeSlider; }
	[[nodiscard]] const Slider* GetVerticalThemeSlider() const noexcept { return verticalThemeSlider; }
	[[nodiscard]] ProgressBar* GetVerticalThemeProgress() noexcept { return verticalThemeProgress; }
	[[nodiscard]] const ProgressBar* GetVerticalThemeProgress() const noexcept { return verticalThemeProgress; }
	[[nodiscard]] TextBox* GetGradientInput() noexcept { return gradientInput; }
	[[nodiscard]] const TextBox* GetGradientInput() const noexcept { return gradientInput; }
	[[nodiscard]] Label* GetGradientLabel() noexcept { return gradientLabel; }
	[[nodiscard]] const Label* GetGradientLabel() const noexcept { return gradientLabel; }
	[[nodiscard]] DemoWindowGeneratedFeatureCard* GetFeatureCard() noexcept { return featureCard; }
	[[nodiscard]] const DemoWindowGeneratedFeatureCard* GetFeatureCard() const noexcept { return featureCard; }
	[[nodiscard]] Label* GetFeatureCardContent() noexcept { return featureCardContent; }
	[[nodiscard]] const Label* GetFeatureCardContent() const noexcept { return featureCardContent; }
	[[nodiscard]] Button* GetFeatureActionA() noexcept { return featureActionA; }
	[[nodiscard]] const Button* GetFeatureActionA() const noexcept { return featureActionA; }
	[[nodiscard]] Button* GetFeatureActionB() noexcept { return featureActionB; }
	[[nodiscard]] const Button* GetFeatureActionB() const noexcept { return featureActionB; }
	[[nodiscard]] GroupBox* GetBasicGroup() noexcept { return basicGroup; }
	[[nodiscard]] const GroupBox* GetBasicGroup() const noexcept { return basicGroup; }
	[[nodiscard]] StackPanel* GetBasicGroupContent() noexcept { return basicGroupContent; }
	[[nodiscard]] const StackPanel* GetBasicGroupContent() const noexcept { return basicGroupContent; }
	[[nodiscard]] Label* GetGroupHint() noexcept { return groupHint; }
	[[nodiscard]] const Label* GetGroupHint() const noexcept { return groupHint; }
	[[nodiscard]] TextBox* GetGroupName() noexcept { return groupName; }
	[[nodiscard]] const TextBox* GetGroupName() const noexcept { return groupName; }
	[[nodiscard]] CheckBox* GetGroupEnabled() noexcept { return groupEnabled; }
	[[nodiscard]] const CheckBox* GetGroupEnabled() const noexcept { return groupEnabled; }
	[[nodiscard]] Button* GetThemeNormalButton() noexcept { return themeNormalButton; }
	[[nodiscard]] const Button* GetThemeNormalButton() const noexcept { return themeNormalButton; }
	[[nodiscard]] Button* GetThemeDisabledButton() noexcept { return themeDisabledButton; }
	[[nodiscard]] const Button* GetThemeDisabledButton() const noexcept { return themeDisabledButton; }
	[[nodiscard]] Expander* GetBasicExpander() noexcept { return basicExpander; }
	[[nodiscard]] const Expander* GetBasicExpander() const noexcept { return basicExpander; }
	[[nodiscard]] StackPanel* GetBasicExpanderContent() noexcept { return basicExpanderContent; }
	[[nodiscard]] const StackPanel* GetBasicExpanderContent() const noexcept { return basicExpanderContent; }
	[[nodiscard]] Label* GetExpanderText() noexcept { return expanderText; }
	[[nodiscard]] const Label* GetExpanderText() const noexcept { return expanderText; }
	[[nodiscard]] ContentControl* GetThemeContentControlProbe() noexcept { return themeContentControlProbe; }
	[[nodiscard]] const ContentControl* GetThemeContentControlProbe() const noexcept { return themeContentControlProbe; }
	[[nodiscard]] ItemsControl* GetThemeItemsControlProbe() noexcept { return themeItemsControlProbe; }
	[[nodiscard]] const ItemsControl* GetThemeItemsControlProbe() const noexcept { return themeItemsControlProbe; }
	[[nodiscard]] Separator* GetThemeSeparatorProbe() noexcept { return themeSeparatorProbe; }
	[[nodiscard]] const Separator* GetThemeSeparatorProbe() const noexcept { return themeSeparatorProbe; }
	[[nodiscard]] Calendar* GetCalendarDemo() noexcept { return calendarDemo; }
	[[nodiscard]] const Calendar* GetCalendarDemo() const noexcept { return calendarDemo; }
	[[nodiscard]] DatePicker* GetLongDatePickerDemo() noexcept { return longDatePickerDemo; }
	[[nodiscard]] const DatePicker* GetLongDatePickerDemo() const noexcept { return longDatePickerDemo; }
	[[nodiscard]] DatePicker* GetShortDatePickerDemo() noexcept { return shortDatePickerDemo; }
	[[nodiscard]] const DatePicker* GetShortDatePickerDemo() const noexcept { return shortDatePickerDemo; }
	[[nodiscard]] Grid* GetContainerSurface() noexcept { return containerSurface; }
	[[nodiscard]] const Grid* GetContainerSurface() const noexcept { return containerSurface; }
	[[nodiscard]] Button* GetOpenImageButton() noexcept { return openImageButton; }
	[[nodiscard]] const Button* GetOpenImageButton() const noexcept { return openImageButton; }
	[[nodiscard]] Image* GetDemoImage() noexcept { return demoImage; }
	[[nodiscard]] const Image* GetDemoImage() const noexcept { return demoImage; }
	[[nodiscard]] ProgressBar* GetDemoProgress() noexcept { return demoProgress; }
	[[nodiscard]] const ProgressBar* GetDemoProgress() const noexcept { return demoProgress; }
	[[nodiscard]] ProgressBar* GetIndeterminateProgress() noexcept { return indeterminateProgress; }
	[[nodiscard]] const ProgressBar* GetIndeterminateProgress() const noexcept { return indeterminateProgress; }
	[[nodiscard]] LoadingRing* GetLoadingRing() noexcept { return loadingRing; }
	[[nodiscard]] const LoadingRing* GetLoadingRing() const noexcept { return loadingRing; }
	[[nodiscard]] ProgressRing* GetProgressRing() noexcept { return progressRing; }
	[[nodiscard]] const ProgressRing* GetProgressRing() const noexcept { return progressRing; }
	[[nodiscard]] Switch* GetImageVisible() noexcept { return imageVisible; }
	[[nodiscard]] const Switch* GetImageVisible() const noexcept { return imageVisible; }
	[[nodiscard]] Label* GetImageVisibleLabel() noexcept { return imageVisibleLabel; }
	[[nodiscard]] const Label* GetImageVisibleLabel() const noexcept { return imageVisibleLabel; }
	[[nodiscard]] NativeSurface* GetDemoScene() noexcept { return demoScene; }
	[[nodiscard]] const NativeSurface* GetDemoScene() const noexcept { return demoScene; }
	[[nodiscard]] Grid* GetDetailGrid() noexcept { return detailGrid; }
	[[nodiscard]] const Grid* GetDetailGrid() const noexcept { return detailGrid; }
	[[nodiscard]] StackPanel* GetNavigationComposition() noexcept { return navigationComposition; }
	[[nodiscard]] const StackPanel* GetNavigationComposition() const noexcept { return navigationComposition; }
	[[nodiscard]] ListBox* GetSideNavigationList() noexcept { return sideNavigationList; }
	[[nodiscard]] const ListBox* GetSideNavigationList() const noexcept { return sideNavigationList; }
	[[nodiscard]] StackPanel* GetDetailComposition() noexcept { return detailComposition; }
	[[nodiscard]] const StackPanel* GetDetailComposition() const noexcept { return detailComposition; }
	[[nodiscard]] RichTextBox* GetSplitNotes() noexcept { return splitNotes; }
	[[nodiscard]] const RichTextBox* GetSplitNotes() const noexcept { return splitNotes; }
	[[nodiscard]] GroupBox* GetContainerGroup() noexcept { return containerGroup; }
	[[nodiscard]] const GroupBox* GetContainerGroup() const noexcept { return containerGroup; }
	[[nodiscard]] Label* GetContainerGroupText() noexcept { return containerGroupText; }
	[[nodiscard]] const Label* GetContainerGroupText() const noexcept { return containerGroupText; }
	[[nodiscard]] Grid* GetDataSurface() noexcept { return dataSurface; }
	[[nodiscard]] const Grid* GetDataSurface() const noexcept { return dataSurface; }
	[[nodiscard]] TreeView* GetDemoTree() noexcept { return demoTree; }
	[[nodiscard]] const TreeView* GetDemoTree() const noexcept { return demoTree; }
	[[nodiscard]] ListBox* GetDemoListBox() noexcept { return demoListBox; }
	[[nodiscard]] const ListBox* GetDemoListBox() const noexcept { return demoListBox; }
	[[nodiscard]] ListView* GetDemoList() noexcept { return demoList; }
	[[nodiscard]] const ListView* GetDemoList() const noexcept { return demoList; }
	[[nodiscard]] GroupBox* GetComposedPropertyEditor() noexcept { return composedPropertyEditor; }
	[[nodiscard]] const GroupBox* GetComposedPropertyEditor() const noexcept { return composedPropertyEditor; }
	[[nodiscard]] TextBox* GetComposedTitleEditor() noexcept { return composedTitleEditor; }
	[[nodiscard]] const TextBox* GetComposedTitleEditor() const noexcept { return composedTitleEditor; }
	[[nodiscard]] CheckBox* GetComposedEnabledEditor() noexcept { return composedEnabledEditor; }
	[[nodiscard]] const CheckBox* GetComposedEnabledEditor() const noexcept { return composedEnabledEditor; }
	[[nodiscard]] ComboBox* GetComposedDensityEditor() noexcept { return composedDensityEditor; }
	[[nodiscard]] const ComboBox* GetComposedDensityEditor() const noexcept { return composedDensityEditor; }
	[[nodiscard]] Slider* GetComposedScaleEditor() noexcept { return composedScaleEditor; }
	[[nodiscard]] const Slider* GetComposedScaleEditor() const noexcept { return composedScaleEditor; }
	[[nodiscard]] TreeView* GetAuthoredStateTree() noexcept { return authoredStateTree; }
	[[nodiscard]] const TreeView* GetAuthoredStateTree() const noexcept { return authoredStateTree; }
	[[nodiscard]] Grid* GetAnalyticsSurface() noexcept { return analyticsSurface; }
	[[nodiscard]] const Grid* GetAnalyticsSurface() const noexcept { return analyticsSurface; }
	[[nodiscard]] Grid* GetAnalyticsFilterSurface() noexcept { return analyticsFilterSurface; }
	[[nodiscard]] const Grid* GetAnalyticsFilterSurface() const noexcept { return analyticsFilterSurface; }
	[[nodiscard]] TextBox* GetAnalyticsQuery() noexcept { return analyticsQuery; }
	[[nodiscard]] const TextBox* GetAnalyticsQuery() const noexcept { return analyticsQuery; }
	[[nodiscard]] CheckBox* GetAnalyticsClosed() noexcept { return analyticsClosed; }
	[[nodiscard]] const CheckBox* GetAnalyticsClosed() const noexcept { return analyticsClosed; }
	[[nodiscard]] CheckBox* GetAnalyticsContract() noexcept { return analyticsContract; }
	[[nodiscard]] const CheckBox* GetAnalyticsContract() const noexcept { return analyticsContract; }
	[[nodiscard]] CheckBox* GetAnalyticsHighMargin() noexcept { return analyticsHighMargin; }
	[[nodiscard]] const CheckBox* GetAnalyticsHighMargin() const noexcept { return analyticsHighMargin; }
	[[nodiscard]] Button* GetAnalyticsApply() noexcept { return analyticsApply; }
	[[nodiscard]] const Button* GetAnalyticsApply() const noexcept { return analyticsApply; }
	[[nodiscard]] Button* GetAnalyticsReset() noexcept { return analyticsReset; }
	[[nodiscard]] const Button* GetAnalyticsReset() const noexcept { return analyticsReset; }
	[[nodiscard]] Button* GetChartBar() noexcept { return chartBar; }
	[[nodiscard]] const Button* GetChartBar() const noexcept { return chartBar; }
	[[nodiscard]] Button* GetChartPie() noexcept { return chartPie; }
	[[nodiscard]] const Button* GetChartPie() const noexcept { return chartPie; }
	[[nodiscard]] Button* GetChartLine() noexcept { return chartLine; }
	[[nodiscard]] const Button* GetChartLine() const noexcept { return chartLine; }
	[[nodiscard]] ChartView* GetSalesChart() noexcept { return salesChart; }
	[[nodiscard]] const ChartView* GetSalesChart() const noexcept { return salesChart; }
	[[nodiscard]] GroupBox* GetAnalyticsReport() noexcept { return analyticsReport; }
	[[nodiscard]] const GroupBox* GetAnalyticsReport() const noexcept { return analyticsReport; }
	[[nodiscard]] ListView* GetAnalyticsRows() noexcept { return analyticsRows; }
	[[nodiscard]] const ListView* GetAnalyticsRows() const noexcept { return analyticsRows; }
	[[nodiscard]] Grid* GetLayoutSurface() noexcept { return layoutSurface; }
	[[nodiscard]] const Grid* GetLayoutSurface() const noexcept { return layoutSurface; }
	[[nodiscard]] Label* GetLayoutTitle() noexcept { return layoutTitle; }
	[[nodiscard]] const Label* GetLayoutTitle() const noexcept { return layoutTitle; }
	[[nodiscard]] Canvas* GetCanvasSemanticsProbe() noexcept { return canvasSemanticsProbe; }
	[[nodiscard]] const Canvas* GetCanvasSemanticsProbe() const noexcept { return canvasSemanticsProbe; }
	[[nodiscard]] Label* GetCanvasLeftWins() noexcept { return canvasLeftWins; }
	[[nodiscard]] const Label* GetCanvasLeftWins() const noexcept { return canvasLeftWins; }
	[[nodiscard]] Label* GetCanvasRightBottom() noexcept { return canvasRightBottom; }
	[[nodiscard]] const Label* GetCanvasRightBottom() const noexcept { return canvasRightBottom; }
	[[nodiscard]] StackPanel* GetDemoStack() noexcept { return demoStack; }
	[[nodiscard]] const StackPanel* GetDemoStack() const noexcept { return demoStack; }
	[[nodiscard]] Button* GetStackA() noexcept { return stackA; }
	[[nodiscard]] const Button* GetStackA() const noexcept { return stackA; }
	[[nodiscard]] Button* GetStackB() noexcept { return stackB; }
	[[nodiscard]] const Button* GetStackB() const noexcept { return stackB; }
	[[nodiscard]] Button* GetStackC() noexcept { return stackC; }
	[[nodiscard]] const Button* GetStackC() const noexcept { return stackC; }
	[[nodiscard]] Grid* GetDemoGrid() noexcept { return demoGrid; }
	[[nodiscard]] const Grid* GetDemoGrid() const noexcept { return demoGrid; }
	[[nodiscard]] Button* GetGridHeader() noexcept { return gridHeader; }
	[[nodiscard]] const Button* GetGridHeader() const noexcept { return gridHeader; }
	[[nodiscard]] Label* GetGridLeft() noexcept { return gridLeft; }
	[[nodiscard]] const Label* GetGridLeft() const noexcept { return gridLeft; }
	[[nodiscard]] TextBox* GetGridEditor() noexcept { return gridEditor; }
	[[nodiscard]] const TextBox* GetGridEditor() const noexcept { return gridEditor; }
	[[nodiscard]] Button* GetGridFooter() noexcept { return gridFooter; }
	[[nodiscard]] const Button* GetGridFooter() const noexcept { return gridFooter; }
	[[nodiscard]] DockPanel* GetDemoDock() noexcept { return demoDock; }
	[[nodiscard]] const DockPanel* GetDemoDock() const noexcept { return demoDock; }
	[[nodiscard]] Button* GetDockTop() noexcept { return dockTop; }
	[[nodiscard]] const Button* GetDockTop() const noexcept { return dockTop; }
	[[nodiscard]] Button* GetDockLeft() noexcept { return dockLeft; }
	[[nodiscard]] const Button* GetDockLeft() const noexcept { return dockLeft; }
	[[nodiscard]] Label* GetDockFill() noexcept { return dockFill; }
	[[nodiscard]] const Label* GetDockFill() const noexcept { return dockFill; }
	[[nodiscard]] WrapPanel* GetDemoWrap() noexcept { return demoWrap; }
	[[nodiscard]] const WrapPanel* GetDemoWrap() const noexcept { return demoWrap; }
	[[nodiscard]] Button* GetWrap1() noexcept { return wrap1; }
	[[nodiscard]] const Button* GetWrap1() const noexcept { return wrap1; }
	[[nodiscard]] Button* GetWrap2() noexcept { return wrap2; }
	[[nodiscard]] const Button* GetWrap2() const noexcept { return wrap2; }
	[[nodiscard]] Button* GetWrap3() noexcept { return wrap3; }
	[[nodiscard]] const Button* GetWrap3() const noexcept { return wrap3; }
	[[nodiscard]] Button* GetWrap4() noexcept { return wrap4; }
	[[nodiscard]] const Button* GetWrap4() const noexcept { return wrap4; }
	[[nodiscard]] Button* GetWrap5() noexcept { return wrap5; }
	[[nodiscard]] const Button* GetWrap5() const noexcept { return wrap5; }
	[[nodiscard]] Button* GetWrap6() noexcept { return wrap6; }
	[[nodiscard]] const Button* GetWrap6() const noexcept { return wrap6; }
	[[nodiscard]] RelativePanel* GetDemoRelative() noexcept { return demoRelative; }
	[[nodiscard]] const RelativePanel* GetDemoRelative() const noexcept { return demoRelative; }
	[[nodiscard]] StackPanel* GetRelativeCenter() noexcept { return relativeCenter; }
	[[nodiscard]] const StackPanel* GetRelativeCenter() const noexcept { return relativeCenter; }
	[[nodiscard]] Label* GetNaturalTextProbe() noexcept { return naturalTextProbe; }
	[[nodiscard]] const Label* GetNaturalTextProbe() const noexcept { return naturalTextProbe; }
	[[nodiscard]] Label* GetWrappedTextProbe() noexcept { return wrappedTextProbe; }
	[[nodiscard]] const Label* GetWrappedTextProbe() const noexcept { return wrappedTextProbe; }
	[[nodiscard]] Label* GetTrimmedTextProbe() noexcept { return trimmedTextProbe; }
	[[nodiscard]] const Label* GetTrimmedTextProbe() const noexcept { return trimmedTextProbe; }
	[[nodiscard]] Button* GetRelativeCenterButton() noexcept { return relativeCenterButton; }
	[[nodiscard]] const Button* GetRelativeCenterButton() const noexcept { return relativeCenterButton; }
	[[nodiscard]] ScrollViewer* GetDemoScroll() noexcept { return demoScroll; }
	[[nodiscard]] const ScrollViewer* GetDemoScroll() const noexcept { return demoScroll; }
	[[nodiscard]] Grid* GetDemoScrollContent() noexcept { return demoScrollContent; }
	[[nodiscard]] const Grid* GetDemoScrollContent() const noexcept { return demoScrollContent; }
	[[nodiscard]] StackPanel* GetScrollCard1() noexcept { return scrollCard1; }
	[[nodiscard]] const StackPanel* GetScrollCard1() const noexcept { return scrollCard1; }
	[[nodiscard]] Label* GetScrollCard1Text() noexcept { return scrollCard1Text; }
	[[nodiscard]] const Label* GetScrollCard1Text() const noexcept { return scrollCard1Text; }
	[[nodiscard]] StackPanel* GetScrollCard2() noexcept { return scrollCard2; }
	[[nodiscard]] const StackPanel* GetScrollCard2() const noexcept { return scrollCard2; }
	[[nodiscard]] Label* GetScrollCard2Text() noexcept { return scrollCard2Text; }
	[[nodiscard]] const Label* GetScrollCard2Text() const noexcept { return scrollCard2Text; }
	[[nodiscard]] Button* GetFarButton() noexcept { return farButton; }
	[[nodiscard]] const Button* GetFarButton() const noexcept { return farButton; }
	[[nodiscard]] Grid* GetSystemSurface() noexcept { return systemSurface; }
	[[nodiscard]] const Grid* GetSystemSurface() const noexcept { return systemSurface; }
	[[nodiscard]] Label* GetSystemTitle() noexcept { return systemTitle; }
	[[nodiscard]] const Label* GetSystemTitle() const noexcept { return systemTitle; }
	[[nodiscard]] Button* GetNotifyToggle() noexcept { return notifyToggle; }
	[[nodiscard]] const Button* GetNotifyToggle() const noexcept { return notifyToggle; }
	[[nodiscard]] Button* GetNotifyBalloon() noexcept { return notifyBalloon; }
	[[nodiscard]] const Button* GetNotifyBalloon() const noexcept { return notifyBalloon; }
	[[nodiscard]] Button* GetShowDialog() noexcept { return showDialog; }
	[[nodiscard]] const Button* GetShowDialog() const noexcept { return showDialog; }
	[[nodiscard]] Button* GetShowToast() noexcept { return showToast; }
	[[nodiscard]] const Button* GetShowToast() const noexcept { return showToast; }
	[[nodiscard]] Label* GetSystemHint() noexcept { return systemHint; }
	[[nodiscard]] const Label* GetSystemHint() const noexcept { return systemHint; }
	[[nodiscard]] Button* GetCommandTargetButton() noexcept { return commandTargetButton; }
	[[nodiscard]] const Button* GetCommandTargetButton() const noexcept { return commandTargetButton; }
	[[nodiscard]] Label* GetCommandTargetTrace() noexcept { return commandTargetTrace; }
	[[nodiscard]] const Label* GetCommandTargetTrace() const noexcept { return commandTargetTrace; }
	[[nodiscard]] GroupBox* GetNotificationPanel() noexcept { return notificationPanel; }
	[[nodiscard]] const GroupBox* GetNotificationPanel() const noexcept { return notificationPanel; }
	[[nodiscard]] Label* GetToastMessage() noexcept { return toastMessage; }
	[[nodiscard]] const Label* GetToastMessage() const noexcept { return toastMessage; }
	[[nodiscard]] Button* GetDismissToast() noexcept { return dismissToast; }
	[[nodiscard]] const Button* GetDismissToast() const noexcept { return dismissToast; }
	[[nodiscard]] Grid* GetWebSurface() noexcept { return webSurface; }
	[[nodiscard]] const Grid* GetWebSurface() const noexcept { return webSurface; }
	[[nodiscard]] Button* GetInvokeWeb() noexcept { return invokeWeb; }
	[[nodiscard]] const Button* GetInvokeWeb() const noexcept { return invokeWeb; }
	[[nodiscard]] Button* GetNavigationWeb() noexcept { return navigationWeb; }
	[[nodiscard]] const Button* GetNavigationWeb() const noexcept { return navigationWeb; }
	[[nodiscard]] WebBrowser* GetWebBrowser() noexcept { return webBrowser; }
	[[nodiscard]] const WebBrowser* GetWebBrowser() const noexcept { return webBrowser; }
	[[nodiscard]] Grid* GetMediaSurface() noexcept { return mediaSurface; }
	[[nodiscard]] const Grid* GetMediaSurface() const noexcept { return mediaSurface; }
	[[nodiscard]] MediaElement* GetMediaElement() noexcept { return mediaElement; }
	[[nodiscard]] const MediaElement* GetMediaElement() const noexcept { return mediaElement; }
	[[nodiscard]] Button* GetMediaOpen() noexcept { return mediaOpen; }
	[[nodiscard]] const Button* GetMediaOpen() const noexcept { return mediaOpen; }
	[[nodiscard]] Button* GetMediaPlay() noexcept { return mediaPlay; }
	[[nodiscard]] const Button* GetMediaPlay() const noexcept { return mediaPlay; }
	[[nodiscard]] Button* GetMediaPause() noexcept { return mediaPause; }
	[[nodiscard]] const Button* GetMediaPause() const noexcept { return mediaPause; }
	[[nodiscard]] Button* GetMediaStop() noexcept { return mediaStop; }
	[[nodiscard]] const Button* GetMediaStop() const noexcept { return mediaStop; }
	[[nodiscard]] Label* GetVolumeLabel() noexcept { return volumeLabel; }
	[[nodiscard]] const Label* GetVolumeLabel() const noexcept { return volumeLabel; }
	[[nodiscard]] Slider* GetMediaVolume() noexcept { return mediaVolume; }
	[[nodiscard]] const Slider* GetMediaVolume() const noexcept { return mediaVolume; }
	[[nodiscard]] Label* GetSpeedTitle() noexcept { return speedTitle; }
	[[nodiscard]] const Label* GetSpeedTitle() const noexcept { return speedTitle; }
	[[nodiscard]] Slider* GetMediaSpeed() noexcept { return mediaSpeed; }
	[[nodiscard]] const Slider* GetMediaSpeed() const noexcept { return mediaSpeed; }
	[[nodiscard]] Label* GetMediaSpeedText() noexcept { return mediaSpeedText; }
	[[nodiscard]] const Label* GetMediaSpeedText() const noexcept { return mediaSpeedText; }
	[[nodiscard]] CheckBox* GetMediaLoop() noexcept { return mediaLoop; }
	[[nodiscard]] const CheckBox* GetMediaLoop() const noexcept { return mediaLoop; }
	[[nodiscard]] Slider* GetMediaProgress() noexcept { return mediaProgress; }
	[[nodiscard]] const Slider* GetMediaProgress() const noexcept { return mediaProgress; }
	[[nodiscard]] Label* GetMediaTime() noexcept { return mediaTime; }
	[[nodiscard]] const Label* GetMediaTime() const noexcept { return mediaTime; }
	[[nodiscard]] Grid* GetWpfLabSurface() noexcept { return wpfLabSurface; }
	[[nodiscard]] const Grid* GetWpfLabSurface() const noexcept { return wpfLabSurface; }
	[[nodiscard]] Label* GetWpfLabTitle() noexcept { return wpfLabTitle; }
	[[nodiscard]] const Label* GetWpfLabTitle() const noexcept { return wpfLabTitle; }
	[[nodiscard]] ContentControl* GetWpfBindingScope() noexcept { return wpfBindingScope; }
	[[nodiscard]] const ContentControl* GetWpfBindingScope() const noexcept { return wpfBindingScope; }
	[[nodiscard]] Label* GetWpfTypographyOverride() noexcept { return wpfTypographyOverride; }
	[[nodiscard]] const Label* GetWpfTypographyOverride() const noexcept { return wpfTypographyOverride; }
	[[nodiscard]] TextBox* GetWpfTwoWayEditor() noexcept { return wpfTwoWayEditor; }
	[[nodiscard]] const TextBox* GetWpfTwoWayEditor() const noexcept { return wpfTwoWayEditor; }
	[[nodiscard]] Label* GetWpfElementMirror() noexcept { return wpfElementMirror; }
	[[nodiscard]] const Label* GetWpfElementMirror() const noexcept { return wpfElementMirror; }
	[[nodiscard]] Label* GetWpfSelfValue() noexcept { return wpfSelfValue; }
	[[nodiscard]] const Label* GetWpfSelfValue() const noexcept { return wpfSelfValue; }
	[[nodiscard]] Label* GetWpfAncestorValue() noexcept { return wpfAncestorValue; }
	[[nodiscard]] const Label* GetWpfAncestorValue() const noexcept { return wpfAncestorValue; }
	[[nodiscard]] Label* GetWpfFallbackValue() noexcept { return wpfFallbackValue; }
	[[nodiscard]] const Label* GetWpfFallbackValue() const noexcept { return wpfFallbackValue; }
	[[nodiscard]] Label* GetWpfNullValue() noexcept { return wpfNullValue; }
	[[nodiscard]] const Label* GetWpfNullValue() const noexcept { return wpfNullValue; }
	[[nodiscard]] Label* GetWpfIndexerValue() noexcept { return wpfIndexerValue; }
	[[nodiscard]] const Label* GetWpfIndexerValue() const noexcept { return wpfIndexerValue; }
	[[nodiscard]] Label* GetWpfKeyedIndexerValue() noexcept { return wpfKeyedIndexerValue; }
	[[nodiscard]] const Label* GetWpfKeyedIndexerValue() const noexcept { return wpfKeyedIndexerValue; }
	[[nodiscard]] Label* GetWpfConvertedValue() noexcept { return wpfConvertedValue; }
	[[nodiscard]] const Label* GetWpfConvertedValue() const noexcept { return wpfConvertedValue; }
	[[nodiscard]] Label* GetWpfMultiValue() noexcept { return wpfMultiValue; }
	[[nodiscard]] const Label* GetWpfMultiValue() const noexcept { return wpfMultiValue; }
	[[nodiscard]] StackPanel* GetWpfTemplateAndStyleScope() noexcept { return wpfTemplateAndStyleScope; }
	[[nodiscard]] const StackPanel* GetWpfTemplateAndStyleScope() const noexcept { return wpfTemplateAndStyleScope; }
	[[nodiscard]] Button* GetWpfTemplateButton() noexcept { return wpfTemplateButton; }
	[[nodiscard]] const Button* GetWpfTemplateButton() const noexcept { return wpfTemplateButton; }
	[[nodiscard]] Button* GetWpfTriggerButton() noexcept { return wpfTriggerButton; }
	[[nodiscard]] const Button* GetWpfTriggerButton() const noexcept { return wpfTriggerButton; }
	[[nodiscard]] Label* GetWpfScopeResourceValue() noexcept { return wpfScopeResourceValue; }
	[[nodiscard]] const Label* GetWpfScopeResourceValue() const noexcept { return wpfScopeResourceValue; }
	[[nodiscard]] StackPanel* GetWpfInnerResourceScope() noexcept { return wpfInnerResourceScope; }
	[[nodiscard]] const StackPanel* GetWpfInnerResourceScope() const noexcept { return wpfInnerResourceScope; }
	[[nodiscard]] Label* GetWpfInnerResourceValue() noexcept { return wpfInnerResourceValue; }
	[[nodiscard]] const Label* GetWpfInnerResourceValue() const noexcept { return wpfInnerResourceValue; }
	[[nodiscard]] Grid* GetWpfItemsScope() noexcept { return wpfItemsScope; }
	[[nodiscard]] const Grid* GetWpfItemsScope() const noexcept { return wpfItemsScope; }
	[[nodiscard]] ListBox* GetWpfTemplateList() noexcept { return wpfTemplateList; }
	[[nodiscard]] const ListBox* GetWpfTemplateList() const noexcept { return wpfTemplateList; }
	[[nodiscard]] Border* GetWpfRouteOuter() noexcept { return wpfRouteOuter; }
	[[nodiscard]] const Border* GetWpfRouteOuter() const noexcept { return wpfRouteOuter; }
	[[nodiscard]] Grid* GetWpfRouteMiddle() noexcept { return wpfRouteMiddle; }
	[[nodiscard]] const Grid* GetWpfRouteMiddle() const noexcept { return wpfRouteMiddle; }
	[[nodiscard]] Button* GetWpfRouteSource() noexcept { return wpfRouteSource; }
	[[nodiscard]] const Button* GetWpfRouteSource() const noexcept { return wpfRouteSource; }
	[[nodiscard]] Button* GetWpfFocusPeerB() noexcept { return wpfFocusPeerB; }
	[[nodiscard]] const Button* GetWpfFocusPeerB() const noexcept { return wpfFocusPeerB; }
	[[nodiscard]] Button* GetWpfFocusPeerC() noexcept { return wpfFocusPeerC; }
	[[nodiscard]] const Button* GetWpfFocusPeerC() const noexcept { return wpfFocusPeerC; }
	[[nodiscard]] Button* GetWpfNoFocusPeer() noexcept { return wpfNoFocusPeer; }
	[[nodiscard]] const Button* GetWpfNoFocusPeer() const noexcept { return wpfNoFocusPeer; }
	[[nodiscard]] TextBox* GetWpfTextInputSource() noexcept { return wpfTextInputSource; }
	[[nodiscard]] const TextBox* GetWpfTextInputSource() const noexcept { return wpfTextInputSource; }
	[[nodiscard]] Label* GetWpfRouteTrace() noexcept { return wpfRouteTrace; }
	[[nodiscard]] const Label* GetWpfRouteTrace() const noexcept { return wpfRouteTrace; }
	[[nodiscard]] Label* GetWpfInputStats() noexcept { return wpfInputStats; }
	[[nodiscard]] const Label* GetWpfInputStats() const noexcept { return wpfInputStats; }
	[[nodiscard]] Grid* GetWpfHierarchyScope() noexcept { return wpfHierarchyScope; }
	[[nodiscard]] const Grid* GetWpfHierarchyScope() const noexcept { return wpfHierarchyScope; }
	[[nodiscard]] Label* GetWpfHierarchyChain() noexcept { return wpfHierarchyChain; }
	[[nodiscard]] const Label* GetWpfHierarchyChain() const noexcept { return wpfHierarchyChain; }
	[[nodiscard]] Button* GetWpfDispatcherProbe() noexcept { return wpfDispatcherProbe; }
	[[nodiscard]] const Button* GetWpfDispatcherProbe() const noexcept { return wpfDispatcherProbe; }
	[[nodiscard]] Label* GetWpfDispatcherResult() noexcept { return wpfDispatcherResult; }
	[[nodiscard]] const Label* GetWpfDispatcherResult() const noexcept { return wpfDispatcherResult; }
	[[nodiscard]] Grid* GetTextCompositionLabSurface() noexcept { return textCompositionLabSurface; }
	[[nodiscard]] const Grid* GetTextCompositionLabSurface() const noexcept { return textCompositionLabSurface; }
	[[nodiscard]] TextBox* GetCompositionTextBox() noexcept { return compositionTextBox; }
	[[nodiscard]] const TextBox* GetCompositionTextBox() const noexcept { return compositionTextBox; }
	[[nodiscard]] RichTextBox* GetCompositionRichTextBox() noexcept { return compositionRichTextBox; }
	[[nodiscard]] const RichTextBox* GetCompositionRichTextBox() const noexcept { return compositionRichTextBox; }
	[[nodiscard]] PasswordBox* GetCompositionPasswordBox() noexcept { return compositionPasswordBox; }
	[[nodiscard]] const PasswordBox* GetCompositionPasswordBox() const noexcept { return compositionPasswordBox; }
	[[nodiscard]] Button* GetCompositionStartProbe() noexcept { return compositionStartProbe; }
	[[nodiscard]] const Button* GetCompositionStartProbe() const noexcept { return compositionStartProbe; }
	[[nodiscard]] Button* GetCompositionUpdateProbe() noexcept { return compositionUpdateProbe; }
	[[nodiscard]] const Button* GetCompositionUpdateProbe() const noexcept { return compositionUpdateProbe; }
	[[nodiscard]] Button* GetCompositionCommitProbe() noexcept { return compositionCommitProbe; }
	[[nodiscard]] const Button* GetCompositionCommitProbe() const noexcept { return compositionCommitProbe; }
	[[nodiscard]] Button* GetCompositionCancelProbe() noexcept { return compositionCancelProbe; }
	[[nodiscard]] const Button* GetCompositionCancelProbe() const noexcept { return compositionCancelProbe; }
	[[nodiscard]] Button* GetCompositionSurrogateProbe() noexcept { return compositionSurrogateProbe; }
	[[nodiscard]] const Button* GetCompositionSurrogateProbe() const noexcept { return compositionSurrogateProbe; }
	[[nodiscard]] Button* GetCompositionUnicharProbe() noexcept { return compositionUnicharProbe; }
	[[nodiscard]] const Button* GetCompositionUnicharProbe() const noexcept { return compositionUnicharProbe; }
	[[nodiscard]] Button* GetCompositionFocusProbe() noexcept { return compositionFocusProbe; }
	[[nodiscard]] const Button* GetCompositionFocusProbe() const noexcept { return compositionFocusProbe; }
	[[nodiscard]] Button* GetCompositionPreviewHandledProbe() noexcept { return compositionPreviewHandledProbe; }
	[[nodiscard]] const Button* GetCompositionPreviewHandledProbe() const noexcept { return compositionPreviewHandledProbe; }
	[[nodiscard]] Button* GetCompositionResetProbe() noexcept { return compositionResetProbe; }
	[[nodiscard]] const Button* GetCompositionResetProbe() const noexcept { return compositionResetProbe; }
	[[nodiscard]] Label* GetCompositionState() noexcept { return compositionState; }
	[[nodiscard]] const Label* GetCompositionState() const noexcept { return compositionState; }
	[[nodiscard]] Label* GetCompositionStats() noexcept { return compositionStats; }
	[[nodiscard]] const Label* GetCompositionStats() const noexcept { return compositionStats; }
	[[nodiscard]] Label* GetCompositionTrace() noexcept { return compositionTrace; }
	[[nodiscard]] const Label* GetCompositionTrace() const noexcept { return compositionTrace; }
	[[nodiscard]] Grid* GetPresentationLabSurface() noexcept { return presentationLabSurface; }
	[[nodiscard]] const Grid* GetPresentationLabSurface() const noexcept { return presentationLabSurface; }
	[[nodiscard]] NativeSurface* GetPresentationProbeSurface() noexcept { return presentationProbeSurface; }
	[[nodiscard]] const NativeSurface* GetPresentationProbeSurface() const noexcept { return presentationProbeSurface; }
	[[nodiscard]] Label* GetPresentationTopologyTile() noexcept { return presentationTopologyTile; }
	[[nodiscard]] const Label* GetPresentationTopologyTile() const noexcept { return presentationTopologyTile; }
	[[nodiscard]] Button* GetPresentationRegionButton() noexcept { return presentationRegionButton; }
	[[nodiscard]] const Button* GetPresentationRegionButton() const noexcept { return presentationRegionButton; }
	[[nodiscard]] Button* GetPresentationGeometryButton() noexcept { return presentationGeometryButton; }
	[[nodiscard]] const Button* GetPresentationGeometryButton() const noexcept { return presentationGeometryButton; }
	[[nodiscard]] Button* GetPresentationCompositionButton() noexcept { return presentationCompositionButton; }
	[[nodiscard]] const Button* GetPresentationCompositionButton() const noexcept { return presentationCompositionButton; }
	[[nodiscard]] Button* GetPresentationFullButton() noexcept { return presentationFullButton; }
	[[nodiscard]] const Button* GetPresentationFullButton() const noexcept { return presentationFullButton; }
	[[nodiscard]] Button* GetPresentationTopologyButton() noexcept { return presentationTopologyButton; }
	[[nodiscard]] const Button* GetPresentationTopologyButton() const noexcept { return presentationTopologyButton; }
	[[nodiscard]] Button* GetPresentationDeviceLossButton() noexcept { return presentationDeviceLossButton; }
	[[nodiscard]] const Button* GetPresentationDeviceLossButton() const noexcept { return presentationDeviceLossButton; }
	[[nodiscard]] Label* GetPresentationStatus() noexcept { return presentationStatus; }
	[[nodiscard]] const Label* GetPresentationStatus() const noexcept { return presentationStatus; }
	[[nodiscard]] ContextMenu* GetSystemContextMenu() noexcept { return systemContextMenu; }
	[[nodiscard]] const ContextMenu* GetSystemContextMenu() const noexcept { return systemContextMenu; }
	[[nodiscard]] StatusBar* GetMainStatusBar() noexcept { return mainStatusBar; }
	[[nodiscard]] const StatusBar* GetMainStatusBar() const noexcept { return mainStatusBar; }

	DemoWindowGenerated();
	virtual ~DemoWindowGenerated();
	bool BindData(BindingSourceReference dataContext);
};
