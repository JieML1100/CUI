#include <Colors.h>

#ifdef CUI_ENABLE_WEBVIEW2

#include "WebBrowser.h"
#include "DCompLayeredHost.h"
#include "EventInfrastructure.h"
#include "PresentationInfrastructure.h"
#include "PresentationScene.h"
#include "Window.h"
#include "WindowInfrastructure.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include <windowsx.h>
#include <dcomp.h>
#include <WebView2.h>

#include <wrl.h>
#include <wrl/client.h>
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

#if defined(_MSC_VER)
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "WebView2LoaderStatic.lib")
#endif

struct WebBrowser::Impl
{
	struct CompositionClipLayer final
	{
		ComPtr<IDCompositionVisual> Visual;
		ComPtr<IDCompositionRectangleClip> Clip;
	};

	bool initialized = false;
	bool webviewReady = false;
	InitializationState initializationState = InitializationState::NotStarted;
	HRESULT lastInitHr = E_PENDING;
	HRESULT lastEnvironmentHr = E_PENDING;
	HRESULT lastControllerHr = E_PENDING;
	HRESULT lastGetWebViewHr = E_PENDING;
	HRESULT lastWebViewHr = E_PENDING;
	HRESULT lastCoInitHr = S_OK;
	int navCompletedCount = 0;
	bool isNavigating = false;
	std::wstring cachedSource;
	std::wstring cachedTitle;
	PendingNavigationKind pendingKind = PendingNavigationKind::None;
	std::wstring pendingUrl;
	std::wstring pendingHtml;
	double zoomFactor = 1.0;
	bool areDefaultContextMenusEnabled = true;
	bool isStatusBarEnabled = false;
	bool isZoomControlEnabled = true;
	D2D1_COLOR_F defaultBackgroundColor = Colors::White;
	::CornerRadius cornerRadius{};
	std::wstring initialUrl;
	std::unordered_map<std::wstring, JsInvokeHandler> invokeHandlers;
	std::shared_ptr<std::atomic<bool>> lifetime =
		std::make_shared<std::atomic<bool>>(true);

	ComPtr<ICoreWebView2Environment> env;
	ComPtr<ICoreWebView2Controller> controller;
	ComPtr<ICoreWebView2CompositionController> compositionController;
	ComPtr<ICoreWebView2> webview;
	ComPtr<ICoreWebView2_2> webview2;
	// The registered anchor owns ancestor clip wrappers.  The boundary visual is
	// CUI-only: it owns the control shape, quality policy, and RenderTransform.
	// WebView2 receives the content visual below it as RootVisualTarget, so the
	// component cannot replace or bypass CUI's final antialiased boundary.
	ComPtr<IDCompositionVisual> dcompVisual;
	ComPtr<IDCompositionVisual> dcompBoundaryVisual;
	ComPtr<IDCompositionRectangleClip> dcompBoundaryClip;
	ComPtr<IDCompositionVisual> dcompContentVisual;
	std::vector<CompositionClipLayer> dcompClipLayers;
	bool dcompClipTreeValid = false;
	int controllerBoundsWidth = 0;
	int controllerBoundsHeight = 0;
	bool rootAttached = false;
	bool interopInstalled = false;
	bool hasSystemCursorId = false;
	UINT32 lastSystemCursorId = 0;
	EventRegistrationToken cursorChangedToken{};
	EventRegistrationToken navStartingToken{};
	EventRegistrationToken navCompletedToken{};
	EventRegistrationToken contentLoadingToken{};
	EventRegistrationToken domContentLoadedToken{};
	EventRegistrationToken sourceChangedToken{};
	EventRegistrationToken historyChangedToken{};
	EventRegistrationToken documentTitleChangedToken{};
	EventRegistrationToken newWindowRequestedToken{};
	EventRegistrationToken processFailedToken{};
	EventRegistrationToken webMessageToken{};
};

// Keep the established implementation readable while all storage lives in Impl.
#define _initialized (_impl->initialized)
#define _webviewReady (_impl->webviewReady)
#define _initializationState (_impl->initializationState)
#define _lastInitHr (_impl->lastInitHr)
#define _lastEnvironmentHr (_impl->lastEnvironmentHr)
#define _lastControllerHr (_impl->lastControllerHr)
#define _lastGetWebViewHr (_impl->lastGetWebViewHr)
#define _lastWebViewHr (_impl->lastWebViewHr)
#define _lastCoInitHr (_impl->lastCoInitHr)
#define _navCompletedCount (_impl->navCompletedCount)
#define _isNavigating (_impl->isNavigating)
#define _cachedSource (_impl->cachedSource)
#define _cachedTitle (_impl->cachedTitle)
#define _pendingKind (_impl->pendingKind)
#define _pendingUrl (_impl->pendingUrl)
#define _pendingHtml (_impl->pendingHtml)
#define _zoomFactor (_impl->zoomFactor)
#define _areDefaultContextMenusEnabled (_impl->areDefaultContextMenusEnabled)
#define _isStatusBarEnabled (_impl->isStatusBarEnabled)
#define _isZoomControlEnabled (_impl->isZoomControlEnabled)
#define _defaultBackgroundColor (_impl->defaultBackgroundColor)
#define _cornerRadius (_impl->cornerRadius)
#define _initialUrl (_impl->initialUrl)
#define _invokeHandlers (_impl->invokeHandlers)
#define _lifetime (_impl->lifetime)
#define _env (_impl->env)
#define _controller (_impl->controller)
#define _compositionController (_impl->compositionController)
#define _webview (_impl->webview)
#define _webview2 (_impl->webview2)
#define _dcompVisual (_impl->dcompVisual)
#define _dcompBoundaryVisual (_impl->dcompBoundaryVisual)
#define _dcompBoundaryClip (_impl->dcompBoundaryClip)
#define _dcompContentVisual (_impl->dcompContentVisual)
#define _dcompClipLayers (_impl->dcompClipLayers)
#define _dcompClipTreeValid (_impl->dcompClipTreeValid)
#define _controllerBoundsWidth (_impl->controllerBoundsWidth)
#define _controllerBoundsHeight (_impl->controllerBoundsHeight)
#define _rootAttached (_impl->rootAttached)
#define _interopInstalled (_impl->interopInstalled)
#define _hasSystemCursorId (_impl->hasSystemCursorId)
#define _lastSystemCursorId (_impl->lastSystemCursorId)
#define _cursorChangedToken (_impl->cursorChangedToken)
#define _navStartingToken (_impl->navStartingToken)
#define _navCompletedToken (_impl->navCompletedToken)
#define _contentLoadingToken (_impl->contentLoadingToken)
#define _domContentLoadedToken (_impl->domContentLoadedToken)
#define _sourceChangedToken (_impl->sourceChangedToken)
#define _historyChangedToken (_impl->historyChangedToken)
#define _documentTitleChangedToken (_impl->documentTitleChangedToken)
#define _newWindowRequestedToken (_impl->newWindowRequestedToken)
#define _processFailedToken (_impl->processFailedToken)
#define _webMessageToken (_impl->webMessageToken)

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<WebBrowser, TValue> WebBrowserPropertyOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			DependencyPropertyEditorKind editor))
	{
		DependencyPropertyOptions<WebBrowser, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::None;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Web";
		options.Design.CategoryOrder = 170;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return options;
	}

	auto WebBrowserPropertySubscriber(
		const DependencyProperty& (*propertyAccessor)())
	{
		return [propertyAccessor](
			WebBrowser& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyAccessor, handler = std::move(handler)](
					DependencyObject*, const DependencyPropertyChangedEventArgs& args)
				{
					if (args.Property == &propertyAccessor())
						handler();
				});
		};
	}
}

int WebBrowser::ResolvePresentationOrder(WebBrowser* browser)
{
	if (!browser || !browser->GetPresentationWindow())
		return 0;
	int order = 0;
	if (browser->TryGetPresentationOrderOverride(order))
		return order;
	return browser->GetPresentationWindow()->GetPresentationOrder(browser);
}

static int HexVal(wchar_t c);

WebBrowser::WebBrowser()
	: _impl(std::make_unique<Impl>())
{
	this->RendererBackgroundColor = Colors::White;
	_lastInitHr = E_PENDING;
	_lastControllerHr = E_PENDING;
	_lastGetWebViewHr = E_PENDING;

}

WebBrowser::~WebBrowser()
{
	_lifetime->store(false, std::memory_order_release);
	if (_webview)
	{
		if (_navStartingToken.value != 0) { _webview->remove_NavigationStarting(_navStartingToken); _navStartingToken.value = 0; }
		if (_navCompletedToken.value != 0) { _webview->remove_NavigationCompleted(_navCompletedToken); _navCompletedToken.value = 0; }
		if (_contentLoadingToken.value != 0) { _webview->remove_ContentLoading(_contentLoadingToken); _contentLoadingToken.value = 0; }
		if (_sourceChangedToken.value != 0) { _webview->remove_SourceChanged(_sourceChangedToken); _sourceChangedToken.value = 0; }
		if (_historyChangedToken.value != 0) { _webview->remove_HistoryChanged(_historyChangedToken); _historyChangedToken.value = 0; }
		if (_documentTitleChangedToken.value != 0) { _webview->remove_DocumentTitleChanged(_documentTitleChangedToken); _documentTitleChangedToken.value = 0; }
		if (_newWindowRequestedToken.value != 0) { _webview->remove_NewWindowRequested(_newWindowRequestedToken); _newWindowRequestedToken.value = 0; }
		if (_processFailedToken.value != 0) { _webview->remove_ProcessFailed(_processFailedToken); _processFailedToken.value = 0; }
	}
	if (_webview2 && _domContentLoadedToken.value != 0)
	{
		_webview2->remove_DOMContentLoaded(_domContentLoadedToken);
		_domContentLoadedToken.value = 0;
	}
	if (_webview && _webMessageToken.value != 0)
	{
		_webview->remove_WebMessageReceived(_webMessageToken);
		_webMessageToken.value = 0;
	}
	_webview2.Reset();
	_webview.Reset();
	if (_compositionController && _cursorChangedToken.value != 0)
	{
		_compositionController->remove_CursorChanged(_cursorChangedToken);
		_cursorChangedToken.value = 0;
	}
	_compositionController.Reset();
	_controller.Reset();
	_env.Reset();

		if (this->GetPresentationWindow() && _dcompVisual)
		{
			this->GetPresentationWindow()->UnregisterDCompVisual(_dcompVisual.Get());
			this->GetPresentationWindow()->CommitComposition();
		}
	_dcompClipLayers.clear();
	_dcompClipTreeValid = false;
	_dcompContentVisual.Reset();
	_dcompBoundaryClip.Reset();
	_dcompBoundaryVisual.Reset();
	_dcompVisual.Reset();
}

void WebBrowser::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)InitialUrlProperty();
	(void)ZoomFactorProperty();
	(void)AreDefaultContextMenusEnabledProperty();
	(void)IsStatusBarEnabledProperty();
	(void)IsZoomControlEnabledProperty();
	(void)DefaultBackgroundColorProperty();
	(void)CornerRadiusProperty();
#endif
}

