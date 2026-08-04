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
#include "MediaPlayer.h"
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
	Canvas* toolSeparator = nullptr;
	Button* toolIcon1 = nullptr;
	Image* toolIconImage1 = nullptr;
	Button* toolIcon2 = nullptr;
	Image* toolIconImage2 = nullptr;
	Button* toolIcon3 = nullptr;
	Image* toolIconImage3 = nullptr;
	Canvas* canvas1 = nullptr;
	Slider* globalProgress = nullptr;
	Label* statusText = nullptr;
	Label* runtimeBadge = nullptr;
	TabControl* mainTabs = nullptr;
	TabItem* tabItem1 = nullptr;
	Border* border1 = nullptr;
	Canvas* basicSurface = nullptr;
	Label* basicTitle = nullptr;
	Label* frameworkThemeHint = nullptr;
	Button* basicButton = nullptr;
	CheckBox* enableInput = nullptr;
	RadioButton* radioA = nullptr;
	RadioButton* radioB = nullptr;
	TextBox* nameInput = nullptr;
	PasswordBox* passwordInput = nullptr;
	ComboBox* basicCombo = nullptr;
	TextBox* dateInput = nullptr;
	NumericUpDown* numberInput = nullptr;
	Button* dialogCancelButton = nullptr;
	Button* docsLink = nullptr;
	Label* textBlock1 = nullptr;
	Slider* verticalThemeSlider = nullptr;
	ProgressBar* verticalThemeProgress = nullptr;
	TextBox* gradientInput = nullptr;
	Label* gradientLabel = nullptr;
	DemoWindowGeneratedFeatureCard* featureCard = nullptr;
	Label* featureCardContent = nullptr;
	Button* featureActionA = nullptr;
	Button* featureActionB = nullptr;
	GroupBox* basicGroup = nullptr;
	Canvas* basicGroupContent = nullptr;
	Label* groupHint = nullptr;
	TextBox* groupName = nullptr;
	CheckBox* groupEnabled = nullptr;
	Button* themeNormalButton = nullptr;
	Button* themeDisabledButton = nullptr;
	Expander* basicExpander = nullptr;
	Canvas* basicExpanderContent = nullptr;
	Label* expanderText = nullptr;
	ContentControl* themeContentControlProbe = nullptr;
	ItemsControl* themeItemsControlProbe = nullptr;
	Label* textBlock2 = nullptr;
	Separator* themeSeparatorProbe = nullptr;
	TabItem* tabItem2 = nullptr;
	Border* border2 = nullptr;
	Canvas* containerSurface = nullptr;
	Button* openImageButton = nullptr;
	Image* demoImage = nullptr;
	ProgressBar* demoProgress = nullptr;
	ProgressBar* indeterminateProgress = nullptr;
	LoadingRing* loadingRing = nullptr;
	ProgressRing* progressRing = nullptr;
	Switch* imageVisible = nullptr;
	Label* imageVisibleLabel = nullptr;
	NativeSurface* demoScene = nullptr;
	Grid* detailGrid = nullptr;
	Canvas* navigationComposition = nullptr;
	Label* textBlock3 = nullptr;
	ListBox* sideNavigationList = nullptr;
	Canvas* canvas2 = nullptr;
	Canvas* detailComposition = nullptr;
	StackPanel* stackPanel1 = nullptr;
	Label* textBlock4 = nullptr;
	Label* textBlock5 = nullptr;
	Label* textBlock6 = nullptr;
	Label* textBlock7 = nullptr;
	Label* textBlock8 = nullptr;
	RichTextBox* splitNotes = nullptr;
	GroupBox* containerGroup = nullptr;
	Label* containerGroupText = nullptr;
	TabItem* tabItem3 = nullptr;
	Border* border3 = nullptr;
	Canvas* dataSurface = nullptr;
	TreeView* demoTree = nullptr;
	ListBox* demoListBox = nullptr;
	ListView* demoList = nullptr;
	GroupBox* composedPropertyEditor = nullptr;
	Canvas* canvas3 = nullptr;
	Label* textBlock9 = nullptr;
	TextBox* composedTitleEditor = nullptr;
	Label* textBlock10 = nullptr;
	CheckBox* composedEnabledEditor = nullptr;
	Label* textBlock11 = nullptr;
	ComboBox* composedDensityEditor = nullptr;
	ComboBoxItem* comboBoxItem1 = nullptr;
	ComboBoxItem* comboBoxItem2 = nullptr;
	ComboBoxItem* comboBoxItem3 = nullptr;
	Label* textBlock12 = nullptr;
	Slider* composedScaleEditor = nullptr;
	Label* textBlock13 = nullptr;
	TreeView* authoredStateTree = nullptr;
	TreeViewItem* treeViewItem1 = nullptr;
	TreeViewItem* treeViewItem2 = nullptr;
	TreeViewItem* treeViewItem3 = nullptr;
	TreeViewItem* treeViewItem4 = nullptr;
	Label* textBlock14 = nullptr;
	TabItem* tabItem4 = nullptr;
	Border* border4 = nullptr;
	Canvas* analyticsSurface = nullptr;
	Border* border5 = nullptr;
	Canvas* analyticsFilterSurface = nullptr;
	TextBox* analyticsQuery = nullptr;
	CheckBox* analyticsClosed = nullptr;
	CheckBox* analyticsContract = nullptr;
	CheckBox* analyticsHighMargin = nullptr;
	Button* analyticsApply = nullptr;
	Button* analyticsReset = nullptr;
	GroupBox* groupBox1 = nullptr;
	Canvas* canvas4 = nullptr;
	Label* textBlock15 = nullptr;
	Label* textBlock16 = nullptr;
	GroupBox* groupBox2 = nullptr;
	Canvas* canvas5 = nullptr;
	Label* textBlock17 = nullptr;
	ProgressBar* progressBar1 = nullptr;
	GroupBox* groupBox3 = nullptr;
	Canvas* canvas6 = nullptr;
	Label* textBlock18 = nullptr;
	Label* textBlock19 = nullptr;
	Button* chartBar = nullptr;
	Button* chartPie = nullptr;
	Button* chartLine = nullptr;
	ChartView* salesChart = nullptr;
	GroupBox* analyticsReport = nullptr;
	Canvas* canvas7 = nullptr;
	StackPanel* stackPanel2 = nullptr;
	Label* textBlock20 = nullptr;
	Label* textBlock21 = nullptr;
	Label* textBlock22 = nullptr;
	Label* textBlock23 = nullptr;
	Label* textBlock24 = nullptr;
	ListView* analyticsRows = nullptr;
	Label* textBlock25 = nullptr;
	TabItem* tabItem5 = nullptr;
	Border* border6 = nullptr;
	Canvas* layoutSurface = nullptr;
	Label* layoutTitle = nullptr;
	Canvas* canvasSemanticsProbe = nullptr;
	Border* border7 = nullptr;
	Label* canvasLeftWins = nullptr;
	Label* canvasRightBottom = nullptr;
	Border* border8 = nullptr;
	StackPanel* demoStack = nullptr;
	Button* stackA = nullptr;
	Button* stackB = nullptr;
	Button* stackC = nullptr;
	Border* border9 = nullptr;
	Grid* demoGrid = nullptr;
	Button* gridHeader = nullptr;
	Label* gridLeft = nullptr;
	TextBox* gridEditor = nullptr;
	Button* gridFooter = nullptr;
	Border* border10 = nullptr;
	DockPanel* demoDock = nullptr;
	Button* dockTop = nullptr;
	Button* dockLeft = nullptr;
	Label* dockFill = nullptr;
	Border* border11 = nullptr;
	WrapPanel* demoWrap = nullptr;
	Button* wrap1 = nullptr;
	Button* wrap2 = nullptr;
	Button* wrap3 = nullptr;
	Button* wrap4 = nullptr;
	Button* wrap5 = nullptr;
	Button* wrap6 = nullptr;
	Border* border12 = nullptr;
	RelativePanel* demoRelative = nullptr;
	StackPanel* relativeCenter = nullptr;
	Label* naturalTextProbe = nullptr;
	Label* wrappedTextProbe = nullptr;
	Label* trimmedTextProbe = nullptr;
	Button* relativeCenterButton = nullptr;
	Border* border13 = nullptr;
	ScrollViewer* demoScroll = nullptr;
	Canvas* demoScrollContent = nullptr;
	Border* border14 = nullptr;
	Canvas* scrollCard1 = nullptr;
	Label* scrollCard1Text = nullptr;
	Border* border15 = nullptr;
	Canvas* scrollCard2 = nullptr;
	Label* scrollCard2Text = nullptr;
	Button* farButton = nullptr;
	TabItem* tabItem6 = nullptr;
	Border* border16 = nullptr;
	Canvas* systemSurface = nullptr;
	Label* systemTitle = nullptr;
	Button* notifyToggle = nullptr;
	Button* notifyBalloon = nullptr;
	Button* showDialog = nullptr;
	Button* showToast = nullptr;
	Label* systemHint = nullptr;
	Border* border17 = nullptr;
	Canvas* canvas8 = nullptr;
	Label* textBlock26 = nullptr;
	Button* commandTargetButton = nullptr;
	Label* textBlock27 = nullptr;
	Label* commandTargetTrace = nullptr;
	Label* textBlock28 = nullptr;
	Label* textBlock29 = nullptr;
	Label* textBlock30 = nullptr;
	GroupBox* notificationPanel = nullptr;
	Canvas* canvas9 = nullptr;
	Label* textBlock31 = nullptr;
	Label* toastMessage = nullptr;
	ProgressBar* progressBar2 = nullptr;
	Button* dismissToast = nullptr;
	Label* textBlock32 = nullptr;
	TabItem* tabItem7 = nullptr;
	Border* border18 = nullptr;
	Canvas* webSurface = nullptr;
	Button* invokeWeb = nullptr;
	Label* webHint = nullptr;
	Border* border19 = nullptr;
	WebBrowser* webBrowser = nullptr;
	TabItem* tabItem8 = nullptr;
	Border* border20 = nullptr;
	Canvas* mediaSurface = nullptr;
	MediaPlayer* mediaPlayer = nullptr;
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
	Slider* mediaProgress = nullptr;
	Label* mediaTime = nullptr;
	TabItem* tabItem9 = nullptr;
	Border* border21 = nullptr;
	Canvas* wpfLabSurface = nullptr;
	Label* wpfLabTitle = nullptr;
	ContentControl* wpfBindingScope = nullptr;
	StackPanel* stackPanel3 = nullptr;
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
	Canvas* wpfTemplateAndStyleScope = nullptr;
	Label* textBlock33 = nullptr;
	Button* wpfTemplateButton = nullptr;
	Button* wpfTriggerButton = nullptr;
	Label* wpfScopeResourceValue = nullptr;
	StackPanel* wpfInnerResourceScope = nullptr;
	Label* wpfInnerResourceValue = nullptr;
	Label* textBlock34 = nullptr;
	Canvas* wpfItemsScope = nullptr;
	Label* textBlock35 = nullptr;
	ListBox* wpfTemplateList = nullptr;
	Border* wpfRouteOuter = nullptr;
	Canvas* canvas10 = nullptr;
	Label* textBlock36 = nullptr;
	Canvas* wpfRouteMiddle = nullptr;
	Button* wpfRouteSource = nullptr;
	Button* wpfFocusPeerB = nullptr;
	Button* wpfFocusPeerC = nullptr;
	Button* wpfNoFocusPeer = nullptr;
	TextBox* wpfTextInputSource = nullptr;
	Label* wpfRouteTrace = nullptr;
	Label* wpfInputStats = nullptr;
	Canvas* wpfHierarchyScope = nullptr;
	Label* wpfHierarchyChain = nullptr;
	Button* wpfDispatcherProbe = nullptr;
	Label* wpfDispatcherResult = nullptr;
	TabItem* tabItem10 = nullptr;
	Border* border22 = nullptr;
	Canvas* textCompositionLabSurface = nullptr;
	Label* textBlock37 = nullptr;
	Border* border23 = nullptr;
	Canvas* canvas11 = nullptr;
	Label* textBlock38 = nullptr;
	Label* textBlock39 = nullptr;
	TextBox* compositionTextBox = nullptr;
	Label* textBlock40 = nullptr;
	RichTextBox* compositionRichTextBox = nullptr;
	Label* textBlock41 = nullptr;
	PasswordBox* compositionPasswordBox = nullptr;
	Label* textBlock42 = nullptr;
	Border* border24 = nullptr;
	Canvas* canvas12 = nullptr;
	Label* textBlock43 = nullptr;
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
	Border* border25 = nullptr;
	Canvas* canvas13 = nullptr;
	Label* textBlock44 = nullptr;
	Label* compositionTrace = nullptr;
	TabItem* tabItem11 = nullptr;
	Border* border26 = nullptr;
	Canvas* presentationLabSurface = nullptr;
	Label* textBlock45 = nullptr;
	NativeSurface* presentationProbeSurface = nullptr;
	Label* presentationTopologyTile = nullptr;
	Canvas* canvas14 = nullptr;
	Label* textBlock46 = nullptr;
	Label* textBlock47 = nullptr;
	Label* textBlock48 = nullptr;
	Label* textBlock49 = nullptr;
	Label* textBlock50 = nullptr;
	Label* textBlock51 = nullptr;
	Button* presentationRegionButton = nullptr;
	Button* presentationGeometryButton = nullptr;
	Button* presentationCompositionButton = nullptr;
	Button* presentationFullButton = nullptr;
	Button* presentationTopologyButton = nullptr;
	Button* presentationDeviceLossButton = nullptr;
	Label* presentationStatus = nullptr;
	Label* textBlock52 = nullptr;
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
		static constexpr int canvas1 = 31;
		static constexpr int globalProgress = 3;
		static constexpr int statusText = 4;
		static constexpr int runtimeBadge = 5;
		static constexpr int mainTabs = 10;
		static constexpr int tabItem1 = 32;
		static constexpr int border1 = 33;
		static constexpr int basicSurface = 100;
		static constexpr int basicTitle = 101;
		static constexpr int frameworkThemeHint = 124;
		static constexpr int basicButton = 102;
		static constexpr int enableInput = 103;
		static constexpr int radioA = 104;
		static constexpr int radioB = 105;
		static constexpr int nameInput = 106;
		static constexpr int passwordInput = 107;
		static constexpr int basicCombo = 108;
		static constexpr int dateInput = 109;
		static constexpr int numberInput = 110;
		static constexpr int dialogCancelButton = 123;
		static constexpr int docsLink = 111;
		static constexpr int textBlock1 = 129;
		static constexpr int verticalThemeSlider = 127;
		static constexpr int verticalThemeProgress = 128;
		static constexpr int gradientInput = 112;
		static constexpr int gradientLabel = 113;
		static constexpr int featureCard = 122;
		static constexpr int featureCardContent = 130;
		static constexpr int featureActionA = 131;
		static constexpr int featureActionB = 132;
		static constexpr int basicGroup = 114;
		static constexpr int basicGroupContent = 120;
		static constexpr int groupHint = 115;
		static constexpr int groupName = 116;
		static constexpr int groupEnabled = 117;
		static constexpr int themeNormalButton = 125;
		static constexpr int themeDisabledButton = 126;
		static constexpr int basicExpander = 118;
		static constexpr int basicExpanderContent = 121;
		static constexpr int expanderText = 119;
		static constexpr int themeContentControlProbe = 133;
		static constexpr int themeItemsControlProbe = 134;
		static constexpr int textBlock2 = 135;
		static constexpr int themeSeparatorProbe = 136;
		static constexpr int tabItem2 = 137;
		static constexpr int border2 = 138;
		static constexpr int containerSurface = 200;
		static constexpr int openImageButton = 201;
		static constexpr int demoImage = 202;
		static constexpr int demoProgress = 203;
		static constexpr int indeterminateProgress = 215;
		static constexpr int loadingRing = 204;
		static constexpr int progressRing = 205;
		static constexpr int imageVisible = 206;
		static constexpr int imageVisibleLabel = 207;
		static constexpr int demoScene = 214;
		static constexpr int detailGrid = 208;
		static constexpr int navigationComposition = 209;
		static constexpr int textBlock3 = 216;
		static constexpr int sideNavigationList = 217;
		static constexpr int canvas2 = 218;
		static constexpr int detailComposition = 210;
		static constexpr int stackPanel1 = 219;
		static constexpr int textBlock4 = 220;
		static constexpr int textBlock5 = 221;
		static constexpr int textBlock6 = 222;
		static constexpr int textBlock7 = 223;
		static constexpr int textBlock8 = 224;
		static constexpr int splitNotes = 211;
		static constexpr int containerGroup = 212;
		static constexpr int containerGroupText = 213;
		static constexpr int tabItem3 = 225;
		static constexpr int border3 = 226;
		static constexpr int dataSurface = 300;
		static constexpr int demoTree = 301;
		static constexpr int demoListBox = 302;
		static constexpr int demoList = 303;
		static constexpr int composedPropertyEditor = 309;
		static constexpr int canvas3 = 310;
		static constexpr int textBlock9 = 311;
		static constexpr int composedTitleEditor = 312;
		static constexpr int textBlock10 = 313;
		static constexpr int composedEnabledEditor = 314;
		static constexpr int textBlock11 = 315;
		static constexpr int composedDensityEditor = 316;
		static constexpr int comboBoxItem1 = 317;
		static constexpr int comboBoxItem2 = 318;
		static constexpr int comboBoxItem3 = 319;
		static constexpr int textBlock12 = 320;
		static constexpr int composedScaleEditor = 321;
		static constexpr int textBlock13 = 322;
		static constexpr int authoredStateTree = 323;
		static constexpr int treeViewItem1 = 324;
		static constexpr int treeViewItem2 = 325;
		static constexpr int treeViewItem3 = 326;
		static constexpr int treeViewItem4 = 327;
		static constexpr int textBlock14 = 328;
		static constexpr int tabItem4 = 329;
		static constexpr int border4 = 330;
		static constexpr int analyticsSurface = 400;
		static constexpr int border5 = 410;
		static constexpr int analyticsFilterSurface = 401;
		static constexpr int analyticsQuery = 411;
		static constexpr int analyticsClosed = 412;
		static constexpr int analyticsContract = 413;
		static constexpr int analyticsHighMargin = 414;
		static constexpr int analyticsApply = 415;
		static constexpr int analyticsReset = 416;
		static constexpr int groupBox1 = 402;
		static constexpr int canvas4 = 417;
		static constexpr int textBlock15 = 418;
		static constexpr int textBlock16 = 419;
		static constexpr int groupBox2 = 403;
		static constexpr int canvas5 = 420;
		static constexpr int textBlock17 = 421;
		static constexpr int progressBar1 = 422;
		static constexpr int groupBox3 = 404;
		static constexpr int canvas6 = 423;
		static constexpr int textBlock18 = 424;
		static constexpr int textBlock19 = 425;
		static constexpr int chartBar = 405;
		static constexpr int chartPie = 406;
		static constexpr int chartLine = 407;
		static constexpr int salesChart = 408;
		static constexpr int analyticsReport = 409;
		static constexpr int canvas7 = 426;
		static constexpr int stackPanel2 = 427;
		static constexpr int textBlock20 = 428;
		static constexpr int textBlock21 = 429;
		static constexpr int textBlock22 = 430;
		static constexpr int textBlock23 = 431;
		static constexpr int textBlock24 = 432;
		static constexpr int analyticsRows = 433;
		static constexpr int textBlock25 = 434;
		static constexpr int tabItem5 = 435;
		static constexpr int border6 = 436;
		static constexpr int layoutSurface = 500;
		static constexpr int layoutTitle = 501;
		static constexpr int canvasSemanticsProbe = 530;
		static constexpr int border7 = 533;
		static constexpr int canvasLeftWins = 531;
		static constexpr int canvasRightBottom = 532;
		static constexpr int border8 = 534;
		static constexpr int demoStack = 502;
		static constexpr int stackA = 503;
		static constexpr int stackB = 504;
		static constexpr int stackC = 505;
		static constexpr int border9 = 535;
		static constexpr int demoGrid = 506;
		static constexpr int gridHeader = 507;
		static constexpr int gridLeft = 508;
		static constexpr int gridEditor = 509;
		static constexpr int gridFooter = 510;
		static constexpr int border10 = 536;
		static constexpr int demoDock = 511;
		static constexpr int dockTop = 512;
		static constexpr int dockLeft = 513;
		static constexpr int dockFill = 514;
		static constexpr int border11 = 537;
		static constexpr int demoWrap = 515;
		static constexpr int wrap1 = 516;
		static constexpr int wrap2 = 517;
		static constexpr int wrap3 = 518;
		static constexpr int wrap4 = 519;
		static constexpr int wrap5 = 520;
		static constexpr int wrap6 = 521;
		static constexpr int border12 = 538;
		static constexpr int demoRelative = 522;
		static constexpr int relativeCenter = 523;
		static constexpr int naturalTextProbe = 539;
		static constexpr int wrappedTextProbe = 540;
		static constexpr int trimmedTextProbe = 541;
		static constexpr int relativeCenterButton = 542;
		static constexpr int border13 = 543;
		static constexpr int demoScroll = 524;
		static constexpr int demoScrollContent = 544;
		static constexpr int border14 = 545;
		static constexpr int scrollCard1 = 525;
		static constexpr int scrollCard1Text = 526;
		static constexpr int border15 = 546;
		static constexpr int scrollCard2 = 527;
		static constexpr int scrollCard2Text = 528;
		static constexpr int farButton = 529;
		static constexpr int tabItem6 = 547;
		static constexpr int border16 = 548;
		static constexpr int systemSurface = 600;
		static constexpr int systemTitle = 601;
		static constexpr int notifyToggle = 602;
		static constexpr int notifyBalloon = 603;
		static constexpr int showDialog = 604;
		static constexpr int showToast = 605;
		static constexpr int systemHint = 606;
		static constexpr int border17 = 609;
		static constexpr int canvas8 = 610;
		static constexpr int textBlock26 = 611;
		static constexpr int commandTargetButton = 612;
		static constexpr int textBlock27 = 613;
		static constexpr int commandTargetTrace = 614;
		static constexpr int textBlock28 = 615;
		static constexpr int textBlock29 = 616;
		static constexpr int textBlock30 = 617;
		static constexpr int notificationPanel = 607;
		static constexpr int canvas9 = 618;
		static constexpr int textBlock31 = 619;
		static constexpr int toastMessage = 620;
		static constexpr int progressBar2 = 621;
		static constexpr int dismissToast = 622;
		static constexpr int textBlock32 = 623;
		static constexpr int tabItem7 = 624;
		static constexpr int border18 = 625;
		static constexpr int webSurface = 700;
		static constexpr int invokeWeb = 701;
		static constexpr int webHint = 702;
		static constexpr int border19 = 704;
		static constexpr int webBrowser = 703;
		static constexpr int tabItem8 = 705;
		static constexpr int border20 = 706;
		static constexpr int mediaSurface = 800;
		static constexpr int mediaPlayer = 801;
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
		static constexpr int mediaProgress = 812;
		static constexpr int mediaTime = 813;
		static constexpr int tabItem9 = 814;
		static constexpr int border21 = 815;
		static constexpr int wpfLabSurface = 816;
		static constexpr int wpfLabTitle = 817;
		static constexpr int wpfBindingScope = 818;
		static constexpr int stackPanel3 = 819;
		static constexpr int wpfTypographyOverride = 820;
		static constexpr int wpfTwoWayEditor = 821;
		static constexpr int wpfElementMirror = 822;
		static constexpr int wpfSelfValue = 823;
		static constexpr int wpfAncestorValue = 824;
		static constexpr int wpfFallbackValue = 825;
		static constexpr int wpfNullValue = 826;
		static constexpr int wpfIndexerValue = 827;
		static constexpr int wpfKeyedIndexerValue = 828;
		static constexpr int wpfConvertedValue = 829;
		static constexpr int wpfMultiValue = 830;
		static constexpr int wpfTemplateAndStyleScope = 831;
		static constexpr int textBlock33 = 832;
		static constexpr int wpfTemplateButton = 833;
		static constexpr int wpfTriggerButton = 834;
		static constexpr int wpfScopeResourceValue = 835;
		static constexpr int wpfInnerResourceScope = 836;
		static constexpr int wpfInnerResourceValue = 837;
		static constexpr int textBlock34 = 838;
		static constexpr int wpfItemsScope = 839;
		static constexpr int textBlock35 = 840;
		static constexpr int wpfTemplateList = 841;
		static constexpr int wpfRouteOuter = 842;
		static constexpr int canvas10 = 843;
		static constexpr int textBlock36 = 844;
		static constexpr int wpfRouteMiddle = 845;
		static constexpr int wpfRouteSource = 846;
		static constexpr int wpfFocusPeerB = 847;
		static constexpr int wpfFocusPeerC = 848;
		static constexpr int wpfNoFocusPeer = 849;
		static constexpr int wpfTextInputSource = 850;
		static constexpr int wpfRouteTrace = 851;
		static constexpr int wpfInputStats = 852;
		static constexpr int wpfHierarchyScope = 853;
		static constexpr int wpfHierarchyChain = 854;
		static constexpr int wpfDispatcherProbe = 855;
		static constexpr int wpfDispatcherResult = 856;
		static constexpr int tabItem10 = 857;
		static constexpr int border22 = 858;
		static constexpr int textCompositionLabSurface = 859;
		static constexpr int textBlock37 = 860;
		static constexpr int border23 = 861;
		static constexpr int canvas11 = 862;
		static constexpr int textBlock38 = 863;
		static constexpr int textBlock39 = 864;
		static constexpr int compositionTextBox = 865;
		static constexpr int textBlock40 = 866;
		static constexpr int compositionRichTextBox = 867;
		static constexpr int textBlock41 = 868;
		static constexpr int compositionPasswordBox = 869;
		static constexpr int textBlock42 = 870;
		static constexpr int border24 = 871;
		static constexpr int canvas12 = 872;
		static constexpr int textBlock43 = 873;
		static constexpr int compositionStartProbe = 874;
		static constexpr int compositionUpdateProbe = 875;
		static constexpr int compositionCommitProbe = 876;
		static constexpr int compositionCancelProbe = 877;
		static constexpr int compositionSurrogateProbe = 878;
		static constexpr int compositionUnicharProbe = 879;
		static constexpr int compositionFocusProbe = 880;
		static constexpr int compositionPreviewHandledProbe = 881;
		static constexpr int compositionResetProbe = 882;
		static constexpr int compositionState = 883;
		static constexpr int compositionStats = 884;
		static constexpr int border25 = 885;
		static constexpr int canvas13 = 886;
		static constexpr int textBlock44 = 887;
		static constexpr int compositionTrace = 888;
		static constexpr int tabItem11 = 889;
		static constexpr int border26 = 890;
		static constexpr int presentationLabSurface = 891;
		static constexpr int textBlock45 = 892;
		static constexpr int presentationProbeSurface = 893;
		static constexpr int presentationTopologyTile = 894;
		static constexpr int canvas14 = 895;
		static constexpr int textBlock46 = 896;
		static constexpr int textBlock47 = 897;
		static constexpr int textBlock48 = 898;
		static constexpr int textBlock49 = 899;
		static constexpr int textBlock50 = 901;
		static constexpr int textBlock51 = 902;
		static constexpr int presentationRegionButton = 903;
		static constexpr int presentationGeometryButton = 904;
		static constexpr int presentationCompositionButton = 905;
		static constexpr int presentationFullButton = 906;
		static constexpr int presentationTopologyButton = 907;
		static constexpr int presentationDeviceLossButton = 908;
		static constexpr int presentationStatus = 909;
		static constexpr int textBlock52 = 910;
		static constexpr int systemContextMenu = 608;
		static constexpr int menuItem6 = 911;
		static constexpr int menuItem7 = 912;
		static constexpr int separator2 = 913;
		static constexpr int menuItem8 = 914;
		static constexpr int menuItem9 = 915;
		static constexpr int menuItem10 = 916;
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
	[[nodiscard]] Canvas* GetToolSeparator() noexcept { return toolSeparator; }
	[[nodiscard]] const Canvas* GetToolSeparator() const noexcept { return toolSeparator; }
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
	[[nodiscard]] Canvas* GetCanvas1() noexcept { return canvas1; }
	[[nodiscard]] const Canvas* GetCanvas1() const noexcept { return canvas1; }
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
	[[nodiscard]] Border* GetBorder1() noexcept { return border1; }
	[[nodiscard]] const Border* GetBorder1() const noexcept { return border1; }
	[[nodiscard]] Canvas* GetBasicSurface() noexcept { return basicSurface; }
	[[nodiscard]] const Canvas* GetBasicSurface() const noexcept { return basicSurface; }
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
	[[nodiscard]] Label* GetTextBlock1() noexcept { return textBlock1; }
	[[nodiscard]] const Label* GetTextBlock1() const noexcept { return textBlock1; }
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
	[[nodiscard]] Canvas* GetBasicGroupContent() noexcept { return basicGroupContent; }
	[[nodiscard]] const Canvas* GetBasicGroupContent() const noexcept { return basicGroupContent; }
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
	[[nodiscard]] Canvas* GetBasicExpanderContent() noexcept { return basicExpanderContent; }
	[[nodiscard]] const Canvas* GetBasicExpanderContent() const noexcept { return basicExpanderContent; }
	[[nodiscard]] Label* GetExpanderText() noexcept { return expanderText; }
	[[nodiscard]] const Label* GetExpanderText() const noexcept { return expanderText; }
	[[nodiscard]] ContentControl* GetThemeContentControlProbe() noexcept { return themeContentControlProbe; }
	[[nodiscard]] const ContentControl* GetThemeContentControlProbe() const noexcept { return themeContentControlProbe; }
	[[nodiscard]] ItemsControl* GetThemeItemsControlProbe() noexcept { return themeItemsControlProbe; }
	[[nodiscard]] const ItemsControl* GetThemeItemsControlProbe() const noexcept { return themeItemsControlProbe; }
	[[nodiscard]] Label* GetTextBlock2() noexcept { return textBlock2; }
	[[nodiscard]] const Label* GetTextBlock2() const noexcept { return textBlock2; }
	[[nodiscard]] Separator* GetThemeSeparatorProbe() noexcept { return themeSeparatorProbe; }
	[[nodiscard]] const Separator* GetThemeSeparatorProbe() const noexcept { return themeSeparatorProbe; }
	[[nodiscard]] TabItem* GetTabItem2() noexcept { return tabItem2; }
	[[nodiscard]] const TabItem* GetTabItem2() const noexcept { return tabItem2; }
	[[nodiscard]] Border* GetBorder2() noexcept { return border2; }
	[[nodiscard]] const Border* GetBorder2() const noexcept { return border2; }
	[[nodiscard]] Canvas* GetContainerSurface() noexcept { return containerSurface; }
	[[nodiscard]] const Canvas* GetContainerSurface() const noexcept { return containerSurface; }
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
	[[nodiscard]] Canvas* GetNavigationComposition() noexcept { return navigationComposition; }
	[[nodiscard]] const Canvas* GetNavigationComposition() const noexcept { return navigationComposition; }
	[[nodiscard]] Label* GetTextBlock3() noexcept { return textBlock3; }
	[[nodiscard]] const Label* GetTextBlock3() const noexcept { return textBlock3; }
	[[nodiscard]] ListBox* GetSideNavigationList() noexcept { return sideNavigationList; }
	[[nodiscard]] const ListBox* GetSideNavigationList() const noexcept { return sideNavigationList; }
	[[nodiscard]] Canvas* GetCanvas2() noexcept { return canvas2; }
	[[nodiscard]] const Canvas* GetCanvas2() const noexcept { return canvas2; }
	[[nodiscard]] Canvas* GetDetailComposition() noexcept { return detailComposition; }
	[[nodiscard]] const Canvas* GetDetailComposition() const noexcept { return detailComposition; }
	[[nodiscard]] StackPanel* GetStackPanel1() noexcept { return stackPanel1; }
	[[nodiscard]] const StackPanel* GetStackPanel1() const noexcept { return stackPanel1; }
	[[nodiscard]] Label* GetTextBlock4() noexcept { return textBlock4; }
	[[nodiscard]] const Label* GetTextBlock4() const noexcept { return textBlock4; }
	[[nodiscard]] Label* GetTextBlock5() noexcept { return textBlock5; }
	[[nodiscard]] const Label* GetTextBlock5() const noexcept { return textBlock5; }
	[[nodiscard]] Label* GetTextBlock6() noexcept { return textBlock6; }
	[[nodiscard]] const Label* GetTextBlock6() const noexcept { return textBlock6; }
	[[nodiscard]] Label* GetTextBlock7() noexcept { return textBlock7; }
	[[nodiscard]] const Label* GetTextBlock7() const noexcept { return textBlock7; }
	[[nodiscard]] Label* GetTextBlock8() noexcept { return textBlock8; }
	[[nodiscard]] const Label* GetTextBlock8() const noexcept { return textBlock8; }
	[[nodiscard]] RichTextBox* GetSplitNotes() noexcept { return splitNotes; }
	[[nodiscard]] const RichTextBox* GetSplitNotes() const noexcept { return splitNotes; }
	[[nodiscard]] GroupBox* GetContainerGroup() noexcept { return containerGroup; }
	[[nodiscard]] const GroupBox* GetContainerGroup() const noexcept { return containerGroup; }
	[[nodiscard]] Label* GetContainerGroupText() noexcept { return containerGroupText; }
	[[nodiscard]] const Label* GetContainerGroupText() const noexcept { return containerGroupText; }
	[[nodiscard]] TabItem* GetTabItem3() noexcept { return tabItem3; }
	[[nodiscard]] const TabItem* GetTabItem3() const noexcept { return tabItem3; }
	[[nodiscard]] Border* GetBorder3() noexcept { return border3; }
	[[nodiscard]] const Border* GetBorder3() const noexcept { return border3; }
	[[nodiscard]] Canvas* GetDataSurface() noexcept { return dataSurface; }
	[[nodiscard]] const Canvas* GetDataSurface() const noexcept { return dataSurface; }
	[[nodiscard]] TreeView* GetDemoTree() noexcept { return demoTree; }
	[[nodiscard]] const TreeView* GetDemoTree() const noexcept { return demoTree; }
	[[nodiscard]] ListBox* GetDemoListBox() noexcept { return demoListBox; }
	[[nodiscard]] const ListBox* GetDemoListBox() const noexcept { return demoListBox; }
	[[nodiscard]] ListView* GetDemoList() noexcept { return demoList; }
	[[nodiscard]] const ListView* GetDemoList() const noexcept { return demoList; }
	[[nodiscard]] GroupBox* GetComposedPropertyEditor() noexcept { return composedPropertyEditor; }
	[[nodiscard]] const GroupBox* GetComposedPropertyEditor() const noexcept { return composedPropertyEditor; }
	[[nodiscard]] Canvas* GetCanvas3() noexcept { return canvas3; }
	[[nodiscard]] const Canvas* GetCanvas3() const noexcept { return canvas3; }
	[[nodiscard]] Label* GetTextBlock9() noexcept { return textBlock9; }
	[[nodiscard]] const Label* GetTextBlock9() const noexcept { return textBlock9; }
	[[nodiscard]] TextBox* GetComposedTitleEditor() noexcept { return composedTitleEditor; }
	[[nodiscard]] const TextBox* GetComposedTitleEditor() const noexcept { return composedTitleEditor; }
	[[nodiscard]] Label* GetTextBlock10() noexcept { return textBlock10; }
	[[nodiscard]] const Label* GetTextBlock10() const noexcept { return textBlock10; }
	[[nodiscard]] CheckBox* GetComposedEnabledEditor() noexcept { return composedEnabledEditor; }
	[[nodiscard]] const CheckBox* GetComposedEnabledEditor() const noexcept { return composedEnabledEditor; }
	[[nodiscard]] Label* GetTextBlock11() noexcept { return textBlock11; }
	[[nodiscard]] const Label* GetTextBlock11() const noexcept { return textBlock11; }
	[[nodiscard]] ComboBox* GetComposedDensityEditor() noexcept { return composedDensityEditor; }
	[[nodiscard]] const ComboBox* GetComposedDensityEditor() const noexcept { return composedDensityEditor; }
	[[nodiscard]] ComboBoxItem* GetComboBoxItem1() noexcept { return comboBoxItem1; }
	[[nodiscard]] const ComboBoxItem* GetComboBoxItem1() const noexcept { return comboBoxItem1; }
	[[nodiscard]] ComboBoxItem* GetComboBoxItem2() noexcept { return comboBoxItem2; }
	[[nodiscard]] const ComboBoxItem* GetComboBoxItem2() const noexcept { return comboBoxItem2; }
	[[nodiscard]] ComboBoxItem* GetComboBoxItem3() noexcept { return comboBoxItem3; }
	[[nodiscard]] const ComboBoxItem* GetComboBoxItem3() const noexcept { return comboBoxItem3; }
	[[nodiscard]] Label* GetTextBlock12() noexcept { return textBlock12; }
	[[nodiscard]] const Label* GetTextBlock12() const noexcept { return textBlock12; }
	[[nodiscard]] Slider* GetComposedScaleEditor() noexcept { return composedScaleEditor; }
	[[nodiscard]] const Slider* GetComposedScaleEditor() const noexcept { return composedScaleEditor; }
	[[nodiscard]] Label* GetTextBlock13() noexcept { return textBlock13; }
	[[nodiscard]] const Label* GetTextBlock13() const noexcept { return textBlock13; }
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
	[[nodiscard]] Label* GetTextBlock14() noexcept { return textBlock14; }
	[[nodiscard]] const Label* GetTextBlock14() const noexcept { return textBlock14; }
	[[nodiscard]] TabItem* GetTabItem4() noexcept { return tabItem4; }
	[[nodiscard]] const TabItem* GetTabItem4() const noexcept { return tabItem4; }
	[[nodiscard]] Border* GetBorder4() noexcept { return border4; }
	[[nodiscard]] const Border* GetBorder4() const noexcept { return border4; }
	[[nodiscard]] Canvas* GetAnalyticsSurface() noexcept { return analyticsSurface; }
	[[nodiscard]] const Canvas* GetAnalyticsSurface() const noexcept { return analyticsSurface; }
	[[nodiscard]] Border* GetBorder5() noexcept { return border5; }
	[[nodiscard]] const Border* GetBorder5() const noexcept { return border5; }
	[[nodiscard]] Canvas* GetAnalyticsFilterSurface() noexcept { return analyticsFilterSurface; }
	[[nodiscard]] const Canvas* GetAnalyticsFilterSurface() const noexcept { return analyticsFilterSurface; }
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
	[[nodiscard]] GroupBox* GetGroupBox1() noexcept { return groupBox1; }
	[[nodiscard]] const GroupBox* GetGroupBox1() const noexcept { return groupBox1; }
	[[nodiscard]] Canvas* GetCanvas4() noexcept { return canvas4; }
	[[nodiscard]] const Canvas* GetCanvas4() const noexcept { return canvas4; }
	[[nodiscard]] Label* GetTextBlock15() noexcept { return textBlock15; }
	[[nodiscard]] const Label* GetTextBlock15() const noexcept { return textBlock15; }
	[[nodiscard]] Label* GetTextBlock16() noexcept { return textBlock16; }
	[[nodiscard]] const Label* GetTextBlock16() const noexcept { return textBlock16; }
	[[nodiscard]] GroupBox* GetGroupBox2() noexcept { return groupBox2; }
	[[nodiscard]] const GroupBox* GetGroupBox2() const noexcept { return groupBox2; }
	[[nodiscard]] Canvas* GetCanvas5() noexcept { return canvas5; }
	[[nodiscard]] const Canvas* GetCanvas5() const noexcept { return canvas5; }
	[[nodiscard]] Label* GetTextBlock17() noexcept { return textBlock17; }
	[[nodiscard]] const Label* GetTextBlock17() const noexcept { return textBlock17; }
	[[nodiscard]] ProgressBar* GetProgressBar1() noexcept { return progressBar1; }
	[[nodiscard]] const ProgressBar* GetProgressBar1() const noexcept { return progressBar1; }
	[[nodiscard]] GroupBox* GetGroupBox3() noexcept { return groupBox3; }
	[[nodiscard]] const GroupBox* GetGroupBox3() const noexcept { return groupBox3; }
	[[nodiscard]] Canvas* GetCanvas6() noexcept { return canvas6; }
	[[nodiscard]] const Canvas* GetCanvas6() const noexcept { return canvas6; }
	[[nodiscard]] Label* GetTextBlock18() noexcept { return textBlock18; }
	[[nodiscard]] const Label* GetTextBlock18() const noexcept { return textBlock18; }
	[[nodiscard]] Label* GetTextBlock19() noexcept { return textBlock19; }
	[[nodiscard]] const Label* GetTextBlock19() const noexcept { return textBlock19; }
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
	[[nodiscard]] Canvas* GetCanvas7() noexcept { return canvas7; }
	[[nodiscard]] const Canvas* GetCanvas7() const noexcept { return canvas7; }
	[[nodiscard]] StackPanel* GetStackPanel2() noexcept { return stackPanel2; }
	[[nodiscard]] const StackPanel* GetStackPanel2() const noexcept { return stackPanel2; }
	[[nodiscard]] Label* GetTextBlock20() noexcept { return textBlock20; }
	[[nodiscard]] const Label* GetTextBlock20() const noexcept { return textBlock20; }
	[[nodiscard]] Label* GetTextBlock21() noexcept { return textBlock21; }
	[[nodiscard]] const Label* GetTextBlock21() const noexcept { return textBlock21; }
	[[nodiscard]] Label* GetTextBlock22() noexcept { return textBlock22; }
	[[nodiscard]] const Label* GetTextBlock22() const noexcept { return textBlock22; }
	[[nodiscard]] Label* GetTextBlock23() noexcept { return textBlock23; }
	[[nodiscard]] const Label* GetTextBlock23() const noexcept { return textBlock23; }
	[[nodiscard]] Label* GetTextBlock24() noexcept { return textBlock24; }
	[[nodiscard]] const Label* GetTextBlock24() const noexcept { return textBlock24; }
	[[nodiscard]] ListView* GetAnalyticsRows() noexcept { return analyticsRows; }
	[[nodiscard]] const ListView* GetAnalyticsRows() const noexcept { return analyticsRows; }
	[[nodiscard]] Label* GetTextBlock25() noexcept { return textBlock25; }
	[[nodiscard]] const Label* GetTextBlock25() const noexcept { return textBlock25; }
	[[nodiscard]] TabItem* GetTabItem5() noexcept { return tabItem5; }
	[[nodiscard]] const TabItem* GetTabItem5() const noexcept { return tabItem5; }
	[[nodiscard]] Border* GetBorder6() noexcept { return border6; }
	[[nodiscard]] const Border* GetBorder6() const noexcept { return border6; }
	[[nodiscard]] Canvas* GetLayoutSurface() noexcept { return layoutSurface; }
	[[nodiscard]] const Canvas* GetLayoutSurface() const noexcept { return layoutSurface; }
	[[nodiscard]] Label* GetLayoutTitle() noexcept { return layoutTitle; }
	[[nodiscard]] const Label* GetLayoutTitle() const noexcept { return layoutTitle; }
	[[nodiscard]] Canvas* GetCanvasSemanticsProbe() noexcept { return canvasSemanticsProbe; }
	[[nodiscard]] const Canvas* GetCanvasSemanticsProbe() const noexcept { return canvasSemanticsProbe; }
	[[nodiscard]] Border* GetBorder7() noexcept { return border7; }
	[[nodiscard]] const Border* GetBorder7() const noexcept { return border7; }
	[[nodiscard]] Label* GetCanvasLeftWins() noexcept { return canvasLeftWins; }
	[[nodiscard]] const Label* GetCanvasLeftWins() const noexcept { return canvasLeftWins; }
	[[nodiscard]] Label* GetCanvasRightBottom() noexcept { return canvasRightBottom; }
	[[nodiscard]] const Label* GetCanvasRightBottom() const noexcept { return canvasRightBottom; }
	[[nodiscard]] Border* GetBorder8() noexcept { return border8; }
	[[nodiscard]] const Border* GetBorder8() const noexcept { return border8; }
	[[nodiscard]] StackPanel* GetDemoStack() noexcept { return demoStack; }
	[[nodiscard]] const StackPanel* GetDemoStack() const noexcept { return demoStack; }
	[[nodiscard]] Button* GetStackA() noexcept { return stackA; }
	[[nodiscard]] const Button* GetStackA() const noexcept { return stackA; }
	[[nodiscard]] Button* GetStackB() noexcept { return stackB; }
	[[nodiscard]] const Button* GetStackB() const noexcept { return stackB; }
	[[nodiscard]] Button* GetStackC() noexcept { return stackC; }
	[[nodiscard]] const Button* GetStackC() const noexcept { return stackC; }
	[[nodiscard]] Border* GetBorder9() noexcept { return border9; }
	[[nodiscard]] const Border* GetBorder9() const noexcept { return border9; }
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
	[[nodiscard]] Border* GetBorder10() noexcept { return border10; }
	[[nodiscard]] const Border* GetBorder10() const noexcept { return border10; }
	[[nodiscard]] DockPanel* GetDemoDock() noexcept { return demoDock; }
	[[nodiscard]] const DockPanel* GetDemoDock() const noexcept { return demoDock; }
	[[nodiscard]] Button* GetDockTop() noexcept { return dockTop; }
	[[nodiscard]] const Button* GetDockTop() const noexcept { return dockTop; }
	[[nodiscard]] Button* GetDockLeft() noexcept { return dockLeft; }
	[[nodiscard]] const Button* GetDockLeft() const noexcept { return dockLeft; }
	[[nodiscard]] Label* GetDockFill() noexcept { return dockFill; }
	[[nodiscard]] const Label* GetDockFill() const noexcept { return dockFill; }
	[[nodiscard]] Border* GetBorder11() noexcept { return border11; }
	[[nodiscard]] const Border* GetBorder11() const noexcept { return border11; }
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
	[[nodiscard]] Border* GetBorder12() noexcept { return border12; }
	[[nodiscard]] const Border* GetBorder12() const noexcept { return border12; }
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
	[[nodiscard]] Border* GetBorder13() noexcept { return border13; }
	[[nodiscard]] const Border* GetBorder13() const noexcept { return border13; }
	[[nodiscard]] ScrollViewer* GetDemoScroll() noexcept { return demoScroll; }
	[[nodiscard]] const ScrollViewer* GetDemoScroll() const noexcept { return demoScroll; }
	[[nodiscard]] Canvas* GetDemoScrollContent() noexcept { return demoScrollContent; }
	[[nodiscard]] const Canvas* GetDemoScrollContent() const noexcept { return demoScrollContent; }
	[[nodiscard]] Border* GetBorder14() noexcept { return border14; }
	[[nodiscard]] const Border* GetBorder14() const noexcept { return border14; }
	[[nodiscard]] Canvas* GetScrollCard1() noexcept { return scrollCard1; }
	[[nodiscard]] const Canvas* GetScrollCard1() const noexcept { return scrollCard1; }
	[[nodiscard]] Label* GetScrollCard1Text() noexcept { return scrollCard1Text; }
	[[nodiscard]] const Label* GetScrollCard1Text() const noexcept { return scrollCard1Text; }
	[[nodiscard]] Border* GetBorder15() noexcept { return border15; }
	[[nodiscard]] const Border* GetBorder15() const noexcept { return border15; }
	[[nodiscard]] Canvas* GetScrollCard2() noexcept { return scrollCard2; }
	[[nodiscard]] const Canvas* GetScrollCard2() const noexcept { return scrollCard2; }
	[[nodiscard]] Label* GetScrollCard2Text() noexcept { return scrollCard2Text; }
	[[nodiscard]] const Label* GetScrollCard2Text() const noexcept { return scrollCard2Text; }
	[[nodiscard]] Button* GetFarButton() noexcept { return farButton; }
	[[nodiscard]] const Button* GetFarButton() const noexcept { return farButton; }
	[[nodiscard]] TabItem* GetTabItem6() noexcept { return tabItem6; }
	[[nodiscard]] const TabItem* GetTabItem6() const noexcept { return tabItem6; }
	[[nodiscard]] Border* GetBorder16() noexcept { return border16; }
	[[nodiscard]] const Border* GetBorder16() const noexcept { return border16; }
	[[nodiscard]] Canvas* GetSystemSurface() noexcept { return systemSurface; }
	[[nodiscard]] const Canvas* GetSystemSurface() const noexcept { return systemSurface; }
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
	[[nodiscard]] Border* GetBorder17() noexcept { return border17; }
	[[nodiscard]] const Border* GetBorder17() const noexcept { return border17; }
	[[nodiscard]] Canvas* GetCanvas8() noexcept { return canvas8; }
	[[nodiscard]] const Canvas* GetCanvas8() const noexcept { return canvas8; }
	[[nodiscard]] Label* GetTextBlock26() noexcept { return textBlock26; }
	[[nodiscard]] const Label* GetTextBlock26() const noexcept { return textBlock26; }
	[[nodiscard]] Button* GetCommandTargetButton() noexcept { return commandTargetButton; }
	[[nodiscard]] const Button* GetCommandTargetButton() const noexcept { return commandTargetButton; }
	[[nodiscard]] Label* GetTextBlock27() noexcept { return textBlock27; }
	[[nodiscard]] const Label* GetTextBlock27() const noexcept { return textBlock27; }
	[[nodiscard]] Label* GetCommandTargetTrace() noexcept { return commandTargetTrace; }
	[[nodiscard]] const Label* GetCommandTargetTrace() const noexcept { return commandTargetTrace; }
	[[nodiscard]] Label* GetTextBlock28() noexcept { return textBlock28; }
	[[nodiscard]] const Label* GetTextBlock28() const noexcept { return textBlock28; }
	[[nodiscard]] Label* GetTextBlock29() noexcept { return textBlock29; }
	[[nodiscard]] const Label* GetTextBlock29() const noexcept { return textBlock29; }
	[[nodiscard]] Label* GetTextBlock30() noexcept { return textBlock30; }
	[[nodiscard]] const Label* GetTextBlock30() const noexcept { return textBlock30; }
	[[nodiscard]] GroupBox* GetNotificationPanel() noexcept { return notificationPanel; }
	[[nodiscard]] const GroupBox* GetNotificationPanel() const noexcept { return notificationPanel; }
	[[nodiscard]] Canvas* GetCanvas9() noexcept { return canvas9; }
	[[nodiscard]] const Canvas* GetCanvas9() const noexcept { return canvas9; }
	[[nodiscard]] Label* GetTextBlock31() noexcept { return textBlock31; }
	[[nodiscard]] const Label* GetTextBlock31() const noexcept { return textBlock31; }
	[[nodiscard]] Label* GetToastMessage() noexcept { return toastMessage; }
	[[nodiscard]] const Label* GetToastMessage() const noexcept { return toastMessage; }
	[[nodiscard]] ProgressBar* GetProgressBar2() noexcept { return progressBar2; }
	[[nodiscard]] const ProgressBar* GetProgressBar2() const noexcept { return progressBar2; }
	[[nodiscard]] Button* GetDismissToast() noexcept { return dismissToast; }
	[[nodiscard]] const Button* GetDismissToast() const noexcept { return dismissToast; }
	[[nodiscard]] Label* GetTextBlock32() noexcept { return textBlock32; }
	[[nodiscard]] const Label* GetTextBlock32() const noexcept { return textBlock32; }
	[[nodiscard]] TabItem* GetTabItem7() noexcept { return tabItem7; }
	[[nodiscard]] const TabItem* GetTabItem7() const noexcept { return tabItem7; }
	[[nodiscard]] Border* GetBorder18() noexcept { return border18; }
	[[nodiscard]] const Border* GetBorder18() const noexcept { return border18; }
	[[nodiscard]] Canvas* GetWebSurface() noexcept { return webSurface; }
	[[nodiscard]] const Canvas* GetWebSurface() const noexcept { return webSurface; }
	[[nodiscard]] Button* GetInvokeWeb() noexcept { return invokeWeb; }
	[[nodiscard]] const Button* GetInvokeWeb() const noexcept { return invokeWeb; }
	[[nodiscard]] Label* GetWebHint() noexcept { return webHint; }
	[[nodiscard]] const Label* GetWebHint() const noexcept { return webHint; }
	[[nodiscard]] Border* GetBorder19() noexcept { return border19; }
	[[nodiscard]] const Border* GetBorder19() const noexcept { return border19; }
	[[nodiscard]] WebBrowser* GetWebBrowser() noexcept { return webBrowser; }
	[[nodiscard]] const WebBrowser* GetWebBrowser() const noexcept { return webBrowser; }
	[[nodiscard]] TabItem* GetTabItem8() noexcept { return tabItem8; }
	[[nodiscard]] const TabItem* GetTabItem8() const noexcept { return tabItem8; }
	[[nodiscard]] Border* GetBorder20() noexcept { return border20; }
	[[nodiscard]] const Border* GetBorder20() const noexcept { return border20; }
	[[nodiscard]] Canvas* GetMediaSurface() noexcept { return mediaSurface; }
	[[nodiscard]] const Canvas* GetMediaSurface() const noexcept { return mediaSurface; }
	[[nodiscard]] MediaPlayer* GetMediaPlayer() noexcept { return mediaPlayer; }
	[[nodiscard]] const MediaPlayer* GetMediaPlayer() const noexcept { return mediaPlayer; }
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
	[[nodiscard]] TabItem* GetTabItem9() noexcept { return tabItem9; }
	[[nodiscard]] const TabItem* GetTabItem9() const noexcept { return tabItem9; }
	[[nodiscard]] Border* GetBorder21() noexcept { return border21; }
	[[nodiscard]] const Border* GetBorder21() const noexcept { return border21; }
	[[nodiscard]] Canvas* GetWpfLabSurface() noexcept { return wpfLabSurface; }
	[[nodiscard]] const Canvas* GetWpfLabSurface() const noexcept { return wpfLabSurface; }
	[[nodiscard]] Label* GetWpfLabTitle() noexcept { return wpfLabTitle; }
	[[nodiscard]] const Label* GetWpfLabTitle() const noexcept { return wpfLabTitle; }
	[[nodiscard]] ContentControl* GetWpfBindingScope() noexcept { return wpfBindingScope; }
	[[nodiscard]] const ContentControl* GetWpfBindingScope() const noexcept { return wpfBindingScope; }
	[[nodiscard]] StackPanel* GetStackPanel3() noexcept { return stackPanel3; }
	[[nodiscard]] const StackPanel* GetStackPanel3() const noexcept { return stackPanel3; }
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
	[[nodiscard]] Canvas* GetWpfTemplateAndStyleScope() noexcept { return wpfTemplateAndStyleScope; }
	[[nodiscard]] const Canvas* GetWpfTemplateAndStyleScope() const noexcept { return wpfTemplateAndStyleScope; }
	[[nodiscard]] Label* GetTextBlock33() noexcept { return textBlock33; }
	[[nodiscard]] const Label* GetTextBlock33() const noexcept { return textBlock33; }
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
	[[nodiscard]] Label* GetTextBlock34() noexcept { return textBlock34; }
	[[nodiscard]] const Label* GetTextBlock34() const noexcept { return textBlock34; }
	[[nodiscard]] Canvas* GetWpfItemsScope() noexcept { return wpfItemsScope; }
	[[nodiscard]] const Canvas* GetWpfItemsScope() const noexcept { return wpfItemsScope; }
	[[nodiscard]] Label* GetTextBlock35() noexcept { return textBlock35; }
	[[nodiscard]] const Label* GetTextBlock35() const noexcept { return textBlock35; }
	[[nodiscard]] ListBox* GetWpfTemplateList() noexcept { return wpfTemplateList; }
	[[nodiscard]] const ListBox* GetWpfTemplateList() const noexcept { return wpfTemplateList; }
	[[nodiscard]] Border* GetWpfRouteOuter() noexcept { return wpfRouteOuter; }
	[[nodiscard]] const Border* GetWpfRouteOuter() const noexcept { return wpfRouteOuter; }
	[[nodiscard]] Canvas* GetCanvas10() noexcept { return canvas10; }
	[[nodiscard]] const Canvas* GetCanvas10() const noexcept { return canvas10; }
	[[nodiscard]] Label* GetTextBlock36() noexcept { return textBlock36; }
	[[nodiscard]] const Label* GetTextBlock36() const noexcept { return textBlock36; }
	[[nodiscard]] Canvas* GetWpfRouteMiddle() noexcept { return wpfRouteMiddle; }
	[[nodiscard]] const Canvas* GetWpfRouteMiddle() const noexcept { return wpfRouteMiddle; }
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
	[[nodiscard]] Canvas* GetWpfHierarchyScope() noexcept { return wpfHierarchyScope; }
	[[nodiscard]] const Canvas* GetWpfHierarchyScope() const noexcept { return wpfHierarchyScope; }
	[[nodiscard]] Label* GetWpfHierarchyChain() noexcept { return wpfHierarchyChain; }
	[[nodiscard]] const Label* GetWpfHierarchyChain() const noexcept { return wpfHierarchyChain; }
	[[nodiscard]] Button* GetWpfDispatcherProbe() noexcept { return wpfDispatcherProbe; }
	[[nodiscard]] const Button* GetWpfDispatcherProbe() const noexcept { return wpfDispatcherProbe; }
	[[nodiscard]] Label* GetWpfDispatcherResult() noexcept { return wpfDispatcherResult; }
	[[nodiscard]] const Label* GetWpfDispatcherResult() const noexcept { return wpfDispatcherResult; }
	[[nodiscard]] TabItem* GetTabItem10() noexcept { return tabItem10; }
	[[nodiscard]] const TabItem* GetTabItem10() const noexcept { return tabItem10; }
	[[nodiscard]] Border* GetBorder22() noexcept { return border22; }
	[[nodiscard]] const Border* GetBorder22() const noexcept { return border22; }
	[[nodiscard]] Canvas* GetTextCompositionLabSurface() noexcept { return textCompositionLabSurface; }
	[[nodiscard]] const Canvas* GetTextCompositionLabSurface() const noexcept { return textCompositionLabSurface; }
	[[nodiscard]] Label* GetTextBlock37() noexcept { return textBlock37; }
	[[nodiscard]] const Label* GetTextBlock37() const noexcept { return textBlock37; }
	[[nodiscard]] Border* GetBorder23() noexcept { return border23; }
	[[nodiscard]] const Border* GetBorder23() const noexcept { return border23; }
	[[nodiscard]] Canvas* GetCanvas11() noexcept { return canvas11; }
	[[nodiscard]] const Canvas* GetCanvas11() const noexcept { return canvas11; }
	[[nodiscard]] Label* GetTextBlock38() noexcept { return textBlock38; }
	[[nodiscard]] const Label* GetTextBlock38() const noexcept { return textBlock38; }
	[[nodiscard]] Label* GetTextBlock39() noexcept { return textBlock39; }
	[[nodiscard]] const Label* GetTextBlock39() const noexcept { return textBlock39; }
	[[nodiscard]] TextBox* GetCompositionTextBox() noexcept { return compositionTextBox; }
	[[nodiscard]] const TextBox* GetCompositionTextBox() const noexcept { return compositionTextBox; }
	[[nodiscard]] Label* GetTextBlock40() noexcept { return textBlock40; }
	[[nodiscard]] const Label* GetTextBlock40() const noexcept { return textBlock40; }
	[[nodiscard]] RichTextBox* GetCompositionRichTextBox() noexcept { return compositionRichTextBox; }
	[[nodiscard]] const RichTextBox* GetCompositionRichTextBox() const noexcept { return compositionRichTextBox; }
	[[nodiscard]] Label* GetTextBlock41() noexcept { return textBlock41; }
	[[nodiscard]] const Label* GetTextBlock41() const noexcept { return textBlock41; }
	[[nodiscard]] PasswordBox* GetCompositionPasswordBox() noexcept { return compositionPasswordBox; }
	[[nodiscard]] const PasswordBox* GetCompositionPasswordBox() const noexcept { return compositionPasswordBox; }
	[[nodiscard]] Label* GetTextBlock42() noexcept { return textBlock42; }
	[[nodiscard]] const Label* GetTextBlock42() const noexcept { return textBlock42; }
	[[nodiscard]] Border* GetBorder24() noexcept { return border24; }
	[[nodiscard]] const Border* GetBorder24() const noexcept { return border24; }
	[[nodiscard]] Canvas* GetCanvas12() noexcept { return canvas12; }
	[[nodiscard]] const Canvas* GetCanvas12() const noexcept { return canvas12; }
	[[nodiscard]] Label* GetTextBlock43() noexcept { return textBlock43; }
	[[nodiscard]] const Label* GetTextBlock43() const noexcept { return textBlock43; }
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
	[[nodiscard]] Border* GetBorder25() noexcept { return border25; }
	[[nodiscard]] const Border* GetBorder25() const noexcept { return border25; }
	[[nodiscard]] Canvas* GetCanvas13() noexcept { return canvas13; }
	[[nodiscard]] const Canvas* GetCanvas13() const noexcept { return canvas13; }
	[[nodiscard]] Label* GetTextBlock44() noexcept { return textBlock44; }
	[[nodiscard]] const Label* GetTextBlock44() const noexcept { return textBlock44; }
	[[nodiscard]] Label* GetCompositionTrace() noexcept { return compositionTrace; }
	[[nodiscard]] const Label* GetCompositionTrace() const noexcept { return compositionTrace; }
	[[nodiscard]] TabItem* GetTabItem11() noexcept { return tabItem11; }
	[[nodiscard]] const TabItem* GetTabItem11() const noexcept { return tabItem11; }
	[[nodiscard]] Border* GetBorder26() noexcept { return border26; }
	[[nodiscard]] const Border* GetBorder26() const noexcept { return border26; }
	[[nodiscard]] Canvas* GetPresentationLabSurface() noexcept { return presentationLabSurface; }
	[[nodiscard]] const Canvas* GetPresentationLabSurface() const noexcept { return presentationLabSurface; }
	[[nodiscard]] Label* GetTextBlock45() noexcept { return textBlock45; }
	[[nodiscard]] const Label* GetTextBlock45() const noexcept { return textBlock45; }
	[[nodiscard]] NativeSurface* GetPresentationProbeSurface() noexcept { return presentationProbeSurface; }
	[[nodiscard]] const NativeSurface* GetPresentationProbeSurface() const noexcept { return presentationProbeSurface; }
	[[nodiscard]] Label* GetPresentationTopologyTile() noexcept { return presentationTopologyTile; }
	[[nodiscard]] const Label* GetPresentationTopologyTile() const noexcept { return presentationTopologyTile; }
	[[nodiscard]] Canvas* GetCanvas14() noexcept { return canvas14; }
	[[nodiscard]] const Canvas* GetCanvas14() const noexcept { return canvas14; }
	[[nodiscard]] Label* GetTextBlock46() noexcept { return textBlock46; }
	[[nodiscard]] const Label* GetTextBlock46() const noexcept { return textBlock46; }
	[[nodiscard]] Label* GetTextBlock47() noexcept { return textBlock47; }
	[[nodiscard]] const Label* GetTextBlock47() const noexcept { return textBlock47; }
	[[nodiscard]] Label* GetTextBlock48() noexcept { return textBlock48; }
	[[nodiscard]] const Label* GetTextBlock48() const noexcept { return textBlock48; }
	[[nodiscard]] Label* GetTextBlock49() noexcept { return textBlock49; }
	[[nodiscard]] const Label* GetTextBlock49() const noexcept { return textBlock49; }
	[[nodiscard]] Label* GetTextBlock50() noexcept { return textBlock50; }
	[[nodiscard]] const Label* GetTextBlock50() const noexcept { return textBlock50; }
	[[nodiscard]] Label* GetTextBlock51() noexcept { return textBlock51; }
	[[nodiscard]] const Label* GetTextBlock51() const noexcept { return textBlock51; }
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
	[[nodiscard]] Label* GetTextBlock52() noexcept { return textBlock52; }
	[[nodiscard]] const Label* GetTextBlock52() const noexcept { return textBlock52; }
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
