#include "DesignerCanvas.h"
#include "../CUI/include/Canvas.h"
#include "../CUI/include/EventInfrastructure.h"
#include "../CuiRuntime/include/BindingConverterRegistry.h"
#include "../CUI/include/StyleInfrastructure.h"
#include "../CUI/include/WindowInfrastructure.h"
#include "ProgrammaticControlFactory.h"
#include "DesignerBindingUtils.h"
#include "DesignerControlCatalog.h"
#include "DesignerControlPropertyCatalog.h"
#include "DesignerControlFactory.h"
#include "DesignerDataContextSchemaUtils.h"
#include "DesignerEventCatalog.h"
#include "DesignerPropertyCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include "DesignerCore/DesignerCommandCoordinator.h"
#include "DesignerCore/Commands/ControlPlacementCommand.h"
#include "DesignerCore/Commands/ControlPropertyCommand.h"
#include "DesignerCore/Commands/ControlSubtreeCommand.h"
#include "DesignerCore/Commands/EventHandlerCommand.h"
#include "DesignerCore/HitTestService.h"
#include "DesignerCore/LayoutBridge.h"
#include "DesignerCore/PropertyGridBinder.h"
#include "DesignerCore/SelectionService.h"
#include "DesignerModel/DesignDocument.h"
#include "DesignerModel/DesignDocumentClipboard.h"
#include "DesignerModel/DesignDocumentControlPool.h"
#include "DesignerModel/DesignDocumentEventIndex.h"
#include "DesignerModel/DesignDocumentFileFormat.h"
#include "DesignerModel/DesignDocumentGraph.h"
#include "../CuiRuntime/include/XamlObjectMaterializer.h"
#include "../CuiRuntime/include/XamlRuntimeSchema.h"
#include "../CUI/include/DependencyPropertyInfrastructure.h"
#include "DesignerModel/DesignDataResourceUtils.h"
#include "DesignerModel/DesignDocumentSerializer.h"
#include "DesignerModel/StoryboardPropertyPath.h"
#include "DesignerModel/XamlDocumentParser.h"
#include "DesignerModel/XamlDocumentSerializer.h"
#include <Convert.h>
#include "FakeWebBrowser.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/TextBox.h"
#include "../CUI/include/CheckBox.h"
#include "../CUI/include/RadioButton.h"
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/LoadingRing.h"
#include "../CUI/include/ProgressBar.h"
#include "../CUI/include/ProgressRing.h"
#include "../CUI/include/Slider.h"
#include "../CUI/include/NumericUpDown.h"
#include "../CUI/include/Image.h"
#include "../CUI/include/GroupBox.h"
#include "../CUI/include/Expander.h"
#include "../CUI/include/Switch.h"
#include "../CUI/include/ScrollViewer.h"
#include "../CUI/include/Popup.h"
#include "../CUI/include/RichTextBox.h"
#include "../CUI/include/PasswordBox.h"
#include "../CUI/include/ListView.h"
#include "../CUI/include/ListBox.h"
#include "../CUI/include/ItemsControl.h"
#include "../CUI/include/ItemsPresenter.h"
#include "../CUI/include/InputInfrastructure.h"
#include "../CUI/include/ContentPresenter.h"
#include "../CUI/include/ContentControl.h"
#include "../CUI/include/HeaderedContentControl.h"
#include "../CUI/include/HeaderedItemsControl.h"
#include "../CUI/include/TemplateInfrastructure.h"
#include "../CUI/include/ChartView.h"
#include "../CUI/include/TreeView.h"
#include "../CUI/include/TabControl.h"
#include "../CUI/include/ToolBar.h"
#include "../CUI/include/Menu.h"
#include "../CUI/include/StatusBar.h"
#include "../CUI/include/NativeSurface.h"
#include "../CUI/include/Layout/StackPanel.h"
#include "../CUI/include/Layout/Grid.h"
#include "../CUI/include/Layout/DockPanel.h"
#include "../CUI/include/Layout/WrapPanel.h"
#include "../CUI/include/Layout/RelativePanel.h"
#include "../CUI/include/Window.h"
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <stdexcept>
#include <utility>

#ifdef log
#undef log
#endif

using DesignValue = DesignerModel::DesignValue;

namespace
{
	std::wstring DesignerClipboardFallback;
	DWORD DesignerClipboardSequence = 0;
	bool DesignerClipboardFallbackPreferred = false;
	constexpr float DesignerMinimumViewZoom = 0.25f;
	constexpr float DesignerMaximumViewZoom = 4.0f;
	constexpr float DesignerViewZoomStep = 1.2f;
	constexpr float DesignerFitMargin = 20.0f;
	constexpr float DesignerMinimumVisibleSurface = 48.0f;

	void AssignDefaultXamlType(DesignerControl& control)
	{
		if (const auto* descriptor =
			CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(control.Type))
			control.XamlType = descriptor->TypeId;
	}

	bool ValidateCommandTargetRemoval(
		const DesignerModel::DesignDocument& document,
		const std::unordered_set<std::wstring>& removedNames,
		std::wstring* outError)
	{
		auto validateTarget = [&](const std::wstring& target,
			const std::wstring& source)
		{
			if (target.empty() || !removedNames.contains(target)) return true;
			if (outError) *outError = L"不能删除 " + target + L"："
				+ source + L" 的 CommandTarget 仍引用该 x:Name。";
			return false;
		};
		auto validateBindings = [&](const auto& bindings,
			const std::wstring& source)
		{
			for (const auto& binding : bindings)
				if (!validateTarget(
					binding.CommandTarget,
					source + L".InputBindings")) return false;
			return true;
		};
		if (!validateBindings(document.Window.InputBindings,
			document.Window.Name)) return false;
		for (const auto& node : document.Nodes)
		{
			if (removedNames.contains(node.Name)) continue;
			if (!validateBindings(node.InputBindings, node.Name)
				|| !validateTarget(node.Structure.CommandTarget, node.Name))
				return false;
		}
		return true;
	}

	void ClearManagedPlacementMetadata(DesignerModel::DesignNode& node)
	{
		for (const auto* name : {
			L"Canvas.Left", L"Canvas.Top", L"Canvas.Right", L"Canvas.Bottom",
			L"Margin", L"Grid.Row", L"Grid.Column", L"Grid.RowSpan",
			L"Grid.ColumnSpan", L"HorizontalAlignment", L"VerticalAlignment", L"DockPanel.Dock" })
			node.Properties.Remove(name);
	}

	void SetNodeLiteral(
		DesignerModel::DesignNode& node,
		std::wstring name,
		DesignerStyleValueKind kind,
		std::wstring text)
	{
		node.Properties.Set(std::move(name),
			{ { kind, std::move(text) } });
	}

	void SetNodeInteger(
		DesignerModel::DesignNode& node,
		std::wstring name,
		int value)
	{
		SetNodeLiteral(node, std::move(name),
			DesignerStyleValueKind::Int, std::to_wstring(value));
	}

	void SetNodeFloat(
		DesignerModel::DesignNode& node,
		std::wstring name,
		float value)
	{
		SetNodeLiteral(node, std::move(name),
			DesignerStyleValueKind::Float, std::to_wstring(value));
	}

	float NodeFloat(
		const DesignerModel::DesignNode& node,
		const wchar_t* name,
		float fallback = 0.0f) noexcept
	{
		const auto* assignment = node.Properties.Find(name);
		if (!assignment) return fallback;
		try
		{
			size_t consumed = 0;
			const auto result = std::stof(assignment->Value.Text, &consumed);
			return consumed == assignment->Value.Text.size()
				&& std::isfinite(result) ? result : fallback;
		}
		catch (...) { return fallback; }
	}

	Thickness NodeThickness(
		const DesignerModel::DesignNode& node,
		const wchar_t* name)
	{
		const auto* assignment = node.Properties.Find(name);
		if (!assignment) return {};
		BindingValue converted;
		Thickness result{};
		return DesignerStyleSheetUtils::TryConvertValue(
			assignment->Value, converted) && converted.TryGet(result)
			? result : Thickness{};
	}

	void SetNodeThickness(
		DesignerModel::DesignNode& node,
		std::wstring name,
		const Thickness& value)
	{
		SetNodeLiteral(node, std::move(name),
			DesignerStyleValueKind::Thickness,
			std::to_wstring(value.Left) + L","
			+ std::to_wstring(value.Top) + L","
			+ std::to_wstring(value.Right) + L","
			+ std::to_wstring(value.Bottom));
	}

	enum class ClipboardTextReadState
	{
		Text,
		NoText,
		Unavailable,
	};

	bool TryWriteClipboardText(
		const std::wstring& text,
		std::wstring* outError)
	{
		if (text.size() >= (std::numeric_limits<SIZE_T>::max)()
			/ sizeof(wchar_t))
		{
			if (outError) *outError = L"复制的 CUI XAML 过大。";
			return false;
		}
		const auto bytes = (text.size() + 1) * sizeof(wchar_t);
		auto memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (!memory)
		{
			if (outError) *outError = L"无法为系统剪贴板分配内存。";
			return false;
		}
		auto* destination = static_cast<wchar_t*>(::GlobalLock(memory));
		if (!destination)
		{
			::GlobalFree(memory);
			if (outError) *outError = L"无法写入系统剪贴板内存。";
			return false;
		}
		std::copy(text.begin(), text.end(), destination);
		destination[text.size()] = L'\0';
		::GlobalUnlock(memory);
		if (!::OpenClipboard(nullptr))
		{
			::GlobalFree(memory);
			if (outError) *outError = L"系统剪贴板正被其他程序占用。";
			return false;
		}
		struct ClipboardCloser final
		{
			~ClipboardCloser() { ::CloseClipboard(); }
		} closer;
		if (!::EmptyClipboard())
		{
			::GlobalFree(memory);
			if (outError) *outError = L"无法清空系统剪贴板。";
			return false;
		}
		if (!::SetClipboardData(CF_UNICODETEXT, memory))
		{
			::GlobalFree(memory);
			if (outError) *outError = L"无法发布 CUI XAML 到系统剪贴板。";
			return false;
		}
		if (outError) outError->clear();
		return true;
	}

	ClipboardTextReadState TryReadClipboardText(
		std::wstring& text,
		std::wstring* outError)
	{
		text.clear();
		if (!::IsClipboardFormatAvailable(CF_UNICODETEXT))
			return ClipboardTextReadState::NoText;
		if (!::OpenClipboard(nullptr))
		{
			if (outError) *outError = L"系统剪贴板正被其他程序占用。";
			return ClipboardTextReadState::Unavailable;
		}
		struct ClipboardCloser final
		{
			~ClipboardCloser() { ::CloseClipboard(); }
		} closer;
		const auto memory = ::GetClipboardData(CF_UNICODETEXT);
		if (!memory)
		{
			if (outError) *outError = L"无法读取系统剪贴板文本。";
			return ClipboardTextReadState::Unavailable;
		}
		const auto* source = static_cast<const wchar_t*>(::GlobalLock(memory));
		if (!source)
		{
			if (outError) *outError = L"无法锁定系统剪贴板文本。";
			return ClipboardTextReadState::Unavailable;
		}
		try
		{
			text.assign(source);
		}
		catch (...)
		{
			::GlobalUnlock(memory);
			if (outError) *outError = L"系统剪贴板文本无效。";
			return ClipboardTextReadState::Unavailable;
		}
		::GlobalUnlock(memory);
		if (outError) outError->clear();
		return ClipboardTextReadState::Text;
	}

	bool ParseClipboardXaml(
		const std::wstring& text,
		DesignerModel::DesignDocument& fragment,
		std::wstring* outError)
	{
		if (text.empty())
		{
			if (outError) *outError = L"剪贴板文本为空。";
			return false;
		}
		const auto utf8 = Convert::UnicodeToUtf8(text);
		std::wstring documentError;
		if (DesignerModel::XamlDocumentParser::FromXaml(
			utf8, fragment, &documentError)) return true;
		const std::string wrapped =
			"<Window xmlns=\"urn:cui\" "
			"xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
			"xmlns:d=\"urn:cui:designer\" x:Name=\"Clipboard\">"
			+ utf8 + "</Window>";
		std::wstring fragmentError;
		if (DesignerModel::XamlDocumentParser::FromXaml(
			wrapped, fragment, &fragmentError)) return true;
		if (outError)
			*outError = L"剪贴板文本不是有效的 CUI XAML 文档或控件片段："
				+ (documentError.empty() ? fragmentError : documentError);
		return false;
	}
}

struct DesignerCanvasPlacementInteraction
{
	std::wstring Operation;
	DesignerControlPlacementSnapshot Before;
	std::vector<std::wstring> BeforeSelectionNames;
	std::wstring BeforePrimarySelectionName;
};

struct DesignerCanvasPropertyInteraction
{
	std::wstring Operation;
	DesignerPropertyBatchSnapshot Before;
	std::vector<std::wstring> BeforeSelectionNames;
	std::wstring BeforePrimarySelectionName;
};

namespace
{
	std::optional<DesignerControlDescriptor> BuiltInDescriptor(UIClass type)
	{
		return DesignerControlCatalog::FindBuiltIn(type);
	}

	bool ResolvePropertyTargets(
		DesignerCanvas* canvas,
		const DesignerPropertyBatchSnapshot& snapshot,
		std::vector<std::shared_ptr<DesignerControl>>& targets,
		std::wstring* outError)
	{
		targets.clear();
		if (!canvas || snapshot.Targets.empty())
		{
			if (outError) *outError = L"属性差量没有可解析的目标。";
			return false;
		}
		targets.reserve(snapshot.Targets.size());
		for (const auto& identity : snapshot.Targets)
		{
			auto match = std::find_if(
				canvas->GetAllControls().begin(),
				canvas->GetAllControls().end(),
				[&identity](const std::shared_ptr<DesignerControl>& candidate)
				{
					return candidate && candidate->ControlInstance
						&& candidate->Name == identity.TargetName
						&& candidate->Type == identity.TargetType;
				});
			if (match == canvas->GetAllControls().end()
				|| std::find(targets.begin(), targets.end(), *match)
					!= targets.end())
			{
				if (outError)
					*outError = L"属性差量目标不存在或不唯一："
						+ identity.TargetName;
				targets.clear();
				return false;
			}
			targets.push_back(*match);
		}
		if (outError) outError->clear();
		return true;
	}

	bool CapturePropertySnapshot(
		DesignerCanvas* canvas,
		const DesignerPropertyBatchSnapshot& identity,
		DesignerPropertyBatchSnapshot& snapshot,
		std::wstring* outError)
	{
		try
		{
			std::vector<std::shared_ptr<DesignerControl>> targets;
			if (!ResolvePropertyTargets(canvas, identity, targets, outError))
				return false;
			PropertyGridBinder binder;
			binder.SetCanvas(canvas);
			binder.BindControls(targets, targets.front());
			if (!binder.CaptureControlPropertySnapshot(
				identity.PropertyName, snapshot, outError)) return false;
			if (snapshot.Source != identity.Source
				|| snapshot.PropertyName != identity.PropertyName
				|| snapshot.Targets.size() != identity.Targets.size())
			{
				if (outError) *outError = L"属性差量终点与起点不兼容。";
				return false;
			}
			for (size_t index = 0; index < snapshot.Targets.size(); ++index)
			{
				if (snapshot.Targets[index].TargetName
						!= identity.Targets[index].TargetName
					|| snapshot.Targets[index].TargetType
						!= identity.Targets[index].TargetType)
				{
					if (outError) *outError = L"属性差量目标集合发生变化。";
					return false;
				}
			}
			if (outError) outError->clear();
			return true;
		}
		catch (...)
		{
			snapshot = DesignerPropertyBatchSnapshot{};
			if (outError) *outError = L"捕获属性差量时抛出异常。";
			return false;
		}
	}

	bool RestorePropertySnapshot(
		DesignerCanvas* canvas,
		const DesignerPropertyBatchSnapshot& snapshot,
		std::wstring* outError)
	{
		try
		{
			std::vector<std::shared_ptr<DesignerControl>> targets;
			if (!ResolvePropertyTargets(canvas, snapshot, targets, outError))
				return false;
			PropertyGridBinder binder;
			binder.SetCanvas(canvas);
			binder.BindControls(targets, targets.front());
			return binder.RestoreBoundControlPropertySnapshot(snapshot, outError);
		}
		catch (...)
		{
			if (outError) *outError = L"恢复属性差量时抛出异常。";
			return false;
		}
	}
}

static RECT IntersectRectSafe(const RECT& a, const RECT& b)
{
	RECT r;
	r.left = (std::max)(a.left, b.left);
	r.top = (std::max)(a.top, b.top);
	r.right = (std::min)(a.right, b.right);
	r.bottom = (std::min)(a.bottom, b.bottom);
	if (r.right < r.left) r.right = r.left;
	if (r.bottom < r.top) r.bottom = r.top;
	return r;
}

static bool UsesAlignmentManagedPlacement(Control* control)
{
	return control && (control->HorizontalAlignment != HorizontalAlignment::Left || control->VerticalAlignment != VerticalAlignment::Top);
}

static void ResetAlignmentForManualPlacement(Control* control)
{
	if (!control) return;
	if (control->HorizontalAlignment != HorizontalAlignment::Left)
	{
		control->HorizontalAlignment = HorizontalAlignment::Left;
	}
	if (control->VerticalAlignment != VerticalAlignment::Top)
	{
		control->VerticalAlignment = VerticalAlignment::Top;
	}
}

	static void RefreshDesignerPanelLayout(Control* control)
	{
		if (!control) return;
		if (auto* panel = dynamic_cast<Panel*>(control))
	{
		panel->InvalidateLayout();
		panel->UpdateLayout();
	}
}

static bool ApplyTrackedMetadataProperty(
	DesignerControl& designerControl,
	Control& target,
	const std::wstring& propertyName,
	DesignerStyleValue value,
	bool preserveExisting,
	std::wstring* outError = nullptr)
{
	const auto* metadata = target.FindPropertyMetadata(propertyName);
	const std::wstring canonicalCandidate = metadata
		? metadata->Name() : propertyName;
	const auto existing = std::find_if(
		designerControl.MetadataProperties.begin(),
		designerControl.MetadataProperties.end(),
		[&](const auto& entry)
		{
			return entry.first == canonicalCandidate;
		});
	if (preserveExisting && existing != designerControl.MetadataProperties.end())
	{
		if (outError) outError->clear();
		return true;
	}

	std::wstring canonicalName;
	DesignerStyleValue effective;
	if (!DesignerPropertyCatalog::ApplyAndTrackValue(
		target, designerControl.MetadataProperties, propertyName, value,
		&canonicalName, &effective, outError)) return false;
	designerControl.MetadataPropertyResourceKeys.erase(canonicalName);
	designerControl.MetadataPropertyDynamicResourceKeys.erase(canonicalName);
	return true;
}

DesignerCanvas::DesignerCanvas(int x, int y, int width, int height)
	: Panel()
{
	Canvas::SetLeft(*this, static_cast<float>(x));
	Canvas::SetTop(*this, static_cast<float>(y));
	Width = static_cast<float>(width);
	Height = static_cast<float>(height);
	Focusable = true;
	_commandCoordinator = std::make_unique<DesignerCommandCoordinator>(this);
	_selectionService = std::make_unique<SelectionService>();

	// Window view state is derived once from the schema node; the node remains
	// the sole authored state from this point onward.
	(void)ApplyDesignedWindowNode(_designedWindowNode);

	// 画布（外围）与设计面板（内部）区分：设计面板负责裁剪/承载被设计控件
	this->Background = Colors::WhiteSmoke;
	this->BorderThickness = 2.0f;
	// Zoom/pan is a view transform inherited by every preview descendant.  The
	// DesignerCanvas is the viewport, so transformed children must not escape
	// into the toolbox, property grid, toolbar, or status strip.
	this->ClipToBounds = true;

	_designSurface = cui::designer::NewControl<Panel>(
		static_cast<float>(_designSurfaceOrigin.x),
		static_cast<float>(_designSurfaceOrigin.y),
		_designedWindowSize.width, _designedWindowSize.height);
	Canvas::SetLeft(*(_designSurface), static_cast<float>(_designSurfaceOrigin.x));
	Canvas::SetTop(*(_designSurface), static_cast<float>(_designSurfaceOrigin.y));
	_designSurface->Background = Colors::WhiteSmoke;
	_designSurface->BorderThickness = 0.0f; // 边框由画布统一绘制
	this->AdoptVisualChild(_designSurface);

	{
		int top = DesignedClientTop();
		int h = static_cast<int>(std::lround(
			_designedWindowSize.height)) - top;
		if (h < 0) h = 0;
		_clientSurface = cui::designer::NewControl<ContentControl>(
			0.0f, static_cast<float>(top), _designedWindowSize.width,
			static_cast<float>(h));
	}
	// This ContentControl is Designer infrastructure, not authored content.
	// Keep its direct-content host topology stable when the Designer shell's
	// implicit Generic.xaml styles are installed; a themed ContentPresenter
	// would otherwise become the runtime parent of Window.Content and break the
	// design-parent/runtime-parent invariant used by snapshots and placement.
	_clientSurface->SetTemplate(ControlTemplateReference{});
	Canvas::SetLeft(*(_clientSurface), 0.0f);
	Canvas::SetTop(*(_clientSurface), static_cast<float>(DesignedClientTop()));
	_clientSurface->Background = _designedWindowBackgroundColor;
	_designSurface->AdoptVisualChild(_clientSurface);

	RefreshDesignedWindowTypography();
	CreateDefaultContentRoot();
	// The designer document surface is a model projection and must be usable by
	// headless document operations before a Window performs its first layout.
	// Seed its visual geometry from the authoritative designed-window bounds.
	_designSurface->Arrange(cui::core::Rect{
		static_cast<float>(_designSurfaceOrigin.x),
		static_cast<float>(_designSurfaceOrigin.y),
		_designedWindowSize.width,
		_designedWindowSize.height });
	UpdateClientSurfaceLayout();
	// Clip-aware hit testing uses the arranged viewport.  Seed that geometry for
	// off-screen document operations and self-tests; the owning layout pass may
	// replace it later with the same normal Arrange contract.
	this->Arrange(cui::core::Rect{
		static_cast<float>(x), static_cast<float>(y),
		static_cast<float>(width), static_cast<float>(height) });
}

DesignerCanvas::~DesignerCanvas()
{
}

std::vector<std::wstring> DesignerCanvas::GetCompatibleEventHandlerNames(
	const DesignerEventDescriptor& requested,
	const std::wstring& defaultName,
	const std::wstring& currentName,
	const std::map<std::string, std::vector<std::wstring>>&
		compatibleUserHandlers) const
{
	std::set<std::wstring> compatible;
	if (!defaultName.empty()) compatible.insert(defaultName);
	if (!currentName.empty()) compatible.insert(currentName);
	DesignerModel::DesignDocumentEventIndex documentIndex;
	const bool hasDocumentIndex = BuildEventHandlerIndex(documentIndex, nullptr);
	if (hasDocumentIndex)
		for (const auto& handler : documentIndex.Handlers())
			if (handler.Signature == requested.Signature)
				compatible.insert(handler.Name);

	if (const auto source = compatibleUserHandlers.find(requested.ParameterList);
		source != compatibleUserHandlers.end())
	{
		for (const auto& name : source->second)
		{
			std::wstring validationError;
			if (!DesignerEventCatalog::ValidateHandlerName(
				name, &validationError)) continue;
			const auto* used = hasDocumentIndex
				? documentIndex.FindHandler(name) : nullptr;
			if (used && used->Signature != requested.Signature) continue;
			compatible.insert(name);
		}
	}

	std::vector<std::wstring> result;
	auto appendFirst = [&](const std::wstring& name)
	{
		if (name.empty()) return;
		const auto found = compatible.find(name);
		if (found == compatible.end()) return;
		result.push_back(*found);
		compatible.erase(found);
	};
	appendFirst(defaultName);
	appendFirst(currentName);
	result.insert(result.end(), compatible.begin(), compatible.end());
	return result;
}

POINT DesignerCanvas::ViewToCanvasPoint(POINT point) const
{
	const float zoom = (std::max)(_viewZoom, DesignerMinimumViewZoom);
	return POINT{
		static_cast<LONG>(std::lround(
			(static_cast<float>(point.x) - _viewOffset.x) / zoom)),
		static_cast<LONG>(std::lround(
			(static_cast<float>(point.y) - _viewOffset.y) / zoom)) };
}

POINT DesignerCanvas::CanvasToViewPoint(POINT point) const
{
	return POINT{
		static_cast<LONG>(std::lround(
			_viewOffset.x + static_cast<float>(point.x) * _viewZoom)),
		static_cast<LONG>(std::lround(
			_viewOffset.y + static_cast<float>(point.y) * _viewZoom)) };
}

D2D1_MATRIX_3X2_F DesignerCanvas::GetViewRenderTransform() const
{
	const auto absoluteLocation = GetAbsoluteLocationDip();
	return D2D1::Matrix3x2F::Scale(
		_viewZoom, _viewZoom,
		D2D1::Point2F(absoluteLocation.x, absoluteLocation.y))
		* D2D1::Matrix3x2F::Translation(
			_viewOffset.x, _viewOffset.y);
}

bool DesignerCanvas::TryGetDescendantRenderTransform(
	D2D1_MATRIX_3X2_F& transform) const
{
	transform = GetViewRenderTransform();
	return true;
}

void DesignerCanvas::NotifyViewChanged()
{
	cui::framework::EventAccess::Raise(OnViewChanged, DesignerCanvasViewChangedEventArgs{
		_viewZoom, _viewOffset, _fitToViewport });
}

void DesignerCanvas::NotifyTabOrderStateChanged()
{
	cui::framework::EventAccess::Raise(OnTabOrderStateChanged, DesignerCanvasTabOrderStateEventArgs{
		_tabOrderMode,
		_nextTabOrderIndex,
		CollectTabOrderCandidates().size() });
}

void DesignerCanvas::ClampViewOffset()
{
	const auto surface = GetDesignSurfaceRectInCanvas();
	const float viewportWidth = (std::max)(0.0f, ActualWidth);
	const float viewportHeight = (std::max)(0.0f, ActualHeight);
	if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) return;

	const float visibleX = (std::min)(
		DesignerMinimumVisibleSurface, viewportWidth * 0.5f);
	const float visibleY = (std::min)(
		DesignerMinimumVisibleSurface, viewportHeight * 0.5f);
	const float minimumX = visibleX
		- static_cast<float>(surface.right) * _viewZoom;
	const float maximumX = viewportWidth - visibleX
		- static_cast<float>(surface.left) * _viewZoom;
	const float minimumY = visibleY
		- static_cast<float>(surface.bottom) * _viewZoom;
	const float maximumY = viewportHeight - visibleY
		- static_cast<float>(surface.top) * _viewZoom;
	_viewOffset.x = (std::clamp)(_viewOffset.x, minimumX, maximumX);
	_viewOffset.y = (std::clamp)(_viewOffset.y, minimumY, maximumY);
}

void DesignerCanvas::RecalculateFitView(bool notify)
{
	const auto surface = GetDesignSurfaceRectInCanvas();
	const float surfaceWidth = static_cast<float>(surface.right - surface.left);
	const float surfaceHeight = static_cast<float>(surface.bottom - surface.top);
	const float availableWidth = (std::max)(1.0f,
		ActualWidth - DesignerFitMargin * 2.0f);
	const float availableHeight = (std::max)(1.0f,
		ActualHeight - DesignerFitMargin * 2.0f);
	if (surfaceWidth <= 0.0f || surfaceHeight <= 0.0f) return;

	_viewZoom = (std::clamp)((std::min)(
		availableWidth / surfaceWidth,
		availableHeight / surfaceHeight),
		DesignerMinimumViewZoom, DesignerMaximumViewZoom);
	_viewOffset.x = (ActualWidth
		- surfaceWidth * _viewZoom) * 0.5f
		- static_cast<float>(surface.left) * _viewZoom;
	_viewOffset.y = (ActualHeight
		- surfaceHeight * _viewZoom) * 0.5f
		- static_cast<float>(surface.top) * _viewZoom;
	_lastFitViewportSize = { ActualWidth, ActualHeight };
	this->InvalidateVisual();
	if (notify) NotifyViewChanged();
}

void DesignerCanvas::SetViewZoom(float zoom)
{
	SetViewZoom(zoom, POINT{
		static_cast<LONG>(std::lround(ActualWidth * 0.5f)),
		static_cast<LONG>(std::lround(ActualHeight * 0.5f)) });
}

void DesignerCanvas::SetViewZoom(float zoom, POINT focalPointInView)
{
	if (!std::isfinite(zoom)) return;
	zoom = (std::clamp)(zoom,
		DesignerMinimumViewZoom, DesignerMaximumViewZoom);
	const float oldZoom = (std::max)(_viewZoom, DesignerMinimumViewZoom);
	const float logicalX = (static_cast<float>(focalPointInView.x)
		- _viewOffset.x) / oldZoom;
	const float logicalY = (static_cast<float>(focalPointInView.y)
		- _viewOffset.y) / oldZoom;
	_viewZoom = zoom;
	_viewOffset.x = static_cast<float>(focalPointInView.x)
		- logicalX * _viewZoom;
	_viewOffset.y = static_cast<float>(focalPointInView.y)
		- logicalY * _viewZoom;
	_fitToViewport = false;
	ClampViewOffset();
	this->InvalidateVisual();
	NotifyViewChanged();
}

void DesignerCanvas::ZoomIn()
{
	SetViewZoom(_viewZoom * DesignerViewZoomStep);
}

void DesignerCanvas::ZoomOut()
{
	SetViewZoom(_viewZoom / DesignerViewZoomStep);
}

void DesignerCanvas::ResetView()
{
	_viewZoom = 1.0f;
	_viewOffset = D2D1::Point2F(0.0f, 0.0f);
	_fitToViewport = false;
	ClampViewOffset();
	this->InvalidateVisual();
	NotifyViewChanged();
}

void DesignerCanvas::FitDesignSurfaceToViewport()
{
	_fitToViewport = true;
	RecalculateFitView(true);
}

void DesignerCanvas::SetGridVisible(bool visible)
{
	if (_showGrid == visible) return;
	_showGrid = visible;
	InvalidateVisual();
}

void DesignerCanvas::SetSnapToGridEnabled(bool enabled)
{
	if (_snapToGrid == enabled) return;
	_snapToGrid = enabled;
	ClearAlignmentGuides();
	InvalidateVisual();
}

void DesignerCanvas::SetSnapToGuidesEnabled(bool enabled)
{
	if (_snapToGuides == enabled) return;
	_snapToGuides = enabled;
	ClearAlignmentGuides();
	InvalidateVisual();
}

void DesignerCanvas::SetGridSize(int gridSize)
{
	gridSize = (std::clamp)(gridSize, 2, 100);
	if (_gridSize == gridSize) return;
	_gridSize = gridSize;
	InvalidateVisual();
}

bool DesignerCanvas::IsTabOrderCandidate(
	const std::shared_ptr<DesignerControl>& control) const
{
	return control && control->ControlInstance
		&& control->Type != UIClass::UI_TabItem
		&& control->ControlInstance->CanParticipateInTabNavigation()
		&& HasVisibleDesignerAncestors(control->ControlInstance);
}

std::vector<std::shared_ptr<DesignerControl>>
DesignerCanvas::CollectTabOrderCandidates() const
{
	std::vector<std::shared_ptr<DesignerControl>> result;
	result.reserve(_designerControls.size());
	for (const auto& control : _designerControls)
		if (IsTabOrderCandidate(control)) result.push_back(control);
	return result;
}

bool DesignerCanvas::SetTabOrderMode(bool active, int nextIndex)
{
	nextIndex = (std::max)(0, nextIndex);
	if (active && HasActiveDocumentTransaction()) return false;
	if (_tabOrderMode == active
		&& (!active || _nextTabOrderIndex == nextIndex)) return true;

	if (active)
	{
		_controlToAdd.reset();
		ClearControlDropPreview();
		if (_isBoxSelecting || _isDragging || _isResizing
			|| HasActiveDeltaInteraction()
			|| !_activeInteractionTransaction.empty())
		{
			(void)CancelActivePointerInteraction(
				L"进入 Tab 顺序模式前已取消画布交互。");
		}
	}
	_tabOrderMode = active;
	_nextTabOrderIndex = active ? nextIndex : 0;
	_lastTabOrderStableId = 0;
	_interactionCursor = CursorKind::Arrow;
	InvalidateVisual();
	NotifyTabOrderStateChanged();
	return true;
}

void DesignerCanvas::BeginViewPan(POINT viewPoint, bool leftButton)
{
	_isPanning = true;
	_panStartedWithLeftButton = leftButton;
	_panStartViewPoint = viewPoint;
	_panStartViewOffset = _viewOffset;
	_fitToViewport = false;
	_interactionCursor = CursorKind::SizeAll;
	(void)this->CaptureMouse();
}

void DesignerCanvas::EndViewPan()
{
	if (!_isPanning) return;
	_isPanning = false;
	_panStartedWithLeftButton = false;
	_interactionCursor = CursorKind::Arrow;
	(void)this->ReleaseMouseCapture();
	NotifyViewChanged();
}

int DesignerCanvas::GetSelectionHandleSizeInCanvas() const
{
	return (std::max)(1, static_cast<int>(std::lround(6.0f
		/ (std::max)(_viewZoom, DesignerMinimumViewZoom))));
}

DesignerModel::DesignNode DesignerCanvas::CaptureDesignedWindowNode() const
{
	return _designedWindowNode;
}

void DesignerCanvas::RewriteInputBindingCommandTargetReferences(
	const std::wstring& previousName,
	const std::wstring& nextName)
{
	if (previousName.empty() || nextName.empty()
		|| previousName == nextName) return;
	auto rewrite = [&](auto& bindings)
	{
		for (auto& binding : bindings)
			if (binding.CommandTarget == previousName)
				binding.CommandTarget = nextName;
	};
	rewrite(_designedWindowNode.InputBindings);
	for (const auto& control : _designerControls)
	{
		if (!control) continue;
		rewrite(control->InputBindings);
		if (control->AuthoredCommandTarget == previousName)
			control->AuthoredCommandTarget = nextName;
	}
}

bool DesignerCanvas::ApplyDesignedWindowNode(
	const DesignerModel::DesignNode& window,
	std::wstring* outError)
{
	if (window.Type != UIClass::UI_Window || !window.XamlType.Valid())
	{
		if (outError) *outError = L"Designer 根节点必须是有效的 XAML Window。";
		return false;
	}
	if (window.Name.empty())
	{
		if (outError) *outError = L"Window x:Name 不能为空。";
		return false;
	}
	auto read = [&](const wchar_t* property, auto& target)
	{
		BindingValue runtimeValue;
		std::wstring readError;
		if (!DesignerPropertyCatalog::ReadNodeValue(
			window, property, runtimeValue, nullptr, &readError,
			_documentResourceBasePath, _documentResources)
			|| !runtimeValue.TryGet(target))
		{
			if (outError) *outError = readError.empty()
				? L"Window 属性类型不匹配：" + std::wstring(property)
				: readError;
			return false;
		}
		return true;
	};

	std::wstring title;
	std::wstring fontName;
	double fontSize = 18.0;
	cui::layout::Length width = cui::layout::Length::Fixed(800.0f);
	cui::layout::Length height = cui::layout::Length::Fixed(600.0f);
	cui::drawing::Brush background;
	cui::drawing::Brush foreground;
	bool showInTaskbar = true;
	bool topmost = false;
	bool enable = true;
	::WindowStyle windowStyle = ::WindowStyle::SingleBorderWindow;
	::ResizeMode resizeMode = ::ResizeMode::CanResize;
	if (!read(L"Title", title) || !read(L"FontFamily", fontName)
		|| !read(L"FontSize", fontSize) || !read(L"Width", width)
		|| !read(L"Height", height) || !read(L"Background", background)
		|| !read(L"Foreground", foreground)
		|| !read(L"ShowInTaskbar", showInTaskbar)
		|| !read(L"Topmost", topmost) || !read(L"IsEnabled", enable)
		|| !read(L"WindowStyle", windowStyle)
		|| !read(L"ResizeMode", resizeMode)) return false;

	const bool fontChanged = _designedWindowFontFamily != fontName
		|| std::fabs(_designedWindowFontSize - static_cast<float>(fontSize)) >= 1e-6f;
	const bool headerLayoutChanged = _designedWindowStyle != windowStyle;
	const cui::core::Size size{
		width.IsFixed() ? width.value : 800.0f,
		height.IsFixed() ? height.value : 600.0f };
	const bool sizeChanged = _designedWindowSize != size;

	_designedWindowNode = window;
	_designedWindowName = window.Name;
	_designedWindowTitle = std::move(title);
	_designedWindowFontFamily = std::move(fontName);
	_designedWindowFontSize = static_cast<float>(fontSize);
	auto solidColor = [](const cui::drawing::Brush& brush, D2D1_COLOR_F fallback)
	{
		if (brush.Kind != cui::drawing::BrushKind::Solid) return fallback;
		auto color = brush.Color;
		color.a *= (std::clamp)(brush.Opacity, 0.0f, 1.0f);
		return color;
	};
	_designedWindowBackgroundColor = solidColor(
		background, _designedWindowBackgroundColor);
	_designedWindowForegroundColor = solidColor(
		foreground, _designedWindowForegroundColor);
	_designedWindowShowInTaskbar = showInTaskbar;
	_designedWindowTopmost = topmost;
	_designedWindowEnable = enable;
	_designedWindowStyle = windowStyle;
	_designedWindowResizeMode = resizeMode;
	if (_clientSurface)
		(void)_clientSurface->TrySetPropertyValue(
			L"Background", BindingValue(std::move(background)));
	if (sizeChanged)
	{
		_applyingDesignedWindowNode = true;
		SetDesignedWindowSize(size);
		_applyingDesignedWindowNode = false;
	}
	else if (headerLayoutChanged) UpdateClientSurfaceLayout();
	if (fontChanged) RefreshDesignedWindowTypography();
	this->InvalidateVisual();
	if (outError) outError->clear();
	return true;
}

bool DesignerCanvas::ApplyDesignedWindowProperty(
	const std::wstring& propertyName,
	const DesignerStyleValue& value,
	DesignerStyleValue* outEffective,
	std::wstring* outError)
{
	auto candidate = _designedWindowNode;
	if (!DesignerPropertyCatalog::ApplyNodeValue(
		candidate, propertyName, value, outEffective, nullptr, outError,
		_documentResourceBasePath, _documentResources)) return false;
	return ApplyDesignedWindowNode(candidate, outError);
}

bool DesignerCanvas::ResetDesignedWindowProperty(
	const std::wstring& propertyName,
	DesignerStyleValue* outEffective,
	std::wstring* outError)
{
	auto candidate = _designedWindowNode;
	if (!DesignerPropertyCatalog::ResetNodeValue(
		candidate, propertyName, outEffective, nullptr, outError)) return false;
	return ApplyDesignedWindowNode(candidate, outError);
}

void DesignerCanvas::RefreshDesignedWindowTypography()
{
	const std::wstring family = _designedWindowFontFamily.empty()
		? std::wstring(L"Arial") : _designedWindowFontFamily;
	const double size = (std::max)(1.0, static_cast<double>(
		_designedWindowFontSize));
	_designedWindowChromeFont = std::make_unique<::Font>(
		family, static_cast<float>(size));
	if (!_clientSurface) return;
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*_clientSurface, L"FontFamily", BindingValue(family),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*_clientSurface, L"FontSize", BindingValue(size),
		DependencyPropertyValueSource::Template);
}

void DesignerCanvas::PreparePresentation()
{
	Panel::PreparePresentation();
	if (_fitToViewport
		&& (_lastFitViewportSize.width != ActualWidth
			|| _lastFitViewportSize.height != ActualHeight))
	{
		RecalculateFitView(false);
	}

	// 自身 Hidden/Collapsed 在设计期仍需保持可选；只在不可见祖先
	// （例如未激活 TabItem）真正遮蔽目标时移除选择。
	bool selectionChangedByVisibility = false;
	if (!_selectedControls.empty())
	{
		auto it = std::remove_if(_selectedControls.begin(), _selectedControls.end(),
			[&](const std::shared_ptr<DesignerControl>& dc) {
				if (!dc || !dc->ControlInstance) return true;
				if (!HasVisibleDesignerAncestors(dc->ControlInstance))
				{
					dc->IsSelected = false;
					selectionChangedByVisibility = true;
					return true;
				}
				return false;
			});
		_selectedControls.erase(it, _selectedControls.end());
		if (_selectedControl && !IsSelected(_selectedControl))
		{
			_selectedControl = _selectedControls.empty() ? nullptr : _selectedControls.back();
			selectionChangedByVisibility = true;
		}
		if (selectionChangedByVisibility)
		{
			cui::framework::EventAccess::Raise(OnControlSelected, _selectedControl);
		}
	}
	UpdateContentPreviewLayout();
}

void DesignerCanvas::OnRender()
{
	if (this->IsVisible == false) return;
	if (!this->GetPresentationWindow()) return;

	auto d2d = this->GetDrawingContext();
	auto absoluteLocation = this->GetAbsoluteLocationDip();
	auto size = this->GetActualSizeDip();
	auto absoluteRect = GetAbsoluteBoundsDip();
	const float absoluteX = static_cast<float>(absoluteLocation.x);
	const float absoluteY = static_cast<float>(absoluteLocation.y);

	d2d->PushDrawRect(absoluteRect.left, absoluteRect.top, absoluteRect.right - absoluteRect.left, absoluteRect.bottom - absoluteRect.top);
	{
		d2d->FillRect(absoluteX, absoluteY, size.width, size.height,
			this->Background.Color);

		D2D1_MATRIX_3X2_F previousTransform =
			D2D1::Matrix3x2F::Identity();
		auto* deviceContext = d2d->GetDeviceContextRaw();
		const bool hasViewTransform = deviceContext != nullptr;
		if (hasViewTransform)
		{
			deviceContext->GetTransform(&previousTransform);
			const auto previous = D2D1::Matrix3x2F(
				previousTransform._11, previousTransform._12,
				previousTransform._21, previousTransform._22,
				previousTransform._31, previousTransform._32);
			const auto rawViewTransform = GetViewRenderTransform();
			const auto viewTransform = D2D1::Matrix3x2F(
				rawViewTransform._11, rawViewTransform._12,
				rawViewTransform._21, rawViewTransform._22,
				rawViewTransform._31, rawViewTransform._32);
			deviceContext->SetTransform(viewTransform * previous);
		}

		DrawGrid();

		// 绘制“仿真窗体”边框 + 标题栏（不影响控件布局，控件都在 clientSurface 内）
		{
			auto canvasAbs = this->GetAbsoluteLocationDip();
			auto formRect = GetDesignSurfaceRectInCanvas();
			float fx = (float)(canvasAbs.x + formRect.left);
			float fy = (float)(canvasAbs.y + formRect.top);
			float fw = (float)(formRect.right - formRect.left);
			float fh = (float)(formRect.bottom - formRect.top);

			// 线宽按视图倍率补偿，缩小时仍保持清晰可见。
			const float viewStroke = 1.0f / _viewZoom;
			d2d->DrawRect(fx, fy, fw, fh, Colors::DimGrey, viewStroke);

			// 标题栏
			int headH = DesignedClientTop();
			if (headH > 0)
			{
				D2D1_COLOR_F headBack = D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.25f);
				d2d->FillRect(fx, fy, fw, (float)headH, headBack);

				// 标题文字
				std::wstring title = _designedWindowTitle.empty()
					? L"Window" : _designedWindowTitle;
				float textY = fy + (float)((headH - 14) * 0.5f);
				if (textY < fy) textY = fy;
				float pad = 8.0f;
				float btnW = (float)headH;
				::Font* titleFont = _designedWindowChromeFont
					? _designedWindowChromeFont.get() : this->GetRenderFont();
				d2d->DrawString(title, fx + pad, textY,
					_designedWindowForegroundColor, titleFont);

				// 右侧标题栏按钮（按 Window 的方式绘制图标）
				float xRight = fx + fw;
				auto drawBtnIcon = [&](bool enabled, int kind)
				{
					if (!enabled) return;
					xRight -= btnW;

					const float left = xRight;
					const float top = fy;
					const float bw = btnW;
					const float bh = (float)headH;
					const float s = (bw < bh) ? bw : bh;
					const float cx = left + bw * 0.5f;
					const float cy = top + bh * 0.5f;

					const float icon = s * 0.42f;
					const float half = icon * 0.5f;
					float stroke = s * 0.08f;
					if (stroke < 1.0f) stroke = 1.0f;

					auto drawMinimize = [&]()
						{
							const float y = cy + half * 0.35f;
							d2d->DrawLine(cx - half, y, cx + half, y, Colors::Black, stroke);
						};
					auto drawMaximize = [&]()
						{
							const float x = cx - half;
							const float y = cy - half;
							d2d->DrawRect(x, y, icon, icon, Colors::Black, stroke);
						};
					auto drawClose = [&]()
						{
							d2d->DrawLine(cx - half, cy - half, cx + half, cy + half, Colors::Black, stroke);
							d2d->DrawLine(cx + half, cy - half, cx - half, cy + half, Colors::Black, stroke);
						};

					switch (kind)
					{
					case 0: // Minimize
						drawMinimize();
						break;
					case 1: // Maximize
						drawMaximize();
						break;
					case 2: // Close
						drawClose();
						break;
					}
				};

				// 顺序与 Window 一致：Close / Max / Min
				drawBtnIcon(DesignedWindowHasChrome(), 2);
				drawBtnIcon(DesignedWindowHasMaximizeBox(), 1);
				drawBtnIcon(DesignedWindowHasMinimizeBox(), 0);
			}
		}
		// 选中边框/手柄/框选矩形：裁剪到设计面板
		{
			auto clip = GetClientSurfaceRectInCanvas();
			auto finalClip = IntersectRectSafe(clip, GetViewportRectInCanvas());
			auto canvasAbs = this->GetAbsoluteLocationDip();
			d2d->PushDrawRect((float)(canvasAbs.x + finalClip.left), (float)(canvasAbs.y + finalClip.top),
				(float)(finalClip.right - finalClip.left), (float)(finalClip.bottom - finalClip.top));

			// 运行时不绘制非 Visible 控件；Designer 叠加一个可命中的
			// 半透明占位，使其 Visibility 仍可被编辑。
			for (const auto& dc : _designerControls)
			{
				if (!dc || dc->Type == UIClass::UI_TabItem
					|| !dc->ControlInstance
					|| dc->ControlInstance->Visibility == Visibility::Visible
					|| !HasVisibleDesignerAncestors(dc->ControlInstance))
					continue;
				const auto rect = GetControlRectInCanvas(dc->ControlInstance);
				const float width = static_cast<float>(rect.right - rect.left);
				const float height = static_cast<float>(rect.bottom - rect.top);
				if (width <= 0.0f || height <= 0.0f) continue;
				const float x = static_cast<float>(canvasAbs.x + rect.left);
				const float y = static_cast<float>(canvasAbs.y + rect.top);
				auto fill = Colors::DarkOrange;
				fill.a = 0.12f;
				auto border = Colors::DarkOrange;
				border.a = 0.9f;
				d2d->FillRect(x, y, width, height, fill);
				d2d->DrawRect(x, y, width, height, border, 1.5f / _viewZoom);
				d2d->DrawLine(x, y, x + width, y + height, border, 1.0f / _viewZoom);
				d2d->DrawLine(x + width, y, x, y + height, border, 1.0f / _viewZoom);
				if (width >= 54.0f && height >= 18.0f)
				{
					auto* font = _designedWindowChromeFont
						? _designedWindowChromeFont.get() : this->GetRenderFont();
					if (font)
						d2d->DrawString(
							dc->Name + (dc->ControlInstance->Visibility
								== Visibility::Hidden
								? L" (Hidden)" : L" (Collapsed)"),
							x + 4.0f, y + 2.0f, border, font);
				}
			}

			// Designer Tab-order mode: keep numbered badges a stable
			// screen size while the design surface itself is zoomed.
			if (_tabOrderMode)
			{
				if (!_tabOrderBadgeFont
					|| std::fabs(_tabOrderBadgeFontZoom - _viewZoom) > 0.0001f)
				{
					_tabOrderBadgeFont.reset();
					try
					{
						_tabOrderBadgeFont = std::make_unique<::Font>(
							L"Microsoft YaHei",
							14.0f / (std::max)(_viewZoom,
								DesignerMinimumViewZoom));
						_tabOrderBadgeFontZoom = _viewZoom;
					}
					catch (...)
					{
						_tabOrderBadgeFontZoom = 0.0f;
					}
				}
				const float inverseZoom = 1.0f
					/ (std::max)(_viewZoom, DesignerMinimumViewZoom);
				for (const auto& dc : CollectTabOrderCandidates())
				{
					const auto rect = GetControlRectInCanvas(
						dc->ControlInstance);
					const std::wstring text = std::to_wstring(
						dc->ControlInstance->TabIndex);
					const auto textSize = _tabOrderBadgeFont
						? _tabOrderBadgeFont->GetTextSize(text)
						: D2D1_SIZE_F{ 18.0f * inverseZoom,
							16.0f * inverseZoom };
					const float width = (std::max)(
						24.0f * inverseZoom,
						textSize.width + 12.0f * inverseZoom);
					const float height = 22.0f * inverseZoom;
					const float x = static_cast<float>(
						canvasAbs.x + rect.left);
					const float y = static_cast<float>(
						canvasAbs.y + rect.top);
					auto fill = dc->StableId == _lastTabOrderStableId
						? D2D1::ColorF(0.05f, 0.62f, 0.34f, 0.96f)
						: D2D1::ColorF(0.08f, 0.38f, 0.82f, 0.94f);
					auto border = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.96f);
					d2d->FillRect(x, y, width, height, fill);
					d2d->DrawRect(x, y, width, height,
						border, 1.0f * inverseZoom);
					if (_tabOrderBadgeFont)
						d2d->DrawString(text,
							x + 6.0f * inverseZoom,
							y + 2.0f * inverseZoom,
							Colors::White, _tabOrderBadgeFont.get());
				}
			}

			// Direct toolbox drag: show the runtime host that will own the new
			// control and an approximate default-size ghost at the final point.
			if (_controlDropPreviewVisible && _controlDropPreviewDescriptor)
			{
				const auto target = _controlDropTargetRect;
				const auto ghost = _controlDropPreviewRect;
				const float targetWidth = static_cast<float>(target.right - target.left);
				const float targetHeight = static_cast<float>(target.bottom - target.top);
				const float ghostWidth = static_cast<float>(ghost.right - ghost.left);
				const float ghostHeight = static_cast<float>(ghost.bottom - ghost.top);
				auto targetFill = D2D1::ColorF(0.08f, 0.67f, 0.37f, 0.055f);
				auto targetBorder = D2D1::ColorF(0.05f, 0.58f, 0.31f, 0.92f);
				auto ghostFill = D2D1::ColorF(0.10f, 0.48f, 0.95f, 0.18f);
				auto ghostBorder = D2D1::ColorF(0.08f, 0.42f, 0.90f, 0.96f);
				if (targetWidth > 0.0f && targetHeight > 0.0f)
				{
					const float x = static_cast<float>(canvasAbs.x + target.left);
					const float y = static_cast<float>(canvasAbs.y + target.top);
					d2d->FillRect(x, y, targetWidth, targetHeight, targetFill);
					d2d->DrawRect(x, y, targetWidth, targetHeight,
						targetBorder, 2.0f / _viewZoom);
				}
				if (ghostWidth > 0.0f && ghostHeight > 0.0f)
				{
					const float x = static_cast<float>(canvasAbs.x + ghost.left);
					const float y = static_cast<float>(canvasAbs.y + ghost.top);
					d2d->FillRect(x, y, ghostWidth, ghostHeight, ghostFill);
					d2d->DrawRect(x, y, ghostWidth, ghostHeight,
						ghostBorder, 2.0f / _viewZoom);
					if (ghostWidth >= 48.0f && ghostHeight >= 18.0f)
					{
						auto* font = _designedWindowChromeFont
							? _designedWindowChromeFont.get() : this->GetRenderFont();
						if (font)
							d2d->DrawString(
								_controlDropPreviewDescriptor->DisplayName,
								x + 4.0f, y + 2.0f, ghostBorder, font);
					}
				}
			}

			// 参考线（拖拽/缩放期间）
			if ((_isDragging || _isResizing) && (!_vGuides.empty() || !_hGuides.empty()))
			{
				float left = (float)(canvasAbs.x + clip.left);
				float top = (float)(canvasAbs.y + clip.top);
				float right = (float)(canvasAbs.x + clip.right);
				float bottom = (float)(canvasAbs.y + clip.bottom);
				auto c = Colors::DodgerBlue;
				c.a = 0.85f;
				for (int xCanvas : _vGuides)
				{
					float x = (float)(canvasAbs.x + xCanvas);
					d2d->DrawLine(x, top, x, bottom, c, 1.0f / _viewZoom);
				}
				for (int yCanvas : _hGuides)
				{
					float y = (float)(canvasAbs.y + yCanvas);
					d2d->DrawLine(left, y, right, y, c, 1.0f / _viewZoom);
				}
			}

			// 先绘制所有选中的边框
			for (auto& dc : _selectedControls)
			{
				if (!dc || !dc->ControlInstance) continue;
				auto rect = GetControlRectInCanvas(dc->ControlInstance);
				int w = rect.right - rect.left;
				int h = rect.bottom - rect.top;
				float x = (float)(canvasAbs.x + rect.left);
				float y = (float)(canvasAbs.y + rect.top);
				d2d->DrawRect(x, y, (float)w, (float)h, Colors::DodgerBlue, 2.0f / _viewZoom);
			}

			// 每个锁定控件都显示锁标记；仅未锁定的主选中显示调整手柄。
			for (const auto& dc : _selectedControls)
				if (dc && dc->IsLocked) DrawSelectionHandles(dc);
			if (_selectedControl && !_selectedControl->IsLocked)
				DrawSelectionHandles(_selectedControl);

			// 框选矩形
			if (_isBoxSelecting)
			{
				auto r = _boxSelectRect;
				int w = r.right - r.left;
				int h = r.bottom - r.top;
				float x = (float)(canvasAbs.x + r.left);
				float y = (float)(canvasAbs.y + r.top);
				D2D1_COLOR_F c = D2D1::ColorF(0.12f, 0.50f, 0.95f, 0.25f);
				d2d->FillRect(x, y, (float)w, (float)h, c);
				d2d->DrawRect(x, y, (float)w, (float)h, Colors::DodgerBlue, 1.0f / _viewZoom);
			}

			d2d->PopDrawRect();
		}

		if (hasViewTransform)
			deviceContext->SetTransform(previousTransform);
		d2d->DrawRect(absoluteX, absoluteY, size.width, size.height,
			this->BorderBrush.Color, this->BorderThickness.MaxEdge());
	}
	if (!this->IsEnabled)
	{
		d2d->FillRect(absoluteX, absoluteY, size.width, size.height, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	d2d->PopDrawRect();
}

void DesignerCanvas::ClearSelection()
{
	if (_selectionService)
	{
		_selectionService->Clear(_selectedControls, _selectedControl);
	}
}

bool DesignerCanvas::IsSelected(const std::shared_ptr<DesignerControl>& dc) const
{
	if (!_selectionService)
	{
		return false;
	}
	return _selectionService->IsSelected(_selectedControls, dc);
}

void DesignerCanvas::SetPrimarySelection(const std::shared_ptr<DesignerControl>& dc, bool fireEvent)
{
	if (_selectionService)
	{
		_selectionService->SetPrimary(_selectedControl, dc);
	}
	if (fireEvent)
		cui::framework::EventAccess::Raise(OnControlSelected, _selectedControl);
}

void DesignerCanvas::AddToSelection(const std::shared_ptr<DesignerControl>& dc, bool setPrimary, bool fireEvent)
{
	if (!_selectionService)
	{
		return;
	}
	bool changed = _selectionService->Add(_selectedControls, _selectedControl, dc, setPrimary);
	if (!changed)
	{
		return;
	}
	if (setPrimary)
	{
		SetPrimarySelection(dc, fireEvent);
	}
	else if (fireEvent)
	{
		cui::framework::EventAccess::Raise(OnControlSelected, _selectedControl);
	}
}

void DesignerCanvas::ToggleSelection(const std::shared_ptr<DesignerControl>& dc, bool fireEvent)
{
	if (!_selectionService)
	{
		return;
	}
	bool changed = _selectionService->Toggle(_selectedControls, _selectedControl, dc);
	if (!changed)
	{
		return;
	}
	if (fireEvent)
		cui::framework::EventAccess::Raise(OnControlSelected, _selectedControl);
}

RECT DesignerCanvas::GetSelectionBoundsInCanvas() const
{
	RECT out{ 0,0,0,0 };
	bool first = true;
	for (auto& dc : _selectedControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		auto r = const_cast<DesignerCanvas*>(this)->GetControlRectInCanvas(dc->ControlInstance);
		if (first) { out = r; first = false; }
		else
		{
			out.left = (std::min)(out.left, r.left);
			out.top = (std::min)(out.top, r.top);
			out.right = (std::max)(out.right, r.right);
			out.bottom = (std::max)(out.bottom, r.bottom);
		}
	}
	return out;
}

void DesignerCanvas::BeginDragFromCurrentSelection(POINT mousePos)
{
	_dragStartItems.clear();
	if (HasLockedSelectedControls())
	{
		_isDragging = false;
		_dragHasMoved = false;
		_dragLiftedToRoot = false;
		return;
	}
	for (auto& dc : _selectedControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		auto* c = dc->ControlInstance;
		// Window.Content itself has no Canvas placement relative to the design
		// chrome. Its layout is governed by Window's content slot.
		if (!dc->DesignerParent && _clientSurface
			&& c->GetVisualParent() == _clientSurface) continue;
		DragStartItem it;
		it.ControlInstance = c;
		it.Parent = c->GetVisualParent() ? c->GetVisualParent() : (_clientSurface ? (Control*)_clientSurface : (Control*)_designSurface);
		it.StartRectInCanvas = GetControlRectInCanvas(c);
		it.StartMargin = c->Margin;
		it.UsesRelativeMargin = (it.Parent && it.Parent->Type() == UIClass::UI_RelativePanel);
		_dragStartItems.push_back(it);
	}
	_isDragging = !_dragStartItems.empty();
	_dragHasMoved = false;
	_dragLiftedToRoot = false;
	_dragStartPoint = mousePos;
	if (_selectedControl && _selectedControl->ControlInstance)
		_dragStartRectInCanvas = GetControlRectInCanvas(_selectedControl->ControlInstance);
}

bool DesignerCanvas::IsLayoutContainer(Control* c) const
{
	if (!c) return false;
	if (auto* items = dynamic_cast<ItemsControl*>(c))
		return !items->GetItemsSource();
	switch (c->Type())
	{
	case UIClass::UI_Grid:
	case UIClass::UI_StackPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
		return true;
	default:
		return false;
	}
}

void DesignerCanvas::LiftSelectedToRootForDrag()
{
	if (_dragLiftedToRoot) return;
	if (!_selectedControl || !_selectedControl->ControlInstance) return;
	if (!_clientSurface) return;

	auto* moving = _selectedControl->ControlInstance;
	auto* parent = moving->GetVisualParent();
	if (!parent) return;
	auto* contentRoot = GetDocumentContentRoot();
	if (!contentRoot || moving == contentRoot || parent == contentRoot
		|| contentRoot->Type() != UIClass::UI_Canvas
		|| !LayoutBridge::CanAcceptChild(contentRoot, moving->Type())) return;

	const auto parentType = parent->Type();
	const bool fromGrid = (parentType == UIClass::UI_Grid);
	const bool fromRelative = (parentType == UIClass::UI_RelativePanel);

	// 抬升前先拿到当前视觉矩形，保持“画面不跳”
	RECT r = GetControlRectInCanvas(moving);
	int w = r.right - r.left;
	int h = r.bottom - r.top;
	if (w < 0) w = 0;
	if (h < 0) h = 0;

	// 从原容器移除，加入唯一的 Window.Content Canvas；设计 chrome
	// 从来不是第二个可序列化根集合。
	// 鼠标释放后再根据落点决定是否重新放回原容器或其他容器。
	auto movingOwner = parent->DetachVisualChild(moving);
	if (!movingOwner) return;
	contentRoot->AddOwned(std::move(movingOwner));
	if (UsesAlignmentManagedPlacement(moving))
	{
		ResetAlignmentForManualPlacement(moving);
	}
	// 从 Grid 抬升到根：避免默认 Stretch 直接把控件“铺满”
	if (fromGrid)
	{
		ResetAlignmentForManualPlacement(moving);
	}
	// 从 RelativePanel 抬升到根：清掉用作定位的 Margin，回到 Location 语义
	if (fromRelative)
	{
		auto m = moving->Margin;
		m.Left = 0.0f;
		m.Top = 0.0f;
		m.Right = 0.0f;
		m.Bottom = 0.0f;
		moving->Margin = m;
	}
	RECT rootRect = r;
	rootRect.right = rootRect.left + w;
	rootRect.bottom = rootRect.top + h;
	ApplyRectToControl(moving, rootRect);
	_selectedControl->DesignerParent = contentRoot;
	_dragLiftedToRoot = true;

	if (auto* p = dynamic_cast<Panel*>(parent))
	{
		RefreshDesignerPanelLayout(p);
	}
}

void DesignerCanvas::ApplyMoveDeltaToSelection(int dx, int dy)
{
	if (_dragStartItems.empty()) return;

	// 多选移动：目前只支持同一父容器（AddToSelection 已约束）
	for (auto& it : _dragStartItems)
	{
		if (!it.ControlInstance) continue;
		RECT newRect = it.StartRectInCanvas;
		newRect.left += dx;
		newRect.right += dx;
		newRect.top += dy;
		newRect.bottom += dy;

		// 根级控件：约束到客户区；容器内控件不做全局 clamp（由容器布局决定）
		if (_clientSurface && it.ControlInstance->GetVisualParent() == _clientSurface)
		{
			auto bounds = GetClientSurfaceRectInCanvas();
			newRect = ClampRectToBounds(newRect, bounds, true);
		}

		// Moving must not turn a measured/arranged size back into a Local
		// Width/Height.  In particular, a themed control may arrange larger
		// than its authored XAML size; nudge changes placement only.
		ApplyRectToControl(it.ControlInstance, newRect, true);
	}
}

std::vector<std::wstring> DesignerCanvas::CaptureSelectionNames() const
{
	if (!_selectionService)
	{
		return {};
	}
	return _selectionService->CaptureNames(_selectedControls);
}

bool DesignerCanvas::HasActiveDeltaInteraction() const noexcept
{
	return static_cast<bool>(_activePlacementInteraction)
		|| static_cast<bool>(_activePropertyInteraction);
}

DesignerDocumentTransactionResult DesignerCanvas::ExecuteCommand(
	std::unique_ptr<IDesignerCommand> command)
{
	const auto label = command ? command->GetLabel() : std::wstring{};
	auto result = ExecuteCommandCore(std::move(command));
	PublishCanvasCommandResult(L"ExecuteCommand", label, result);
	return result;
}

DesignerDocumentTransactionResult DesignerCanvas::ExecuteCommandCore(
	std::unique_ptr<IDesignerCommand> command)
{
	return HasActiveDeltaInteraction()
		? DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布差量预览进行中，不能执行其他文档命令。")
		: _commandCoordinator
		? _commandCoordinator->Execute(std::move(command))
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"文档命令协调器不可用。", false);
}

DesignerDocumentTransactionResult DesignerCanvas::CommitAlreadyAppliedCommand(
	std::unique_ptr<IDesignerCommand> command)
{
	return HasActiveDeltaInteraction()
		? DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布差量预览进行中，不能提交其他文档命令。")
		: _commandCoordinator
		? _commandCoordinator->Execute(std::move(command))
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"文档命令协调器不可用。", false);
}

DesignerDocumentTransactionResult DesignerCanvas::UndoCommand()
{
	const auto label = _commandCoordinator
		? _commandCoordinator->GetUndoLabel() : std::wstring{};
	auto result = HasActiveDeltaInteraction()
		? DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布差量预览进行中，不能撤销。")
		: _commandCoordinator
		? _commandCoordinator->Undo()
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"文档命令协调器不可用。", false);
	PublishCanvasCommandResult(L"Undo", label, result);
	return result;
}

DesignerDocumentTransactionResult DesignerCanvas::RedoCommand()
{
	const auto label = _commandCoordinator
		? _commandCoordinator->GetRedoLabel() : std::wstring{};
	auto result = HasActiveDeltaInteraction()
		? DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布差量预览进行中，不能重做。")
		: _commandCoordinator
		? _commandCoordinator->Redo()
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"文档命令协调器不可用。", false);
	PublishCanvasCommandResult(L"Redo", label, result);
	return result;
}

std::wstring DesignerCanvas::GetUndoCommandLabel() const
{
	return _commandCoordinator
		? _commandCoordinator->GetUndoLabel() : std::wstring{};
}

std::wstring DesignerCanvas::GetRedoCommandLabel() const
{
	return _commandCoordinator
		? _commandCoordinator->GetRedoLabel() : std::wstring{};
}

bool DesignerCanvas::IsDocumentDirty() const noexcept
{
	return _commandCoordinator
		&& _commandCoordinator->IsDocumentDirty();
}

uint64_t DesignerCanvas::GetCurrentDocumentStateId() const noexcept
{
	return _commandCoordinator
		? _commandCoordinator->GetCurrentDocumentStateId() : 0;
}

uint64_t DesignerCanvas::GetSavedDocumentStateId() const noexcept
{
	return _commandCoordinator
		? _commandCoordinator->GetSavedDocumentStateId() : 0;
}

void DesignerCanvas::SetCommandHistoryMemoryLimit(size_t byteLimit)
{
	if (_commandCoordinator)
		_commandCoordinator->SetHistoryMemoryLimit(byteLimit);
}

size_t DesignerCanvas::GetCommandHistoryMemoryLimit() const noexcept
{
	return _commandCoordinator
		? _commandCoordinator->GetHistoryMemoryLimit() : 0;
}

size_t DesignerCanvas::GetCommandHistoryMemoryUsage() const noexcept
{
	return _commandCoordinator
		? _commandCoordinator->GetHistoryMemoryUsage() : 0;
}

size_t DesignerCanvas::GetUndoCommandCount() const noexcept
{
	return _commandCoordinator
		? _commandCoordinator->GetUndoCount() : 0;
}

size_t DesignerCanvas::GetRedoCommandCount() const noexcept
{
	return _commandCoordinator
		? _commandCoordinator->GetRedoCount() : 0;
}

DesignerDocumentTransactionResult DesignerCanvas::MarkDocumentSaved()
{
	if (!_commandCoordinator)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"文档命令协调器不可用。", false);
	if (HasActiveDeltaInteraction())
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布差量预览进行中，不能标记保存点。");
	return _commandCoordinator->MarkDocumentSaved();
}

DesignerDocumentTransactionResult
DesignerCanvas::ResetDocumentHistoryAsSaved()
{
	if (!_commandCoordinator)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"文档命令协调器不可用。", false);
	if (HasActiveDeltaInteraction())
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布差量预览进行中，不能重置历史。");
	return _commandCoordinator->ResetHistoryAsSaved();
}

DesignerDocumentTransactionResult
DesignerCanvas::ResetDocumentHistoryAsUnsaved()
{
	if (!_commandCoordinator)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"文档命令协调器不可用。", false);
	if (HasActiveDeltaInteraction())
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布差量预览进行中，不能重置历史。");
	return _commandCoordinator->ResetHistoryAsUnsaved();
}

bool DesignerCanvas::HasActiveDocumentTransaction() const noexcept
{
	return HasActiveDeltaInteraction() || (_commandCoordinator
		&& _commandCoordinator->HasActiveDocumentTransaction());
}

DesignerDocumentTransactionResult
DesignerCanvas::ExecuteDocumentEditTransaction(
	const std::wstring& label,
	const std::function<bool(std::wstring& error)>& applyChange)
{
	if (!_commandCoordinator)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"文档命令协调器不可用。", false);
	if (HasActiveDeltaInteraction())
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布差量预览进行中，不能开始文档事务。");
	return _commandCoordinator->ExecuteDocumentTransaction(
		label, applyChange);
}

DesignerDocumentTransactionResult
DesignerCanvas::BeginDocumentEditTransaction(
	const std::wstring& label)
{
	if (!_commandCoordinator)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"文档命令协调器不可用。", false);
	if (HasActiveDeltaInteraction())
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布差量预览进行中，不能开始文档事务。");
	return _commandCoordinator->BeginDocumentTransaction(label);
}

DesignerDocumentTransactionResult
DesignerCanvas::CommitDocumentEditTransaction()
{
	if (!_commandCoordinator)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"文档命令协调器不可用。", false);
	if (HasActiveDeltaInteraction())
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布差量预览进行中，不能提交其他文档事务。");
	return _commandCoordinator->CommitDocumentTransaction();
}

DesignerDocumentTransactionResult
DesignerCanvas::RollbackDocumentEditTransaction()
{
	if (!_commandCoordinator)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"文档命令协调器不可用。", false);
	if (HasActiveDeltaInteraction())
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布差量预览进行中，不能回滚其他文档事务。");
	return _commandCoordinator->RollbackDocumentTransaction();
}

DesignerDocumentTransactionResult
DesignerCanvas::CancelDocumentEditTransaction()
{
	if (!_commandCoordinator)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"文档命令协调器不可用。", false);
	if (HasActiveDeltaInteraction())
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布差量预览进行中，不能取消其他文档事务。");
	return _commandCoordinator->CancelDocumentTransaction();
}

bool DesignerCanvas::BeginCanvasInteractionTransaction(
	const std::wstring& operation)
{
	if (HasActiveDeltaInteraction())
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"另一个画布差量预览正在进行。");
		PublishCanvasInteractionTransactionResult(operation, result);
		return false;
	}
	if (!_activeInteractionTransaction.empty())
	{
		if (_activeInteractionTransaction == operation) return true;
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"另一个画布交互事务正在进行。");
		PublishCanvasInteractionTransactionResult(operation, result);
		return false;
	}

	auto result = BeginDocumentEditTransaction(operation);
	if (!result)
	{
		PublishCanvasInteractionTransactionResult(operation, result);
		return false;
	}
	_activeInteractionTransaction = operation;
	return true;
}

bool DesignerCanvas::BeginPlacementInteraction(
	const std::wstring& operation)
{
	if (_activePropertyInteraction)
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"另一个画布属性预览正在进行。");
		PublishCanvasInteractionTransactionResult(operation, result);
		return false;
	}
	if (_activePlacementInteraction)
	{
		if (_activePlacementInteraction->Operation == operation) return true;
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"另一个画布位置交互正在进行。");
		PublishCanvasInteractionTransactionResult(operation, result);
		return false;
	}
	if (!_activeInteractionTransaction.empty())
	{
		if (_activeInteractionTransaction == operation) return true;
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"另一个画布交互事务正在进行。");
		PublishCanvasInteractionTransactionResult(operation, result);
		return false;
	}
	if (_commandCoordinator
		&& _commandCoordinator->HasActiveDocumentTransaction())
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"另一个文档事务正在进行，不能开始位置预览。");
		PublishCanvasInteractionTransactionResult(operation, result);
		return false;
	}

	DesignerControlPlacementSnapshot before;
	std::wstring captureError;
	if (ControlPlacementCommand::Capture(
		this, _selectedControls, before, &captureError))
	{
		try
		{
			auto interaction =
				std::make_unique<DesignerCanvasPlacementInteraction>();
			interaction->Operation = operation;
			interaction->Before = std::move(before);
			interaction->BeforeSelectionNames = CaptureSelectionNames();
			interaction->BeforePrimarySelectionName = _selectedControl
				? _selectedControl->Name : std::wstring{};
			_activePlacementInteraction = std::move(interaction);
			return true;
		}
		catch (...)
		{
			captureError = L"无法分配位置差量预览状态。";
		}
	}

	// Unknown/custom parent structures remain compatible through the strict
	// full-document transaction until they gain an explicit parent locator.
	auto fallback = BeginDocumentEditTransaction(operation);
	if (!fallback)
	{
		if (!captureError.empty())
			fallback.Error = L"无法建立位置差量（" + captureError
				+ L"），完整文档事务也失败：" + fallback.Error;
		PublishCanvasInteractionTransactionResult(operation, fallback);
		return false;
	}
	_activeInteractionTransaction = operation;
	return true;
}

bool DesignerCanvas::BeginControlPropertyInteraction(
	const std::wstring& operation,
	const std::shared_ptr<DesignerControl>& target,
	const std::wstring& propertyName)
{
	if (_activePropertyInteraction)
	{
		const auto& before = _activePropertyInteraction->Before;
		if (_activePropertyInteraction->Operation == operation
			&& before.PropertyName == propertyName
			&& before.Targets.size() == 1
			&& target
			&& before.Targets.front().TargetName == target->Name
			&& before.Targets.front().TargetType == target->Type)
			return true;
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"另一个画布属性预览正在进行。");
		PublishCanvasInteractionTransactionResult(operation, result);
		return false;
	}
	if (_activePlacementInteraction || !_activeInteractionTransaction.empty()
		|| (_commandCoordinator
			&& _commandCoordinator->HasActiveDocumentTransaction()))
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"另一个画布或文档事务正在进行，不能开始属性预览。");
		PublishCanvasInteractionTransactionResult(operation, result);
		return false;
	}
	if (!target || !target->ControlInstance || propertyName.empty())
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"属性预览目标已经失效。", false);
		PublishCanvasInteractionTransactionResult(operation, result);
		return false;
	}

	PropertyGridBinder binder;
	binder.SetCanvas(this);
	binder.BindControl(target);
	DesignerPropertyBatchSnapshot before;
	std::wstring error;
	bool captured = false;
	try
	{
		captured = binder.CaptureControlPropertySnapshot(
			propertyName, before, &error);
	}
	catch (...)
	{
		error = L"捕获属性预览起点时抛出异常。";
	}
	if (!captured)
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法捕获属性预览起点：" + error, false);
		PublishCanvasInteractionTransactionResult(operation, result);
		return false;
	}
	try
	{
		auto interaction =
			std::make_unique<DesignerCanvasPropertyInteraction>();
		interaction->Operation = operation;
		interaction->Before = std::move(before);
		interaction->BeforeSelectionNames = CaptureSelectionNames();
		interaction->BeforePrimarySelectionName = _selectedControl
			? _selectedControl->Name : std::wstring{};
		_activePropertyInteraction = std::move(interaction);
		return true;
	}
	catch (...)
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法分配属性差量预览状态。", false);
		PublishCanvasInteractionTransactionResult(operation, result);
		return false;
	}
}

DesignerDocumentTransactionResult
DesignerCanvas::CommitCanvasInteractionTransaction()
{
	if (_activePropertyInteraction)
		return CommitControlPropertyInteraction();
	if (_activePlacementInteraction)
		return CommitPlacementInteraction();
	if (_activeInteractionTransaction.empty())
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	const auto operation = std::move(_activeInteractionTransaction);
	_activeInteractionTransaction.clear();
	auto result = CommitDocumentEditTransaction();
	PublishCanvasInteractionTransactionResult(operation, result);
	return result;
}

DesignerDocumentTransactionResult
DesignerCanvas::CommitControlPropertyInteraction()
{
	if (!_activePropertyInteraction)
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	auto interaction = std::move(_activePropertyInteraction);
	const auto operation = interaction->Operation;
	DesignerPropertyBatchSnapshot after;
	std::wstring error;
	if (!CapturePropertySnapshot(this, interaction->Before, after, &error))
	{
		std::wstring restoreError;
		const bool restored = RestorePropertySnapshot(
			this, interaction->Before, &restoreError);
		RestoreSelectionByNames(
			interaction->BeforeSelectionNames,
			interaction->BeforePrimarySelectionName, true);
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法捕获属性交互终点：" + error
				+ (restored ? L"" : L" 起点恢复失败：" + restoreError),
			restored);
		PublishCanvasInteractionTransactionResult(operation, result);
		return result;
	}
	const auto afterSelectionNames = CaptureSelectionNames();
	const auto afterPrimarySelectionName = _selectedControl
		? _selectedControl->Name : std::wstring{};
	if (interaction->Before.EquivalentTo(after)
		&& interaction->BeforeSelectionNames == afterSelectionNames
		&& interaction->BeforePrimarySelectionName
			== afterPrimarySelectionName)
	{
		auto result = DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
		PublishCanvasInteractionTransactionResult(operation, result);
		return result;
	}

	DesignerDocumentTransactionResult result;
	try
	{
		auto command = std::make_unique<ControlPropertyCommand>(
			this,
			interaction->Before,
			std::move(after),
			interaction->BeforeSelectionNames,
			afterSelectionNames,
			interaction->BeforePrimarySelectionName,
			afterPrimarySelectionName,
			operation,
			true,
			false);
		result = CommitAlreadyAppliedCommand(std::move(command));
	}
	catch (...)
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法创建属性交互历史命令。", false);
	}
	if (!result)
	{
		std::wstring restoreError;
		const bool restored = RestorePropertySnapshot(
			this, interaction->Before, &restoreError);
		RestoreSelectionByNames(
			interaction->BeforeSelectionNames,
			interaction->BeforePrimarySelectionName, true);
		result.DocumentRestored = restored;
		if (!restored)
			result.Error += L" 属性交互起点恢复失败：" + restoreError;
	}
	PublishCanvasInteractionTransactionResult(operation, result);
	return result;
}

DesignerDocumentTransactionResult
DesignerCanvas::CommitPlacementInteraction()
{
	if (!_activePlacementInteraction)
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	auto interaction = std::move(_activePlacementInteraction);
	const auto operation = interaction->Operation;
	DesignerControlPlacementSnapshot after;
	std::wstring error;
	if (!ControlPlacementCommand::Capture(
		this, _selectedControls, after, &error))
	{
		std::wstring restoreError;
		const bool restored = ControlPlacementCommand::Restore(
			this, interaction->Before, &restoreError);
		RestoreSelectionByNames(
			interaction->BeforeSelectionNames,
			interaction->BeforePrimarySelectionName, true);
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法捕获位置交互终点：" + error
				+ (restored ? L"" : L" 起点恢复失败：" + restoreError),
			restored);
		PublishCanvasInteractionTransactionResult(operation, result);
		return result;
	}
	bool compatibleTargets = interaction->Before.Targets.size()
		== after.Targets.size();
	for (size_t index = 0;
		compatibleTargets && index < after.Targets.size(); ++index)
	{
		compatibleTargets = interaction->Before.Targets[index].TargetName
				== after.Targets[index].TargetName
			&& interaction->Before.Targets[index].TargetType
				== after.Targets[index].TargetType;
	}
	if (!compatibleTargets)
	{
		std::wstring restoreError;
		const bool restored = ControlPlacementCommand::Restore(
			this, interaction->Before, &restoreError);
		RestoreSelectionByNames(
			interaction->BeforeSelectionNames,
			interaction->BeforePrimarySelectionName, true);
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"位置交互期间目标集合发生变化。"
				+ (restored ? L"" : L" 起点恢复失败：" + restoreError),
			restored);
		PublishCanvasInteractionTransactionResult(operation, result);
		return result;
	}
	const auto afterSelectionNames = CaptureSelectionNames();
	const auto afterPrimarySelectionName = _selectedControl
		? _selectedControl->Name : std::wstring{};
	if (interaction->Before.EquivalentTo(after)
		&& interaction->BeforeSelectionNames == afterSelectionNames
		&& interaction->BeforePrimarySelectionName
			== afterPrimarySelectionName)
	{
		auto result = DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
		PublishCanvasInteractionTransactionResult(operation, result);
		return result;
	}

	DesignerDocumentTransactionResult result;
	try
	{
		auto command = std::make_unique<ControlPlacementCommand>(
			this,
			interaction->Before,
			std::move(after),
			interaction->BeforeSelectionNames,
			afterSelectionNames,
			interaction->BeforePrimarySelectionName,
			afterPrimarySelectionName,
			operation,
			true);
		result = CommitAlreadyAppliedCommand(std::move(command));
	}
	catch (...)
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法创建位置交互历史命令。", false);
	}
	if (!result)
	{
		std::wstring restoreError;
		const bool restored = ControlPlacementCommand::Restore(
			this, interaction->Before, &restoreError);
		RestoreSelectionByNames(
			interaction->BeforeSelectionNames,
			interaction->BeforePrimarySelectionName, true);
		result.DocumentRestored = restored;
		if (!restored)
			result.Error += L" 位置交互起点恢复失败：" + restoreError;
	}
	PublishCanvasInteractionTransactionResult(operation, result);
	return result;
}

DesignerDocumentTransactionResult
DesignerCanvas::RollbackCanvasInteractionTransaction(std::wstring message)
{
	if (_activePropertyInteraction)
		return RollbackControlPropertyInteraction(std::move(message));
	if (_activePlacementInteraction)
		return RollbackPlacementInteraction(std::move(message));
	if (_activeInteractionTransaction.empty())
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	const auto operation = std::move(_activeInteractionTransaction);
	_activeInteractionTransaction.clear();
	auto result = RollbackDocumentEditTransaction();
	PublishCanvasInteractionTransactionResult(
		operation, result, std::move(message));
	return result;
}

DesignerDocumentTransactionResult
DesignerCanvas::RollbackControlPropertyInteraction(std::wstring message)
{
	if (!_activePropertyInteraction)
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	auto interaction = std::move(_activePropertyInteraction);
	std::wstring error;
	const bool restored = RestorePropertySnapshot(
		this, interaction->Before, &error);
	RestoreSelectionByNames(
		interaction->BeforeSelectionNames,
		interaction->BeforePrimarySelectionName, true);
	auto result = restored
		? DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::RolledBack)
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"属性交互回滚失败：" + error, false);
	PublishCanvasInteractionTransactionResult(
		interaction->Operation, result, std::move(message));
	return result;
}

DesignerDocumentTransactionResult
DesignerCanvas::RollbackPlacementInteraction(std::wstring message)
{
	if (!_activePlacementInteraction)
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	auto interaction = std::move(_activePlacementInteraction);
	std::wstring error;
	const bool restored = ControlPlacementCommand::Restore(
		this, interaction->Before, &error);
	RestoreSelectionByNames(
		interaction->BeforeSelectionNames,
		interaction->BeforePrimarySelectionName, true);
	auto result = restored
		? DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::RolledBack)
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"位置交互回滚失败：" + error, false);
	PublishCanvasInteractionTransactionResult(
		interaction->Operation, result, std::move(message));
	return result;
}

DesignerDocumentTransactionResult
DesignerCanvas::AbortCanvasInteractionTransaction(std::wstring error)
{
	if (_activePropertyInteraction)
		return AbortControlPropertyInteraction(std::move(error));
	if (_activePlacementInteraction)
		return AbortPlacementInteraction(std::move(error));
	if (error.empty()) error = L"画布交互修改被拒绝。";
	const auto operation = _activeInteractionTransaction.empty()
		? CurrentPointerInteractionOperation()
		: _activeInteractionTransaction;
	DesignerDocumentTransactionResult rollback =
		DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	if (!_activeInteractionTransaction.empty())
	{
		_activeInteractionTransaction.clear();
		rollback = RollbackDocumentEditTransaction();
	}
	if (!rollback)
		error += L" " + rollback.Error;
	auto result = DesignerDocumentTransactionResult::Failure(
		DesignerDocumentTransactionState::Aborted,
		std::move(error), rollback.Succeeded());
	PublishCanvasInteractionTransactionResult(operation, result);
	return result;
}

DesignerDocumentTransactionResult
DesignerCanvas::AbortControlPropertyInteraction(std::wstring error)
{
	if (error.empty()) error = L"画布属性交互修改被拒绝。";
	if (!_activePropertyInteraction)
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Aborted,
			std::move(error));
		PublishCanvasInteractionTransactionResult(
			CurrentPointerInteractionOperation(), result);
		return result;
	}
	auto interaction = std::move(_activePropertyInteraction);
	std::wstring restoreError;
	const bool restored = RestorePropertySnapshot(
		this, interaction->Before, &restoreError);
	RestoreSelectionByNames(
		interaction->BeforeSelectionNames,
		interaction->BeforePrimarySelectionName, true);
	if (!restored)
		error += L" 属性交互起点恢复失败：" + restoreError;
	auto result = DesignerDocumentTransactionResult::Failure(
		DesignerDocumentTransactionState::Aborted,
		std::move(error), restored);
	PublishCanvasInteractionTransactionResult(
		interaction->Operation, result);
	return result;
}

DesignerDocumentTransactionResult
DesignerCanvas::AbortPlacementInteraction(std::wstring error)
{
	if (error.empty()) error = L"画布位置交互修改被拒绝。";
	if (!_activePlacementInteraction)
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Aborted,
			std::move(error));
		PublishCanvasInteractionTransactionResult(
			CurrentPointerInteractionOperation(), result);
		return result;
	}
	auto interaction = std::move(_activePlacementInteraction);
	std::wstring restoreError;
	const bool restored = ControlPlacementCommand::Restore(
		this, interaction->Before, &restoreError);
	RestoreSelectionByNames(
		interaction->BeforeSelectionNames,
		interaction->BeforePrimarySelectionName, true);
	if (!restored)
		error += L" 位置交互起点恢复失败：" + restoreError;
	auto result = DesignerDocumentTransactionResult::Failure(
		DesignerDocumentTransactionState::Aborted,
		std::move(error), restored);
	PublishCanvasInteractionTransactionResult(
		interaction->Operation, result);
	return result;
}

void DesignerCanvas::PublishCanvasInteractionTransactionResult(
	const std::wstring& operation,
	const DesignerDocumentTransactionResult& result,
	std::wstring message)
{
	_lastInteractionTransaction = operation;
	_lastInteractionTransactionResult = result;
	_hasInteractionTransactionResult = true;
	DesignerCanvasInteractionTransactionEventArgs args{
		operation, std::move(message), result
	};
	cui::framework::EventAccess::Raise(OnInteractionTransactionCompleted, args);
}

void DesignerCanvas::PublishCanvasCommandResult(
	const std::wstring& operation,
	const std::wstring& label,
	const DesignerDocumentTransactionResult& result,
	std::wstring message)
{
	_lastCommandOperation = operation;
	_lastCommandLabel = label;
	_lastCommandResult = result;
	_hasCommandResult = true;
	DesignerCanvasCommandEventArgs args{
		operation, label, std::move(message), result
	};
	cui::framework::EventAccess::Raise(OnCommandCompleted, args);
}

void DesignerCanvas::NotifyDocumentStateChanged()
{
	DesignerCanvasDocumentStateEventArgs args{
		GetCurrentDocumentStateId(),
		GetSavedDocumentStateId(),
		IsDocumentDirty()
	};
	cui::framework::EventAccess::Raise(OnDocumentStateChanged, args);
}

std::wstring DesignerCanvas::CurrentPointerInteractionOperation() const
{
	if (_activePropertyInteraction)
		return _activePropertyInteraction->Operation;
	if (_activePlacementInteraction)
		return _activePlacementInteraction->Operation;
	if (!_activeInteractionTransaction.empty())
		return _activeInteractionTransaction;
	if (_isResizing) return L"ResizeSelection";
	if (_isDragging) return L"MoveSelection";
	if (_isBoxSelecting) return L"BoxSelection";
	return L"CanvasInteraction";
}

void DesignerCanvas::ResetPointerInteractionState()
{
	_isBoxSelecting = false;
	_boxSelectAddToSelection = false;
	_isDragging = false;
	_dragHasMoved = false;
	_dragLiftedToRoot = false;
	_dragStartItems.clear();
	_isResizing = false;
	_resizeHandle = DesignerControl::ResizeHandle::None;
	ClearAlignmentGuides();
	_interactionCursor = CursorKind::Arrow;
	this->InvalidateVisual();
}

DesignerDocumentTransactionResult
DesignerCanvas::CancelActivePointerInteraction(const std::wstring& reason)
{
	const auto operation = CurrentPointerInteractionOperation();
	const bool hadPointerInteraction = _isBoxSelecting || _isDragging
		|| _isResizing
		|| _activePropertyInteraction
		|| _activePlacementInteraction
		|| !_activeInteractionTransaction.empty();
	ResetPointerInteractionState();
	if (HasActiveDeltaInteraction() || !_activeInteractionTransaction.empty())
	{
		return RollbackCanvasInteractionTransaction(reason);
	}
	if (!hadPointerInteraction)
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	auto result = DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::Canceled);
	PublishCanvasInteractionTransactionResult(operation, result, reason);
	return result;
}

void DesignerCanvas::ClearInteractionTransactionResult()
{
	_lastInteractionTransaction.clear();
	_lastInteractionTransactionResult =
		DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	_hasInteractionTransactionResult = false;
}

void DesignerCanvas::ClearCommandResult()
{
	_lastCommandOperation.clear();
	_lastCommandLabel.clear();
	_lastCommandResult = DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::Unchanged);
	_hasCommandResult = false;
}

bool DesignerCanvas::HasLockedSelectedControls() const noexcept
{
	return std::any_of(
		_selectedControls.begin(), _selectedControls.end(),
		[](const auto& control) { return control && control->IsLocked; });
}

bool DesignerCanvas::AreAllSelectedControlsLocked() const noexcept
{
	return !_selectedControls.empty()
		&& std::all_of(
			_selectedControls.begin(), _selectedControls.end(),
			[](const auto& control) { return control && control->IsLocked; });
}

DesignerDocumentTransactionResult
DesignerCanvas::SetSelectedControlsLocked(bool locked)
{
	const std::wstring operation = locked ? L"SetLocked" : L"UnlockControls";
	auto finish = [this, &operation](
		DesignerDocumentTransactionResult result, std::wstring message = {})
	{
		PublishCanvasCommandResult(
			operation, operation, result, std::move(message));
		return result;
	};
	if (HasActiveDocumentTransaction() || HasActiveDeltaInteraction()
		|| _isDragging || _isResizing
		|| _isBoxSelecting || _isPanning)
	{
		return finish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"其他画布交互或文档事务进行中，不能修改锁定状态。"));
	}
	if (_selectedControls.empty() || !_selectedControl)
	{
		return finish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"没有可锁定或解锁的选中控件。"));
	}

	PropertyGridBinder binder;
	binder.SetCanvas(this);
	binder.BindControls(_selectedControls, _selectedControl);
	DesignerPropertyBatchSnapshot before;
	std::wstring error;
	if (!binder.CaptureControlPropertySnapshot(L"Locked", before, &error))
	{
		return finish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法建立锁定状态起点：" + error, false));
	}
	const auto selectionNames = CaptureSelectionNames();
	const auto primaryName = _selectedControl->Name;
	const auto applied = binder.ApplyControlPropertyValue(
		L"Locked", locked ? L"true" : L"false");
	if (!applied)
	{
		std::wstring restoreError;
		const bool restored = binder.RestoreBoundControlPropertySnapshot(
			before, &restoreError);
		RestoreSelectionByNames(selectionNames, primaryName, true);
		return finish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Aborted,
			L"无法修改控件锁定状态：" + applied.Error
				+ (restored ? L"" : L" 属性恢复失败：" + restoreError),
			restored));
	}

	DesignerPropertyBatchSnapshot after;
	if (!binder.CaptureControlPropertySnapshot(L"Locked", after, &error))
	{
		std::wstring restoreError;
		const bool restored = binder.RestoreBoundControlPropertySnapshot(
			before, &restoreError);
		RestoreSelectionByNames(selectionNames, primaryName, true);
		return finish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法建立锁定状态终点：" + error
				+ (restored ? L"" : L" 属性恢复失败：" + restoreError),
			restored));
	}
	if (before.EquivalentTo(after))
	{
		InvalidateVisual();
		return finish(DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged),
			locked ? L"选中控件已全部锁定。" : L"选中控件已全部解锁。");
	}

	const auto rollback = before;
	DesignerDocumentTransactionResult result;
	try
	{
		auto command = std::make_unique<ControlPropertyCommand>(
			this, std::move(before), std::move(after),
			selectionNames, selectionNames, primaryName, primaryName,
			operation, true, false);
		result = CommitAlreadyAppliedCommand(std::move(command));
	}
	catch (...)
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"记录锁定状态差量时抛出异常。", false);
	}
	if (!result || !result.HasChanges())
	{
		std::wstring restoreError;
		const bool restored = binder.RestoreBoundControlPropertySnapshot(
			rollback, &restoreError);
		RestoreSelectionByNames(selectionNames, primaryName, true);
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			result.Error.empty() ? L"无法把锁定状态加入撤销栈。"
				: result.Error + (restored ? L""
					: L" 属性恢复失败：" + restoreError),
			restored);
	}
	else
	{
		cui::framework::EventAccess::Raise(OnControlSelected, _selectedControl);
		InvalidateVisual();
	}
	return finish(result,
		locked ? L"已锁定选中控件。" : L"已解锁选中控件。");
}

DesignerDocumentTransactionResult
DesignerCanvas::NudgeSelectionBy(int dx, int dy)
{
	if (dx == 0 && dy == 0)
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	if (_selectedControls.empty() || !_selectedControl
		|| !_selectedControl->ControlInstance)
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"没有可移动的选中控件。");
		PublishCanvasInteractionTransactionResult(
			L"MoveSelection", result);
		return result;
	}
	if (HasLockedSelectedControls())
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"选中内容包含已锁定控件；请先解锁后再微调位置。");
		PublishCanvasInteractionTransactionResult(
			L"MoveSelection", result);
		return result;
	}

	DesignerControlPlacementSnapshot before;
	std::wstring error;
	if (!ControlPlacementCommand::Capture(
		this, _selectedControls, before, &error))
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法建立微调起点：" + error);
		PublishCanvasInteractionTransactionResult(
			L"MoveSelection", result);
		return result;
	}
	const auto beforeSelectionNames = CaptureSelectionNames();
	const auto beforePrimarySelectionName = _selectedControl->Name;
	try
	{
		BeginDragFromCurrentSelection(_dragStartPoint);
		// Keyboard nudge is a DIP delta, not a pointer-coordinate round trip.
		// Preserve subpixel layout origins (for example a 0.5 DIP Grid split)
		// so one key press cannot become a two-DIP Canvas.Left/Top change.
		for (const auto& item : _dragStartItems)
		{
			auto* control = item.ControlInstance;
			auto* parent = item.Parent;
			if (!control || !parent) continue;
			const auto controlLocation = control->GetAbsoluteLocationDip();
			const auto parentLocation = parent->GetAbsoluteLocationDip();
			const auto layoutOrigin = parent->GetVisualChildrenLayoutOriginDip();
			const auto renderOffset = parent->GetVisualChildrenRenderOffset();
			const float localX = controlLocation.x - parentLocation.x
				- layoutOrigin.x - renderOffset.x + static_cast<float>(dx);
			const float localY = controlLocation.y - parentLocation.y
				- layoutOrigin.y - renderOffset.y + static_cast<float>(dy);

			if (parent->Type() == UIClass::UI_RelativePanel)
			{
				auto margin = control->Margin;
				margin.Left = localX;
				margin.Top = localY;
				margin.Right = 0.0f;
				margin.Bottom = 0.0f;
				Canvas::SetLeft(*control, 0.0f);
				Canvas::SetTop(*control, 0.0f);
				control->Margin = margin;
			}
			else
			{
				if (!IsLayoutContainer(parent)
					&& UsesAlignmentManagedPlacement(control))
					ResetAlignmentForManualPlacement(control);
				Canvas::SetLeft(*control, localX);
				Canvas::SetTop(*control, localY);
				Canvas::SetRight(*control, cui::layout::UnsetCanvasOffset);
				Canvas::SetBottom(*control, cui::layout::UnsetCanvasOffset);
				control->Margin = {};
			}
			if (auto* panel = dynamic_cast<Panel*>(parent))
				RefreshDesignerPanelLayout(panel);
		}
		for (auto& selected : _selectedControls)
			if (selected && selected->ControlInstance)
				ClampControlToDesignSurface(selected->ControlInstance);
	}
	catch (...)
	{
		std::wstring restoreError;
		const bool restored = ControlPlacementCommand::Restore(
			this, before, &restoreError);
		ResetPointerInteractionState();
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"微调操作抛出异常。"
				+ (restored ? L"" : L" 布局恢复失败：" + restoreError),
			restored);
		PublishCanvasInteractionTransactionResult(
			L"MoveSelection", result);
		return result;
	}

	DesignerControlPlacementSnapshot after;
	if (!ControlPlacementCommand::Capture(
		this, _selectedControls, after, &error))
	{
		std::wstring restoreError;
		const bool restored = ControlPlacementCommand::Restore(
			this, before, &restoreError);
		RestoreSelectionByNames(
			beforeSelectionNames, beforePrimarySelectionName, true);
		ResetPointerInteractionState();
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法建立微调终点：" + error
				+ (restored ? L"" : L" 布局恢复失败：" + restoreError),
			restored);
		PublishCanvasInteractionTransactionResult(
			L"MoveSelection", result);
		return result;
	}
	const auto afterSelectionNames = CaptureSelectionNames();
	const auto afterPrimarySelectionName = _selectedControl
		? _selectedControl->Name : std::wstring{};
	if (before.EquivalentTo(after)
		&& beforeSelectionNames == afterSelectionNames
		&& beforePrimarySelectionName == afterPrimarySelectionName)
	{
		ResetPointerInteractionState();
		auto result = DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
		PublishCanvasInteractionTransactionResult(
			L"MoveSelection", result);
		return result;
	}

	DesignerDocumentTransactionResult result =
		DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法把微调差量加入撤销栈。", false);
	const auto rollbackBefore = before;
	try
	{
		auto command = std::make_unique<ControlPlacementCommand>(
			this,
			std::move(before),
			std::move(after),
			beforeSelectionNames,
			afterSelectionNames,
			beforePrimarySelectionName,
			afterPrimarySelectionName,
			L"NudgeSelection",
			true);
		result = CommitAlreadyAppliedCommand(std::move(command));
	}
	catch (...)
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"记录微调差量时抛出异常。", false);
	}
	if (!result || !result.HasChanges())
	{
		std::wstring restoreError;
		const bool restored = ControlPlacementCommand::Restore(
			this, rollbackBefore, &restoreError);
		RestoreSelectionByNames(
			beforeSelectionNames, beforePrimarySelectionName, true);
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			result.Error.empty()
				? L"无法把微调差量加入撤销栈。"
				: result.Error + (restored
					? L"" : L" 布局恢复失败：" + restoreError),
			restored);
	}
	ResetPointerInteractionState();
	PublishCanvasInteractionTransactionResult(
		L"MoveSelection", result);
	return result;
}

DesignerDocumentTransactionResult DesignerCanvas::ApplyTabOrderAssignments(
	const std::vector<std::pair<std::shared_ptr<DesignerControl>, int>>&
		assignments,
	const std::wstring& operation,
	std::wstring successMessage)
{
	auto finish = [this, &operation](
		DesignerDocumentTransactionResult result,
		std::wstring message = {})
	{
		PublishCanvasCommandResult(
			operation, operation, result, std::move(message));
		return result;
	};
	if (HasActiveDocumentTransaction() || _isDragging || _isResizing
		|| _isBoxSelecting || _isPanning)
	{
		return finish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"其他画布交互或文档事务进行中，不能修改 Tab 顺序。"));
	}
	if (assignments.empty())
	{
		return finish(DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged),
			L"没有可编排 Tab 顺序的控件。");
	}

	std::vector<std::shared_ptr<DesignerControl>> targets;
	targets.reserve(assignments.size());
	for (const auto& [target, index] : assignments)
	{
		if (!IsTabOrderCandidate(target) || index < 0)
		{
			return finish(DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Rejected,
				L"Tab 顺序目标无效，或控件不能接收键盘焦点。"));
		}
		if (std::find(targets.begin(), targets.end(), target)
			!= targets.end())
		{
			return finish(DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Rejected,
				L"Tab 顺序批次包含重复控件。"));
		}
		targets.push_back(target);
	}

	PropertyGridBinder batchBinder;
	batchBinder.SetCanvas(this);
	batchBinder.BindControls(targets, targets.front());
	DesignerPropertyBatchSnapshot before;
	std::wstring error;
	if (!batchBinder.CaptureControlPropertySnapshot(
		L"TabIndex", before, &error))
	{
		return finish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法建立 Tab 顺序属性起点：" + error, false));
	}
	const auto selectionNames = CaptureSelectionNames();
	const auto primaryName = _selectedControl
		? _selectedControl->Name : std::wstring{};

	for (const auto& [target, index] : assignments)
	{
		PropertyGridBinder targetBinder;
		targetBinder.SetCanvas(this);
		targetBinder.BindControl(target);
		auto applied = targetBinder.ApplyControlPropertyValue(
			L"TabIndex", std::to_wstring(index));
		if (applied) continue;

		std::wstring restoreError;
		const bool restored = batchBinder.RestoreBoundControlPropertySnapshot(
			before, &restoreError);
		RestoreSelectionByNames(selectionNames, primaryName, true);
		return finish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Aborted,
			L"无法设置 “" + target->Name + L"” 的 TabIndex："
				+ applied.Error + (restored ? L""
					: L" 属性恢复失败：" + restoreError),
			restored));
	}

	DesignerPropertyBatchSnapshot after;
	if (!batchBinder.CaptureControlPropertySnapshot(
		L"TabIndex", after, &error))
	{
		std::wstring restoreError;
		const bool restored = batchBinder.RestoreBoundControlPropertySnapshot(
			before, &restoreError);
		RestoreSelectionByNames(selectionNames, primaryName, true);
		return finish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法建立 Tab 顺序属性终点：" + error
				+ (restored ? L"" : L" 属性恢复失败：" + restoreError),
			restored));
	}
	if (before.EquivalentTo(after))
	{
		InvalidateVisual();
		return finish(DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged),
			std::move(successMessage));
	}

	const auto rollback = before;
	DesignerDocumentTransactionResult result;
	try
	{
		auto command = std::make_unique<ControlPropertyCommand>(
			this, std::move(before), std::move(after),
			selectionNames, selectionNames, primaryName, primaryName,
			operation, true, false);
		result = CommitAlreadyAppliedCommand(std::move(command));
	}
	catch (...)
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"记录 Tab 顺序属性差量时抛出异常。", false);
	}
	if (!result || !result.HasChanges())
	{
		std::wstring restoreError;
		const bool restored = batchBinder.RestoreBoundControlPropertySnapshot(
			rollback, &restoreError);
		RestoreSelectionByNames(selectionNames, primaryName, true);
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			result.Error.empty() ? L"无法把 Tab 顺序加入撤销栈。"
				: result.Error + (restored ? L""
					: L" 属性恢复失败：" + restoreError),
			restored);
	}
	else
	{
		cui::framework::EventAccess::Raise(OnControlSelected, _selectedControl);
		InvalidateVisual();
	}
	return finish(result, std::move(successMessage));
}

DesignerDocumentTransactionResult DesignerCanvas::AssignTabOrderIndex(
	const std::shared_ptr<DesignerControl>& control,
	int tabIndex)
{
	const int effectiveIndex = (std::max)(0, tabIndex);
	auto result = ApplyTabOrderAssignments(
		{ { control, effectiveIndex } },
		L"SetTabOrder",
		control ? L"已将 “" + control->Name + L"” 设为 TabIndex "
			+ std::to_wstring(effectiveIndex) + L"。" : std::wstring{});
	if (result && _tabOrderMode && control)
	{
		_nextTabOrderIndex = effectiveIndex + 1;
		_lastTabOrderStableId = control->StableId;
		NotifyTabOrderStateChanged();
		InvalidateVisual();
	}
	return result;
}

DesignerDocumentTransactionResult DesignerCanvas::AutoArrangeTabOrder()
{
	auto controls = CollectTabOrderCandidates();
	std::stable_sort(controls.begin(), controls.end(),
		[this](const auto& left, const auto& right)
		{
			const auto leftRect = GetControlRectInCanvas(left->ControlInstance);
			const auto rightRect = GetControlRectInCanvas(right->ControlInstance);
			if (leftRect.top != rightRect.top)
				return leftRect.top < rightRect.top;
			if (leftRect.left != rightRect.left)
				return leftRect.left < rightRect.left;
			if (leftRect.bottom != rightRect.bottom)
				return leftRect.bottom < rightRect.bottom;
			if (leftRect.right != rightRect.right)
				return leftRect.right < rightRect.right;
			return left->StableId < right->StableId;
		});
	std::vector<std::pair<std::shared_ptr<DesignerControl>, int>> assignments;
	assignments.reserve(controls.size());
	for (size_t index = 0; index < controls.size(); ++index)
		assignments.emplace_back(
			controls[index], static_cast<int>(index));
	auto result = ApplyTabOrderAssignments(
		assignments,
		L"AutoTabOrder",
		L"已按从上到下、从左到右的布局顺序编排 "
			+ std::to_wstring(controls.size()) + L" 个控件。");
	if (result && _tabOrderMode)
	{
		_nextTabOrderIndex = static_cast<int>(controls.size());
		_lastTabOrderStableId = controls.empty()
			? 0 : controls.back()->StableId;
		NotifyTabOrderStateChanged();
		InvalidateVisual();
	}
	return result;
}

DesignerDocumentTransactionResult DesignerCanvas::ArrangeSelection(
	DesignerSelectionArrangeAction action)
{
	auto labelFor = [](DesignerSelectionArrangeAction value)
	{
		switch (value)
		{
		case DesignerSelectionArrangeAction::AlignLeft: return L"AlignLeft";
		case DesignerSelectionArrangeAction::AlignHorizontalCenters: return L"AlignHorizontalCenters";
		case DesignerSelectionArrangeAction::AlignRight: return L"AlignRight";
		case DesignerSelectionArrangeAction::AlignTop: return L"AlignTop";
		case DesignerSelectionArrangeAction::AlignVerticalCenters: return L"AlignVerticalCenters";
		case DesignerSelectionArrangeAction::AlignBottom: return L"AlignBottom";
		case DesignerSelectionArrangeAction::MakeSameWidth: return L"MakeSameWidth";
		case DesignerSelectionArrangeAction::MakeSameHeight: return L"MakeSameHeight";
		case DesignerSelectionArrangeAction::MakeSameSize: return L"MakeSameSize";
		case DesignerSelectionArrangeAction::DistributeHorizontally: return L"DistributeHorizontally";
		case DesignerSelectionArrangeAction::DistributeVertically: return L"DistributeVertically";
		case DesignerSelectionArrangeAction::BringForward: return L"BringForward";
		case DesignerSelectionArrangeAction::SendBackward: return L"SendBackward";
		case DesignerSelectionArrangeAction::BringToFront: return L"BringToFront";
		case DesignerSelectionArrangeAction::SendToBack: return L"SendToBack";
		}
		return L"ArrangeSelection";
	};
	const std::wstring label = labelFor(action);
	auto publish = [this, &label](DesignerDocumentTransactionResult result,
		std::wstring message = {})
	{
		PublishCanvasCommandResult(label, label, result, std::move(message));
		return result;
	};

	if (HasActiveDocumentTransaction() || HasActiveDeltaInteraction()
		|| _isDragging || _isResizing
		|| _isBoxSelecting)
	{
		return publish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布交互或事务进行中，不能排列控件。"));
	}
	if (_selectedControls.empty() || !_selectedControl
		|| !_selectedControl->ControlInstance)
	{
		return publish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"没有可排列的选中控件。"));
	}
	if (HasLockedSelectedControls())
	{
		return publish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"选中内容包含已锁定控件；请先解锁后再排列。"));
	}

	const bool layerAction =
		action == DesignerSelectionArrangeAction::BringForward
		|| action == DesignerSelectionArrangeAction::SendBackward
		|| action == DesignerSelectionArrangeAction::BringToFront
		|| action == DesignerSelectionArrangeAction::SendToBack;
	const bool distribution =
		action == DesignerSelectionArrangeAction::DistributeHorizontally
		|| action == DesignerSelectionArrangeAction::DistributeVertically;
	const size_t minimum = layerAction ? 1U : distribution ? 3U : 2U;
	if (_selectedControls.size() < minimum)
	{
		return publish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			layerAction
				? L"层级操作至少需要一个选中控件。"
				: distribution
				? L"分布操作至少需要三个选中控件。"
				: L"对齐或同尺寸操作至少需要两个选中控件。"));
	}

	Control* parent = _selectedControl->ControlInstance->GetVisualParent();
	if (!parent)
	{
		return publish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"选中控件没有可排列的父级。"));
	}
	for (const auto& selected : _selectedControls)
	{
		if (!selected || !selected->ControlInstance
			|| selected->ControlInstance->GetVisualParent() != parent)
		{
			return publish(DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Rejected,
				L"只能排列同一父级中的控件。"));
		}
	}
	if (!layerAction && IsLayoutContainer(parent))
	{
		return publish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"当前父级由布局系统管理位置；请使用容器的布局属性，而不是几何排列。"));
	}

	DesignerControlPlacementSnapshot before;
	std::wstring error;
	if (!ControlPlacementCommand::Capture(
		this, _selectedControls, before, &error))
	{
		return publish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法建立排列起点：" + error));
	}
	const auto selectionNames = CaptureSelectionNames();
	const auto primaryName = _selectedControl->Name;
	bool changed = false;

	try
	{
		if (layerAction)
		{
			std::unordered_set<Control*> selectedSet;
			for (const auto& selected : _selectedControls)
				selectedSet.insert(selected->ControlInstance);
			std::unordered_set<Control*> peerSet;
			for (const auto& candidate : _designerControls)
				if (candidate && candidate->ControlInstance
					&& candidate->ControlInstance->GetVisualParent() == parent)
					peerSet.insert(candidate->ControlInstance);
			auto zOrder = parent->GetVisualChildrenInZOrder();
			zOrder.erase(std::remove_if(zOrder.begin(), zOrder.end(),
				[&peerSet](Control* control)
				{
					return !control || !peerSet.contains(control);
				}), zOrder.end());

			auto moveAfter = [parent](Control* control, Control* anchor)
			{
				const int oldIndex = parent->IndexOfVisualChild(control);
				const int anchorIndex = parent->IndexOfVisualChild(anchor);
				if (oldIndex < 0 || anchorIndex < 0 || oldIndex == anchorIndex)
					return false;
				const int newIndex = oldIndex < anchorIndex
					? anchorIndex : anchorIndex + 1;
				return newIndex == oldIndex || parent->MoveVisualChild(
					static_cast<size_t>(oldIndex), static_cast<size_t>(newIndex));
			};
			auto moveBefore = [parent](Control* control, Control* anchor)
			{
				const int oldIndex = parent->IndexOfVisualChild(control);
				const int anchorIndex = parent->IndexOfVisualChild(anchor);
				if (oldIndex < 0 || anchorIndex < 0 || oldIndex == anchorIndex)
					return false;
				const int newIndex = oldIndex < anchorIndex
					? anchorIndex - 1 : anchorIndex;
				return newIndex == oldIndex || parent->MoveVisualChild(
					static_cast<size_t>(oldIndex), static_cast<size_t>(newIndex));
			};

			if (action == DesignerSelectionArrangeAction::BringForward)
			{
				for (int index = static_cast<int>(zOrder.size()) - 2;
					index >= 0; --index)
				{
					auto* control = zOrder[static_cast<size_t>(index)];
					auto* neighbor = zOrder[static_cast<size_t>(index + 1)];
					if (!selectedSet.contains(control)
						|| selectedSet.contains(neighbor)) continue;
					control->ZIndex = neighbor->ZIndex;
					if (!moveAfter(control, neighbor))
						throw std::runtime_error("layer move failed");
					std::swap(zOrder[static_cast<size_t>(index)],
						zOrder[static_cast<size_t>(index + 1)]);
					changed = true;
				}
			}
			else if (action == DesignerSelectionArrangeAction::SendBackward)
			{
				for (size_t index = 1; index < zOrder.size(); ++index)
				{
					auto* control = zOrder[index];
					auto* neighbor = zOrder[index - 1];
					if (!selectedSet.contains(control)
						|| selectedSet.contains(neighbor)) continue;
					control->ZIndex = neighbor->ZIndex;
					if (!moveBefore(control, neighbor))
						throw std::runtime_error("layer move failed");
					std::swap(zOrder[index], zOrder[index - 1]);
					changed = true;
				}
			}
			else
			{
				std::vector<Control*> selectedInOrder;
				std::vector<Control*> desired;
				for (auto* control : zOrder)
					(selectedSet.contains(control)
						? selectedInOrder : desired).push_back(control);
				if (action == DesignerSelectionArrangeAction::BringToFront)
					desired.insert(desired.end(), selectedInOrder.begin(),
						selectedInOrder.end());
				else
					desired.insert(desired.begin(), selectedInOrder.begin(),
						selectedInOrder.end());
				if (desired != zOrder)
				{
					const auto boundary = action
						== DesignerSelectionArrangeAction::BringToFront
						? (std::max_element)(zOrder.begin(), zOrder.end(),
							[](Control* left, Control* right)
							{ return left->ZIndex < right->ZIndex; })
						: (std::min_element)(zOrder.begin(), zOrder.end(),
							[](Control* left, Control* right)
							{ return left->ZIndex < right->ZIndex; });
					const int boundaryZ = (*boundary)->ZIndex;
					if (action == DesignerSelectionArrangeAction::BringToFront)
					{
						Control* anchor = nullptr;
						for (auto* control : zOrder)
							if (!selectedSet.contains(control)) anchor = control;
						for (auto* control : selectedInOrder)
						{
							control->ZIndex = boundaryZ;
							if (anchor && !moveAfter(control, anchor))
								throw std::runtime_error("layer move failed");
							anchor = control;
						}
					}
					else
					{
						Control* anchor = nullptr;
						for (auto* control : zOrder)
							if (!selectedSet.contains(control)) { anchor = control; break; }
						for (auto it = selectedInOrder.rbegin();
							it != selectedInOrder.rend(); ++it)
						{
							(*it)->ZIndex = boundaryZ;
							if (anchor && !moveBefore(*it, anchor))
								throw std::runtime_error("layer move failed");
							anchor = *it;
						}
					}
					changed = true;
				}
			}
			if (changed)
			{
				RefreshDesignerPanelLayout(parent);
				parent->InvalidateVisual();
			}
		}
		else
		{
			const RECT reference = GetControlRectInCanvas(
				_selectedControl->ControlInstance);
			if (distribution)
			{
				struct Item { std::shared_ptr<DesignerControl> Control; RECT Rect; };
				std::vector<Item> items;
				items.reserve(_selectedControls.size());
				for (const auto& selected : _selectedControls)
					items.push_back({ selected,
						GetControlRectInCanvas(selected->ControlInstance) });
				const bool horizontal = action
					== DesignerSelectionArrangeAction::DistributeHorizontally;
				std::stable_sort(items.begin(), items.end(),
					[horizontal](const Item& left, const Item& right)
					{
						return horizontal ? left.Rect.left < right.Rect.left
							: left.Rect.top < right.Rect.top;
					});
				double totalSize = 0.0;
				for (const auto& item : items)
					totalSize += horizontal
						? item.Rect.right - item.Rect.left
						: item.Rect.bottom - item.Rect.top;
				const double outerSpan = horizontal
					? items.back().Rect.right - items.front().Rect.left
					: items.back().Rect.bottom - items.front().Rect.top;
				const double gap = (outerSpan - totalSize)
					/ static_cast<double>(items.size() - 1);
				double cursor = horizontal
					? static_cast<double>(items.front().Rect.right)
					: static_cast<double>(items.front().Rect.bottom);
				for (size_t index = 1; index + 1 < items.size(); ++index)
				{
					RECT rect = items[index].Rect;
					const int position = static_cast<int>(std::lround(cursor + gap));
					if (horizontal)
					{
						const int width = rect.right - rect.left;
						rect.left = position;
						rect.right = position + width;
						cursor += gap + width;
					}
					else
					{
						const int height = rect.bottom - rect.top;
						rect.top = position;
						rect.bottom = position + height;
						cursor += gap + height;
					}
					ApplyRectToControl(items[index].Control->ControlInstance, rect);
				}
			}
			else
			{
				for (const auto& selected : _selectedControls)
				{
					if (selected == _selectedControl) continue;
					RECT rect = GetControlRectInCanvas(selected->ControlInstance);
					const int width = rect.right - rect.left;
					const int height = rect.bottom - rect.top;
					switch (action)
					{
					case DesignerSelectionArrangeAction::AlignLeft:
						rect.left = reference.left; rect.right = rect.left + width; break;
					case DesignerSelectionArrangeAction::AlignHorizontalCenters:
						rect.left = (reference.left + reference.right - width) / 2;
						rect.right = rect.left + width; break;
					case DesignerSelectionArrangeAction::AlignRight:
						rect.right = reference.right; rect.left = rect.right - width; break;
					case DesignerSelectionArrangeAction::AlignTop:
						rect.top = reference.top; rect.bottom = rect.top + height; break;
					case DesignerSelectionArrangeAction::AlignVerticalCenters:
						rect.top = (reference.top + reference.bottom - height) / 2;
						rect.bottom = rect.top + height; break;
					case DesignerSelectionArrangeAction::AlignBottom:
						rect.bottom = reference.bottom; rect.top = rect.bottom - height; break;
					case DesignerSelectionArrangeAction::MakeSameWidth:
						rect.right = rect.left + reference.right - reference.left; break;
					case DesignerSelectionArrangeAction::MakeSameHeight:
						rect.bottom = rect.top + reference.bottom - reference.top; break;
					case DesignerSelectionArrangeAction::MakeSameSize:
						rect.right = rect.left + reference.right - reference.left;
						rect.bottom = rect.top + reference.bottom - reference.top; break;
					default: break;
					}
					ApplyRectToControl(selected->ControlInstance, rect);
				}
			}
			changed = true;
		}
	}
	catch (...)
	{
		std::wstring restoreError;
		const bool restored = ControlPlacementCommand::Restore(
			this, before, &restoreError);
		RestoreSelectionByNames(selectionNames, primaryName, true);
		return publish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"排列控件时发生异常。" + (restored ? L""
				: L" 布局恢复失败：" + restoreError), restored));
	}

	DesignerControlPlacementSnapshot after;
	if (!changed || !ControlPlacementCommand::Capture(
		this, _selectedControls, after, &error))
	{
		if (!changed)
			return publish(DesignerDocumentTransactionResult::Success(
				DesignerDocumentTransactionState::Unchanged),
				L"控件已经处于请求的排列位置。");
		std::wstring restoreError;
		const bool restored = ControlPlacementCommand::Restore(
			this, before, &restoreError);
		RestoreSelectionByNames(selectionNames, primaryName, true);
		return publish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法建立排列终点：" + error + (restored ? L""
				: L" 布局恢复失败：" + restoreError), restored));
	}
	if (before.EquivalentTo(after))
		return publish(DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged),
			L"控件已经处于请求的排列位置。");

	const auto rollback = before;
	auto command = std::make_unique<ControlPlacementCommand>(
		this, std::move(before), std::move(after),
		selectionNames, selectionNames, primaryName, primaryName,
		label, true);
	auto result = CommitAlreadyAppliedCommand(std::move(command));
	if (!result || !result.HasChanges())
	{
		std::wstring restoreError;
		const bool restored = ControlPlacementCommand::Restore(
			this, rollback, &restoreError);
		RestoreSelectionByNames(selectionNames, primaryName, true);
		return publish(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			result.Error.empty() ? L"无法把排列操作加入撤销栈。"
				: result.Error + (restored ? L""
					: L" 布局恢复失败：" + restoreError), restored));
	}
	cui::framework::EventAccess::Raise(OnControlSelected, _selectedControl);
	this->InvalidateVisual();
	return publish(result, L"已排列 "
		+ std::to_wstring(_selectedControls.size()) + L" 个控件。");
}

DesignerDocumentTransactionResult DesignerCanvas::MoveControlInHierarchy(
	int sourceStableId,
	std::optional<int> targetStableId,
	DesignerHierarchyDropPosition position)
{
	constexpr auto label = L"MoveControlInHierarchy";
	auto finishEarly = [this](DesignerDocumentTransactionResult result,
		std::wstring message = {})
	{
		PublishCanvasCommandResult(
			L"MoveHierarchy", L"MoveControlInHierarchy", result,
			std::move(message));
		return result;
	};
	auto reject = [&](std::wstring error)
	{
		return finishEarly(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			std::move(error)));
	};
	if (!_clientSurface || HasActiveDocumentTransaction())
		return reject(L"当前文档事务进行中，不能调整控件层级。");

	auto findByStableId = [this](int stableId)
		-> std::shared_ptr<DesignerControl>
	{
		const auto found = std::find_if(
			_designerControls.begin(), _designerControls.end(),
			[stableId](const std::shared_ptr<DesignerControl>& candidate)
			{
				return candidate && candidate->ControlInstance
					&& candidate->StableId == stableId;
			});
		return found == _designerControls.end() ? nullptr : *found;
	};
	const auto source = findByStableId(sourceStableId);
	if (!source) return reject(L"要移动的控件已经不存在。");
	if (source->IsLocked)
		return reject(L"该控件已锁定；请先解锁后再调整层级。");

	std::shared_ptr<DesignerControl> target;
	if (targetStableId)
	{
		target = findByStableId(*targetStableId);
		if (!target) return reject(L"拖放目标已经不存在。");
		if (target == source)
			return reject(L"不能把控件拖放到自身。");
	}
	else if (position != DesignerHierarchyDropPosition::Inside)
	{
		return reject(L"窗体根节点只接受“置于内部”。");
	}
	if (!target)
	{
		target = GetDocumentContentRootRecord();
		if (!target || target == source)
			return reject(L"Window.Content 已是唯一根元素，不能创建同级根。");
		position = DesignerHierarchyDropPosition::Inside;
	}

	DesignerControlPlacementSnapshot before;
	std::wstring error;
	if (!ControlPlacementCommand::Capture(this, { source }, before, &error))
		return finishEarly(DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法记录层级移动起点：" + error, false));
	DesignerControlPlacementSnapshot after = before;
	auto& desired = after.Targets.front();
	auto* moving = source->ControlInstance;
	Control* desiredRuntimeParent = nullptr;

	auto copyParent = [&desired](
		const DesignerControlPlacementState& targetState)
	{
		desired.ParentKind = targetState.ParentKind;
		desired.ParentName = targetState.ParentName;
		desired.ParentType = targetState.ParentType;
		desired.ComponentContentProperty =
			targetState.ComponentContentProperty;
	};

	if (position == DesignerHierarchyDropPosition::Inside)
	{
		auto destination = target;
		if (target->Type == UIClass::UI_TabControl
			&& source->Type != UIClass::UI_TabItem)
		{
			auto* tabs = dynamic_cast<TabControl*>(target->ControlInstance);
			if (!tabs || static_cast<int>(tabs->ItemCount()) <= 0)
				return reject(L"空 TabControl 尚无可接收控件的 TabItem。");
			const int pageIndex = (std::clamp)(tabs->SelectedIndex, 0,
				static_cast<int>(tabs->ItemCount()) - 1);
			auto* page = tabs->GetItem(pageIndex);
			const auto pageRecord = std::find_if(
				_designerControls.begin(), _designerControls.end(),
				[page](const std::shared_ptr<DesignerControl>& candidate)
				{
					return candidate && candidate->ControlInstance == page;
				});
			if (pageRecord == _designerControls.end())
				return reject(L"TabControl 当前 TabItem 缺少 authored 设计节点。");
			destination = *pageRecord;
		}

		auto* targetControl = destination->ControlInstance;
		if (!IsContainerControl(targetControl))
			return reject(L"目标控件不是可承载子控件的容器。");
		desired.ParentName = destination->Name;
		desired.ParentType = destination->Type;
		if (auto* items = dynamic_cast<ItemsControl*>(targetControl))
		{
			if (items->GetItemsSource()
				|| !cui::framework::TemplateAccess::GetItemsHost(*items))
				return reject(L"ItemsSource 驱动的 ItemsControl 不接受 authored Items。");
			desired.ParentKind = DesignerPlacementParentKind::ItemsControl;
			desired.ComponentContentProperty.clear();
			desiredRuntimeParent =
				cui::framework::TemplateAccess::GetItemsHost(*items);
		}
		else if (!destination->ComponentType.Empty())
		{
			desired.ParentKind = DesignerPlacementParentKind::Control;
			const auto content = std::find_if(
				destination->ComponentContentProperties.begin(),
				destination->ComponentContentProperties.end(),
				[](const auto& property) { return property.IsDefault; });
			if (content == destination->ComponentContentProperties.end())
				return reject(L"目标组件没有默认视觉内容属性。");
			const auto presenter = destination->ComponentContentPresenters.find(
				content->Name);
			if (presenter == destination->ComponentContentPresenters.end()
				|| !presenter->second)
				return reject(L"目标组件的默认内容 Presenter 不可用。");
			if (content->Cardinality ==
				DesignerComponentContentCardinality::Single)
			{
				const auto occupied = std::any_of(
					_designerControls.begin(), _designerControls.end(),
					[&](const auto& candidate)
					{
						return candidate && candidate != source
							&& candidate->DesignerParent == targetControl
							&& candidate->ComponentContentProperty
								== content->Name;
					});
				if (occupied)
					return reject(L"目标组件的默认单值内容已经被占用。");
			}
			desired.ComponentContentProperty = content->Name;
			desiredRuntimeParent = presenter->second;
		}
		else
		{
			desired.ParentKind = DesignerPlacementParentKind::Control;
			desired.ComponentContentProperty.clear();
			desiredRuntimeParent = targetControl;
		}
	}
	else
	{
		DesignerControlPlacementSnapshot targetSnapshot;
		if (!ControlPlacementCommand::Capture(
			this, { target }, targetSnapshot, &error))
			return finishEarly(DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"无法解析拖放目标父级：" + error, false));
		const auto& targetState = targetSnapshot.Targets.front();
		copyParent(targetState);
		desiredRuntimeParent = target->ControlInstance->GetVisualParent();
		if (!desiredRuntimeParent)
			return reject(L"目标所在集合不接受普通控件重排。");

		const int targetIndex = desiredRuntimeParent->IndexOfVisualChild(
			target->ControlInstance);
		if (targetIndex < 0)
			return reject(L"拖放目标不在其父级集合中。");
		if (moving->GetVisualParent() == desiredRuntimeParent)
		{
			const int sourceIndex = desiredRuntimeParent->IndexOfVisualChild(moving);
			if (sourceIndex < 0)
				return reject(L"移动控件不在其父级集合中。");
			if (position == DesignerHierarchyDropPosition::Before)
				desired.ChildIndex = sourceIndex < targetIndex
					? targetIndex - 1 : targetIndex;
			else
				desired.ChildIndex = sourceIndex < targetIndex
					? targetIndex : targetIndex + 1;
		}
		else
		{
			desired.ChildIndex = targetIndex
				+ (position == DesignerHierarchyDropPosition::After ? 1 : 0);
		}
	}

	if (!desiredRuntimeParent)
		return reject(L"无法解析拖放后的运行时父级。");
	if (desiredRuntimeParent == moving
		|| IsDescendantOf(moving, desiredRuntimeParent))
		return reject(L"该层级移动会形成父子循环。");
	if (!LayoutBridge::CanAcceptChild(desiredRuntimeParent, source->Type))
		return reject(L"目标容器不接受该控件类型。");
	if (position == DesignerHierarchyDropPosition::Inside)
	{
		if (desired.ParentKind == DesignerPlacementParentKind::ItemsControl)
		{
			auto* items = dynamic_cast<ItemsControl*>(
				desiredRuntimeParent->GetLogicalParent());
			const int count = items
				? static_cast<int>(items->AuthoredItemCount())
				: desiredRuntimeParent->VisualChildCount();
			desired.ChildIndex = moving->GetVisualParent() == desiredRuntimeParent
				? (std::max)(0, count - 1) : count;
		}
		else
			desired.ChildIndex = moving->GetVisualParent() == desiredRuntimeParent
				? (std::max)(0, desiredRuntimeParent->VisualChildCount() - 1)
				: desiredRuntimeParent->VisualChildCount();
	}

	if (moving->GetVisualParent() != desiredRuntimeParent)
	{
		auto clearCanvasPlacement = [&desired]()
		{
			desired.CanvasLeft = cui::layout::UnsetCanvasOffset;
			desired.CanvasTop = cui::layout::UnsetCanvasOffset;
			desired.CanvasRight = cui::layout::UnsetCanvasOffset;
			desired.CanvasBottom = cui::layout::UnsetCanvasOffset;
			desired.SetLocalValue(
				DesignerPlacementLocalValue::CanvasLeft, false);
			desired.SetLocalValue(
				DesignerPlacementLocalValue::CanvasTop, false);
			desired.SetLocalValue(
				DesignerPlacementLocalValue::CanvasRight, false);
			desired.SetLocalValue(
				DesignerPlacementLocalValue::CanvasBottom, false);
		};
		const RECT rect = GetControlRectInCanvas(moving);
		const POINT local = CanvasToChildLayoutPoint(
			{ rect.left, rect.top }, desiredRuntimeParent);
		const POINT centerLocal = CanvasToContainerPoint(
			{ (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 },
			desiredRuntimeParent);
		desired.Horizontal = HorizontalAlignment::Left;
		desired.Vertical = VerticalAlignment::Top;
		desired.SetLocalValue(
			DesignerPlacementLocalValue::HorizontalAlignment);
		desired.SetLocalValue(
			DesignerPlacementLocalValue::VerticalAlignment);
		switch (desiredRuntimeParent->Type())
		{
		case UIClass::UI_Grid:
		{
			if (auto* grid = dynamic_cast<Grid*>(desiredRuntimeParent))
			{
				int row = 0;
				int column = 0;
				if (grid->TryGetCellAtPoint(cui::core::Point{
					static_cast<float>(centerLocal.x),
					static_cast<float>(centerLocal.y) }, row, column))
				{
					desired.GridRow = row;
					desired.GridColumn = column;
					desired.SetLocalValue(
						DesignerPlacementLocalValue::GridRow);
					desired.SetLocalValue(
						DesignerPlacementLocalValue::GridColumn);
				}
			}
			desired.Horizontal = HorizontalAlignment::Stretch;
			desired.Vertical = VerticalAlignment::Stretch;
			clearCanvasPlacement();
			break;
		}
		case UIClass::UI_StackPanel:
		case UIClass::UI_DockPanel:
		case UIClass::UI_WrapPanel:
			clearCanvasPlacement();
			break;
		case UIClass::UI_RelativePanel:
			clearCanvasPlacement();
			desired.Margin.Left = static_cast<float>(local.x);
			desired.Margin.Top = static_cast<float>(local.y);
			desired.Margin.Right = 0.0f;
			desired.Margin.Bottom = 0.0f;
			desired.SetLocalValue(
				DesignerPlacementLocalValue::Margin);
			break;
		default:
		{
			desired.CanvasLeft = static_cast<float>(local.x);
			desired.CanvasTop = static_cast<float>(local.y);
			desired.CanvasRight = cui::layout::UnsetCanvasOffset;
			desired.CanvasBottom = cui::layout::UnsetCanvasOffset;
			desired.Margin = {};
			desired.SetLocalValue(
				DesignerPlacementLocalValue::CanvasLeft);
			desired.SetLocalValue(
				DesignerPlacementLocalValue::CanvasTop);
			desired.SetLocalValue(
				DesignerPlacementLocalValue::CanvasRight, false);
			desired.SetLocalValue(
				DesignerPlacementLocalValue::CanvasBottom, false);
			desired.SetLocalValue(
				DesignerPlacementLocalValue::Margin, false);
			break;
		}
		}
	}

	if (before.EquivalentTo(after))
		return finishEarly(DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged),
			L"控件已经处于请求的层级位置。");
	const auto selectionNames = CaptureSelectionNames();
	const auto primaryName = _selectedControl
		? _selectedControl->Name : source->Name;
	auto command = std::make_unique<ControlPlacementCommand>(
		this, std::move(before), std::move(after),
		selectionNames, selectionNames, primaryName, primaryName,
		label, false);
	return ExecuteCommand(std::move(command));
}

void DesignerCanvas::ApplyRectToControl(
	Control* c, const RECT& rectInCanvas, bool preserveSize)
{
	if (!c) return;
	Control* parent = c->GetVisualParent() ? c->GetVisualParent() : (_clientSurface ? (Control*)_clientSurface : (Control*)_designSurface);
	if (!parent) return;

	POINT newLocal = CanvasToChildLayoutPoint(
		{ rectInCanvas.left, rectInCanvas.top }, parent);
	int newW = rectInCanvas.right - rectInCanvas.left;
	int newH = rectInCanvas.bottom - rectInCanvas.top;
	if (newW < 0) newW = 0;
	if (newH < 0) newH = 0;

	if (parent->Type() != UIClass::UI_RelativePanel && !IsLayoutContainer(parent) && UsesAlignmentManagedPlacement(c))
	{
		ResetAlignmentForManualPlacement(c);
	}

	// RelativePanel：运行时主要用 Margin 表达定位
	if (parent->Type() == UIClass::UI_RelativePanel)
	{
		auto m = c->Margin;
		m.Left = (float)newLocal.x;
		m.Top = (float)newLocal.y;
		m.Right = 0.0f;
		m.Bottom = 0.0f;
		Canvas::SetLeft(*(c), 0.0f);
		Canvas::SetTop(*(c), 0.0f);
		c->Margin = m;
		if (!preserveSize)
		{
			c->Width = static_cast<float>(newW);
			c->Height = static_cast<float>(newH);
		}
		if (auto* p = dynamic_cast<Panel*>(parent))
		{
			RefreshDesignerPanelLayout(p);
		}
		return;
	}

	Canvas::SetLeft(*(c), static_cast<float>(newLocal.x));
	Canvas::SetTop(*(c), static_cast<float>(newLocal.y));
	Canvas::SetRight(*(c), cui::layout::UnsetCanvasOffset);
	Canvas::SetBottom(*(c), cui::layout::UnsetCanvasOffset);
	if (!preserveSize)
	{
		c->Width = static_cast<float>(newW);
		c->Height = static_cast<float>(newH);
	}
	c->Margin = {};

	if (auto* p = dynamic_cast<Panel*>(parent))
	{
		RefreshDesignerPanelLayout(p);
	}
}

void DesignerCanvas::RestorePrimarySelectionByName(const std::wstring& name, bool fireEvent)
{
	RestoreSelectionByNames(name.empty() ? std::vector<std::wstring>() : std::vector<std::wstring>{ name }, name, fireEvent);
}

void DesignerCanvas::RestoreSelectionByNames(const std::vector<std::wstring>& selectionNames, const std::wstring& primaryName, bool fireEvent)
{
	if (_selectionService)
	{
		_selectionService->RestoreByNames(_designerControls, _selectedControls, _selectedControl, selectionNames, primaryName);
	}
	if (selectionNames.empty() && primaryName.empty())
	{
		if (fireEvent)
		{
			cui::framework::EventAccess::Raise(OnControlSelected, nullptr);
		}
		this->InvalidateVisual();
		return;
	}

	if (fireEvent)
	{
		cui::framework::EventAccess::Raise(OnControlSelected, _selectedControl);
	}
	this->InvalidateVisual();
}

bool DesignerCanvas::SelectAllInCurrentContainer(bool fireEvent)
{
	Control* requiredParent = GetDocumentContentRoot();
	if (!requiredParent)
		requiredParent = _clientSurface
			? static_cast<Control*>(_clientSurface)
			: static_cast<Control*>(_designSurface);
	if (_selectedControl && _selectedControl->ControlInstance
		&& _selectedControl->ControlInstance->GetVisualParent())
		requiredParent = _selectedControl->ControlInstance->GetVisualParent();

	ClearSelection();
	std::shared_ptr<DesignerControl> first;
	for (const auto& control : _designerControls)
	{
		if (!control || !control->ControlInstance
			|| control->Type == UIClass::UI_TabItem
			|| control->ControlInstance->GetVisualParent() != requiredParent)
			continue;
		if (!first)
		{
			first = control;
			AddToSelection(control, true, false);
		}
		else
		{
			AddToSelection(control, false, false);
		}
	}
	if (fireEvent) cui::framework::EventAccess::Raise(OnControlSelected, _selectedControl);
	this->InvalidateVisual();
	return !_selectedControls.empty();
}

void DesignerCanvas::NotifySelectionChangedThrottled()
{
	// 拖动中频繁重建 PropertyGrid 可能较重，这里做一个简单节流。
	// Designer.cpp 已订阅 OnControlSelected 并把完整选择集交给 PropertyGrid。
	DWORD now = GetTickCount();
	if (now - _lastPropSyncTick < 40) return; // ~25fps
	_lastPropSyncTick = now;
	cui::framework::EventAccess::Raise(OnControlSelected, _selectedControl);
}

void DesignerCanvas::DrawGrid()
{
	if (!_showGrid) return;
	if (!this->GetPresentationWindow()) return;
	if (!_clientSurface) return;
	auto d2d = this->GetDrawingContext();
	int gridSize = _gridSize;
	auto canvasAbs = this->GetAbsoluteLocationDip();
	auto surfRect = GetClientSurfaceRectInCanvas();
	auto surfAbsLeft = (float)(canvasAbs.x + surfRect.left);
	auto surfAbsTop = (float)(canvasAbs.y + surfRect.top);
	auto surfW = (float)(surfRect.right - surfRect.left);
	auto surfH = (float)(surfRect.bottom - surfRect.top);

	// 裁剪到设计面板
	d2d->PushDrawRect(surfAbsLeft, surfAbsTop, surfW, surfH);

	// 绘制浅色网格
	D2D1_COLOR_F gridColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);

	for (int x = 0; x < (surfRect.right - surfRect.left); x += gridSize)
	{
		d2d->DrawLine(surfAbsLeft + x, surfAbsTop, surfAbsLeft + x, surfAbsTop + surfH, gridColor, 0.5f / _viewZoom);
	}

	for (int y = 0; y < (surfRect.bottom - surfRect.top); y += gridSize)
	{
		d2d->DrawLine(surfAbsLeft, surfAbsTop + y, surfAbsLeft + surfW, surfAbsTop + y, gridColor, 0.5f / _viewZoom);
	}

	d2d->PopDrawRect();
}

void DesignerCanvas::ClearAlignmentGuides()
{
	_vGuides.clear();
	_hGuides.clear();
}

void DesignerCanvas::AddVGuide(int xCanvas)
{
	_vGuides.push_back(xCanvas);
}

void DesignerCanvas::AddHGuide(int yCanvas)
{
	_hGuides.push_back(yCanvas);
}

RECT DesignerCanvas::ApplyMoveSnap(RECT desiredRectInCanvas, Control* referenceParent)
{
	ClearAlignmentGuides();
	if (!_clientSurface) return desiredRectInCanvas;
	if (!_snapToGrid && !_snapToGuides) return desiredRectInCanvas;

	auto surfRect = GetClientSurfaceRectInCanvas();
	int dx = 0;
	int dy = 0;

	auto snapToGrid1 = [&](int value, int origin) {
		if (_gridSize <= 1) return value;
		int rel = value - origin;
		int snapped = origin + (int)std::lround((double)rel / (double)_gridSize) * _gridSize;
		if (std::abs(snapped - value) <= _snapThreshold) return snapped;
		return value;
	};

	if (_snapToGrid)
	{
		int newLeft = snapToGrid1(desiredRectInCanvas.left, surfRect.left);
		int newTop = snapToGrid1(desiredRectInCanvas.top, surfRect.top);
		dx += (newLeft - desiredRectInCanvas.left);
		dy += (newTop - desiredRectInCanvas.top);
	}

	if (_snapToGuides)
	{
		std::vector<int> refX;
		std::vector<int> refY;
		refX.reserve(_designerControls.size() * 3 + 4);
		refY.reserve(_designerControls.size() * 3 + 4);

		// design surface edges/centers
		refX.push_back(surfRect.left);
		refX.push_back(surfRect.right);
		refX.push_back((surfRect.left + surfRect.right) / 2);
		refY.push_back(surfRect.top);
		refY.push_back(surfRect.bottom);
		refY.push_back((surfRect.top + surfRect.bottom) / 2);

		for (auto& dc : _designerControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			if (dc->Type == UIClass::UI_TabItem) continue;
			Control* c = dc->ControlInstance;
			if (referenceParent && c->GetVisualParent() != referenceParent) continue;
			if (IsSelected(dc)) continue;
			auto r = GetControlRectInCanvas(c);
			refX.push_back(r.left);
			refX.push_back(r.right);
			refX.push_back((r.left + r.right) / 2);
			refY.push_back(r.top);
			refY.push_back(r.bottom);
			refY.push_back((r.top + r.bottom) / 2);
		}

		RECT moved = desiredRectInCanvas;
		moved.left += dx; moved.right += dx;
		moved.top += dy; moved.bottom += dy;

		int candX[3] = { moved.left, moved.right, (moved.left + moved.right) / 2 };
		int candY[3] = { moved.top, moved.bottom, (moved.top + moved.bottom) / 2 };

		int bestDx = 0; int bestAbsX = _snapThreshold + 1; int bestGuideX = INT_MIN;
		for (int rx : refX)
		{
			for (int cx : candX)
			{
				int delta = rx - cx;
				int absDelta = std::abs(delta);
				if (absDelta <= _snapThreshold && absDelta < bestAbsX)
				{
					bestAbsX = absDelta;
					bestDx = delta;
					bestGuideX = rx;
				}
			}
		}

		int bestDy = 0; int bestAbsY = _snapThreshold + 1; int bestGuideY = INT_MIN;
		for (int ry : refY)
		{
			for (int cy : candY)
			{
				int delta = ry - cy;
				int absDelta = std::abs(delta);
				if (absDelta <= _snapThreshold && absDelta < bestAbsY)
				{
					bestAbsY = absDelta;
					bestDy = delta;
					bestGuideY = ry;
				}
			}
		}

		dx += bestDx;
		dy += bestDy;
		if (bestGuideX != INT_MIN) AddVGuide(bestGuideX);
		if (bestGuideY != INT_MIN) AddHGuide(bestGuideY);
	}

	desiredRectInCanvas.left += dx;
	desiredRectInCanvas.right += dx;
	desiredRectInCanvas.top += dy;
	desiredRectInCanvas.bottom += dy;
	return desiredRectInCanvas;
}

RECT DesignerCanvas::ApplyResizeSnap(RECT desiredRectInCanvas, Control* referenceParent, DesignerControl::ResizeHandle handle)
{
	ClearAlignmentGuides();
	if (!_clientSurface) return desiredRectInCanvas;
	if (!_snapToGrid && !_snapToGuides) return desiredRectInCanvas;

	auto surfRect = GetClientSurfaceRectInCanvas();

	auto snapToGridEdge = [&](int value, int origin) {
		if (_gridSize <= 1) return value;
		int rel = value - origin;
		int snapped = origin + (int)std::lround((double)rel / (double)_gridSize) * _gridSize;
		if (std::abs(snapped - value) <= _snapThreshold) return snapped;
		return value;
	};

	auto collectRefX = [&]() {
		std::vector<int> refX;
		refX.reserve(_designerControls.size() * 2 + 2);
		refX.push_back(surfRect.left);
		refX.push_back(surfRect.right);
		for (auto& dc : _designerControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			if (dc->Type == UIClass::UI_TabItem) continue;
			Control* c = dc->ControlInstance;
			if (referenceParent && c->GetVisualParent() != referenceParent) continue;
			if (IsSelected(dc)) continue;
			auto r = GetControlRectInCanvas(c);
			refX.push_back(r.left);
			refX.push_back(r.right);
		}
		return refX;
	};
	auto collectRefY = [&]() {
		std::vector<int> refY;
		refY.reserve(_designerControls.size() * 2 + 2);
		refY.push_back(surfRect.top);
		refY.push_back(surfRect.bottom);
		for (auto& dc : _designerControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			if (dc->Type == UIClass::UI_TabItem) continue;
			Control* c = dc->ControlInstance;
			if (referenceParent && c->GetVisualParent() != referenceParent) continue;
			if (IsSelected(dc)) continue;
			auto r = GetControlRectInCanvas(c);
			refY.push_back(r.top);
			refY.push_back(r.bottom);
		}
		return refY;
	};

	auto hasLeft = (handle == DesignerControl::ResizeHandle::Left || handle == DesignerControl::ResizeHandle::TopLeft || handle == DesignerControl::ResizeHandle::BottomLeft);
	auto hasRight = (handle == DesignerControl::ResizeHandle::Right || handle == DesignerControl::ResizeHandle::TopRight || handle == DesignerControl::ResizeHandle::BottomRight);
	auto hasTop = (handle == DesignerControl::ResizeHandle::Top || handle == DesignerControl::ResizeHandle::TopLeft || handle == DesignerControl::ResizeHandle::TopRight);
	auto hasBottom = (handle == DesignerControl::ResizeHandle::Bottom || handle == DesignerControl::ResizeHandle::BottomLeft || handle == DesignerControl::ResizeHandle::BottomRight);

	if (_snapToGrid)
	{
		if (hasLeft) desiredRectInCanvas.left = snapToGridEdge(desiredRectInCanvas.left, surfRect.left);
		if (hasRight) desiredRectInCanvas.right = snapToGridEdge(desiredRectInCanvas.right, surfRect.left);
		if (hasTop) desiredRectInCanvas.top = snapToGridEdge(desiredRectInCanvas.top, surfRect.top);
		if (hasBottom) desiredRectInCanvas.bottom = snapToGridEdge(desiredRectInCanvas.bottom, surfRect.top);
	}

	if (_snapToGuides)
	{
		if (hasLeft || hasRight)
		{
			auto refX = collectRefX();
			int edge = hasLeft ? desiredRectInCanvas.left : desiredRectInCanvas.right;
			int bestDx = 0; int bestAbs = _snapThreshold + 1; int bestGuide = INT_MIN;
			for (int rx : refX)
			{
				int delta = rx - edge;
				int absDelta = std::abs(delta);
				if (absDelta <= _snapThreshold && absDelta < bestAbs)
				{
					bestAbs = absDelta;
					bestDx = delta;
					bestGuide = rx;
				}
			}
			if (bestGuide != INT_MIN)
			{
				if (hasLeft) desiredRectInCanvas.left += bestDx;
				else desiredRectInCanvas.right += bestDx;
				AddVGuide(bestGuide);
			}
		}
		if (hasTop || hasBottom)
		{
			auto refY = collectRefY();
			int edge = hasTop ? desiredRectInCanvas.top : desiredRectInCanvas.bottom;
			int bestDy = 0; int bestAbs = _snapThreshold + 1; int bestGuide = INT_MIN;
			for (int ry : refY)
			{
				int delta = ry - edge;
				int absDelta = std::abs(delta);
				if (absDelta <= _snapThreshold && absDelta < bestAbs)
				{
					bestAbs = absDelta;
					bestDy = delta;
					bestGuide = ry;
				}
			}
			if (bestGuide != INT_MIN)
			{
				if (hasTop) desiredRectInCanvas.top += bestDy;
				else desiredRectInCanvas.bottom += bestDy;
				AddHGuide(bestGuide);
			}
		}
	}

	return desiredRectInCanvas;
}

RECT DesignerCanvas::GetDesignSurfaceRectInCanvas() const
{
	if (!_designSurface) return RECT{ 0,0,0,0 };
	const auto width = static_cast<LONG>(std::lround(
		(std::max)(0.0f, _designedWindowSize.width)));
	const auto height = static_cast<LONG>(std::lround(
		(std::max)(0.0f, _designedWindowSize.height)));
	return RECT{
		_designSurfaceOrigin.x,
		_designSurfaceOrigin.y,
		_designSurfaceOrigin.x + width,
		_designSurfaceOrigin.y + height };
}

RECT DesignerCanvas::GetClientSurfaceRectInCanvas() const
{
	if (!_designSurface || !_clientSurface)
		return GetDesignSurfaceRectInCanvas();
	const auto window = GetDesignSurfaceRectInCanvas();
	const auto clientTop = (std::clamp)(
		static_cast<LONG>(DesignedClientTop()),
		0L, window.bottom - window.top);
	return RECT{
		window.left,
		window.top + clientTop,
		window.right,
		window.bottom };
}

RECT DesignerCanvas::GetViewportRectInCanvas() const
{
	const auto first = ViewToCanvasPoint({ 0, 0 });
	const auto viewportSize = GetActualSizeDip();
	const auto second = ViewToCanvasPoint({
		static_cast<LONG>(std::lround(viewportSize.width)),
		static_cast<LONG>(std::lround(viewportSize.height)) });
	return RECT{
		(std::min)(first.x, second.x),
		(std::min)(first.y, second.y),
		(std::max)(first.x, second.x),
		(std::max)(first.y, second.y)
	};
}

void DesignerCanvas::UpdateClientSurfaceLayout()
{
	if (!_designSurface || !_clientSurface) return;
	if (!_defaultContentRoot)
	{
		const auto root = GetDocumentContentRootRecord();
		if (root && root->Name == L"contentRoot"
			&& root->XamlType.NamespaceUri
				== CuiRuntime::XamlRuntimeSchema::CuiNamespace
			&& root->XamlType.LocalName == L"Canvas")
			_defaultContentRoot = root->ControlInstance;
	}
	int top = DesignedClientTop();
	const auto designSize = _designSurface->GetActualSizeDip();
	int h = static_cast<int>(std::lround(designSize.height)) - top;
	if (h < 0) h = 0;
	_clientSurface->Width = cui::layout::Length::Fixed(designSize.width);
	_clientSurface->Height = cui::layout::Length::Fixed(
		static_cast<float>(h));
	_clientSurface->Arrange(cui::core::Rect{
		0.0f, static_cast<float>(top), designSize.width, static_cast<float>(h) });
	Canvas::SetLeft(*(_clientSurface), 0.0f);
	Canvas::SetTop(*(_clientSurface), static_cast<float>(top));
	if (auto* p = dynamic_cast<Panel*>(_clientSurface))
	{
		RefreshDesignerPanelLayout(p);
	}
	// WPF Window.ArrangeOverride gives its single visual child the complete
	// client bounds.  The design chrome must do the same for every authored root,
	// not only for the synthetic default Canvas; otherwise an Auto-sized Grid can
	// retain its former desired width when the authored Window is wider.
	const auto root = GetDocumentContentRootRecord();
	if (root && root->ControlInstance
		&& root->ControlInstance->GetVisualParent() == _clientSurface)
	{
		root->ControlInstance->Arrange(cui::core::Rect{
			{}, _clientSurface->GetActualSizeDip() });
		RefreshDesignerPanelLayout(root->ControlInstance);
	}
}

void DesignerCanvas::UpdateContentPreviewLayout()
{
	if (!_clientSurface) return;
	if (_designSurface) _designSurface->UpdateLayout();
	_clientSurface->UpdateLayout();
}

bool DesignerCanvas::IsPointInDesignSurface(POINT ptCanvas) const
{
	auto r = GetClientSurfaceRectInCanvas();
	return ptCanvas.x >= r.left && ptCanvas.x <= r.right && ptCanvas.y >= r.top && ptCanvas.y <= r.bottom;
}

bool DesignerCanvas::IsPointInDesignedWindow(POINT ptCanvas) const
{
	auto r = GetDesignSurfaceRectInCanvas();
	return ptCanvas.x >= r.left && ptCanvas.x <= r.right
		&& ptCanvas.y >= r.top && ptCanvas.y <= r.bottom;
}

RECT DesignerCanvas::ClampRectToBounds(RECT r, const RECT& bounds, bool keepSize) const
{
	int w = r.right - r.left;
	int h = r.bottom - r.top;
	if (keepSize)
	{
		if (r.left < bounds.left) { r.left = bounds.left; r.right = r.left + w; }
		if (r.top < bounds.top) { r.top = bounds.top; r.bottom = r.top + h; }
		if (r.right > bounds.right) { r.right = bounds.right; r.left = r.right - w; }
		if (r.bottom > bounds.bottom) { r.bottom = bounds.bottom; r.top = r.bottom - h; }
	}
	else
	{
		if (r.left < bounds.left) r.left = bounds.left;
		if (r.top < bounds.top) r.top = bounds.top;
		if (r.right > bounds.right) r.right = bounds.right;
		if (r.bottom > bounds.bottom) r.bottom = bounds.bottom;
	}
	// 防御：避免反转
	if (r.right < r.left) r.right = r.left;
	if (r.bottom < r.top) r.bottom = r.top;
	return r;
}

bool DesignerCanvas::TryHandleTabHeaderClick(POINT ptCanvas)
{
	// DesignerCanvas 自己处理选中/拖拽，导致 TabControl 无法收到点击切页。
	// 这里在画布层模拟 TabControl 标题栏点击，设置 SelectedIndex。
	TabControl* bestTc = nullptr;
	std::shared_ptr<DesignerControl> bestDc = nullptr;
	int bestArea = INT_MAX;

	for (auto& dc : _designerControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		if (dc->Type != UIClass::UI_TabControl) continue;
		auto* tabControl = dynamic_cast<TabControl*>(dc->ControlInstance);
		if (!tabControl) continue;
		auto r = GetControlRectInCanvas(tabControl);
		if (ptCanvas.x < r.left || ptCanvas.x > r.right || ptCanvas.y < r.top || ptCanvas.y > r.bottom) continue;
		int area = (r.right - r.left) * (r.bottom - r.top);
		if (area < bestArea)
		{
			bestArea = area;
			bestTc = tabControl;
			bestDc = dc;
		}
	}

	if (!bestTc || !bestDc) return false;

	auto r = GetControlRectInCanvas(bestTc);
	int localX = ptCanvas.x - r.left;
	int localY = ptCanvas.y - r.top;
	int titleIndex = -1;
	if (!bestTc->TryGetTabHeaderIndexAt(
		localX, localY, titleIndex)) return false;

	bestTc->SelectItem(titleIndex);
	bestTc->InvalidateVisual();

	// 切页后：清除之前页上选中的控件，避免选框残留；并把 TabControl 设为当前选中。
	// 即使 titleIndex 未变化，点击标题栏也视为在操作 TabControl。
	ClearSelection();
	AddToSelection(bestDc, true, true);
	return true;
}

bool DesignerCanvas::RevealControlInDesigner(Control* control)
{
	if (!control) return false;
	bool changed = false;
	for (auto* current = control; current && current != this;
		current = current->GetVisualParent())
	{
		auto* page = dynamic_cast<TabItem*>(current);
		if (!page) continue;
		auto* tab = dynamic_cast<TabControl*>(page->GetLogicalParent());
		if (!tab) continue;
		const int index = tab->IndexOfItem(page);
		if (index < 0) continue;
		if (tab->SelectedIndex != index)
		{
			(void)tab->SelectItem(index);
			changed = true;
		}
		tab->InvalidateVisual();
	}
	if (changed) this->InvalidateVisual();
	return changed;
}

void DesignerCanvas::SetDesignedWindowSize(cui::core::Size value)
{
	value.width = (std::max)(50.0f, value.width);
	value.height = (std::max)(50.0f, value.height);
	_designedWindowSize = value;
	if (!_applyingDesignedWindowNode)
	{
		(void)DesignerPropertyCatalog::ApplyNodeValue(
			_designedWindowNode, L"Width",
			{ DesignerStyleValueKind::Int,
				std::to_wstring(static_cast<int>(
					std::lround(value.width))) });
		(void)DesignerPropertyCatalog::ApplyNodeValue(
			_designedWindowNode, L"Height",
			{ DesignerStyleValueKind::Int,
				std::to_wstring(static_cast<int>(
					std::lround(value.height))) });
	}
	if (_designSurface)
	{
		_designSurface->Width = value.width;
		_designSurface->Height = value.height;
		_designSurface->Arrange(cui::core::Rect{
			static_cast<float>(_designSurfaceOrigin.x),
			static_cast<float>(_designSurfaceOrigin.y),
			value.width, value.height });
		if (auto* p = dynamic_cast<Panel*>(_designSurface))
		{
			RefreshDesignerPanelLayout(p);
		}
	}
	UpdateClientSurfaceLayout();
	// 尺寸变化后：尽量把现有控件也约束到设计面板内
	for (auto& dc : _designerControls)
	{
		if (dc && dc->ControlInstance)
			ClampControlToDesignSurface(dc->ControlInstance);
	}
	if (_fitToViewport) RecalculateFitView(true);
	this->InvalidateVisual();
}

void DesignerCanvas::ClampControlToDesignSurface(Control* c)
{
	if (!c) return;
	if (_clientSurface && c->GetVisualParent() == _clientSurface)
	{
		auto rCanvas = GetControlRectInCanvas(c);
		auto bounds = GetClientSurfaceRectInCanvas();
		RECT clamped = ClampRectToBounds(rCanvas, bounds, true);
		POINT newTopLeftCanvas{ clamped.left, clamped.top };
		POINT newLocal = CanvasToChildLayoutPoint(newTopLeftCanvas, _clientSurface);
		Canvas::SetLeft(*(c), static_cast<float>(newLocal.x));
		Canvas::SetTop(*(c), static_cast<float>(newLocal.y));
		return;
	}
	if (_designSurface && c->GetVisualParent() == _designSurface)
	{
		auto rCanvas = GetControlRectInCanvas(c);
		auto bounds = GetDesignSurfaceRectInCanvas();
		RECT clamped = ClampRectToBounds(rCanvas, bounds, true);
		POINT newTopLeftCanvas{ clamped.left, clamped.top };
		POINT newLocal = CanvasToChildLayoutPoint(newTopLeftCanvas, _designSurface);
		Canvas::SetLeft(*(c), static_cast<float>(newLocal.x));
		Canvas::SetTop(*(c), static_cast<float>(newLocal.y));
	}
}

void DesignerCanvas::DrawSelectionHandles(std::shared_ptr<DesignerControl> dc)
{
	if (!dc || !dc->ControlInstance || !this->GetPresentationWindow()) return;

	auto d2d = this->GetDrawingContext();
	auto absoluteLocation = this->GetAbsoluteLocationDip();
	auto rect = GetControlRectInCanvas(dc->ControlInstance);
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;

	// 绘制选中边框
	float x = (float)(absoluteLocation.x + rect.left);
	float y = (float)(absoluteLocation.y + rect.top);
	d2d->DrawRect(x, y, (float)w, (float)h, Colors::DodgerBlue, 2.0f / _viewZoom);
	if (dc->IsLocked)
	{
		const float scale = 1.0f / (std::max)(_viewZoom, 0.01f);
		const float bodyWidth = 11.0f * scale;
		const float bodyHeight = 8.0f * scale;
		const float bodyX = x + static_cast<float>(w) - bodyWidth * 0.5f;
		const float bodyY = y - bodyHeight * 0.2f;
		const float stroke = 2.0f * scale;
		d2d->FillRect(
			bodyX, bodyY, bodyWidth, bodyHeight, Colors::DarkOrange);
		const float shackleLeft = bodyX + 2.5f * scale;
		const float shackleRight = bodyX + bodyWidth - 2.5f * scale;
		const float shackleTop = bodyY - 5.0f * scale;
		d2d->DrawLine(
			shackleLeft, bodyY, shackleLeft, shackleTop,
			Colors::DarkOrange, stroke);
		d2d->DrawLine(
			shackleLeft, shackleTop, shackleRight, shackleTop,
			Colors::DarkOrange, stroke);
		d2d->DrawLine(
			shackleRight, shackleTop, shackleRight, bodyY,
			Colors::DarkOrange, stroke);
		return;
	}

	// 绘制8个调整手柄
	auto rects = GetHandleRectsFromRect(
		rect, GetSelectionHandleSizeInCanvas());

	for (const auto& r : rects)
	{
		float hx = (float)(absoluteLocation.x + r.left);
		float hy = (float)(absoluteLocation.y + r.top);
		float hw = (float)(r.right - r.left);
		float hh = (float)(r.bottom - r.top);
		d2d->FillRect(hx, hy, hw, hh, Colors::White);
		d2d->DrawRect(hx, hy, hw, hh, Colors::DodgerBlue, 1.0f / _viewZoom);
	}
}

std::shared_ptr<DesignerControl> DesignerCanvas::HitTestControl(
	POINT pt, bool preferParentContainer)
{
	auto findDesigner = [&](Control* control)
		-> std::shared_ptr<DesignerControl>
	{
		for (auto it = _designerControls.rbegin();
			it != _designerControls.rend(); ++it)
			if (*it && (*it)->ControlInstance == control) return *it;
		return nullptr;
	};
	const auto viewPoint = CanvasToViewPoint(pt);
	const auto canvasAbsolute = GetAbsoluteLocationDip();
	const POINT renderPoint{
		static_cast<LONG>(std::lround(canvasAbsolute.x + viewPoint.x)),
		static_cast<LONG>(std::lround(canvasAbsolute.y + viewPoint.y)) };

	// 占位绘制在运行时子树之上，所以自身不可见目标也应先于
	// 其可见父容器命中。
	for (auto it = _designerControls.rbegin();
		it != _designerControls.rend(); ++it)
	{
		const auto& dc = *it;
		if (!dc || dc->Type == UIClass::UI_TabItem
			|| !dc->ControlInstance
			|| dc->ControlInstance->Visibility == Visibility::Visible
			|| !HasVisibleDesignerAncestors(dc->ControlInstance))
			continue;
		D2D1_POINT_2F local{};
		if (!dc->ControlInstance->TryTransformRenderPointToLocal(
			D2D1::Point2F(
				static_cast<float>(renderPoint.x),
				static_cast<float>(renderPoint.y)), local)
			|| !dc->ControlInstance->IsRenderPointInsideClip(D2D1::Point2F(
				static_cast<float>(renderPoint.x),
				static_cast<float>(renderPoint.y)))
			|| !dc->ControlInstance->ContainsPoint(
				static_cast<int>(std::floor(local.x)),
				static_cast<int>(std::floor(local.y))))
			continue;
		if (preferParentContainer)
		{
			for (auto* parent = dc->ControlInstance->GetVisualParent();
				parent && parent != this; parent = parent->GetVisualParent())
				if (auto designerParent = findDesigner(parent))
					return designerParent;
		}
		return dc;
	}

	return HitTestService::HitTestControl(
		this, _designerControls, renderPoint, preferParentContainer);
}

bool DesignerCanvas::HasVisibleDesignerAncestors(
	Control* control) const noexcept
{
	if (!control) return false;
	for (auto* ancestor = control->GetVisualParent();
		ancestor && ancestor != this; ancestor = ancestor->GetVisualParent())
		if (ancestor->Visibility != Visibility::Visible) return false;
	return true;
}

RECT DesignerCanvas::GetControlRectInCanvas(Control* c)
{
	RECT r{ 0,0,0,0 };
	if (!c) return r;
	// 设计器操作的是布局槽，而不是 RenderTransform 后的外接矩形。
	// 否则旋转/缩放控件每移动一次，外接矩形会被写回 Size 并再次变换，
	// 导致选区持续膨胀。视图缩放由 DesignerCanvas 的绘制变换统一处理。
	const auto absolute = c->GetAbsoluteRectDip();
	const auto canvasAbs = GetAbsoluteLocationDip();
	r.left = static_cast<LONG>(std::lround(absolute.Left() - canvasAbs.x));
	r.top = static_cast<LONG>(std::lround(absolute.Top() - canvasAbs.y));
	r.right = r.left + static_cast<LONG>(std::lround(absolute.width));
	r.bottom = r.top + static_cast<LONG>(std::lround(absolute.height));
	return r;
}

std::vector<RECT> DesignerCanvas::GetHandleRectsFromRect(const RECT& r, int handleSize)
{
	std::vector<RECT> rects;
	int half = handleSize / 2;
	int cx = (r.left + r.right) / 2;
	int cy = (r.top + r.bottom) / 2;

	// TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left
	rects.push_back({ r.left - half, r.top - half, r.left + half, r.top + half });
	rects.push_back({ cx - half, r.top - half, cx + half, r.top + half });
	rects.push_back({ r.right - half, r.top - half, r.right + half, r.top + half });
	rects.push_back({ r.right - half, cy - half, r.right + half, cy + half });
	rects.push_back({ r.right - half, r.bottom - half, r.right + half, r.bottom + half });
	rects.push_back({ cx - half, r.bottom - half, cx + half, r.bottom + half });
	rects.push_back({ r.left - half, r.bottom - half, r.left + half, r.bottom + half });
	rects.push_back({ r.left - half, cy - half, r.left + half, cy + half });
	return rects;
}

DesignerControl::ResizeHandle DesignerCanvas::HitTestHandleFromRect(const RECT& r, POINT pt, int handleSize)
{
	auto rects = GetHandleRectsFromRect(r, handleSize);
	for (size_t i = 0; i < rects.size(); i++)
	{
		auto& hr = rects[i];
		if (pt.x >= hr.left && pt.x <= hr.right && pt.y >= hr.top && pt.y <= hr.bottom)
			return (DesignerControl::ResizeHandle)(i + 1);
	}
	return DesignerControl::ResizeHandle::None;
}

bool DesignerCanvas::IsDescendantOf(Control* ancestor, Control* node)
{
	return HitTestService::IsDescendantOf(ancestor, node);
}

void DesignerCanvas::RemoveDesignerControlsInSubtree(Control* root)
{
	if (!root) return;

	auto isInSubtree = [this, root](Control* node) -> bool {
		if (!node) return false;
		if (node == root) return true;
		return IsDescendantOf(root, node);
	};

	bool selectionRemoved = false;
	for (auto& selectedControl : _selectedControls)
	{
		if (selectedControl && selectedControl->ControlInstance && isInSubtree(selectedControl->ControlInstance))
		{
			selectionRemoved = true;
			break;
		}
	}

	_designerControls.erase(
		std::remove_if(_designerControls.begin(), _designerControls.end(),
			[&](const std::shared_ptr<DesignerControl>& dc) {
				return dc && dc->ControlInstance && isInSubtree(dc->ControlInstance);
			}),
		_designerControls.end());

	if (selectionRemoved)
	{
		ClearSelection();
		cui::framework::EventAccess::Raise(OnControlSelected, nullptr);
	}
}

bool DesignerCanvas::IsContainerControl(Control* c)
{
	return HitTestService::IsContainerControl(c);
}

Control* DesignerCanvas::NormalizeContainerForDrop(
	Control* container, UIClass childType)
{
	return LayoutBridge::NormalizeContainerForDrop(container, childType);
}

POINT DesignerCanvas::CanvasToContainerPoint(POINT ptCanvas, Control* container)
{
	if (!container) return ptCanvas;
	auto canvasAbs = this->GetAbsoluteLocationDip();
	auto containerLocation = container->GetAbsoluteLocationDip();
	POINT containerPoint{
		static_cast<LONG>(std::lround(
			static_cast<float>(ptCanvas.x)
			- (containerLocation.x - canvasAbs.x))),
		static_cast<LONG>(std::lround(
			static_cast<float>(ptCanvas.y)
			- (containerLocation.y - canvasAbs.y))) };
	// TabItem content 的坐标已经是 page 本地坐标，不需要额外处理
	return containerPoint;
}

POINT DesignerCanvas::CanvasToChildLayoutPoint(
	POINT ptCanvas, Control* container)
{
	auto point = CanvasToContainerPoint(ptCanvas, container);
	if (!container) return point;
	const auto origin = container->GetVisualChildrenLayoutOriginDip();
	const auto renderOffset = container->GetVisualChildrenRenderOffset();
	point.x -= static_cast<LONG>(std::lround(origin.x + renderOffset.x));
	point.y -= static_cast<LONG>(std::lround(origin.y + renderOffset.y));
	return point;
}

Control* DesignerCanvas::FindBestContainerAtPoint(POINT ptCanvas, Control* ignore)
{
	const auto viewPoint = CanvasToViewPoint(ptCanvas);
	const auto canvasAbsolute = GetAbsoluteLocationDip();
	const auto renderPoint = D2D1::Point2F(
		canvasAbsolute.x + viewPoint.x,
		canvasAbsolute.y + viewPoint.y);
	return HitTestService::FindBestContainerAtPoint(
		_designerControls, ptCanvas, ignore,
		[this](Control* control) { return GetControlRectInCanvas(control); },
		[renderPoint](Control* control)
		{
			if (!control || !control->IsRenderPointInsideClip(renderPoint))
				return false;
			D2D1_POINT_2F local{};
			return control->TryTransformRenderPointToLocal(renderPoint, local)
				&& control->ContainsPoint(
					static_cast<int>(std::floor(local.x)),
					static_cast<int>(std::floor(local.y)));
		});
}

void DesignerCanvas::DeleteVisualChildRecursive(Control* c)
{
	if (!c) return;
	if (c == _defaultContentRoot) _defaultContentRoot = nullptr;
	// Control 析构会递归释放整棵子树；优先通过父容器完成显式所有权销毁。
	if (c->GetVisualParent() && c->GetVisualParent()->DeleteVisualChild(c))
		return;
	delete c;
}

void DesignerCanvas::TryReparentSelectedAfterDrag()
{
	if (!_selectedControl || !_selectedControl->ControlInstance) return;
	auto* moving = _selectedControl->ControlInstance;
	if (!_designSurface || !_clientSurface) return;

	// ToolBar 现在可承载任意控件；LayoutBridge 仍负责统一入口校验。
	auto movingType = moving->Type();

	auto r = GetControlRectInCanvas(moving);
	POINT center{ (r.left + r.right) / 2, (r.top + r.bottom) / 2 };

	Control* rawContainer = FindBestContainerAtPoint(center, moving);
	Control* container = NormalizeContainerForDrop(rawContainer, movingType);
	if (!container)
	{
		auto* contentRoot = GetDocumentContentRoot();
		if (contentRoot && contentRoot != moving
			&& LayoutBridge::CanAcceptChild(contentRoot, movingType))
			container = contentRoot;
		else return;
	}

	bool containerChanged = (_selectedControl->DesignerParent != container);

	// 防止把自己塞进自己的子树
	if (container == moving || IsDescendantOf(moving, container))
		return;

	// 计算保持视觉不动的目标位置
	POINT canvasTopLeft{ r.left, r.top };
	POINT dropLocalToContainer = CanvasToContainerPoint(center, container);
	Control* runtimeHost = container;
	std::wstring destinationContentProperty;
	const auto containerRecord = std::find_if(
		_designerControls.begin(), _designerControls.end(),
		[container](const auto& candidate)
		{
			return candidate && candidate->ControlInstance == container;
		});
	if (containerRecord != _designerControls.end()
		&& !(*containerRecord)->ComponentType.Empty())
	{
		auto content = (*containerRecord)->ComponentContentProperties.end();
		if (!containerChanged
			&& !_selectedControl->ComponentContentProperty.empty())
		{
			content = std::find_if(
				(*containerRecord)->ComponentContentProperties.begin(),
				(*containerRecord)->ComponentContentProperties.end(),
				[this](const auto& property)
				{
					return property.Name
						== _selectedControl->ComponentContentProperty;
				});
		}
		if (content == (*containerRecord)->ComponentContentProperties.end())
			content = std::find_if(
				(*containerRecord)->ComponentContentProperties.begin(),
				(*containerRecord)->ComponentContentProperties.end(),
				[](const auto& property) { return property.IsDefault; });
		if (content == (*containerRecord)->ComponentContentProperties.end())
			return;
		const auto presenter = (*containerRecord)->ComponentContentPresenters.find(
			content->Name);
		if (presenter == (*containerRecord)->ComponentContentPresenters.end()
			|| !presenter->second) return;
		if (content->Cardinality ==
			DesignerComponentContentCardinality::Single)
		{
			const auto occupied = std::any_of(
				_designerControls.begin(), _designerControls.end(),
				[&](const auto& candidate)
				{
					return candidate && candidate != _selectedControl
						&& candidate->DesignerParent == container
						&& candidate->ComponentContentProperty == content->Name;
				});
			if (occupied) return;
		}
		destinationContentProperty = content->Name;
		runtimeHost = presenter->second;
	}
	// TabControl content has already been normalized to TabItem. Component
	// content is validated against the selected template presenter above.
	if (!LayoutBridge::CanAcceptChild(runtimeHost, movingType))
		return;
	POINT newLocal = CanvasToChildLayoutPoint(canvasTopLeft, runtimeHost);
	POINT dropLocalCenter = CanvasToContainerPoint(center, runtimeHost);
	auto authoredOwnerOf = [](Control* control) -> ItemsControl*
	{
		if (!control) return nullptr;
		auto* items = dynamic_cast<ItemsControl*>(control->GetLogicalParent());
		if (!items || control->GetVisualParent()
			!= cui::framework::TemplateAccess::GetItemsHost(*items))
			return nullptr;
		for (size_t index = 0; index < items->AuthoredItemCount(); ++index)
			if (items->GetAuthoredItem(index) == control) return items;
		return nullptr;
	};
	auto* movingItemsOwner = authoredOwnerOf(moving);
	auto* destinationItemsOwner = dynamic_cast<ItemsControl*>(runtimeHost);
	bool runtimeHostChanged = movingItemsOwner
		? movingItemsOwner != destinationItemsOwner
		: moving->GetVisualParent() != runtimeHost;

	if (containerChanged || runtimeHostChanged)
	{
		std::unique_ptr<Control> movingOwner;
		if (movingItemsOwner)
		{
			movingOwner = movingItemsOwner->DetachItemControl(moving);
			if (!movingOwner) return;
		}
		else if (moving->GetVisualParent())
		{
			movingOwner = moving->GetVisualParent()->DetachVisualChild(moving);
			if (!movingOwner) return;
		}

		// 加入新容器
		if (movingOwner)
			LayoutBridge::AttachChild(runtimeHost, std::move(movingOwner));
		else
			LayoutBridge::AttachChild(runtimeHost, moving);

		_selectedControl->DesignerParent = container;
	}
	_selectedControl->ComponentContentProperty =
		std::move(destinationContentProperty);

	LayoutBridge::ApplyExistingChildLayout(runtimeHost, moving, newLocal, dropLocalCenter, containerChanged || runtimeHostChanged, r, [this, &moving](const RECT& rectInCanvas) {
		ApplyRectToControl(moving, rectInCanvas);
	});
	LayoutBridge::RefreshContainerLayout(runtimeHost);
	this->InvalidateVisual();
}

CursorKind DesignerCanvas::GetResizeCursor(DesignerControl::ResizeHandle handle)
{
	switch (handle)
	{
	case DesignerControl::ResizeHandle::TopLeft:
	case DesignerControl::ResizeHandle::BottomRight:
		return CursorKind::SizeNWSE;
	case DesignerControl::ResizeHandle::TopRight:
	case DesignerControl::ResizeHandle::BottomLeft:
		return CursorKind::SizeNESW;
	case DesignerControl::ResizeHandle::Top:
	case DesignerControl::ResizeHandle::Bottom:
		return CursorKind::SizeNS;
	case DesignerControl::ResizeHandle::Left:
	case DesignerControl::ResizeHandle::Right:
		return CursorKind::SizeWE;
	default:
		return CursorKind::Arrow;
	}
}

bool DesignerCanvas::ProcessInput(const InputReport& input)
{
	if (!this->IsEnabled) return false;

	// InputReport coordinates are viewport-local; designer operations remain in
	// the unscaled authored-canvas coordinate space.
	const POINT viewMousePos = { input.X, input.Y };
	const POINT mousePos = ViewToCanvasPoint(viewMousePos);
	const Key key = input.Key;

	if (input.Kind == InputReportKind::KeyDown && key == Key::Space)
	{
		_spacePanModifierDown = true;
		return true;
	}
	if (input.Kind == InputReportKind::KeyUp && key == Key::Space)
	{
		_spacePanModifierDown = false;
		return true;
	}
	if (input.Kind == InputReportKind::FocusLost)
	{
		_spacePanModifierDown = false;
	}

	if (input.Kind == InputReportKind::PointerDown
		&& input.ChangedButton == MouseButton::Middle)
	{
		if (_isBoxSelecting || _isDragging || _isResizing
			|| HasActiveDeltaInteraction()
			|| !_activeInteractionTransaction.empty())
		{
			(void)CancelActivePointerInteraction(
				L"平移画布前已取消当前控件交互。");
		}
		BeginViewPan(viewMousePos, false);
		return true;
	}
	if (input.Kind == InputReportKind::PointerUp
		&& input.ChangedButton == MouseButton::Middle)
	{
		if (_isPanning && !_panStartedWithLeftButton)
		{
			EndViewPan();
			return true;
		}
	}
	if (input.Kind == InputReportKind::PointerUp
		&& input.ChangedButton == MouseButton::Right)
	{
		if (_isBoxSelecting || _isDragging || _isResizing
			|| HasActiveDeltaInteraction()
			|| !_activeInteractionTransaction.empty())
		{
			(void)CancelActivePointerInteraction(
				L"右键菜单中断了画布预览，修改已回滚。");
		}
		_controlToAdd.reset();
		_interactionCursor = CursorKind::Arrow;
		if (this->GetPresentationWindow())
			this->GetPresentationWindow()->SetKeyboardFocus(this, true);

		auto hitControl = HitTestControl(mousePos, input.HasModifier(ModifierKeys::Alt));
		if (hitControl)
		{
			if (!IsSelected(hitControl))
			{
				ClearSelection();
				AddToSelection(hitControl, true, true);
			}
			else
			{
				SetPrimarySelection(hitControl, true);
			}
		}
		else if (IsPointInDesignedWindow(mousePos))
		{
			ClearSelection();
			cui::framework::EventAccess::Raise(OnControlSelected, nullptr);
		}
		else
		{
			return false;
		}

		cui::framework::EventAccess::Raise(OnContextMenuRequested, DesignerCanvasContextMenuEventArgs{
			viewMousePos, !_selectedControls.empty() });
		this->InvalidateVisual();
		return true;
	}

	switch (input.Kind)
	{
	case InputReportKind::MouseWheel:
	{
		if (!input.HasModifier(ModifierKeys::Control)) break;
		const int delta = input.WheelDelta;
		if (delta != 0)
		{
			const float steps = static_cast<float>(delta)
				/ static_cast<float>(InputReport::WheelDeltaUnit);
			SetViewZoom(_viewZoom * std::pow(
				DesignerViewZoomStep, steps), viewMousePos);
		}
		return true;
	}
	case InputReportKind::KeyDown:
	{
		const bool shiftDown = input.HasModifier(ModifierKeys::Shift);
		const bool controlDown = input.HasModifier(ModifierKeys::Control);
		if (key == Key::Apps || (key == Key::F10 && shiftDown))
		{
			POINT position{};
			if (_selectedControl && _selectedControl->ControlInstance)
			{
				const auto rect = GetControlRectInCanvas(
					_selectedControl->ControlInstance);
				position = POINT{ rect.left + 8, rect.bottom + 4 };
			}
			else
			{
				const auto rect = GetClientSurfaceRectInCanvas();
				position = POINT{
					(rect.left + rect.right) / 2,
					(rect.top + rect.bottom) / 2 };
			}
			cui::framework::EventAccess::Raise(OnContextMenuRequested, DesignerCanvasContextMenuEventArgs{
				CanvasToViewPoint(position), !_selectedControls.empty() });
			return true;
		}
		if (_tabOrderMode)
		{
			if (key == Key::Escape)
			{
				(void)SetTabOrderMode(false);
				return true;
			}
			const bool allowedViewOrHistoryKey = controlDown
				&& (key == Key::Z || key == Key::Y
					|| key == Key::D0 || key == Key::NumPad0
					|| key == Key::D1 || key == Key::NumPad1
					|| key == Key::OemPlus || key == Key::Add
					|| key == Key::OemMinus || key == Key::Subtract);
			if (!allowedViewOrHistoryKey) return true;
		}
		if (controlDown)
		{
			if (key == Key::D0 || key == Key::NumPad0)
			{
				FitDesignSurfaceToViewport();
				return true;
			}
			if (key == Key::D1 || key == Key::NumPad1)
			{
				ResetView();
				return true;
			}
			if (key == Key::OemPlus || key == Key::Add)
			{
				ZoomIn();
				return true;
			}
			if (key == Key::OemMinus || key == Key::Subtract)
			{
				ZoomOut();
				return true;
			}
			if (key == Key::C)
			{
				(void)CopySelectedControls();
				return true;
			}
			if (key == Key::X)
			{
				(void)CutSelectedControls();
				return true;
			}
			if (key == Key::V)
			{
				if (shiftDown)
					(void)PasteControlsFromClipboardInPlace();
				else (void)PasteControlsFromClipboard();
				return true;
			}
			if (key == Key::D)
			{
				(void)DuplicateSelectedControls();
				return true;
			}
			if (key == Key::L)
			{
				(void)SetSelectedControlsLocked(
					!AreAllSelectedControlsLocked());
				return true;
			}
			if (key == Key::OemCloseBrackets)
			{
				(void)ArrangeSelection(shiftDown
					? DesignerSelectionArrangeAction::BringToFront
					: DesignerSelectionArrangeAction::BringForward);
				return true;
			}
			if (key == Key::OemOpenBrackets)
			{
				(void)ArrangeSelection(shiftDown
					? DesignerSelectionArrangeAction::SendToBack
					: DesignerSelectionArrangeAction::SendBackward);
				return true;
			}
			if (key == Key::Z && !shiftDown)
			{
				(void)UndoCommand();
				return true;
			}
			else if (key == Key::Y || (key == Key::Z && shiftDown))
			{
				(void)RedoCommand();
				return true;
			}
		}

		// 设计器模式下，把键盘操作收敛到画布
		if (key == Key::Escape)
		{
			if (_isPanning)
			{
				EndViewPan();
				return true;
			}
			if (_isBoxSelecting || _isDragging || _isResizing
				|| HasActiveDeltaInteraction()
				|| !_activeInteractionTransaction.empty())
			{
				(void)CancelActivePointerInteraction(
					L"已通过 Escape 取消画布交互。");
				return true;
			}
			// 取消“点击添加控件”模式
			_controlToAdd.reset();
			_interactionCursor = CursorKind::Arrow;
			return true;
		}

		if (key == Key::Delete || key == Key::Back)
		{
			DeleteSelectedControl();
			this->InvalidateVisual();
			return true;
		}

		// Ctrl+A：全选当前容器
		if (key == Key::A && controlDown)
		{
			(void)SelectAllInCurrentContainer(true);
			return true;
		}

		if (_selectedControls.empty() || !_selectedControl || !_selectedControl->ControlInstance)
		{
			break;
		}

		const int step = shiftDown ? 10 : 1;
		int dx = 0, dy = 0;

		switch (key)
		{
		case Key::Left:
			dx = -step;
			break;
		case Key::Right:
			dx = step;
			break;
		case Key::Up:
			dy = -step;
			break;
		case Key::Down:
			dy = step;
			break;
		default:
			break;
		}
		if (dx != 0 || dy != 0)
		{
			(void)NudgeSelectionBy(dx, dy);
			return true;
		}
		break;
	}
	case InputReportKind::PointerDoubleClick:
	{
		if (input.ChangedButton != MouseButton::Left) break;
		if (_tabOrderMode) return true;
		if (_controlToAdd) return true;
		auto hitControl = HitTestControl(mousePos, input.HasModifier(ModifierKeys::Alt));
		if (hitControl)
		{
			if (!IsSelected(hitControl) || _selectedControls.size() != 1)
			{
				ClearSelection();
				AddToSelection(hitControl, true, true);
			}
			else
			{
				SetPrimarySelection(hitControl, true);
			}
			cui::framework::EventAccess::Raise(OnDefaultEventRequested, hitControl);
			this->InvalidateVisual();
			return true;
		}
		if (IsPointInDesignedWindow(mousePos))
		{
			ClearSelection();
			cui::framework::EventAccess::Raise(OnControlSelected, nullptr);
			cui::framework::EventAccess::Raise(OnDefaultEventRequested, nullptr);
			this->InvalidateVisual();
			return true;
		}
		break;
	}
	case InputReportKind::PointerDown:
	{
		if (input.ChangedButton != MouseButton::Left) break;
		if (_spacePanModifierDown)
		{
			BeginViewPan(viewMousePos, true);
			return true;
		}
		// 确保键盘消息会转发到画布（Window 优先发给键盘焦点元素）
		if (this->GetPresentationWindow())
		{
			this->GetPresentationWindow()->SetKeyboardFocus(this, true);
		}

		// 如果有待添加的控件，点击时添加（必须在设计面板内）
		if (_controlToAdd)
		{
			if (IsPointInDesignSurface(mousePos))
				AdoptVisualChildToCanvas(*_controlToAdd, mousePos);
			_controlToAdd.reset();
			_interactionCursor = CursorKind::Arrow;
			return true;
		}

		// 先处理 TabControl 标题栏点击（切页）
		if (TryHandleTabHeaderClick(mousePos))
			return true;

		if (_tabOrderMode)
		{
			auto hitControl = HitTestControl(mousePos, input.HasModifier(ModifierKeys::Alt));
			if (!IsTabOrderCandidate(hitControl))
			{
				PublishCanvasCommandResult(
					L"SetTabOrder", L"SetTabOrder",
					DesignerDocumentTransactionResult::Success(
						DesignerDocumentTransactionState::Unchanged),
					L"请单击可接收键盘焦点的控件；Escape 退出 Tab 顺序模式。");
				return true;
			}
			(void)AssignTabOrderIndex(hitControl, _nextTabOrderIndex);
			return true;
		}

		// 设计器里的 Menu：需要“可交互”但也要选中。
		// 注意：不能让 Menu 抢走 Window 的键盘焦点，否则 Delete/方向键等设计器快捷键会失效。
		auto findDesignedMenu = [&]() -> Menu* {
			for (auto& dc : _designerControls)
			{
				if (!dc || !dc->ControlInstance) continue;
				if (dc->Type != UIClass::UI_Menu) continue;
				return dynamic_cast<Menu*>(dc->ControlInstance);
			}
			return nullptr;
		};
		auto findDesignerByControl = [&](Control* c) -> std::shared_ptr<DesignerControl> {
			if (!c) return nullptr;
			for (auto it = _designerControls.rbegin(); it != _designerControls.rend(); ++it)
			{
				auto& dc = *it;
				if (dc && dc->ControlInstance == c)
					return dc;
			}
			return nullptr;
		};

		if (auto* menu = findDesignedMenu())
		{
			auto r = GetControlRectInCanvas(menu);
			if (mousePos.x >= r.left && mousePos.x <= r.right && mousePos.y >= r.top && mousePos.y <= r.bottom)
			{
				auto dc = findDesignerByControl(menu);
				if (dc)
				{
					ClearSelection();
					AddToSelection(dc, true, true);
				}

				// Forward the normalized report so the authored menu remains
				// interactive without reopening a native-message side path.
				POINT local{ mousePos.x - r.left, mousePos.y - r.top };
				(void)cui::framework::InputAccess::DispatchInput(*menu, input.Retarget(local.x, local.y));
				// 恢复：让键盘快捷键仍由画布处理
				if (this->GetPresentationWindow()) this->GetPresentationWindow()->SetKeyboardFocus(this, true);
				return true;
			}
		}

		// 检查是否点击主选中手柄（仅单选/主选中可调整大小）
		if (_selectedControl && !_selectedControl->IsLocked
			&& _selectedControls.size() == 1)
		{
			auto rect = GetControlRectInCanvas(_selectedControl->ControlInstance);
			auto handle = HitTestHandleFromRect(
				rect, mousePos, GetSelectionHandleSizeInCanvas());
			if (handle != DesignerControl::ResizeHandle::None)
			{
				_isResizing = true;
				_resizeHandle = handle;
				auto r = GetControlRectInCanvas(_selectedControl->ControlInstance);
				_resizeStartRect = r;
				_dragStartPoint = mousePos;
				return true;
			}
		}

		const bool shift = input.HasModifier(ModifierKeys::Shift);
		ClearAlignmentGuides();
		// 选中控件
		auto hitControl = HitTestControl(mousePos, input.HasModifier(ModifierKeys::Alt));
		if (hitControl)
		{
			if (shift)
			{
				ToggleSelection(hitControl, true);
			}
			else
			{
				// 单击：如果点在已选中集合内，则保留多选并切换主选中；否则选中单个
				if (IsSelected(hitControl) && _selectedControls.size() > 1)
				{
					SetPrimarySelection(hitControl, true);
				}
				else
				{
					ClearSelection();
					AddToSelection(hitControl, true, true);
				}
			}
			if (!HasLockedSelectedControls())
				BeginDragFromCurrentSelection(mousePos);
			return true;
		}
		else
		{
			// 空白处：开始框选
			_boxSelectAddToSelection = shift;
			if (!shift)
			{
				ClearSelection();
				cui::framework::EventAccess::Raise(OnControlSelected, nullptr);
				_interactionCursor = CursorKind::Arrow;
				if (this->GetPresentationWindow())
				{
					cui::framework::WindowAccess::UpdateCursorFromCurrentMouse(
						*this->GetPresentationWindow());
				}
			}
			if (IsPointInDesignSurface(mousePos))
			{
				_isBoxSelecting = true;
				_boxSelectStart = mousePos;
				_boxSelectRect = { mousePos.x, mousePos.y, mousePos.x, mousePos.y };
				return true;
			}
			if (IsPointInDesignedWindow(mousePos)) return true;
		}
		break;
	}
	case InputReportKind::PointerMove:
	{
		if (_isPanning)
		{
			_viewOffset.x = _panStartViewOffset.x
				+ static_cast<float>(viewMousePos.x - _panStartViewPoint.x);
			_viewOffset.y = _panStartViewOffset.y
				+ static_cast<float>(viewMousePos.y - _panStartViewPoint.y);
			ClampViewOffset();
			_interactionCursor = CursorKind::SizeAll;
			this->InvalidateVisual();
			return true;
		}
		if (_tabOrderMode)
		{
			_interactionCursor = IsTabOrderCandidate(
				HitTestControl(mousePos, input.HasModifier(ModifierKeys::Alt)))
				? CursorKind::Hand : CursorKind::Arrow;
			return true;
		}
		// 框选更新
		if (_isBoxSelecting)
		{
			RECT r;
			r.left = (std::min)(_boxSelectStart.x, mousePos.x);
			r.top = (std::min)(_boxSelectStart.y, mousePos.y);
			r.right = (std::max)(_boxSelectStart.x, mousePos.x);
			r.bottom = (std::max)(_boxSelectStart.y, mousePos.y);
			_boxSelectRect = r;
			_interactionCursor = CursorKind::Arrow;
			this->InvalidateVisual();
			return true;
		}

		// 拖拽控件
		if (_isDragging && !_dragStartItems.empty())
		{
			int rawDx = mousePos.x - _dragStartPoint.x;
			int rawDy = mousePos.y - _dragStartPoint.y;
			if (rawDx == 0 && rawDy == 0)
			{
				_interactionCursor = CursorKind::SizeAll;
				return true;
			}
			if (!BeginPlacementInteraction(L"MoveSelection"))
			{
				ResetPointerInteractionState();
				return true;
			}
			try
			{
				if (!_dragHasMoved
					&& (std::abs(rawDx) >= _dragStartThreshold
						|| std::abs(rawDy) >= _dragStartThreshold))
				{
					_dragHasMoved = true;
					// 若原先在布局容器内，先抬升到根设计面，才能拖出容器边界
					if (_selectedControls.size() == 1)
						LiftSelectedToRootForDrag();
				}
				Control* refParent = (_selectedControl
					&& _selectedControl->ControlInstance)
					? _selectedControl->ControlInstance->GetVisualParent()
					: (_clientSurface ? static_cast<Control*>(_clientSurface)
						: static_cast<Control*>(_designSurface));
				RECT desired = _dragStartRectInCanvas;
				desired.left += rawDx; desired.right += rawDx;
				desired.top += rawDy; desired.bottom += rawDy;
				desired = ApplyMoveSnap(desired, refParent);
				int dx = desired.left - _dragStartRectInCanvas.left;
				int dy = desired.top - _dragStartRectInCanvas.top;
				ApplyMoveDeltaToSelection(dx, dy);
			}
			catch (...)
			{
				(void)AbortCanvasInteractionTransaction(
					L"拖动预览更新布局时抛出异常。");
				ResetPointerInteractionState();
				return true;
			}
			NotifySelectionChangedThrottled();
			_interactionCursor = CursorKind::SizeAll;
			return true;
		}

		// 调整大小
		if (_isResizing && _selectedControl && _selectedControl->ControlInstance)
		{
			int dx = mousePos.x - _dragStartPoint.x;
			int dy = mousePos.y - _dragStartPoint.y;
			if (dx == 0 && dy == 0)
			{
				_interactionCursor = GetResizeCursor(_resizeHandle);
				return true;
			}
			if (!BeginPlacementInteraction(L"ResizeSelection"))
			{
				ResetPointerInteractionState();
				return true;
			}
			try
			{
				RECT newRect = _resizeStartRect;
				switch (_resizeHandle)
				{
				case DesignerControl::ResizeHandle::TopLeft:
					newRect.left += dx; newRect.top += dy; break;
				case DesignerControl::ResizeHandle::Top:
					newRect.top += dy; break;
				case DesignerControl::ResizeHandle::TopRight:
					newRect.right += dx; newRect.top += dy; break;
				case DesignerControl::ResizeHandle::Right:
					newRect.right += dx; break;
				case DesignerControl::ResizeHandle::BottomRight:
					newRect.right += dx; newRect.bottom += dy; break;
				case DesignerControl::ResizeHandle::Bottom:
					newRect.bottom += dy; break;
				case DesignerControl::ResizeHandle::BottomLeft:
					newRect.left += dx; newRect.bottom += dy; break;
				case DesignerControl::ResizeHandle::Left:
					newRect.left += dx; break;
				}

				// 最小尺寸限制
				int minSize = 20;
				if (newRect.right - newRect.left < minSize)
					newRect.right = newRect.left + minSize;
				if (newRect.bottom - newRect.top < minSize)
					newRect.bottom = newRect.top + minSize;

				Control* refParent = _selectedControl->ControlInstance->GetVisualParent()
					? _selectedControl->ControlInstance->GetVisualParent()
					: (_clientSurface ? static_cast<Control*>(_clientSurface)
						: static_cast<Control*>(_designSurface));
				newRect = ApplyResizeSnap(
					newRect, refParent, _resizeHandle);

				// 再次最小尺寸限制（吸附后可能破坏）
				if (newRect.right - newRect.left < minSize)
					newRect.right = newRect.left + minSize;
				if (newRect.bottom - newRect.top < minSize)
					newRect.bottom = newRect.top + minSize;
				// 约束到客户区（不允许进入标题栏）
				auto bounds = GetClientSurfaceRectInCanvas();
				newRect = ClampRectToBounds(newRect, bounds, false);
				ApplyRectToControl(
					_selectedControl->ControlInstance, newRect);
			}
			catch (...)
			{
				(void)AbortCanvasInteractionTransaction(
					L"缩放预览更新布局时抛出异常。");
				ResetPointerInteractionState();
				return true;
			}
			NotifySelectionChangedThrottled();

			_interactionCursor = GetResizeCursor(_resizeHandle);
			return true;
		}

		// 更新鼠标样式（仅单选时显示 resize cursor）
		if (_selectedControl && !_selectedControl->IsLocked
			&& _selectedControls.size() == 1)
		{
			auto rect = GetControlRectInCanvas(_selectedControl->ControlInstance);
			auto handle = HitTestHandleFromRect(
				rect, mousePos, GetSelectionHandleSizeInCanvas());
			if (handle != DesignerControl::ResizeHandle::None)
			{
				_interactionCursor = GetResizeCursor(handle);
				return true;
			}
		}

		// 如果是添加控件模式
		if (_controlToAdd)
		{
			_interactionCursor = CursorKind::Hand;
		}
		else
		{
			_interactionCursor = CursorKind::Arrow;
		}
		break;
	}
	case InputReportKind::PointerUp:
	{
		if (input.ChangedButton != MouseButton::Left) break;
		if (_isPanning && _panStartedWithLeftButton)
		{
			EndViewPan();
			return true;
		}
		if (_tabOrderMode) return true;
		// 框选结束：按矩形选中（限制：同一父容器）
		if (_isBoxSelecting)
		{
			_isBoxSelecting = false;
			RECT sel = _boxSelectRect;
			auto intersects = [](const RECT& a, const RECT& b) {
				RECT r;
				r.left = (std::max)(a.left, b.left);
				r.top = (std::max)(a.top, b.top);
				r.right = (std::min)(a.right, b.right);
				r.bottom = (std::min)(a.bottom, b.bottom);
				return (r.right > r.left) && (r.bottom > r.top);
			};

			std::shared_ptr<DesignerControl> firstPick = nullptr;
			Control* requiredParent = nullptr;
			if (_boxSelectAddToSelection && _selectedControl && _selectedControl->ControlInstance)
				requiredParent = _selectedControl->ControlInstance->GetVisualParent();

			bool primarySet = (_selectedControl != nullptr);
			for (auto& dc : _designerControls)
			{
				if (!dc || !dc->ControlInstance) continue;
				if (dc->Type == UIClass::UI_TabItem) continue;
				auto r = GetControlRectInCanvas(dc->ControlInstance);
				if (!intersects(sel, r)) continue;

				if (!requiredParent)
					requiredParent = dc->ControlInstance->GetVisualParent();
				if (dc->ControlInstance->GetVisualParent() != requiredParent) continue;

				if (!firstPick) firstPick = dc;

				// Shift+框选：追加；普通框选：此时已在 LBUTTONDOWN 清空过
				if (!primarySet)
				{
					AddToSelection(dc, true, false);
					primarySet = true;
				}
				else
				{
					AddToSelection(dc, false, false);
				}
			}
			_boxSelectAddToSelection = false;
			cui::framework::EventAccess::Raise(OnControlSelected, _selectedControl);
			this->InvalidateVisual();
			return true;
		}

		// 拖拽结束：单选时尝试放入容器
		if (_isDragging && _selectedControls.size() == 1
			&& (_dragHasMoved || _dragLiftedToRoot))
		{
			try
			{
				TryReparentSelectedAfterDrag();
			}
			catch (...)
			{
				(void)AbortCanvasInteractionTransaction(
					L"拖放期间更新父子关系时抛出异常。");
				ResetPointerInteractionState();
				return true;
			}
		}
		const bool hasTransaction = _activePropertyInteraction
			|| _activePlacementInteraction
			|| !_activeInteractionTransaction.empty();
		ResetPointerInteractionState();
		if (hasTransaction)
			(void)CommitCanvasInteractionTransaction();
		return true;
	}
	case InputReportKind::Cancel:
	case InputReportKind::CaptureLost:
	case InputReportKind::FocusLost:
	{
		if (_isPanning)
		{
			_isPanning = false;
			_panStartedWithLeftButton = false;
			_interactionCursor = CursorKind::Arrow;
			NotifyViewChanged();
			return true;
		}
		(void)CancelActivePointerInteraction(
			input.Kind == InputReportKind::Cancel
				? L"系统取消了画布交互。"
				: input.Kind == InputReportKind::FocusLost
					? L"画布失去键盘焦点，修改已回滚。"
					: L"画布失去鼠标捕获，修改已回滚。");
		return true;
	}
	}

	return Panel::ProcessInput(input);
}

void DesignerCanvas::SetControlToAdd(UIClass type)
{
	if (_tabOrderMode) (void)SetTabOrderMode(false);
	_controlToAdd = BuiltInDescriptor(type);
}

void DesignerCanvas::SetControlToAdd(
	const DesignerControlDescriptor& descriptor)
{
	if (_tabOrderMode) (void)SetTabOrderMode(false);
	_controlToAdd = descriptor.IsValid()
		? BuiltInDescriptor(descriptor.Type) : std::nullopt;
}

bool DesignerCanvas::UpdateControlDropPreview(
	const DesignerControlDescriptor& descriptor,
	POINT canvasPos,
	std::wstring* outTargetDescription)
{
	if (outTargetDescription) outTargetDescription->clear();
	const auto schemaDescriptor = descriptor.IsValid()
		? BuiltInDescriptor(descriptor.Type) : std::nullopt;
	if (!schemaDescriptor || !_designSurface || !_clientSurface
		|| HasActiveDocumentTransaction()
		|| !IsPointInDesignSurface(canvasPos))
	{
		ClearControlDropPreview();
		return false;
	}

	Control* container = nullptr;
	Control* runtimeHost = _clientSurface;
	container = NormalizeContainerForDrop(
		FindBestContainerAtPoint(canvasPos, nullptr),
		schemaDescriptor->Type);
	if (container
		&& !LayoutBridge::CanAcceptChild(container, schemaDescriptor->Type))
		container = nullptr;
	if (!container && _clientSurface->VisualChildCount() != 0)
	{
		ClearControlDropPreview();
		return false;
	}
	if (container)
		runtimeHost = container;
	if (!runtimeHost)
	{
		ClearControlDropPreview();
		return false;
	}

	std::wstring target = L"窗体根";
	if (container)
	{
		const auto found = std::find_if(
			_designerControls.begin(), _designerControls.end(),
			[container](const std::shared_ptr<DesignerControl>& candidate)
			{
				return candidate && candidate->ControlInstance == container;
			});
		target = found != _designerControls.end() && *found
			? (*found)->Name : L"目标容器";
	}

	const int width = static_cast<int>(std::ceil((std::max)(
		1.0f, schemaDescriptor->DefaultSize.width)));
	const int height = static_cast<int>(std::ceil((std::max)(
		1.0f, schemaDescriptor->DefaultSize.height)));
	RECT preview{
		canvasPos.x - 30,
		canvasPos.y - 12,
		canvasPos.x - 30 + width,
		canvasPos.y - 12 + height };
	const RECT targetRect = runtimeHost == _clientSurface
		? GetClientSurfaceRectInCanvas()
		: GetControlRectInCanvas(runtimeHost);
	preview = ClampRectToBounds(preview, targetRect, true);

	_controlDropPreviewVisible = true;
	_controlDropPreviewRect = preview;
	_controlDropTargetRect = targetRect;
	_controlDropTargetDescription = target;
	_controlDropPreviewDescriptor = *schemaDescriptor;
	if (outTargetDescription) *outTargetDescription = target;
	this->InvalidateVisual();
	return true;
}

void DesignerCanvas::ClearControlDropPreview()
{
	const bool changed = _controlDropPreviewVisible
		|| _controlDropPreviewDescriptor.has_value();
	_controlDropPreviewVisible = false;
	_controlDropPreviewRect = { 0, 0, 0, 0 };
	_controlDropTargetRect = { 0, 0, 0, 0 };
	_controlDropTargetDescription.clear();
	_controlDropPreviewDescriptor.reset();
	if (changed) this->InvalidateVisual();
}

DesignerDocumentTransactionResult DesignerCanvas::AdoptVisualChildToCanvas(
	UIClass type, POINT canvasPos)

{
	const auto descriptor = BuiltInDescriptor(type);
	return AdoptVisualChildToCanvas(
		descriptor.value_or(DesignerControlDescriptor{}), canvasPos);
}

DesignerDocumentTransactionResult DesignerCanvas::AdoptVisualChildToCanvas(
	const DesignerControlDescriptor& descriptor, POINT canvasPos)
{
	ClearControlDropPreview();
	const auto schemaDescriptor = descriptor.IsValid()
		? BuiltInDescriptor(descriptor.Type) : std::nullopt;
	const auto type = schemaDescriptor
		? schemaDescriptor->Type : UIClass::UI_Base;
	DesignerDocumentTransactionResult result;
	if (!schemaDescriptor)
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"控件类型不属于当前 XAML Schema 工具箱。");
	}
	else if (!_designSurface || !_clientSurface)
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"设计区域不可用。", false);
	}
	else if (!IsPointInDesignSurface(canvasPos))
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"只能在设计区域内添加控件。");
	}
	else if (HasActiveDocumentTransaction())
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"其他文档事务进行中，不能添加控件。");
	}
	else
	{
		const auto beforeSelectionNames = CaptureSelectionNames();
		const auto beforePrimarySelectionName = _selectedControl
			? _selectedControl->Name : std::wstring{};
		const auto beforeCount = _designerControls.size();
		std::unordered_set<Control*> beforeRuntimeControls;
		std::function<void(Control*)> collectRuntimeControls =
			[&](Control* parent)
			{
				if (!parent) return;
				for (int index = 0; index < parent->VisualChildCount(); ++index)
				{
					auto* child = parent->GetVisualChild(index);
					if (!child || !beforeRuntimeControls.insert(child).second)
						continue;
					collectRuntimeControls(child);
				}
			};
		collectRuntimeControls(_clientSurface);
		std::shared_ptr<DesignerControl> added;
		std::wstring error;

		try
		{
			AdoptVisualChildToCanvasCore(*schemaDescriptor, canvasPos);
			if (_designerControls.size() > beforeCount)
				added = _selectedControl;
		}
		catch (...)
		{
			error = L"创建控件时抛出异常。";
		}

		auto removeAddedControl = [&]() noexcept
		{
			try
			{
				std::vector<Control*> newRoots;
				std::function<void(Control*)> findNewRoots =
					[&](Control* parent)
					{
						if (!parent) return;
						for (int index = 0; index < parent->VisualChildCount(); ++index)
						{
							auto* child = parent->GetVisualChild(index);
							if (!child) continue;
							if (!beforeRuntimeControls.contains(child))
							{
								newRoots.push_back(child);
								continue;
							}
							findNewRoots(child);
						}
					};
				findNewRoots(_clientSurface);
				ClearSelection();
				for (auto* root : newRoots)
				{
					for (const auto& wrapper : _designerControls)
						if (wrapper && wrapper->ControlInstance
							&& (wrapper->ControlInstance == root
								|| IsDescendantOf(
									root, wrapper->ControlInstance)))
							DetachDesignBindingPreview(*wrapper);
					RemoveDesignerControlsInSubtree(root);
					if (root->GetVisualParent())
						(void)root->GetVisualParent()->DeleteVisualChild(root);
				}
				RestoreSelectionByNames(
					beforeSelectionNames,
					beforePrimarySelectionName,
					true);
			}
			catch (...) {}
		};

		if (!added || !added->ControlInstance)
		{
			removeAddedControl();
			result = DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				error.empty()
					? L"控件创建失败或该控件类型不受支持。"
					: error,
				true);
		}
		else
		{
			DesignerControlSubtreeSnapshot snapshot;
			if (!ControlSubtreeCommand::Capture(
				this, { added }, snapshot, &error))
			{
				removeAddedControl();
				result = DesignerDocumentTransactionResult::Failure(
					DesignerDocumentTransactionState::Failed,
					L"无法记录新控件子树：" + error,
					true);
			}
			else
			{
				const auto afterSelectionNames = CaptureSelectionNames();
				const auto afterPrimarySelectionName = _selectedControl
					? _selectedControl->Name : std::wstring{};
				try
				{
					auto command = std::make_unique<ControlSubtreeCommand>(
						this,
						std::move(snapshot),
						beforeSelectionNames,
						afterSelectionNames,
						beforePrimarySelectionName,
						afterPrimarySelectionName,
						true,
						L"AdoptVisualChild",
						true);
					result = CommitAlreadyAppliedCommand(std::move(command));
				}
				catch (...)
				{
					result = DesignerDocumentTransactionResult::Failure(
						DesignerDocumentTransactionState::Failed,
						L"创建新控件历史命令时抛出异常。",
						false);
				}
				if (!result && !result.DocumentRestored)
				{
					removeAddedControl();
					result.DocumentRestored = true;
				}
			}
		}
	}
	PublishCanvasCommandResult(L"AdoptVisualChild", L"AdoptVisualChild", result);
	return result;
}

DesignerDocumentTransactionResult DesignerCanvas::CopySelectedControlsCore(
	bool publishResult)
{
	DesignerDocumentTransactionResult result;
	std::wstring message;
	if (_selectedControls.empty())
	{
		result = DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
		message = L"没有选中可复制的控件。";
	}
	else if (HasActiveDocumentTransaction())
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布事务进行中，不能复制控件。");
	}
	else
	{
		DesignerModel::DesignDocument document;
		DesignerModel::DesignDocument fragment;
		std::wstring error;
		std::vector<int> selectedIds;
		selectedIds.reserve(_selectedControls.size());
		for (const auto& control : _selectedControls)
			if (control && control->StableId > 0)
				selectedIds.push_back(control->StableId);
		if (!BuildDesignDocument(document, &error)
			|| !DesignerModel::DesignDocumentClipboard::Capture(
				document, selectedIds, fragment, &error))
		{
			result = DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				error.empty() ? L"无法构造控件剪贴板片段。" : error);
		}
		else
		{
			try
			{
				const auto xaml = DesignerModel::XamlDocumentSerializer::ToXaml(
					fragment);
				DesignerClipboardFallback = Convert::Utf8ToUnicode(xaml);
				_clipboardPasteSequence = 0;
				_lastPastedClipboardText.clear();
				_lastPastedRootNames.clear();
				std::wstring clipboardError;
				const bool systemClipboard = TryWriteClipboardText(
					DesignerClipboardFallback, &clipboardError);
				DesignerClipboardSequence = ::GetClipboardSequenceNumber();
				DesignerClipboardFallbackPreferred = !systemClipboard;
				const auto rootCount = static_cast<size_t>(std::count_if(
					fragment.Nodes.begin(), fragment.Nodes.end(),
					[](const auto& node)
					{
						return node.ParentId == 0 && node.ParentRef.empty();
					}));
				message = L"已复制 " + std::to_wstring(rootCount)
					+ L" 个控件子树（共 "
					+ std::to_wstring(fragment.Nodes.size()) + L" 个控件）";
				if (!systemClipboard)
					message += L"；系统剪贴板暂不可用，仍可在当前设计器会话粘贴";
				message += L"。";
				result = DesignerDocumentTransactionResult::Success(
					DesignerDocumentTransactionState::Unchanged);
			}
			catch (const std::exception& exception)
			{
				result = DesignerDocumentTransactionResult::Failure(
					DesignerDocumentTransactionState::Failed,
					L"无法把控件片段写成 CUI XAML："
						+ Convert::Utf8ToUnicode(exception.what()));
			}
			catch (...)
			{
				result = DesignerDocumentTransactionResult::Failure(
					DesignerDocumentTransactionState::Failed,
					L"把控件片段写成 CUI XAML 时发生未知异常。");
			}
		}
	}
	if (publishResult)
		PublishCanvasCommandResult(
			L"CopySelection", L"CopySelection", result, std::move(message));
	return result;
}

DesignerDocumentTransactionResult DesignerCanvas::CopySelectedControls()
{
	return CopySelectedControlsCore(true);
}

DesignerDocumentTransactionResult DesignerCanvas::CutSelectedControls()
{
	const auto selectedCount = _selectedControls.size();
	auto result = CopySelectedControlsCore(false);
	std::wstring message;
	if (result)
	{
		result = DeleteSelectedControl(false);
		if (result)
			message = L"已剪切 " + std::to_wstring(selectedCount)
				+ L" 个选中控件到 CUI XAML 剪贴板。";
	}
	PublishCanvasCommandResult(
		L"CutSelection", L"CutSelection", result, std::move(message));
	return result;
}

bool DesignerCanvas::CanPasteControlsFromClipboard() const noexcept
{
	const auto currentSequence = ::GetClipboardSequenceNumber();
	if (DesignerClipboardFallbackPreferred
		&& currentSequence == DesignerClipboardSequence)
		return !DesignerClipboardFallback.empty();
	std::wstring clipboardText;
	const auto readState = TryReadClipboardText(clipboardText, nullptr);
	if (readState == ClipboardTextReadState::Text)
		return !clipboardText.empty();
	if (readState == ClipboardTextReadState::Unavailable)
		return ::IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE;
	return !DesignerClipboardFallback.empty()
		&& (DesignerClipboardSequence == 0
			|| currentSequence == DesignerClipboardSequence);
}

DesignerDocumentTransactionResult DesignerCanvas::PasteControlsFromClipboard()
{
	return PasteControlsFromClipboardCore(
		ClipboardPastePlacement::Cascade, std::nullopt);
}

DesignerDocumentTransactionResult
DesignerCanvas::PasteControlsFromClipboardInPlace()
{
	return PasteControlsFromClipboardCore(
		ClipboardPastePlacement::InPlace, std::nullopt);
}

DesignerDocumentTransactionResult DesignerCanvas::PasteControlsFromClipboardAt(
	POINT canvasPosition)
{
	return PasteControlsFromClipboardCore(
		ClipboardPastePlacement::AtCanvasPoint, canvasPosition);
}

DesignerDocumentTransactionResult DesignerCanvas::PasteControlsFromClipboardCore(
	ClipboardPastePlacement placement,
	std::optional<POINT> canvasPosition)
{
	DesignerDocumentTransactionResult result;
	std::wstring message;
	if (HasActiveDocumentTransaction())
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布事务进行中，不能粘贴控件。");
		PublishCanvasCommandResult(
			L"PasteSelection", L"PasteSelection", result);
		return result;
	}

	std::wstring clipboardText;
	std::wstring clipboardError;
	const auto currentSequence = ::GetClipboardSequenceNumber();
	if (DesignerClipboardFallbackPreferred
		&& currentSequence == DesignerClipboardSequence)
	{
		clipboardText = DesignerClipboardFallback;
	}
	else
	{
		const auto readState = TryReadClipboardText(
			clipboardText, &clipboardError);
		if (readState != ClipboardTextReadState::Text
			&& (DesignerClipboardSequence == 0
				|| currentSequence == DesignerClipboardSequence))
			clipboardText = DesignerClipboardFallback;
	}
	if (clipboardText.empty())
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			clipboardError.empty()
				? L"系统剪贴板中没有可粘贴的 CUI XAML 文本。"
				: clipboardError);
		PublishCanvasCommandResult(
			L"PasteSelection", L"PasteSelection", result);
		return result;
	}
	return PasteControlsFromXamlTextCore(
		clipboardText, placement, canvasPosition);
}

DesignerDocumentTransactionResult DesignerCanvas::DuplicateSelectedControls()
{
	DesignerDocumentTransactionResult result;
	std::wstring message;
	if (HasActiveDocumentTransaction() || HasActiveDeltaInteraction())
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布事务进行中，不能重复控件。");
		PublishCanvasCommandResult(
			L"DuplicateSelection", L"DuplicateSelection", result);
		return result;
	}
	try
	{
		DesignerModel::DesignDocument current;
		DesignerModel::DesignDocument fragment;
		DesignerModel::DesignDocument merged;
		DesignerModel::DesignClipboardPasteResult duplicate;
		std::vector<DesignerModel::DesignClipboardRootTarget> duplicateTargets;
		std::wstring error;
		std::vector<int> selectedIds;
		selectedIds.reserve(_selectedControls.size());
		for (const auto& selected : _selectedControls)
			if (selected && selected->StableId > 0)
				selectedIds.push_back(selected->StableId);
		if (!BuildDesignDocument(current, &error)
			|| !DesignerModel::DesignDocumentClipboard::Capture(
				current, selectedIds, fragment, &error))
		{
			result = DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Rejected,
				error.empty() ? L"没有可重复的选中控件。" : error);
		}
		else
		{
			for (const auto& root : fragment.Nodes)
			{
				if (root.ParentId != 0 || !root.ParentRef.empty()) continue;
				const auto source = std::find_if(
					current.Nodes.begin(), current.Nodes.end(),
					[&root](const auto& node) { return node.Id == root.Id; });
				if (source == current.Nodes.end())
				{
					error = L"无法恢复重复控件的原父级：" + root.Name;
					break;
				}
				if (source->ParentId == 0 && source->ParentRef.empty())
				{
					error = L"Window.Content 根元素不能创建同级副本；请重复其子元素。";
					break;
				}
				DesignerModel::DesignClipboardRootTarget destination;
				destination.FragmentRootId = root.Id;
				destination.ParentId = source->ParentId;
				destination.ParentRef = source->ParentRef;
				destination.ComponentContentProperty = std::nullopt;
				if (source->ParentId > 0)
				{
					const auto parent = std::find_if(
						current.Nodes.begin(), current.Nodes.end(),
						[&source](const auto& node)
						{
							return node.Id == source->ParentId;
						});
					if (parent != current.Nodes.end()
						&& (parent->Type == UIClass::UI_StackPanel
							|| parent->Type == UIClass::UI_WrapPanel
							|| parent->Type == UIClass::UI_DockPanel
							|| IsUIClassAssignableFrom(
								UIClass::UI_ItemsControl, parent->Type)))
						destination.InsertIndex = source->Order + 1;
				}
				duplicateTargets.push_back(std::move(destination));
			}
			if (!error.empty()
				|| !DesignerModel::DesignDocumentClipboard::Paste(
					current, fragment, duplicateTargets, 12, 12,
					merged, &duplicate, &error))
			{
				result = DesignerDocumentTransactionResult::Failure(
					DesignerDocumentTransactionState::Rejected,
					error.empty() ? L"无法在原容器中重复控件。" : error);
					PublishCanvasCommandResult(
						L"DuplicateSelection", L"DuplicateSelection", result);
					return result;
				}

			const std::unordered_set<int> duplicateRootIds(
				duplicate.RootIds.begin(), duplicate.RootIds.end());
			for (auto& node : merged.Nodes)
			{
				if (!duplicateRootIds.contains(node.Id)
					|| node.ParentId <= 0) continue;
				const auto parent = std::find_if(
					current.Nodes.begin(), current.Nodes.end(),
					[&node](const auto& candidate)
					{
						return candidate.Id == node.ParentId;
					});
				if (parent == current.Nodes.end()) continue;
				const bool managedParent = parent->Type == UIClass::UI_StackPanel
					|| parent->Type == UIClass::UI_Grid
					|| parent->Type == UIClass::UI_DockPanel
					|| parent->Type == UIClass::UI_WrapPanel
					|| parent->Type == UIClass::UI_RelativePanel
					|| IsUIClassAssignableFrom(
						UIClass::UI_ItemsControl, parent->Type);
				if (!managedParent) continue;
				const auto relativeMargin = NodeThickness(node, L"Margin");
				ClearManagedPlacementMetadata(node);
				if (parent->Type == UIClass::UI_RelativePanel)
				{
					auto margin = relativeMargin;
					margin.Left += 12.0f;
					margin.Top += 12.0f;
					SetNodeThickness(node, L"Margin", margin);
				}
			}
			result = ExecuteDocumentEditTransaction(
				L"DuplicateSelection",
				[this, &merged, &duplicate](std::wstring& applyError)
				{
					if (!ApplyDesignDocument(merged, &applyError)) return false;
					RestoreSelectionByNames(
						duplicate.RootNames,
						duplicate.RootNames.empty()
							? std::wstring{} : duplicate.RootNames.front(),
						true);
					return true;
				});
			if (result)
				message = L"已创建 "
					+ std::to_wstring(duplicate.RootNames.size())
					+ L" 个同层级偏移副本并选中新控件。";
		}
	}
	catch (const std::exception& exception)
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"重复控件时发生异常："
				+ Convert::Utf8ToUnicode(exception.what()));
	}
	catch (...)
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"重复控件时发生未知异常。");
	}
	PublishCanvasCommandResult(
		L"DuplicateSelection", L"DuplicateSelection", result,
		std::move(message));
	return result;
}

DesignerDocumentTransactionResult DesignerCanvas::PasteControlsFromXamlText(
	const std::wstring& clipboardText)
{
	return PasteControlsFromXamlTextCore(
		clipboardText, ClipboardPastePlacement::Cascade, std::nullopt);
}

DesignerDocumentTransactionResult
DesignerCanvas::PasteControlsFromXamlTextInPlace(
	const std::wstring& clipboardText)
{
	return PasteControlsFromXamlTextCore(
		clipboardText, ClipboardPastePlacement::InPlace, std::nullopt);
}

DesignerDocumentTransactionResult DesignerCanvas::PasteControlsFromXamlTextAt(
	const std::wstring& clipboardText,
	POINT canvasPosition)
{
	return PasteControlsFromXamlTextCore(
		clipboardText, ClipboardPastePlacement::AtCanvasPoint, canvasPosition);
}

DesignerDocumentTransactionResult DesignerCanvas::PasteControlsFromXamlTextCore(
	const std::wstring& clipboardText,
	ClipboardPastePlacement placement,
	std::optional<POINT> canvasPosition)
{
	DesignerDocumentTransactionResult result;
	std::wstring message;
	std::wstring clipboardError;
	if (HasActiveDocumentTransaction())
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"画布事务进行中，不能粘贴控件。");
		PublishCanvasCommandResult(
			L"PasteSelection", L"PasteSelection", result);
		return result;
	}
	if (placement == ClipboardPastePlacement::AtCanvasPoint
		&& (!canvasPosition || !IsPointInDesignSurface(*canvasPosition)))
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"“粘贴到此处”的目标必须位于设计区域内。");
		PublishCanvasCommandResult(
			L"PasteSelection", L"PasteSelection", result);
		return result;
	}
	try
	{
		DesignerModel::DesignDocument fragment;
		if (!ParseClipboardXaml(clipboardText, fragment, &clipboardError))
		{
			result = DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Rejected,
				clipboardError);
			PublishCanvasCommandResult(
				L"PasteSelection", L"PasteSelection", result);
			return result;
		}
		DesignerModel::DesignDocument current;
		if (!BuildDesignDocument(current, &clipboardError))
		{
			result = DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"无法建立粘贴前文档：" + clipboardError);
			PublishCanvasCommandResult(
				L"PasteSelection", L"PasteSelection", result);
			return result;
		}
		if (clipboardText != _lastPastedClipboardText)
		{
			_clipboardPasteSequence = 0;
			_lastPastedRootNames.clear();
		}
		const auto nextSequence = (_clipboardPasteSequence % 20U) + 1U;
		int offsetX = placement == ClipboardPastePlacement::Cascade
			? static_cast<int>(nextSequence * 12U) : 0;
		int offsetY = offsetX;
		DesignerModel::DesignDocument merged;
		DesignerModel::DesignClipboardPasteResult pasteResult;

		DesignerModel::DesignDocumentGraph fragmentGraph;
		if (!DesignerModel::DesignDocumentGraph::Build(
			fragment, fragmentGraph, &clipboardError))
		{
			result = DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Rejected,
				clipboardError);
			PublishCanvasCommandResult(
				L"PasteSelection", L"PasteSelection", result);
			return result;
		}

		int destinationParentId = 0;
		std::wstring destinationParentRef;
		Control* destinationRuntimeParent = _clientSurface;
		std::wstring destinationDescription = L"窗体根";
		std::optional<int> destinationInsertIndex;
		std::optional<std::pair<int, int>> destinationGridCell;
		std::optional<Dock> destinationDock;
		auto findCurrentNode = [&](int id)
			-> const DesignerModel::DesignNode*
		{
			const auto found = std::find_if(
				current.Nodes.begin(), current.Nodes.end(),
				[id](const auto& node) { return node.Id == id; });
			return found == current.Nodes.end() ? nullptr : &*found;
		};
		auto setTabItemDestination = [&](TabControl* tabs,
			const DesignerControl& owner, int pageIndex) -> bool
		{
			if (!tabs || pageIndex < 0
				|| pageIndex >= static_cast<int>(tabs->ItemCount())) return false;
			auto* page = tabs->GetItem(pageIndex);
			const auto wrapper = std::find_if(
				_designerControls.begin(), _designerControls.end(),
				[page](const auto& candidate)
				{ return candidate && candidate->ControlInstance == page; });
			if (wrapper == _designerControls.end() || !*wrapper) return false;
			destinationParentId = (*wrapper)->StableId;
			destinationParentRef.clear();
			destinationRuntimeParent = page;
			destinationDescription = owner.Name + L" 的当前页";
			return destinationRuntimeParent != nullptr;
		};

		const bool pasteAtPoint = placement
			== ClipboardPastePlacement::AtCanvasPoint;
		if (pasteAtPoint)
		{
			UIClass pastedRootType = UIClass::UI_Base;
			if (!fragmentGraph.Roots().empty())
			{
				const auto firstRootIndex =
					fragmentGraph.Nodes()[fragmentGraph.Roots().front()]
						.SourceIndex;
				const auto candidateType =
					fragment.Nodes[firstRootIndex].Type;
				const bool commonRootType = std::all_of(
					fragmentGraph.Roots().begin(),
					fragmentGraph.Roots().end(),
					[&](size_t graphIndex)
					{
						return fragment.Nodes[
							fragmentGraph.Nodes()[graphIndex].SourceIndex]
								.Type == candidateType;
					});
				if (commonRootType) pastedRootType = candidateType;
			}
			Control* container = NormalizeContainerForDrop(
				FindBestContainerAtPoint(*canvasPosition, nullptr),
				pastedRootType);
			if (container)
			{
				const auto wrapper = std::find_if(
					_designerControls.begin(), _designerControls.end(),
					[container](const auto& candidate)
					{
						return candidate
							&& candidate->ControlInstance == container;
					});
				if (wrapper == _designerControls.end() || !*wrapper)
				{
					clipboardError = L"无法解析右键位置的目标容器。";
				}
				else
				{
					destinationParentId = (*wrapper)->StableId;
					destinationParentRef.clear();
					destinationRuntimeParent = container;
					destinationDescription = (*wrapper)->Name;
				}
			}
		}
		else
		{
			bool fragmentContainsPrimary = false;
			if (_selectedControl)
			{
				for (const auto graphIndex : fragmentGraph.Roots())
				{
					const auto& root = fragment.Nodes[
						fragmentGraph.Nodes()[graphIndex].SourceIndex];
					if (root.Name == _selectedControl->Name)
					{
						fragmentContainsPrimary = true;
						break;
					}
				}
			}

			const bool selectionIsLastPasteRoot = _selectedControl
				&& clipboardText == _lastPastedClipboardText
				&& std::find(_lastPastedRootNames.begin(),
					_lastPastedRootNames.end(), _selectedControl->Name)
					!= _lastPastedRootNames.end();
			const bool pasteInsideSelection = _selectedControl
				&& _selectedControl->ControlInstance
				&& IsContainerControl(_selectedControl->ControlInstance)
				&& !fragmentContainsPrimary
				&& !selectionIsLastPasteRoot;
			if (pasteInsideSelection)
			{
				auto* selected = _selectedControl->ControlInstance;
				if (_selectedControl->Type == UIClass::UI_TabControl)
				{
					auto* tabs = dynamic_cast<TabControl*>(selected);
					const int pageIndex = tabs && static_cast<int>(tabs->ItemCount()) > 0
						? (std::clamp)(tabs->SelectedIndex, 0, static_cast<int>(tabs->ItemCount()) - 1)
						: -1;
					if (!setTabItemDestination(
						tabs, *_selectedControl, pageIndex))
						clipboardError = L"选中的 TabControl 没有可接收控件的页面。";
				}
				else
				{
					destinationParentId = _selectedControl->StableId;
					destinationParentRef.clear();
					destinationRuntimeParent = selected;
					destinationDescription = _selectedControl->Name;
				}
			}
			else if (_selectedControl && _selectedControl->ControlInstance)
			{
				const auto* selectedNode = findCurrentNode(
					_selectedControl->StableId);
				if (!selectedNode)
				{
					clipboardError = L"无法解析当前控件所在的粘贴容器。";
				}
				else
				{
					destinationParentId = selectedNode->ParentId;
					destinationParentRef = selectedNode->ParentRef;
					auto* selectedControl = _selectedControl->ControlInstance;
					destinationRuntimeParent = selectedControl->GetVisualParent();
					if (auto* itemsOwner = dynamic_cast<ItemsControl*>(
						selectedControl->GetLogicalParent());
						itemsOwner
						&& destinationRuntimeParent
							== cui::framework::TemplateAccess::GetItemsHost(
								*itemsOwner))
					{
						destinationRuntimeParent = itemsOwner;
					}
					destinationDescription = destinationParentId > 0
						|| !destinationParentRef.empty()
						? L"当前容器" : L"窗体根";
				}
			}
		}

		// The design host itself is not a XAML collection. If Window.Content
		// already exists, every would-be root paste targets that authored element.
		if (destinationRuntimeParent == _clientSurface)
		{
			if (const auto contentRoot = GetDocumentContentRootRecord())
			{
				destinationParentId = contentRoot->StableId;
				destinationParentRef.clear();
				destinationRuntimeParent = contentRoot->ControlInstance;
				destinationDescription = L"Window.Content ("
					+ contentRoot->Name + L")";
			}
		}
		Control* destinationLayoutParent = destinationRuntimeParent;
		if (auto* items = dynamic_cast<ItemsControl*>(destinationRuntimeParent))
			destinationLayoutParent =
				cui::framework::TemplateAccess::GetItemsHost(*items);

		auto rootCoordinate = [](const DesignerModel::DesignNode& node,
			const wchar_t* propertyName)
		{
			return static_cast<int>(std::lround(
				NodeFloat(node, propertyName)));
		};

		if (pasteAtPoint && clipboardError.empty()
			&& destinationRuntimeParent && destinationLayoutParent)
		{
			const auto dropLocal = CanvasToContainerPoint(
				*canvasPosition, destinationLayoutParent);
			auto linearInsertionIndex = [&](Orientation orientation)
			{
				int insertion = destinationLayoutParent->VisualChildCount();
				for (int index = 0;
					index < destinationLayoutParent->VisualChildCount(); ++index)
				{
					auto* child = destinationLayoutParent->GetVisualChild(index);
					if (!child || child->IsCollapsed()) continue;
					const auto location = child->GetActualLocationDip();
					const auto size = child->GetActualSizeDip();
					const float midpoint = orientation == Orientation::Vertical
						? location.y + size.height * 0.5f
						: location.x + size.width * 0.5f;
					const float point = orientation == Orientation::Vertical
						? static_cast<float>(dropLocal.y)
						: static_cast<float>(dropLocal.x);
					if (point < midpoint)
					{
						insertion = index;
						break;
					}
				}
				return insertion;
			};
			switch (destinationLayoutParent->Type())
			{
			case UIClass::UI_StackPanel:
				destinationInsertIndex = linearInsertionIndex(
					static_cast<StackPanel*>(destinationLayoutParent)
						->GetOrientation());
				break;
			case UIClass::UI_WrapPanel:
			{
				auto* wrap = static_cast<WrapPanel*>(destinationLayoutParent);
				const auto orientation = wrap->GetOrientation();
				int insertion = wrap->VisualChildCount();
				constexpr float lineTolerance = 10.0f;
				for (int index = 0; index < wrap->VisualChildCount(); ++index)
				{
					auto* child = wrap->GetVisualChild(index);
					if (!child || child->IsCollapsed()) continue;
					const auto location = child->GetActualLocationDip();
					const auto size = child->GetActualSizeDip();
					const float childLine = orientation == Orientation::Horizontal
						? static_cast<float>(location.y)
						: static_cast<float>(location.x);
					const float childMid = orientation == Orientation::Horizontal
						? location.x + size.width * 0.5f
						: location.y + size.height * 0.5f;
					const float pointLine = orientation == Orientation::Horizontal
						? static_cast<float>(dropLocal.y)
						: static_cast<float>(dropLocal.x);
					const float pointAxis = orientation == Orientation::Horizontal
						? static_cast<float>(dropLocal.x)
						: static_cast<float>(dropLocal.y);
					if (childLine > pointLine + lineTolerance
						|| (std::fabs(childLine - pointLine) <= lineTolerance
							&& pointAxis < childMid))
					{
						insertion = index;
						break;
					}
				}
				destinationInsertIndex = insertion;
				break;
			}
			case UIClass::UI_Grid:
			{
				int row = 0;
				int column = 0;
				if (static_cast<Grid*>(destinationLayoutParent)
					->TryGetCellAtPoint(cui::core::Point{
						static_cast<float>(dropLocal.x),
						static_cast<float>(dropLocal.y) }, row, column))
				{
					destinationGridCell = std::pair{ row, column };
					destinationDescription += L" 的第 "
						+ std::to_wstring(row + 1) + L" 行、第 "
						+ std::to_wstring(column + 1) + L" 列";
				}
				else clipboardError = L"无法解析 Grid 粘贴单元格。";
				break;
			}
			case UIClass::UI_DockPanel:
			{
				const auto size = destinationLayoutParent->GetActualSizeDip();
				const float width = size.width;
				const float height = size.height;
				const float left = static_cast<float>(dropLocal.x);
				const float right = width - left;
				const float top = static_cast<float>(dropLocal.y);
				const float bottom = height - top;
				const float minimumDimension = (std::min)(width, height);
				const float snap = (std::min)(40.0f,
					(std::max)(12.0f, minimumDimension * 0.25f));
				float distance = left;
				Dock dock = Dock::Left;
				if (top < distance) { distance = top; dock = Dock::Top; }
				if (right < distance) { distance = right; dock = Dock::Right; }
				if (bottom < distance) { distance = bottom; dock = Dock::Bottom; }
				const bool targetsFillSlot = distance > snap;
				if (targetsFillSlot) dock = Dock::Left;
				destinationDock = dock;
				if (!targetsFillSlot
					&& static_cast<DockPanel*>(destinationLayoutParent)
						->GetLastChildFill())
				{
					int lastVisible = destinationLayoutParent->VisualChildCount();
					for (int index = destinationLayoutParent->VisualChildCount() - 1;
						index >= 0; --index)
					{
						auto* child = destinationLayoutParent->GetVisualChild(index);
						if (!child || child->IsCollapsed()) continue;
						lastVisible = index;
						break;
					}
					destinationInsertIndex = lastVisible;
				}
				break;
			}
			default:
				break;
			}
			if (destinationInsertIndex)
				destinationDescription += *destinationInsertIndex
					< destinationLayoutParent->VisualChildCount()
					? L" 的第 " + std::to_wstring(
						*destinationInsertIndex + 1) + L" 项之前"
					: L" 的末尾";
		}

		if (pasteAtPoint && clipboardError.empty() && destinationLayoutParent)
		{
			int minimumX = (std::numeric_limits<int>::max)();
			int minimumY = (std::numeric_limits<int>::max)();
			for (const auto graphIndex : fragmentGraph.Roots())
			{
				const auto& root = fragment.Nodes[
					fragmentGraph.Nodes()[graphIndex].SourceIndex];
				minimumX = (std::min)(minimumX,
					rootCoordinate(root, L"Canvas.Left"));
				minimumY = (std::min)(minimumY,
					rootCoordinate(root, L"Canvas.Top"));
			}
			if (minimumX == (std::numeric_limits<int>::max)()) minimumX = 0;
			if (minimumY == (std::numeric_limits<int>::max)()) minimumY = 0;
			const auto destinationPoint = CanvasToChildLayoutPoint(
				*canvasPosition, destinationLayoutParent);
			offsetX = destinationPoint.x - minimumX;
			offsetY = destinationPoint.y - minimumY;
		}

		std::vector<DesignerModel::DesignClipboardRootTarget> pasteTargets;
		pasteTargets.reserve(fragmentGraph.Roots().size());
		for (const auto graphIndex : fragmentGraph.Roots())
		{
			const auto& root = fragment.Nodes[
				fragmentGraph.Nodes()[graphIndex].SourceIndex];
			if (!destinationRuntimeParent)
			{
				if (clipboardError.empty())
					clipboardError = L"当前粘贴容器不可用。";
				break;
			}
			if (!LayoutBridge::CanAcceptChild(destinationRuntimeParent, root.Type))
			{
				clipboardError = L"当前容器不接受控件 " + root.Name + L"。";
				break;
			}
			DesignerModel::DesignClipboardRootTarget destination;
			destination.FragmentRootId = root.Id;
			destination.ParentId = destinationParentId;
			destination.ParentRef = destinationParentRef;
			destination.InsertIndex = destinationInsertIndex;
			pasteTargets.push_back(std::move(destination));
		}
		if (!clipboardError.empty()
			|| !DesignerModel::DesignDocumentClipboard::Paste(
				current, fragment, pasteTargets, offsetX, offsetY,
				merged, &pasteResult, &clipboardError))
		{
			result = DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Rejected,
				clipboardError);
			PublishCanvasCommandResult(
				L"PasteSelection", L"PasteSelection", result);
			return result;
		}

		const auto destinationType = destinationRuntimeParent
			? destinationRuntimeParent->Type() : UIClass::UI_Base;
		const bool managedDestination = destinationType == UIClass::UI_StackPanel
			|| destinationType == UIClass::UI_Grid
			|| destinationType == UIClass::UI_DockPanel
			|| destinationType == UIClass::UI_WrapPanel
			|| destinationType == UIClass::UI_RelativePanel
			|| IsUIClassAssignableFrom(
				UIClass::UI_ItemsControl, destinationType);
		if (managedDestination)
		{
			const std::unordered_set<int> pastedRootIds(
				pasteResult.RootIds.begin(), pasteResult.RootIds.end());
			for (auto& node : merged.Nodes)
			{
				if (!pastedRootIds.contains(node.Id)) continue;
				const int translatedX = rootCoordinate(node, L"Canvas.Left");
				const int translatedY = rootCoordinate(node, L"Canvas.Top");
				ClearManagedPlacementMetadata(node);

				if (destinationType == UIClass::UI_RelativePanel)
				{
					SetNodeThickness(node, L"Margin", Thickness(
						static_cast<float>(translatedX),
						static_cast<float>(translatedY), 0.0f, 0.0f));
				}
				else if (destinationType == UIClass::UI_Grid
					&& destinationGridCell)
				{
					SetNodeInteger(node, L"Grid.Row", destinationGridCell->first);
					SetNodeInteger(node, L"Grid.Column", destinationGridCell->second);
					SetNodeInteger(node, L"Grid.RowSpan", 1);
					SetNodeInteger(node, L"Grid.ColumnSpan", 1);
					SetNodeInteger(node, L"HorizontalAlignment",
						static_cast<int>(HorizontalAlignment::Stretch));
					SetNodeInteger(node, L"VerticalAlignment",
						static_cast<int>(VerticalAlignment::Stretch));
				}
				else if (destinationType == UIClass::UI_DockPanel
					&& destinationDock)
				{
					SetNodeInteger(node, L"DockPanel.Dock",
						static_cast<int>(*destinationDock));
				}
			}
		}
		result = ExecuteDocumentEditTransaction(
			L"PasteSelection",
			[this, &merged, &pasteResult](std::wstring& error)
			{
				if (!ApplyDesignDocument(merged, &error)) return false;
				RestoreSelectionByNames(
					pasteResult.RootNames,
					pasteResult.RootNames.empty()
						? std::wstring{} : pasteResult.RootNames.front(),
					true);
				return true;
			});
		if (result)
		{
			if (placement == ClipboardPastePlacement::Cascade)
				_clipboardPasteSequence = nextSequence;
			_lastPastedClipboardText = clipboardText;
			_lastPastedRootNames = pasteResult.RootNames;
			message = placement == ClipboardPastePlacement::InPlace
				? L"已原位粘贴 "
				: placement == ClipboardPastePlacement::AtCanvasPoint
					? L"已粘贴到此处 " : L"已粘贴 ";
			message += std::to_wstring(pasteResult.RootNames.size())
				+ L" 个控件子树（共 "
				+ std::to_wstring(pasteResult.NodeIds.size())
				+ L" 个控件）到 " + destinationDescription
				+ L"，并选中新副本。";
		}
	}
	catch (const std::exception& exception)
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"粘贴 CUI XAML 时发生异常："
				+ Convert::Utf8ToUnicode(exception.what()));
	}
	catch (...)
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"粘贴 CUI XAML 时发生未知异常。");
	}
	PublishCanvasCommandResult(
		L"PasteSelection", L"PasteSelection", result, std::move(message));
	return result;
}

void DesignerCanvas::AdoptVisualChildToCanvasCore(UIClass type, POINT canvasPos)

{
	const auto descriptor = BuiltInDescriptor(type);
	if (descriptor) AdoptVisualChildToCanvasCore(*descriptor, canvasPos);
}

void DesignerCanvas::AdoptVisualChildToCanvasCore(
	const DesignerControlDescriptor& descriptor, POINT canvasPos)
{
	const auto type = descriptor.Type;
	Control* newControl = nullptr;
	std::unique_ptr<Control> newControlOwner;
	Control* defaultTabItem = nullptr;
	std::unique_ptr<Control> defaultTabItemOwner;
	std::wstring typeName;
	if (!_designSurface || !_clientSurface) return;
	if (!IsPointInDesignSurface(canvasPos)) return;

	// 在点击位置创建控件（左上角对齐，稍微偏移避免手感奇怪）
	int centerX = (int)canvasPos.x - 30;
	int centerY = (int)canvasPos.y - 12;

	if (!descriptor.IsValid()) return;
	newControlOwner = DesignerControlFactory::Create(type, centerX, centerY);
	if (!newControlOwner) return;
	newControl = newControlOwner.get();
	typeName = descriptor.Name;
	newControl->Width = cui::layout::Length::Fixed(
		descriptor.DefaultSize.width);
	newControl->Height = cui::layout::Length::Fixed(
		descriptor.DefaultSize.height);
	// Toolbox instances must carry the same XAML type identity and schema-owned
	// defaults as Runtime materialization. C++ constructors are behavior hosts;
	// they do not define author-facing capabilities such as Focusable.
	if (newControl)
	{
		const auto* xamlType = CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(type);
		XamlSchemaContext schemaContext;
		std::wstring schemaError;
		if (!xamlType || !CuiRuntime::XamlRuntimeSchema::AttachBuiltInType(
			*newControl, *xamlType, schemaContext, &schemaError))
			return;
	}
	if (type == UIClass::UI_TabControl)
	{
		defaultTabItemOwner =
			CuiRuntime::XamlRuntimeSchema::CreateNativeControl(
				UIClass::UI_TabItem);
		defaultTabItem = defaultTabItemOwner.get();
		const auto* xamlType =
			CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(
				UIClass::UI_TabItem);
		XamlSchemaContext schemaContext;
		std::wstring schemaError;
		if (defaultTabItem)
			(void)defaultTabItem->ClearPropertyValues();
		if (!defaultTabItem || !xamlType
			|| !CuiRuntime::XamlRuntimeSchema::AttachBuiltInType(
				*defaultTabItem, *xamlType, schemaContext, &schemaError)
			|| !defaultTabItem->TrySetPropertyValue(
				L"Header", BindingValue(L"Page 1")))
			return;
	}

	if (newControl)
	{
		// A TabControl never fabricates an untracked page while hit-testing.
		// Non-TabItem children target only an authored selected TabItem.
		Control* rawContainer = FindBestContainerAtPoint(canvasPos, nullptr);
		Control* container = NormalizeContainerForDrop(rawContainer, type);
		Control* designerParent = nullptr;
		Control* runtimeHost = nullptr;

		if (container)
		{
			if (!LayoutBridge::CanAcceptChild(container, type))
			{
				container = nullptr;
			}
		}

		if (container)
		{
			designerParent = container;
			runtimeHost = container;
			if (!runtimeHost
				|| !LayoutBridge::CanAcceptChild(runtimeHost, type)) return;
		}
		else
		{
			// Window has one Content value. A second direct child is never a
			// fallback placement target; the document must expose a container.
			if (_clientSurface->VisualChildCount() != 0) return;
		}

		// Reserve identity only after the destination is known to be valid.
		const int stableId = AllocateStableControlId();
		int defaultTabItemStableId = 0;
		if (defaultTabItem)
		{
			defaultTabItemStableId = AllocateStableControlId();
			if (LayoutBridge::AttachChild(
					newControl, std::move(defaultTabItemOwner))
				!= defaultTabItem)
				return;
			static_cast<TabControl*>(newControl)->SelectItem(0);
		}

		if (container)
		{
			POINT local = CanvasToChildLayoutPoint(
				{ centerX, centerY }, runtimeHost);
			POINT dropLocal = CanvasToContainerPoint(canvasPos, runtimeHost);
			(void)LayoutBridge::AttachChild(
				runtimeHost, std::move(newControlOwner));
			LayoutBridge::ApplyNewChildLayout(runtimeHost, newControl, local, dropLocal);
			LayoutBridge::RefreshContainerLayout(runtimeHost);
		}
		else
		{
			_clientSurface->AddOwned(std::move(newControlOwner));
			POINT local = CanvasToChildLayoutPoint(
				{ centerX, centerY }, _clientSurface);
			Canvas::SetLeft(*(newControl), static_cast<float>(local.x));
			Canvas::SetTop(*(newControl), static_cast<float>(local.y));
			// 约束初始位置到客户区
			ClampControlToDesignSurface(newControl);
		}

		std::wstring name = GenerateDefaultControlName(type, typeName);

		// 创建设计器控件包装
		auto dc = std::make_shared<DesignerControl>(
			newControl, name, type, designerParent, stableId);
		AssignDefaultXamlType(*dc);
		_designerControls.push_back(dc);
		UpdateDefaultNameCounterFromName(type, name);
		if (defaultTabItem)
		{
			auto tabItemName = GenerateDefaultControlName(
				UIClass::UI_TabItem, L"TabItem");
			auto tabItemRecord = std::make_shared<DesignerControl>(
				defaultTabItem, tabItemName, UIClass::UI_TabItem,
				newControl, defaultTabItemStableId);
			AssignDefaultXamlType(*tabItemRecord);
			_designerControls.push_back(std::move(tabItemRecord));
			UpdateDefaultNameCounterFromName(
				UIClass::UI_TabItem, tabItemName);
		}

		// 自动选中新添加的控件
		ClearSelection();
		AddToSelection(dc, true, true);
		this->InvalidateVisual();
	}
}

DesignerDocumentTransactionResult DesignerCanvas::DeleteSelectedControl(
	bool publishResult)
{
	if (_selectedControls.empty())
	{
		auto result = DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
		if (publishResult)
			PublishCanvasCommandResult(
				L"DeleteSelection", L"DeleteSelection", result,
				L"没有选中可删除的控件。");
		return result;
	}
	if (HasActiveDocumentTransaction())
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"其他文档事务进行中，不能删除控件。");
		if (publishResult)
			PublishCanvasCommandResult(
				L"DeleteSelection", L"DeleteSelection", result);
		return result;
	}

	const auto beforeSelectionNames = CaptureSelectionNames();
	const auto beforePrimarySelectionName = _selectedControl
		? _selectedControl->Name : std::wstring{};
	DesignerControlSubtreeSnapshot snapshot;
	std::wstring error;
	DesignerDocumentTransactionResult result;
	if (!ControlSubtreeCommand::Capture(
		this, _selectedControls, snapshot, &error))
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法记录待删除控件子树：" + error,
			true);
	}
	else
	{
		DesignerModel::DesignDocument currentDocument;
		std::unordered_set<std::wstring> removedNames;
		for (const auto& identity : snapshot.Identities)
			removedNames.insert(identity.Name);
		if (!BuildDesignDocument(currentDocument, &error)
			|| !ValidateCommandTargetRemoval(
				currentDocument, removedNames, &error))
		{
			result = DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Rejected,
				error.empty()
					? L"删除会产生悬空的 CommandTarget 引用。"
					: std::move(error),
				true);
		}
		else try
		{
			auto command = std::make_unique<ControlSubtreeCommand>(
				this,
				std::move(snapshot),
				beforeSelectionNames,
				std::vector<std::wstring>{},
				beforePrimarySelectionName,
				std::wstring{},
				false,
				L"DeleteSelection",
				false);
			result = ExecuteCommandCore(std::move(command));
		}
		catch (...)
		{
			result = DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"创建删除子树历史命令时抛出异常。",
				true);
		}
	}
	if (publishResult)
		PublishCanvasCommandResult(
			L"DeleteSelection", L"DeleteSelection", result);
	return result;
}

void DesignerCanvas::DeleteSelectedControlCore()
{
	if (_selectedControls.empty()) return;

	// 复制要删除的实例列表（避免删除过程中修改 _selectedControls）
	std::vector<Control*> toDelete;
	toDelete.reserve(_selectedControls.size());
	for (auto& dc : _selectedControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		// 安全：不允许删除设计面板本身
		if (dc->ControlInstance == _designSurface || dc->ControlInstance == _clientSurface) continue;
		toDelete.push_back(dc->ControlInstance);
	}

	ClearSelection();
	cui::framework::EventAccess::Raise(OnControlSelected, nullptr);

	for (auto* inst : toDelete)
	{
		if (!inst) continue;
		// 删除控件前：先移除该子树下所有 DesignerControl，避免悬挂指针
		RemoveDesignerControlsInSubtree(inst);
		DeleteVisualChildRecursive(inst);
	}
	this->InvalidateVisual();
}

void DesignerCanvas::ClearCanvasCore()
{
	if (_tabOrderMode) (void)SetTabOrderMode(false);
	ClearControlDropPreview();
	_controlToAdd.reset();
	// Selection records own shared DesignerControl wrappers whose runtime pointers
	// are about to be released below.  Drop them first so the selection-changed
	// callback cannot expose freed ControlInstance values while the document is
	// being rebuilt (for example to PropertyGrid diagnostic subscriptions).
	ClearSelection();

	_defaultContentRoot = nullptr;
	if (_clientSurface)
	{
		// 清空客户区内的所有控件（递归释放）
		while (_clientSurface->VisualChildCount() > 0)
		{
			auto c = _clientSurface->GetVisualChild(_clientSurface->VisualChildCount() - 1);
			DeleteVisualChildRecursive(c);
		}
	}
	_designerControls.clear();
	_controlTypeCounters.clear();
	_nextStableControlId = 1;
	_designedWindowNode = DesignerModel::DesignDocument{}.Window;
	(void)ApplyDesignedWindowNode(_designedWindowNode);
	_dataContextSchema.clear();
	_documentStyleSheet = {};
	_componentDefinitions.clear();
	_controlTemplates.clear();
	_dataTypes.clear();
	_dataTemplates.clear();
	_itemsPanelTemplates.clear();
	_groupStyles.clear();
	_dataLists.clear();
	_collectionViews.clear();
	_documentResourceBasePath.clear();
	_documentResources.reset();
	_previewStyleSheet.reset();
	if (_clientSurface)
		(void)cui::framework::StyleAccess::SetDocumentStyles(
			*_clientSurface, nullptr, true);

	cui::framework::EventAccess::Raise(OnControlSelected, nullptr);
}

Control* DesignerCanvas::FindControlInstanceByName(
	const std::wstring& name) const noexcept
{
	const auto found = std::find_if(
		_designerControls.begin(), _designerControls.end(),
		[&](const auto& control)
		{
			return control && control->Name == name;
		});
	return found == _designerControls.end() || !*found
		? nullptr : (*found)->ControlInstance;
}

std::optional<DesignerDataContextSchema>
DesignerCanvas::ResolveBindingSourceSchema(
	const DesignerControl& control,
	bool inherited,
	const DesignerDataContextSchema& rootSchema) const
{
	auto findRecord = [&](Control* instance) -> const DesignerControl*
	{
		if (!instance) return nullptr;
		const auto found = std::find_if(_designerControls.begin(),
			_designerControls.end(), [&](const auto& candidate)
			{
				return candidate && candidate->ControlInstance == instance;
			});
		return found == _designerControls.end() ? nullptr : found->get();
	};
	std::unordered_set<const DesignerControl*> resolving;
	std::function<std::optional<std::wstring>(const DesignerControl&, bool)>
		resolvePrefix;
	resolvePrefix = [&](const DesignerControl& current,
		bool inheritedOnly) -> std::optional<std::wstring>
	{
		if (!resolving.insert(&current).second) return std::nullopt;
		std::optional<std::wstring> prefix = std::wstring{};
		if (const auto* parent = findRecord(current.DesignerParent))
			prefix = resolvePrefix(*parent, false);
		if (!inheritedOnly)
		{
			const auto dataContext = std::find_if(
				current.DataBindings.begin(), current.DataBindings.end(),
				[](const auto& item)
				{
					return item.first == L"DataContext";
				});
			if (dataContext != current.DataBindings.end())
			{
				const auto& binding = dataContext->second;
				if (binding.IsMultiBinding()) prefix.reset();
				else if (prefix && binding.ElementName.empty()
					&& binding.RelativeSource
						== DesignerBindingRelativeSource::None)
					prefix = prefix->empty() ? binding.SourceProperty
						: *prefix + L"." + binding.SourceProperty;
				else prefix.reset();
			}
		}
		resolving.erase(&current);
		return prefix;
	};
	const auto prefix = resolvePrefix(control, inherited);
	if (!prefix) return std::nullopt;
	if (prefix->empty()) return rootSchema;
	DesignerDataContextSchema result;
	const auto normalized = DesignerDataContextSchemaUtils::NormalizePath(*prefix);
	const auto childPrefix = normalized + L".";
	for (const auto& property : rootSchema)
	{
		const auto path = DesignerDataContextSchemaUtils::NormalizePath(property.Path);
		if (!path.starts_with(childPrefix)) continue;
		auto projected = property;
		projected.Path = path.substr(normalized.size() + 1);
		result.push_back(std::move(projected));
	}
	DesignerDataContextSchemaUtils::Canonicalize(result);
	return result;
}

DesignerDataContextSchema DesignerCanvas::GetEffectiveDataContextSchema(
	const DesignerControl& control) const
{
	auto schema = ResolveBindingSourceSchema(control, false, _dataContextSchema);
	return schema ? std::move(*schema) : DesignerDataContextSchema{};
}

IBindingSource* DesignerCanvas::GetEffectiveDesignDataContextSource(
	DesignerControl& control) const
{
	return _designDataContext && control.ControlInstance
		? &control.ControlInstance->DataContextSource() : nullptr;
}

bool DesignerCanvas::SetDataContextSchema(
	DesignerDataContextSchema schema,
	std::wstring* outError)
{
	DesignerDataContextSchemaUtils::Canonicalize(schema);
	if (!DesignerDataContextSchemaUtils::Validate(schema, outError)) return false;
	for (const auto& property : schema)
		if (property.ObjectKind == DesignerDataObjectKind::BindingList
			&& std::none_of(_dataTypes.begin(), _dataTypes.end(),
				[&](const DesignerModel::DesignDataTypeDefinition& type)
				{
					return type.Name == property.ItemType;
				}))
		{
			if (outError) *outError = L"集合属性 " + property.Path
				+ L" 引用了未声明的 DataType：" + property.ItemType;
			return false;
		}
	for (const auto& control : _designerControls)
	{
		if (!control || !control->ControlInstance) continue;
		for (const auto& [targetProperty, binding] : control->DataBindings)
		{
			DesignerDataContextSchema elementSourceSchema;
			auto scopedSourceSchema = ResolveBindingSourceSchema(
				*control, targetProperty == L"DataContext",
				schema);
			const DesignerDataContextSchema* sourceSchema = scopedSourceSchema
				? &*scopedSourceSchema : nullptr;
			if (!binding.ElementName.empty())
			{
				auto* source = FindControlInstanceByName(binding.ElementName);
				if (!source)
				{
					if (outError) *outError = L"控件 " + control->Name
						+ L" 的 ElementName 引用了不存在的控件："
						+ binding.ElementName;
					return false;
				}
				elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(*source);
				sourceSchema = &elementSourceSchema;
			}
			else if (binding.RelativeSource
				== DesignerBindingRelativeSource::Self)
			{
				elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(
					*control->ControlInstance);
				sourceSchema = &elementSourceSchema;
			}
			else if (binding.RelativeSource
				== DesignerBindingRelativeSource::FindAncestor)
			{
				if (auto* source = DesignerBindingUtils::FindAncestorSource(
					*control->ControlInstance, binding))
				{
					elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(
						*source);
					sourceSchema = &elementSourceSchema;
				}
				else sourceSchema = nullptr;
			}
			if (binding.IsMultiBinding()) sourceSchema = nullptr;
			std::wstring validationError;
			const bool valid = DesignerBindingUtils::Validate(
					*control->ControlInstance,
					targetProperty,
					binding,
					nullptr,
					&validationError,
					sourceSchema);
			if (!valid)
			{
				if (outError) *outError = L"控件 " + control->Name + L"：" + validationError;
				return false;
			}
		}
	}
	_dataContextSchema = std::move(schema);
	(void)RefreshAllDesignBindings(nullptr);
	if (outError) outError->clear();
	return true;
}

void DesignerCanvas::SetDesignDataContext(
	std::shared_ptr<IBindingSource> source)
{
	_designDataContext = std::move(source);
	if (_clientSurface)
	{
		if (_designDataContext)
			(void)_clientSurface->SetDataContext(
				BindingSourceReference(_designDataContext));
		else
			(void)_clientSurface->ClearDataContext();
	}
	(void)RefreshAllDesignBindings(nullptr);
	cui::framework::EventAccess::Raise(OnControlSelected, _selectedControl);
}

void DesignerCanvas::DetachDesignBindingPreview(
	DesignerControl& control)
{
	auto* target = control.ControlInstance;
	// Remove only bindings installed by the Designer preview. User/runtime-owned
	// bindings on unrelated properties must remain intact.
	if (target)
	{
		for (const auto& [property, state] : control.BindingPreviewStates)
		{
			if (state.Status == DesignerBindingPreviewStatus::Active)
				(void)target->DataBindings.Remove(property);
		}
	}
	control.BindingPreviewStates.clear();
}

bool DesignerCanvas::RefreshDesignBindings(
	DesignerControl& control,
	std::wstring* outError)
{
	if (outError) outError->clear();
	auto* target = control.ControlInstance;
	if (!target)
	{
		if (outError) *outError = L"控件实例已经失效。";
		return false;
	}

	DetachDesignBindingPreview(control);

	bool success = true;
	std::wstring firstError;
	for (const auto& [targetProperty, configuration] : control.DataBindings)
	{
		auto& state = control.BindingPreviewStates[targetProperty];
		if (configuration.IsMultiBinding())
		{
			const bool needsDataContext = std::any_of(
				configuration.ChildBindings.begin(),
				configuration.ChildBindings.end(), [](const auto& child)
				{
					return child.ElementName.empty()
						&& child.RelativeSource == DesignerBindingRelativeSource::None;
				});
			if (needsDataContext && !_designDataContext
				&& targetProperty != L"DataContext")
			{
				state.Status = DesignerBindingPreviewStatus::Detached;
				state.Message = L"未设置设计期 DataContext；MultiBinding 配置已保存但尚未连接。";
				continue;
			}
			std::wstring validationError;
			if (!DesignerBindingUtils::Validate(*target, targetProperty,
				configuration, nullptr, &validationError, nullptr))
			{
				state.Status = DesignerBindingPreviewStatus::Error;
				state.Message = validationError;
				success = false;
				if (firstError.empty()) firstError = validationError;
				continue;
			}
			auto resolveSource = [&](const DesignerDataBinding& child,
				DesignerBindingUtils::ResolvedBindingSource& resolved,
				std::wstring* error)
			{
				if (!child.ElementName.empty())
				{
					resolved.Source = FindControlInstanceByName(child.ElementName);
					if (!resolved.Source)
					{
						if (error) *error = L"ElementName 引用了不存在的控件："
							+ child.ElementName;
						return false;
					}
				}
				else if (child.RelativeSource == DesignerBindingRelativeSource::Self)
					resolved.Source = target;
				else if (child.RelativeSource
					== DesignerBindingRelativeSource::TemplatedParent)
				{
					if (error) *error = L"公开设计树不能解析 TemplatedParent。";
					return false;
				}
				else if (child.RelativeSource
					== DesignerBindingRelativeSource::FindAncestor)
				{
					resolved.OwnedSource = DesignerBindingUtils::CreateAncestorSource(
						*target, child);
					resolved.Source = resolved.OwnedSource.Get();
				}
				else if (targetProperty == L"DataContext")
					resolved.Source = control.DesignerParent
						&& target->GetInheritanceParent()
						? &target->GetInheritanceParent()->DataContextSource()
						: _designDataContext.get();
				else if (_designDataContext)
					resolved.Source = &target->DataContextSource();
				else
				{
					if (error) *error = L"未设置设计期 DataContext。";
					return false;
				}
				return true;
			};
			std::wstring installError;
			if (!DesignerBindingUtils::InstallBinding(*target, targetProperty,
				configuration, resolveSource, &installError))
			{
				state.Status = DesignerBindingPreviewStatus::Error;
				state.Message = L"预览 MultiBinding 连接失败：" + installError;
				success = false;
				if (firstError.empty()) firstError = state.Message;
				continue;
			}
			state.Status = DesignerBindingPreviewStatus::Active;
			state.Message = L"设计期 MultiBinding 预览已连接。";
			continue;
		}
		IBindingSource* bindingSource = nullptr;
		BindingSourceReference ownedBindingSource;
		DesignerDataContextSchema elementSourceSchema;
		auto scopedSourceSchema = ResolveBindingSourceSchema(
			control, targetProperty == L"DataContext",
			_dataContextSchema);
		const DesignerDataContextSchema* sourceSchema = scopedSourceSchema
			&& !scopedSourceSchema->empty() ? &*scopedSourceSchema : nullptr;
		if (!configuration.ElementName.empty())
		{
			bindingSource = FindControlInstanceByName(configuration.ElementName);
			if (!bindingSource)
			{
				state.Status = DesignerBindingPreviewStatus::Error;
				state.Message = L"ElementName 引用了不存在的控件："
					+ configuration.ElementName;
				success = false;
				if (firstError.empty()) firstError = state.Message;
				continue;
			}
			elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(
				*bindingSource);
			sourceSchema = &elementSourceSchema;
		}
		else if (configuration.RelativeSource
			== DesignerBindingRelativeSource::Self)
		{
			bindingSource = target;
			elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(*target);
			sourceSchema = &elementSourceSchema;
		}
		else if (configuration.RelativeSource
			== DesignerBindingRelativeSource::TemplatedParent)
		{
			state.Status = DesignerBindingPreviewStatus::Error;
			state.Message = L"公开设计树不能解析 TemplatedParent。";
			success = false;
			if (firstError.empty()) firstError = state.Message;
			continue;
		}
		else if (configuration.RelativeSource
			== DesignerBindingRelativeSource::FindAncestor)
		{
			ownedBindingSource = DesignerBindingUtils::CreateAncestorSource(
				*target, configuration);
			bindingSource = ownedBindingSource.Get();
			if (auto* current = DesignerBindingUtils::FindAncestorSource(
				*target, configuration))
			{
				elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(*current);
				sourceSchema = &elementSourceSchema;
			}
			else sourceSchema = nullptr;
		}
		else if (targetProperty == L"DataContext")
			bindingSource = control.DesignerParent
				&& target->GetInheritanceParent()
				? &target->GetInheritanceParent()->DataContextSource()
				: _designDataContext.get();
		else if (_designDataContext)
			bindingSource = &target->DataContextSource();
		else if (!bindingSource)
		{
			state.Status = DesignerBindingPreviewStatus::Detached;
			state.Message = L"未设置设计期 DataContext；配置已保存但尚未连接。";
			continue;
		}
		if (bindingSource && configuration.ElementName.empty()
			&& configuration.RelativeSource
				== DesignerBindingRelativeSource::None)
		{
			elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(
				*bindingSource);
			if (!elementSourceSchema.empty()) sourceSchema = &elementSourceSchema;
			else if (target->GetInheritanceParent()
				&& targetProperty != L"DataContext")
				sourceSchema = nullptr;
		}

		std::wstring validationError;
		const bool valid = DesignerBindingUtils::Validate(
			*target,
			targetProperty,
			configuration,
			nullptr,
			&validationError,
			sourceSchema);
		if (!valid)
		{
			state.Status = DesignerBindingPreviewStatus::Error;
			state.Message = validationError;
			success = false;
			if (firstError.empty()) firstError = validationError;
			continue;
		}
		std::shared_ptr<const IBindingValueConverter> converter;
		const auto converterName = DesignerBindingUtils::Trim(
			configuration.Converter);
		if (!converterName.empty())
		{
			converter = BindingValueConverterRegistry::Create(converterName);
			if (!converter)
			{
				state.Status = DesignerBindingPreviewStatus::Error;
				state.Message = L"无法创建 Converter：" + converterName;
				success = false;
				if (firstError.empty()) firstError = state.Message;
				continue;
			}
		}
		std::optional<BindingValue> fallbackValue;
		std::optional<BindingValue> targetNullValue;
		std::optional<BindingValue> converterParameter;
		std::wstring literalError;
		if (!DesignerBindingUtils::TryConvertOptionalLiteral(
			configuration.FallbackValue, fallbackValue, &literalError)
			|| !DesignerBindingUtils::TryConvertOptionalLiteral(
				configuration.TargetNullValue, targetNullValue, &literalError)
			|| !DesignerBindingUtils::TryConvertOptionalLiteral(
				configuration.ConverterParameter, converterParameter, &literalError))
		{
			state.Status = DesignerBindingPreviewStatus::Error;
			state.Message = literalError;
			success = false;
			if (firstError.empty()) firstError = state.Message;
			continue;
		}

		auto* binding = ownedBindingSource
			? target->DataBindings.Add(
				targetProperty, std::move(ownedBindingSource),
				configuration.SourceProperty, configuration.Mode,
				configuration.UpdateMode, std::move(converter),
				std::move(fallbackValue), std::move(targetNullValue),
				std::move(converterParameter), configuration.StringFormat)
			: target->DataBindings.Add(
				targetProperty, *bindingSource, configuration.SourceProperty,
				configuration.Mode, configuration.UpdateMode,
				std::move(converter), std::move(fallbackValue),
				std::move(targetNullValue), std::move(converterParameter),
				configuration.StringFormat);
		if (!binding)
		{
			const std::wstring bindingError = target->DataBindings.LastErrorMessage();
			state.Status = DesignerBindingPreviewStatus::Error;
			state.Message = L"预览绑定连接失败：" + bindingError;
			success = false;
			if (firstError.empty()) firstError = state.Message;
			continue;
		}

		state.Status = DesignerBindingPreviewStatus::Active;
		state.Message = !configuration.ElementName.empty()
			? L"设计期 ElementName 预览绑定已连接。"
			: configuration.RelativeSource == DesignerBindingRelativeSource::Self
				? L"设计期 RelativeSource Self 预览绑定已连接。"
				: configuration.RelativeSource
					== DesignerBindingRelativeSource::FindAncestor
					? L"设计期 RelativeSource FindAncestor 预览绑定已连接。"
				: L"设计期 DataContext 预览绑定已连接。";
	}

	if (outError && !success)
		*outError = L"控件 " + control.Name + L"：" + firstError;
	return success;
}

bool DesignerCanvas::RefreshAllDesignBindings(std::wstring* outError)
{
	if (outError) outError->clear();
	bool success = true;
	for (const auto& control : _designerControls)
	{
		if (!control) continue;
		std::wstring error;
		if (!RefreshDesignBindings(*control, &error))
		{
			success = false;
			if (outError && outError->empty()) *outError = std::move(error);
		}
	}
	return success;
}

bool DesignerCanvas::SetDocumentStyleSheet(
	DesignerStyleSheet styleSheet,
	std::wstring* outError,
	const std::vector<std::pair<std::wstring, std::wstring>>& resourceRenames,
	bool allowVisualStateRebuild)
{
	DesignerStyleSheetUtils::Canonicalize(styleSheet);
	auto remapResourceKey = [&](std::wstring key)
	{
		for (const auto& [source, destination] : resourceRenames)
			if (key == source)
				key = destination;
		return key;
	};
	auto remapAnimationResources = [&](DesignerVisualStateAnimation& animation)
	{
		if (animation.HasTo && animation.ToUsesResource)
			animation.ToResourceKey = remapResourceKey(animation.ToResourceKey);
		if (animation.HasFrom && animation.FromUsesResource)
			animation.FromResourceKey = remapResourceKey(
				animation.FromResourceKey);
		if (animation.HasBy && animation.ByUsesResource)
			animation.ByResourceKey = remapResourceKey(animation.ByResourceKey);
		for (auto& keyFrame : animation.KeyFrames)
			if (keyFrame.UsesResource)
				keyFrame.ResourceKey = remapResourceKey(keyFrame.ResourceKey);
	};
	auto rewrittenComponents = _componentDefinitions;
	auto rewrittenControlTemplates = _controlTemplates;
	auto rewrittenTemplates = _dataTemplates;
	auto rewriteNodeReferences = [&](std::vector<DesignerModel::DesignNode>& nodes)
	{
		std::unordered_map<int, const DesignerModel::DesignNode*> byId;
		for (const auto& node : nodes) byId.emplace(node.Id, &node);
		auto hasLocalResource = [&](const DesignerModel::DesignNode& origin,
			const std::wstring& key)
		{
			const DesignerModel::DesignNode* scope = &origin;
			while (scope)
			{
				if (std::any_of(scope->LocalResources.Resources.begin(),
					scope->LocalResources.Resources.end(), [&](const auto& resource)
					{ return resource.Key == key; })
					|| std::any_of(scope->LocalResources.Rules.begin(),
						scope->LocalResources.Rules.end(), [&](const auto& rule)
						{ return rule.Id == key; }))
					return true;
				const auto parent = byId.find(scope->ParentId);
				scope = parent == byId.end() ? nullptr : parent->second;
			}
			return false;
		};
		for (auto& node : nodes)
		{
			for (auto& rule : node.LocalResources.Rules)
				DesignerStyleSheetUtils::RemapRuleResourceKeys(
					rule, resourceRenames, [&](const std::wstring& key)
					{ return !hasLocalResource(node, key); });
			for (auto& [property, assignment] : node.Properties.Values)
			{
				(void)property;
				for (auto* key : {
					&assignment.ResourceKey,
					&assignment.DynamicResourceKey })
				{
					if (!key->empty() && !hasLocalResource(node, *key))
						*key = remapResourceKey(*key);
				}
			}
		}
	};
	for (auto& definition : rewrittenComponents)
	{
		for (auto& property : definition.Properties)
			if (!property.DefaultResourceKey.empty())
				property.DefaultResourceKey = remapResourceKey(
				property.DefaultResourceKey);
		for (auto& group : definition.VisualStateGroups)
		{
			for (auto& transition : group.Transitions)
				for (auto& animation : transition.Animations)
					remapAnimationResources(animation);
			for (auto& state : group.States)
			{
				for (auto& setter : state.Setters)
					if (setter.UsesResource)
						setter.ResourceKey = remapResourceKey(
							setter.ResourceKey);
				for (auto& animation : state.Animations)
					remapAnimationResources(animation);
			}
		}
		for (auto& trigger : definition.EventTriggers)
			for (auto& action : trigger.Actions)
				for (auto& animation : action.Animations)
					remapAnimationResources(animation);
		rewriteNodeReferences(definition.Template);
	}
	for (auto& definition : rewrittenTemplates)
		rewriteNodeReferences(definition.Template);
	for (auto& definition : rewrittenControlTemplates)
	{
		for (auto& group : definition.VisualStateGroups)
		{
			for (auto& transition : group.Transitions)
				for (auto& animation : transition.Animations)
					remapAnimationResources(animation);
			for (auto& state : group.States)
			{
				for (auto& setter : state.Setters)
					if (setter.UsesResource)
						setter.ResourceKey = remapResourceKey(
							setter.ResourceKey);
				for (auto& animation : state.Animations)
					remapAnimationResources(animation);
			}
		}
		for (auto& trigger : definition.EventTriggers)
			for (auto& action : trigger.Actions)
				for (auto& animation : action.Animations)
					remapAnimationResources(animation);
		rewriteNodeReferences(definition.Template);
	}

	DesignerModel::DesignDocument componentContext;
	componentContext.Components = rewrittenComponents;
	componentContext.ControlTemplates = rewrittenControlTemplates;
	componentContext.DataTemplates = rewrittenTemplates;
	componentContext.ItemsPanelTemplates = _itemsPanelTemplates;
	componentContext.GroupStyles = _groupStyles;
	componentContext.StyleSheet = styleSheet;
	componentContext.ResourceBasePath = _documentResourceBasePath;
	componentContext.Resources = _documentResources;
	if (!DesignerStyleSheetUtils::ValidateAgainstRulePropertyMetadata(
		styleSheet,
		[&](const DesignerStyleRule& rule,
			CuiRuntime::XamlTypePropertySchema& schema,
			std::wstring* error) -> bool
		{
			const auto* component = rule.ComponentType.Empty()
				? nullptr : componentContext.FindComponent(rule.ComponentType);
			if (!rule.ComponentType.Empty() && !component)
			{
				if (error) *error = L"样式 TargetType 组件不存在。";
				return false;
			}
			return CuiRuntime::XamlRuntimeSchema::BuildPropertySchema(
				rule.HasType ? rule.Type : UIClass::UI_Base,
				component, componentContext, schema, error);
		},
		outError,
		_documentResourceBasePath, _documentResources))
		return false;
	std::shared_ptr<ControlStyleSheet> runtime;
	auto runtimeResourceDocument =
		std::make_shared<const DesignerModel::DesignDocument>(
			componentContext);
	CuiRuntime::XamlMaterializationOptions runtimeResourceOptions;
	runtimeResourceOptions.ControlFactory =
		[](UIClass type) { return DesignerControlFactory::Create(type); };
	runtimeResourceOptions.AllowNativeSurfacePlaceholder = true;
	auto structuralResources =
		DesignerStyleSheetUtils::BuildItemsPanelStyleResources(
			_itemsPanelTemplates);
	auto templateResources =
		CuiRuntime::XamlObjectMaterializer::
			BuildControlTemplateStyleResources(
				runtimeResourceDocument, runtimeResourceOptions);
	structuralResources.insert(
		structuralResources.end(),
		std::make_move_iterator(templateResources.begin()),
		std::make_move_iterator(templateResources.end()));
	if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
		styleSheet, runtime, outError, _documentResourceBasePath,
		_documentResources,
		structuralResources))
		return false;

	struct ScopedStyleUpdate
	{
		DesignerControl* Owner = nullptr;
		Control* Target = nullptr;
		DesignerStyleSheet Authored;
		std::shared_ptr<const ControlStyleSheet> Previous;
		std::shared_ptr<const ControlStyleSheet> Next;
	};
	std::vector<ScopedStyleUpdate> scopedStyleUpdates;
	std::unordered_map<Control*, DesignerControl*> scopedDesignerByRuntime;
	for (const auto& control : _designerControls)
		if (control && control->ControlInstance)
			scopedDesignerByRuntime.emplace(
				control->ControlInstance, control.get());
	auto parentDesigner = [&](const DesignerControl& origin)
		-> DesignerControl*
	{
		Control* parent = origin.DesignerParent;
		while (parent)
		{
			const auto found = scopedDesignerByRuntime.find(parent);
			if (found != scopedDesignerByRuntime.end()) return found->second;
			parent = parent->GetVisualParent();
		}
		return nullptr;
	};
	auto visibleItemsPanelTemplates = [&](const DesignerControl& origin)
	{
		auto result = _itemsPanelTemplates;
		std::vector<const DesignerControl*> route;
		for (const DesignerControl* scope = &origin; scope;
			scope = parentDesigner(*scope)) route.push_back(scope);
		for (auto scope = route.rbegin(); scope != route.rend(); ++scope)
		{
			if (!(*scope)->LocalObjectResources) continue;
			for (const auto& definition :
				(*scope)->LocalObjectResources->ItemsPanelTemplates)
			{
				result.erase(std::remove_if(
					result.begin(), result.end(),
					[&](const auto& current)
					{ return current.Key == definition.Key; }),
					result.end());
				result.push_back(definition);
			}
		}
		return result;
	};
	auto visibleControlTemplates = [&](const DesignerControl& origin)
	{
		auto result = rewrittenControlTemplates;
		std::vector<const DesignerControl*> route;
		for (const DesignerControl* scope = &origin; scope;
			scope = parentDesigner(*scope)) route.push_back(scope);
		for (auto scope = route.rbegin(); scope != route.rend(); ++scope)
		{
			if (!(*scope)->LocalObjectResources) continue;
			for (const auto& definition :
				(*scope)->LocalObjectResources->ControlTemplates)
			{
				result.erase(std::remove_if(
					result.begin(), result.end(),
					[&](const auto& current)
					{
						return current.Key == definition.Key
							&& current.TargetComponentType
								== definition.TargetComponentType
							&& current.TargetType
								== definition.TargetType;
					}), result.end());
				result.push_back(definition);
			}
		}
		return result;
	};
	auto hasLocalStyleResource = [&](const DesignerControl& origin,
		const std::wstring& key)
	{
		const DesignerControl* scope = &origin;
		while (scope)
		{
			if (scope->LocalResources
				&& (std::any_of(scope->LocalResources->Resources.begin(),
					scope->LocalResources->Resources.end(), [&](const auto& resource)
					{ return resource.Key == key; })
					|| std::any_of(scope->LocalResources->Rules.begin(),
						scope->LocalResources->Rules.end(), [&](const auto& rule)
						{ return rule.Id == key; })))
				return true;
			scope = parentDesigner(*scope);
		}
		return false;
	};
	std::unordered_map<DesignerControl*, DesignerStyleSheet> rewrittenLocalStyles;
	for (const auto& control : _designerControls)
	{
		if (!control || !control->LocalResources
			|| control->LocalResources->Rules.empty()) continue;
		auto authored = *control->LocalResources;
		for (auto& rule : authored.Rules)
			DesignerStyleSheetUtils::RemapRuleResourceKeys(
				rule, resourceRenames, [&](const std::wstring& key)
				{ return !hasLocalStyleResource(*control, key); });
		rewrittenLocalStyles.emplace(control.get(), std::move(authored));
	}
	for (const auto& [owner, authored] : rewrittenLocalStyles)
	{
		DesignerStyleSheet visible = styleSheet;
		std::vector<const DesignerControl*> route;
		for (const DesignerControl* scope = owner; scope;
			scope = parentDesigner(*scope)) route.push_back(scope);
		for (auto scope = route.rbegin(); scope != route.rend(); ++scope)
		{
			if (!(*scope)->LocalResources) continue;
			const auto rewritten = rewrittenLocalStyles.find(
				const_cast<DesignerControl*>(*scope));
			DesignerStyleSheetUtils::AppendLexicalScope(
				visible, rewritten == rewrittenLocalStyles.end()
				? *(*scope)->LocalResources : rewritten->second);
		}
		DesignerStyleSheet runtimeSource;
		if (!DesignerStyleSheetUtils::PrepareLocalRuntimeStyleSheet(
			authored, visible, runtimeSource, outError)) return false;
		std::shared_ptr<ControlStyleSheet> next;
		auto localItemsPanelTemplates =
			visibleItemsPanelTemplates(*owner);
		auto localResourceDocument =
			std::make_shared<DesignerModel::DesignDocument>(
				componentContext);
		localResourceDocument->ControlTemplates =
			visibleControlTemplates(*owner);
		localResourceDocument->ItemsPanelTemplates =
			localItemsPanelTemplates;
		localResourceDocument->StyleSheet = visible;
		auto localStructuralResources =
			DesignerStyleSheetUtils::BuildItemsPanelStyleResources(
				localItemsPanelTemplates);
		auto localTemplateResources =
			CuiRuntime::XamlObjectMaterializer::
				BuildControlTemplateStyleResources(
					std::move(localResourceDocument),
					runtimeResourceOptions);
		localStructuralResources.insert(
			localStructuralResources.end(),
			std::make_move_iterator(
				localTemplateResources.begin()),
			std::make_move_iterator(
				localTemplateResources.end()));
		if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
			runtimeSource, next, outError, _documentResourceBasePath,
			_documentResources,
			localStructuralResources)) return false;
		scopedStyleUpdates.push_back({ owner, owner->ControlInstance,
			authored,
			cui::framework::StyleAccess::Resources(*owner->ControlInstance),
			next });
	}

	struct DirectResourceUpdate
	{
		DesignerControl* Owner = nullptr;
		Control* Target = nullptr;
		std::wstring PropertyName;
		std::wstring ResourceKey;
		DesignerStyleValue Value;
		bool HadLocalValue = false;
		BindingValue LocalValue;
		bool HadTrackedValue = false;
		DesignerStyleValue TrackedValue;
		std::wstring PreviousResourceKey;
	};
	auto findResource = [&](const std::wstring& key)
		-> const DesignerStyleResource*
	{
		const auto found = std::find_if(
			styleSheet.Resources.begin(), styleSheet.Resources.end(),
			[&](const auto& resource)
			{
				return resource.Key == key;
			});
		return found == styleSheet.Resources.end() ? nullptr : &*found;
	};
	std::unordered_map<Control*, DesignerControl*> designerByRuntime;
	for (const auto& control : _designerControls)
		if (control && control->ControlInstance)
			designerByRuntime.emplace(control->ControlInstance, control.get());
	auto findLocalResource = [&](const DesignerControl& origin,
		const std::wstring& key) -> const DesignerStyleResource*
	{
		const DesignerControl* scope = &origin;
		while (scope)
		{
			if (scope->LocalResources)
			{
				const auto found = std::find_if(
					scope->LocalResources->Resources.rbegin(),
					scope->LocalResources->Resources.rend(),
					[&](const auto& resource)
					{ return resource.Key == key; });
				if (found != scope->LocalResources->Resources.rend()) return &*found;
			}
			Control* parent = scope->DesignerParent;
			scope = nullptr;
			while (parent && !scope)
			{
				const auto found = designerByRuntime.find(parent);
				if (found != designerByRuntime.end()) scope = found->second;
				else parent = parent->GetVisualParent();
			}
		}
		return nullptr;
	};
	auto findScopedResource = [&](const DesignerControl& owner,
		const std::wstring& key) -> const DesignerStyleResource*
	{
		if (const auto* local = findLocalResource(owner, key)) return local;
		return findResource(key);
	};
	std::vector<DirectResourceUpdate> directUpdates;
	struct DynamicResourceUpdate
	{
		DesignerControl* Owner = nullptr;
		Control* Target = nullptr;
		std::wstring PropertyName;
		std::wstring PreviousResourceKey;
		std::wstring ResourceKey;
	};
	std::vector<DynamicResourceUpdate> dynamicUpdates;
	for (const auto& control : _designerControls)
	{
		if (!control || !control->ControlInstance) continue;
		for (const auto& [propertyName, sourceKey]
			: control->MetadataPropertyResourceKeys)
		{
			const auto requestedKey = findLocalResource(*control, sourceKey)
				? sourceKey : remapResourceKey(sourceKey);
			const auto* resource = findScopedResource(*control, requestedKey);
			if (!resource)
			{
				if (outError) *outError = L"控件 " + control->Name
					+ L" 的属性 " + propertyName
					+ L" 引用了不存在的资源：" + requestedKey;
				return false;
			}
			std::wstring validationError;
			if (!DesignerPropertyCatalog::ValidateStyleValue(
				*control->ControlInstance, propertyName, resource->Value,
				&validationError, _documentResourceBasePath, _documentResources))
			{
				if (outError) *outError = L"控件 " + control->Name
					+ L" 的资源属性无效：" + validationError;
				return false;
			}
			DirectResourceUpdate update;
			update.Owner = control.get();
			update.Target = control->ControlInstance;
			update.PropertyName = propertyName;
			update.ResourceKey = resource->Key;
			update.Value = resource->Value;
			update.HadLocalValue = update.Target->TryGetPropertyValue(
				propertyName, DependencyPropertyValueSource::Local, update.LocalValue);
			if (const auto tracked = control->MetadataProperties.find(propertyName);
				tracked != control->MetadataProperties.end())
			{
				update.HadTrackedValue = true;
				update.TrackedValue = tracked->second;
			}
			update.PreviousResourceKey = sourceKey;
			directUpdates.push_back(std::move(update));
		}
		for (const auto& [propertyName, sourceKey]
			: control->MetadataPropertyDynamicResourceKeys)
		{
			const auto requestedKey = findLocalResource(*control, sourceKey)
				? sourceKey : remapResourceKey(sourceKey);
			if (const auto* resource = findScopedResource(*control, requestedKey))
			{
				std::wstring validationError;
				if (!DesignerPropertyCatalog::ValidateStyleValue(
					*control->ControlInstance, propertyName, resource->Value,
					&validationError, _documentResourceBasePath, _documentResources))
				{
					if (outError) *outError = L"控件 " + control->Name
						+ L" 的动态资源属性无效：" + validationError;
					return false;
				}
			}
			dynamicUpdates.push_back({ control.get(), control->ControlInstance,
				propertyName, sourceKey, requestedKey });
		}
	}
	// Template/component definitions may not currently have an instance. At
	// minimum enforce that every preserved expression resolves in the candidate.
	auto validateDefinitionReferences = [&](const std::vector<DesignerModel::DesignNode>& nodes,
		const std::wstring& owner) -> bool
	{
		std::unordered_map<int, const DesignerModel::DesignNode*> byId;
		for (const auto& node : nodes) byId.emplace(node.Id, &node);
		auto resolvesResource = [&](const DesignerModel::DesignNode& origin,
			const std::wstring& key)
		{
			const DesignerModel::DesignNode* scope = &origin;
			while (scope)
			{
				if (std::any_of(scope->LocalResources.Resources.begin(),
					scope->LocalResources.Resources.end(), [&](const auto& resource)
					{ return resource.Key == key; }))
					return true;
				const auto parent = byId.find(scope->ParentId);
				scope = parent == byId.end() ? nullptr : parent->second;
			}
			return findResource(key) != nullptr;
		};
		for (const auto& node : nodes)
		{
			for (const auto& [property, assignment] : node.Properties.Values)
			{
				const auto& key = assignment.ResourceKey;
				if (key.empty()) continue;
				if (resolvesResource(node, key)) continue;
				if (outError) *outError = owner + L" 的控件 " + node.Name
					+ L" 属性 " + property
					+ L" 引用了不存在的资源：" + key;
				return false;
			}
		}
		return true;
	};
	for (const auto& definition : rewrittenComponents)
	{
		for (const auto& property : definition.Properties)
			if (!property.DefaultResourceKey.empty())
			{
				const auto* resource = findResource(property.DefaultResourceKey);
				if (resource && resource->Value.Kind == property.DefaultValue.Kind)
					continue;
				if (outError) *outError = L"组件 " + definition.Type.XamlName
					+ L" 的属性 " + property.Name
					+ (resource ? L" 默认资源类型不匹配："
						: L" 引用了不存在的默认资源：")
					+ property.DefaultResourceKey;
				return false;
			}
		if (!validateDefinitionReferences(
			definition.Template, L"组件 " + definition.Type.XamlName)) return false;
		auto buildVisualStateTargetSchema = [&](const std::wstring& targetName,
			CuiRuntime::XamlTypePropertySchema& schema,
			std::wstring* error)
		{
			const DesignerModel::DesignComponentDefinition* targetComponent = nullptr;
			UIClass targetType = definition.BaseType;
			if (targetName.empty())
				targetComponent = &definition;
			else
			{
				const auto node = std::find_if(
					definition.Template.begin(), definition.Template.end(),
					[&](const auto& candidate)
					{ return candidate.Name == targetName; });
				if (node == definition.Template.end())
				{
					if (error) *error = L"模板部件不存在：" + targetName;
					return false;
				}
				targetType = node->Type;
				if (!node->ComponentType.Empty())
					targetComponent = componentContext.FindComponent(node->ComponentType);
			}
			if (!targetName.empty()
				&& std::any_of(definition.Template.begin(), definition.Template.end(),
					[&](const auto& node)
					{
						return node.Name == targetName
							&& !node.ComponentType.Empty();
					}) && !targetComponent)
			{
				if (error) *error = L"模板部件的组件 Schema 不存在："
					+ targetName;
				return false;
			}
			return CuiRuntime::XamlRuntimeSchema::BuildPropertySchema(
				targetType, targetComponent, componentContext, schema, error);
		};
		auto validateTransitionAnimation = [&](const auto& animation,
			const std::wstring& animationContext)
		{
			CuiRuntime::XamlTypePropertySchema targetSchema;
			std::wstring schemaError;
			if (!buildVisualStateTargetSchema(
				animation.TargetName, targetSchema, &schemaError))
			{
				if (outError) *outError = L"组件 " + definition.Type.XamlName
					+ L" 的 " + animationContext + L" 目标 Schema 无效："
					+ schemaError;
				return false;
			}
			const auto objectPathKind =
				DesignerModel::ClassifyStoryboardObjectPath(animation.PropertyName);
			const bool objectPath = objectPathKind
				!= DesignerModel::StoryboardObjectPathKind::None;
			DesignerModel::ResolvedStoryboardObjectPath resolvedObjectPath;
			std::wstring pathError;
			if (objectPath
				&& !DesignerModel::TryResolveStoryboardObjectPath(
					definition, animation.TargetName, animation.PropertyName,
					animation.Kind, resolvedObjectPath, &pathError))
			{
				if (outError) *outError = L"组件 " + definition.Type.XamlName
					+ L" 的 " + animationContext
					+ L" 动画路径无效：" + pathError;
				return false;
			}
			auto validateResource = [&](bool usesResource,
				const std::wstring& resourceKey, const std::wstring& label,
				bool isDelta = false)
			{
				if (!usesResource) return true;
				const auto* resource = findResource(resourceKey);
				std::wstring validationError;
				const auto* metadata = !objectPath
					? targetSchema.FindProperty(animation.PropertyName) : nullptr;
				BindingValue parsed;
				BindingValue converted;
				if (resource
					&& DesignerStyleSheetUtils::TryConvertValue(
						resource->Value, parsed, &validationError,
						_documentResourceBasePath, _documentResources))
				{
					if (objectPath && DesignerModel::ValidateStoryboardObjectPathValue(
						objectPathKind, parsed, isDelta)) return true;
					if (!objectPath && metadata && metadata->CanWrite()
						&& metadata->TryConvert(parsed, converted)) return true;
				}
				if (outError) *outError = L"组件 " + definition.Type.XamlName
					+ L" 的 " + animationContext + L" 动画 "
					+ animation.PropertyName + L" " + label
					+ (resource ? L" 资源类型不兼容：" : L" 引用了不存在的资源：")
					+ resourceKey
					+ (validationError.empty() ? L"" : L"（" + validationError + L"）");
				return false;
			};
			if ((animation.HasTo && !validateResource(animation.ToUsesResource,
				animation.ToResourceKey, L"To"))
				|| (animation.HasFrom && !validateResource(
					animation.FromUsesResource,
					animation.FromResourceKey, L"From"))
				|| (animation.HasBy && !validateResource(
					animation.ByUsesResource,
					animation.ByResourceKey, L"By", true))) return false;
			for (const auto& keyFrame : animation.KeyFrames)
				if (!validateResource(keyFrame.UsesResource,
					keyFrame.ResourceKey, L"KeyFrame")) return false;
			return true;
		};
		for (const auto& group : definition.VisualStateGroups)
		{
			for (const auto& transition : group.Transitions)
				for (const auto& animation : transition.Animations)
					if (!validateTransitionAnimation(animation,
						L"VisualTransition " + transition.FromState
							+ L" -> " + transition.ToState)) return false;
			for (const auto& state : group.States)
			{
				for (const auto& setter : state.Setters)
				{
					if (!setter.UsesResource) continue;
					const auto* resource = findResource(setter.ResourceKey);
					CuiRuntime::XamlTypePropertySchema targetSchema;
					std::wstring schemaError;
					const bool hasSchema = buildVisualStateTargetSchema(
						setter.TargetName, targetSchema, &schemaError);
					const auto* metadata = hasSchema
						? targetSchema.FindProperty(setter.PropertyName) : nullptr;
					std::wstring validationError;
					DesignerStyleValue canonical;
					if (!resource || !metadata
						|| !DesignerPropertyCatalog::NormalizeStyleValue(
							*metadata, resource->Value, canonical,
							&validationError, _documentResourceBasePath,
							_documentResources))
					{
						if (outError) *outError = L"组件 " + definition.Type.XamlName
							+ L" 的视觉状态 " + state.Name + L" Setter "
							+ setter.PropertyName
							+ (resource ? L" 资源类型不兼容：" : L" 引用了不存在的资源：")
							+ setter.ResourceKey
							+ (!schemaError.empty() ? L"（" + schemaError + L"）"
								: validationError.empty() ? L""
									: L"（" + validationError + L"）");
						return false;
					}
				}
				for (const auto& animation : state.Animations)
				{
					CuiRuntime::XamlTypePropertySchema targetSchema;
					std::wstring schemaError;
					if (!buildVisualStateTargetSchema(
						animation.TargetName, targetSchema, &schemaError))
					{
						if (outError) *outError = L"组件 " + definition.Type.XamlName
							+ L" 的视觉状态 " + state.Name
							+ L" 目标 Schema 无效：" + schemaError;
						return false;
					}
					const auto objectPathKind =
						DesignerModel::ClassifyStoryboardObjectPath(animation.PropertyName);
					const bool objectPath = objectPathKind
						!= DesignerModel::StoryboardObjectPathKind::None;
					DesignerModel::ResolvedStoryboardObjectPath resolvedObjectPath;
					std::wstring pathError;
					if (objectPath
						&& !DesignerModel::TryResolveStoryboardObjectPath(
							definition, animation.TargetName, animation.PropertyName,
							animation.Kind, resolvedObjectPath, &pathError))
					{
						if (outError) *outError = L"组件 " + definition.Type.XamlName
							+ L" 的视觉状态 " + state.Name + L" 动画路径无效："
							+ pathError;
						return false;
					}
					auto validateResource = [&](bool usesResource,
						const std::wstring& resourceKey, const std::wstring& label,
						bool isDelta = false)
					{
						if (!usesResource) return true;
						const auto* resource = findResource(resourceKey);
						std::wstring validationError;
						const auto* metadata = !objectPath
							? targetSchema.FindProperty(animation.PropertyName)
							: nullptr;
						BindingValue parsed;
						BindingValue converted;
						if (resource
							&& DesignerStyleSheetUtils::TryConvertValue(
								resource->Value, parsed, &validationError,
								_documentResourceBasePath, _documentResources))
						{
							if (objectPath
								&& DesignerModel::ValidateStoryboardObjectPathValue(
									objectPathKind, parsed, isDelta)) return true;
							if (!objectPath && metadata && metadata->CanWrite()
								&& metadata->TryConvert(parsed, converted))
								return true;
						}
						if (outError) *outError = L"组件 " + definition.Type.XamlName
							+ L" 的视觉状态 " + state.Name + L" 动画 "
							+ animation.PropertyName + L" " + label
							+ (resource ? L" 资源类型不兼容：" : L" 引用了不存在的资源：")
							+ resourceKey
							+ (validationError.empty() ? L"" : L"（" + validationError + L"）");
						return false;
					};
					if ((animation.HasTo && !validateResource(animation.ToUsesResource,
						animation.ToResourceKey, L"To"))
						|| (animation.HasFrom && !validateResource(
							animation.FromUsesResource,
							animation.FromResourceKey, L"From"))
						|| (animation.HasBy && !validateResource(
							animation.ByUsesResource,
							animation.ByResourceKey, L"By", true))) return false;
					for (const auto& keyFrame : animation.KeyFrames)
						if (!validateResource(keyFrame.UsesResource,
							keyFrame.ResourceKey, L"KeyFrame")) return false;
				}
			}
		}
		for (const auto& trigger : definition.EventTriggers)
			for (const auto& action : trigger.Actions)
				for (const auto& animation : action.Animations)
					if (!validateTransitionAnimation(animation,
						L"EventTrigger " + trigger.EventName)) return false;
	}
	for (const auto& definition : rewrittenTemplates)
		if (!validateDefinitionReferences(
			definition.Template, L"DataTemplate " + definition.DisplayName()))
			return false;

	const bool visualStatesUseResources = std::any_of(
		rewrittenComponents.begin(), rewrittenComponents.end(),
		[](const auto& definition)
		{
			auto animationUsesResource = [](const auto& animation)
			{
				return (animation.HasTo && animation.ToUsesResource)
					|| (animation.HasFrom && animation.FromUsesResource)
					|| (animation.HasBy && animation.ByUsesResource)
					|| std::any_of(animation.KeyFrames.begin(),
						animation.KeyFrames.end(), [](const auto& keyFrame)
						{ return keyFrame.UsesResource; });
			};
			return std::any_of(definition.VisualStateGroups.begin(),
				definition.VisualStateGroups.end(), [&](const auto& group)
				{
					return std::any_of(group.Transitions.begin(),
						group.Transitions.end(), [&](const auto& transition)
						{
							return std::any_of(transition.Animations.begin(),
								transition.Animations.end(), animationUsesResource);
						}) || std::any_of(group.States.begin(), group.States.end(),
						[&](const auto& state)
						{
							return std::any_of(state.Setters.begin(), state.Setters.end(),
								[](const auto& setter) { return setter.UsesResource; })
								|| std::any_of(state.Animations.begin(), state.Animations.end(),
									animationUsesResource);
						});
				}) || std::any_of(definition.EventTriggers.begin(),
				definition.EventTriggers.end(), [&](const auto& trigger)
				{
					return std::any_of(trigger.Actions.begin(),
						trigger.Actions.end(), [&](const auto& action)
						{
							return std::any_of(action.Animations.begin(),
								action.Animations.end(), animationUsesResource);
						});
				});
		});
	const bool structuralResourcesNeedRebuild =
		(!resourceRenames.empty() || styleSheet != _documentStyleSheet)
		&& (!rewrittenComponents.empty()
			|| !rewrittenControlTemplates.empty()
			|| !rewrittenTemplates.empty());
	if ((visualStatesUseResources || structuralResourcesNeedRebuild)
		&& allowVisualStateRebuild)
	{
		DesignerModel::DesignDocument candidate;
		if (!BuildDesignDocument(candidate, outError)) return false;
		rewriteNodeReferences(candidate.Nodes);
		candidate.Components = std::move(rewrittenComponents);
		candidate.ControlTemplates = std::move(rewrittenControlTemplates);
		candidate.DataTemplates = std::move(rewrittenTemplates);
		candidate.StyleSheet = std::move(styleSheet);
		return ApplyDesignDocument(candidate, outError);
	}

	auto describeIssue = [](const ControlStyleResolutionIssue& issue)
	{
		switch (issue.Code)
		{
		case ControlStyleResolutionIssueCode::MissingResource:
			return L"缺少资源 " + issue.ResourceKey;
		case ControlStyleResolutionIssueCode::PropertyNotFound:
			return L"找不到属性 " + issue.PropertyName;
		case ControlStyleResolutionIssueCode::PropertyNotWritable:
			return L"属性不可写 " + issue.PropertyName;
		case ControlStyleResolutionIssueCode::InvalidValue:
			return L"属性值无效 " + issue.PropertyName;
		}
		return std::wstring(L"未知样式错误");
	};

	if (_clientSurface)
	{
		auto resolution = runtime->Resolve(*_clientSurface);
		if (!resolution.Success())
		{
			if (outError) *outError = L"窗体客户区：" + describeIssue(resolution.Issues.front());
			return false;
		}
	}
	for (const auto& control : _designerControls)
	{
		if (!control || !control->ControlInstance) continue;
		auto resolution = runtime->Resolve(*control->ControlInstance);
		if (!resolution.Success())
		{
			if (outError) *outError = L"控件 " + control->Name + L"："
				+ describeIssue(resolution.Issues.front());
			return false;
		}
	}

	auto rollbackDirectUpdates = [&](size_t count)
	{
		bool restored = true;
		while (count > 0)
		{
			auto& update = directUpdates[--count];
			const bool localRestored = update.HadLocalValue
				? update.Target->TrySetPropertyValue(
					update.PropertyName, update.LocalValue)
				: (!update.Target->HasPropertyValue(
					update.PropertyName, DependencyPropertyValueSource::Local)
					|| update.Target->ClearPropertyValue(update.PropertyName));
			restored = restored && localRestored;
			if (update.HadTrackedValue)
				update.Owner->MetadataProperties[update.PropertyName]
					= update.TrackedValue;
			else
				update.Owner->MetadataProperties.erase(update.PropertyName);
			update.Owner->MetadataPropertyResourceKeys[update.PropertyName]
				= update.PreviousResourceKey;
		}
		return restored;
	};
	size_t appliedDirectUpdates = 0;
	for (auto& update : directUpdates)
	{
		std::wstring propertyError;
		std::wstring canonicalName;
		DesignerStyleValue effective;
		if (!DesignerPropertyCatalog::ApplyAndTrackValue(
			*update.Target, update.Owner->MetadataProperties,
			update.PropertyName, update.Value,
			&canonicalName, &effective, &propertyError,
			_documentResourceBasePath, _documentResources))
		{
			const bool restored = rollbackDirectUpdates(appliedDirectUpdates + 1);
			if (outError) *outError = L"无法更新控件资源属性：" + propertyError
				+ (restored ? L"" : L"；回滚未完整恢复。");
			return false;
		}
		update.Owner->MetadataPropertyResourceKeys.erase(update.PropertyName);
		update.Owner->MetadataPropertyResourceKeys[canonicalName]
			= update.ResourceKey;
		++appliedDirectUpdates;
	}

	if (_clientSurface)
	{
		const auto previous = _previewStyleSheet;
		const bool applied = !styleSheet.Empty()
			? cui::framework::StyleAccess::SetDocumentStyles(
				*_clientSurface, runtime, true)
			: cui::framework::StyleAccess::SetDocumentStyles(
				*_clientSurface, nullptr, true);
		if (!applied)
		{
			(void)cui::framework::StyleAccess::SetDocumentStyles(
				*_clientSurface, previous, true);
			const bool restored = rollbackDirectUpdates(appliedDirectUpdates);
			if (outError) *outError = std::wstring(
				L"样式表无法应用到完整控件树；请检查通配规则的目标属性类型。")
				+ (restored ? L"" : L" 资源属性回滚未完整恢复。");
			return false;
		}
	}
	size_t appliedScopedStyles = 0;
	auto rollbackScopedStyles = [&]() noexcept
	{
		while (appliedScopedStyles > 0)
		{
			auto& update = scopedStyleUpdates[--appliedScopedStyles];
			if (update.Target)
				(void)cui::framework::StyleAccess::SetResources(
					*update.Target, update.Previous);
		}
	};
	for (auto& update : scopedStyleUpdates)
	{
		if (update.Target
			&& cui::framework::StyleAccess::SetResources(
				*update.Target, update.Next))
		{
			++appliedScopedStyles;
			continue;
		}
		rollbackScopedStyles();
		if (_clientSurface)
			(void)cui::framework::StyleAccess::SetDocumentStyles(
				*_clientSurface, _previewStyleSheet, true);
		const bool restored = rollbackDirectUpdates(appliedDirectUpdates);
		if (outError) *outError = L"无法更新控件局部 Style 作用域。"
			+ std::wstring(restored ? L"" : L" 静态资源属性回滚未完整恢复。");
		return false;
	}
	for (size_t index = 0; index < dynamicUpdates.size(); ++index)
	{
		auto& update = dynamicUpdates[index];
		if (update.Target->SetDynamicResource(
			update.PropertyName, update.ResourceKey))
		{
			update.Owner->MetadataPropertyDynamicResourceKeys[
				update.PropertyName] = update.ResourceKey;
			continue;
		}

		if (_clientSurface)
			(void)cui::framework::StyleAccess::SetDocumentStyles(
				*_clientSurface, _previewStyleSheet, true);
		rollbackScopedStyles();
		for (size_t rollback = 0; rollback < index; ++rollback)
		{
			auto& restored = dynamicUpdates[rollback];
			(void)restored.Target->SetDynamicResource(
				restored.PropertyName, restored.PreviousResourceKey);
			restored.Owner->MetadataPropertyDynamicResourceKeys[
				restored.PropertyName] = restored.PreviousResourceKey;
		}
		const bool restored = rollbackDirectUpdates(appliedDirectUpdates);
		if (outError) *outError = L"无法更新控件动态资源属性："
			+ update.PropertyName
			+ (restored ? L"" : L"；静态资源属性回滚未完整恢复。");
		return false;
	}
	for (auto& update : scopedStyleUpdates)
		if (update.Owner && update.Owner->LocalResources)
			*update.Owner->LocalResources = std::move(update.Authored);
	_componentDefinitions = std::move(rewrittenComponents);
	_controlTemplates = std::move(rewrittenControlTemplates);
	_dataTemplates = std::move(rewrittenTemplates);
	_documentStyleSheet = std::move(styleSheet);
	_previewStyleSheet = _documentStyleSheet.Empty() ? nullptr : std::move(runtime);
	if (outError) outError->clear();
	this->InvalidateVisual();
	return true;
}

bool DesignerCanvas::SetCodeBehind(
	DesignerModel::DesignCodeBehindModel codeBehind,
	std::wstring* outError)
{
	std::wstring normalizedClass;
	if (!DesignerModel::DesignCodeBehindModel::TryNormalizeClassName(
		codeBehind.ClassName, normalizedClass, outError))
		return false;
	codeBehind.ClassName = std::move(normalizedClass);
	std::wstring normalized;
	if (!DesignerModel::DesignCodeBehindModel::TryNormalizeRelativeBasePath(
		codeBehind.RelativeBasePath, normalized, outError))
		return false;
	codeBehind.RelativeBasePath = std::move(normalized);
	if (!codeBehind.Validate(outError)) return false;
	_codeBehind = std::move(codeBehind);
	if (outError) outError->clear();
	return true;
}

static std::wstring ExportTypeName(UIClass t)
{
	if (const auto* descriptor =
		CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(t))
		return descriptor->TypeId.LocalName;
	return L"Control";
}

namespace
{
	static bool TryParseNumericSuffix(const std::wstring& name, const std::wstring& prefix, int& outSuffix);
}

namespace
{
	static std::wstring TrimWs(const std::wstring& s)
	{
		size_t b = 0;
		while (b < s.size() && iswspace(s[b])) b++;
		size_t e = s.size();
		while (e > b && iswspace(s[e - 1])) e--;
		return s.substr(b, e - b);
	}

	static std::string ToUtf8(const std::wstring& s)
	{
		return Convert::UnicodeToUtf8(s);
	}

	static std::wstring FromUtf8(const std::string& s)
	{
		return Convert::Utf8ToUnicode(s);
	}

	static DesignValue ColorToValue(const D2D1_COLOR_F& c)
	{
		return DesignValue{ {"r", c.r}, {"g", c.g}, {"b", c.b}, {"a", c.a} };
	}

	static DesignValue BrushToValue(const cui::drawing::Brush& brush)
	{
		DesignValue value = DesignValue::object();
		value["type"] = brush.Kind == cui::drawing::BrushKind::None ? "none"
			: brush.Kind == cui::drawing::BrushKind::Solid ? "solid"
			: brush.Kind == cui::drawing::BrushKind::LinearGradient ? "linear"
			: brush.Kind == cui::drawing::BrushKind::RadialGradient ? "radial"
			: "image";
		if (brush.Kind == cui::drawing::BrushKind::None) return value;
		value["mapping"] = brush.MappingMode
			== cui::drawing::BrushMappingMode::Absolute ? "absolute" : "relative";
		value["opacity"] = brush.Opacity;
		if (brush.Kind == cui::drawing::BrushKind::Solid)
			value["color"] = ColorToValue(brush.Color);
		else if (brush.Kind == cui::drawing::BrushKind::Image)
		{
			value["source"] = brush.ImageSource
				? ToUtf8(brush.ImageSource->GetSourceUri()) : std::string{};
			value["stretch"] = brush.Stretch == ::Stretch::None
				? "none" : brush.Stretch == ::Stretch::Uniform
					? "uniform" : brush.Stretch == ::Stretch::UniformToFill
						? "uniformToFill" : "fill";
			value["alignmentX"] = brush.AlignmentX == cui::drawing::ImageBrushAlignmentX::Left
				? "left" : brush.AlignmentX == cui::drawing::ImageBrushAlignmentX::Right
					? "right" : "center";
			value["alignmentY"] = brush.AlignmentY == cui::drawing::ImageBrushAlignmentY::Top
				? "top" : brush.AlignmentY == cui::drawing::ImageBrushAlignmentY::Bottom
					? "bottom" : "center";
		}
		else
		{
			if (brush.Kind == cui::drawing::BrushKind::LinearGradient)
			{
				value["startX"] = brush.StartPoint.x;
				value["startY"] = brush.StartPoint.y;
				value["endX"] = brush.EndPoint.x;
				value["endY"] = brush.EndPoint.y;
			}
			else
			{
				value["centerX"] = brush.Center.x;
				value["centerY"] = brush.Center.y;
				value["originX"] = brush.GradientOrigin.x;
				value["originY"] = brush.GradientOrigin.y;
				value["radiusX"] = brush.RadiusX;
				value["radiusY"] = brush.RadiusY;
			}
			DesignValue stops = DesignValue::array();
			for (const auto& stop : brush.GradientStops)
				stops.push_back(DesignValue{
					{ "offset", stop.Offset }, { "color", ColorToValue(stop.Color) } });
			value["stops"] = std::move(stops);
		}
		return value;
	}

	static DesignValue TransformToValue(const cui::drawing::Transform& transform)
	{
		DesignValue result = DesignValue::array();
		for (const auto& operation : transform.Operations)
		{
			DesignValue value = DesignValue::object();
			switch (operation.Kind)
			{
			case cui::drawing::TransformKind::Matrix:
				value["type"] = "matrix";
				value["m11"] = operation.Matrix._11;
				value["m12"] = operation.Matrix._12;
				value["m21"] = operation.Matrix._21;
				value["m22"] = operation.Matrix._22;
				value["dx"] = operation.Matrix._31;
				value["dy"] = operation.Matrix._32;
				break;
			case cui::drawing::TransformKind::Translate:
				value["type"] = "translate";
				value["x"] = operation.X;
				value["y"] = operation.Y;
				break;
			case cui::drawing::TransformKind::Scale:
				value["type"] = "scale";
				value["scaleX"] = operation.ScaleX;
				value["scaleY"] = operation.ScaleY;
				break;
			case cui::drawing::TransformKind::Rotate:
				value["type"] = "rotate";
				value["angle"] = operation.Angle;
				break;
			case cui::drawing::TransformKind::Skew:
				value["type"] = "skew";
				value["angleX"] = operation.AngleX;
				value["angleY"] = operation.AngleY;
				break;
			}
			if (operation.Kind == cui::drawing::TransformKind::Scale
				|| operation.Kind == cui::drawing::TransformKind::Rotate
				|| operation.Kind == cui::drawing::TransformKind::Skew)
			{
				value["centerX"] = operation.CenterX;
				value["centerY"] = operation.CenterY;
			}
			result.push_back(std::move(value));
		}
		return result;
	}

	static DesignValue GeometryToValue(const cui::drawing::Geometry& geometry)
	{
		DesignValue value = DesignValue::object();
		auto finish = [&]()
		{
			if (geometry.LocalTransform)
				value["transform"] = TransformToValue(*geometry.LocalTransform);
			return value;
		};
		if (geometry.Kind == cui::drawing::GeometryKind::Rectangle)
		{
			value["type"] = "rectangle";
			value["x"] = geometry.Rect.left;
			value["y"] = geometry.Rect.top;
			value["width"] = geometry.Rect.right - geometry.Rect.left;
			value["height"] = geometry.Rect.bottom - geometry.Rect.top;
			value["radiusX"] = geometry.RadiusX;
			value["radiusY"] = geometry.RadiusY;
			return finish();
		}
		if (geometry.Kind == cui::drawing::GeometryKind::Ellipse)
		{
			value["type"] = "ellipse";
			value["centerX"] = geometry.Center.x;
			value["centerY"] = geometry.Center.y;
			value["radiusX"] = geometry.RadiusX;
			value["radiusY"] = geometry.RadiusY;
			return finish();
		}
		if (geometry.Kind == cui::drawing::GeometryKind::Path)
		{
			value["type"] = "path";
			value["fillRule"] = geometry.FillRule
				== cui::drawing::GeometryFillRule::Nonzero ? "nonzero" : "evenodd";
			DesignValue figures = DesignValue::array();
			for (const auto& figure : geometry.Figures)
			{
				DesignValue figureValue{
					{ "startX", figure.StartPoint.x },
					{ "startY", figure.StartPoint.y },
					{ "closed", figure.IsClosed },
					{ "filled", figure.IsFilled },
					{ "segments", DesignValue::array() } };
				for (const auto& segment : figure.Segments)
				{
					DesignValue segmentValue = DesignValue::object();
					if (segment.Kind == cui::drawing::PathSegmentKind::Line)
					{
						segmentValue["type"] = "line";
						segmentValue["x"] = segment.Point.x;
						segmentValue["y"] = segment.Point.y;
					}
					else if (segment.Kind == cui::drawing::PathSegmentKind::Bezier)
					{
						segmentValue["type"] = "bezier";
						segmentValue["x1"] = segment.Point1.x;
						segmentValue["y1"] = segment.Point1.y;
						segmentValue["x2"] = segment.Point2.x;
						segmentValue["y2"] = segment.Point2.y;
						segmentValue["x3"] = segment.Point3.x;
						segmentValue["y3"] = segment.Point3.y;
					}
					else if (segment.Kind == cui::drawing::PathSegmentKind::QuadraticBezier)
					{
						segmentValue["type"] = "quadratic";
						segmentValue["x1"] = segment.Point1.x;
						segmentValue["y1"] = segment.Point1.y;
						segmentValue["x2"] = segment.Point2.x;
						segmentValue["y2"] = segment.Point2.y;
					}
					else
					{
						segmentValue["type"] = "arc";
						segmentValue["x"] = segment.Point.x;
						segmentValue["y"] = segment.Point.y;
						segmentValue["width"] = segment.Size.width;
						segmentValue["height"] = segment.Size.height;
						segmentValue["rotation"] = segment.RotationAngle;
						segmentValue["large"] = segment.IsLargeArc;
						segmentValue["sweep"] = segment.Sweep
							== cui::drawing::SweepDirection::Clockwise
							? "clockwise" : "counterclockwise";
					}
					figureValue["segments"].push_back(std::move(segmentValue));
				}
				figures.push_back(std::move(figureValue));
			}
			value["figures"] = std::move(figures);
			return finish();
		}
		value["type"] = "group";
		value["fillRule"] = geometry.FillRule
			== cui::drawing::GeometryFillRule::Nonzero ? "nonzero" : "evenodd";
		DesignValue children = DesignValue::array();
		for (const auto& child : geometry.Children)
			children.push_back(GeometryToValue(child));
		value["children"] = std::move(children);
		return finish();
	}

	static D2D1_COLOR_F ColorFromValue(const DesignValue& j, const D2D1_COLOR_F& def)
	{
		D2D1_COLOR_F c = def;
		if (j.is_object())
		{
			c.r = j.value("r", def.r);
			c.g = j.value("g", def.g);
			c.b = j.value("b", def.b);
			c.a = j.value("a", def.a);
		}
		return c;
	}
	static std::wstring ColorToMetadataText(const D2D1_COLOR_F& color)
	{
		auto byte = [](float value) -> unsigned int
		{
			return static_cast<unsigned int>(std::lround(
				(std::clamp)(value, 0.0f, 1.0f) * 255.0f));
		};
		wchar_t text[10]{};
		swprintf_s(text, L"#%02X%02X%02X%02X",
			byte(color.a), byte(color.r), byte(color.g), byte(color.b));
		return text;
	}

	static DesignValue ThicknessToValue(const Thickness& t)
	{
		return DesignValue{ {"l", t.Left}, {"t", t.Top}, {"r", t.Right}, {"b", t.Bottom} };
	}
	static Thickness ThicknessFromValue(const DesignValue& j, const Thickness& def)
	{
		Thickness t = def;
		if (j.is_object())
		{
			t.Left = j.value("l", def.Left);
			t.Top = j.value("t", def.Top);
			t.Right = j.value("r", def.Right);
			t.Bottom = j.value("b", def.Bottom);
		}
		return t;
	}

	static std::string HorizontalAlignmentToString(HorizontalAlignment a)
	{
		switch (a)
		{
		case HorizontalAlignment::Left: return "Left";
		case HorizontalAlignment::Center: return "Center";
		case HorizontalAlignment::Right: return "Right";
		case HorizontalAlignment::Stretch: return "Stretch";
		default: return "Left";
		}
	}
	static bool TryParseHorizontalAlignment(const std::string& s, HorizontalAlignment& out)
	{
		if (s == "Left") { out = HorizontalAlignment::Left; return true; }
		if (s == "Center") { out = HorizontalAlignment::Center; return true; }
		if (s == "Right") { out = HorizontalAlignment::Right; return true; }
		if (s == "Stretch") { out = HorizontalAlignment::Stretch; return true; }
		return false;
	}
	static std::string VerticalAlignmentToString(VerticalAlignment a)
	{
		switch (a)
		{
		case VerticalAlignment::Top: return "Top";
		case VerticalAlignment::Center: return "Center";
		case VerticalAlignment::Bottom: return "Bottom";
		case VerticalAlignment::Stretch: return "Stretch";
		default: return "Top";
		}
	}
	static bool TryParseVerticalAlignment(const std::string& s, VerticalAlignment& out)
	{
		if (s == "Top") { out = VerticalAlignment::Top; return true; }
		if (s == "Center") { out = VerticalAlignment::Center; return true; }
		if (s == "Bottom") { out = VerticalAlignment::Bottom; return true; }
		if (s == "Stretch") { out = VerticalAlignment::Stretch; return true; }
		return false;
	}
	static std::string DockToString(Dock d)
	{
		switch (d)
		{
		case Dock::Left: return "Left";
		case Dock::Top: return "Top";
		case Dock::Right: return "Right";
		case Dock::Bottom: return "Bottom";
		default: return "Left";
		}
	}
	static bool TryParseDock(const std::string& s, Dock& out)
	{
		if (s == "Left") { out = Dock::Left; return true; }
		if (s == "Top") { out = Dock::Top; return true; }
		if (s == "Right") { out = Dock::Right; return true; }
		if (s == "Bottom") { out = Dock::Bottom; return true; }
		return false;
	}
	static std::string OrientationToString(Orientation o)
	{
		switch (o)
		{
		case Orientation::Horizontal: return "Horizontal";
		case Orientation::Vertical: return "Vertical";
		default: return "Vertical";
		}
	}
	static bool TryParseOrientation(const std::string& s, Orientation& out)
	{
		if (s == "Horizontal") { out = Orientation::Horizontal; return true; }
		if (s == "Vertical") { out = Orientation::Vertical; return true; }
		return false;
	}

	static std::string SizeUnitToString(SizeUnit u)
	{
		switch (u)
		{
		case SizeUnit::Pixel: return "Pixel";
		case SizeUnit::Auto: return "Auto";
		case SizeUnit::Star: return "Star";
		default: return "Pixel";
		}
	}
	static bool TryParseSizeUnit(const std::string& s, SizeUnit& out)
	{
		if (s == "Pixel") { out = SizeUnit::Pixel; return true; }
		if (s == "Auto") { out = SizeUnit::Auto; return true; }
		if (s == "Star") { out = SizeUnit::Star; return true; }
		return false;
	}
	static DesignValue GridLengthToValue(const GridLength& gl)
	{
		return DesignValue{ {"value", gl.Value}, {"unit", SizeUnitToString(gl.Unit)} };
	}
	static GridLength GridLengthFromValue(const DesignValue& j, const GridLength& def)
	{
		GridLength gl = def;
		if (!j.is_object()) return gl;
		gl.Value = j.value("value", def.Value);
		SizeUnit u = def.Unit;
		if (j.contains("unit") && j["unit"].is_string())
		{
			TryParseSizeUnit(j["unit"].get<std::string>(), u);
		}
		gl.Unit = u;
		return gl;
	}

	static int GetVisualChildIndex(Control* parent, Control* child)
	{
		if (!parent || !child) return -1;
		for (int i = 0; i < parent->VisualChildCount(); i++)
		{
			if (parent->GetVisualChild(i) == child) return i;
		}
		return -1;
	}

	static int ParseTrailingIntOrZero(const std::wstring& s)
	{
		int i = (int)s.size() - 1;
		while (i >= 0 && iswdigit(s[(size_t)i])) i--;
		if (i == (int)s.size() - 1) return 0;
		try
		{
			return std::stoi(s.substr((size_t)i + 1));
		}
		catch (...) { return 0; }
	}

	static bool StartsWith(const std::wstring& s, const std::wstring& prefix)
	{
		if (s.size() < prefix.size()) return false;
		return s.compare(0, prefix.size(), prefix) == 0;
	}

	static bool TryParseNumericSuffix(const std::wstring& name, const std::wstring& prefix, int& outSuffix)
	{
		outSuffix = 0;
		if (!StartsWith(name, prefix)) return false;
		std::wstring rest = name.substr(prefix.size());
		if (rest.empty()) return false;
		for (wchar_t ch : rest)
		{
			if (!iswdigit(ch)) return false;
		}
		try
		{
			outSuffix = std::stoi(rest);
			return outSuffix > 0;
		}
		catch (...) { return false; }
	}
}

std::wstring DesignerCanvas::MakeUniqueControlName(const std::shared_ptr<DesignerControl>& target, const std::wstring& desired) const
{
	std::wstring base = TrimWs(desired);
	if (base.empty()) base = L"Control";

	auto isUsed = [&](const std::wstring& n) -> bool
	{
		for (auto& dc : _designerControls)
		{
			if (!dc) continue;
			if (dc == target) continue;
			if (dc->Name == n) return true;
		}
		return false;
	};

	if (!isUsed(base)) return base;

	int suffix = 2;
	while (suffix < 1000000)
	{
		std::wstring candidate = base + std::to_wstring(suffix);
		if (!isUsed(candidate)) return candidate;
		suffix++;
	}
	// 极端情况下兜底：保持可用但不保证美观
	return base + L"_";
}

std::wstring DesignerCanvas::GenerateDefaultControlName(UIClass type, const std::wstring& typeName)
{
	std::wstring base = typeName;
	if (base.empty()) base = L"Control";

	int maxExisting = 0;
	for (auto& dc : _designerControls)
	{
		if (!dc) continue;
		if (dc->Type != type) continue;
		int suf = 0;
		if (TryParseNumericSuffix(dc->Name, base, suf))
			maxExisting = (std::max)(maxExisting, suf);
	}

	int& counter = _controlTypeCounters[(int)type];
	counter = (std::max)(counter, maxExisting);

	auto isUsed = [&](const std::wstring& n) -> bool
	{
		for (auto& dc : _designerControls)
		{
			if (!dc) continue;
			if (dc->Name == n) return true;
		}
		return false;
	};

	for (int guard = 0; guard < 1000000; guard++)
	{
		counter++;
		std::wstring candidate = base + std::to_wstring(counter);
		if (!isUsed(candidate)) return candidate;
	}

	return base + L"_";
}

void DesignerCanvas::UpdateDefaultNameCounterFromName(UIClass type, const std::wstring& name)
{
	std::wstring base = ExportTypeName(type);
	if (base.empty()) base = L"Control";
	int suf = 0;
	if (!TryParseNumericSuffix(name, base, suf)) return;
	int& counter = _controlTypeCounters[(int)type];
	counter = (std::max)(counter, suf);
}

int DesignerCanvas::AllocateStableControlId()
{
	if (_nextStableControlId < 1
		|| _nextStableControlId == (std::numeric_limits<int>::max)())
	{
		throw std::overflow_error("Designer stable control id space exhausted");
	}
	return _nextStableControlId++;
}

std::shared_ptr<DesignerControl>
DesignerCanvas::GetDocumentContentRootRecord() const
{
	std::shared_ptr<DesignerControl> result;
	for (const auto& control : _designerControls)
	{
		if (!control || !control->ControlInstance
			|| control->DesignerParent) continue;
		if (result) return nullptr;
		result = control;
	}
	return result;
}

Control* DesignerCanvas::GetDocumentContentRoot() const
{
	const auto root = GetDocumentContentRootRecord();
	return root ? root->ControlInstance : nullptr;
}

void DesignerCanvas::CreateDefaultContentRoot()
{
	if (!_clientSurface || _clientSurface->VisualChildCount() != 0
		|| GetDocumentContentRootRecord()) return;

	const auto* canvasType = CuiRuntime::XamlRuntimeSchema::FindBuiltInType(
		CuiRuntime::XamlRuntimeSchema::CuiNamespace, L"Canvas");
	if (!canvasType)
		throw std::runtime_error("Built-in Canvas XAML schema is unavailable");

	auto owner = std::make_unique<Canvas>();
	auto* root = owner.get();
	root->Width = cui::layout::Length::Auto();
	root->Height = cui::layout::Length::Auto();
	root->HorizontalAlignment = HorizontalAlignment::Stretch;
	root->VerticalAlignment = VerticalAlignment::Stretch;
	root->BorderThickness = 0.0f;
	root->Background = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	const int rootNodeId = AllocateStableControlId();
	XamlSchemaContext schemaContext;
	std::wstring schemaError;
	if (!CuiRuntime::XamlRuntimeSchema::AttachBuiltInType(
		*root, *canvasType, schemaContext, &schemaError))
		throw std::runtime_error(Convert::UnicodeToUtf8(
			schemaError.empty()
				? L"无法附加 Canvas XAML Schema。" : schemaError));

	_clientSurface->AddOwned(std::move(owner));
	root->Arrange(cui::core::Rect{
		{}, _clientSurface->GetActualSizeDip() });
	auto record = std::make_shared<DesignerControl>(
		root, L"contentRoot", UIClass::UI_Canvas, nullptr,
		rootNodeId);
	record->XamlType = canvasType->TypeId;
	_designerControls.push_back(std::move(record));
	_defaultContentRoot = root;
}

DesignerDocumentTransactionResult DesignerCanvas::CreateNewDocument()
{
	DesignerModel::DesignDocument document;
	auto result = ReplaceDesignDocument(document, L"新建文档");
	PublishCanvasCommandResult(
		L"NewDocument", L"NewDocument", result);
	return result;
}

DesignerDocumentTransactionResult DesignerCanvas::SaveDesignFile(
	const std::wstring& filePath,
	std::wstring* outError)
{
	if (outError) outError->clear();
	DesignerDocumentTransactionResult result;
	if (filePath.empty())
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"保存路径为空。");
	}
	else if (HasActiveDocumentTransaction())
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"文档事务进行中，不能保存预览状态。");
	}
	else
	{
		DesignerModel::DesignDocument document;
		std::wstring error;
		if (!BuildDesignDocument(document, &error))
		{
			result = DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"无法构建待保存文档：" + error);
		}
		else if (!(DesignerModel::DetectDesignDocumentFileFormat(filePath)
				== DesignerModel::DesignDocumentFileFormat::Xaml
			? DesignerModel::XamlDocumentSerializer::SaveToFile(
				document, filePath, &error)
			: DesignerModel::DesignDocumentSerializer::SaveToFile(
				document, filePath, &error)))
		{
			result = DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				error.empty() ? L"无法写入设计文件。" : error);
		}
		else
		{
			result = MarkDocumentSaved();
			if (result)
				result = DesignerDocumentTransactionResult::Success(
					DesignerDocumentTransactionState::Unchanged);
		}
	}
	if (!result && outError) *outError = result.Error;
	PublishCanvasCommandResult(
		L"SaveDocument", filePath, result);
	return result;
}

bool DesignerCanvas::BuildDesignDocument(DesignerModel::DesignDocument& document, std::wstring* outError) const
{
	try
	{
		document.Clear();
		document.ResourceBasePath = _documentResourceBasePath;
		document.Resources = _documentResources;
		document.NextStableId = _nextStableControlId;
		document.Window = CaptureDesignedWindowNode();
		document.CodeBehind = _codeBehind;
		document.DataContextSchema = _dataContextSchema;
		DesignerDataContextSchemaUtils::Canonicalize(document.DataContextSchema);
		if (!DesignerDataContextSchemaUtils::Validate(document.DataContextSchema, outError))
			return false;
		document.StyleSheet = _documentStyleSheet;
		document.Components = _componentDefinitions;
		document.ControlTemplates = _controlTemplates;
		document.DataTypes = _dataTypes;
		document.DataTemplates = _dataTemplates;
		document.ItemsPanelTemplates = _itemsPanelTemplates;
		document.GroupStyles = _groupStyles;
		document.DataLists = _dataLists;
		document.CollectionViews = _collectionViews;
		for (const auto& property : document.DataContextSchema)
			if (property.ObjectKind == DesignerDataObjectKind::BindingList
				&& !document.FindDataType(property.ItemType))
			{
				if (outError) *outError = L"集合属性 " + property.Path
					+ L" 引用了未声明的 DataType：" + property.ItemType;
				return false;
			}
		DesignerStyleSheetUtils::Canonicalize(document.StyleSheet);
		if (!DesignerStyleSheetUtils::Validate(
			document.StyleSheet, outError, document.ResourceBasePath,
			document.Resources))
			return false;

		// 防御：Name 与稳定 ID 都必须唯一，且 ID 高水位不可回退。
		{
			std::unordered_set<std::wstring> used;
			std::unordered_set<int> usedIds;
			used.reserve(_designerControls.size());
			usedIds.reserve(_designerControls.size());
			int maxStableId = 0;
			for (auto& dc : _designerControls)
			{
				if (!dc) continue;
				if (dc->Name.empty())
				{
					if (outError) *outError = L"存在空的控件 Name，请先为控件命名。";
					return false;
				}
				if (used.find(dc->Name) != used.end())
				{
					if (outError) *outError = L"存在重复的控件 Name: " + dc->Name;
					return false;
				}
				used.insert(dc->Name);
				if (dc->StableId < 1 || !usedIds.insert(dc->StableId).second)
				{
					if (outError) *outError = L"控件稳定 ID 无效或重复: " + dc->Name;
					return false;
				}
				maxStableId = (std::max)(maxStableId, dc->StableId);
			}
			if (document.NextStableId <= maxStableId)
			{
				if (outError) *outError = L"控件稳定 ID 高水位无效。";
				return false;
			}
		}

		// Window.Content is singular. Designer chrome is never serialized as a
		// second root, and malformed transient topology is rejected here rather
		// than normalized into a compatibility wrapper.
		size_t contentRootCount = 0;
		for (const auto& control : _designerControls)
		{
			if (!control || !control->ControlInstance
				|| control->DesignerParent) continue;
			++contentRootCount;
			if (!_clientSurface
				|| control->ControlInstance->GetVisualParent() != _clientSurface)
			{
				if (outError) *outError = L"Window.Content 的设计父级与运行时父级不一致。";
				return false;
			}
		}
		if (contentRootCount > 1)
		{
			if (outError) *outError = L"Window 只能包含一个 Content 根元素。";
			return false;
		}

		// Control* -> persisted identity
		std::unordered_map<Control*, std::wstring> nameOf;
		std::unordered_map<Control*, int> idOf;
		nameOf.reserve(_designerControls.size());
		idOf.reserve(_designerControls.size());
		for (auto& dc : _designerControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			nameOf[dc->ControlInstance] = dc->Name;
			idOf[dc->ControlInstance] = dc->StableId;
		}

		for (auto& dc : _designerControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			auto* c = dc->ControlInstance;

			DesignerModel::DesignNode node;
			node.Id = dc->StableId;
			node.Name = dc->Name;
			node.NameIsGenerated = dc->NameIsGenerated;
			node.Type = dc->Type;
			node.XamlType = dc->XamlType;
			node.ComponentType = dc->ComponentType;
			node.ComponentContentProperty = dc->ComponentContentProperty;
			node.Locked = dc->IsLocked;
			if (dc->LocalResources)
				node.LocalResources = *dc->LocalResources;
			if (dc->LocalObjectResources)
				node.LocalObjectResources = *dc->LocalObjectResources;

			// parent reference
			if (!dc->DesignerParent)
			{
				node.ParentRef.clear();
			}
			else
			{
				auto itName = nameOf.find(dc->DesignerParent);
				if (itName != nameOf.end())
				{
					node.ParentRef = itName->second;
					auto itId = idOf.find(dc->DesignerParent);
					if (itId != idOf.end()) node.ParentId = itId->second;
				}
				else
				{
					if (outError) *outError = L"控件的设计父级缺少 authored 身份："
						+ dc->Name;
					return false;
				}
			}
			if (node.ParentId > 0)
			{
				const auto parentRecord = std::find_if(
					_designerControls.begin(), _designerControls.end(),
					[&](const auto& candidate)
					{
						return candidate && candidate->StableId == node.ParentId;
					});
				if (parentRecord != _designerControls.end()
					&& !(*parentRecord)->ComponentType.Empty())
				{
					const auto contract = std::find_if(
						(*parentRecord)->ComponentContentProperties.begin(),
						(*parentRecord)->ComponentContentProperties.end(),
						[&](const auto& property)
						{
							return property.Name
								== node.ComponentContentProperty;
						});
					if (contract ==
						(*parentRecord)->ComponentContentProperties.end())
					{
						const auto defaultContent = std::find_if(
							(*parentRecord)->ComponentContentProperties.begin(),
							(*parentRecord)->ComponentContentProperties.end(),
							[](const auto& property) { return property.IsDefault; });
						if (defaultContent ==
							(*parentRecord)->ComponentContentProperties.end())
						{
							if (outError) *outError = L"组件子控件缺少有效视觉内容属性："
								+ node.Name;
							return false;
						}
						node.ComponentContentProperty = defaultContent->Name;
					}
				}
				else node.ComponentContentProperty.clear();
			}
			else node.ComponentContentProperty.clear();

			Control* runtimeParent = c->GetVisualParent() ? c->GetVisualParent() : (dc->DesignerParent ? dc->DesignerParent : (_clientSurface ? (Control*)_clientSurface : (Control*)_designSurface));
			node.Order = GetVisualChildIndex(runtimeParent, c);

			node.Properties = {};
			// ToolBar and Theme templates may install a captured framework Style
			// resource at runtime. It is derived container/template state, not an
			// authored Style attribute, so a Designer round-trip must not persist it.
			if (!cui::framework::StyleAccess::ResourceKeyCapturedFromTheme(*c)
				&& !cui::framework::StyleAccess::ResourceKeyIsAutomatic(*c))
				node.Properties.StyleResourceKey =
					cui::framework::StyleAccess::ResourceKey(*c);
			auto hasAuthoredEntry = [&](const std::wstring& propertyName)
			{
				const auto matches = [&](const auto& values)
				{
					return std::any_of(values.begin(), values.end(),
						[&](const auto& entry)
						{
							return entry.first == propertyName;
						});
				};
				return matches(dc->MetadataProperties)
					|| matches(dc->MetadataPropertyResourceKeys)
					|| matches(dc->MetadataPropertyDynamicResourceKeys);
			};
			auto hasAuthoredBinding = [&](const std::wstring& propertyName)
			{
				return std::any_of(dc->DataBindings.begin(), dc->DataBindings.end(),
					[&](const auto& entry)
					{
						return entry.first == propertyName;
					});
			};
			auto shouldCaptureAuthoredProperty =
				[&](const std::wstring& propertyName)
			{
				if (hasAuthoredBinding(propertyName)) return false;
				if (hasAuthoredEntry(propertyName)) return true;
				return c->HasPropertyValue(propertyName,
						DependencyPropertyValueSource::Local)
					&& c->GetPropertyExpressionKind(propertyName,
						DependencyPropertyValueSource::Local)
						== DependencyPropertyExpressionKind::None;
			};

			auto captureCanonicalProperty = [&](const wchar_t* propertyName)
			{
				std::wstring canonicalName;
				DesignerStyleValue currentValue;
				std::wstring metadataError;
				if (!DesignerPropertyCatalog::CaptureValue(
					*c, propertyName, &canonicalName, currentValue,
					&metadataError))
				{
					if (outError) *outError = L"控件 " + dc->Name
						+ L"：" + metadataError;
					return false;
				}
				DesignerModel::DesignPropertyAssignment assignment;
				assignment.Value = std::move(currentValue);
				if (const auto resource =
					dc->MetadataPropertyResourceKeys.find(canonicalName);
					resource != dc->MetadataPropertyResourceKeys.end())
					assignment.ResourceKey = resource->second;
				if (const auto dynamicResource =
					dc->MetadataPropertyDynamicResourceKeys.find(canonicalName);
					dynamicResource
						!= dc->MetadataPropertyDynamicResourceKeys.end())
					assignment.DynamicResourceKey = dynamicResource->second;
				node.Properties.Set(
					std::move(canonicalName), std::move(assignment));
				return true;
			};

			const wchar_t* capturedProperties[]{
				L"Text", L"Width", L"Height", L"IsEnabled", L"Visibility",
				L"Background", L"Foreground", L"BorderBrush",
				L"AutomationProperties.FullDescription",
				L"Margin", L"Padding", L"HorizontalAlignment", L"VerticalAlignment", L"DockPanel.Dock",
				L"ZIndex", L"Grid.Row", L"Grid.Column", L"Grid.RowSpan",
				L"Grid.ColumnSpan"
			};
			for (const auto* propertyName : capturedProperties)
				if (shouldCaptureAuthoredProperty(propertyName)
					&& !captureCanonicalProperty(propertyName)) return false;
			auto captureScalarXamlMember = [&](
				const wchar_t* propertyName,
				const BindingValue& value)
			{
				// Visual/object content belongs to the document tree. Attribute
				// literals remain native XAML members and must round-trip even
				// though these object slots stay hidden from the scalar grid.
				return value.Kind() == BindingValueKind::Object
					|| !shouldCaptureAuthoredProperty(propertyName)
					|| captureCanonicalProperty(propertyName);
			};
			if (auto* presenter = dynamic_cast<ContentPresenter*>(c);
				presenter
				&& !captureScalarXamlMember(
					L"Content", presenter->GetContent()))
				return false;
			if (auto* content = dynamic_cast<ContentControl*>(c);
				content
				&& !captureScalarXamlMember(
					L"Content", content->GetContent()))
				return false;
			if (auto* headered =
				dynamic_cast<HeaderedContentControl*>(c);
				headered
				&& !captureScalarXamlMember(
					L"Header", headered->GetHeader()))
				return false;
			if (auto* headered =
				dynamic_cast<HeaderedItemsControl*>(c);
				headered
				&& !captureScalarXamlMember(
					L"Header", headered->GetHeader()))
				return false;
			if (dc->Type == UIClass::UI_Image
				&& ((shouldCaptureAuthoredProperty(L"Source")
						&& !captureCanonicalProperty(L"Source"))
					|| (shouldCaptureAuthoredProperty(L"Stretch")
						&& !captureCanonicalProperty(L"Stretch")))) return false;

			// Canvas attached coordinates belong only to an authored Canvas
			// layout parent. ContentPresenter projections, content slots, template
			// hosts, Split regions, and other non-linear containers are not an
			// implicit absolute-positioning fallback. An unset coordinate is NaN by
			// design and must remain absent from the canonical XAML.
			const bool canvasPlacement = dc->DesignerParent
				&& dc->DesignerParent->Type() == UIClass::UI_Canvas;
			auto captureCanvasCoordinate = [&](const wchar_t* propertyName)
			{
				return !shouldCaptureAuthoredProperty(propertyName)
					|| captureCanonicalProperty(propertyName);
			};
			if (canvasPlacement
				&& (!captureCanvasCoordinate(L"Canvas.Left")
					|| !captureCanvasCoordinate(L"Canvas.Top"))) return false;

			auto authoredProperties = dc->MetadataProperties;
			for (const auto& [propertyName, ignored]
				: dc->MetadataPropertyResourceKeys)
			{
				(void)ignored;
				authoredProperties.try_emplace(propertyName);
			}
			for (const auto& [propertyName, ignored]
				: dc->MetadataPropertyDynamicResourceKeys)
			{
				(void)ignored;
				authoredProperties.try_emplace(propertyName);
			}
			for (const auto& [propertyName, ignored] : authoredProperties)
			{
				(void)ignored;
				if (!captureCanonicalProperty(propertyName.c_str())) return false;
			}
			DesignValue extra = DesignValue::object();
			if (auto* relativeParent = dynamic_cast<RelativePanel*>(
				dc->DesignerParent))
			{
				if (const auto* constraints = relativeParent->GetConstraints(c))
				{
					DesignValue relative = DesignValue::object();
					if (constraints->CenterHorizontal)
						relative["centerHorizontal"] = true;
					if (constraints->CenterVertical)
						relative["centerVertical"] = true;
					if (constraints->AlignLeftWithPanel)
						relative["alignLeftWithPanel"] = true;
					if (constraints->AlignTopWithPanel)
						relative["alignTopWithPanel"] = true;
					if (constraints->AlignRightWithPanel)
						relative["alignRightWithPanel"] = true;
					if (constraints->AlignBottomWithPanel)
						relative["alignBottomWithPanel"] = true;
					auto storeTarget = [&](const char* key, Control* target)
					{
						if (!target) return true;
						const auto found = nameOf.find(target);
						if (found == nameOf.end())
						{
							if (outError) *outError = L"控件 " + dc->Name
								+ L" 的 RelativePanel 约束目标不在当前设计文档中。";
							return false;
						}
						relative[key] = ToUtf8(found->second);
						return true;
					};
					if (!storeTarget("alignLeftWith", constraints->AlignLeftWith)
						|| !storeTarget("alignRightWith", constraints->AlignRightWith)
						|| !storeTarget("alignTopWith", constraints->AlignTopWith)
						|| !storeTarget("alignBottomWith", constraints->AlignBottomWith)
						|| !storeTarget("leftOf", constraints->LeftOf)
						|| !storeTarget("rightOf", constraints->RightOf)
						|| !storeTarget("above", constraints->Above)
						|| !storeTarget("below", constraints->Below)) return false;
					if (!relative.empty())
						extra["relativePanelConstraints"] = std::move(relative);
				}
			}
			if (dc->Type == UIClass::UI_ChartView)
			{
				auto* chart = static_cast<ChartView*>(c);
				DesignValue seriesValues = DesignValue::array();
				for (const auto& series : chart->GetSeries())
				{
					DesignValue seriesValue{
						{ "name", ToUtf8(series.Name) },
						{ "visible", series.Visible } };
					if (series.Color.a > 0.0f)
						seriesValue["color"] = ColorToValue(series.Color);
					DesignValue points = DesignValue::array();
					for (const auto& point : series.Points)
					{
						DesignValue pointValue{
							{ "label", ToUtf8(point.Label) },
							{ "value", point.Value },
							{ "tag", point.Tag },
							{ "useCustomColor", point.UseCustomColor } };
						if (point.UseCustomColor)
							pointValue["color"] = ColorToValue(point.Color);
						points.push_back(std::move(pointValue));
					}
					seriesValue["points"] = std::move(points);
					seriesValues.push_back(std::move(seriesValue));
				}
				if (!seriesValues.empty()) extra["series"] = std::move(seriesValues);
			}
			else if (dc->Type == UIClass::UI_Grid)
			{
				auto* gridPanel = (Grid*)c;
				DesignValue rows = DesignValue::array();
				for (auto& r : gridPanel->GetRows())
				{
					rows.push_back(DesignValue{
						{"height", GridLengthToValue(r.Height)},
						{"min", r.MinHeight},
						{"max", r.MaxHeight}
					});
				}
				DesignValue cols = DesignValue::array();
				for (auto& col : gridPanel->GetColumns())
				{
					cols.push_back(DesignValue{
						{"width", GridLengthToValue(col.Width)},
						{"min", col.MinWidth},
						{"max", col.MaxWidth}
					});
				}
				extra["rows"] = rows;
				extra["columns"] = cols;
			}
			auto shouldCaptureLocalObject = [&](const wchar_t* propertyName)
			{
				return shouldCaptureAuthoredProperty(propertyName);
			};
			if (shouldCaptureLocalObject(L"Foreground")
				&& !captureCanonicalProperty(L"Foreground")) return false;
			if (const auto& clip = c->GetClip();
				clip && shouldCaptureLocalObject(L"Clip"))
				if (!captureCanonicalProperty(L"Clip")) return false;
			if (const auto& transform = c->GetRenderTransform();
				transform && shouldCaptureLocalObject(L"RenderTransform"))
				if (!captureCanonicalProperty(L"RenderTransform")) return false;
			const auto transformOrigin = c->GetRenderTransformOrigin();
			if ((transformOrigin.x != 0.0f || transformOrigin.y != 0.0f)
				&& shouldCaptureLocalObject(L"RenderTransformOrigin"))
				if (!captureCanonicalProperty(L"RenderTransformOrigin")) return false;
			if (const auto itemTemplate = dc->DesignStrings.find(L"itemTemplate");
				itemTemplate != dc->DesignStrings.end()
				&& !itemTemplate->second.empty())
				extra["itemTemplate"] = ToUtf8(itemTemplate->second);
			if (const auto controlTemplate = dc->DesignStrings.find(
				L"controlTemplate");
				controlTemplate != dc->DesignStrings.end()
				&& !controlTemplate->second.empty())
				extra["controlTemplate"] = ToUtf8(controlTemplate->second);
			if (const auto contentTemplate = dc->DesignStrings.find(
				L"contentTemplate");
				contentTemplate != dc->DesignStrings.end()
				&& !contentTemplate->second.empty())
				extra["contentTemplate"] = ToUtf8(contentTemplate->second);
			if (const auto headerTemplate = dc->DesignStrings.find(
				L"headerTemplate");
				headerTemplate != dc->DesignStrings.end()
				&& !headerTemplate->second.empty())
				extra["headerTemplate"] = ToUtf8(headerTemplate->second);
			if (const auto headerRegion = dc->DesignStrings.find(
				L"headeredRegion");
				headerRegion != dc->DesignStrings.end()
				&& headerRegion->second == L"header")
				extra["headeredRegion"] = "header";
			else if (auto* headered = dynamic_cast<HeaderedContentControl*>(runtimeParent);
				headered && headered->GetVisualHeader() == c)
				extra["headeredRegion"] = "header";
			if (const auto itemsSource = dc->DesignStrings.find(
				L"itemsSourceResource");
				itemsSource != dc->DesignStrings.end()
				&& !itemsSource->second.empty())
				extra["itemsSourceResource"] = ToUtf8(itemsSource->second);
			if (const auto containerStyle = dc->DesignStrings.find(
				L"itemContainerStyle");
				containerStyle != dc->DesignStrings.end()
				&& !containerStyle->second.empty())
				extra["itemContainerStyle"] = ToUtf8(containerStyle->second);
			if (const auto itemsPanel = dc->DesignStrings.find(L"itemsPanel");
				itemsPanel != dc->DesignStrings.end()
				&& !itemsPanel->second.empty())
				extra["itemsPanel"] = ToUtf8(itemsPanel->second);

			std::wstring structureError;
			if (!DecodeDesignNodeStructure(
				node.Type, extra, node.Structure, &structureError))
			{
				if (outError) *outError = L"控件 " + dc->Name
					+ L" 的结构状态无法捕获：" + structureError;
				return false;
			}
			if (dc->AuthoredRichTextDocument)
				node.Structure.Document = *dc->AuthoredRichTextDocument;
			node.Structure.CommandTarget = dc->AuthoredCommandTarget;

			// Event values are explicit C++ member-function names.
			if (!dc->EventHandlers.empty())
			{
				for (const auto& kv : dc->EventHandlers)
				{
					if (kv.first.empty()) continue;
					const auto handlerName =
						DesignerEventCatalog::NormalizeHandlerName(kv.second);
					if (handlerName.empty()) continue;
					node.Events[kv.first] = handlerName;
				}
			}

			if (!dc->DataBindings.empty())
			{
				for (const auto& [targetProperty, binding] : dc->DataBindings)
				{
					DesignerDataContextSchema elementSourceSchema;
					auto scopedSourceSchema = ResolveBindingSourceSchema(
						*dc, targetProperty == L"DataContext",
						_dataContextSchema);
					const DesignerDataContextSchema* sourceSchema = scopedSourceSchema
						? &*scopedSourceSchema : nullptr;
					if (!binding.ElementName.empty())
					{
						auto* source = FindControlInstanceByName(binding.ElementName);
						if (!source)
						{
							if (outError) *outError = L"控件 " + dc->Name
								+ L" 的 ElementName 引用了不存在的控件："
								+ binding.ElementName;
							return false;
						}
						elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(*source);
						sourceSchema = &elementSourceSchema;
					}
					else if (binding.RelativeSource
						== DesignerBindingRelativeSource::Self)
					{
						elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(*c);
						sourceSchema = &elementSourceSchema;
					}
					else if (binding.RelativeSource
						== DesignerBindingRelativeSource::FindAncestor)
					{
						if (auto* source = DesignerBindingUtils::FindAncestorSource(
							*c, binding))
						{
							elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(
								*source);
							sourceSchema = &elementSourceSchema;
						}
						else sourceSchema = nullptr;
					}
					if (binding.IsMultiBinding()) sourceSchema = nullptr;
					const DependencyPropertyMetadata* metadata = nullptr;
					std::wstring validationError;
					const bool valid = DesignerBindingUtils::Validate(
							*c, targetProperty, binding, &metadata,
							&validationError, sourceSchema);
					if (!valid)
					{
						if (outError) *outError = L"控件 " + dc->Name + L"：" + validationError;
						return false;
					}

					node.Bindings[metadata->Name()] = binding;
				}
			}
			document.Nodes.push_back(std::move(node));
		}

		if (!document.ValidateRichTextStructure(outError)) return false;
		if (!DesignerModel::DesignDataResourceUtils::ValidateAndCanonicalize(
			document, outError)) return false;
		if (!document.ValidateCommandTargetReferences(outError)) return false;
		return true;
	}
	catch (const std::exception& expander)
	{
		if (outError) *outError = L"保存失败: " + FromUtf8(expander.what());
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"保存失败：未知错误。";
		return false;
	}
}

bool DesignerCanvas::BuildXamlDocumentText(
	std::wstring& xamlText,
	std::wstring* outError) const
{
	try
	{
		DesignerModel::DesignDocument document;
		if (!BuildDesignDocument(document, outError)) return false;
		xamlText = Convert::Utf8ToUnicode(
			DesignerModel::XamlDocumentSerializer::ToXaml(document));
		if (outError) outError->clear();
		return true;
	}
	catch (const std::exception& exception)
	{
		if (outError)
			*outError = L"无法生成当前 CUI XAML："
				+ Convert::Utf8ToUnicode(exception.what());
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"生成当前 CUI XAML 时发生未知异常。";
		return false;
	}
}

bool DesignerCanvas::PreviewXamlDocumentText(
	const std::wstring& xamlText,
	std::wstring* outError,
	DesignerModel::XamlDocumentDiagnostic* outDiagnostic)
{
	if (outDiagnostic) *outDiagnostic = {};
	auto reportFailure = [outError, outDiagnostic](std::wstring message)
	{
		if (outError) *outError = message;
		if (outDiagnostic) outDiagnostic->Message = std::move(message);
	};
	if (!HasActiveDocumentTransaction())
	{
		reportFailure(L"实时 XAML 预览必须在文档事务中执行。");
		return false;
	}
	DesignerModel::DesignDocument candidate;
	std::wstring error;
	try
	{
		DesignerModel::XamlDocumentParseOptions parseOptions;
		parseOptions.ResourceBasePath = _documentResourceBasePath;
		if (_documentResources)
			parseOptions.Resources = std::make_shared<ResourceLoadContext>(
				_documentResources->Resolver());
		if (!DesignerModel::XamlDocumentParser::FromXaml(
			Convert::UnicodeToUtf8(xamlText), candidate, parseOptions,
			&error, outDiagnostic))
		{
			if (outError) *outError = std::move(error);
			return false;
		}
	}
	catch (const std::exception& exception)
	{
		reportFailure(L"XAML 文本转换失败："
			+ Convert::Utf8ToUnicode(exception.what()));
		return false;
	}
	catch (...)
	{
		reportFailure(L"XAML 文本转换时发生未知异常。");
		return false;
	}

	DesignerModel::DesignDocument rollback;
	if (!BuildDesignDocument(rollback, &error))
	{
		reportFailure(L"无法建立 XAML 预览恢复点：" + error);
		return false;
	}
	std::vector<int> selectionStableIds;
	selectionStableIds.reserve(_selectedControls.size());
	for (const auto& selected : _selectedControls)
		if (selected && selected->StableId > 0)
			selectionStableIds.push_back(selected->StableId);
	const int primarySelectionStableId = _selectedControl
		? _selectedControl->StableId : 0;
	auto restoreSelectionByStableId = [this, &selectionStableIds,
		primarySelectionStableId]()
	{
		std::vector<std::wstring> names;
		std::wstring primaryName;
		for (const int stableId : selectionStableIds)
			for (const auto& control : _designerControls)
				if (control && control->StableId == stableId)
				{
					names.push_back(control->Name);
					if (stableId == primarySelectionStableId)
						primaryName = control->Name;
					break;
				}
		RestoreSelectionByNames(names, primaryName, true);
	};
	if (!ApplyDesignDocument(candidate, &error, outDiagnostic))
	{
		std::wstring restoreError;
		const bool restored = ApplyDesignDocument(rollback, &restoreError);
		if (restored) restoreSelectionByStableId();
		if (outError)
		{
			*outError = error.empty()
				? L"XAML 预览无法应用。" : std::move(error);
			if (!restored)
				*outError += L" 此前预览恢复失败：" + restoreError;
		}
		if (outDiagnostic)
			outDiagnostic->Message = outError
				? *outError
				: (error.empty() ? L"XAML 预览无法应用。" : error);
		return false;
	}
	restoreSelectionByStableId();
	this->InvalidateVisual();
	if (outError) outError->clear();
	if (outDiagnostic) *outDiagnostic = {};
	return true;
}

bool DesignerCanvas::BuildEventHandlerIndex(
	DesignerModel::DesignDocumentEventIndex& index,
	std::wstring* outError) const
{
	DesignerModel::DesignDocument document;
	document.Window = CaptureDesignedWindowNode();
	document.Components = _componentDefinitions;
	document.Nodes.reserve(_designerControls.size());
	for (const auto& control : _designerControls)
	{
		if (!control) continue;
		DesignerModel::DesignNode node;
		node.Id = control->StableId;
		node.Name = control->Name;
		node.Type = control->Type;
		node.XamlType = control->XamlType;
		node.ComponentType = control->ComponentType;
		for (const auto& [eventName, handler] : control->EventHandlers)
			if (!eventName.empty() && !handler.empty())
				node.Events[eventName] = handler;
		document.Nodes.push_back(std::move(node));
	}
	return DesignerModel::DesignDocumentEventIndex::Build(
		document, index, outError);
}

DesignerDocumentTransactionResult DesignerCanvas::UpdateEventHandler(
	const std::shared_ptr<DesignerControl>& control,
	const std::wstring& eventName,
	const std::wstring& handlerName,
	std::wstring* outError)
{
	if (outError) outError->clear();
	auto fail = [&](std::wstring error)
	{
		if (outError) *outError = error;
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			std::move(error), false);
	};

	const auto normalizedHandler = TrimWs(handlerName);
	std::wstring validationError;
	if (!DesignerEventCatalog::ValidateHandlerName(
		normalizedHandler, &validationError))
		return fail(std::move(validationError));

	std::optional<DesignerEventDescriptor> descriptor;
	if (control)
	{
		const auto found = std::find(
			_designerControls.begin(), _designerControls.end(), control);
		if (found == _designerControls.end() || control->StableId <= 0)
			return fail(L"事件目标控件不属于当前设计文档。");
		descriptor = DesignerEventCatalog::FindControlEvent(
			control->Type, eventName, control->ComponentEvents);
	}
	else
	{
		descriptor = DesignerEventCatalog::FindWindowEvent(eventName);
	}
	if (!descriptor)
		return fail(L"目标不支持事件 " + eventName + L"。");

	DesignerModel::DesignDocumentEventIndex index;
	if (!BuildEventHandlerIndex(index, &validationError))
		return fail(validationError.empty()
			? L"无法建立事件处理函数索引。" : std::move(validationError));
	if (!normalizedHandler.empty())
	{
		if (const auto* existing = index.FindHandler(normalizedHandler);
			existing && existing->Signature != descriptor->Signature)
		{
			return fail(L"处理函数 “" + normalizedHandler
				+ L"” 已被不同参数签名的事件使用。请换一个函数名。");
		}
	}

	const auto& canonicalEventName = descriptor->Name;
	const auto& handlers = control
		? control->EventHandlers : _designedWindowNode.Events;
	DesignerEventHandlerValueSnapshot before;
	if (const auto found = handlers.find(canonicalEventName);
		found != handlers.end())
	{
		before.Exists = true;
		before.StoredHandler = found->second;
	}
	DesignerEventHandlerValueSnapshot after;
	after.Exists = !normalizedHandler.empty();
	after.StoredHandler = normalizedHandler;
	if (before.EquivalentTo(after))
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);

	DesignerEventHandlerDelta delta;
	delta.IsWindow = !control;
	delta.StableId = control ? control->StableId : 0;
	delta.ControlType = control ? control->Type : UIClass::UI_Base;
	delta.SubjectName = control ? control->Name : _designedWindowName;
	delta.EventName = canonicalEventName;
	delta.Before = std::move(before);
	delta.After = std::move(after);
	const auto selectionNames = CaptureSelectionNames();
	const auto primarySelectionName = _selectedControl
		? _selectedControl->Name : std::wstring{};
	auto result = ExecuteCommand(std::make_unique<EventHandlerCommand>(
		this,
		std::vector<DesignerEventHandlerDelta>{ std::move(delta) },
		selectionNames,
		primarySelectionName,
		L"UpdateProperty:" + canonicalEventName));
	if (!result && outError) *outError = result.Error;
	return result;
}

DesignerDocumentTransactionResult DesignerCanvas::UpdateEventHandlers(
	const std::vector<std::shared_ptr<DesignerControl>>& controls,
	const std::wstring& eventName,
	const std::wstring& handlerName,
	std::wstring* outError)
{
	if (outError) outError->clear();
	auto fail = [&](std::wstring error)
	{
		if (outError) *outError = error;
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			std::move(error), false);
	};
	if (controls.empty())
		return fail(L"多选事件编辑没有目标控件。");

	const auto normalizedHandler = TrimWs(handlerName);
	std::wstring validationError;
	if (!DesignerEventCatalog::ValidateHandlerName(
		normalizedHandler, &validationError))
		return fail(std::move(validationError));

	struct Target
	{
		std::shared_ptr<DesignerControl> Control;
		DesignerEventDescriptor Event;
	};
	std::vector<Target> targets;
	targets.reserve(controls.size());
	std::set<int> stableIds;
	std::optional<DesignerEventDescriptor> commonEvent;
	for (const auto& control : controls)
	{
		if (!control || control->StableId <= 0
			|| std::find(_designerControls.begin(), _designerControls.end(), control)
				== _designerControls.end())
			return fail(L"多选事件目标控件不属于当前设计文档。");
		if (!stableIds.insert(control->StableId).second)
			return fail(L"多选事件目标包含重复控件。");
		auto descriptor = DesignerEventCatalog::FindControlEvent(
			control->Type, eventName, control->ComponentEvents);
		if (!descriptor)
			return fail(L"控件 “" + control->Name + L"” 不支持事件 "
				+ eventName + L"。");
		if (!commonEvent)
			commonEvent = descriptor;
		else if (!commonEvent->SameSignature(*descriptor))
			return fail(L"事件 “" + eventName
				+ L"” 在所选控件上具有不同参数签名。");
		targets.push_back({ control, std::move(*descriptor) });
	}

	DesignerModel::DesignDocumentEventIndex index;
	if (!BuildEventHandlerIndex(index, &validationError))
		return fail(validationError.empty()
			? L"无法建立事件处理函数索引。" : std::move(validationError));
	if (!normalizedHandler.empty())
	{
		if (const auto* existing = index.FindHandler(normalizedHandler);
			existing && commonEvent
			&& existing->Signature != commonEvent->Signature)
		{
			return fail(L"处理函数 “" + normalizedHandler
				+ L"” 已被不同参数签名的事件使用。请换一个函数名。");
		}
	}

	std::vector<DesignerEventHandlerDelta> deltas;
	deltas.reserve(targets.size());
	for (const auto& target : targets)
	{
		DesignerEventHandlerValueSnapshot before;
		if (const auto found = target.Control->EventHandlers.find(
			target.Event.Name);
			found != target.Control->EventHandlers.end())
		{
			before.Exists = true;
			before.StoredHandler = found->second;
		}
		DesignerEventHandlerValueSnapshot after;
		after.Exists = !normalizedHandler.empty();
		after.StoredHandler = normalizedHandler;
		if (before.EquivalentTo(after)) continue;

		DesignerEventHandlerDelta delta;
		delta.StableId = target.Control->StableId;
		delta.ControlType = target.Control->Type;
		delta.SubjectName = target.Control->Name;
		delta.EventName = target.Event.Name;
		delta.Before = std::move(before);
		delta.After = after;
		deltas.push_back(std::move(delta));
	}
	if (deltas.empty())
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);

	const auto selectionNames = CaptureSelectionNames();
	const auto primarySelectionName = _selectedControl
		? _selectedControl->Name : std::wstring{};
	auto result = ExecuteCommand(std::make_unique<EventHandlerCommand>(
		this,
		std::move(deltas),
		selectionNames,
		primarySelectionName,
		L"UpdateProperty:" + eventName));
	if (!result && outError) *outError = result.Error;
	return result;
}

DesignerDocumentTransactionResult DesignerCanvas::RenameEventHandler(
	const std::wstring& oldName,
	const std::wstring& newName,
	size_t* outRenamedReferenceCount,
	std::wstring* outError,
	const DesignerEventHandlerCodeMigration* codeMigration)
{
	if (outRenamedReferenceCount) *outRenamedReferenceCount = 0;
	if (outError) outError->clear();
	auto fail = [&](std::wstring error)
	{
		if (outError) *outError = error;
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			std::move(error), false);
	};
	const auto from = TrimWs(oldName);
	const auto to = TrimWs(newName);
	std::wstring validationError;
	if (from.empty() || to.empty())
		return fail(L"重命名的原函数名和新函数名都不能为空。");
	if (!DesignerEventCatalog::ValidateHandlerName(from, &validationError)
		|| !DesignerEventCatalog::ValidateHandlerName(to, &validationError))
		return fail(std::move(validationError));

	DesignerModel::DesignDocumentEventIndex index;
	if (!BuildEventHandlerIndex(index, &validationError))
		return fail(validationError.empty()
			? L"无法建立事件处理函数索引。" : std::move(validationError));
	const auto* source = index.FindHandler(from);
	if (!source)
		return fail(L"文档中不存在事件处理函数 “" + from + L"”。");
	if (codeMigration)
	{
		if (!codeMigration->Enabled()
			|| codeMigration->OldName != from
			|| codeMigration->NewName != to
			|| codeMigration->ParameterList != source->ParameterList)
		{
			return fail(L"用户函数体迁移参数与当前事件处理函数不一致。");
		}
		const auto& association = GetCodeBehind();
		if (association.ClassName.empty()
			|| association.ClassName != codeMigration->ClassName)
		{
			return fail(L"用户函数体迁移目标与当前 x:Class 不一致。");
		}
		const auto headerPath = codeMigration->OutputBasePath + L".h";
		const auto sourcePath = codeMigration->OutputBasePath + L".cpp";
		if (_wcsicmp(codeMigration->UserCodePath.c_str(), headerPath.c_str()) != 0
			&& _wcsicmp(codeMigration->UserCodePath.c_str(), sourcePath.c_str()) != 0)
		{
			return fail(L"用户函数体迁移文件必须是当前 code-behind 的 .h 或 .cpp。");
		}
	}
	if (from == to)
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	if (const auto* target = index.FindHandler(to);
		target && target->Signature != source->Signature)
	{
		return fail(L"不能重命名为 “" + to
			+ L"”：该名称已用于不同参数签名。");
	}
	if (codeMigration && index.FindHandler(to))
		return fail(L"合并到已有处理函数时不能迁移用户函数体。");

	std::vector<DesignerEventHandlerDelta> deltas;
	deltas.reserve(source->ReferenceIndices.size());
	for (const auto referenceIndex : source->ReferenceIndices)
	{
		if (referenceIndex >= index.References().size())
			return fail(L"事件处理函数索引包含越界引用。");
		const auto& reference = index.References()[referenceIndex];
		DesignerEventHandlerDelta delta;
		delta.IsWindow = reference.OwnerKind
			== DesignerModel::DesignEventOwnerKind::Window;
		delta.StableId = delta.IsWindow ? 0 : reference.OwnerNodeId;
		delta.ControlType = reference.SubjectType;
		delta.SubjectName = reference.SubjectName;
		delta.EventName = reference.EventName;
		const DesignerModel::DesignEventHandlerMap* handlers = nullptr;
		if (delta.IsWindow)
		{
			handlers = &_designedWindowNode.Events;
		}
		else
		{
			const auto found = std::find_if(
				_designerControls.begin(), _designerControls.end(),
				[&](const std::shared_ptr<DesignerControl>& control)
				{
					return control
						&& control->StableId == reference.OwnerNodeId;
				});
			if (found == _designerControls.end() || !*found)
				return fail(L"重命名事件时无法按稳定 ID 找到控件："
					+ reference.SubjectName);
			handlers = &(*found)->EventHandlers;
		}
		const auto foundHandler = handlers->find(reference.EventName);
		if (foundHandler == handlers->end())
		{
			return fail(L"事件处理函数索引与当前映射不一致："
				+ reference.SubjectName + L"." + reference.EventName);
		}
		delta.Before.Exists = true;
		delta.Before.StoredHandler = foundHandler->second;
		delta.After.Exists = true;
		delta.After.StoredHandler = to;
		deltas.push_back(std::move(delta));
	}
	if (deltas.size() != source->ReferenceIndices.size())
		return fail(L"事件处理函数索引在重命名期间发生不一致。");

	const auto selectionNames = CaptureSelectionNames();
	const auto primarySelectionName = _selectedControl
		? _selectedControl->Name : std::wstring{};
	auto result = ExecuteCommand(std::make_unique<EventHandlerCommand>(
		this,
		std::move(deltas),
		selectionNames,
		primarySelectionName,
		L"RenameEventHandler",
		codeMigration ? *codeMigration : DesignerEventHandlerCodeMigration{}));
	if (result)
	{
		if (outRenamedReferenceCount)
			*outRenamedReferenceCount = source->ReferenceIndices.size();
	}
	else if (outError)
	{
		*outError = result.Error;
	}
	return result;
}

DesignerDocumentTransactionResult DesignerCanvas::LoadDesignFile(
	const std::wstring& filePath,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (filePath.empty())
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"打开路径为空。");
		if (outError) *outError = result.Error;
		PublishCanvasCommandResult(
			L"OpenDocument", filePath, result);
		return result;
	}
	DesignerModel::DesignDocument document;
	std::wstring error;
	const bool loaded = DesignerModel::DetectDesignDocumentFileFormat(filePath)
		== DesignerModel::DesignDocumentFileFormat::Xaml
		? DesignerModel::XamlDocumentParser::LoadFromFile(
			filePath, document, &error)
		: DesignerModel::DesignDocumentSerializer::LoadFromFile(
			filePath, document, &error);
	if (!loaded)
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			error.empty() ? L"无法读取设计文件。" : error);
		if (outError) *outError = result.Error;
		PublishCanvasCommandResult(
			L"OpenDocument", filePath, result);
		return result;
	}
	auto result = ReplaceDesignDocument(document, L"打开文档");
	if (!result && outError) *outError = result.Error;
	PublishCanvasCommandResult(
		L"OpenDocument", filePath, result);
	return result;
}

DesignerDocumentTransactionResult DesignerCanvas::RestoreRecoveredDocument(
	const DesignerModel::DesignDocument& document)
{
	auto result = ReplaceDesignDocument(
		document, L"恢复自动保存文档", false);
	PublishCanvasCommandResult(
		L"RestoreRecovery", L"RestoreRecovery", result);
	return result;
}

DesignerDocumentTransactionResult DesignerCanvas::ReplaceDesignDocument(
	const DesignerModel::DesignDocument& document,
	const std::wstring& operation,
	bool markAsSaved)
{
	if (HasActiveDocumentTransaction())
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"文档事务进行中，不能" + operation + L"。");

	DesignerModel::DesignDocument previousDocument;
	std::wstring captureError;
	if (!BuildDesignDocument(previousDocument, &captureError))
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法建立当前文档的恢复快照：" + captureError);
	const auto previousSelectionNames = CaptureSelectionNames();
	const auto previousPrimarySelection = _selectedControl
		? _selectedControl->Name : std::wstring{};
	const bool documentChanged = previousDocument != document;

	std::wstring applyError;
	if (ApplyDesignDocument(document, &applyError))
	{
		auto resetResult = markAsSaved
			? ResetDocumentHistoryAsSaved()
			: ResetDocumentHistoryAsUnsaved();
		if (resetResult)
			return DesignerDocumentTransactionResult::Success(
				documentChanged || !markAsSaved
					? DesignerDocumentTransactionState::Committed
					: DesignerDocumentTransactionState::Unchanged);
		applyError = L"无法重置新文档的命令历史："
			+ resetResult.Error;
	}

	std::wstring restoreError;
	const bool restored = ApplyDesignDocument(
		previousDocument, &restoreError);
	if (restored)
		RestoreSelectionByNames(
			previousSelectionNames, previousPrimarySelection, true);
	std::wstring message = operation
		+ (restored ? L"失败，已恢复此前文档："
			: L"失败，且无法恢复此前文档：")
		+ (applyError.empty() ? L"目标文档无效。" : applyError);
	if (!restored && !restoreError.empty())
		message += L" 恢复错误：" + restoreError;
	return DesignerDocumentTransactionResult::Failure(
		DesignerDocumentTransactionState::Failed,
		std::move(message), restored);
}

bool DesignerCanvas::ApplyDesignDocument(
	const DesignerModel::DesignDocument& document,
	std::wstring* outError,
	DesignerModel::XamlDocumentDiagnostic* outDiagnostic)
{
	if (!document.ValidateCommandTargetReferences(outError)) return false;
	CuiRuntime::XamlObjectTree materialized;
	CuiRuntime::XamlMaterializationOptions materializationOptions;
	materializationOptions.ControlFactory =
		[](UIClass type) { return DesignerControlFactory::Create(type); };
	materializationOptions.AllowNativeSurfacePlaceholder = true;
	if (!CuiRuntime::XamlObjectMaterializer::Materialize(
		document, materialized, materializationOptions,
		outError, outDiagnostic))
		return false;

	try
	{
		std::unordered_map<int, const DesignerModel::DesignNode*> authoredById;
		authoredById.reserve(document.Nodes.size());
		for (const auto& node : document.Nodes)
			authoredById.emplace(node.Id, &node);
		for (const auto& control : materialized.Controls)
		{
			if (!control) continue;
			const auto authored = authoredById.find(control->StableId);
			if (authored == authoredById.end())
			{
				if (outError) *outError = L"物化控件缺少 authored 节点："
					+ control->Name;
				return false;
			}
			control->AuthoredCommandTarget =
				authored->second->Structure.CommandTarget;
		}

		ClearCanvasCore();
		_documentResourceBasePath = document.ResourceBasePath;
		_documentResources = document.Resources;
		_controlTypeCounters.clear();
		if (!ApplyDesignedWindowNode(document.Window, outError)) return false;
		if (!SetCodeBehind(document.CodeBehind, outError)) return false;
		_dataContextSchema = document.DataContextSchema;
		DesignerDataContextSchemaUtils::Canonicalize(_dataContextSchema);
		_componentDefinitions = document.Components;
		_controlTemplates = document.ControlTemplates;
		_dataTypes = document.DataTypes;
		_dataTemplates = document.DataTemplates;
		_itemsPanelTemplates = document.ItemsPanelTemplates;
		_groupStyles = document.GroupStyles;
		_dataLists = document.DataLists;
		_collectionViews = document.CollectionViews;
		_nextStableControlId = document.NextStableId;

		if (!_clientSurface)
		{
			if (outError) *outError = L"设计器客户区不可用。";
			return false;
		}
		if (materialized.ContentRoot)
			_clientSurface->SetVisualContent(
				std::move(materialized.ContentRoot));

		_designerControls = std::move(materialized.Controls);
		if (_designerControls.empty())
		{
			CreateDefaultContentRoot();
		}
		else if (const auto root = GetDocumentContentRootRecord();
			root && root->Name == L"contentRoot"
			&& root->XamlType.NamespaceUri
				== CuiRuntime::XamlRuntimeSchema::CuiNamespace
			&& root->XamlType.LocalName == L"Canvas")
		{
			_defaultContentRoot = root->ControlInstance;
		}
		for (const auto& control : _designerControls)
			if (control)
				UpdateDefaultNameCounterFromName(
					control->Type, control->Name);

		if (!SetDocumentStyleSheet(
			document.StyleSheet, outError, {}, false))
			return false;
		// Preview failures are diagnostics, not document-corruption failures: the
		// persisted configuration remains editable without a compatible source.
		(void)RefreshAllDesignBindings(nullptr);

		if (_designSurface)
			RefreshDesignerPanelLayout(_designSurface);
		UpdateClientSurfaceLayout();
		for (const auto& control : _designerControls)
			if (control && control->ControlInstance)
				RefreshDesignerPanelLayout(control->ControlInstance);

		ClearSelection();
		cui::framework::EventAccess::Raise(OnControlSelected, nullptr);
		this->InvalidateVisual();
		if (outError) outError->clear();
		return true;
	}
	catch (const std::exception& exception)
	{
		if (outError) *outError = L"加载失败: " + FromUtf8(exception.what());
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"加载失败：未知错误。";
		return false;
	}
}