const DependencyProperty& WebBrowser::ZoomFactorProperty()
{
	static const auto registration = []
	{
		auto options = WebBrowserPropertyOptions(
			1.0 CUI_DESIGN_METADATA_ARGUMENTS(
				20, DependencyPropertyEditorKind::Number));
		options.Validate = [](const double& proposed)
		{ return std::isfinite(proposed); };
		options.Coerce = [](WebBrowser&, const double& proposed)
			-> std::optional<double>
		{
			return (std::clamp)(proposed, 0.25, 5.0);
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = 0.25;
		options.Design.Maximum = 5.0;
		options.Design.Step = 0.05;
		)
		return DependencyPropertyRegistry::RegisterStatic<WebBrowser, double>(
			DependencyPropertyRegistrationLiteral(L"ZoomFactor"),
			[](WebBrowser& target) { return target.GetZoomFactor(); },
			[](WebBrowser& target, const double& value)
			{ target.SetZoomFactor(value); },
			WebBrowserPropertySubscriber(&WebBrowser::ZoomFactorProperty),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& WebBrowser::AreDefaultContextMenusEnabledProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<WebBrowser, bool>(
			DependencyPropertyRegistrationLiteral(
				L"AreDefaultContextMenusEnabled"),
			[](WebBrowser& target)
			{ return target.GetAreDefaultContextMenusEnabled(); },
			[](WebBrowser& target, const bool& value)
			{ target.SetAreDefaultContextMenusEnabled(value); },
			WebBrowserPropertySubscriber(
				&WebBrowser::AreDefaultContextMenusEnabledProperty),
			WebBrowserPropertyOptions(
				true CUI_DESIGN_METADATA_ARGUMENTS(
					30, DependencyPropertyEditorKind::Boolean)));
	return *registration;
}

const DependencyProperty& WebBrowser::IsStatusBarEnabledProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<WebBrowser, bool>(
			DependencyPropertyRegistrationLiteral(L"IsStatusBarEnabled"),
			[](WebBrowser& target) { return target.GetIsStatusBarEnabled(); },
			[](WebBrowser& target, const bool& value)
			{ target.SetIsStatusBarEnabled(value); },
			WebBrowserPropertySubscriber(
				&WebBrowser::IsStatusBarEnabledProperty),
			WebBrowserPropertyOptions(
				false CUI_DESIGN_METADATA_ARGUMENTS(
					40, DependencyPropertyEditorKind::Boolean)));
	return *registration;
}

const DependencyProperty& WebBrowser::IsZoomControlEnabledProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<WebBrowser, bool>(
			DependencyPropertyRegistrationLiteral(L"IsZoomControlEnabled"),
			[](WebBrowser& target) { return target.GetIsZoomControlEnabled(); },
			[](WebBrowser& target, const bool& value)
			{ target.SetIsZoomControlEnabled(value); },
			WebBrowserPropertySubscriber(
				&WebBrowser::IsZoomControlEnabledProperty),
			WebBrowserPropertyOptions(
				true CUI_DESIGN_METADATA_ARGUMENTS(
					50, DependencyPropertyEditorKind::Boolean)));
	return *registration;
}

