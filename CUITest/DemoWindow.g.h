#pragma once
#include "Binding.h"
#include "BindingList.h"
#include "Border.h"
#include "Button.h"
#include "Canvas.h"
#include "ChartView.h"
#include "CheckBox.h"
#include "CollectionViewSource.h"
#include "ComboBox.h"
#include "ContentControl.h"
#include "ContextMenu.h"
#include "Control.h"
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
	[[nodiscard]] StackPanel* GetstackPanel1() noexcept { return _part_stackPanel1; }
	[[nodiscard]] const StackPanel* GetstackPanel1() const noexcept { return _part_stackPanel1; }
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
	Grid* containerSurface = nullptr;
	Label* textBlock7 = nullptr;
	StackPanel* stackPanel10 = nullptr;
	Button* openImageButton = nullptr;
	Border* border4 = nullptr;
	Image* demoImage = nullptr;
	Label* textBlock8 = nullptr;
	ProgressBar* demoProgress = nullptr;
	Label* textBlock9 = nullptr;
	ProgressBar* indeterminateProgress = nullptr;
	StackPanel* stackPanel11 = nullptr;
	Label* textBlock10 = nullptr;
	WrapPanel* wrapPanel1 = nullptr;
	LoadingRing* loadingRing = nullptr;
	ProgressRing* progressRing = nullptr;
	StackPanel* stackPanel12 = nullptr;
	Switch* imageVisible = nullptr;
	Label* imageVisibleLabel = nullptr;
	NativeSurface* demoScene = nullptr;
	Label* textBlock11 = nullptr;
	Grid* grid3 = nullptr;
	Grid* detailGrid = nullptr;
	StackPanel* navigationComposition = nullptr;
	Label* textBlock12 = nullptr;
	ListBox* sideNavigationList = nullptr;
	Border* border5 = nullptr;
	StackPanel* detailComposition = nullptr;
	StackPanel* stackPanel13 = nullptr;
	Label* textBlock13 = nullptr;
	Label* textBlock14 = nullptr;
	Label* textBlock15 = nullptr;
	Label* textBlock16 = nullptr;
	Label* textBlock17 = nullptr;
	RichTextBox* splitNotes = nullptr;
	GroupBox* containerGroup = nullptr;
	Label* containerGroupText = nullptr;
	TabItem* tabItem3 = nullptr;
	Border* border6 = nullptr;
	Grid* dataSurface = nullptr;
	Label* textBlock18 = nullptr;
	TreeView* demoTree = nullptr;
	ListBox* demoListBox = nullptr;
	ListView* demoList = nullptr;
	GroupBox* composedPropertyEditor = nullptr;
	Grid* grid4 = nullptr;
	Label* textBlock19 = nullptr;
	TextBox* composedTitleEditor = nullptr;
	Label* textBlock20 = nullptr;
	CheckBox* composedEnabledEditor = nullptr;
	Label* textBlock21 = nullptr;
	ComboBox* composedDensityEditor = nullptr;
	ComboBoxItem* comboBoxItem1 = nullptr;
	ComboBoxItem* comboBoxItem2 = nullptr;
	ComboBoxItem* comboBoxItem3 = nullptr;
	Label* textBlock22 = nullptr;
	Slider* composedScaleEditor = nullptr;
	Label* textBlock23 = nullptr;
	StackPanel* stackPanel14 = nullptr;
	Label* textBlock24 = nullptr;
	TreeView* authoredStateTree = nullptr;
	TreeViewItem* treeViewItem1 = nullptr;
	TreeViewItem* treeViewItem2 = nullptr;
	TreeViewItem* treeViewItem3 = nullptr;
	TreeViewItem* treeViewItem4 = nullptr;
	Label* textBlock25 = nullptr;
	TabItem* tabItem4 = nullptr;
	Border* border7 = nullptr;
	Grid* analyticsSurface = nullptr;
	Label* textBlock26 = nullptr;
	Border* border8 = nullptr;
	Grid* analyticsFilterSurface = nullptr;
	TextBox* analyticsQuery = nullptr;
	CheckBox* analyticsClosed = nullptr;
	CheckBox* analyticsContract = nullptr;
	CheckBox* analyticsHighMargin = nullptr;
	Button* analyticsApply = nullptr;
	Button* analyticsReset = nullptr;
	Grid* grid5 = nullptr;
	GroupBox* groupBox1 = nullptr;
	StackPanel* stackPanel15 = nullptr;
	Label* textBlock27 = nullptr;
	Label* textBlock28 = nullptr;
	GroupBox* groupBox2 = nullptr;
	StackPanel* stackPanel16 = nullptr;
	Label* textBlock29 = nullptr;
	ProgressBar* progressBar1 = nullptr;
	GroupBox* groupBox3 = nullptr;
	StackPanel* stackPanel17 = nullptr;
	Label* textBlock30 = nullptr;
	Label* textBlock31 = nullptr;
	Label* textBlock32 = nullptr;
	WrapPanel* wrapPanel2 = nullptr;
	Button* chartBar = nullptr;
	Button* chartPie = nullptr;
	Button* chartLine = nullptr;
	Grid* grid6 = nullptr;
	ChartView* salesChart = nullptr;
	GroupBox* analyticsReport = nullptr;
	Grid* grid7 = nullptr;
	StackPanel* stackPanel18 = nullptr;
	Label* textBlock33 = nullptr;
	Label* textBlock34 = nullptr;
	Label* textBlock35 = nullptr;
	Label* textBlock36 = nullptr;
	Label* textBlock37 = nullptr;
	ListView* analyticsRows = nullptr;
	Label* textBlock38 = nullptr;
	TabItem* tabItem5 = nullptr;
	Border* border9 = nullptr;
	Grid* layoutSurface = nullptr;
	Label* layoutTitle = nullptr;
	Canvas* canvasSemanticsProbe = nullptr;
	Border* border10 = nullptr;
	Label* canvasLeftWins = nullptr;
	Label* canvasRightBottom = nullptr;
	Border* border11 = nullptr;
	StackPanel* demoStack = nullptr;
	Label* textBlock39 = nullptr;
	Button* stackA = nullptr;
	Button* stackB = nullptr;
	Button* stackC = nullptr;
	Border* border12 = nullptr;
	Grid* demoGrid = nullptr;
	Button* gridHeader = nullptr;
	Label* gridLeft = nullptr;
	TextBox* gridEditor = nullptr;
	Button* gridFooter = nullptr;
	Border* border13 = nullptr;
	DockPanel* demoDock = nullptr;
	Label* textBlock40 = nullptr;
	Button* dockTop = nullptr;
	Button* dockLeft = nullptr;
	Label* dockFill = nullptr;
	Border* border14 = nullptr;
	WrapPanel* demoWrap = nullptr;
	Button* wrap1 = nullptr;
	Button* wrap2 = nullptr;
	Button* wrap3 = nullptr;
	Button* wrap4 = nullptr;
	Button* wrap5 = nullptr;
	Button* wrap6 = nullptr;
	Border* border15 = nullptr;
	RelativePanel* demoRelative = nullptr;
	StackPanel* relativeCenter = nullptr;
	Label* naturalTextProbe = nullptr;
	Label* wrappedTextProbe = nullptr;
	Label* trimmedTextProbe = nullptr;
	Button* relativeCenterButton = nullptr;
	Border* border16 = nullptr;
	ScrollViewer* demoScroll = nullptr;
	Grid* demoScrollContent = nullptr;
	Border* border17 = nullptr;
	StackPanel* scrollCard1 = nullptr;
	Label* scrollCard1Text = nullptr;
	Label* textBlock41 = nullptr;
	Border* border18 = nullptr;
	StackPanel* scrollCard2 = nullptr;
	Label* scrollCard2Text = nullptr;
	Label* textBlock42 = nullptr;
	Label* textBlock43 = nullptr;
	Button* farButton = nullptr;
	TabItem* tabItem6 = nullptr;
	Border* border19 = nullptr;
	Grid* systemSurface = nullptr;
	Label* systemTitle = nullptr;
	StackPanel* stackPanel19 = nullptr;
	WrapPanel* wrapPanel3 = nullptr;
	Button* notifyToggle = nullptr;
	Button* notifyBalloon = nullptr;
	Button* showDialog = nullptr;
	Button* showToast = nullptr;
	Label* systemHint = nullptr;
	Border* border20 = nullptr;
	Grid* grid8 = nullptr;
	Label* textBlock44 = nullptr;
	Button* commandTargetButton = nullptr;
	Label* textBlock45 = nullptr;
	Label* commandTargetTrace = nullptr;
	Label* textBlock46 = nullptr;
	Label* textBlock47 = nullptr;
	Label* textBlock48 = nullptr;
	GroupBox* notificationPanel = nullptr;
	Grid* grid9 = nullptr;
	Label* textBlock49 = nullptr;
	Label* toastMessage = nullptr;
	ProgressBar* progressBar2 = nullptr;
	Button* dismissToast = nullptr;
	Label* textBlock50 = nullptr;
	TabItem* tabItem7 = nullptr;
	Border* border21 = nullptr;
	Grid* webSurface = nullptr;
	Grid* grid10 = nullptr;
	Button* invokeWeb = nullptr;
	Label* webHint = nullptr;
	Border* border22 = nullptr;
	WebBrowser* webBrowser = nullptr;
	TabItem* tabItem8 = nullptr;
	Border* border23 = nullptr;
	Grid* mediaSurface = nullptr;
	MediaElement* mediaElement = nullptr;
	Grid* grid11 = nullptr;
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
	Grid* grid12 = nullptr;
	Slider* mediaProgress = nullptr;
	Label* mediaTime = nullptr;
	TabItem* tabItem9 = nullptr;
	Border* border24 = nullptr;
	Grid* wpfLabSurface = nullptr;
	Label* wpfLabTitle = nullptr;
	ContentControl* wpfBindingScope = nullptr;
	StackPanel* stackPanel20 = nullptr;
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
	Label* textBlock51 = nullptr;
	Button* wpfTemplateButton = nullptr;
	Button* wpfTriggerButton = nullptr;
	Label* wpfScopeResourceValue = nullptr;
	StackPanel* wpfInnerResourceScope = nullptr;
	Label* wpfInnerResourceValue = nullptr;
	Label* textBlock52 = nullptr;
	Grid* wpfItemsScope = nullptr;
	Label* textBlock53 = nullptr;
	ListBox* wpfTemplateList = nullptr;
	Border* wpfRouteOuter = nullptr;
	Grid* grid13 = nullptr;
	Label* textBlock54 = nullptr;
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
	TabItem* tabItem10 = nullptr;
	Border* border25 = nullptr;
	Grid* textCompositionLabSurface = nullptr;
	Label* textBlock55 = nullptr;
	Border* border26 = nullptr;
	Grid* grid14 = nullptr;
	Label* textBlock56 = nullptr;
	Label* textBlock57 = nullptr;
	TextBox* compositionTextBox = nullptr;
	Label* textBlock58 = nullptr;
	RichTextBox* compositionRichTextBox = nullptr;
	Label* textBlock59 = nullptr;
	PasswordBox* compositionPasswordBox = nullptr;
	Label* textBlock60 = nullptr;
	Border* border27 = nullptr;
	Grid* grid15 = nullptr;
	Label* textBlock61 = nullptr;
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
	Border* border28 = nullptr;
	Grid* grid16 = nullptr;
	Label* textBlock62 = nullptr;
	Label* compositionTrace = nullptr;
	TabItem* tabItem11 = nullptr;
	Border* border29 = nullptr;
	Grid* presentationLabSurface = nullptr;
	Label* textBlock63 = nullptr;
	Grid* grid17 = nullptr;
	NativeSurface* presentationProbeSurface = nullptr;
	Canvas* canvas1 = nullptr;
	Label* presentationTopologyTile = nullptr;
	StackPanel* stackPanel21 = nullptr;
	Label* textBlock64 = nullptr;
	Label* textBlock65 = nullptr;
	Label* textBlock66 = nullptr;
	Label* textBlock67 = nullptr;
	Label* textBlock68 = nullptr;
	Label* textBlock69 = nullptr;
	Grid* grid18 = nullptr;
	WrapPanel* wrapPanel5 = nullptr;
	Button* presentationRegionButton = nullptr;
	Button* presentationGeometryButton = nullptr;
	Button* presentationCompositionButton = nullptr;
	Button* presentationFullButton = nullptr;
	Button* presentationTopologyButton = nullptr;
	Button* presentationDeviceLossButton = nullptr;
	Label* presentationStatus = nullptr;
	Label* textBlock70 = nullptr;
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

	// Stable identities shared by static and dynamic document paths.
	struct ControlIds final
	{
		static constexpr int windowContent = 6;
		static constexpr int mainMenu = 1;
		static constexpr int menuItem1 = 7;
		static constexpr int menuItem2 = 8;
		static constexpr int separator1 = 9;
		static constexpr int menuItem3 = 11;
		static constexpr int menuItem4 = 12;
		static constexpr int menuItem5 = 13;
		static constexpr int mainToolBar = 2;
		static constexpr int toolBasic = 20;
		static constexpr int toolData = 21;
		static constexpr int toolAnalytics = 22;
		static constexpr int toolSystem = 23;
		static constexpr int toolSeparator = 24;
		static constexpr int toolIcon1 = 25;
		static constexpr int toolIconImage1 = 28;
		static constexpr int toolIcon2 = 26;
		static constexpr int toolIconImage2 = 29;
		static constexpr int toolIcon3 = 27;
		static constexpr int toolIconImage3 = 30;
		static constexpr int border1 = 31;
		static constexpr int grid1 = 32;
		static constexpr int globalProgress = 3;
		static constexpr int statusText = 4;
		static constexpr int runtimeBadge = 5;
		static constexpr int mainTabs = 10;
		static constexpr int tabItem1 = 33;
		static constexpr int border2 = 34;
		static constexpr int basicSurface = 100;
		static constexpr int basicTitle = 101;
		static constexpr int frameworkThemeHint = 124;
		static constexpr int grid2 = 129;
		static constexpr int stackPanel1 = 130;
		static constexpr int textBlock1 = 131;
		static constexpr int stackPanel2 = 132;
		static constexpr int basicButton = 102;
		static constexpr int enableInput = 103;
		static constexpr int stackPanel3 = 133;
		static constexpr int radioA = 104;
		static constexpr int radioB = 105;
		static constexpr int textBlock2 = 134;
		static constexpr int nameInput = 106;
		static constexpr int passwordInput = 107;
		static constexpr int basicCombo = 108;
		static constexpr int dateInput = 109;
		static constexpr int stackPanel4 = 135;
		static constexpr int numberInput = 110;
		static constexpr int dialogCancelButton = 123;
		static constexpr int docsLink = 111;
		static constexpr int stackPanel5 = 136;
		static constexpr int textBlock3 = 137;
		static constexpr int stackPanel6 = 138;
		static constexpr int verticalThemeSlider = 127;
		static constexpr int verticalThemeProgress = 128;
		static constexpr int stackPanel7 = 139;
		static constexpr int textBlock4 = 140;
		static constexpr int gradientInput = 112;
		static constexpr int gradientLabel = 113;
		static constexpr int featureCard = 122;
		static constexpr int featureCardContent = 141;
		static constexpr int featureActionA = 142;
		static constexpr int featureActionB = 143;
		static constexpr int stackPanel8 = 144;
		static constexpr int textBlock5 = 145;
		static constexpr int basicGroup = 114;
		static constexpr int basicGroupContent = 120;
		static constexpr int groupHint = 115;
		static constexpr int groupName = 116;
		static constexpr int groupEnabled = 117;
		static constexpr int stackPanel9 = 146;
		static constexpr int themeNormalButton = 125;
		static constexpr int themeDisabledButton = 126;
		static constexpr int basicExpander = 118;
		static constexpr int basicExpanderContent = 121;
		static constexpr int expanderText = 119;
		static constexpr int themeContentControlProbe = 147;
		static constexpr int themeItemsControlProbe = 148;
		static constexpr int textBlock6 = 149;
		static constexpr int themeSeparatorProbe = 150;
		static constexpr int tabItem2 = 151;
		static constexpr int border3 = 152;
		static constexpr int containerSurface = 200;
		static constexpr int textBlock7 = 216;
		static constexpr int stackPanel10 = 217;
		static constexpr int openImageButton = 201;
		static constexpr int border4 = 218;
		static constexpr int demoImage = 202;
		static constexpr int textBlock8 = 219;
		static constexpr int demoProgress = 203;
		static constexpr int textBlock9 = 220;
		static constexpr int indeterminateProgress = 215;
		static constexpr int stackPanel11 = 221;
		static constexpr int textBlock10 = 222;
		static constexpr int wrapPanel1 = 223;
		static constexpr int loadingRing = 204;
		static constexpr int progressRing = 205;
		static constexpr int stackPanel12 = 224;
		static constexpr int imageVisible = 206;
		static constexpr int imageVisibleLabel = 207;
		static constexpr int demoScene = 214;
		static constexpr int textBlock11 = 225;
		static constexpr int grid3 = 226;
		static constexpr int detailGrid = 208;
		static constexpr int navigationComposition = 209;
		static constexpr int textBlock12 = 227;
		static constexpr int sideNavigationList = 228;
		static constexpr int border5 = 229;
		static constexpr int detailComposition = 210;
		static constexpr int stackPanel13 = 230;
		static constexpr int textBlock13 = 231;
		static constexpr int textBlock14 = 232;
		static constexpr int textBlock15 = 233;
		static constexpr int textBlock16 = 234;
		static constexpr int textBlock17 = 235;
		static constexpr int splitNotes = 211;
		static constexpr int containerGroup = 212;
		static constexpr int containerGroupText = 213;
		static constexpr int tabItem3 = 236;
		static constexpr int border6 = 237;
		static constexpr int dataSurface = 300;
		static constexpr int textBlock18 = 304;
		static constexpr int demoTree = 301;
		static constexpr int demoListBox = 302;
		static constexpr int demoList = 303;
		static constexpr int composedPropertyEditor = 309;
		static constexpr int grid4 = 310;
		static constexpr int textBlock19 = 311;
		static constexpr int composedTitleEditor = 312;
		static constexpr int textBlock20 = 313;
		static constexpr int composedEnabledEditor = 314;
		static constexpr int textBlock21 = 315;
		static constexpr int composedDensityEditor = 316;
		static constexpr int comboBoxItem1 = 317;
		static constexpr int comboBoxItem2 = 318;
		static constexpr int comboBoxItem3 = 319;
		static constexpr int textBlock22 = 320;
		static constexpr int composedScaleEditor = 321;
		static constexpr int textBlock23 = 322;
		static constexpr int stackPanel14 = 323;
		static constexpr int textBlock24 = 324;
		static constexpr int authoredStateTree = 325;
		static constexpr int treeViewItem1 = 326;
		static constexpr int treeViewItem2 = 327;
		static constexpr int treeViewItem3 = 328;
		static constexpr int treeViewItem4 = 329;
		static constexpr int textBlock25 = 330;
		static constexpr int tabItem4 = 331;
		static constexpr int border7 = 332;
		static constexpr int analyticsSurface = 400;
		static constexpr int textBlock26 = 410;
		static constexpr int border8 = 411;
		static constexpr int analyticsFilterSurface = 401;
		static constexpr int analyticsQuery = 412;
		static constexpr int analyticsClosed = 413;
		static constexpr int analyticsContract = 414;
		static constexpr int analyticsHighMargin = 415;
		static constexpr int analyticsApply = 416;
		static constexpr int analyticsReset = 417;
		static constexpr int grid5 = 418;
		static constexpr int groupBox1 = 402;
		static constexpr int stackPanel15 = 419;
		static constexpr int textBlock27 = 420;
		static constexpr int textBlock28 = 421;
		static constexpr int groupBox2 = 403;
		static constexpr int stackPanel16 = 422;
		static constexpr int textBlock29 = 423;
		static constexpr int progressBar1 = 424;
		static constexpr int groupBox3 = 404;
		static constexpr int stackPanel17 = 425;
		static constexpr int textBlock30 = 426;
		static constexpr int textBlock31 = 427;
		static constexpr int textBlock32 = 428;
		static constexpr int wrapPanel2 = 429;
		static constexpr int chartBar = 405;
		static constexpr int chartPie = 406;
		static constexpr int chartLine = 407;
		static constexpr int grid6 = 430;
		static constexpr int salesChart = 408;
		static constexpr int analyticsReport = 409;
		static constexpr int grid7 = 431;
		static constexpr int stackPanel18 = 432;
		static constexpr int textBlock33 = 433;
		static constexpr int textBlock34 = 434;
		static constexpr int textBlock35 = 435;
		static constexpr int textBlock36 = 436;
		static constexpr int textBlock37 = 437;
		static constexpr int analyticsRows = 438;
		static constexpr int textBlock38 = 439;
		static constexpr int tabItem5 = 440;
		static constexpr int border9 = 441;
		static constexpr int layoutSurface = 500;
		static constexpr int layoutTitle = 501;
		static constexpr int canvasSemanticsProbe = 530;
		static constexpr int border10 = 533;
		static constexpr int canvasLeftWins = 531;
		static constexpr int canvasRightBottom = 532;
		static constexpr int border11 = 534;
		static constexpr int demoStack = 502;
		static constexpr int textBlock39 = 535;
		static constexpr int stackA = 503;
		static constexpr int stackB = 504;
		static constexpr int stackC = 505;
		static constexpr int border12 = 536;
		static constexpr int demoGrid = 506;
		static constexpr int gridHeader = 507;
		static constexpr int gridLeft = 508;
		static constexpr int gridEditor = 509;
		static constexpr int gridFooter = 510;
		static constexpr int border13 = 537;
		static constexpr int demoDock = 511;
		static constexpr int textBlock40 = 538;
		static constexpr int dockTop = 512;
		static constexpr int dockLeft = 513;
		static constexpr int dockFill = 514;
		static constexpr int border14 = 539;
		static constexpr int demoWrap = 515;
		static constexpr int wrap1 = 516;
		static constexpr int wrap2 = 517;
		static constexpr int wrap3 = 518;
		static constexpr int wrap4 = 519;
		static constexpr int wrap5 = 520;
		static constexpr int wrap6 = 521;
		static constexpr int border15 = 540;
		static constexpr int demoRelative = 522;
		static constexpr int relativeCenter = 523;
		static constexpr int naturalTextProbe = 541;
		static constexpr int wrappedTextProbe = 542;
		static constexpr int trimmedTextProbe = 543;
		static constexpr int relativeCenterButton = 544;
		static constexpr int border16 = 545;
		static constexpr int demoScroll = 524;
		static constexpr int demoScrollContent = 546;
		static constexpr int border17 = 547;
		static constexpr int scrollCard1 = 525;
		static constexpr int scrollCard1Text = 526;
		static constexpr int textBlock41 = 548;
		static constexpr int border18 = 549;
		static constexpr int scrollCard2 = 527;
		static constexpr int scrollCard2Text = 528;
		static constexpr int textBlock42 = 550;
		static constexpr int textBlock43 = 551;
		static constexpr int farButton = 529;
		static constexpr int tabItem6 = 552;
		static constexpr int border19 = 553;
		static constexpr int systemSurface = 600;
		static constexpr int systemTitle = 601;
		static constexpr int stackPanel19 = 609;
		static constexpr int wrapPanel3 = 610;
		static constexpr int notifyToggle = 602;
		static constexpr int notifyBalloon = 603;
		static constexpr int showDialog = 604;
		static constexpr int showToast = 605;
		static constexpr int systemHint = 606;
		static constexpr int border20 = 611;
		static constexpr int grid8 = 612;
		static constexpr int textBlock44 = 613;
		static constexpr int commandTargetButton = 614;
		static constexpr int textBlock45 = 615;
		static constexpr int commandTargetTrace = 616;
		static constexpr int textBlock46 = 617;
		static constexpr int textBlock47 = 618;
		static constexpr int textBlock48 = 619;
		static constexpr int notificationPanel = 607;
		static constexpr int grid9 = 620;
		static constexpr int textBlock49 = 621;
		static constexpr int toastMessage = 622;
		static constexpr int progressBar2 = 623;
		static constexpr int dismissToast = 624;
		static constexpr int textBlock50 = 625;
		static constexpr int tabItem7 = 626;
		static constexpr int border21 = 627;
		static constexpr int webSurface = 700;
		static constexpr int grid10 = 704;
		static constexpr int invokeWeb = 701;
		static constexpr int webHint = 702;
		static constexpr int border22 = 705;
		static constexpr int webBrowser = 703;
		static constexpr int tabItem8 = 706;
		static constexpr int border23 = 707;
		static constexpr int mediaSurface = 800;
		static constexpr int mediaElement = 801;
		static constexpr int grid11 = 814;
		static constexpr int mediaOpen = 802;
		static constexpr int mediaPlay = 803;
		static constexpr int mediaPause = 804;
		static constexpr int mediaStop = 805;
		static constexpr int volumeLabel = 806;
		static constexpr int mediaVolume = 807;
		static constexpr int speedTitle = 808;
		static constexpr int mediaSpeed = 809;
		static constexpr int mediaSpeedText = 810;
		static constexpr int mediaLoop = 811;
		static constexpr int grid12 = 815;
		static constexpr int mediaProgress = 812;
		static constexpr int mediaTime = 813;
		static constexpr int tabItem9 = 816;
		static constexpr int border24 = 817;
		static constexpr int wpfLabSurface = 818;
		static constexpr int wpfLabTitle = 819;
		static constexpr int wpfBindingScope = 820;
		static constexpr int stackPanel20 = 821;
		static constexpr int wpfTypographyOverride = 822;
		static constexpr int wpfTwoWayEditor = 823;
		static constexpr int wpfElementMirror = 824;
		static constexpr int wpfSelfValue = 825;
		static constexpr int wpfAncestorValue = 826;
		static constexpr int wpfFallbackValue = 827;
		static constexpr int wpfNullValue = 828;
		static constexpr int wpfIndexerValue = 829;
		static constexpr int wpfKeyedIndexerValue = 830;
		static constexpr int wpfConvertedValue = 831;
		static constexpr int wpfMultiValue = 832;
		static constexpr int wpfTemplateAndStyleScope = 833;
		static constexpr int textBlock51 = 834;
		static constexpr int wpfTemplateButton = 835;
		static constexpr int wpfTriggerButton = 836;
		static constexpr int wpfScopeResourceValue = 837;
		static constexpr int wpfInnerResourceScope = 838;
		static constexpr int wpfInnerResourceValue = 839;
		static constexpr int textBlock52 = 840;
		static constexpr int wpfItemsScope = 841;
		static constexpr int textBlock53 = 842;
		static constexpr int wpfTemplateList = 843;
		static constexpr int wpfRouteOuter = 844;
		static constexpr int grid13 = 845;
		static constexpr int textBlock54 = 846;
		static constexpr int wpfRouteMiddle = 847;
		static constexpr int wpfRouteSource = 848;
		static constexpr int wpfFocusPeerB = 849;
		static constexpr int wpfFocusPeerC = 850;
		static constexpr int wpfNoFocusPeer = 851;
		static constexpr int wpfTextInputSource = 852;
		static constexpr int wpfRouteTrace = 853;
		static constexpr int wpfInputStats = 854;
		static constexpr int wpfHierarchyScope = 855;
		static constexpr int wpfHierarchyChain = 856;
		static constexpr int wpfDispatcherProbe = 857;
		static constexpr int wpfDispatcherResult = 858;
		static constexpr int tabItem10 = 859;
		static constexpr int border25 = 860;
		static constexpr int textCompositionLabSurface = 861;
		static constexpr int textBlock55 = 862;
		static constexpr int border26 = 863;
		static constexpr int grid14 = 864;
		static constexpr int textBlock56 = 865;
		static constexpr int textBlock57 = 866;
		static constexpr int compositionTextBox = 867;
		static constexpr int textBlock58 = 868;
		static constexpr int compositionRichTextBox = 869;
		static constexpr int textBlock59 = 870;
		static constexpr int compositionPasswordBox = 871;
		static constexpr int textBlock60 = 872;
		static constexpr int border27 = 873;
		static constexpr int grid15 = 874;
		static constexpr int textBlock61 = 875;
		static constexpr int wrapPanel4 = 876;
		static constexpr int compositionStartProbe = 877;
		static constexpr int compositionUpdateProbe = 878;
		static constexpr int compositionCommitProbe = 879;
		static constexpr int compositionCancelProbe = 880;
		static constexpr int compositionSurrogateProbe = 881;
		static constexpr int compositionUnicharProbe = 882;
		static constexpr int compositionFocusProbe = 883;
		static constexpr int compositionPreviewHandledProbe = 884;
		static constexpr int compositionResetProbe = 885;
		static constexpr int compositionState = 886;
		static constexpr int compositionStats = 887;
		static constexpr int border28 = 888;
		static constexpr int grid16 = 889;
		static constexpr int textBlock62 = 890;
		static constexpr int compositionTrace = 891;
		static constexpr int tabItem11 = 892;
		static constexpr int border29 = 893;
		static constexpr int presentationLabSurface = 894;
		static constexpr int textBlock63 = 895;
		static constexpr int grid17 = 896;
		static constexpr int presentationProbeSurface = 897;
		static constexpr int canvas1 = 898;
		static constexpr int presentationTopologyTile = 899;
		static constexpr int stackPanel21 = 901;
		static constexpr int textBlock64 = 902;
		static constexpr int textBlock65 = 903;
		static constexpr int textBlock66 = 904;
		static constexpr int textBlock67 = 905;
		static constexpr int textBlock68 = 906;
		static constexpr int textBlock69 = 907;
		static constexpr int grid18 = 908;
		static constexpr int wrapPanel5 = 909;
		static constexpr int presentationRegionButton = 910;
		static constexpr int presentationGeometryButton = 911;
		static constexpr int presentationCompositionButton = 912;
		static constexpr int presentationFullButton = 913;
		static constexpr int presentationTopologyButton = 914;
		static constexpr int presentationDeviceLossButton = 915;
		static constexpr int presentationStatus = 916;
		static constexpr int textBlock70 = 917;
		static constexpr int systemContextMenu = 608;
		static constexpr int menuItem6 = 918;
		static constexpr int menuItem7 = 919;
		static constexpr int separator2 = 920;
		static constexpr int menuItem8 = 921;
		static constexpr int menuItem9 = 922;
		static constexpr int menuItem10 = 923;
		static constexpr int mainStatusBar = 900;
	};

	// Type-safe x:Name accessors; ownership remains with the generated Window.
	[[nodiscard]] Grid* GetWindowContent() noexcept { return windowContent; }
	[[nodiscard]] const Grid* GetWindowContent() const noexcept { return windowContent; }
	[[nodiscard]] Menu* GetMainMenu() noexcept { return mainMenu; }
	[[nodiscard]] const Menu* GetMainMenu() const noexcept { return mainMenu; }
	[[nodiscard]] MenuItem* GetMenuItem1() noexcept { return menuItem1; }
	[[nodiscard]] const MenuItem* GetMenuItem1() const noexcept { return menuItem1; }
	[[nodiscard]] MenuItem* GetMenuItem2() noexcept { return menuItem2; }
	[[nodiscard]] const MenuItem* GetMenuItem2() const noexcept { return menuItem2; }
	[[nodiscard]] Separator* GetSeparator1() noexcept { return separator1; }
	[[nodiscard]] const Separator* GetSeparator1() const noexcept { return separator1; }
	[[nodiscard]] MenuItem* GetMenuItem3() noexcept { return menuItem3; }
	[[nodiscard]] const MenuItem* GetMenuItem3() const noexcept { return menuItem3; }
	[[nodiscard]] MenuItem* GetMenuItem4() noexcept { return menuItem4; }
	[[nodiscard]] const MenuItem* GetMenuItem4() const noexcept { return menuItem4; }
	[[nodiscard]] MenuItem* GetMenuItem5() noexcept { return menuItem5; }
	[[nodiscard]] const MenuItem* GetMenuItem5() const noexcept { return menuItem5; }
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
	[[nodiscard]] Border* GetBorder1() noexcept { return border1; }
	[[nodiscard]] const Border* GetBorder1() const noexcept { return border1; }
	[[nodiscard]] Grid* GetGrid1() noexcept { return grid1; }
	[[nodiscard]] const Grid* GetGrid1() const noexcept { return grid1; }
	[[nodiscard]] Slider* GetGlobalProgress() noexcept { return globalProgress; }
	[[nodiscard]] const Slider* GetGlobalProgress() const noexcept { return globalProgress; }
	[[nodiscard]] Label* GetStatusText() noexcept { return statusText; }
	[[nodiscard]] const Label* GetStatusText() const noexcept { return statusText; }
	[[nodiscard]] Label* GetRuntimeBadge() noexcept { return runtimeBadge; }
	[[nodiscard]] const Label* GetRuntimeBadge() const noexcept { return runtimeBadge; }
	[[nodiscard]] TabControl* GetMainTabs() noexcept { return mainTabs; }
	[[nodiscard]] const TabControl* GetMainTabs() const noexcept { return mainTabs; }
	[[nodiscard]] TabItem* GetTabItem1() noexcept { return tabItem1; }
	[[nodiscard]] const TabItem* GetTabItem1() const noexcept { return tabItem1; }
	[[nodiscard]] Border* GetBorder2() noexcept { return border2; }
	[[nodiscard]] const Border* GetBorder2() const noexcept { return border2; }
	[[nodiscard]] Grid* GetBasicSurface() noexcept { return basicSurface; }
	[[nodiscard]] const Grid* GetBasicSurface() const noexcept { return basicSurface; }
	[[nodiscard]] Label* GetBasicTitle() noexcept { return basicTitle; }
	[[nodiscard]] const Label* GetBasicTitle() const noexcept { return basicTitle; }
	[[nodiscard]] Label* GetFrameworkThemeHint() noexcept { return frameworkThemeHint; }
	[[nodiscard]] const Label* GetFrameworkThemeHint() const noexcept { return frameworkThemeHint; }
	[[nodiscard]] Grid* GetGrid2() noexcept { return grid2; }
	[[nodiscard]] const Grid* GetGrid2() const noexcept { return grid2; }
	[[nodiscard]] StackPanel* GetStackPanel1() noexcept { return stackPanel1; }
	[[nodiscard]] const StackPanel* GetStackPanel1() const noexcept { return stackPanel1; }
	[[nodiscard]] Label* GetTextBlock1() noexcept { return textBlock1; }
	[[nodiscard]] const Label* GetTextBlock1() const noexcept { return textBlock1; }
	[[nodiscard]] StackPanel* GetStackPanel2() noexcept { return stackPanel2; }
	[[nodiscard]] const StackPanel* GetStackPanel2() const noexcept { return stackPanel2; }
	[[nodiscard]] Button* GetBasicButton() noexcept { return basicButton; }
	[[nodiscard]] const Button* GetBasicButton() const noexcept { return basicButton; }
	[[nodiscard]] CheckBox* GetEnableInput() noexcept { return enableInput; }
	[[nodiscard]] const CheckBox* GetEnableInput() const noexcept { return enableInput; }
	[[nodiscard]] StackPanel* GetStackPanel3() noexcept { return stackPanel3; }
	[[nodiscard]] const StackPanel* GetStackPanel3() const noexcept { return stackPanel3; }
	[[nodiscard]] RadioButton* GetRadioA() noexcept { return radioA; }
	[[nodiscard]] const RadioButton* GetRadioA() const noexcept { return radioA; }
	[[nodiscard]] RadioButton* GetRadioB() noexcept { return radioB; }
	[[nodiscard]] const RadioButton* GetRadioB() const noexcept { return radioB; }
	[[nodiscard]] Label* GetTextBlock2() noexcept { return textBlock2; }
	[[nodiscard]] const Label* GetTextBlock2() const noexcept { return textBlock2; }
	[[nodiscard]] TextBox* GetNameInput() noexcept { return nameInput; }
	[[nodiscard]] const TextBox* GetNameInput() const noexcept { return nameInput; }
	[[nodiscard]] PasswordBox* GetPasswordInput() noexcept { return passwordInput; }
	[[nodiscard]] const PasswordBox* GetPasswordInput() const noexcept { return passwordInput; }
	[[nodiscard]] ComboBox* GetBasicCombo() noexcept { return basicCombo; }
	[[nodiscard]] const ComboBox* GetBasicCombo() const noexcept { return basicCombo; }
	[[nodiscard]] TextBox* GetDateInput() noexcept { return dateInput; }
	[[nodiscard]] const TextBox* GetDateInput() const noexcept { return dateInput; }
	[[nodiscard]] StackPanel* GetStackPanel4() noexcept { return stackPanel4; }
	[[nodiscard]] const StackPanel* GetStackPanel4() const noexcept { return stackPanel4; }
	[[nodiscard]] NumericUpDown* GetNumberInput() noexcept { return numberInput; }
	[[nodiscard]] const NumericUpDown* GetNumberInput() const noexcept { return numberInput; }
	[[nodiscard]] Button* GetDialogCancelButton() noexcept { return dialogCancelButton; }
	[[nodiscard]] const Button* GetDialogCancelButton() const noexcept { return dialogCancelButton; }
	[[nodiscard]] Button* GetDocsLink() noexcept { return docsLink; }
	[[nodiscard]] const Button* GetDocsLink() const noexcept { return docsLink; }
	[[nodiscard]] StackPanel* GetStackPanel5() noexcept { return stackPanel5; }
	[[nodiscard]] const StackPanel* GetStackPanel5() const noexcept { return stackPanel5; }
	[[nodiscard]] Label* GetTextBlock3() noexcept { return textBlock3; }
	[[nodiscard]] const Label* GetTextBlock3() const noexcept { return textBlock3; }
	[[nodiscard]] StackPanel* GetStackPanel6() noexcept { return stackPanel6; }
	[[nodiscard]] const StackPanel* GetStackPanel6() const noexcept { return stackPanel6; }
	[[nodiscard]] Slider* GetVerticalThemeSlider() noexcept { return verticalThemeSlider; }
	[[nodiscard]] const Slider* GetVerticalThemeSlider() const noexcept { return verticalThemeSlider; }
	[[nodiscard]] ProgressBar* GetVerticalThemeProgress() noexcept { return verticalThemeProgress; }
	[[nodiscard]] const ProgressBar* GetVerticalThemeProgress() const noexcept { return verticalThemeProgress; }
	[[nodiscard]] StackPanel* GetStackPanel7() noexcept { return stackPanel7; }
	[[nodiscard]] const StackPanel* GetStackPanel7() const noexcept { return stackPanel7; }
	[[nodiscard]] Label* GetTextBlock4() noexcept { return textBlock4; }
	[[nodiscard]] const Label* GetTextBlock4() const noexcept { return textBlock4; }
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
	[[nodiscard]] StackPanel* GetStackPanel8() noexcept { return stackPanel8; }
	[[nodiscard]] const StackPanel* GetStackPanel8() const noexcept { return stackPanel8; }
	[[nodiscard]] Label* GetTextBlock5() noexcept { return textBlock5; }
	[[nodiscard]] const Label* GetTextBlock5() const noexcept { return textBlock5; }
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
	[[nodiscard]] StackPanel* GetStackPanel9() noexcept { return stackPanel9; }
	[[nodiscard]] const StackPanel* GetStackPanel9() const noexcept { return stackPanel9; }
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
	[[nodiscard]] Label* GetTextBlock6() noexcept { return textBlock6; }
	[[nodiscard]] const Label* GetTextBlock6() const noexcept { return textBlock6; }
	[[nodiscard]] Separator* GetThemeSeparatorProbe() noexcept { return themeSeparatorProbe; }
	[[nodiscard]] const Separator* GetThemeSeparatorProbe() const noexcept { return themeSeparatorProbe; }
	[[nodiscard]] TabItem* GetTabItem2() noexcept { return tabItem2; }
	[[nodiscard]] const TabItem* GetTabItem2() const noexcept { return tabItem2; }
	[[nodiscard]] Border* GetBorder3() noexcept { return border3; }
	[[nodiscard]] const Border* GetBorder3() const noexcept { return border3; }
	[[nodiscard]] Grid* GetContainerSurface() noexcept { return containerSurface; }
	[[nodiscard]] const Grid* GetContainerSurface() const noexcept { return containerSurface; }
	[[nodiscard]] Label* GetTextBlock7() noexcept { return textBlock7; }
	[[nodiscard]] const Label* GetTextBlock7() const noexcept { return textBlock7; }
	[[nodiscard]] StackPanel* GetStackPanel10() noexcept { return stackPanel10; }
	[[nodiscard]] const StackPanel* GetStackPanel10() const noexcept { return stackPanel10; }
	[[nodiscard]] Button* GetOpenImageButton() noexcept { return openImageButton; }
	[[nodiscard]] const Button* GetOpenImageButton() const noexcept { return openImageButton; }
	[[nodiscard]] Border* GetBorder4() noexcept { return border4; }
	[[nodiscard]] const Border* GetBorder4() const noexcept { return border4; }
	[[nodiscard]] Image* GetDemoImage() noexcept { return demoImage; }
	[[nodiscard]] const Image* GetDemoImage() const noexcept { return demoImage; }
	[[nodiscard]] Label* GetTextBlock8() noexcept { return textBlock8; }
	[[nodiscard]] const Label* GetTextBlock8() const noexcept { return textBlock8; }
	[[nodiscard]] ProgressBar* GetDemoProgress() noexcept { return demoProgress; }
	[[nodiscard]] const ProgressBar* GetDemoProgress() const noexcept { return demoProgress; }
	[[nodiscard]] Label* GetTextBlock9() noexcept { return textBlock9; }
	[[nodiscard]] const Label* GetTextBlock9() const noexcept { return textBlock9; }
	[[nodiscard]] ProgressBar* GetIndeterminateProgress() noexcept { return indeterminateProgress; }
	[[nodiscard]] const ProgressBar* GetIndeterminateProgress() const noexcept { return indeterminateProgress; }
	[[nodiscard]] StackPanel* GetStackPanel11() noexcept { return stackPanel11; }
	[[nodiscard]] const StackPanel* GetStackPanel11() const noexcept { return stackPanel11; }
	[[nodiscard]] Label* GetTextBlock10() noexcept { return textBlock10; }
	[[nodiscard]] const Label* GetTextBlock10() const noexcept { return textBlock10; }
	[[nodiscard]] WrapPanel* GetWrapPanel1() noexcept { return wrapPanel1; }
	[[nodiscard]] const WrapPanel* GetWrapPanel1() const noexcept { return wrapPanel1; }
	[[nodiscard]] LoadingRing* GetLoadingRing() noexcept { return loadingRing; }
	[[nodiscard]] const LoadingRing* GetLoadingRing() const noexcept { return loadingRing; }
	[[nodiscard]] ProgressRing* GetProgressRing() noexcept { return progressRing; }
	[[nodiscard]] const ProgressRing* GetProgressRing() const noexcept { return progressRing; }
	[[nodiscard]] StackPanel* GetStackPanel12() noexcept { return stackPanel12; }
	[[nodiscard]] const StackPanel* GetStackPanel12() const noexcept { return stackPanel12; }
	[[nodiscard]] Switch* GetImageVisible() noexcept { return imageVisible; }
	[[nodiscard]] const Switch* GetImageVisible() const noexcept { return imageVisible; }
	[[nodiscard]] Label* GetImageVisibleLabel() noexcept { return imageVisibleLabel; }
	[[nodiscard]] const Label* GetImageVisibleLabel() const noexcept { return imageVisibleLabel; }
	[[nodiscard]] NativeSurface* GetDemoScene() noexcept { return demoScene; }
	[[nodiscard]] const NativeSurface* GetDemoScene() const noexcept { return demoScene; }
	[[nodiscard]] Label* GetTextBlock11() noexcept { return textBlock11; }
	[[nodiscard]] const Label* GetTextBlock11() const noexcept { return textBlock11; }
	[[nodiscard]] Grid* GetGrid3() noexcept { return grid3; }
	[[nodiscard]] const Grid* GetGrid3() const noexcept { return grid3; }
	[[nodiscard]] Grid* GetDetailGrid() noexcept { return detailGrid; }
	[[nodiscard]] const Grid* GetDetailGrid() const noexcept { return detailGrid; }
	[[nodiscard]] StackPanel* GetNavigationComposition() noexcept { return navigationComposition; }
	[[nodiscard]] const StackPanel* GetNavigationComposition() const noexcept { return navigationComposition; }
	[[nodiscard]] Label* GetTextBlock12() noexcept { return textBlock12; }
	[[nodiscard]] const Label* GetTextBlock12() const noexcept { return textBlock12; }
	[[nodiscard]] ListBox* GetSideNavigationList() noexcept { return sideNavigationList; }
	[[nodiscard]] const ListBox* GetSideNavigationList() const noexcept { return sideNavigationList; }
	[[nodiscard]] Border* GetBorder5() noexcept { return border5; }
	[[nodiscard]] const Border* GetBorder5() const noexcept { return border5; }
	[[nodiscard]] StackPanel* GetDetailComposition() noexcept { return detailComposition; }
	[[nodiscard]] const StackPanel* GetDetailComposition() const noexcept { return detailComposition; }
	[[nodiscard]] StackPanel* GetStackPanel13() noexcept { return stackPanel13; }
	[[nodiscard]] const StackPanel* GetStackPanel13() const noexcept { return stackPanel13; }
	[[nodiscard]] Label* GetTextBlock13() noexcept { return textBlock13; }
	[[nodiscard]] const Label* GetTextBlock13() const noexcept { return textBlock13; }
	[[nodiscard]] Label* GetTextBlock14() noexcept { return textBlock14; }
	[[nodiscard]] const Label* GetTextBlock14() const noexcept { return textBlock14; }
	[[nodiscard]] Label* GetTextBlock15() noexcept { return textBlock15; }
	[[nodiscard]] const Label* GetTextBlock15() const noexcept { return textBlock15; }
	[[nodiscard]] Label* GetTextBlock16() noexcept { return textBlock16; }
	[[nodiscard]] const Label* GetTextBlock16() const noexcept { return textBlock16; }
	[[nodiscard]] Label* GetTextBlock17() noexcept { return textBlock17; }
	[[nodiscard]] const Label* GetTextBlock17() const noexcept { return textBlock17; }
	[[nodiscard]] RichTextBox* GetSplitNotes() noexcept { return splitNotes; }
	[[nodiscard]] const RichTextBox* GetSplitNotes() const noexcept { return splitNotes; }
	[[nodiscard]] GroupBox* GetContainerGroup() noexcept { return containerGroup; }
	[[nodiscard]] const GroupBox* GetContainerGroup() const noexcept { return containerGroup; }
	[[nodiscard]] Label* GetContainerGroupText() noexcept { return containerGroupText; }
	[[nodiscard]] const Label* GetContainerGroupText() const noexcept { return containerGroupText; }
	[[nodiscard]] TabItem* GetTabItem3() noexcept { return tabItem3; }
	[[nodiscard]] const TabItem* GetTabItem3() const noexcept { return tabItem3; }
	[[nodiscard]] Border* GetBorder6() noexcept { return border6; }
	[[nodiscard]] const Border* GetBorder6() const noexcept { return border6; }
	[[nodiscard]] Grid* GetDataSurface() noexcept { return dataSurface; }
	[[nodiscard]] const Grid* GetDataSurface() const noexcept { return dataSurface; }
	[[nodiscard]] Label* GetTextBlock18() noexcept { return textBlock18; }
	[[nodiscard]] const Label* GetTextBlock18() const noexcept { return textBlock18; }
	[[nodiscard]] TreeView* GetDemoTree() noexcept { return demoTree; }
	[[nodiscard]] const TreeView* GetDemoTree() const noexcept { return demoTree; }
	[[nodiscard]] ListBox* GetDemoListBox() noexcept { return demoListBox; }
	[[nodiscard]] const ListBox* GetDemoListBox() const noexcept { return demoListBox; }
	[[nodiscard]] ListView* GetDemoList() noexcept { return demoList; }
	[[nodiscard]] const ListView* GetDemoList() const noexcept { return demoList; }
	[[nodiscard]] GroupBox* GetComposedPropertyEditor() noexcept { return composedPropertyEditor; }
	[[nodiscard]] const GroupBox* GetComposedPropertyEditor() const noexcept { return composedPropertyEditor; }
	[[nodiscard]] Grid* GetGrid4() noexcept { return grid4; }
	[[nodiscard]] const Grid* GetGrid4() const noexcept { return grid4; }
	[[nodiscard]] Label* GetTextBlock19() noexcept { return textBlock19; }
	[[nodiscard]] const Label* GetTextBlock19() const noexcept { return textBlock19; }
	[[nodiscard]] TextBox* GetComposedTitleEditor() noexcept { return composedTitleEditor; }
	[[nodiscard]] const TextBox* GetComposedTitleEditor() const noexcept { return composedTitleEditor; }
	[[nodiscard]] Label* GetTextBlock20() noexcept { return textBlock20; }
	[[nodiscard]] const Label* GetTextBlock20() const noexcept { return textBlock20; }
	[[nodiscard]] CheckBox* GetComposedEnabledEditor() noexcept { return composedEnabledEditor; }
	[[nodiscard]] const CheckBox* GetComposedEnabledEditor() const noexcept { return composedEnabledEditor; }
	[[nodiscard]] Label* GetTextBlock21() noexcept { return textBlock21; }
	[[nodiscard]] const Label* GetTextBlock21() const noexcept { return textBlock21; }
	[[nodiscard]] ComboBox* GetComposedDensityEditor() noexcept { return composedDensityEditor; }
	[[nodiscard]] const ComboBox* GetComposedDensityEditor() const noexcept { return composedDensityEditor; }
	[[nodiscard]] ComboBoxItem* GetComboBoxItem1() noexcept { return comboBoxItem1; }
	[[nodiscard]] const ComboBoxItem* GetComboBoxItem1() const noexcept { return comboBoxItem1; }
	[[nodiscard]] ComboBoxItem* GetComboBoxItem2() noexcept { return comboBoxItem2; }
	[[nodiscard]] const ComboBoxItem* GetComboBoxItem2() const noexcept { return comboBoxItem2; }
	[[nodiscard]] ComboBoxItem* GetComboBoxItem3() noexcept { return comboBoxItem3; }
	[[nodiscard]] const ComboBoxItem* GetComboBoxItem3() const noexcept { return comboBoxItem3; }
	[[nodiscard]] Label* GetTextBlock22() noexcept { return textBlock22; }
	[[nodiscard]] const Label* GetTextBlock22() const noexcept { return textBlock22; }
	[[nodiscard]] Slider* GetComposedScaleEditor() noexcept { return composedScaleEditor; }
	[[nodiscard]] const Slider* GetComposedScaleEditor() const noexcept { return composedScaleEditor; }
	[[nodiscard]] Label* GetTextBlock23() noexcept { return textBlock23; }
	[[nodiscard]] const Label* GetTextBlock23() const noexcept { return textBlock23; }
	[[nodiscard]] StackPanel* GetStackPanel14() noexcept { return stackPanel14; }
	[[nodiscard]] const StackPanel* GetStackPanel14() const noexcept { return stackPanel14; }
	[[nodiscard]] Label* GetTextBlock24() noexcept { return textBlock24; }
	[[nodiscard]] const Label* GetTextBlock24() const noexcept { return textBlock24; }
	[[nodiscard]] TreeView* GetAuthoredStateTree() noexcept { return authoredStateTree; }
	[[nodiscard]] const TreeView* GetAuthoredStateTree() const noexcept { return authoredStateTree; }
	[[nodiscard]] TreeViewItem* GetTreeViewItem1() noexcept { return treeViewItem1; }
	[[nodiscard]] const TreeViewItem* GetTreeViewItem1() const noexcept { return treeViewItem1; }
	[[nodiscard]] TreeViewItem* GetTreeViewItem2() noexcept { return treeViewItem2; }
	[[nodiscard]] const TreeViewItem* GetTreeViewItem2() const noexcept { return treeViewItem2; }
	[[nodiscard]] TreeViewItem* GetTreeViewItem3() noexcept { return treeViewItem3; }
	[[nodiscard]] const TreeViewItem* GetTreeViewItem3() const noexcept { return treeViewItem3; }
	[[nodiscard]] TreeViewItem* GetTreeViewItem4() noexcept { return treeViewItem4; }
	[[nodiscard]] const TreeViewItem* GetTreeViewItem4() const noexcept { return treeViewItem4; }
	[[nodiscard]] Label* GetTextBlock25() noexcept { return textBlock25; }
	[[nodiscard]] const Label* GetTextBlock25() const noexcept { return textBlock25; }
	[[nodiscard]] TabItem* GetTabItem4() noexcept { return tabItem4; }
	[[nodiscard]] const TabItem* GetTabItem4() const noexcept { return tabItem4; }
	[[nodiscard]] Border* GetBorder7() noexcept { return border7; }
	[[nodiscard]] const Border* GetBorder7() const noexcept { return border7; }
	[[nodiscard]] Grid* GetAnalyticsSurface() noexcept { return analyticsSurface; }
	[[nodiscard]] const Grid* GetAnalyticsSurface() const noexcept { return analyticsSurface; }
	[[nodiscard]] Label* GetTextBlock26() noexcept { return textBlock26; }
	[[nodiscard]] const Label* GetTextBlock26() const noexcept { return textBlock26; }
	[[nodiscard]] Border* GetBorder8() noexcept { return border8; }
	[[nodiscard]] const Border* GetBorder8() const noexcept { return border8; }
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
	[[nodiscard]] Grid* GetGrid5() noexcept { return grid5; }
	[[nodiscard]] const Grid* GetGrid5() const noexcept { return grid5; }
	[[nodiscard]] GroupBox* GetGroupBox1() noexcept { return groupBox1; }
	[[nodiscard]] const GroupBox* GetGroupBox1() const noexcept { return groupBox1; }
	[[nodiscard]] StackPanel* GetStackPanel15() noexcept { return stackPanel15; }
	[[nodiscard]] const StackPanel* GetStackPanel15() const noexcept { return stackPanel15; }
	[[nodiscard]] Label* GetTextBlock27() noexcept { return textBlock27; }
	[[nodiscard]] const Label* GetTextBlock27() const noexcept { return textBlock27; }
	[[nodiscard]] Label* GetTextBlock28() noexcept { return textBlock28; }
	[[nodiscard]] const Label* GetTextBlock28() const noexcept { return textBlock28; }
	[[nodiscard]] GroupBox* GetGroupBox2() noexcept { return groupBox2; }
	[[nodiscard]] const GroupBox* GetGroupBox2() const noexcept { return groupBox2; }
	[[nodiscard]] StackPanel* GetStackPanel16() noexcept { return stackPanel16; }
	[[nodiscard]] const StackPanel* GetStackPanel16() const noexcept { return stackPanel16; }
	[[nodiscard]] Label* GetTextBlock29() noexcept { return textBlock29; }
	[[nodiscard]] const Label* GetTextBlock29() const noexcept { return textBlock29; }
	[[nodiscard]] ProgressBar* GetProgressBar1() noexcept { return progressBar1; }
	[[nodiscard]] const ProgressBar* GetProgressBar1() const noexcept { return progressBar1; }
	[[nodiscard]] GroupBox* GetGroupBox3() noexcept { return groupBox3; }
	[[nodiscard]] const GroupBox* GetGroupBox3() const noexcept { return groupBox3; }
	[[nodiscard]] StackPanel* GetStackPanel17() noexcept { return stackPanel17; }
	[[nodiscard]] const StackPanel* GetStackPanel17() const noexcept { return stackPanel17; }
	[[nodiscard]] Label* GetTextBlock30() noexcept { return textBlock30; }
	[[nodiscard]] const Label* GetTextBlock30() const noexcept { return textBlock30; }
	[[nodiscard]] Label* GetTextBlock31() noexcept { return textBlock31; }
	[[nodiscard]] const Label* GetTextBlock31() const noexcept { return textBlock31; }
	[[nodiscard]] Label* GetTextBlock32() noexcept { return textBlock32; }
	[[nodiscard]] const Label* GetTextBlock32() const noexcept { return textBlock32; }
	[[nodiscard]] WrapPanel* GetWrapPanel2() noexcept { return wrapPanel2; }
	[[nodiscard]] const WrapPanel* GetWrapPanel2() const noexcept { return wrapPanel2; }
	[[nodiscard]] Button* GetChartBar() noexcept { return chartBar; }
	[[nodiscard]] const Button* GetChartBar() const noexcept { return chartBar; }
	[[nodiscard]] Button* GetChartPie() noexcept { return chartPie; }
	[[nodiscard]] const Button* GetChartPie() const noexcept { return chartPie; }
	[[nodiscard]] Button* GetChartLine() noexcept { return chartLine; }
	[[nodiscard]] const Button* GetChartLine() const noexcept { return chartLine; }
	[[nodiscard]] Grid* GetGrid6() noexcept { return grid6; }
	[[nodiscard]] const Grid* GetGrid6() const noexcept { return grid6; }
	[[nodiscard]] ChartView* GetSalesChart() noexcept { return salesChart; }
	[[nodiscard]] const ChartView* GetSalesChart() const noexcept { return salesChart; }
	[[nodiscard]] GroupBox* GetAnalyticsReport() noexcept { return analyticsReport; }
	[[nodiscard]] const GroupBox* GetAnalyticsReport() const noexcept { return analyticsReport; }
	[[nodiscard]] Grid* GetGrid7() noexcept { return grid7; }
	[[nodiscard]] const Grid* GetGrid7() const noexcept { return grid7; }
	[[nodiscard]] StackPanel* GetStackPanel18() noexcept { return stackPanel18; }
	[[nodiscard]] const StackPanel* GetStackPanel18() const noexcept { return stackPanel18; }
	[[nodiscard]] Label* GetTextBlock33() noexcept { return textBlock33; }
	[[nodiscard]] const Label* GetTextBlock33() const noexcept { return textBlock33; }
	[[nodiscard]] Label* GetTextBlock34() noexcept { return textBlock34; }
	[[nodiscard]] const Label* GetTextBlock34() const noexcept { return textBlock34; }
	[[nodiscard]] Label* GetTextBlock35() noexcept { return textBlock35; }
	[[nodiscard]] const Label* GetTextBlock35() const noexcept { return textBlock35; }
	[[nodiscard]] Label* GetTextBlock36() noexcept { return textBlock36; }
	[[nodiscard]] const Label* GetTextBlock36() const noexcept { return textBlock36; }
	[[nodiscard]] Label* GetTextBlock37() noexcept { return textBlock37; }
	[[nodiscard]] const Label* GetTextBlock37() const noexcept { return textBlock37; }
	[[nodiscard]] ListView* GetAnalyticsRows() noexcept { return analyticsRows; }
	[[nodiscard]] const ListView* GetAnalyticsRows() const noexcept { return analyticsRows; }
	[[nodiscard]] Label* GetTextBlock38() noexcept { return textBlock38; }
	[[nodiscard]] const Label* GetTextBlock38() const noexcept { return textBlock38; }
	[[nodiscard]] TabItem* GetTabItem5() noexcept { return tabItem5; }
	[[nodiscard]] const TabItem* GetTabItem5() const noexcept { return tabItem5; }
	[[nodiscard]] Border* GetBorder9() noexcept { return border9; }
	[[nodiscard]] const Border* GetBorder9() const noexcept { return border9; }
	[[nodiscard]] Grid* GetLayoutSurface() noexcept { return layoutSurface; }
	[[nodiscard]] const Grid* GetLayoutSurface() const noexcept { return layoutSurface; }
	[[nodiscard]] Label* GetLayoutTitle() noexcept { return layoutTitle; }
	[[nodiscard]] const Label* GetLayoutTitle() const noexcept { return layoutTitle; }
	[[nodiscard]] Canvas* GetCanvasSemanticsProbe() noexcept { return canvasSemanticsProbe; }
	[[nodiscard]] const Canvas* GetCanvasSemanticsProbe() const noexcept { return canvasSemanticsProbe; }
	[[nodiscard]] Border* GetBorder10() noexcept { return border10; }
	[[nodiscard]] const Border* GetBorder10() const noexcept { return border10; }
	[[nodiscard]] Label* GetCanvasLeftWins() noexcept { return canvasLeftWins; }
	[[nodiscard]] const Label* GetCanvasLeftWins() const noexcept { return canvasLeftWins; }
	[[nodiscard]] Label* GetCanvasRightBottom() noexcept { return canvasRightBottom; }
	[[nodiscard]] const Label* GetCanvasRightBottom() const noexcept { return canvasRightBottom; }
	[[nodiscard]] Border* GetBorder11() noexcept { return border11; }
	[[nodiscard]] const Border* GetBorder11() const noexcept { return border11; }
	[[nodiscard]] StackPanel* GetDemoStack() noexcept { return demoStack; }
	[[nodiscard]] const StackPanel* GetDemoStack() const noexcept { return demoStack; }
	[[nodiscard]] Label* GetTextBlock39() noexcept { return textBlock39; }
	[[nodiscard]] const Label* GetTextBlock39() const noexcept { return textBlock39; }
	[[nodiscard]] Button* GetStackA() noexcept { return stackA; }
	[[nodiscard]] const Button* GetStackA() const noexcept { return stackA; }
	[[nodiscard]] Button* GetStackB() noexcept { return stackB; }
	[[nodiscard]] const Button* GetStackB() const noexcept { return stackB; }
	[[nodiscard]] Button* GetStackC() noexcept { return stackC; }
	[[nodiscard]] const Button* GetStackC() const noexcept { return stackC; }
	[[nodiscard]] Border* GetBorder12() noexcept { return border12; }
	[[nodiscard]] const Border* GetBorder12() const noexcept { return border12; }
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
	[[nodiscard]] Border* GetBorder13() noexcept { return border13; }
	[[nodiscard]] const Border* GetBorder13() const noexcept { return border13; }
	[[nodiscard]] DockPanel* GetDemoDock() noexcept { return demoDock; }
	[[nodiscard]] const DockPanel* GetDemoDock() const noexcept { return demoDock; }
	[[nodiscard]] Label* GetTextBlock40() noexcept { return textBlock40; }
	[[nodiscard]] const Label* GetTextBlock40() const noexcept { return textBlock40; }
	[[nodiscard]] Button* GetDockTop() noexcept { return dockTop; }
	[[nodiscard]] const Button* GetDockTop() const noexcept { return dockTop; }
	[[nodiscard]] Button* GetDockLeft() noexcept { return dockLeft; }
	[[nodiscard]] const Button* GetDockLeft() const noexcept { return dockLeft; }
	[[nodiscard]] Label* GetDockFill() noexcept { return dockFill; }
	[[nodiscard]] const Label* GetDockFill() const noexcept { return dockFill; }
	[[nodiscard]] Border* GetBorder14() noexcept { return border14; }
	[[nodiscard]] const Border* GetBorder14() const noexcept { return border14; }
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
	[[nodiscard]] Border* GetBorder15() noexcept { return border15; }
	[[nodiscard]] const Border* GetBorder15() const noexcept { return border15; }
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
	[[nodiscard]] Border* GetBorder16() noexcept { return border16; }
	[[nodiscard]] const Border* GetBorder16() const noexcept { return border16; }
	[[nodiscard]] ScrollViewer* GetDemoScroll() noexcept { return demoScroll; }
	[[nodiscard]] const ScrollViewer* GetDemoScroll() const noexcept { return demoScroll; }
	[[nodiscard]] Grid* GetDemoScrollContent() noexcept { return demoScrollContent; }
	[[nodiscard]] const Grid* GetDemoScrollContent() const noexcept { return demoScrollContent; }
	[[nodiscard]] Border* GetBorder17() noexcept { return border17; }
	[[nodiscard]] const Border* GetBorder17() const noexcept { return border17; }
	[[nodiscard]] StackPanel* GetScrollCard1() noexcept { return scrollCard1; }
	[[nodiscard]] const StackPanel* GetScrollCard1() const noexcept { return scrollCard1; }
	[[nodiscard]] Label* GetScrollCard1Text() noexcept { return scrollCard1Text; }
	[[nodiscard]] const Label* GetScrollCard1Text() const noexcept { return scrollCard1Text; }
	[[nodiscard]] Label* GetTextBlock41() noexcept { return textBlock41; }
	[[nodiscard]] const Label* GetTextBlock41() const noexcept { return textBlock41; }
	[[nodiscard]] Border* GetBorder18() noexcept { return border18; }
	[[nodiscard]] const Border* GetBorder18() const noexcept { return border18; }
	[[nodiscard]] StackPanel* GetScrollCard2() noexcept { return scrollCard2; }
	[[nodiscard]] const StackPanel* GetScrollCard2() const noexcept { return scrollCard2; }
	[[nodiscard]] Label* GetScrollCard2Text() noexcept { return scrollCard2Text; }
	[[nodiscard]] const Label* GetScrollCard2Text() const noexcept { return scrollCard2Text; }
	[[nodiscard]] Label* GetTextBlock42() noexcept { return textBlock42; }
	[[nodiscard]] const Label* GetTextBlock42() const noexcept { return textBlock42; }
	[[nodiscard]] Label* GetTextBlock43() noexcept { return textBlock43; }
	[[nodiscard]] const Label* GetTextBlock43() const noexcept { return textBlock43; }
	[[nodiscard]] Button* GetFarButton() noexcept { return farButton; }
	[[nodiscard]] const Button* GetFarButton() const noexcept { return farButton; }
	[[nodiscard]] TabItem* GetTabItem6() noexcept { return tabItem6; }
	[[nodiscard]] const TabItem* GetTabItem6() const noexcept { return tabItem6; }
	[[nodiscard]] Border* GetBorder19() noexcept { return border19; }
	[[nodiscard]] const Border* GetBorder19() const noexcept { return border19; }
	[[nodiscard]] Grid* GetSystemSurface() noexcept { return systemSurface; }
	[[nodiscard]] const Grid* GetSystemSurface() const noexcept { return systemSurface; }
	[[nodiscard]] Label* GetSystemTitle() noexcept { return systemTitle; }
	[[nodiscard]] const Label* GetSystemTitle() const noexcept { return systemTitle; }
	[[nodiscard]] StackPanel* GetStackPanel19() noexcept { return stackPanel19; }
	[[nodiscard]] const StackPanel* GetStackPanel19() const noexcept { return stackPanel19; }
	[[nodiscard]] WrapPanel* GetWrapPanel3() noexcept { return wrapPanel3; }
	[[nodiscard]] const WrapPanel* GetWrapPanel3() const noexcept { return wrapPanel3; }
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
	[[nodiscard]] Border* GetBorder20() noexcept { return border20; }
	[[nodiscard]] const Border* GetBorder20() const noexcept { return border20; }
	[[nodiscard]] Grid* GetGrid8() noexcept { return grid8; }
	[[nodiscard]] const Grid* GetGrid8() const noexcept { return grid8; }
	[[nodiscard]] Label* GetTextBlock44() noexcept { return textBlock44; }
	[[nodiscard]] const Label* GetTextBlock44() const noexcept { return textBlock44; }
	[[nodiscard]] Button* GetCommandTargetButton() noexcept { return commandTargetButton; }
	[[nodiscard]] const Button* GetCommandTargetButton() const noexcept { return commandTargetButton; }
	[[nodiscard]] Label* GetTextBlock45() noexcept { return textBlock45; }
	[[nodiscard]] const Label* GetTextBlock45() const noexcept { return textBlock45; }
	[[nodiscard]] Label* GetCommandTargetTrace() noexcept { return commandTargetTrace; }
	[[nodiscard]] const Label* GetCommandTargetTrace() const noexcept { return commandTargetTrace; }
	[[nodiscard]] Label* GetTextBlock46() noexcept { return textBlock46; }
	[[nodiscard]] const Label* GetTextBlock46() const noexcept { return textBlock46; }
	[[nodiscard]] Label* GetTextBlock47() noexcept { return textBlock47; }
	[[nodiscard]] const Label* GetTextBlock47() const noexcept { return textBlock47; }
	[[nodiscard]] Label* GetTextBlock48() noexcept { return textBlock48; }
	[[nodiscard]] const Label* GetTextBlock48() const noexcept { return textBlock48; }
	[[nodiscard]] GroupBox* GetNotificationPanel() noexcept { return notificationPanel; }
	[[nodiscard]] const GroupBox* GetNotificationPanel() const noexcept { return notificationPanel; }
	[[nodiscard]] Grid* GetGrid9() noexcept { return grid9; }
	[[nodiscard]] const Grid* GetGrid9() const noexcept { return grid9; }
	[[nodiscard]] Label* GetTextBlock49() noexcept { return textBlock49; }
	[[nodiscard]] const Label* GetTextBlock49() const noexcept { return textBlock49; }
	[[nodiscard]] Label* GetToastMessage() noexcept { return toastMessage; }
	[[nodiscard]] const Label* GetToastMessage() const noexcept { return toastMessage; }
	[[nodiscard]] ProgressBar* GetProgressBar2() noexcept { return progressBar2; }
	[[nodiscard]] const ProgressBar* GetProgressBar2() const noexcept { return progressBar2; }
	[[nodiscard]] Button* GetDismissToast() noexcept { return dismissToast; }
	[[nodiscard]] const Button* GetDismissToast() const noexcept { return dismissToast; }
	[[nodiscard]] Label* GetTextBlock50() noexcept { return textBlock50; }
	[[nodiscard]] const Label* GetTextBlock50() const noexcept { return textBlock50; }
	[[nodiscard]] TabItem* GetTabItem7() noexcept { return tabItem7; }
	[[nodiscard]] const TabItem* GetTabItem7() const noexcept { return tabItem7; }
	[[nodiscard]] Border* GetBorder21() noexcept { return border21; }
	[[nodiscard]] const Border* GetBorder21() const noexcept { return border21; }
	[[nodiscard]] Grid* GetWebSurface() noexcept { return webSurface; }
	[[nodiscard]] const Grid* GetWebSurface() const noexcept { return webSurface; }
	[[nodiscard]] Grid* GetGrid10() noexcept { return grid10; }
	[[nodiscard]] const Grid* GetGrid10() const noexcept { return grid10; }
	[[nodiscard]] Button* GetInvokeWeb() noexcept { return invokeWeb; }
	[[nodiscard]] const Button* GetInvokeWeb() const noexcept { return invokeWeb; }
	[[nodiscard]] Label* GetWebHint() noexcept { return webHint; }
	[[nodiscard]] const Label* GetWebHint() const noexcept { return webHint; }
	[[nodiscard]] Border* GetBorder22() noexcept { return border22; }
	[[nodiscard]] const Border* GetBorder22() const noexcept { return border22; }
	[[nodiscard]] WebBrowser* GetWebBrowser() noexcept { return webBrowser; }
	[[nodiscard]] const WebBrowser* GetWebBrowser() const noexcept { return webBrowser; }
	[[nodiscard]] TabItem* GetTabItem8() noexcept { return tabItem8; }
	[[nodiscard]] const TabItem* GetTabItem8() const noexcept { return tabItem8; }
	[[nodiscard]] Border* GetBorder23() noexcept { return border23; }
	[[nodiscard]] const Border* GetBorder23() const noexcept { return border23; }
	[[nodiscard]] Grid* GetMediaSurface() noexcept { return mediaSurface; }
	[[nodiscard]] const Grid* GetMediaSurface() const noexcept { return mediaSurface; }
	[[nodiscard]] MediaElement* GetMediaElement() noexcept { return mediaElement; }
	[[nodiscard]] const MediaElement* GetMediaElement() const noexcept { return mediaElement; }
	[[nodiscard]] Grid* GetGrid11() noexcept { return grid11; }
	[[nodiscard]] const Grid* GetGrid11() const noexcept { return grid11; }
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
	[[nodiscard]] Grid* GetGrid12() noexcept { return grid12; }
	[[nodiscard]] const Grid* GetGrid12() const noexcept { return grid12; }
	[[nodiscard]] Slider* GetMediaProgress() noexcept { return mediaProgress; }
	[[nodiscard]] const Slider* GetMediaProgress() const noexcept { return mediaProgress; }
	[[nodiscard]] Label* GetMediaTime() noexcept { return mediaTime; }
	[[nodiscard]] const Label* GetMediaTime() const noexcept { return mediaTime; }
	[[nodiscard]] TabItem* GetTabItem9() noexcept { return tabItem9; }
	[[nodiscard]] const TabItem* GetTabItem9() const noexcept { return tabItem9; }
	[[nodiscard]] Border* GetBorder24() noexcept { return border24; }
	[[nodiscard]] const Border* GetBorder24() const noexcept { return border24; }
	[[nodiscard]] Grid* GetWpfLabSurface() noexcept { return wpfLabSurface; }
	[[nodiscard]] const Grid* GetWpfLabSurface() const noexcept { return wpfLabSurface; }
	[[nodiscard]] Label* GetWpfLabTitle() noexcept { return wpfLabTitle; }
	[[nodiscard]] const Label* GetWpfLabTitle() const noexcept { return wpfLabTitle; }
	[[nodiscard]] ContentControl* GetWpfBindingScope() noexcept { return wpfBindingScope; }
	[[nodiscard]] const ContentControl* GetWpfBindingScope() const noexcept { return wpfBindingScope; }
	[[nodiscard]] StackPanel* GetStackPanel20() noexcept { return stackPanel20; }
	[[nodiscard]] const StackPanel* GetStackPanel20() const noexcept { return stackPanel20; }
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
	[[nodiscard]] Label* GetTextBlock51() noexcept { return textBlock51; }
	[[nodiscard]] const Label* GetTextBlock51() const noexcept { return textBlock51; }
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
	[[nodiscard]] Label* GetTextBlock52() noexcept { return textBlock52; }
	[[nodiscard]] const Label* GetTextBlock52() const noexcept { return textBlock52; }
	[[nodiscard]] Grid* GetWpfItemsScope() noexcept { return wpfItemsScope; }
	[[nodiscard]] const Grid* GetWpfItemsScope() const noexcept { return wpfItemsScope; }
	[[nodiscard]] Label* GetTextBlock53() noexcept { return textBlock53; }
	[[nodiscard]] const Label* GetTextBlock53() const noexcept { return textBlock53; }
	[[nodiscard]] ListBox* GetWpfTemplateList() noexcept { return wpfTemplateList; }
	[[nodiscard]] const ListBox* GetWpfTemplateList() const noexcept { return wpfTemplateList; }
	[[nodiscard]] Border* GetWpfRouteOuter() noexcept { return wpfRouteOuter; }
	[[nodiscard]] const Border* GetWpfRouteOuter() const noexcept { return wpfRouteOuter; }
	[[nodiscard]] Grid* GetGrid13() noexcept { return grid13; }
	[[nodiscard]] const Grid* GetGrid13() const noexcept { return grid13; }
	[[nodiscard]] Label* GetTextBlock54() noexcept { return textBlock54; }
	[[nodiscard]] const Label* GetTextBlock54() const noexcept { return textBlock54; }
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
	[[nodiscard]] TabItem* GetTabItem10() noexcept { return tabItem10; }
	[[nodiscard]] const TabItem* GetTabItem10() const noexcept { return tabItem10; }
	[[nodiscard]] Border* GetBorder25() noexcept { return border25; }
	[[nodiscard]] const Border* GetBorder25() const noexcept { return border25; }
	[[nodiscard]] Grid* GetTextCompositionLabSurface() noexcept { return textCompositionLabSurface; }
	[[nodiscard]] const Grid* GetTextCompositionLabSurface() const noexcept { return textCompositionLabSurface; }
	[[nodiscard]] Label* GetTextBlock55() noexcept { return textBlock55; }
	[[nodiscard]] const Label* GetTextBlock55() const noexcept { return textBlock55; }
	[[nodiscard]] Border* GetBorder26() noexcept { return border26; }
	[[nodiscard]] const Border* GetBorder26() const noexcept { return border26; }
	[[nodiscard]] Grid* GetGrid14() noexcept { return grid14; }
	[[nodiscard]] const Grid* GetGrid14() const noexcept { return grid14; }
	[[nodiscard]] Label* GetTextBlock56() noexcept { return textBlock56; }
	[[nodiscard]] const Label* GetTextBlock56() const noexcept { return textBlock56; }
	[[nodiscard]] Label* GetTextBlock57() noexcept { return textBlock57; }
	[[nodiscard]] const Label* GetTextBlock57() const noexcept { return textBlock57; }
	[[nodiscard]] TextBox* GetCompositionTextBox() noexcept { return compositionTextBox; }
	[[nodiscard]] const TextBox* GetCompositionTextBox() const noexcept { return compositionTextBox; }
	[[nodiscard]] Label* GetTextBlock58() noexcept { return textBlock58; }
	[[nodiscard]] const Label* GetTextBlock58() const noexcept { return textBlock58; }
	[[nodiscard]] RichTextBox* GetCompositionRichTextBox() noexcept { return compositionRichTextBox; }
	[[nodiscard]] const RichTextBox* GetCompositionRichTextBox() const noexcept { return compositionRichTextBox; }
	[[nodiscard]] Label* GetTextBlock59() noexcept { return textBlock59; }
	[[nodiscard]] const Label* GetTextBlock59() const noexcept { return textBlock59; }
	[[nodiscard]] PasswordBox* GetCompositionPasswordBox() noexcept { return compositionPasswordBox; }
	[[nodiscard]] const PasswordBox* GetCompositionPasswordBox() const noexcept { return compositionPasswordBox; }
	[[nodiscard]] Label* GetTextBlock60() noexcept { return textBlock60; }
	[[nodiscard]] const Label* GetTextBlock60() const noexcept { return textBlock60; }
	[[nodiscard]] Border* GetBorder27() noexcept { return border27; }
	[[nodiscard]] const Border* GetBorder27() const noexcept { return border27; }
	[[nodiscard]] Grid* GetGrid15() noexcept { return grid15; }
	[[nodiscard]] const Grid* GetGrid15() const noexcept { return grid15; }
	[[nodiscard]] Label* GetTextBlock61() noexcept { return textBlock61; }
	[[nodiscard]] const Label* GetTextBlock61() const noexcept { return textBlock61; }
	[[nodiscard]] WrapPanel* GetWrapPanel4() noexcept { return wrapPanel4; }
	[[nodiscard]] const WrapPanel* GetWrapPanel4() const noexcept { return wrapPanel4; }
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
	[[nodiscard]] Border* GetBorder28() noexcept { return border28; }
	[[nodiscard]] const Border* GetBorder28() const noexcept { return border28; }
	[[nodiscard]] Grid* GetGrid16() noexcept { return grid16; }
	[[nodiscard]] const Grid* GetGrid16() const noexcept { return grid16; }
	[[nodiscard]] Label* GetTextBlock62() noexcept { return textBlock62; }
	[[nodiscard]] const Label* GetTextBlock62() const noexcept { return textBlock62; }
	[[nodiscard]] Label* GetCompositionTrace() noexcept { return compositionTrace; }
	[[nodiscard]] const Label* GetCompositionTrace() const noexcept { return compositionTrace; }
	[[nodiscard]] TabItem* GetTabItem11() noexcept { return tabItem11; }
	[[nodiscard]] const TabItem* GetTabItem11() const noexcept { return tabItem11; }
	[[nodiscard]] Border* GetBorder29() noexcept { return border29; }
	[[nodiscard]] const Border* GetBorder29() const noexcept { return border29; }
	[[nodiscard]] Grid* GetPresentationLabSurface() noexcept { return presentationLabSurface; }
	[[nodiscard]] const Grid* GetPresentationLabSurface() const noexcept { return presentationLabSurface; }
	[[nodiscard]] Label* GetTextBlock63() noexcept { return textBlock63; }
	[[nodiscard]] const Label* GetTextBlock63() const noexcept { return textBlock63; }
	[[nodiscard]] Grid* GetGrid17() noexcept { return grid17; }
	[[nodiscard]] const Grid* GetGrid17() const noexcept { return grid17; }
	[[nodiscard]] NativeSurface* GetPresentationProbeSurface() noexcept { return presentationProbeSurface; }
	[[nodiscard]] const NativeSurface* GetPresentationProbeSurface() const noexcept { return presentationProbeSurface; }
	[[nodiscard]] Canvas* GetCanvas1() noexcept { return canvas1; }
	[[nodiscard]] const Canvas* GetCanvas1() const noexcept { return canvas1; }
	[[nodiscard]] Label* GetPresentationTopologyTile() noexcept { return presentationTopologyTile; }
	[[nodiscard]] const Label* GetPresentationTopologyTile() const noexcept { return presentationTopologyTile; }
	[[nodiscard]] StackPanel* GetStackPanel21() noexcept { return stackPanel21; }
	[[nodiscard]] const StackPanel* GetStackPanel21() const noexcept { return stackPanel21; }
	[[nodiscard]] Label* GetTextBlock64() noexcept { return textBlock64; }
	[[nodiscard]] const Label* GetTextBlock64() const noexcept { return textBlock64; }
	[[nodiscard]] Label* GetTextBlock65() noexcept { return textBlock65; }
	[[nodiscard]] const Label* GetTextBlock65() const noexcept { return textBlock65; }
	[[nodiscard]] Label* GetTextBlock66() noexcept { return textBlock66; }
	[[nodiscard]] const Label* GetTextBlock66() const noexcept { return textBlock66; }
	[[nodiscard]] Label* GetTextBlock67() noexcept { return textBlock67; }
	[[nodiscard]] const Label* GetTextBlock67() const noexcept { return textBlock67; }
	[[nodiscard]] Label* GetTextBlock68() noexcept { return textBlock68; }
	[[nodiscard]] const Label* GetTextBlock68() const noexcept { return textBlock68; }
	[[nodiscard]] Label* GetTextBlock69() noexcept { return textBlock69; }
	[[nodiscard]] const Label* GetTextBlock69() const noexcept { return textBlock69; }
	[[nodiscard]] Grid* GetGrid18() noexcept { return grid18; }
	[[nodiscard]] const Grid* GetGrid18() const noexcept { return grid18; }
	[[nodiscard]] WrapPanel* GetWrapPanel5() noexcept { return wrapPanel5; }
	[[nodiscard]] const WrapPanel* GetWrapPanel5() const noexcept { return wrapPanel5; }
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
	[[nodiscard]] Label* GetTextBlock70() noexcept { return textBlock70; }
	[[nodiscard]] const Label* GetTextBlock70() const noexcept { return textBlock70; }
	[[nodiscard]] ContextMenu* GetSystemContextMenu() noexcept { return systemContextMenu; }
	[[nodiscard]] const ContextMenu* GetSystemContextMenu() const noexcept { return systemContextMenu; }
	[[nodiscard]] MenuItem* GetMenuItem6() noexcept { return menuItem6; }
	[[nodiscard]] const MenuItem* GetMenuItem6() const noexcept { return menuItem6; }
	[[nodiscard]] MenuItem* GetMenuItem7() noexcept { return menuItem7; }
	[[nodiscard]] const MenuItem* GetMenuItem7() const noexcept { return menuItem7; }
	[[nodiscard]] Separator* GetSeparator2() noexcept { return separator2; }
	[[nodiscard]] const Separator* GetSeparator2() const noexcept { return separator2; }
	[[nodiscard]] MenuItem* GetMenuItem8() noexcept { return menuItem8; }
	[[nodiscard]] const MenuItem* GetMenuItem8() const noexcept { return menuItem8; }
	[[nodiscard]] MenuItem* GetMenuItem9() noexcept { return menuItem9; }
	[[nodiscard]] const MenuItem* GetMenuItem9() const noexcept { return menuItem9; }
	[[nodiscard]] MenuItem* GetMenuItem10() noexcept { return menuItem10; }
	[[nodiscard]] const MenuItem* GetMenuItem10() const noexcept { return menuItem10; }
	[[nodiscard]] StatusBar* GetMainStatusBar() noexcept { return mainStatusBar; }
	[[nodiscard]] const StatusBar* GetMainStatusBar() const noexcept { return mainStatusBar; }

	DemoWindowGenerated();
	virtual ~DemoWindowGenerated();
	bool BindData(BindingSourceReference dataContext);
};
