#pragma once

/**
 * CUITest dynamic-XAML host.
 *
 * DemoWindow.cui.xaml owns the visual tree, layout, persistent properties,
 * styles, and event names. This class owns only runtime services, data, and
 * the C++ implementations registered for those event names.
 */
#include <CuiRuntime.h>
#include <Form.h>

#include <memory>
#include <string>
#include <vector>

class BitmapSource;
class Button;
class ChartView;
class ComboBox;
class ContextMenu;
class FilterBar;
class KpiCard;
class Label;
class MediaPlayer;
class Menu;
class NotifyIcon;
class PagedGridView;
class PictureBox;
class ProgressBar;
class ProgressRing;
class PropertyGridView;
class RadioBox;
class RelativePanel;
class ReportView;
class SideBar;
class Slider;
class StatusBar;
class Switch;
class TabControl;
class Taskbar;
class ToastHost;
class ToolBar;
class TreeView;
class WebBrowser;

class DemoWindow final : public Form
{
public:
	DemoWindow();
	~DemoWindow();

	static std::wstring XamlFilePath();
	static bool ValidateXaml(std::wstring* outError = nullptr);

private:
	template<typename T>
	T* RequireControl(const wchar_t* name);

	void RegisterXamlHandlers();
	void MountXaml();
	void ResolveControls();
	void LoadImages();
	void InitializeChrome();
	void InitializeBasicPage();
	void InitializeContainerPage();
	void InitializeDataPage();
	void InitializeAnalyticsPage();
	void InitializeLayoutPage();
	void InitializeSystemPage();
	void InitializeWebPage();
	void InitializeMediaPage();

	void UpdateStatus(const std::wstring& text);
	void UpdateProgress(float value01);
	void LoadPicture(const std::wstring& path);

	void HandleShown(Form* sender);
	void HandleClosing(Form* sender, bool& canceled);
	void HandleMenuCommand(Control* sender, int id);
	void HandleGlobalProgress(Control* sender, float oldValue, float newValue);
	void HandleMouseWheel(Control* sender, MouseEventArgs e);
	void HandleBasicClick(Control* sender, MouseEventArgs e);
	void HandleEnableInput(Control* sender);
	void HandleRadio(Control* sender);
	void HandleComboSelection(ComboBox* sender);
	void HandleNumericValue(class NumericUpDown* sender, double oldValue, double newValue);
	void HandleDocsLink(Control* sender, MouseEventArgs e);
	void HandleExpander(class Expander* sender, bool expanded);
	void HandleOpenImage(Control* sender, MouseEventArgs e);
	void HandleDropImage(Control* sender, std::vector<std::wstring> files);
	void HandlePictureVisibility(Control* sender);
	void HandleListItem(class ListView* sender, int index);
	void HandleGridEnabled(Control* sender);
	void HandleGridVisible(Control* sender);
	void HandlePropertyValue(PropertyGridView* sender, int index,
		std::wstring oldValue, std::wstring newValue);
	void HandleFilterApply(FilterBar* sender);
	void HandleFilterReset(FilterBar* sender);
	void HandleKpiClick(KpiCard* sender);
	void HandleChartKind(Control* sender, MouseEventArgs e);
	void HandleChartPoint(ChartView* sender, int seriesIndex, int pointIndex);
	void HandleReportRow(ReportView* sender, int rowIndex);
	void HandleFarButton(Control* sender, MouseEventArgs e);
	void HandleSystemAction(Control* sender, MouseEventArgs e);
	void HandleSystemSurfaceMouseUp(Control* sender, MouseEventArgs e);
	void HandleToastClick(ToastHost* sender, int index);
	void HandleInvokeWeb(Control* sender, MouseEventArgs e);
	void HandleMediaCommand(Control* sender, MouseEventArgs e);
	void HandleMediaVolume(Control* sender, float oldValue, float newValue);
	void HandleMediaSpeed(Control* sender, float oldValue, float newValue);
	void HandleMediaLoop(Control* sender);
	void HandleMediaSeek(Control* sender, float oldValue, float newValue);
	void HandleMediaOpened(MediaPlayer* sender);
	void HandleMediaEnded(MediaPlayer* sender);
	void HandleMediaFailed(MediaPlayer* sender);
	void HandleMediaPosition(MediaPlayer* sender, double position);
	void HandleSystemContextMenu(Control* sender, int id);

	std::shared_ptr<DesignerModel::RuntimeCustomControlRegistry> _customControls;
	DesignerModel::RuntimeDocumentSession _xamlSession;

	std::shared_ptr<BitmapSource> _images[10]{};
	std::shared_ptr<BitmapSource> _icons[5]{};

	Menu* _menu = nullptr;
	ToolBar* _toolBar = nullptr;
	StatusBar* _statusBar = nullptr;
	Slider* _globalProgress = nullptr;
	Label* _statusText = nullptr;
	TabControl* _tabs = nullptr;
	Button* _basicButton = nullptr;
	RadioBox* _radioA = nullptr;
	RadioBox* _radioB = nullptr;
	PictureBox* _picture = nullptr;
	ProgressBar* _progress = nullptr;
	ProgressRing* _progressRing = nullptr;
	PagedGridView* _pagedGrid = nullptr;
	PropertyGridView* _propertyGrid = nullptr;
	FilterBar* _filter = nullptr;
	KpiCard* _kpiRevenue = nullptr;
	KpiCard* _kpiDeals = nullptr;
	KpiCard* _kpiMargin = nullptr;
	ChartView* _chart = nullptr;
	ReportView* _report = nullptr;
	ToastHost* _toast = nullptr;
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