const DependencyProperty& WebBrowser::DefaultBackgroundColorProperty()
{
	static const auto registration = []
	{
		auto options = WebBrowserPropertyOptions(
			Colors::White CUI_DESIGN_METADATA_ARGUMENTS(
				60, DependencyPropertyEditorKind::Color));
		options.Validate = [](const D2D1_COLOR_F& proposed)
		{
			auto channel = [](float value)
			{
				return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
			};
			return channel(proposed.r) && channel(proposed.g)
				&& channel(proposed.b) && channel(proposed.a)
				&& (proposed.a <= 1e-6f || proposed.a >= 1.0f - 1e-6f);
		};
		options.Equals = [](const D2D1_COLOR_F& left,
			const D2D1_COLOR_F& right)
		{
			return std::fabs(left.r - right.r) <= 1e-6f
				&& std::fabs(left.g - right.g) <= 1e-6f
				&& std::fabs(left.b - right.b) <= 1e-6f
				&& std::fabs(left.a - right.a) <= 1e-6f;
		};
		return DependencyPropertyRegistry::RegisterStatic<
			WebBrowser, D2D1_COLOR_F>(
				DependencyPropertyRegistrationLiteral(
					L"DefaultBackgroundColor"),
				[](WebBrowser& target)
				{ return target.GetDefaultBackgroundColor(); },
				[](WebBrowser& target, const D2D1_COLOR_F& value)
				{ target.SetDefaultBackgroundColor(value); },
				WebBrowserPropertySubscriber(
					&WebBrowser::DefaultBackgroundColorProperty),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& WebBrowser::CornerRadiusProperty()
{
	static const auto registration = []
	{
		auto options = WebBrowserPropertyOptions(
			::CornerRadius{} CUI_DESIGN_METADATA_ARGUMENTS(
				70, DependencyPropertyEditorKind::Text));
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Validate = [](const ::CornerRadius& value)
		{
			return std::isfinite(value.TopLeft) && value.TopLeft >= 0.0f
				&& std::isfinite(value.TopRight) && value.TopRight >= 0.0f
				&& std::isfinite(value.BottomRight) && value.BottomRight >= 0.0f
				&& std::isfinite(value.BottomLeft) && value.BottomLeft >= 0.0f;
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			WebBrowser, ::CornerRadius>(
				DependencyPropertyRegistrationLiteral(L"CornerRadius"),
				[](WebBrowser& target) { return target.GetCornerRadius(); },
				[](WebBrowser& target, const ::CornerRadius& value)
				{ target.SetCornerRadius(value); },
				WebBrowserPropertySubscriber(
					&WebBrowser::CornerRadiusProperty),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& WebBrowser::InitialUrlProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<WebBrowser, std::wstring>(
			DependencyPropertyRegistrationLiteral(L"InitialUrl"),
			[](WebBrowser& target) { return target.GetInitialUrl(); },
			[](WebBrowser& target, const std::wstring& value)
			{ target.SetInitialUrl(value); },
			WebBrowserPropertySubscriber(&WebBrowser::InitialUrlProperty),
			WebBrowserPropertyOptions(
				std::wstring{} CUI_DESIGN_METADATA_ARGUMENTS(
					10, DependencyPropertyEditorKind::Text)));
	return *registration;
}

bool WebBrowser::TryInitialize()
{
	if (_initializationState == InitializationState::Ready) return true;
	if (_initializationState == InitializationState::Failed) return false;
	if (!GetPresentationWindow() || !GetPresentationWindow()->Handle) return false;
	EnsureInitialized();
	return _initializationState == InitializationState::Initializing
		|| _initializationState == InitializationState::Ready;
}

bool WebBrowser::IsInitialized() const { return _initialized; }
bool WebBrowser::IsWebViewReady() const { return _webviewReady; }
WebBrowser::InitializationState WebBrowser::GetInitializationState() const
{
	return _initializationState;
}
HRESULT WebBrowser::GetLastInitializationError() const { return _lastInitHr; }
HRESULT WebBrowser::GetLastEnvironmentError() const { return _lastEnvironmentHr; }
HRESULT WebBrowser::GetLastControllerError() const { return _lastControllerHr; }
HRESULT WebBrowser::GetLastWebViewError() const { return _lastWebViewHr; }

bool WebBrowser::IsNavigating() const { return _isNavigating; }
bool WebBrowser::IsWebViewVisible() const
{
	auto* self = const_cast<WebBrowser*>(this);
	return _webviewReady && _controller && self->IsVisible;
}

bool WebBrowser::HasPendingNavigation() const
{
	return _pendingKind != PendingNavigationKind::None;
}
WebBrowser::PendingNavigationKind WebBrowser::GetPendingNavigationKind() const
{
	return _pendingKind;
}
std::wstring WebBrowser::GetPendingUrl() const
{
	return _pendingKind == PendingNavigationKind::Url ? _pendingUrl : L"";
}
void WebBrowser::ClearPendingNavigation()
{
	_pendingKind = PendingNavigationKind::None;
	_pendingUrl.clear();
	_pendingHtml.clear();
}

bool WebBrowser::GetAreDefaultContextMenusEnabled() const
{
	return _areDefaultContextMenusEnabled;
}
void WebBrowser::SetAreDefaultContextMenusEnabled(bool value)
{
	if (SetPropertyField(
		AreDefaultContextMenusEnabledProperty(),
		_impl->areDefaultContextMenusEnabled, value))
		ApplyWebViewSettings();
}
bool WebBrowser::GetIsStatusBarEnabled() const { return _isStatusBarEnabled; }
void WebBrowser::SetIsStatusBarEnabled(bool value)
{
	if (SetPropertyField(
		IsStatusBarEnabledProperty(), _impl->isStatusBarEnabled, value))
		ApplyWebViewSettings();
}
bool WebBrowser::GetIsZoomControlEnabled() const { return _isZoomControlEnabled; }
void WebBrowser::SetIsZoomControlEnabled(bool value)
{
	if (SetPropertyField(
		IsZoomControlEnabledProperty(), _impl->isZoomControlEnabled, value))
		ApplyWebViewSettings();
}
D2D1_COLOR_F WebBrowser::GetDefaultBackgroundColor() const
{
	return _defaultBackgroundColor;
}
void WebBrowser::SetDefaultBackgroundColor(D2D1_COLOR_F value)
{
	if (SetPropertyField(DefaultBackgroundColorProperty(),
		_impl->defaultBackgroundColor, value))
		ApplyWebViewSettings();
}
::CornerRadius WebBrowser::GetCornerRadius() const
{
	return _cornerRadius;
}
void WebBrowser::SetCornerRadius(::CornerRadius value)
{
	if (SetPropertyField(
		CornerRadiusProperty(), _impl->cornerRadius, value))
		EnsureControllerBounds();
}
std::wstring WebBrowser::GetInitialUrl() const { return _initialUrl; }
void WebBrowser::SetInitialUrl(std::wstring value)
{
	const auto previousValue = _initialUrl;
	if (!SetPropertyField(
		InitialUrlProperty(), _impl->initialUrl, std::move(value)))
		return;
	if (_initialUrl.empty())
	{
		if (_pendingKind == PendingNavigationKind::Url
			&& _pendingUrl == previousValue)
			ClearPendingNavigation();
	}
	else
		TryNavigate(_initialUrl);
}

void WebBrowser::RegisterJsInvokeHandler(const std::wstring& name, JsInvokeHandler handler)
{
	_invokeHandlers[name] = std::move(handler);
}

void WebBrowser::UnregisterJsInvokeHandler(const std::wstring& name)
{
	auto it = _invokeHandlers.find(name);
	if (it != _invokeHandlers.end()) _invokeHandlers.erase(it);
}

void WebBrowser::ClearJsInvokeHandlers()
{
	_invokeHandlers.clear();
}

static int HexVal(wchar_t c)
{
	if (c >= L'0' && c <= L'9') return (int)(c - L'0');
	if (c >= L'a' && c <= L'f') return 10 + (int)(c - L'a');
	if (c >= L'A' && c <= L'F') return 10 + (int)(c - L'A');
	return -1;
}

static std::wstring JsonUnquote(const std::wstring& json)
{
	if (json == L"null") return L"";
	if (json.size() < 2) return json;
	if (json.front() != L'"' || json.back() != L'"') return json;

	std::wstring out;
	out.reserve(json.size());
	for (size_t i = 1; i + 1 < json.size(); i++)
	{
		wchar_t c = json[i];
		if (c != L'\\')
		{
			out.push_back(c);
			continue;
		}
		if (i + 1 >= json.size() - 1) break;
		wchar_t e = json[++i];
		switch (e)
		{
		case L'"': out.push_back(L'"'); break;
		case L'\\': out.push_back(L'\\'); break;
		case L'/': out.push_back(L'/'); break;
		case L'b': out.push_back(L'\b'); break;
		case L'f': out.push_back(L'\f'); break;
		case L'n': out.push_back(L'\n'); break;
		case L'r': out.push_back(L'\r'); break;
		case L't': out.push_back(L'\t'); break;
		case L'u':
		{
			if (i + 4 >= json.size() - 1) break;
			int h1 = HexVal(json[i + 1]);
			int h2 = HexVal(json[i + 2]);
			int h3 = HexVal(json[i + 3]);
			int h4 = HexVal(json[i + 4]);
			if (h1 < 0 || h2 < 0 || h3 < 0 || h4 < 0) break;
			wchar_t uc = (wchar_t)((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
			out.push_back(uc);
			i += 4;
		}
		break;
		default:
			out.push_back(e);
			break;
		}
	}
	return out;
}

bool WebBrowser::RebindCompositionVisual()
{
	auto* window = this->GetPresentationWindow();
	if (!window || !window->Handle) return false;
	IDCompositionDevice* dcompDevice = window->GetDCompDevice();
	if (!dcompDevice) return false;

	if (_compositionController && _rootAttached)
		(void)_compositionController->put_RootVisualTarget(nullptr);
	_rootAttached = false;
	if (_dcompVisual)
		window->UnregisterDCompVisual(_dcompVisual.Get());
	_dcompClipLayers.clear();
	_dcompClipTreeValid = false;
	_dcompContentVisual.Reset();
	_dcompBoundaryClip.Reset();
	_dcompBoundaryVisual.Reset();
	_dcompVisual.Reset();

	ComPtr<IDCompositionVisual> replacementVisual;
	ComPtr<IDCompositionVisual> replacementBoundaryVisual;
	ComPtr<IDCompositionRectangleClip> replacementBoundaryClip;
	ComPtr<IDCompositionVisual> replacementContentVisual;
	auto configureQuality = [&](IDCompositionVisual* visual)
	{
		if (!visual) return false;
		HRESULT result = visual->SetBitmapInterpolationMode(
			DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
		if (SUCCEEDED(result))
			result = visual->SetBorderMode(
				DCOMPOSITION_BORDER_MODE_SOFT);
		if (FAILED(result)) _lastControllerHr = result;
		return SUCCEEDED(result);
	};
	HRESULT hr = dcompDevice->CreateVisual(
		replacementVisual.GetAddressOf());
	if (FAILED(hr) || !replacementVisual) return false;
	if (!configureQuality(replacementVisual.Get())) return false;

	// Never put the CUI clip or RenderTransform on RootVisualTarget itself.
	// WebView2 owns the subtree connected there.  This dedicated parent is the
	// final, component-independent raster boundary and therefore the only layer
	// that defines the browser shape and its transformed edge coverage.
	hr = dcompDevice->CreateVisual(
		replacementBoundaryVisual.GetAddressOf());
	if (FAILED(hr) || !replacementBoundaryVisual) return false;
	if (!configureQuality(replacementBoundaryVisual.Get())) return false;
	hr = dcompDevice->CreateRectangleClip(
		replacementBoundaryClip.GetAddressOf());
	if (FAILED(hr) || !replacementBoundaryClip) return false;
	hr = replacementBoundaryVisual->SetClip(
		replacementBoundaryClip.Get());
	if (FAILED(hr)) return false;

	hr = dcompDevice->CreateVisual(
		replacementContentVisual.GetAddressOf());
	if (FAILED(hr) || !replacementContentVisual) return false;
	if (!configureQuality(replacementContentVisual.Get())) return false;
	hr = replacementBoundaryVisual->AddVisual(
		replacementContentVisual.Get(), FALSE, nullptr);
	if (FAILED(hr)) return false;
	hr = replacementVisual->AddVisual(
		replacementBoundaryVisual.Get(), FALSE, nullptr);
	if (FAILED(hr)) return false;
	if (!window->RegisterDCompVisual(
		replacementVisual.Get(), PresentationSceneContentLayer,
		ResolvePresentationOrder(this)))
	{
		return false;
	}
	_dcompVisual = std::move(replacementVisual);
	_dcompBoundaryVisual = std::move(replacementBoundaryVisual);
	_dcompBoundaryClip = std::move(replacementBoundaryClip);
	_dcompContentVisual = std::move(replacementContentVisual);
	_dcompClipTreeValid = true;
	if (window->CommitComposition()) return true;
	window->UnregisterDCompVisual(_dcompVisual.Get());
	_dcompContentVisual.Reset();
	_dcompBoundaryClip.Reset();
	_dcompBoundaryVisual.Reset();
	_dcompVisual.Reset();
	_dcompClipTreeValid = false;
	return false;
}

bool WebBrowser::EnsureCompositionClipLayerCount(std::size_t count)
{
	if (!_dcompVisual || !_dcompBoundaryVisual || !_dcompContentVisual)
		return false;
	if (_dcompClipTreeValid && _dcompClipLayers.size() == count) return true;
	auto* window = GetPresentationWindow();
	auto* device = window ? window->GetDCompDevice() : nullptr;
	if (!device) return false;

	std::vector<Impl::CompositionClipLayer> replacement;
	replacement.reserve(count);
	for (std::size_t index = 0; index < count; ++index)
	{
		Impl::CompositionClipLayer layer;
		HRESULT hr = device->CreateVisual(layer.Visual.GetAddressOf());
		if (FAILED(hr) || !layer.Visual)
		{
			_lastControllerHr = FAILED(hr) ? hr : E_FAIL;
			return false;
		}
		hr = layer.Visual->SetBitmapInterpolationMode(
			DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
		if (SUCCEEDED(hr))
			hr = layer.Visual->SetBorderMode(
				DCOMPOSITION_BORDER_MODE_SOFT);
		if (FAILED(hr))
		{
			_lastControllerHr = hr;
			return false;
		}
		hr = device->CreateRectangleClip(layer.Clip.GetAddressOf());
		if (FAILED(hr) || !layer.Clip)
		{
			_lastControllerHr = FAILED(hr) ? hr : E_FAIL;
			return false;
		}
		hr = layer.Visual->SetClip(layer.Clip.Get());
		if (FAILED(hr))
		{
			_lastControllerHr = hr;
			return false;
		}
		replacement.push_back(std::move(layer));
	}

	// A DComp visual has one parent. Fully disconnect the old wrappers before
	// moving CUI's retained boundary visual into the replacement chain.  The
	// WebView RootVisualTarget remains attached below that boundary throughout.
	_dcompClipTreeValid = false;
	HRESULT hr = _dcompVisual->RemoveAllVisuals();
	if (FAILED(hr))
	{
		_lastControllerHr = hr;
		return false;
	}
	for (auto& layer : _dcompClipLayers)
	{
		if (!layer.Visual) continue;
		hr = layer.Visual->RemoveAllVisuals();
		if (FAILED(hr))
		{
			_lastControllerHr = hr;
			return false;
		}
	}
	IDCompositionVisual* parent = _dcompVisual.Get();
	for (auto& layer : replacement)
	{
		hr = parent->AddVisual(layer.Visual.Get(), FALSE, nullptr);
		if (FAILED(hr)) break;
		parent = layer.Visual.Get();
	}
	if (SUCCEEDED(hr))
		hr = parent->AddVisual(
			_dcompBoundaryVisual.Get(), FALSE, nullptr);
	if (FAILED(hr))
	{
		for (auto& layer : replacement)
			if (layer.Visual) (void)layer.Visual->RemoveAllVisuals();
		(void)_dcompVisual->RemoveAllVisuals();
		const HRESULT restoreResult = _dcompVisual->AddVisual(
			_dcompBoundaryVisual.Get(), FALSE, nullptr);
		_dcompClipLayers.clear();
		_dcompClipTreeValid = SUCCEEDED(restoreResult);
		_lastControllerHr = hr;
		return false;
	}

	_dcompClipLayers = std::move(replacement);
	_dcompClipTreeValid = true;
	return true;
}

void WebBrowser::EnsureInitialized()
{
	if (_initialized) return;
	if (!this->GetPresentationWindow() || !this->GetPresentationWindow()->Handle) return;

	_initialized = true;
	_initializationState = InitializationState::Initializing;
	_lastInitHr = E_PENDING;
	_lastEnvironmentHr = E_PENDING;
	_lastControllerHr = E_PENDING;
	_lastGetWebViewHr = E_PENDING;
	_lastWebViewHr = E_PENDING;
	_webviewReady = false;
	_navCompletedCount = 0;
	_isNavigating = false;
	_cachedSource.clear();
	_cachedTitle.clear();

	// A composition-device rotation can make the host temporarily unavailable.
	// Do not turn that recoverable host condition into a permanent WebView2
	// initialization failure (and do this before balancing COM initialization
	// would become necessary on every retry).
	if (!_dcompVisual && !RebindCompositionVisual())
	{
		_lastInitHr = E_PENDING;
		_initializationState = InitializationState::NotStarted;
		_initialized = false;
		this->InvalidateVisual();
		return;
	}

	_lastCoInitHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(_lastCoInitHr) && _lastCoInitHr != RPC_E_CHANGED_MODE)
	{
		_lastInitHr = _lastCoInitHr;
		_initializationState = InitializationState::Failed;
		this->InvalidateVisual();
		return;
	}

	auto lifetime = _lifetime;
	auto envCompleted = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
		[this, lifetime](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
		{
			if (!lifetime->load(std::memory_order_acquire)) return S_OK;
			_lastInitHr = result;
			_lastEnvironmentHr = result;
			if (FAILED(result) || !env)
			{
				_initializationState = InitializationState::Failed;
				this->InvalidateVisual();
				return S_OK;
			}
			if (!this->GetPresentationWindow() || !this->GetPresentationWindow()->Handle)
			{
				_lastControllerHr = E_ABORT;
				_lastInitHr = E_ABORT;
				_initializationState = InitializationState::Failed;
				this->InvalidateVisual();
				return S_OK;
			}
			_env = env;

			auto ctlCompleted = Callback<ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>(
				[this, lifetime](HRESULT result2, ICoreWebView2CompositionController* compositionController) -> HRESULT
				{
					if (!lifetime->load(std::memory_order_acquire)) return S_OK;
					if (!this->GetPresentationWindow() || !this->GetPresentationWindow()->Handle)
					{
						_lastControllerHr = E_ABORT;
						_lastInitHr = E_ABORT;
						_initializationState = InitializationState::Failed;
						this->InvalidateVisual();
						return S_OK;
					}
					_lastControllerHr = result2;
					if (FAILED(result2) || !compositionController)
					{
						_lastInitHr = FAILED(result2) ? result2 : E_NOINTERFACE;
						_initializationState = InitializationState::Failed;
						this->InvalidateVisual();
						return S_OK;
					}
					_compositionController = compositionController;
					_controller.Reset();
					_controllerBoundsWidth = 0;
					_controllerBoundsHeight = 0;
					// 同一对象上也实现 ICoreWebView2Controller
					const HRESULT controllerInterfaceHr =
						_compositionController.As(&_controller);
					if (FAILED(controllerInterfaceHr) || !_controller)
					{
						_lastControllerHr = FAILED(controllerInterfaceHr)
							? controllerInterfaceHr : E_NOINTERFACE;
						_lastInitHr = _lastControllerHr;
						_initializationState = InitializationState::Failed;
						this->InvalidateVisual();
						return S_OK;
					}
					_webview.Reset();
					_lastGetWebViewHr = _controller->get_CoreWebView2(_webview.GetAddressOf());
					_lastWebViewHr = _lastGetWebViewHr;
					if (FAILED(_lastGetWebViewHr) || !_webview)
					{
						_lastInitHr = FAILED(_lastGetWebViewHr)
							? _lastGetWebViewHr : E_NOINTERFACE;
						_initializationState = InitializationState::Failed;
						this->InvalidateVisual();
						return S_OK;
					}
					const HRESULT webView2InterfaceHr = _webview.As(&_webview2);
					if (FAILED(webView2InterfaceHr) || !_webview2)
					{
						_lastWebViewHr = FAILED(webView2InterfaceHr)
							? webView2InterfaceHr : E_NOINTERFACE;
						_lastInitHr = _lastWebViewHr;
						_initializationState = InitializationState::Failed;
						this->InvalidateVisual();
						return S_OK;
					}

					// 将 WebView2 视觉树挂到我们的 DComp Visual
					if (_compositionController && _dcompContentVisual)
					{
						const HRESULT rootResult =
							_compositionController->put_RootVisualTarget(
								_dcompContentVisual.Get());
						_rootAttached = SUCCEEDED(rootResult);
						if (FAILED(rootResult))
						{
							_lastControllerHr = rootResult;
							_lastInitHr = rootResult;
						}
						this->GetPresentationWindow()->CommitComposition();
					}

					// CursorChanged：缓存 system cursor id，交给 Window::UpdateCursor 使用
					if (_compositionController)
					{
						_cursorChangedToken.value = 0;
						_compositionController->add_CursorChanged(
							Callback<ICoreWebView2CursorChangedEventHandler>(
								[this, lifetime](ICoreWebView2CompositionController* sender, IUnknown* args) -> HRESULT
								{
									if (!lifetime->load(std::memory_order_acquire)) return S_OK;
									(void)args;
									UINT32 id = 0;
									if (sender && SUCCEEDED(sender->get_SystemCursorId(&id)))
									{
										_lastSystemCursorId = id;
										_hasSystemCursorId = true;
									}
									else
									{
										_hasSystemCursorId = false;
									}
									// 如果当前鼠标在 WebBrowser 上，立刻刷新一次光标
									if (this->GetPresentationWindow() && this->IsMouseOver)
										cui::framework::WindowAccess::UpdateCursorFromCurrentMouse(
											*this->GetPresentationWindow());
									return S_OK;
								}).Get(),
							&_cursorChangedToken);
					}

					_webviewReady = (SUCCEEDED(_lastGetWebViewHr) && _webview != nullptr);
					ApplyWebViewSettings();
					EnsureControllerBounds();
					if (!EnsureInteropInstalled())
					{
						_lastInitHr = FAILED(_lastWebViewHr)
							? _lastWebViewHr : E_FAIL;
						_webviewReady = false;
						_initializationState = InitializationState::Failed;
						this->InvalidateVisual();
						return S_OK;
					}

					// WebView2 事件注册
					if (_webview)
					{
						_navStartingToken.value = 0;
						_webview->add_NavigationStarting(
							Callback<ICoreWebView2NavigationStartingEventHandler>(
								[this, lifetime](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT
								{
									if (!lifetime->load(std::memory_order_acquire)) return S_OK;
									(void)sender;
									WebBrowser::NavigationStartingArgs ev;
									LPWSTR raw = nullptr;
									if (args && SUCCEEDED(args->get_Uri(&raw)) && raw)
									{
										ev.Uri = raw;
										CoTaskMemFree(raw);
									}
									BOOL isUser = FALSE;
									BOOL isRedirected = FALSE;
									if (args)
									{
										args->get_IsUserInitiated(&isUser);
										args->get_IsRedirected(&isRedirected);
									}
									ev.IsUserInitiated = (isUser != FALSE);
									ev.IsRedirected = (isRedirected != FALSE);
									_isNavigating = true;
									cui::framework::EventAccess::Raise(
										OnNavigationStarting, this, ev);
									if (args && ev.Cancel)
										args->put_Cancel(TRUE);
									return S_OK;
								}).Get(),
							&_navStartingToken);

						_navCompletedToken.value = 0;
						_webview->add_NavigationCompleted(
							Callback<ICoreWebView2NavigationCompletedEventHandler>(
								[this, lifetime](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT
								{
									if (!lifetime->load(std::memory_order_acquire)) return S_OK;
									(void)sender;
									WebBrowser::NavigationCompletedArgs ev;
									BOOL isSuccess = FALSE;
									COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
									UINT64 navId = 0;
									if (args)
									{
										args->get_IsSuccess(&isSuccess);
										args->get_WebErrorStatus(&status);
										args->get_NavigationId(&navId);
									}
									ev.IsSuccess = (isSuccess != FALSE);
									ev.WebErrorStatus = static_cast<int>(status);
									ev.NavigationId = navId;
									LPWSTR raw = nullptr;
									if (_webview && SUCCEEDED(_webview->get_Source(&raw)) && raw)
									{
										ev.Uri = raw;
										_cachedSource = ev.Uri;
										CoTaskMemFree(raw);
									}
									if (args)
									{
										ComPtr<ICoreWebView2NavigationCompletedEventArgs2> args2;
										if (SUCCEEDED(args->QueryInterface(IID_PPV_ARGS(&args2))) && args2)
											args2->get_HttpStatusCode(&ev.HttpStatusCode);
									}
									ev.IsHttpErrorStatus = ev.HttpStatusCode >= 400;

									_isNavigating = false;
									cui::framework::EventAccess::Raise(
										OnNavigationCompleted, this, ev);
									if (!lifetime->load(std::memory_order_acquire)) return S_OK;
									if (!ev.IsSuccess)
										cui::framework::EventAccess::Raise(
											OnNavigationFailed, this, ev);
									if (!lifetime->load(std::memory_order_acquire)) return S_OK;
									_navCompletedCount++;
									this->InvalidateVisual();
									return S_OK;
								}).Get(),
							&_navCompletedToken);

						_contentLoadingToken.value = 0;
						_webview->add_ContentLoading(
							Callback<ICoreWebView2ContentLoadingEventHandler>(
								[this, lifetime](ICoreWebView2* sender, ICoreWebView2ContentLoadingEventArgs* args) -> HRESULT
								{
									if (!lifetime->load(std::memory_order_acquire)) return S_OK;
									(void)sender;
									WebBrowser::ContentLoadingArgs ev;
									BOOL isError = FALSE;
									UINT64 navId = 0;
									if (args)
									{
										args->get_IsErrorPage(&isError);
										args->get_NavigationId(&navId);
									}
									ev.IsErrorPage = (isError != FALSE);
									ev.NavigationId = navId;
									cui::framework::EventAccess::Raise(
										OnContentLoading, this, ev);
									if (!lifetime->load(std::memory_order_acquire)) return S_OK;
									this->InvalidateVisual();
									return S_OK;
								}).Get(),
							&_contentLoadingToken);

						_domContentLoadedToken.value = 0;
						const HRESULT domContentLoadedHr = _webview2->add_DOMContentLoaded(
							Callback<ICoreWebView2DOMContentLoadedEventHandler>(
								[this, lifetime](ICoreWebView2* sender, ICoreWebView2DOMContentLoadedEventArgs* args) -> HRESULT
								{
									if (!lifetime->load(std::memory_order_acquire)) return S_OK;
									(void)sender;
									WebBrowser::DomContentLoadedArgs ev;
									if (args)
										args->get_NavigationId(&ev.NavigationId);
									cui::framework::EventAccess::Raise(
										OnDOMContentLoaded, this, ev);
									return S_OK;
								}).Get(),
							&_domContentLoadedToken);
						if (FAILED(domContentLoadedHr))
						{
							_lastWebViewHr = domContentLoadedHr;
							_lastInitHr = domContentLoadedHr;
							_webviewReady = false;
							_initializationState = InitializationState::Failed;
							this->InvalidateVisual();
							return S_OK;
						}

						_sourceChangedToken.value = 0;
						_webview->add_SourceChanged(
							Callback<ICoreWebView2SourceChangedEventHandler>(
								[this, lifetime](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs* args) -> HRESULT
								{
									if (!lifetime->load(std::memory_order_acquire)) return S_OK;
									(void)sender;
									WebBrowser::SourceChangedArgs ev;
									BOOL isNew = FALSE;
									if (args)
										args->get_IsNewDocument(&isNew);
									ev.IsNewDocument = (isNew != FALSE);
									LPWSTR raw = nullptr;
									if (_webview && SUCCEEDED(_webview->get_Source(&raw)) && raw)
									{
										ev.Uri = raw;
										_cachedSource = ev.Uri;
										CoTaskMemFree(raw);
									}
									cui::framework::EventAccess::Raise(
										OnSourceChanged, this, ev);
									return S_OK;
								}).Get(),
							&_sourceChangedToken);

						_historyChangedToken.value = 0;
						_webview->add_HistoryChanged(
							Callback<ICoreWebView2HistoryChangedEventHandler>(
								[this, lifetime](ICoreWebView2* sender, IUnknown* args) -> HRESULT
								{
									if (!lifetime->load(std::memory_order_acquire)) return S_OK;
									(void)sender;
									(void)args;
									WebBrowser::HistoryChangedArgs ev;
									BOOL canBack = FALSE;
									BOOL canForward = FALSE;
									if (_webview)
									{
										_webview->get_CanGoBack(&canBack);
										_webview->get_CanGoForward(&canForward);
									}
									ev.CanGoBack = (canBack != FALSE);
									ev.CanGoForward = (canForward != FALSE);
									cui::framework::EventAccess::Raise(
										OnHistoryChanged, this, ev);
									return S_OK;
								}).Get(),
							&_historyChangedToken);

						_documentTitleChangedToken.value = 0;
						_webview->add_DocumentTitleChanged(
							Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
								[this, lifetime](ICoreWebView2* sender, IUnknown* args) -> HRESULT
								{
									if (!lifetime->load(std::memory_order_acquire)) return S_OK;
									(void)sender;
									(void)args;
									WebBrowser::DocumentTitleChangedArgs ev;
									LPWSTR raw = nullptr;
									if (_webview && SUCCEEDED(_webview->get_DocumentTitle(&raw)) && raw)
									{
										ev.Title = raw;
										_cachedTitle = ev.Title;
										CoTaskMemFree(raw);
									}
									cui::framework::EventAccess::Raise(
										OnDocumentTitleChanged, this, ev);
									return S_OK;
								}).Get(),
							&_documentTitleChangedToken);

						_newWindowRequestedToken.value = 0;
						_webview->add_NewWindowRequested(
							Callback<ICoreWebView2NewWindowRequestedEventHandler>(
								[this, lifetime](ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT
								{
									if (!lifetime->load(std::memory_order_acquire)) return S_OK;
									(void)sender;
									WebBrowser::NewWindowRequestedArgs ev;
									LPWSTR raw = nullptr;
									if (args && SUCCEEDED(args->get_Uri(&raw)) && raw)
									{
										ev.Uri = raw;
										CoTaskMemFree(raw);
									}
									BOOL isUser = FALSE;
									if (args)
										args->get_IsUserInitiated(&isUser);
									ev.IsUserInitiated = (isUser != FALSE);
									cui::framework::EventAccess::Raise(
										OnNewWindowRequested, this, ev);
									if (args && ev.Handled)
										args->put_Handled(TRUE);
									return S_OK;
								}).Get(),
							&_newWindowRequestedToken);

						_processFailedToken.value = 0;
						_webview->add_ProcessFailed(
							Callback<ICoreWebView2ProcessFailedEventHandler>(
								[this, lifetime](ICoreWebView2* sender, ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT
								{
									if (!lifetime->load(std::memory_order_acquire)) return S_OK;
									(void)sender;
									WebBrowser::ProcessFailedArgs ev;
									COREWEBVIEW2_PROCESS_FAILED_KIND kind = COREWEBVIEW2_PROCESS_FAILED_KIND_UNKNOWN_PROCESS_EXITED;
									if (args)
										args->get_ProcessFailedKind(&kind);
									ev.Kind = static_cast<int>(kind);
									cui::framework::EventAccess::Raise(
										OnProcessFailed, this, ev);
									return S_OK;
								}).Get(),
							&_processFailedToken);
					}

					_initializationState = InitializationState::Ready;
					_lastInitHr = S_OK;

					// URL 与 HTML 共用一个“最后写入获胜”的延迟导航槽。
					const auto pendingKind = _pendingKind;
					auto pendingUrl = std::move(_pendingUrl);
					auto pendingHtml = std::move(_pendingHtml);
					ClearPendingNavigation();
					if (pendingKind == PendingNavigationKind::Html)
						TrySetHtml(pendingHtml);
					else if (pendingKind == PendingNavigationKind::Url)
						TryNavigate(pendingUrl);

					this->InvalidateVisual();
					return S_OK;
				});

			ComPtr<ICoreWebView2Environment3> env3;
			HRESULT hrEnv3 = env->QueryInterface(IID_PPV_ARGS(&env3));
			if (FAILED(hrEnv3) || !env3)
			{
				_lastControllerHr = hrEnv3;
				_lastInitHr = FAILED(hrEnv3) ? hrEnv3 : E_NOINTERFACE;
				_initializationState = InitializationState::Failed;
				this->InvalidateVisual();
				return S_OK;
			}
			const HRESULT createControllerHr =
				env3->CreateCoreWebView2CompositionController(
					this->GetPresentationWindow()->Handle, ctlCompleted.Get());
			if (FAILED(createControllerHr))
			{
				_lastControllerHr = createControllerHr;
				_lastInitHr = createControllerHr;
				_initializationState = InitializationState::Failed;
				this->InvalidateVisual();
			}
			return S_OK;
		});

	HRESULT hrStart = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, envCompleted.Get());
	if (FAILED(hrStart))
	{
		_lastInitHr = hrStart;
		_lastEnvironmentHr = hrStart;
		_initializationState = InitializationState::Failed;
		this->InvalidateVisual();
	}
}

void WebBrowser::ApplyWebViewSettings()
{
	if (!_webview || !_controller) return;

	HRESULT firstFailure = S_OK;
	auto rememberFailure = [&firstFailure](HRESULT hr)
	{
		if (FAILED(hr) && SUCCEEDED(firstFailure)) firstFailure = hr;
	};

	ComPtr<ICoreWebView2Settings> settings;
	const HRESULT settingsHr = _webview->get_Settings(&settings);
	rememberFailure(settingsHr);
	if (SUCCEEDED(settingsHr) && settings)
	{
		rememberFailure(settings->put_AreDefaultContextMenusEnabled(
			_areDefaultContextMenusEnabled ? TRUE : FALSE));
		rememberFailure(settings->put_IsStatusBarEnabled(
			_isStatusBarEnabled ? TRUE : FALSE));
		rememberFailure(settings->put_IsZoomControlEnabled(
			_isZoomControlEnabled ? TRUE : FALSE));
	}
	rememberFailure(_controller->put_ZoomFactor(_zoomFactor));
	ComPtr<ICoreWebView2Controller2> controller2;
	const HRESULT controller2Hr = _controller.As(&controller2);
	rememberFailure(controller2Hr);
	if (SUCCEEDED(controller2Hr) && controller2)
	{
		auto byteChannel = [](float value)
		{
			return static_cast<BYTE>(std::lround(
				(std::clamp)(value, 0.0f, 1.0f) * 255.0f));
		};
		const COREWEBVIEW2_COLOR color{
			_defaultBackgroundColor.a <= 1e-6f ? BYTE{ 0 } : BYTE{ 255 },
			byteChannel(_defaultBackgroundColor.r),
			byteChannel(_defaultBackgroundColor.g),
			byteChannel(_defaultBackgroundColor.b) };
		rememberFailure(controller2->put_DefaultBackgroundColor(color));
	}
	_lastWebViewHr = firstFailure;
}

bool WebBrowser::EnsureInteropInstalled()
{
	if (_interopInstalled) return true;
	if (!_webviewReady || !_webview) return false;

	// 1) 注入 JS 桥：window.CUI.invoke(name, payload) -> postMessage("cui://invoke?..."), 并监听回包
	// 2) C++ 侧回包：postMessage("cui://resp?...&result=...")
	std::wstring bridgeJs =
		L"(function(){"
		L" if(window.CUI && window.CUI.__installed) return;"
		L" if(!window.chrome || !chrome.webview || !chrome.webview.postMessage) return;"
		L" const pending = new Map();"
		L" let seq = 0;"
		L" function enc(s){ return encodeURIComponent(s==null?'':String(s)); }"
		L" function post(url){ chrome.webview.postMessage(url); }"
		L" chrome.webview.addEventListener('message', function(ev){"
		L"   try{"
		L"     const messageText = String(ev.data||'');"
		L"     if(!messageText.startsWith('cui://resp?')) return;"
		L"     const q = messageText.substring('cui://resp?'.length);"
		L"     const params = new URLSearchParams(q);"
		L"     const id = params.get('id');"
		L"     const ok = params.get('ok') === '1';"
		L"     const res = decodeURIComponent(params.get('result')||'');"
		L"     const err = decodeURIComponent(params.get('error')||'');"
		L"     const p = pending.get(id);"
		L"     if(!p) return;"
		L"     pending.delete(id);"
		L"     ok ? p.resolve(res) : p.reject(new Error(err||'CUI invoke failed'));"
		L"   }catch(e){}"
		L" });"
		L" window.CUI = {"
		L"   __installed:true,"
		L"   invoke: function(name, payload){"
		L"     const id = String(++seq);"
		L"     const url = 'cui://invoke?id='+id+'&name='+enc(name)+'&payload='+enc(payload);"
		L"     return new Promise(function(resolve,reject){"
		L"       pending.set(id,{resolve:resolve,reject:reject});"
		L"       post(url);"
		L"     });"
		L"   },"
		L"   notify: function(name, payload){"
		L"     const url = 'cui://notify?name='+enc(name)+'&payload='+enc(payload);"
		L"     post(url);"
		L"   }"
		L" };"
		L"})();";

	// 让桥在每次文档创建时都存在（Navigate / NavigateToString 都覆盖）。
	const HRESULT bridgeInstallHr =
		_webview->AddScriptToExecuteOnDocumentCreated(bridgeJs.c_str(), nullptr);
	if (FAILED(bridgeInstallHr))
	{
		_lastWebViewHr = bridgeInstallHr;
		return false;
	}

	// WebMessageReceived：接收 JS -> C++
	_webMessageToken.value = 0;
	auto lifetime = _lifetime;
	const HRESULT webMessageHr = _webview->add_WebMessageReceived(
		Callback<ICoreWebView2WebMessageReceivedEventHandler>(
			[this, lifetime](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
			{
				if (!lifetime->load(std::memory_order_acquire)) return S_OK;
				(void)sender;
				if (!args || !_webview) return S_OK;

				LPWSTR raw = nullptr;
				if (FAILED(args->TryGetWebMessageAsString(&raw)) || !raw) return S_OK;
				std::wstring messageText(raw);
				CoTaskMemFree(raw);

				std::wstring action;
				std::unordered_map<std::wstring, std::wstring> q;
				if (!TryParseCuiUrl(messageText, action, q))
				{
					WebBrowser::WebMessageReceivedArgs ev{ messageText };
					cui::framework::EventAccess::Raise(
						OnWebMessageReceived, this, ev);
					return S_OK;
				}

				auto get = [&](const wchar_t* k) -> std::wstring
				{
					auto it = q.find(k);
					return (it == q.end()) ? L"" : it->second;
				};

				if (action == L"invoke")
				{
					std::wstring id = get(L"id");
					std::wstring name = UrlDecodeUtf8(get(L"name"));
					std::wstring payload = UrlDecodeUtf8(get(L"payload"));

					std::wstring ok = L"0";
					std::wstring result;
					std::wstring error;

					auto it = _invokeHandlers.find(name);
					if (it != _invokeHandlers.end() && it->second)
					{
						try
						{
							result = it->second(payload);
							if (!lifetime->load(std::memory_order_acquire)) return S_OK;
							ok = L"1";
						}
						catch (...)
						{
							error = L"handler exception";
						}
					}
					else
					{
						error = L"handler not found: " + name;
					}

					std::wstring resp =
						L"cui://resp?id=" + UrlEncodeUtf8(id) +
						L"&ok=" + ok +
						L"&result=" + UrlEncodeUtf8(result) +
						L"&error=" + UrlEncodeUtf8(error);
					_webview->PostWebMessageAsString(resp.c_str());
				}
				else if (action == L"notify")
				{
					// 预留：如需可在这里扩展 OnNotify 事件
				}
				else
				{
					WebBrowser::WebMessageReceivedArgs ev{ messageText };
					cui::framework::EventAccess::Raise(
						OnWebMessageReceived, this, ev);
				}
				return S_OK;
			}).Get(),
		&_webMessageToken);
	if (FAILED(webMessageHr))
	{
		_lastWebViewHr = webMessageHr;
		return false;
	}

	_interopInstalled = true;
	return true;
}

void WebBrowser::EnsureControllerBounds()
{
	if (!this->GetPresentationWindow() || !this->GetPresentationWindow()->Handle) return;

	// The WebView controller owns a local physical-pixel surface. DirectComposition
	// then projects that surface through the same local-to-render matrix used by
	// CUI drawing and inverse hit testing.
	const float dpiSc = this->GetPresentationWindow()->GetDpiScale();
	const auto size = this->GetActualSizeDip();
	const int top = this->GetPresentationWindow()->GetTitleBarHeightPixels();
	const int w = (std::max)(1,
		static_cast<int>(std::ceil(size.width * dpiSc)));
	const int h = (std::max)(1,
		static_cast<int>(std::ceil(size.height * dpiSc)));

	struct ClipEntry final
	{
		D2D1_RECT_F Rect{};
		D2D1_MATRIX_3X2_F ToRoot{};
	};
	std::vector<ClipEntry> clips;
	for (auto* current = this->GetVisualParent(); current;
		current = current->GetVisualParent())
	{
		if (current->ClipsChildren())
		{
			clips.push_back(ClipEntry{
				current->GetVisualChildrenClipRect(),
				current->GetLocalToRenderTransform() });
		}
		if (cui::framework::PresentationAccess::
			BreaksVisualPresentationInheritance(*current))
			break;
	}
	std::reverse(clips.begin(), clips.end());

	bool compositionReady = EnsureCompositionClipLayerCount(clips.size());
	bool transformReady = compositionReady;
	auto record = [&](HRESULT result)
	{
		if (FAILED(result))
		{
			_lastControllerHr = result;
			transformReady = false;
		}
	};
	auto asMatrix = [](const D2D1_MATRIX_3X2_F& value)
	{
		return D2D1::Matrix3x2F(
			value._11, value._12, value._21,
			value._22, value._31, value._32);
	};
	auto relativeTransform = [&](const D2D1_MATRIX_3X2_F& child,
		const D2D1_MATRIX_3X2_F& parent,
		D2D1_MATRIX_3X2_F& result)
	{
		auto inverseParent = asMatrix(parent);
		if (!inverseParent.Invert()) return false;
		result = asMatrix(child) * inverseParent;
		return true;
	};

	// RenderTransform is a compositor concern.  Do not renegotiate the browser
	// viewport for transform-only frames: changing Bounds can relayout the page
	// and reallocate browser-side surfaces, whereas DComp can project the stable
	// local surface without either cost.
	if (_controller
		&& (w != _controllerBoundsWidth || h != _controllerBoundsHeight))
	{
		RECT rc{ 0,0,w,h };
		const HRESULT boundsResult = _controller->put_Bounds(rc);
		record(boundsResult);
		if (SUCCEEDED(boundsResult))
		{
			_controllerBoundsWidth = w;
			_controllerBoundsHeight = h;
		}
	}

	if (_dcompBoundaryClip)
	{
		const float clipWidth = (std::max)(0.0f, size.width * dpiSc);
		const float clipHeight = (std::max)(0.0f, size.height * dpiSc);
		const auto radii = cui::dcomp_detail::ResolveRoundedClipRadii(
			clipWidth, clipHeight, dpiSc,
			_cornerRadius.TopLeft, _cornerRadius.TopRight,
			_cornerRadius.BottomRight, _cornerRadius.BottomLeft);
		record(_dcompBoundaryClip->SetLeft(0.0f));
		record(_dcompBoundaryClip->SetTop(0.0f));
		record(_dcompBoundaryClip->SetRight(clipWidth));
		record(_dcompBoundaryClip->SetBottom(clipHeight));
		record(_dcompBoundaryClip->SetTopLeftRadiusX(radii.TopLeft));
		record(_dcompBoundaryClip->SetTopLeftRadiusY(radii.TopLeft));
		record(_dcompBoundaryClip->SetTopRightRadiusX(radii.TopRight));
		record(_dcompBoundaryClip->SetTopRightRadiusY(radii.TopRight));
		record(_dcompBoundaryClip->SetBottomRightRadiusX(radii.BottomRight));
		record(_dcompBoundaryClip->SetBottomRightRadiusY(radii.BottomRight));
		record(_dcompBoundaryClip->SetBottomLeftRadiusX(radii.BottomLeft));
		record(_dcompBoundaryClip->SetBottomLeftRadiusY(radii.BottomLeft));
	}

	if (compositionReady && _dcompBoundaryVisual)
	{
		const auto browserToRoot = GetLocalToRenderTransform();
		if (clips.empty())
		{
			record(_dcompBoundaryVisual->SetTransform(
				cui::dcomp_detail::DipTransformToPhysicalPixels(
					browserToRoot, dpiSc, static_cast<float>(top))));
		}
		else
		{
			record(_dcompClipLayers.front().Visual->SetTransform(
				cui::dcomp_detail::DipTransformToPhysicalPixels(
					clips.front().ToRoot, dpiSc, static_cast<float>(top))));
			for (std::size_t index = 0; index < clips.size(); ++index)
			{
				const auto& rect = clips[index].Rect;
				auto& layer = _dcompClipLayers[index];
				record(layer.Clip->SetLeft(rect.left * dpiSc));
				record(layer.Clip->SetTop(rect.top * dpiSc));
				record(layer.Clip->SetRight(rect.right * dpiSc));
				record(layer.Clip->SetBottom(rect.bottom * dpiSc));
				if (index == 0) continue;
				D2D1_MATRIX_3X2_F relative{};
				if (!relativeTransform(
					clips[index].ToRoot, clips[index - 1].ToRoot, relative))
				{
					transformReady = false;
					continue;
				}
				record(layer.Visual->SetTransform(
					cui::dcomp_detail::DipTransformToPhysicalPixels(
						relative, dpiSc)));
			}
			D2D1_MATRIX_3X2_F boundaryRelative{};
			if (!relativeTransform(
				browserToRoot, clips.back().ToRoot, boundaryRelative))
				transformReady = false;
			else
				record(_dcompBoundaryVisual->SetTransform(
					cui::dcomp_detail::DipTransformToPhysicalPixels(
						boundaryRelative, dpiSc)));
		}
	}

	bool hasArea = size.width > 0.0f && size.height > 0.0f;
	for (const auto& clip : clips)
		hasArea = hasArea && clip.Rect.right > clip.Rect.left
			&& clip.Rect.bottom > clip.Rect.top;
	const bool parentEnabled = ::IsWindowEnabled(
		this->GetPresentationWindow()->Handle) != FALSE;
	const bool visible = parentEnabled && this->IsVisible && _webviewReady
		&& hasArea && compositionReady && transformReady;
	if (_controller)
	{
		record(_controller->put_IsVisible(visible ? TRUE : FALSE));
		record(_controller->NotifyParentWindowPositionChanged());
	}

	if (_dcompVisual)
	{
		this->GetPresentationWindow()->UpdateDCompVisualOrder(
			_dcompVisual.Get(), PresentationSceneContentLayer,
			ResolvePresentationOrder(this));

		// 关键：隐藏时断开 RootVisualTarget，避免“隐藏页残留显示上一帧”
		if (_compositionController)
		{
			if (!visible && _rootAttached)
			{
				const HRESULT rootResult =
					_compositionController->put_RootVisualTarget(nullptr);
				if (SUCCEEDED(rootResult)) _rootAttached = false;
				else _lastControllerHr = rootResult;
			}
			else if (visible && !_rootAttached)
			{
				const HRESULT rootResult =
					_compositionController->put_RootVisualTarget(
						_dcompContentVisual.Get());
				if (SUCCEEDED(rootResult)) _rootAttached = true;
				else _lastControllerHr = rootResult;
			}
		}

		// 位置/裁剪/挂载更新需要 Commit
		if (this->GetPresentationWindow()) this->GetPresentationWindow()->CommitComposition();
	}
}

void WebBrowser::OnEffectiveIsVisibleChanged(
	bool previousValue, bool currentValue)
{
	Control::OnEffectiveIsVisibleChanged(previousValue, currentValue);
	if (!_initialized && !_controller && !_dcompVisual)
		return;
	EnsureControllerBounds();
}

void WebBrowser::OnRender()
{
	EnsureInitialized();
	if (_initialized && !_dcompVisual)
		(void)RebindCompositionVisual();
	EnsureControllerBounds();

}

void WebBrowser::NotifyDeviceResourcesInvalidated() noexcept
{
	try
	{
		if (_initialized && RebindCompositionVisual())
			EnsureControllerBounds();
	}
	catch (...)
	{
	}
	Control::NotifyDeviceResourcesInvalidated();
}


bool WebBrowser::ProcessInput(const InputReport& input)
{
	const bool forwarded = ForwardMouseInputToWebView(input);
	if (forwarded && input.Kind == InputReportKind::PointerDown)
		(void)CaptureMouse();
	if (input.Kind == InputReportKind::PointerUp && IsMouseCaptured()
		&& !input.IsButtonPressed(MouseButton::Left)
		&& !input.IsButtonPressed(MouseButton::Right)
		&& !input.IsButtonPressed(MouseButton::Middle)
		&& !input.IsButtonPressed(MouseButton::XButton1)
		&& !input.IsButtonPressed(MouseButton::XButton2))
		(void)ReleaseMouseCapture();
	if (input.Kind == InputReportKind::Cancel && IsMouseCaptured())
		(void)ReleaseMouseCapture();
	Control::ProcessInput(input);
	return true;
}

bool WebBrowser::TryGetSystemCursorId(UINT32& outId) const
{
	if (!_webviewReady || !_compositionController) return false;
	if (!_hasSystemCursorId) return false;
	outId = _lastSystemCursorId;
	return true;
}

bool WebBrowser::ForwardMouseInputToWebView(const InputReport& input)
{
	if (!_webviewReady || !_compositionController) return false;
	if (!this->IsVisible) return false;

	COREWEBVIEW2_MOUSE_EVENT_KIND kind{};
	UINT32 mouseData = 0;

	switch (input.Kind)
	{
	case InputReportKind::PointerMove:
		kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MOVE;
		break;
	case InputReportKind::PointerLeave:
	case InputReportKind::CaptureLost:
	case InputReportKind::Cancel:
		kind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEAVE;
		break;
	case InputReportKind::PointerDown:
		switch (input.ChangedButton)
		{
		case MouseButton::Left:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN; break;
		case MouseButton::Right:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOWN; break;
		case MouseButton::Middle:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOWN; break;
		case MouseButton::XButton1:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_DOWN;
			mouseData = 1;
			break;
		case MouseButton::XButton2:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_DOWN;
			mouseData = 2;
			break;
		default: return false;
		}
		break;
	case InputReportKind::PointerUp:
		switch (input.ChangedButton)
		{
		case MouseButton::Left:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_UP; break;
		case MouseButton::Right:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_UP; break;
		case MouseButton::Middle:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_UP; break;
		case MouseButton::XButton1:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_UP;
			mouseData = 1;
			break;
		case MouseButton::XButton2:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_UP;
			mouseData = 2;
			break;
		default: return false;
		}
		break;
	case InputReportKind::PointerDoubleClick:
		switch (input.ChangedButton)
		{
		case MouseButton::Left:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOUBLE_CLICK;
			break;
		case MouseButton::Right:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOUBLE_CLICK;
			break;
		case MouseButton::Middle:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOUBLE_CLICK;
			break;
		case MouseButton::XButton1:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_DOUBLE_CLICK;
			mouseData = 1;
			break;
		case MouseButton::XButton2:
			kind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_DOUBLE_CLICK;
			mouseData = 2;
			break;
		default: return false;
		}
		break;
	case InputReportKind::MouseWheel:
		kind = COREWEBVIEW2_MOUSE_EVENT_KIND_WHEEL;
		mouseData = static_cast<UINT32>(input.WheelDelta);
		break;
	case InputReportKind::HorizontalMouseWheel:
		kind = COREWEBVIEW2_MOUSE_EVENT_KIND_HORIZONTAL_WHEEL;
		mouseData = static_cast<UINT32>(input.WheelDelta);
		break;
	default:
		return false;
	}

	COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)0;
	if (input.IsButtonPressed(MouseButton::Left)) vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(vkeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_LEFT_BUTTON);
	if (input.IsButtonPressed(MouseButton::Right)) vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(vkeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_RIGHT_BUTTON);
	if (input.IsButtonPressed(MouseButton::Middle)) vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(vkeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_MIDDLE_BUTTON);
	if (input.IsButtonPressed(MouseButton::XButton1)) vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(vkeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON1);
	if (input.IsButtonPressed(MouseButton::XButton2)) vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(vkeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON2);
	if (input.HasModifier(ModifierKeys::Control)) vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(vkeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_CONTROL);
	if (input.HasModifier(ModifierKeys::Shift)) vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(vkeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_SHIFT);

	// Window already inverse-transformed the report to WebBrowser-local DIPs.
	// SendMouseInput consumes physical pixels within the controller surface.
	const float dpiSc = (this->GetPresentationWindow() ? this->GetPresentationWindow()->GetDpiScale() : 1.0f);
	POINT pt{
		static_cast<LONG>(std::lround(input.X * dpiSc)),
		static_cast<LONG>(std::lround(input.Y * dpiSc)) };
	if (kind == COREWEBVIEW2_MOUSE_EVENT_KIND_LEAVE)
	{
		vkeys = static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(0);
		mouseData = 0;
		pt = POINT{};
	}
	const HRESULT sendResult = _compositionController->SendMouseInput(
		kind, vkeys, mouseData, pt);
	if (FAILED(sendResult))
	{
		_lastControllerHr = sendResult;
		return false;
	}

	// 尽量同步光标（Window 的 UpdateCursor 会覆盖一次，这里在鼠标移动时再补一刀）
	if (input.Kind == InputReportKind::PointerMove
		&& this->GetPresentationWindow() && this->IsMouseOver)
	{
		UINT32 id = 0;
		if (SUCCEEDED(_compositionController->get_SystemCursorId(&id)) && id != 0)
		{
			_lastSystemCursorId = id;
			_hasSystemCursorId = true;
			auto h = LoadCursorW(nullptr, MAKEINTRESOURCEW((ULONG_PTR)id));
			if (h) ::SetCursor(h);
		}
	}

	// 点入时尝试把焦点交给 WebView
	if (_controller && input.Kind == InputReportKind::PointerDown)
	{
		_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
	}

	return true;
}

bool WebBrowser::TryNavigate(const std::wstring& url)
{
	if (url.empty()) return false;
	if (_initializationState == InitializationState::Failed
		|| _initializationState == InitializationState::Unsupported)
		return false;
	if (!_webviewReady || !_webview)
	{
		_pendingKind = PendingNavigationKind::Url;
		_pendingUrl = url;
		_pendingHtml.clear();
		return true;
	}
	const HRESULT hr = _webview->Navigate(url.c_str());
	_lastWebViewHr = hr;
	return SUCCEEDED(hr);
}

bool WebBrowser::TrySetHtml(const std::wstring& html)
{
	if (_initializationState == InitializationState::Failed
		|| _initializationState == InitializationState::Unsupported)
		return false;
	if (!_webviewReady || !_webview)
	{
		_pendingKind = PendingNavigationKind::Html;
		_pendingHtml = html;
		_pendingUrl.clear();
		return true;
	}
	const HRESULT hr = _webview->NavigateToString(html.c_str());
	_lastWebViewHr = hr;
	return SUCCEEDED(hr);
}

bool WebBrowser::TryReload()
{
	if (!_webview) return false;
	const HRESULT hr = _webview->Reload();
	_lastWebViewHr = hr;
	return SUCCEEDED(hr);
}

bool WebBrowser::TryStop()
{
	if (!_webview) return false;
	const HRESULT hr = _webview->Stop();
	_lastWebViewHr = hr;
	return SUCCEEDED(hr);
}

bool WebBrowser::TryGoBack()
{
	if (!_webview || !CanGoBack()) return false;
	const HRESULT hr = _webview->GoBack();
	_lastWebViewHr = hr;
	return SUCCEEDED(hr);
}

bool WebBrowser::TryGoForward()
{
	if (!_webview || !CanGoForward()) return false;
	const HRESULT hr = _webview->GoForward();
	_lastWebViewHr = hr;
	return SUCCEEDED(hr);
}

void WebBrowser::Navigate(const std::wstring& url) { (void)TryNavigate(url); }
void WebBrowser::SetHtml(const std::wstring& html) { (void)TrySetHtml(html); }
void WebBrowser::Reload() { (void)TryReload(); }
void WebBrowser::Stop() { (void)TryStop(); }
void WebBrowser::GoBack() { (void)TryGoBack(); }
void WebBrowser::GoForward() { (void)TryGoForward(); }

bool WebBrowser::CanGoBack() const
{
	if (!_webview) return false;
	BOOL v = FALSE;
	_webview->get_CanGoBack(&v);
	return v != FALSE;
}

bool WebBrowser::CanGoForward() const
{
	if (!_webview) return false;
	BOOL v = FALSE;
	_webview->get_CanGoForward(&v);
	return v != FALSE;
}

std::wstring WebBrowser::GetSource() const
{
	if (_webview)
	{
		LPWSTR raw = nullptr;
		if (SUCCEEDED(_webview->get_Source(&raw)) && raw)
		{
			std::wstring s = raw;
			CoTaskMemFree(raw);
			return s;
		}
	}
	return _cachedSource;
}

std::wstring WebBrowser::GetDocumentTitle() const
{
	if (_webview)
	{
		LPWSTR raw = nullptr;
		if (SUCCEEDED(_webview->get_DocumentTitle(&raw)) && raw)
		{
			std::wstring s = raw;
			CoTaskMemFree(raw);
			return s;
		}
	}
	return _cachedTitle;
}

double WebBrowser::GetZoomFactor() const
{
	return _zoomFactor;
}

void WebBrowser::SetZoomFactor(double factor)
{
	if (!std::isfinite(factor)) return;
	factor = (std::clamp)(factor, 0.25, 5.0);
	if (SetPropertyField(ZoomFactorProperty(), _impl->zoomFactor, factor))
		ApplyWebViewSettings();
}

void WebBrowser::ExecuteScriptAsync(const std::wstring& script,
	std::function<void(HRESULT hr, const std::wstring& jsonResult)> callback)
{
	if (!_webviewReady || !_webview)
	{
		if (callback) callback(E_PENDING, L"");
		return;
	}

	auto lifetime = _lifetime;
	auto scriptCallback = Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
		[callback, this, lifetime](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT
		{
			if (!lifetime->load(std::memory_order_acquire)) return S_OK;
			if (callback)
				callback(errorCode, resultObjectAsJson ? resultObjectAsJson : L"");
			if (!lifetime->load(std::memory_order_acquire)) return S_OK;
			this->InvalidateVisual();
			return S_OK;
		});

	_webview->ExecuteScript(script.c_str(), scriptCallback.Get());
}

void WebBrowser::GetHtmlAsync(std::function<void(HRESULT hr, const std::wstring& html)> callback)
{
	ExecuteScriptAsync(L"document.documentElement.outerHTML",
		[callback](HRESULT hr, const std::wstring& json) {
			if (callback) callback(hr, JsonUnquote(json));
		});
}

void WebBrowser::SetElementInnerHtmlAsync(const std::wstring& cssSelector, const std::wstring& html,
	std::function<void(HRESULT hr)> callback)
{
	std::wstring script =
		L"(function(){"
		L"const el=document.querySelector(" + JsStringLiteral(cssSelector) + L");"
		L"if(el){el.innerHTML=" + JsStringLiteral(html) + L"; return true;} return false;"
		L"})();";

	ExecuteScriptAsync(script, [callback](HRESULT hr, const std::wstring&) {
		if (callback) callback(hr);
		});
}

void WebBrowser::QuerySelectorAllOuterHtmlAsync(const std::wstring& cssSelector,
	std::function<void(HRESULT hr, const std::wstring& jsonArray)> callback)
{
	std::wstring script =
		L"(function(){"
		L"const nodes=[...document.querySelectorAll(" + JsStringLiteral(cssSelector) + L")];"
		L"return nodes.map(n=>n.outerHTML);"
		L"})();";

	ExecuteScriptAsync(script, callback);
}

#else

#include "WebBrowser.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <utility>

struct WebBrowser::Impl
{
	InitializationState initializationState = InitializationState::Unsupported;
	HRESULT lastInitHr = E_NOTIMPL;
	HRESULT lastEnvironmentHr = E_NOTIMPL;
	HRESULT lastControllerHr = E_NOTIMPL;
	HRESULT lastWebViewHr = E_NOTIMPL;
	double zoomFactor = 1.0;
	bool areDefaultContextMenusEnabled = true;
	bool isStatusBarEnabled = false;
	bool isZoomControlEnabled = true;
	D2D1_COLOR_F defaultBackgroundColor = Colors::White;
	::CornerRadius cornerRadius{};
	std::wstring initialUrl;
	std::unordered_map<std::wstring, JsInvokeHandler> invokeHandlers;
	std::shared_ptr<std::atomic<bool>> lifetime =
		std::make_shared<std::atomic<bool>>(true);
};

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<WebBrowser, TValue> UnsupportedWebPropertyOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			DependencyPropertyEditorKind editor))
	{
		DependencyPropertyOptions<WebBrowser, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::None;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Web";
		options.Design.CategoryOrder = 170;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return options;
	}

	auto UnsupportedWebPropertySubscriber(
		const DependencyProperty& (*propertyAccessor)())
	{
		return [propertyAccessor](
			WebBrowser& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyAccessor, handler = std::move(handler)](
					DependencyObject*, const DependencyPropertyChangedEventArgs& args)
				{
					if (args.Property == &propertyAccessor())
						handler();
				});
		};
	}
}

WebBrowser::WebBrowser()
	: _impl(std::make_unique<Impl>())
{
	this->RendererBackgroundColor = Colors::White;
}

WebBrowser::~WebBrowser()
{
	_impl->lifetime->store(false, std::memory_order_release);
}

void WebBrowser::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)InitialUrlProperty();
	(void)ZoomFactorProperty();
	(void)AreDefaultContextMenusEnabledProperty();
	(void)IsStatusBarEnabledProperty();
	(void)IsZoomControlEnabledProperty();
	(void)DefaultBackgroundColorProperty();
	(void)CornerRadiusProperty();
#endif
}

const DependencyProperty& WebBrowser::ZoomFactorProperty()
{
	static const auto registration = []
	{
		auto options = UnsupportedWebPropertyOptions(
			1.0 CUI_DESIGN_METADATA_ARGUMENTS(
				20, DependencyPropertyEditorKind::Number));
		options.Validate = [](const double& proposed)
		{ return std::isfinite(proposed); };
		options.Coerce = [](WebBrowser&, const double& proposed)
			-> std::optional<double>
		{
			return (std::clamp)(proposed, 0.25, 5.0);
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = 0.25;
		options.Design.Maximum = 5.0;
		options.Design.Step = 0.05;
		)
		return DependencyPropertyRegistry::RegisterStatic<WebBrowser, double>(
			DependencyPropertyRegistrationLiteral(L"ZoomFactor"),
			[](WebBrowser& target) { return target.GetZoomFactor(); },
			[](WebBrowser& target, const double& value)
			{ target.SetZoomFactor(value); },
			UnsupportedWebPropertySubscriber(&WebBrowser::ZoomFactorProperty),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& WebBrowser::AreDefaultContextMenusEnabledProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<WebBrowser, bool>(
			DependencyPropertyRegistrationLiteral(
				L"AreDefaultContextMenusEnabled"),
			[](WebBrowser& target)
			{ return target.GetAreDefaultContextMenusEnabled(); },
			[](WebBrowser& target, const bool& value)
			{ target.SetAreDefaultContextMenusEnabled(value); },
			UnsupportedWebPropertySubscriber(
				&WebBrowser::AreDefaultContextMenusEnabledProperty),
			UnsupportedWebPropertyOptions(
				true CUI_DESIGN_METADATA_ARGUMENTS(
					30, DependencyPropertyEditorKind::Boolean)));
	return *registration;
}

const DependencyProperty& WebBrowser::IsStatusBarEnabledProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<WebBrowser, bool>(
			DependencyPropertyRegistrationLiteral(L"IsStatusBarEnabled"),
			[](WebBrowser& target) { return target.GetIsStatusBarEnabled(); },
			[](WebBrowser& target, const bool& value)
			{ target.SetIsStatusBarEnabled(value); },
			UnsupportedWebPropertySubscriber(
				&WebBrowser::IsStatusBarEnabledProperty),
			UnsupportedWebPropertyOptions(
				false CUI_DESIGN_METADATA_ARGUMENTS(
					40, DependencyPropertyEditorKind::Boolean)));
	return *registration;
}

const DependencyProperty& WebBrowser::IsZoomControlEnabledProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<WebBrowser, bool>(
			DependencyPropertyRegistrationLiteral(L"IsZoomControlEnabled"),
			[](WebBrowser& target) { return target.GetIsZoomControlEnabled(); },
			[](WebBrowser& target, const bool& value)
			{ target.SetIsZoomControlEnabled(value); },
			UnsupportedWebPropertySubscriber(
				&WebBrowser::IsZoomControlEnabledProperty),
			UnsupportedWebPropertyOptions(
				true CUI_DESIGN_METADATA_ARGUMENTS(
					50, DependencyPropertyEditorKind::Boolean)));
	return *registration;
}

const DependencyProperty& WebBrowser::DefaultBackgroundColorProperty()
{
	static const auto registration = []
	{
		auto options = UnsupportedWebPropertyOptions(
			Colors::White CUI_DESIGN_METADATA_ARGUMENTS(
				60, DependencyPropertyEditorKind::Color));
		options.Validate = [](const D2D1_COLOR_F& proposed)
		{
			auto channel = [](float value)
			{
				return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
			};
			return channel(proposed.r) && channel(proposed.g)
				&& channel(proposed.b) && channel(proposed.a)
				&& (proposed.a <= 1e-6f || proposed.a >= 1.0f - 1e-6f);
		};
		options.Equals = [](const D2D1_COLOR_F& left,
			const D2D1_COLOR_F& right)
		{
			return std::fabs(left.r - right.r) <= 1e-6f
				&& std::fabs(left.g - right.g) <= 1e-6f
				&& std::fabs(left.b - right.b) <= 1e-6f
				&& std::fabs(left.a - right.a) <= 1e-6f;
		};
		return DependencyPropertyRegistry::RegisterStatic<
			WebBrowser, D2D1_COLOR_F>(
				DependencyPropertyRegistrationLiteral(
					L"DefaultBackgroundColor"),
				[](WebBrowser& target)
				{ return target.GetDefaultBackgroundColor(); },
				[](WebBrowser& target, const D2D1_COLOR_F& value)
				{ target.SetDefaultBackgroundColor(value); },
				UnsupportedWebPropertySubscriber(
					&WebBrowser::DefaultBackgroundColorProperty),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& WebBrowser::CornerRadiusProperty()
{
	static const auto registration = []
	{
		auto options = UnsupportedWebPropertyOptions(
			::CornerRadius{} CUI_DESIGN_METADATA_ARGUMENTS(
				70, DependencyPropertyEditorKind::Text));
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Validate = [](const ::CornerRadius& value)
		{
			return std::isfinite(value.TopLeft) && value.TopLeft >= 0.0f
				&& std::isfinite(value.TopRight) && value.TopRight >= 0.0f
				&& std::isfinite(value.BottomRight) && value.BottomRight >= 0.0f
				&& std::isfinite(value.BottomLeft) && value.BottomLeft >= 0.0f;
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			WebBrowser, ::CornerRadius>(
				DependencyPropertyRegistrationLiteral(L"CornerRadius"),
				[](WebBrowser& target) { return target.GetCornerRadius(); },
				[](WebBrowser& target, const ::CornerRadius& value)
				{ target.SetCornerRadius(value); },
				UnsupportedWebPropertySubscriber(
					&WebBrowser::CornerRadiusProperty),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& WebBrowser::InitialUrlProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<WebBrowser, std::wstring>(
			DependencyPropertyRegistrationLiteral(L"InitialUrl"),
			[](WebBrowser& target) { return target.GetInitialUrl(); },
			[](WebBrowser& target, const std::wstring& value)
			{ target.SetInitialUrl(value); },
			UnsupportedWebPropertySubscriber(&WebBrowser::InitialUrlProperty),
			UnsupportedWebPropertyOptions(
				std::wstring{} CUI_DESIGN_METADATA_ARGUMENTS(
					10, DependencyPropertyEditorKind::Text)));
	return *registration;
}

bool WebBrowser::TryInitialize() { return false; }
bool WebBrowser::IsInitialized() const { return false; }
bool WebBrowser::IsWebViewReady() const { return false; }
WebBrowser::InitializationState WebBrowser::GetInitializationState() const
{
	return _impl->initializationState;
}
HRESULT WebBrowser::GetLastInitializationError() const { return _impl->lastInitHr; }
HRESULT WebBrowser::GetLastEnvironmentError() const { return _impl->lastEnvironmentHr; }
HRESULT WebBrowser::GetLastControllerError() const { return _impl->lastControllerHr; }
HRESULT WebBrowser::GetLastWebViewError() const { return _impl->lastWebViewHr; }

bool WebBrowser::TryNavigate(const std::wstring&) { return false; }
bool WebBrowser::TrySetHtml(const std::wstring&) { return false; }
bool WebBrowser::TryReload() { return false; }
bool WebBrowser::TryStop() { return false; }
bool WebBrowser::TryGoBack() { return false; }
bool WebBrowser::TryGoForward() { return false; }
void WebBrowser::Navigate(const std::wstring& value) { (void)TryNavigate(value); }
void WebBrowser::SetHtml(const std::wstring& value) { (void)TrySetHtml(value); }
void WebBrowser::Reload() { (void)TryReload(); }
void WebBrowser::Stop() { (void)TryStop(); }
void WebBrowser::GoBack() { (void)TryGoBack(); }
void WebBrowser::GoForward() { (void)TryGoForward(); }
bool WebBrowser::CanGoBack() const { return false; }
bool WebBrowser::CanGoForward() const { return false; }
std::wstring WebBrowser::GetSource() const { return L""; }
std::wstring WebBrowser::GetDocumentTitle() const { return L""; }
bool WebBrowser::IsNavigating() const { return false; }
bool WebBrowser::IsWebViewVisible() const { return false; }
bool WebBrowser::HasPendingNavigation() const { return false; }
WebBrowser::PendingNavigationKind WebBrowser::GetPendingNavigationKind() const
{
	return PendingNavigationKind::None;
}
std::wstring WebBrowser::GetPendingUrl() const { return L""; }
void WebBrowser::ClearPendingNavigation() {}

double WebBrowser::GetZoomFactor() const { return _impl->zoomFactor; }
void WebBrowser::SetZoomFactor(double value)
{
	if (!std::isfinite(value)) return;
	SetPropertyField(ZoomFactorProperty(), _impl->zoomFactor,
		(std::clamp)(value, 0.25, 5.0));
}
bool WebBrowser::GetAreDefaultContextMenusEnabled() const
{
	return _impl->areDefaultContextMenusEnabled;
}
void WebBrowser::SetAreDefaultContextMenusEnabled(bool value)
{
	SetPropertyField(AreDefaultContextMenusEnabledProperty(),
		_impl->areDefaultContextMenusEnabled, value);
}
bool WebBrowser::GetIsStatusBarEnabled() const
{
	return _impl->isStatusBarEnabled;
}
void WebBrowser::SetIsStatusBarEnabled(bool value)
{
	SetPropertyField(
		IsStatusBarEnabledProperty(), _impl->isStatusBarEnabled, value);
}
bool WebBrowser::GetIsZoomControlEnabled() const
{
	return _impl->isZoomControlEnabled;
}
void WebBrowser::SetIsZoomControlEnabled(bool value)
{
	SetPropertyField(
		IsZoomControlEnabledProperty(), _impl->isZoomControlEnabled, value);
}
D2D1_COLOR_F WebBrowser::GetDefaultBackgroundColor() const
{
	return _impl->defaultBackgroundColor;
}
void WebBrowser::SetDefaultBackgroundColor(D2D1_COLOR_F value)
{
	(void)SetPropertyField(DefaultBackgroundColorProperty(),
		_impl->defaultBackgroundColor, value);
}
::CornerRadius WebBrowser::GetCornerRadius() const
{
	return _impl->cornerRadius;
}
void WebBrowser::SetCornerRadius(::CornerRadius value)
{
	(void)SetPropertyField(
		CornerRadiusProperty(), _impl->cornerRadius, value);
}
std::wstring WebBrowser::GetInitialUrl() const { return _impl->initialUrl; }
void WebBrowser::SetInitialUrl(std::wstring value)
{
	SetPropertyField(InitialUrlProperty(), _impl->initialUrl, std::move(value));
}

void WebBrowser::ExecuteScriptAsync(
	const std::wstring&,
	std::function<void(HRESULT, const std::wstring&)> callback)
{
	if (callback) callback(E_NOTIMPL, L"");
}
void WebBrowser::RegisterJsInvokeHandler(
	const std::wstring& name, JsInvokeHandler handler)
{
	_impl->invokeHandlers[name] = std::move(handler);
}
void WebBrowser::UnregisterJsInvokeHandler(const std::wstring& name)
{
	_impl->invokeHandlers.erase(name);
}
void WebBrowser::ClearJsInvokeHandlers() { _impl->invokeHandlers.clear(); }
void WebBrowser::GetHtmlAsync(
	std::function<void(HRESULT, const std::wstring&)> callback)
{
	if (callback) callback(E_NOTIMPL, L"");
}
void WebBrowser::SetElementInnerHtmlAsync(
	const std::wstring&, const std::wstring&, std::function<void(HRESULT)> callback)
{
	if (callback) callback(E_NOTIMPL);
}
void WebBrowser::QuerySelectorAllOuterHtmlAsync(
	const std::wstring&,
	std::function<void(HRESULT, const std::wstring&)> callback)
{
	if (callback) callback(E_NOTIMPL, L"");
}

void WebBrowser::OnRender()
{
}

void WebBrowser::OnEffectiveIsVisibleChanged(bool, bool) {}

bool WebBrowser::TryGetSystemCursorId(UINT32&) const { return false; }

bool WebBrowser::ProcessInput(const InputReport& input)
{
	(void)input;
	return true;
}

void WebBrowser::EnsureInitialized() {}
bool WebBrowser::RebindCompositionVisual() { return false; }
bool WebBrowser::EnsureCompositionClipLayerCount(std::size_t) { return false; }
bool WebBrowser::EnsureInteropInstalled() { return false; }
void WebBrowser::EnsureControllerBounds() {}
void WebBrowser::ApplyWebViewSettings() {}
void WebBrowser::NotifyDeviceResourcesInvalidated() noexcept
{
	Control::NotifyDeviceResourcesInvalidated();
}
bool WebBrowser::ForwardMouseInputToWebView(const InputReport&)
{
	return false;
}

#endif

bool WebBrowser::HandlesNavigationKey(Key key) const
{
	// Once the composition controller owns focus, every semantic key belongs to
	// the embedded document. In particular Tab/arrows must not escape through
	// Window's focus-navigation fallback before ProcessInput can forward them.
	return key != Key::None;
}

void WebBrowser::Arrange(cui::core::Rect finalRect)
{
	Control::Arrange(finalRect);
	EnsureControllerBounds();
}
