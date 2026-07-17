#include "DesignerCanvas.h"
#include "CodeGenInput.h"
#include "DesignerBindingUtils.h"
#include "DesignerControlFactory.h"
#include "DesignerDataContextSchemaUtils.h"
#include "DesignerEventCatalog.h"
#include "DesignerPropertyCatalog.h"
#include "DesignerPreviewBridge.h"
#include "DesignerStyleSheetUtils.h"
#include "DesignerCore/DesignerCommandCoordinator.h"
#include "DesignerCore/Commands/ControlPlacementCommand.h"
#include "DesignerCore/Commands/ControlPropertyCommand.h"
#include "DesignerCore/Commands/ControlSubtreeCommand.h"
#include "DesignerCore/HitTestService.h"
#include "DesignerCore/LayoutBridge.h"
#include "DesignerCore/PropertyGridBinder.h"
#include "DesignerCore/SelectionService.h"
#include "DesignerModel/DesignDocument.h"
#include "DesignerModel/DesignDocumentControlPool.h"
#include "DesignerModel/DesignDocumentEventIndex.h"
#include "DesignerModel/DesignDocumentFileFormat.h"
#include "DesignerModel/DesignDocumentGraph.h"
#include "DesignerModel/DesignDocumentMaterializer.h"
#include "DesignerModel/DesignDocumentSerializer.h"
#include "DesignerModel/XamlDocumentParser.h"
#include "DesignerModel/XamlDocumentSerializer.h"
#include <Convert.h>
#include "FakeWebBrowser.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/LinkLabel.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/TextBox.h"
#include "../CUI/include/CheckBox.h"
#include "../CUI/include/RadioBox.h"
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/LoadingRing.h"
#include "../CUI/include/ProgressBar.h"
#include "../CUI/include/ProgressRing.h"
#include "../CUI/include/Slider.h"
#include "../CUI/include/NumericUpDown.h"
#include "../CUI/include/PictureBox.h"
#include "../CUI/include/DateTimePicker.h"
#include "../CUI/include/GroupBox.h"
#include "../CUI/include/Expander.h"
#include "../CUI/include/Switch.h"
#include "../CUI/include/ScrollView.h"
#include "../CUI/include/RichTextBox.h"
#include "../CUI/include/PasswordBox.h"
#include "../CUI/include/RoundTextBox.h"
#include "../CUI/include/ListView.h"
#include "../CUI/include/GridView.h"
#include "../CUI/include/PropertyGrid.h"
#include "../CUI/include/ChartView.h"
#include "../CUI/include/ReportView.h"
#include "../CUI/include/KpiCard.h"
#include "../CUI/include/FilterBar.h"
#include "../CUI/include/TreeView.h"
#include "../CUI/include/TabControl.h"
#include "../CUI/include/ToolBar.h"
#include "../CUI/include/Menu.h"
#include "../CUI/include/StatusBar.h"
#include "../CUI/include/Toast.h"
#include "../CUI/include/MediaPlayer.h"
#include "../CUI/include/SplitContainer.h"
#include "../CUI/include/Layout/StackPanel.h"
#include "../CUI/include/Layout/GridPanel.h"
#include "../CUI/include/Layout/DockPanel.h"
#include "../CUI/include/Layout/WrapPanel.h"
#include "../CUI/include/Layout/RelativePanel.h"
#include "../CUI/include/Form.h"
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <utility>

#ifdef log
#undef log
#endif

using DesignValue = DesignerModel::DesignValue;

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
	DesignerBindingUtils::TargetMetadata CustomBindingTargetMetadata(
		const DesignerCustomPropertyDescriptor& property)
	{
		DesignerBindingUtils::TargetMetadata metadata{
			property.Name,
			BindingValueKind::Object,
			property.SupportsTwoWayBinding,
			property.Bindable,
			property.SupportsTwoWayBinding };
		switch (property.DefaultValue.Kind)
		{
		case DesignerStyleValueKind::Bool:
			metadata.ValueKind = BindingValueKind::Bool; break;
		case DesignerStyleValueKind::Int:
			metadata.ValueKind = BindingValueKind::Int; break;
		case DesignerStyleValueKind::Int64:
			metadata.ValueKind = BindingValueKind::Int64; break;
		case DesignerStyleValueKind::Float:
			metadata.ValueKind = BindingValueKind::Float; break;
		case DesignerStyleValueKind::Double:
			metadata.ValueKind = BindingValueKind::Double; break;
		case DesignerStyleValueKind::String:
			metadata.ValueKind = BindingValueKind::String; break;
		default:
			break;
		}
		return metadata;
	}

	const DesignerCustomPropertyDescriptor* FindCustomProperty(
		const DesignerControl& control,
		const std::wstring& name)
	{
		const auto found = std::find_if(
			control.CustomProperties.begin(), control.CustomProperties.end(),
			[&](const auto& property)
			{
				return _wcsicmp(property.Name.c_str(), name.c_str()) == 0;
			});
		return found == control.CustomProperties.end() ? nullptr : &*found;
	}

	std::optional<DesignerControlDescriptor> BuiltInDescriptor(UIClass type)
	{
		for (const auto& metadata : ControlRegistry::GetAvailableControls())
			if (metadata.Type == type)
				return DesignerControlDescriptor::BuiltIn(metadata);
		return std::nullopt;
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

static bool IsSplitContainerControl(Control* control)
{
	return control && control->Type() == UIClass::UI_SplitContainer;
}

static SplitContainer* AsSplitContainer(Control* control)
{
	return IsSplitContainerControl(control) ? (SplitContainer*)control : nullptr;
}

static std::string GetSplitRegionKey(SplitContainer* split, Control* runtimeParent)
{
	if (!split || !runtimeParent) return std::string();
	if (runtimeParent == split->FirstPanel()) return "panel1";
	if (runtimeParent == split->SecondPanel()) return "panel2";
	return std::string();
}

static Control* ResolveSplitRuntimeHost(SplitContainer* split, POINT localToSplit)
{
	if (!split) return nullptr;
	auto* first = split->FirstPanel();
	auto* second = split->SecondPanel();
	if (!first || !second) return split;

	auto firstLoc = first->ActualLocation;
	auto firstSize = first->ActualSize();
	auto secondLoc = second->ActualLocation;
	auto secondSize = second->ActualSize();

	bool inFirst = localToSplit.x >= firstLoc.x &&
		localToSplit.y >= firstLoc.y &&
		localToSplit.x <= firstLoc.x + firstSize.cx &&
		localToSplit.y <= firstLoc.y + firstSize.cy;
	if (inFirst) return first;

	bool inSecond = localToSplit.x >= secondLoc.x &&
		localToSplit.y >= secondLoc.y &&
		localToSplit.x <= secondLoc.x + secondSize.cx &&
		localToSplit.y <= secondLoc.y + secondSize.cy;
	if (inSecond) return second;

	if (split->SplitOrientation == Orientation::Horizontal)
	{
		int splitterCenter = secondLoc.x > firstLoc.x ? (firstLoc.x + firstSize.cx + secondLoc.x) / 2 : firstLoc.x + firstSize.cx;
		return localToSplit.x < splitterCenter ? first : second;
	}
	int splitterCenter = secondLoc.y > firstLoc.y ? (firstLoc.y + firstSize.cy + secondLoc.y) / 2 : firstLoc.y + firstSize.cy;
	return localToSplit.y < splitterCenter ? first : second;
}

static bool UsesAlignmentManagedPlacement(Control* control)
{
	return control && (control->HAlign != HorizontalAlignment::Left || control->VAlign != VerticalAlignment::Top);
}

static void ResetAlignmentForManualPlacement(Control* control)
{
	if (!control) return;
	if (control->HAlign != HorizontalAlignment::Left)
	{
		control->HAlign = HorizontalAlignment::Left;
	}
	if (control->VAlign != VerticalAlignment::Top)
	{
		control->VAlign = VerticalAlignment::Top;
	}
}

static void RefreshDesignerPanelLayout(Control* control)
{
	if (!control) return;
	if (auto* split = dynamic_cast<SplitContainer*>(control))
	{
		split->RefreshSplitterLayout();
		return;
	}
	if (auto* panel = dynamic_cast<Panel*>(control))
	{
		panel->InvalidateLayout();
		panel->PerformLayout();
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
			return _wcsicmp(entry.first.c_str(), canonicalCandidate.c_str()) == 0;
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
	return true;
}

static void AttachPreviewPropertySink(DesignerControl& control)
{
	if (!control.ControlInstance || control.CustomType.Empty())
	{
		control.PreviewPropertyChanged = {};
		return;
	}
	auto* target = control.ControlInstance;
	control.PreviewPropertyChanged =
		[target](const std::wstring& name, const DesignerStyleValue& value)
		{
			(void)DesignerPreviewBridge::SetValue(*target, name, value);
		};
}

DesignerCanvas::DesignerCanvas(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	_commandCoordinator = std::make_unique<DesignerCommandCoordinator>(this);
	_selectionService = std::make_unique<SelectionService>();

	// 初始化窗体字体为框架默认字体
	if (auto* def = GetDefaultFontObject())
	{
		_designedFormFontSize = def->FontSize;
	}
	_designedFormFontName = L"";

	// 画布（外围）与设计面板（内部）区分：设计面板负责裁剪/承载被设计控件
	this->BackColor = Colors::WhiteSmoke;
	this->BorderThickness = 2.0f;

	_designSurface = new Panel(_designSurfaceOrigin.x, _designSurfaceOrigin.y, _designedFormSize.cx, _designedFormSize.cy);
	_designSurface->BackColor = Colors::WhiteSmoke;
	_designSurface->BorderThickness = 0.0f; // 边框由画布统一绘制
	this->AddControl(_designSurface);

	{
		int top = DesignedClientTop();
		int h = _designedFormSize.cy - top;
		if (h < 0) h = 0;
		_clientSurface = new Panel(0, top, _designedFormSize.cx, h);
	}
	_clientSurface->BackColor = _designedFormBackColor;
	_clientSurface->BorderThickness = 0.0f;
	_designSurface->AddControl(_clientSurface);

	// 确保 clientSurface 使用窗体默认字体（初始为默认，不创建共享对象）
	RebuildDesignedFormSharedFont();
}

DesignerCanvas::~DesignerCanvas()
{
	if (_designedFormSharedFont)
	{
		delete _designedFormSharedFont;
		_designedFormSharedFont = nullptr;
	}
	for (auto* f : _retiredDesignedFormSharedFonts)
	{
		delete f;
	}
	_retiredDesignedFormSharedFonts.clear();
}

void DesignerCanvas::SetDesignedFormFontName(const std::wstring& name)
{
	_designedFormFontName = name;
	RebuildDesignedFormSharedFont();
	this->InvalidateVisual();
}

void DesignerCanvas::SetDesignedFormFontSize(float size)
{
	if (std::isnan(size) || std::isinf(size)) return;
	if (size < 1.0f) size = 1.0f;
	if (size > 200.0f) size = 200.0f;
	_designedFormFontSize = size;
	RebuildDesignedFormSharedFont();
	this->InvalidateVisual();
}

DesignerModel::DesignFormModel DesignerCanvas::CaptureDesignedFormModel() const
{
	DesignerModel::DesignFormModel form;
	form.Name = _designedFormName;
	form.Text = _designedFormText;
	form.FontName = _designedFormFontName;
	form.FontSize = _designedFormFontSize;
	form.Size = _designedFormSize;
	form.Location = _designedFormLocation;
	form.BackColor = _designedFormBackColor;
	form.ForeColor = _designedFormForeColor;
	form.ShowInTaskBar = _designedFormShowInTaskBar;
	form.TopMost = _designedFormTopMost;
	form.Enable = _designedFormEnable;
	form.Visible = _designedFormVisible;
	form.VisibleHead = _designedFormVisibleHead;
	form.HeadHeight = _designedFormHeadHeight;
	form.MinBox = _designedFormMinBox;
	form.MaxBox = _designedFormMaxBox;
	form.CloseBox = _designedFormCloseBox;
	form.CenterTitle = _designedFormCenterTitle;
	form.AllowResize = _designedFormAllowResize;
	form.EventHandlers = _designedFormEventHandlers;
	return form;
}

void DesignerCanvas::ApplyDesignedFormModel(
	const DesignerModel::DesignFormModel& value)
{
	auto form = value;
	if (form.Name.empty()) form.Name = L"MainForm";
	if (!std::isfinite(form.FontSize)) form.FontSize = 18.0f;
	form.FontSize = (std::clamp)(form.FontSize, 1.0f, 200.0f);
	form.HeadHeight = (std::max)(0, form.HeadHeight);
	if (form.Size.cx < 50) form.Size.cx = 50;
	if (form.Size.cy < 50) form.Size.cy = 50;

	const bool fontChanged = _designedFormFontName != form.FontName
		|| std::fabs(_designedFormFontSize - form.FontSize) >= 1e-6f;
	const bool headerLayoutChanged = _designedFormVisibleHead != form.VisibleHead
		|| _designedFormHeadHeight != form.HeadHeight;
	const bool sizeChanged = _designedFormSize.cx != form.Size.cx
		|| _designedFormSize.cy != form.Size.cy;

	_designedFormName = std::move(form.Name);
	_designedFormText = std::move(form.Text);
	_designedFormFontName = std::move(form.FontName);
	_designedFormFontSize = form.FontSize;
	_designedFormLocation = form.Location;
	_designedFormBackColor = form.BackColor;
	_designedFormForeColor = form.ForeColor;
	_designedFormShowInTaskBar = form.ShowInTaskBar;
	_designedFormTopMost = form.TopMost;
	_designedFormEnable = form.Enable;
	_designedFormVisible = form.Visible;
	_designedFormVisibleHead = form.VisibleHead;
	_designedFormHeadHeight = form.HeadHeight;
	_designedFormMinBox = form.MinBox;
	_designedFormMaxBox = form.MaxBox;
	_designedFormCloseBox = form.CloseBox;
	_designedFormCenterTitle = form.CenterTitle;
	_designedFormAllowResize = form.AllowResize;
	_designedFormEventHandlers = std::move(form.EventHandlers);
	if (_clientSurface) _clientSurface->BackColor = _designedFormBackColor;
	if (sizeChanged) SetDesignedFormSize(form.Size);
	else if (headerLayoutChanged) UpdateClientSurfaceLayout();
	if (fontChanged) RebuildDesignedFormSharedFont();
	this->InvalidateVisual();
}

void DesignerCanvas::RebuildDesignedFormSharedFont()
{
	auto rebindFontOf = [](Control* c, ::Font* value) {
		if (!c) return;
		if (value) c->SetFontEx(value, false);
		else c->SetFontEx(nullptr, false);
	};

	auto isDefaultLikeFont = [](const ::Font* cur, const ::Font* oldShared) -> bool {
		if (cur == GetDefaultFontObject()) return true;
		if (oldShared && cur == oldShared) return true;
		return false;
	};

	auto rebindFontsRecursive = [&](Control* root, ::Font* oldShared, ::Font* newShared) {
		if (!root) return;
		std::vector<Control*> stack;
		stack.reserve(128);
		stack.push_back(root);
		while (!stack.empty())
		{
			auto* c = stack.back();
			stack.pop_back();
			if (!c) continue;
			::Font* cur = c->Font;
			if (isDefaultLikeFont(cur, oldShared))
				rebindFontOf(c, newShared);
			for (size_t i = 0; i < c->Children.size(); ++i)
			{
				stack.push_back(c->Children[i]);
			}
		}
	};

	auto isFontUsedRecursive = [&](Control* root, const ::Font* f) -> bool {
		if (!root || !f) return false;
		std::vector<Control*> stack;
		stack.reserve(128);
		stack.push_back(root);
		while (!stack.empty())
		{
			auto* c = stack.back();
			stack.pop_back();
			if (!c) continue;
			if (c->Font == f) return true;
			for (size_t i = 0; i < c->Children.size(); ++i)
				stack.push_back(c->Children[i]);
		}
		return false;
	};

	::Font* oldShared = _designedFormSharedFont;
	_designedFormSharedFont = nullptr;

	auto* def = GetDefaultFontObject();
	std::wstring defName = def ? def->FontName : L"Arial";
	float defSize = def ? def->FontSize : 18.0f;

	std::wstring desiredName = _designedFormFontName.empty() ? defName : _designedFormFontName;
	float desiredSize = _designedFormFontSize;
	if (desiredSize < 1.0f) desiredSize = 1.0f;

	bool needShared = true;
	// 当字体名未显式设置且字号等于框架默认值时，使用框架默认字体（不创建共享对象）
	if (_designedFormFontName.empty() && std::fabs(desiredSize - defSize) < 1e-6f && desiredName == defName)
	{
		needShared = false;
	}

	::Font* newShared = nullptr;
	if (needShared)
	{
		try
		{
			newShared = new ::Font(desiredName, desiredSize);
		}
		catch (...)
		{
			newShared = nullptr;
		}
	}

	_designedFormSharedFont = newShared;

	// 让 clientSurface 也使用窗体字体（不拥有）
	if (_clientSurface)
	{
		rebindFontOf(_clientSurface, newShared);
	}

	// 将“默认字体”的控件绑定到新的共享字体；显式字体不受影响。
	// 注意：仅遍历 _designerControls 可能遗漏复合控件内部对象；这里额外递归遍历设计面板子树。
	for (auto& dc : _designerControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		auto* c = dc->ControlInstance;
		::Font* cur = c->Font;
		if (!isDefaultLikeFont(cur, oldShared)) continue;
		rebindFontOf(c, newShared);
	}
	rebindFontsRecursive(_designSurface ? (Control*)_designSurface : (Control*)_clientSurface, oldShared, newShared);

	// 释放策略（设计器安全优先）：
	// 字体对象可能被某些复合控件/缓存/延迟渲染路径引用，但不一定挂在 designSurface 子树下。
	// 为避免“修改字号两次”触发 UAF 崩溃，这里不在重建时 delete 旧共享字体，统一留到析构释放。
	if (oldShared)
		_retiredDesignedFormSharedFonts.push_back(oldShared);
}

void DesignerCanvas::Update()
{
	if (this->IsVisual == false) return;

	// 自身 Visible=false 在设计期仍需保持可选，否则用户无法再把它改回 true。
	// 只在不可见祖先（例如未激活 TabPage）真正遮蔽目标时移除选择。
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
			OnControlSelected(_selectedControl);
		}
	}
	if (!this->ParentForm) return;

	auto d2d = this->ParentForm->Render;
	auto absoluteLocation = this->AbsLocation;
	auto size = this->ActualSize();
	auto absoluteRect = this->AbsRect;
	const float absoluteX = static_cast<float>(absoluteLocation.x);
	const float absoluteY = static_cast<float>(absoluteLocation.y);

	d2d->PushDrawRect(absoluteRect.left, absoluteRect.top, absoluteRect.right - absoluteRect.left, absoluteRect.bottom - absoluteRect.top);
	{
		d2d->FillRect(absoluteX, absoluteY, (float)size.cx, (float)size.cy, this->BackColor);
		DrawGrid();
		if (_designSurface)
		{
			auto* oldMainMenu = this->ParentForm->MainMenu;
			auto* oldMainStatusBar = this->ParentForm->MainStatusBar;

			auto isInternalDesignedControl = [&](Control* c) -> bool {
				if (!c || !_designSurface) return false;
				if (c == _designSurface) return true;
				return IsDescendantOf(_designSurface, c);
			};

			this->ParentForm->MainMenu = nullptr;
			this->ParentForm->MainStatusBar = nullptr;
			_designSurface->Update();

			this->ParentForm->MainMenu = (oldMainMenu && !isInternalDesignedControl((Control*)oldMainMenu)) ? oldMainMenu : nullptr;
			this->ParentForm->MainStatusBar = (oldMainStatusBar && !isInternalDesignedControl((Control*)oldMainStatusBar)) ? oldMainStatusBar : nullptr;
		}

		// 绘制“仿真窗体”边框 + 标题栏（不影响控件布局，控件都在 clientSurface 内）
		{
			auto canvasAbs = this->AbsLocation;
			auto formRect = GetDesignSurfaceRectInCanvas();
			float fx = (float)(canvasAbs.x + formRect.left);
			float fy = (float)(canvasAbs.y + formRect.top);
			float fw = (float)(formRect.right - formRect.left);
			float fh = (float)(formRect.bottom - formRect.top);

			// 窗体边框
			d2d->DrawRect(fx, fy, fw, fh, Colors::DimGrey, 1.0f);

			// 标题栏
			int headH = DesignedClientTop();
			if (headH > 0)
			{
				D2D1_COLOR_F headBack = D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.25f);
				d2d->FillRect(fx, fy, fw, (float)headH, headBack);

				// 标题文字
				std::wstring title = _designedFormText.empty() ? L"Form" : _designedFormText;
				float textY = fy + (float)((headH - 14) * 0.5f);
				if (textY < fy) textY = fy;
				float pad = 8.0f;
				float btnW = (float)headH;
				int buttonCount = (_designedFormMinBox ? 1 : 0) + (_designedFormMaxBox ? 1 : 0) + (_designedFormCloseBox ? 1 : 0);
				float rightPad = (float)buttonCount * btnW;
				if (_designedFormCenterTitle)
				{
					// 简化：居中绘制（不做精确测量，按经验偏移）
					::Font* titleFont = _designedFormSharedFont ? _designedFormSharedFont : GetDefaultFontObject();
					if (!titleFont) titleFont = this->Font;
					d2d->DrawString(title, fx + (fw - rightPad) * 0.5f - 30.0f, textY, _designedFormForeColor, titleFont);
				}
				else
				{
					::Font* titleFont = _designedFormSharedFont ? _designedFormSharedFont : GetDefaultFontObject();
					if (!titleFont) titleFont = this->Font;
					d2d->DrawString(title, fx + pad, textY, _designedFormForeColor, titleFont);
				}

				// 右侧标题栏按钮（按 Form 的方式绘制图标）
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

				// 顺序与 Form 一致：Close / Max / Min
				drawBtnIcon(_designedFormCloseBox, 2);
				drawBtnIcon(_designedFormMaxBox, 1);
				drawBtnIcon(_designedFormMinBox, 0);
			}
		}
		// 选中边框/手柄/框选矩形：裁剪到设计面板
		{
			auto clip = GetClientSurfaceRectInCanvas();
			RECT canvasRect{ 0,0,this->Width,this->Height };
			auto finalClip = IntersectRectSafe(clip, canvasRect);
			auto canvasAbs = this->AbsLocation;
			d2d->PushDrawRect((float)(canvasAbs.x + finalClip.left), (float)(canvasAbs.y + finalClip.top),
				(float)(finalClip.right - finalClip.left), (float)(finalClip.bottom - finalClip.top));

			// 运行时不绘制 Visible=false 控件；Designer 叠加一个可命中的
			// 半透明占位，使它可被选中并恢复 Visible。
			for (const auto& dc : _designerControls)
			{
				if (!dc || dc->Type == UIClass::UI_TabPage
					|| !dc->ControlInstance
					|| dc->ControlInstance->Visible
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
				d2d->DrawRect(x, y, width, height, border, 1.5f);
				d2d->DrawLine(x, y, x + width, y + height, border, 1.0f);
				d2d->DrawLine(x + width, y, x, y + height, border, 1.0f);
				if (width >= 54.0f && height >= 18.0f)
				{
					auto* font = _designedFormSharedFont
						? _designedFormSharedFont : GetDefaultFontObject();
					if (font)
						d2d->DrawString(
							dc->Name + L" (Hidden)",
							x + 4.0f, y + 2.0f, border, font);
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
					d2d->DrawLine(x, top, x, bottom, c, 1.0f);
				}
				for (int yCanvas : _hGuides)
				{
					float y = (float)(canvasAbs.y + yCanvas);
					d2d->DrawLine(left, y, right, y, c, 1.0f);
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
				d2d->DrawRect(x, y, (float)w, (float)h, Colors::DodgerBlue, 2.0f);
			}

			// 主选中控件的调整手柄
			if (_selectedControl)
			{
				DrawSelectionHandles(_selectedControl);
			}

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
				d2d->DrawRect(x, y, (float)w, (float)h, Colors::DodgerBlue, 1.0f);
			}

			d2d->PopDrawRect();
		}

		d2d->DrawRect(absoluteX, absoluteY, (float)size.cx, (float)size.cy, this->BorderColor, this->BorderThickness);
	}
	if (!this->Enable)
	{
		d2d->FillRect(absoluteX, absoluteY, (float)size.cx, (float)size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
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
		OnControlSelected(_selectedControl);
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
		OnControlSelected(_selectedControl);
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
		OnControlSelected(_selectedControl);
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
	for (auto& dc : _selectedControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		auto* c = dc->ControlInstance;
		DragStartItem it;
		it.ControlInstance = c;
		it.Parent = c->Parent ? c->Parent : (_clientSurface ? (Control*)_clientSurface : (Control*)_designSurface);
		it.StartRectInCanvas = GetControlRectInCanvas(c);
		it.StartLocation = c->Location;
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
	switch (c->Type())
	{
	case UIClass::UI_GridPanel:
	case UIClass::UI_StackPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
	case UIClass::UI_ToolBar:
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
	auto* parent = moving->Parent;
	if (!parent) return;
	if (parent == _clientSurface) return;

	const auto parentType = parent->Type();
	const bool fromGrid = (parentType == UIClass::UI_GridPanel);
	const bool fromRelative = (parentType == UIClass::UI_RelativePanel);

	// 抬升前先拿到当前视觉矩形，保持“画面不跳”
	RECT r = GetControlRectInCanvas(moving);
	POINT newLocal = CanvasToContainerPoint({ r.left, r.top }, _clientSurface);
	int w = r.right - r.left;
	int h = r.bottom - r.top;
	if (w < 0) w = 0;
	if (h < 0) h = 0;

	// 从原容器移除，加入根客户区；这样拖动时不再受父容器裁剪限制。
	// 鼠标释放后再根据落点决定是否重新放回原容器或其他容器。
	auto movingOwner = parent->DetachControl(moving);
	if (!movingOwner) return;
	_clientSurface->AddOwned(std::move(movingOwner));
	if (UsesAlignmentManagedPlacement(moving))
	{
		ResetAlignmentForManualPlacement(moving);
	}
	// 从 GridPanel 抬升到根：避免默认 Stretch 直接把控件“铺满”
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
	_selectedControl->DesignerParent = nullptr;
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
		if (_clientSurface && it.ControlInstance->Parent == _clientSurface)
		{
			auto bounds = GetClientSurfaceRectInCanvas();
			newRect = ClampRectToBounds(newRect, bounds, true);
		}

		ApplyRectToControl(it.ControlInstance, newRect);
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
	OnInteractionTransactionCompleted(args);
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
	OnCommandCompleted(args);
}

void DesignerCanvas::NotifyDocumentStateChanged()
{
	DesignerCanvasDocumentStateEventArgs args{
		GetCurrentDocumentStateId(),
		GetSavedDocumentStateId(),
		IsDocumentDirty()
	};
	OnDocumentStateChanged(args);
}

std::wstring DesignerCanvas::CurrentPointerInteractionOperation() const
{
	if (_activePropertyInteraction)
		return _activePropertyInteraction->Operation;
	if (_activePlacementInteraction)
		return _activePlacementInteraction->Operation;
	if (!_activeInteractionTransaction.empty())
		return _activeInteractionTransaction;
	if (_isSplitterDragging) return L"UpdateProperty:SplitterDistance";
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
	_isSplitterDragging = false;
	_splitterDragTarget = nullptr;
	_splitterDragStartDistance = 0;
	_isResizing = false;
	_resizeHandle = DesignerControl::ResizeHandle::None;
	ClearAlignmentGuides();
	this->Cursor = CursorKind::Arrow;
	this->InvalidateVisual();
}

DesignerDocumentTransactionResult
DesignerCanvas::CancelActivePointerInteraction(const std::wstring& reason)
{
	const auto operation = CurrentPointerInteractionOperation();
	const bool hadPointerInteraction = _isBoxSelecting || _isDragging
		|| _isResizing || _isSplitterDragging
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
		ApplyMoveDeltaToSelection(dx, dy);
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

Thickness DesignerCanvas::GetPaddingOfContainer(Control* container)
{
	if (!container) return Thickness();
	if (auto* p = dynamic_cast<Panel*>(container))
		return p->Padding;
	return Thickness();
}

void DesignerCanvas::ApplyAnchorStylesKeepingBounds(Control* c, uint8_t newAnchorStyles)
{
	if (!c) return;
	// 以“当前视觉矩形”为准，切换 Anchor 后通过 Margin/Location 换算保持不变
	RECT r = GetControlRectInCanvas(c);
	c->AnchorStyles = newAnchorStyles;
	ApplyRectToControl(c, r);
}

void DesignerCanvas::ApplyRectToControl(Control* c, const RECT& rectInCanvas)
{
	if (!c) return;
	Control* parent = c->Parent ? c->Parent : (_clientSurface ? (Control*)_clientSurface : (Control*)_designSurface);
	if (!parent) return;

	POINT newLocal = CanvasToContainerPoint({ rectInCanvas.left, rectInCanvas.top }, parent);
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
		c->Location = { 0,0 };
		c->Margin = m;
		c->Size = { newW, newH };
		if (auto* p = dynamic_cast<Panel*>(parent))
		{
			RefreshDesignerPanelLayout(p);
		}
		return;
	}

	// 其他容器：沿用运行时默认 Anchor+Margin 规则。
	// 设计器这里需要在 Right/Bottom 锚定时同步换算 Margin.Right/Bottom，避免运行时贴边/拉伸异常。
	Thickness pad = GetPaddingOfContainer(parent);
	SIZE ps = parent->Size;
	const int innerRight = (int)ps.cx - (int)pad.Right;
	const int innerBottom = (int)ps.cy - (int)pad.Bottom;
	const int x = newLocal.x;
	const int y = newLocal.y;

	int leftDist = x - (int)pad.Left;
	int topDist = y - (int)pad.Top;
	int rightDist = innerRight - (x + newW);
	int bottomDist = innerBottom - (y + newH);
	if (leftDist < 0) leftDist = 0;
	if (topDist < 0) topDist = 0;
	if (rightDist < 0) rightDist = 0;
	if (bottomDist < 0) bottomDist = 0;

	c->Location = newLocal;
	auto m = c->Margin;
	uint8_t a = c->AnchorStyles;
	m.Left = 0.0f;
	m.Top = 0.0f;
	m.Right = 0.0f;
	m.Bottom = 0.0f;

	if (a & AnchorStyles::Right)
	{
		m.Right = (float)rightDist;
	}

	if (a & AnchorStyles::Bottom)
	{
		m.Bottom = (float)bottomDist;
	}

	c->Size = { newW, newH };
	c->Margin = m;

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
			OnControlSelected(nullptr);
		}
		this->InvalidateVisual();
		return;
	}

	if (fireEvent)
	{
		OnControlSelected(_selectedControl);
	}
	this->InvalidateVisual();
}

void DesignerCanvas::NotifySelectionChangedThrottled()
{
	// 拖动中频繁重建 PropertyGrid 可能较重，这里做一个简单节流。
	// Designer.cpp 已订阅 OnControlSelected 并把完整选择集交给 PropertyGrid。
	DWORD now = GetTickCount();
	if (now - _lastPropSyncTick < 40) return; // ~25fps
	_lastPropSyncTick = now;
	OnControlSelected(_selectedControl);
}

void DesignerCanvas::DrawGrid()
{
	if (!this->ParentForm) return;
	if (!_clientSurface) return;
	auto d2d = this->ParentForm->Render;
	int gridSize = _gridSize;
	auto canvasAbs = this->AbsLocation;
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
		d2d->DrawLine(surfAbsLeft + x, surfAbsTop, surfAbsLeft + x, surfAbsTop + surfH, gridColor, 0.5f);
	}

	for (int y = 0; y < (surfRect.bottom - surfRect.top); y += gridSize)
	{
		d2d->DrawLine(surfAbsLeft, surfAbsTop + y, surfAbsLeft + surfW, surfAbsTop + y, gridColor, 0.5f);
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
			if (dc->Type == UIClass::UI_TabPage) continue;
			Control* c = dc->ControlInstance;
			if (referenceParent && c->Parent != referenceParent) continue;
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
			if (dc->Type == UIClass::UI_TabPage) continue;
			Control* c = dc->ControlInstance;
			if (referenceParent && c->Parent != referenceParent) continue;
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
			if (dc->Type == UIClass::UI_TabPage) continue;
			Control* c = dc->ControlInstance;
			if (referenceParent && c->Parent != referenceParent) continue;
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
	RECT r;
	r.left = _designSurface->Location.x;
	r.top = _designSurface->Location.y;
	r.right = r.left + _designSurface->Size.cx;
	r.bottom = r.top + _designSurface->Size.cy;
	return r;
}

RECT DesignerCanvas::GetClientSurfaceRectInCanvas() const
{
	if (!_designSurface || !_clientSurface)
		return GetDesignSurfaceRectInCanvas();
	auto ds = GetDesignSurfaceRectInCanvas();
	RECT r;
	r.left = ds.left + _clientSurface->Location.x;
	r.top = ds.top + _clientSurface->Location.y;
	r.right = r.left + _clientSurface->Size.cx;
	r.bottom = r.top + _clientSurface->Size.cy;
	return r;
}

void DesignerCanvas::UpdateClientSurfaceLayout()
{
	if (!_designSurface || !_clientSurface) return;
	int top = DesignedClientTop();
	int h = _designSurface->Size.cy - top;
	if (h < 0) h = 0;
	_clientSurface->Location = { 0, top };
	_clientSurface->Size = { _designSurface->Size.cx, h };
	if (auto* p = dynamic_cast<Panel*>(_clientSurface))
	{
		RefreshDesignerPanelLayout(p);
	}
}

bool DesignerCanvas::IsPointInDesignSurface(POINT ptCanvas) const
{
	auto r = GetClientSurfaceRectInCanvas();
	return ptCanvas.x >= r.left && ptCanvas.x <= r.right && ptCanvas.y >= r.top && ptCanvas.y <= r.bottom;
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
	int scrollButton = bestTc->HitTestTitleScrollButton(localX, localY);
	if (scrollButton != 0)
	{
		bestTc->ScrollTitleBy(scrollButton * (std::max)(1.0f, bestTc->TitleScrollMouseWheelStep));
		bestTc->InvalidateVisual();
		ClearSelection();
		AddToSelection(bestDc, true, true);
		return true;
	}

	int titleIndex = -1;
	if (!bestTc->TryGetTitleIndexAt(localX, localY, titleIndex)) return false;

	bestTc->SelectPage(titleIndex);
	bestTc->EnsureTitleVisible(titleIndex);
	bestTc->InvalidateVisual();

	// 切页后：清除之前页上选中的控件，避免选框残留；并把 TabControl 设为当前选中。
	// 即使 titleIndex 未变化，点击标题栏也视为在操作 TabControl。
	ClearSelection();
	AddToSelection(bestDc, true, true);
	return true;
}

void DesignerCanvas::SetDesignedFormSize(SIZE s)
{
	if (s.cx < 50) s.cx = 50;
	if (s.cy < 50) s.cy = 50;
	_designedFormSize = s;
	if (_designSurface)
	{
		_designSurface->Size = s;
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
	this->InvalidateVisual();
}

void DesignerCanvas::ClampControlToDesignSurface(Control* c)
{
	if (!c) return;
	if (_clientSurface && c->Parent == _clientSurface)
	{
		auto rCanvas = GetControlRectInCanvas(c);
		auto bounds = GetClientSurfaceRectInCanvas();
		RECT clamped = ClampRectToBounds(rCanvas, bounds, true);
		POINT newTopLeftCanvas{ clamped.left, clamped.top };
		POINT newLocal = CanvasToContainerPoint(newTopLeftCanvas, _clientSurface);
		c->Location = newLocal;
		return;
	}
	if (_designSurface && c->Parent == _designSurface)
	{
		auto rCanvas = GetControlRectInCanvas(c);
		auto bounds = GetDesignSurfaceRectInCanvas();
		RECT clamped = ClampRectToBounds(rCanvas, bounds, true);
		POINT newTopLeftCanvas{ clamped.left, clamped.top };
		POINT newLocal = CanvasToContainerPoint(newTopLeftCanvas, _designSurface);
		c->Location = newLocal;
	}
}

void DesignerCanvas::DrawSelectionHandles(std::shared_ptr<DesignerControl> dc)
{
	if (!dc || !dc->ControlInstance || !this->ParentForm) return;

	auto d2d = this->ParentForm->Render;
	auto absoluteLocation = this->AbsLocation;
	auto rect = GetControlRectInCanvas(dc->ControlInstance);
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;

	// 绘制选中边框
	float x = (float)(absoluteLocation.x + rect.left);
	float y = (float)(absoluteLocation.y + rect.top);
	d2d->DrawRect(x, y, (float)w, (float)h, Colors::DodgerBlue, 2.0f);

	// 绘制8个调整手柄
	auto rects = GetHandleRectsFromRect(rect, 6);

	for (const auto& r : rects)
	{
		float hx = (float)(absoluteLocation.x + r.left);
		float hy = (float)(absoluteLocation.y + r.top);
		float hw = (float)(r.right - r.left);
		float hh = (float)(r.bottom - r.top);
		d2d->FillRect(hx, hy, hw, hh, Colors::White);
		d2d->DrawRect(hx, hy, hw, hh, Colors::DodgerBlue, 1.0f);
	}
}

std::shared_ptr<DesignerControl> DesignerCanvas::HitTestControl(POINT pt)
{
	const bool preferParentContainer =
		(GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
	auto findDesigner = [&](Control* control)
		-> std::shared_ptr<DesignerControl>
	{
		for (auto it = _designerControls.rbegin();
			it != _designerControls.rend(); ++it)
			if (*it && (*it)->ControlInstance == control) return *it;
		return nullptr;
	};

	// 占位绘制在运行时子树之上，所以自身不可见目标也应先于
	// 其可见父容器命中。
	for (auto it = _designerControls.rbegin();
		it != _designerControls.rend(); ++it)
	{
		const auto& dc = *it;
		if (!dc || dc->Type == UIClass::UI_TabPage
			|| !dc->ControlInstance || dc->ControlInstance->Visible
			|| !HasVisibleDesignerAncestors(dc->ControlInstance))
			continue;
		const auto rect = GetControlRectInCanvas(dc->ControlInstance);
		if (pt.x < rect.left || pt.x > rect.right
			|| pt.y < rect.top || pt.y > rect.bottom)
			continue;
		if (preferParentContainer)
		{
			for (auto* parent = dc->ControlInstance->Parent;
				parent && parent != this; parent = parent->Parent)
				if (auto designerParent = findDesigner(parent))
					return designerParent;
		}
		return dc;
	}

	return HitTestService::HitTestControl(
		this, _designerControls, pt, preferParentContainer);
}

bool DesignerCanvas::HasVisibleDesignerAncestors(
	Control* control) const noexcept
{
	if (!control) return false;
	for (auto* ancestor = control->Parent;
		ancestor && ancestor != this; ancestor = ancestor->Parent)
		if (!ancestor->Visible) return false;
	return true;
}

std::shared_ptr<DesignerControl> DesignerCanvas::HitTestSplitContainerSplitter(POINT pt) const
{
	for (auto it = _designerControls.rbegin(); it != _designerControls.rend(); ++it)
	{
		auto& dc = *it;
		if (!dc || dc->Type != UIClass::UI_SplitContainer || !dc->ControlInstance)
		{
			continue;
		}

		auto* split = (SplitContainer*)dc->ControlInstance;
		if (!split->Visible || !split->Enable)
		{
			continue;
		}

		auto splitterRect = GetSplitContainerSplitterRectInCanvas(split);
		if (pt.x >= splitterRect.left && pt.x <= splitterRect.right &&
			pt.y >= splitterRect.top && pt.y <= splitterRect.bottom)
		{
			return dc;
		}
	}

	return nullptr;
}

RECT DesignerCanvas::GetControlRectInCanvas(Control* c)
{
	RECT r{ 0,0,0,0 };
	if (!c) return r;
	auto absoluteLocation = c->AbsLocation;
	auto canvasAbs = this->AbsLocation;
	auto size = c->ActualSize();
	int left = absoluteLocation.x - canvasAbs.x;
	int top = absoluteLocation.y - canvasAbs.y;
	r.left = left;
	r.top = top;
	r.right = left + size.cx;
	r.bottom = top + size.cy;
	return r;
}

RECT DesignerCanvas::GetSplitContainerSplitterRectInCanvas(SplitContainer* split) const
{
	RECT rect{ 0, 0, 0, 0 };
	if (!split)
	{
		return rect;
	}

	auto splitRect = const_cast<DesignerCanvas*>(this)->GetControlRectInCanvas(split);
	auto size = split->ActualSize();
	int splitterWidth = (std::max)(1, split->SplitterWidth);
	int distance = ClampSplitContainerDistance(split, split->SplitterDistance);

	if (split->SplitOrientation == Orientation::Horizontal)
	{
		rect.left = splitRect.left + distance;
		rect.top = splitRect.top;
		rect.right = splitRect.left + ((distance + splitterWidth) < size.cx ? (distance + splitterWidth) : size.cx);
		rect.bottom = splitRect.bottom;
	}
	else
	{
		rect.left = splitRect.left;
		rect.top = splitRect.top + distance;
		rect.right = splitRect.right;
		rect.bottom = splitRect.top + ((distance + splitterWidth) < size.cy ? (distance + splitterWidth) : size.cy);
	}

	return rect;
}

int DesignerCanvas::ClampSplitContainerDistance(SplitContainer* split, int value) const
{
	if (!split)
	{
		return value;
	}

	auto size = split->ActualSize();
	int splitterWidth = (std::max)(1, split->SplitterWidth);
	int total = split->SplitOrientation == Orientation::Horizontal ? size.cx : size.cy;
	int maxDistance = (std::max)(split->Panel1MinSize, total - splitterWidth - split->Panel2MinSize);
	if (maxDistance < split->Panel1MinSize)
	{
		maxDistance = split->Panel1MinSize;
	}
	return (std::clamp)(value, split->Panel1MinSize, maxDistance);
}

bool DesignerCanvas::UpdateSplitContainerPreview(
	SplitContainer* split,
	int splitterDistance,
	std::wstring* outError)
{
	if (!split)
	{
		if (outError) *outError = L"分隔条目标已经失效。";
		return false;
	}

	const int effectiveDistance = ClampSplitContainerDistance(split, splitterDistance);
	for (const auto& designerControl : _designerControls)
	{
		if (!designerControl || designerControl->ControlInstance != split) continue;
		return ApplyTrackedMetadataProperty(
			*designerControl,
			*split,
			L"SplitterDistance",
			{ DesignerStyleValueKind::Int, std::to_wstring(effectiveDistance) },
			false,
			outError);
	}
	if (outError) *outError = L"找不到分隔条的设计器模型。";
	return false;
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
		OnControlSelected(nullptr);
	}
}

bool DesignerCanvas::IsContainerControl(Control* c)
{
	return HitTestService::IsContainerControl(c);
}

Control* DesignerCanvas::NormalizeContainerForDrop(Control* container)
{
	return LayoutBridge::NormalizeContainerForDrop(container);
}

POINT DesignerCanvas::CanvasToContainerPoint(POINT ptCanvas, Control* container)
{
	if (!container) return ptCanvas;
	auto canvasAbs = this->AbsLocation;
	auto containerLocation = container->AbsLocation;
	POINT containerPoint{ ptCanvas.x - (containerLocation.x - canvasAbs.x), ptCanvas.y - (containerLocation.y - canvasAbs.y) };
	// TabPage content 的坐标已经是 page 本地坐标，不需要额外处理
	return containerPoint;
}

Control* DesignerCanvas::FindBestContainerAtPoint(POINT ptCanvas, Control* ignore)
{
	return HitTestService::FindBestContainerAtPoint(_designerControls, ptCanvas, ignore, [this](Control* control) {
		return GetControlRectInCanvas(control);
	});
}

void DesignerCanvas::DeleteControlRecursive(Control* c)
{
	if (!c) return;
	// Control 析构会递归释放整棵子树；优先通过父容器完成显式所有权销毁。
	if (c->Parent && c->Parent->DeleteControl(c))
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
	Control* container = NormalizeContainerForDrop(rawContainer);
	if (!container) {
		// 落在容器之外：归为根级（客户区），DesignerParent 仍为 nullptr
		if (moving->Parent)
		{
			auto movingOwner = moving->Parent->DetachControl(moving);
			if (!movingOwner) return;
			_clientSurface->AddOwned(std::move(movingOwner));
		}
		else
		{
			_clientSurface->AddControl(moving);
		}
		_selectedControl->DesignerParent = nullptr;
		RECT clamped = ClampRectToBounds(r, GetClientSurfaceRectInCanvas(), true);
		ApplyRectToControl(moving, clamped);
		this->InvalidateVisual();
		return;
	}

	// TabControl 的 content 已归一化为 TabPage；ToolBar 等容器再由 LayoutBridge 校验。
	if (!LayoutBridge::CanAcceptChild(container, movingType))
		return;

	bool containerChanged = (_selectedControl->DesignerParent != container);

	// 防止把自己塞进自己的子树
	if (container == moving || IsDescendantOf(moving, container))
		return;

	// 计算保持视觉不动的目标位置
	POINT canvasTopLeft{ r.left, r.top };
	POINT dropLocalToContainer = CanvasToContainerPoint(center, container);
	Control* runtimeHost = container;
	if (auto* split = AsSplitContainer(container))
	{
		runtimeHost = ResolveSplitRuntimeHost(split, dropLocalToContainer);
	}
	POINT newLocal = CanvasToContainerPoint(canvasTopLeft, runtimeHost);
	POINT dropLocalCenter = CanvasToContainerPoint(center, runtimeHost);
	bool runtimeHostChanged = moving->Parent != runtimeHost;

	if (containerChanged || runtimeHostChanged)
	{
		std::unique_ptr<Control> movingOwner;
		if (moving->Parent)
		{
			movingOwner = moving->Parent->DetachControl(moving);
			if (!movingOwner) return;
		}

		// 加入新容器
		if (movingOwner)
			LayoutBridge::AttachChild(runtimeHost, std::move(movingOwner));
		else
			LayoutBridge::AttachChild(runtimeHost, moving);

		_selectedControl->DesignerParent = container;
	}

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

CursorKind DesignerCanvas::GetSplitContainerSplitterCursor(SplitContainer* split) const
{
	if (!split)
	{
		return CursorKind::Arrow;
	}

	return split->SplitOrientation == Orientation::Horizontal ? CursorKind::SizeWE : CursorKind::SizeNS;
}

bool DesignerCanvas::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable) return false;

	// Note: localX/localY are already local coordinates relative to this canvas.
	POINT mousePos = { localX, localY };

	switch (message)
	{
	case WM_KEYDOWN:
	{
		if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
		{
			const bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			if (wParam == 'Z' && !shiftDown)
			{
				(void)UndoCommand();
				return true;
			}
			else if (wParam == 'Y' || (wParam == 'Z' && shiftDown))
			{
				(void)RedoCommand();
				return true;
			}
		}

		// 设计器模式下，把键盘操作收敛到画布
		if (wParam == VK_ESCAPE)
		{
			if (_isBoxSelecting || _isDragging || _isResizing
				|| _isSplitterDragging
				|| HasActiveDeltaInteraction()
				|| !_activeInteractionTransaction.empty())
			{
				(void)CancelActivePointerInteraction(
					L"已通过 Escape 取消画布交互。");
				return true;
			}
			// 取消“点击添加控件”模式
			_controlToAdd.reset();
			this->Cursor = CursorKind::Arrow;
			return true;
		}

		if (wParam == VK_DELETE || wParam == VK_BACK)
		{
			DeleteSelectedControl();
			this->InvalidateVisual();
			return true;
		}

		// Ctrl+A：全选当前容器
		if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000))
		{
			Control* requiredParent = _clientSurface ? (Control*)_clientSurface : (Control*)_designSurface;
			if (_selectedControl && _selectedControl->ControlInstance)
				requiredParent = _selectedControl->ControlInstance->Parent ? _selectedControl->ControlInstance->Parent : (_clientSurface ? (Control*)_clientSurface : (Control*)_designSurface);
			if (!requiredParent) requiredParent = _clientSurface ? (Control*)_clientSurface : (Control*)_designSurface;

			ClearSelection();
			std::shared_ptr<DesignerControl> first = nullptr;
			for (auto& dc : _designerControls)
			{
				if (!dc || !dc->ControlInstance) continue;
				if (dc->Type == UIClass::UI_TabPage) continue;
				if (dc->ControlInstance->Parent != requiredParent) continue;
				if (!first) { first = dc; AddToSelection(dc, true, false); }
				else AddToSelection(dc, false, false);
			}
			OnControlSelected(_selectedControl);
			this->InvalidateVisual();
			return true;
		}

		if (_selectedControls.empty() || !_selectedControl || !_selectedControl->ControlInstance)
		{
			break;
		}

		int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 10 : 1;
		bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
		int dx = 0, dy = 0;

		switch (wParam)
		{
		case VK_LEFT:
			dx = -step;
			break;
		case VK_RIGHT:
			dx = step;
			break;
		case VK_UP:
			dy = -step;
			break;
		case VK_DOWN:
			dy = step;
			break;
		default:
			break;
		}
		(void)shift;
		if (dx != 0 || dy != 0)
		{
			(void)NudgeSelectionBy(dx, dy);
			return true;
		}
		break;
	}
	case WM_LBUTTONDBLCLK:
	{
		if (_controlToAdd) return true;
		auto hitControl = HitTestControl(mousePos);
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
			OnDefaultEventRequested(hitControl);
			this->InvalidateVisual();
			return true;
		}
		if (IsPointInDesignSurface(mousePos))
		{
			ClearSelection();
			OnControlSelected(nullptr);
			OnDefaultEventRequested(nullptr);
			this->InvalidateVisual();
			return true;
		}
		break;
	}
	case WM_LBUTTONDOWN:
	{
		// 确保键盘消息会转发到画布（Form 优先发给 Selected）
		if (this->ParentForm)
		{
			this->ParentForm->SetSelectedControl(this, true);
		}

		// 如果有待添加的控件，点击时添加（必须在设计面板内）
		if (_controlToAdd)
		{
			if (IsPointInDesignSurface(mousePos))
				AddControlToCanvas(*_controlToAdd, mousePos);
			_controlToAdd.reset();
			this->Cursor = CursorKind::Arrow;
			return true;
		}

		// 先处理 TabControl 标题栏点击（切页）
		if (TryHandleTabHeaderClick(mousePos))
			return true;

		// 设计器里的 Menu：需要“可交互”但也要选中。
		// 注意：不能让 Menu 抢走 Form::Selected，否则 Delete/方向键等设计器快捷键会失效。
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

				// 转发鼠标消息给 Menu 执行展开/点击等交互
				POINT local{ mousePos.x - r.left, mousePos.y - r.top };
				auto* oldSelected = this->ParentForm ? this->ParentForm->Selected : nullptr;
				menu->ProcessMessage(message, wParam, lParam, local.x, local.y);
				// 恢复：让键盘快捷键仍由画布处理
				if (this->ParentForm) this->ParentForm->SetSelectedControl(this, true);
				(void)oldSelected;
				return true;
			}
		}

		// 检查是否点击主选中手柄（仅单选/主选中可调整大小）
		if (_selectedControl && _selectedControls.size() == 1)
		{
			auto rect = GetControlRectInCanvas(_selectedControl->ControlInstance);
			auto handle = HitTestHandleFromRect(rect, mousePos, 6);
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

		auto splitterHit = HitTestSplitContainerSplitter(mousePos);
		if (splitterHit && splitterHit->ControlInstance)
		{
			if (!IsSelected(splitterHit) || _selectedControls.size() != 1)
			{
				ClearSelection();
				AddToSelection(splitterHit, true, true);
			}
			else
			{
				SetPrimarySelection(splitterHit, true);
			}

			auto* split = (SplitContainer*)splitterHit->ControlInstance;
			_isSplitterDragging = true;
			_splitterDragTarget = split;
			_splitterDragStartPoint = mousePos;
			_splitterDragStartDistance = split->SplitterDistance;
			this->Cursor = GetSplitContainerSplitterCursor(split);
			return true;
		}

		bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
		ClearAlignmentGuides();
		// 选中控件
		auto hitControl = HitTestControl(mousePos);
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
				OnControlSelected(nullptr);
				this->Cursor = CursorKind::Arrow;
				if (this->ParentForm)
				{
					this->ParentForm->UpdateCursorFromCurrentMouse();
				}
			}
			if (IsPointInDesignSurface(mousePos))
			{
				_isBoxSelecting = true;
				_boxSelectStart = mousePos;
				_boxSelectRect = { mousePos.x, mousePos.y, mousePos.x, mousePos.y };
				return true;
			}
		}
		break;
	}
	case WM_MOUSEMOVE:
	{
		// 框选更新
		if (_isBoxSelecting)
		{
			RECT r;
			r.left = (std::min)(_boxSelectStart.x, mousePos.x);
			r.top = (std::min)(_boxSelectStart.y, mousePos.y);
			r.right = (std::max)(_boxSelectStart.x, mousePos.x);
			r.bottom = (std::max)(_boxSelectStart.y, mousePos.y);
			_boxSelectRect = r;
			this->Cursor = CursorKind::Arrow;
			this->InvalidateVisual();
			return true;
		}

		if (_isSplitterDragging && _splitterDragTarget)
		{
			int dx = mousePos.x - _splitterDragStartPoint.x;
			int dy = mousePos.y - _splitterDragStartPoint.y;
			int delta = _splitterDragTarget->SplitOrientation == Orientation::Horizontal ? dx : dy;
			if (delta == 0)
			{
				this->Cursor = GetSplitContainerSplitterCursor(
					_splitterDragTarget);
				return true;
			}
			std::shared_ptr<DesignerControl> splitterControl;
			for (const auto& candidate : _designerControls)
			{
				if (candidate
					&& candidate->ControlInstance == _splitterDragTarget)
				{
					splitterControl = candidate;
					break;
				}
			}
			if (!BeginControlPropertyInteraction(
				L"UpdateProperty:SplitterDistance",
				splitterControl,
				L"SplitterDistance"))
			{
				ResetPointerInteractionState();
				return true;
			}

			std::wstring previewError;
			try
			{
				if (!UpdateSplitContainerPreview(
					_splitterDragTarget,
					_splitterDragStartDistance + delta,
					&previewError))
				{
					(void)AbortCanvasInteractionTransaction(
						std::move(previewError));
					ResetPointerInteractionState();
					return true;
				}
			}
			catch (...)
			{
				(void)AbortCanvasInteractionTransaction(
					L"分隔条预览 setter 抛出异常。");
				ResetPointerInteractionState();
				return true;
			}
			NotifySelectionChangedThrottled();
			this->Cursor = GetSplitContainerSplitterCursor(_splitterDragTarget);
			return true;
		}

		// 拖拽控件
		if (_isDragging && !_dragStartItems.empty())
		{
			int rawDx = mousePos.x - _dragStartPoint.x;
			int rawDy = mousePos.y - _dragStartPoint.y;
			if (rawDx == 0 && rawDy == 0)
			{
				this->Cursor = CursorKind::SizeAll;
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
					? _selectedControl->ControlInstance->Parent
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
			this->Cursor = CursorKind::SizeAll;
			return true;
		}

		// 调整大小
		if (_isResizing && _selectedControl && _selectedControl->ControlInstance)
		{
			int dx = mousePos.x - _dragStartPoint.x;
			int dy = mousePos.y - _dragStartPoint.y;
			if (dx == 0 && dy == 0)
			{
				this->Cursor = GetResizeCursor(_resizeHandle);
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

				Control* refParent = _selectedControl->ControlInstance->Parent
					? _selectedControl->ControlInstance->Parent
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

			this->Cursor = GetResizeCursor(_resizeHandle);
			return true;
		}

		// 更新鼠标样式（仅单选时显示 resize cursor）
		if (_selectedControl && _selectedControls.size() == 1)
		{
			auto rect = GetControlRectInCanvas(_selectedControl->ControlInstance);
			auto handle = HitTestHandleFromRect(rect, mousePos, 6);
			if (handle != DesignerControl::ResizeHandle::None)
			{
				this->Cursor = GetResizeCursor(handle);
				return true;
			}
		}

		auto splitterHover = HitTestSplitContainerSplitter(mousePos);
		if (splitterHover && splitterHover->ControlInstance)
		{
			this->Cursor = GetSplitContainerSplitterCursor((SplitContainer*)splitterHover->ControlInstance);
			return true;
		}

		// 如果是添加控件模式
		if (_controlToAdd)
		{
			this->Cursor = CursorKind::Hand;
		}
		else
		{
			this->Cursor = CursorKind::Arrow;
		}
		break;
	}
	case WM_LBUTTONUP:
	{
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
				requiredParent = _selectedControl->ControlInstance->Parent;

			bool primarySet = (_selectedControl != nullptr);
			for (auto& dc : _designerControls)
			{
				if (!dc || !dc->ControlInstance) continue;
				if (dc->Type == UIClass::UI_TabPage) continue;
				auto r = GetControlRectInCanvas(dc->ControlInstance);
				if (!intersects(sel, r)) continue;

				if (!requiredParent)
					requiredParent = dc->ControlInstance->Parent;
				if (dc->ControlInstance->Parent != requiredParent) continue;

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
			OnControlSelected(_selectedControl);
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
	case WM_CANCELMODE:
	case WM_CAPTURECHANGED:
	{
		(void)CancelActivePointerInteraction(
			message == WM_CANCELMODE
				? L"系统取消了画布交互。"
				: L"画布失去鼠标捕获，修改已回滚。");
		return true;
	}
	}

	return Panel::ProcessMessage(message, wParam, lParam, localX, localY);
}

void DesignerCanvas::SetControlToAdd(UIClass type)
{
	_controlToAdd = BuiltInDescriptor(type);
}

void DesignerCanvas::SetControlDescriptors(
	const std::vector<DesignerControlDescriptor>& descriptors)
{
	_customControlDescriptors.clear();
	for (const auto& descriptor : descriptors)
		RegisterControlDescriptor(descriptor);
}

void DesignerCanvas::RegisterControlDescriptor(
	const DesignerControlDescriptor& descriptor)
{
	if (!descriptor.IsValid() || !descriptor.IsCustom()) return;
	_customControlDescriptors[descriptor.CustomType.RegistryKey()] = descriptor;
}

void DesignerCanvas::SetControlToAdd(
	const DesignerControlDescriptor& descriptor)
{
	if (descriptor.IsValid())
	{
		RegisterControlDescriptor(descriptor);
		_controlToAdd = descriptor;
	}
	else _controlToAdd.reset();
}

DesignerDocumentTransactionResult DesignerCanvas::AddControlToCanvas(
	UIClass type, POINT canvasPos)

{
	DesignerControlDescriptor descriptor;
	if (const auto builtIn = BuiltInDescriptor(type)) descriptor = *builtIn;
	else descriptor.Type = type;
	return AddControlToCanvas(descriptor, canvasPos);
}

DesignerDocumentTransactionResult DesignerCanvas::AddControlToCanvas(
	const DesignerControlDescriptor& descriptor, POINT canvasPos)
{
	const auto type = descriptor.Type;
	DesignerDocumentTransactionResult result;
	if (!descriptor.IsValid())
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"未指定有效的控件描述。");
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
		RegisterControlDescriptor(descriptor);
		const auto beforeSelectionNames = CaptureSelectionNames();
		const auto beforePrimarySelectionName = _selectedControl
			? _selectedControl->Name : std::wstring{};
		const auto beforeCount = _designerControls.size();
		std::unordered_set<Control*> beforeRuntimeControls;
		std::function<void(Control*)> collectRuntimeControls =
			[&](Control* parent)
			{
				if (!parent) return;
				for (int index = 0; index < parent->Count; ++index)
				{
					auto* child = parent->operator[](index);
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
			AddControlToCanvasCore(descriptor, canvasPos);
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
						for (int index = 0; index < parent->Count; ++index)
						{
							auto* child = parent->operator[](index);
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
					if (root->Parent)
						(void)root->Parent->DeleteControl(root);
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
						L"AddControl",
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
	PublishCanvasCommandResult(L"AddControl", L"AddControl", result);
	return result;
}

void DesignerCanvas::AddControlToCanvasCore(UIClass type, POINT canvasPos)

{
	const auto descriptor = BuiltInDescriptor(type);
	if (descriptor) AddControlToCanvasCore(*descriptor, canvasPos);
}

void DesignerCanvas::AddControlToCanvasCore(
	const DesignerControlDescriptor& descriptor, POINT canvasPos)
{
	const auto type = descriptor.Type;
	Control* newControl = nullptr;
	std::unique_ptr<Control> newControlOwner;
	auto ownControl = [&](Control* value)
	{
		newControlOwner.reset(value);
		newControl = value;
	};
	std::wstring typeName;
	if (!_designSurface || !_clientSurface) return;
	if (!IsPointInDesignSurface(canvasPos)) return;

	// 在点击位置创建控件（左上角对齐，稍微偏移避免手感奇怪）
	int centerX = (int)canvasPos.x - 30;
	int centerY = (int)canvasPos.y - 12;

	if (!descriptor.IsValid()) return;
	RegisterControlDescriptor(descriptor);
	if (descriptor.IsCustom())
	{
		auto preview = descriptor.PreviewFactory
			? descriptor.PreviewFactory(centerX, centerY)
			: DesignerControlFactory::Create(type, centerX, centerY);
		if (!preview || preview->Type() != type) return;
		preview->Location = { centerX, centerY };
		preview->Size = descriptor.DefaultSize;
		newControl = preview.get();
		newControlOwner = std::move(preview);
		typeName = descriptor.Name;
	}
	else switch (type)
	{
	case UIClass::UI_Label:
		ownControl(new Label(L"标签", centerX, centerY));
		typeName = L"Label";
		break;
	case UIClass::UI_LinkLabel:
		ownControl(new LinkLabel(L"链接标签", centerX, centerY));
		typeName = L"LinkLabel";
		break;
	case UIClass::UI_Button:
		ownControl(new Button(L"按钮", centerX, centerY, 120, 30));
		typeName = L"Button";
		break;
	case UIClass::UI_TextBox:
		ownControl(new TextBox(L"", centerX, centerY, 200, 25));
		typeName = L"TextBox";
		break;
	case UIClass::UI_RichTextBox:
		ownControl(new RichTextBox(L"", centerX, centerY, 300, 160));
		typeName = L"RichTextBox";
		break;
	case UIClass::UI_PasswordBox:
		ownControl(new PasswordBox(L"", centerX, centerY, 200, 25));
		typeName = L"PasswordBox";
		break;
	case UIClass::UI_DateTimePicker:
		ownControl(new DateTimePicker(L"", centerX, centerY, 200, 28));
		typeName = L"DateTimePicker";
		break;
	case UIClass::UI_NumericUpDown:
		ownControl(new NumericUpDown(centerX, centerY, 140, 30));
		typeName = L"NumericUpDown";
		break;
	case UIClass::UI_Panel:
		ownControl(new Panel(centerX, centerY, 200, 200));
		typeName = L"Panel";
		break;
	case UIClass::UI_GroupBox:
		ownControl(new GroupBox(L"GroupBox", centerX, centerY, 240, 180));
		typeName = L"GroupBox";
		break;
	case UIClass::UI_Expander:
		ownControl(new Expander(L"Expander", centerX, centerY, 260, 160));
		typeName = L"Expander";
		break;
	case UIClass::UI_ScrollView:
		ownControl(new ScrollView(centerX, centerY, 240, 200));
		typeName = L"ScrollView";
		break;
	case UIClass::UI_StackPanel:
		ownControl(new StackPanel(centerX, centerY, 200, 200));
		typeName = L"StackPanel";
		break;
	case UIClass::UI_GridPanel:
		ownControl(new GridPanel(centerX, centerY, 200, 200));
		typeName = L"GridPanel";
		break;
	case UIClass::UI_DockPanel:
		ownControl(new DockPanel(centerX, centerY, 200, 200));
		typeName = L"DockPanel";
		break;
	case UIClass::UI_WrapPanel:
		ownControl(new WrapPanel(centerX, centerY, 200, 200));
		typeName = L"WrapPanel";
		break;
	case UIClass::UI_RelativePanel:
		ownControl(new RelativePanel(centerX, centerY, 200, 200));
		typeName = L"RelativePanel";
		break;
	case UIClass::UI_SplitContainer:
		ownControl(new SplitContainer(centerX, centerY, 360, 220));
		typeName = L"SplitContainer";
		break;
	case UIClass::UI_CheckBox:
		ownControl(new CheckBox(L"复选框", centerX, centerY));
		typeName = L"CheckBox";
		break;
	case UIClass::UI_RadioBox:
		ownControl(new RadioBox(L"单选框", centerX, centerY));
		typeName = L"RadioBox";
		break;
	case UIClass::UI_ComboBox:
		ownControl(new ComboBox(L"", centerX, centerY, 150, 25));
		typeName = L"ComboBox";
		break;
	case UIClass::UI_ListView:
	{
		auto* listView = new ListView(centerX, centerY, 320, 220);
		ownControl(listView);
		listView->AddColumn(ListViewColumn(L"名称", 160));
		listView->AddColumn(ListViewColumn(L"说明", 130));
		ListViewItem first(L"ListViewItem 1", L"Details row");
		first.SubItems.push_back(L"Details row");
		listView->AddItem(first);
		listView->AddItem(ListViewItem(L"ListViewItem 2", L"Selectable"));
		typeName = L"ListView";
		break;
	}
	case UIClass::UI_ListBox:
	{
		auto* lb = new ListBox(centerX, centerY, 220, 180);
		ownControl(lb);
		lb->AddItem(ListViewItem(L"ListBox Item 1"));
		lb->AddItem(ListViewItem(L"ListBox Item 2"));
		lb->AddItem(ListViewItem(L"ListBox Item 3"));
		typeName = L"ListBox";
		break;
	}
	case UIClass::UI_GridView:
		ownControl(new GridView(centerX, centerY, 360, 200));
		typeName = L"GridView";
		break;
	case UIClass::UI_PropertyGrid:
	{
		auto* pg = new PropertyGridView(centerX, centerY, 300, 320);
		ownControl(pg);
		pg->AddProperty(L"Appearance", L"Text", L"PropertyGrid", PropertyGridValueType::Text);
		pg->AddProperty(L"Appearance", L"Visible", L"True", PropertyGridValueType::Bool);
		PropertyGridItem dock(L"Layout", L"Dock", L"None", PropertyGridValueType::Enum);
		dock.Options = { L"None", L"Top", L"Bottom", L"Left", L"Right", L"Fill" };
		pg->AddItem(dock);
		pg->AddProperty(L"Layout", L"Width", L"300", PropertyGridValueType::Number);
		typeName = L"PropertyGrid";
		break;
	}
	case UIClass::UI_ChartView:
		ownControl(new ChartView(centerX, centerY, 420, 260));
		typeName = L"ChartView";
		break;
	case UIClass::UI_ReportView:
		ownControl(new ReportView(centerX, centerY, 480, 300));
		typeName = L"ReportView";
		break;
	case UIClass::UI_KpiCard:
		ownControl(new KpiCard(centerX, centerY, 220, 132));
		typeName = L"KpiCard";
		break;
	case UIClass::UI_FilterBar:
		ownControl(new FilterBar(centerX, centerY, 640, 48));
		typeName = L"FilterBar";
		break;
	case UIClass::UI_TreeView:
		ownControl(new TreeView(centerX, centerY, 220, 220));
		typeName = L"TreeView";
		break;
	case UIClass::UI_ProgressBar:
		ownControl(new ProgressBar(centerX, centerY, 200, 20));
		typeName = L"ProgressBar";
		break;
	case UIClass::UI_LoadingRing:
		ownControl(new LoadingRing(centerX, centerY, 48, 48));
		typeName = L"LoadingRing";
		break;
	case UIClass::UI_ProgressRing:
		ownControl(new ProgressRing(centerX, centerY, 72, 72));
		typeName = L"ProgressRing";
		break;
	case UIClass::UI_Slider:
		ownControl(new Slider(centerX, centerY, 200, 30));
		typeName = L"Slider";
		break;
	case UIClass::UI_PictureBox:
		ownControl(new PictureBox(centerX, centerY, 150, 150));
		typeName = L"PictureBox";
		break;
	case UIClass::UI_Switch:
		ownControl(new Switch(centerX, centerY, 60, 30));
		typeName = L"Switch";
		break;
	case UIClass::UI_TabControl:
		ownControl(new TabControl(centerX, centerY, 360, 240));
		typeName = L"TabControl";
		break;
	case UIClass::UI_ToolBar:
		ownControl(new ToolBar(centerX, centerY, 360, 34));
		typeName = L"ToolBar";
		break;
	case UIClass::UI_Menu:
	{
		// Menu 始终为窗体根级控件：放在客户区顶部并拉伸宽度
		int w = _clientSurface ? _clientSurface->Width : 360;
		if (w < 80) w = 80;
		ownControl(new Menu(0, 0, w, 28));
		typeName = L"Menu";
		break;
	}
	case UIClass::UI_StatusBar:
	{
		// StatusBar 始终为窗体根级控件：放在客户区底部并拉伸宽度
		int w = _clientSurface ? _clientSurface->Width : 360;
		if (w < 80) w = 80;
		int h = 26;
		int y = _clientSurface ? (_clientSurface->Height - h) : (centerY);
		if (y < 0) y = 0;
		ownControl(new StatusBar(0, y, w, h));
		typeName = L"StatusBar";
		break;
	}
	case UIClass::UI_ToastHost:
	{
		auto* host = new ToastHost(centerX, centerY, 340, 260);
		ownControl(host);
		host->ShowToast(L"ToastHost", L"运行时调用 ShowToast 添加通知。", ToastKind::Info, 0);
		typeName = L"ToastHost";
		break;
	}
	case UIClass::UI_WebBrowser:
		ownControl(new FakeWebBrowser(centerX, centerY, 500, 360));
		typeName = L"WebBrowser";
		break;
	case UIClass::UI_MediaPlayer:
		ownControl(new MediaPlayer(centerX, centerY, 640, 360));
		typeName = L"MediaPlayer";
		break;
	default:
		return;
	}

	if (newControl)
	{
		// Reserve identity before ownership transfer so allocation failure cannot
		// leave an attached runtime control without a DesignerControl wrapper.
		const int stableId = AllocateStableControlId();
		newControl->DesignId = stableId;
		// Menu/StatusBar 不参与容器命中：强制根级（窗体客户区）
		if (type == UIClass::UI_Menu || type == UIClass::UI_StatusBar)
		{
			_clientSurface->AddOwned(std::move(newControlOwner));
			std::wstring name = GenerateDefaultControlName(type, typeName);
			auto dc = std::make_shared<DesignerControl>(
				newControl, name, type, nullptr, stableId);
			dc->CustomType = descriptor.CustomType;
			dc->CustomProperties = descriptor.CustomProperties;
			dc->CustomEvents = descriptor.CustomEvents;
			AttachPreviewPropertySink(*dc);
			_designerControls.push_back(dc);
			UpdateDefaultNameCounterFromName(type, name);
			ClearSelection();
			AddToSelection(dc, true, true);
			this->InvalidateVisual();
			return;
		}

		// 确定父容器：鼠标点下命中的最内层容器（TabControl 会归一化到当前页）
		Control* rawContainer = FindBestContainerAtPoint(canvasPos, nullptr);
		Control* container = NormalizeContainerForDrop(rawContainer);
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
			POINT dropLocalToContainer = CanvasToContainerPoint(canvasPos, container);
			runtimeHost = container;
			if (auto* split = AsSplitContainer(container))
			{
				runtimeHost = ResolveSplitRuntimeHost(split, dropLocalToContainer);
			}
			POINT local = CanvasToContainerPoint({ centerX, centerY }, runtimeHost);
			POINT dropLocal = CanvasToContainerPoint(canvasPos, runtimeHost);
			(void)LayoutBridge::AttachChild(
				runtimeHost, std::move(newControlOwner));
			LayoutBridge::ApplyNewChildLayout(runtimeHost, newControl, local, dropLocal);
			LayoutBridge::RefreshContainerLayout(runtimeHost);
		}
		else
		{
			// 根级：属于窗体客户区
			_clientSurface->AddOwned(std::move(newControlOwner));
			POINT local = CanvasToContainerPoint({ centerX, centerY }, _clientSurface);
			newControl->Location = local;
			// 约束初始位置到客户区
			ClampControlToDesignSurface(newControl);
		}

		std::wstring name = GenerateDefaultControlName(type, typeName);

		// 创建设计器控件包装
		auto dc = std::make_shared<DesignerControl>(
			newControl, name, type, designerParent, stableId);
		dc->CustomType = descriptor.CustomType;
		dc->CustomProperties = descriptor.CustomProperties;
		dc->CustomEvents = descriptor.CustomEvents;
		AttachPreviewPropertySink(*dc);
		if (type == UIClass::UI_SplitContainer)
		{
			(void)ApplyTrackedMetadataProperty(
				*dc,
				*newControl,
				L"SplitterDistance",
				{ DesignerStyleValueKind::Int, L"176" },
				false);
		}
		else if (type == UIClass::UI_ListView)
		{
			(void)ApplyTrackedMetadataProperty(
				*dc,
				*newControl,
				L"ViewMode",
				{ DesignerStyleValueKind::Int,
					std::to_wstring(static_cast<int>(ListViewViewMode::Details)) },
				false);
		}
		_designerControls.push_back(dc);
		UpdateDefaultNameCounterFromName(type, name);

		// 自动选中新添加的控件
		ClearSelection();
		AddToSelection(dc, true, true);
		this->InvalidateVisual();
	}
}

DesignerDocumentTransactionResult DesignerCanvas::DeleteSelectedControl()
{
	if (_selectedControls.empty())
	{
		auto result = DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
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
		try
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
	OnControlSelected(nullptr);

	for (auto* inst : toDelete)
	{
		if (!inst) continue;
		// 删除控件前：先移除该子树下所有 DesignerControl，避免悬挂指针
		RemoveDesignerControlsInSubtree(inst);
		DeleteControlRecursive(inst);
	}
	this->InvalidateVisual();
}

void DesignerCanvas::ClearCanvasCore()
{
	if (_clientSurface)
	{
		// 清空客户区内的所有控件（递归释放）
		while (_clientSurface->Count > 0)
		{
			auto c = _clientSurface->operator[](_clientSurface->Count - 1);
			DeleteControlRecursive(c);
		}
	}
	_designerControls.clear();
	_selectedControl = nullptr;
	_controlTypeCounters.clear();
	_nextStableControlId = 1;
	_designedFormName = L"MainForm";
	_designedFormEventHandlers.clear();
	_dataContextSchema.clear();
	_documentStyleSheet = {};
	_previewStyleSheet.reset();
	if (_clientSurface) (void)_clientSurface->SetStyleSheet(nullptr, true);

	OnControlSelected(nullptr);
}

bool DesignerCanvas::SetDataContextSchema(
	DesignerDataContextSchema schema,
	std::wstring* outError)
{
	DesignerDataContextSchemaUtils::Canonicalize(schema);
	if (!DesignerDataContextSchemaUtils::Validate(schema, outError)) return false;
	for (const auto& control : _designerControls)
	{
		if (!control || !control->ControlInstance) continue;
		for (const auto& [targetProperty, binding] : control->DataBindings)
		{
			std::wstring validationError;
			const auto* customProperty = FindCustomProperty(*control, targetProperty);
			const bool deferredCustomProperty = customProperty
				&& !control->ControlInstance->FindPropertyMetadata(targetProperty);
			const bool valid = deferredCustomProperty
				? DesignerBindingUtils::ValidateTarget(
					CustomBindingTargetMetadata(*customProperty),
					binding,
					&validationError,
					&schema)
				: DesignerBindingUtils::Validate(
					*control->ControlInstance,
					targetProperty,
					binding,
					nullptr,
					&validationError,
					&schema);
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
	(void)RefreshAllDesignBindings(nullptr);
	OnControlSelected(_selectedControl);
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
		for (const auto& [property, localValue] :
			control.BindingPreviewLocalValues)
		{
			if (localValue)
				(void)target->TrySetPropertyValue(
					property, *localValue, ControlPropertyValueSource::Local);
			else
				(void)target->ClearPropertyValue(
					property, ControlPropertyValueSource::Local);
		}
	}
	control.BindingPreviewStates.clear();
	control.BindingPreviewLocalValues.clear();
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
		const auto* customProperty = FindCustomProperty(control, targetProperty);
		const bool deferredCustomProperty =
			customProperty
			&& !target->FindPropertyMetadata(targetProperty);
		if (deferredCustomProperty)
		{
			std::wstring validationError;
			if (!DesignerBindingUtils::ValidateTarget(
				CustomBindingTargetMetadata(*customProperty),
				configuration,
				&validationError,
				_dataContextSchema.empty() ? nullptr : &_dataContextSchema))
			{
				state.Status = DesignerBindingPreviewStatus::Error;
				state.Message = validationError;
				success = false;
				if (firstError.empty()) firstError = validationError;
				continue;
			}

			state.Status = DesignerBindingPreviewStatus::Detached;
			state.Message = !_designDataContext
				? L"未设置设计期 DataContext；配置已校验并保存但尚未连接。"
				: L"自定义属性 Binding 已校验并保存；"
					L"当前基类代理没有同名运行时元数据，不建立活动预览连接。";
			continue;
		}
		if (!_designDataContext)
		{
			state.Status = DesignerBindingPreviewStatus::Detached;
			state.Message = L"未设置设计期 DataContext；配置已保存但尚未连接。";
			continue;
		}

		std::wstring validationError;
		const bool valid = DesignerBindingUtils::Validate(
			*target,
			targetProperty,
			configuration,
			nullptr,
			&validationError,
			_dataContextSchema.empty() ? nullptr : &_dataContextSchema);
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

		const bool writesTarget = configuration.Mode != BindingMode::OneWayToSource;
		if (writesTarget)
		{
			BindingValue localValue;
			const bool hadLocal = target->TryGetPropertyValue(
				targetProperty,
				ControlPropertyValueSource::Local,
				localValue);
			if (hadLocal && !target->ClearPropertyValue(
				targetProperty, ControlPropertyValueSource::Local))
			{
				state.Status = DesignerBindingPreviewStatus::Error;
				state.Message = L"无法暂存目标属性的 Local 值。";
				success = false;
				if (firstError.empty()) firstError = state.Message;
				continue;
			}
			control.BindingPreviewLocalValues[targetProperty] =
				hadLocal ? std::optional<BindingValue>(std::move(localValue))
					: std::nullopt;
		}

		auto* binding = target->DataBindings.Add(
			targetProperty,
			*_designDataContext,
			configuration.SourceProperty,
			configuration.Mode,
			configuration.UpdateMode,
			std::move(converter));
		if (!binding)
		{
			const std::wstring bindingError = target->DataBindings.LastErrorMessage();
			const auto saved = control.BindingPreviewLocalValues.find(targetProperty);
			if (saved != control.BindingPreviewLocalValues.end())
			{
				if (saved->second)
					(void)target->TrySetPropertyValue(
						targetProperty,
						*saved->second,
						ControlPropertyValueSource::Local);
				control.BindingPreviewLocalValues.erase(saved);
			}
			state.Status = DesignerBindingPreviewStatus::Error;
			state.Message = L"预览绑定连接失败：" + bindingError;
			success = false;
			if (firstError.empty()) firstError = state.Message;
			continue;
		}

		state.Status = DesignerBindingPreviewStatus::Active;
		state.Message = L"设计期预览绑定已连接。";
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
	std::wstring* outError)
{
	DesignerStyleSheetUtils::Canonicalize(styleSheet);
	if (!DesignerStyleSheetUtils::ValidateAgainstPropertyMetadata(
		styleSheet,
		[](UIClass type) { return DesignerControlFactory::Create(type); },
		outError))
		return false;
	std::shared_ptr<ControlStyleSheet> runtime;
	if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(styleSheet, runtime, outError))
		return false;

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

	if (_clientSurface)
	{
		const auto previous = _previewStyleSheet;
		const bool applied = !styleSheet.Empty()
			? _clientSurface->SetStyleSheet(runtime, true)
			: _clientSurface->SetStyleSheet(nullptr, true);
		if (!applied)
		{
			(void)_clientSurface->SetStyleSheet(previous, true);
			if (outError) *outError = L"样式表无法应用到完整控件树；请检查通配规则的目标属性类型。";
			return false;
		}
	}
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

static bool IsExportableDesignType(UIClass t)
{
	switch (t)
	{
	case UIClass::UI_Label:
	case UIClass::UI_LinkLabel:
	case UIClass::UI_Button:
	case UIClass::UI_TextBox:
	case UIClass::UI_RichTextBox:
	case UIClass::UI_PasswordBox:
	case UIClass::UI_DateTimePicker:
	case UIClass::UI_NumericUpDown:
	case UIClass::UI_Panel:
	case UIClass::UI_GroupBox:
	case UIClass::UI_Expander:
	case UIClass::UI_ScrollView:
	case UIClass::UI_StackPanel:
	case UIClass::UI_GridPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
	case UIClass::UI_SplitContainer:
	case UIClass::UI_CheckBox:
	case UIClass::UI_RadioBox:
	case UIClass::UI_ComboBox:
	case UIClass::UI_ListView:
	case UIClass::UI_ListBox:
	case UIClass::UI_GridView:
	case UIClass::UI_PropertyGrid:
	case UIClass::UI_ChartView:
	case UIClass::UI_ReportView:
	case UIClass::UI_KpiCard:
	case UIClass::UI_FilterBar:
	case UIClass::UI_TreeView:
	case UIClass::UI_ProgressBar:
	case UIClass::UI_LoadingRing:
	case UIClass::UI_ProgressRing:
	case UIClass::UI_Slider:
	case UIClass::UI_PictureBox:
	case UIClass::UI_Switch:
	case UIClass::UI_TabControl:
	case UIClass::UI_TabPage:
	case UIClass::UI_ToolBar:
	case UIClass::UI_Menu:
	case UIClass::UI_StatusBar:
	case UIClass::UI_ToastHost:
	case UIClass::UI_WebBrowser:
	case UIClass::UI_MediaPlayer:
		return true;
	default:
		return false;
	}
}

static std::wstring ExportTypeName(UIClass t)
{
	switch (t)
	{
	case UIClass::UI_Label: return L"Label";
	case UIClass::UI_LinkLabel: return L"LinkLabel";
	case UIClass::UI_Button: return L"Button";
	case UIClass::UI_TextBox: return L"TextBox";
	case UIClass::UI_RichTextBox: return L"RichTextBox";
	case UIClass::UI_PasswordBox: return L"PasswordBox";
	case UIClass::UI_DateTimePicker: return L"DateTimePicker";
	case UIClass::UI_NumericUpDown: return L"NumericUpDown";
	case UIClass::UI_ComboBox: return L"ComboBox";
	case UIClass::UI_ListView: return L"ListView";
	case UIClass::UI_ListBox: return L"ListBox";
	case UIClass::UI_GridView: return L"GridView";
	case UIClass::UI_PropertyGrid: return L"PropertyGrid";
	case UIClass::UI_ChartView: return L"ChartView";
	case UIClass::UI_ReportView: return L"ReportView";
	case UIClass::UI_KpiCard: return L"KpiCard";
	case UIClass::UI_FilterBar: return L"FilterBar";
	case UIClass::UI_CheckBox: return L"CheckBox";
	case UIClass::UI_RadioBox: return L"RadioBox";
	case UIClass::UI_ProgressBar: return L"ProgressBar";
	case UIClass::UI_LoadingRing: return L"LoadingRing";
	case UIClass::UI_ProgressRing: return L"ProgressRing";
	case UIClass::UI_TreeView: return L"TreeView";
	case UIClass::UI_Panel: return L"Panel";
	case UIClass::UI_GroupBox: return L"GroupBox";
	case UIClass::UI_Expander: return L"Expander";
	case UIClass::UI_ScrollView: return L"ScrollView";
	case UIClass::UI_TabPage: return L"TabPage";
	case UIClass::UI_TabControl: return L"TabControl";
	case UIClass::UI_Switch: return L"Switch";
	case UIClass::UI_Menu: return L"Menu";
	case UIClass::UI_ToolBar: return L"ToolBar";
	case UIClass::UI_StatusBar: return L"StatusBar";
	case UIClass::UI_ToastHost: return L"ToastHost";
	case UIClass::UI_Slider: return L"Slider";
	case UIClass::UI_WebBrowser: return L"WebBrowser";
	case UIClass::UI_StackPanel: return L"StackPanel";
	case UIClass::UI_GridPanel: return L"GridPanel";
	case UIClass::UI_DockPanel: return L"DockPanel";
	case UIClass::UI_WrapPanel: return L"WrapPanel";
	case UIClass::UI_RelativePanel: return L"RelativePanel";
	case UIClass::UI_PictureBox: return L"PictureBox";
	case UIClass::UI_MediaPlayer: return L"MediaPlayer";
	default: return L"Control";
	}
}

namespace
{
	static bool TryParseNumericSuffix(const std::wstring& name, const std::wstring& prefix, int& outSuffix);
}

std::vector<std::shared_ptr<DesignerControl>> DesignerCanvas::GetAllControlsForExport() const
{
	std::vector<std::shared_ptr<DesignerControl>> out;
	out.reserve(_designerControls.size() + 64);

	std::unordered_map<Control*, std::shared_ptr<DesignerControl>> dcOf;
	dcOf.reserve(_designerControls.size() * 2 + 16);

	std::unordered_set<std::wstring> usedNames;
	usedNames.reserve(_designerControls.size() * 2 + 16);

	for (auto& dc : _designerControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		out.push_back(dc);
		dcOf[dc->ControlInstance] = dc;
		if (!dc->Name.empty()) usedNames.insert(dc->Name);
	}

	std::unordered_map<std::wstring, int> nextSuffixOf;
	nextSuffixOf.reserve(64);

	auto computeMaxSuffix = [&](const std::wstring& base) -> int {
		int maxSuf = 0;
		for (const auto& n : usedNames)
		{
			int suf = 0;
			if (TryParseNumericSuffix(n, base, suf))
				maxSuf = (std::max)(maxSuf, suf);
		}
		return maxSuf;
	};

	auto makeUniqueName = [&](UIClass t) -> std::wstring {
		std::wstring base = ExportTypeName(t);
		if (base.empty()) base = L"Control";
		auto it = nextSuffixOf.find(base);
		if (it == nextSuffixOf.end())
			it = nextSuffixOf.emplace(base, computeMaxSuffix(base)).first;

		for (int guard = 0; guard < 1000000; guard++)
		{
			it->second++;
			std::wstring cand = base + std::to_wstring(it->second);
			if (usedNames.insert(cand).second)
				return cand;
		}

		std::wstring cand = base + L"_auto";
		usedNames.insert(cand);
		return cand;
	};

	auto isInternalSurface = [&](Control* c) -> bool {
		return c == (Control*)_designSurface || c == (Control*)_clientSurface || c == (Control*)this;
	};

	Control* root = _clientSurface ? (Control*)_clientSurface : (_designSurface ? (Control*)_designSurface : (Control*)this);
	if (!root) return out;

	std::function<void(Control*)> walk;
	walk = [&](Control* parent)
	{
		if (!parent) return;
		for (int i = 0; i < parent->Count; i++)
		{
			auto* c = parent->operator[](i);
			if (!c) continue;
			if (isInternalSurface(c)) { walk(c); continue; }

			UIClass t = c->Type();
			if (IsExportableDesignType(t))
			{
				if (dcOf.find(c) == dcOf.end())
				{
					Control* designerParent = nullptr;
					auto* rp = c->Parent;
					if (rp && !isInternalSurface(rp) && rp != root)
						designerParent = rp;
					std::wstring name = makeUniqueName(t);
					auto dc = std::make_shared<DesignerControl>(
						c, name, t, designerParent, c->DesignId);
					out.push_back(dc);
					dcOf[c] = dc;
				}
			}

			walk(c);
		}
	};

	walk(root);
	return out;
}

CodeGenInput DesignerCanvas::BuildCodeGenInput() const
{
	CodeGenInput input;
	input.Controls = GetAllControlsForExport();
	const auto form = CaptureDesignedFormModel();
	input.FormText = form.Text;
	input.FormName = form.Name;
	input.FormSize = form.Size;
	input.FormLocation = form.Location;
	input.FormBackColor = form.BackColor;
	input.FormForeColor = form.ForeColor;
	input.FormShowInTaskBar = form.ShowInTaskBar;
	input.FormTopMost = form.TopMost;
	input.FormEnable = form.Enable;
	input.FormVisible = form.Visible;
	input.FormEventHandlers = form.EventHandlers;
	input.FormVisibleHead = form.VisibleHead;
	input.FormHeadHeight = form.HeadHeight;
	input.FormMinBox = form.MinBox;
	input.FormMaxBox = form.MaxBox;
	input.FormCloseBox = form.CloseBox;
	input.FormCenterTitle = form.CenterTitle;
	input.FormAllowResize = form.AllowResize;
	input.FormFontName = form.FontName;
	input.FormFontSize = form.FontSize;
	input.StyleSheet = _documentStyleSheet;
	return input;
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

	static std::string UIClassToString(UIClass t)
	{
		switch (t)
		{
			case UIClass::UI_Label: return "Label";
			case UIClass::UI_LinkLabel: return "LinkLabel";
			case UIClass::UI_Button: return "Button";
		case UIClass::UI_TextBox: return "TextBox";
		case UIClass::UI_RichTextBox: return "RichTextBox";
		case UIClass::UI_PasswordBox: return "PasswordBox";
			case UIClass::UI_DateTimePicker: return "DateTimePicker";
		case UIClass::UI_NumericUpDown: return "NumericUpDown";
		case UIClass::UI_Panel: return "Panel";
		case UIClass::UI_GroupBox: return "GroupBox";
		case UIClass::UI_Expander: return "Expander";
			case UIClass::UI_ScrollView: return "ScrollView";
		case UIClass::UI_StackPanel: return "StackPanel";
		case UIClass::UI_GridPanel: return "GridPanel";
		case UIClass::UI_DockPanel: return "DockPanel";
		case UIClass::UI_WrapPanel: return "WrapPanel";
		case UIClass::UI_RelativePanel: return "RelativePanel";
		case UIClass::UI_CheckBox: return "CheckBox";
		case UIClass::UI_RadioBox: return "RadioBox";
		case UIClass::UI_ComboBox: return "ComboBox";
		case UIClass::UI_ListView: return "ListView";
		case UIClass::UI_ListBox: return "ListBox";
		case UIClass::UI_GridView: return "GridView";
		case UIClass::UI_PropertyGrid: return "PropertyGrid";
		case UIClass::UI_ChartView: return "ChartView";
		case UIClass::UI_ReportView: return "ReportView";
		case UIClass::UI_KpiCard: return "KpiCard";
		case UIClass::UI_FilterBar: return "FilterBar";
		case UIClass::UI_TreeView: return "TreeView";
		case UIClass::UI_ProgressBar: return "ProgressBar";
		case UIClass::UI_LoadingRing: return "LoadingRing";
		case UIClass::UI_ProgressRing: return "ProgressRing";
		case UIClass::UI_Slider: return "Slider";
		case UIClass::UI_PictureBox: return "PictureBox";
		case UIClass::UI_Switch: return "Switch";
		case UIClass::UI_TabControl: return "TabControl";
		case UIClass::UI_ToolBar: return "ToolBar";
		case UIClass::UI_Menu: return "Menu";
		case UIClass::UI_StatusBar: return "StatusBar";
		case UIClass::UI_ToastHost: return "ToastHost";
		case UIClass::UI_WebBrowser: return "WebBrowser";
		case UIClass::UI_MediaPlayer: return "MediaPlayer";
		case UIClass::UI_TabPage: return "TabPage";
		default: return "Control";
		}
	}

	static bool TryParseUIClass(const std::string& s, UIClass& out)
	{
		if (s == "Label") { out = UIClass::UI_Label; return true; }
		if (s == "LinkLabel") { out = UIClass::UI_LinkLabel; return true; }
		if (s == "Button") { out = UIClass::UI_Button; return true; }
		if (s == "TextBox") { out = UIClass::UI_TextBox; return true; }
		if (s == "RichTextBox") { out = UIClass::UI_RichTextBox; return true; }
		if (s == "PasswordBox") { out = UIClass::UI_PasswordBox; return true; }
		if (s == "DateTimePicker") { out = UIClass::UI_DateTimePicker; return true; }
		if (s == "NumericUpDown") { out = UIClass::UI_NumericUpDown; return true; }
		if (s == "Panel") { out = UIClass::UI_Panel; return true; }
		if (s == "GroupBox") { out = UIClass::UI_GroupBox; return true; }
		if (s == "Expander") { out = UIClass::UI_Expander; return true; }
		if (s == "ScrollView") { out = UIClass::UI_ScrollView; return true; }
		if (s == "StackPanel") { out = UIClass::UI_StackPanel; return true; }
		if (s == "GridPanel") { out = UIClass::UI_GridPanel; return true; }
		if (s == "DockPanel") { out = UIClass::UI_DockPanel; return true; }
		if (s == "WrapPanel") { out = UIClass::UI_WrapPanel; return true; }
		if (s == "RelativePanel") { out = UIClass::UI_RelativePanel; return true; }
		if (s == "CheckBox") { out = UIClass::UI_CheckBox; return true; }
		if (s == "RadioBox") { out = UIClass::UI_RadioBox; return true; }
		if (s == "ComboBox") { out = UIClass::UI_ComboBox; return true; }
		if (s == "ListView") { out = UIClass::UI_ListView; return true; }
		if (s == "ListBox") { out = UIClass::UI_ListBox; return true; }
		if (s == "GridView") { out = UIClass::UI_GridView; return true; }
		if (s == "PropertyGrid") { out = UIClass::UI_PropertyGrid; return true; }
		if (s == "ChartView") { out = UIClass::UI_ChartView; return true; }
		if (s == "ReportView") { out = UIClass::UI_ReportView; return true; }
		if (s == "KpiCard") { out = UIClass::UI_KpiCard; return true; }
		if (s == "FilterBar") { out = UIClass::UI_FilterBar; return true; }
		if (s == "TreeView") { out = UIClass::UI_TreeView; return true; }
		if (s == "ProgressBar") { out = UIClass::UI_ProgressBar; return true; }
		if (s == "LoadingRing") { out = UIClass::UI_LoadingRing; return true; }
		if (s == "ProgressRing") { out = UIClass::UI_ProgressRing; return true; }
		if (s == "Slider") { out = UIClass::UI_Slider; return true; }
		if (s == "PictureBox") { out = UIClass::UI_PictureBox; return true; }
		if (s == "Switch") { out = UIClass::UI_Switch; return true; }
		if (s == "TabControl") { out = UIClass::UI_TabControl; return true; }
		if (s == "ToolBar") { out = UIClass::UI_ToolBar; return true; }
		if (s == "Menu") { out = UIClass::UI_Menu; return true; }
		if (s == "StatusBar") { out = UIClass::UI_StatusBar; return true; }
		if (s == "ToastHost") { out = UIClass::UI_ToastHost; return true; }
		if (s == "WebBrowser") { out = UIClass::UI_WebBrowser; return true; }
		if (s == "MediaPlayer") { out = UIClass::UI_MediaPlayer; return true; }
		if (s == "TabPage") { out = UIClass::UI_TabPage; return true; }
		return false;
	}

	static DesignValue MenuItemToValue(MenuItem* it)
	{
		if (!it) return DesignValue();
		DesignValue j = DesignValue::object();
		j["text"] = ToUtf8(it->Text);
		j["id"] = it->Id;
		j["shortcut"] = ToUtf8(it->Shortcut);
		j["separator"] = it->Separator;
		j["enable"] = it->Enable;
		DesignValue subs = DesignValue::array();
		for (auto* subItem : it->SubItems)
		{
			if (!subItem) continue;
			subs.push_back(MenuItemToValue(subItem));
		}
		j["subItems"] = subs;
		return j;
	}

	static void ValueToMenuSubItems(const DesignValue& arr, std::vector<MenuItem*>& out, MenuItem* owner)
	{
		if (!owner) return;
		if (!arr.is_array()) return;
		for (auto& j : arr)
		{
			if (!j.is_object()) continue;
			bool sep = j.value("separator", false);
			if (sep)
			{
				auto* separatorItem = owner->AddSeparator();
				if (!separatorItem) continue;
				continue;
			}
			auto text = FromUtf8(j.value("text", std::string()));
			int id = j.value("id", 0);
			auto* subItem = owner->AddSubItem(text, id);
			if (!subItem) continue;
			subItem->Shortcut = FromUtf8(j.value("shortcut", std::string()));
			subItem->Enable = j.value("enable", true);
			if (j.contains("subItems"))
			{
				ValueToMenuSubItems(j["subItems"], out, subItem);
			}
		}
	}

	static DesignValue ColorToValue(const D2D1_COLOR_F& c)
	{
		return DesignValue{ {"r", c.r}, {"g", c.g}, {"b", c.b}, {"a", c.a} };
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

	static std::string HAlignToString(HorizontalAlignment a)
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
	static bool TryParseHAlign(const std::string& s, HorizontalAlignment& out)
	{
		if (s == "Left") { out = HorizontalAlignment::Left; return true; }
		if (s == "Center") { out = HorizontalAlignment::Center; return true; }
		if (s == "Right") { out = HorizontalAlignment::Right; return true; }
		if (s == "Stretch") { out = HorizontalAlignment::Stretch; return true; }
		return false;
	}
	static std::string VAlignToString(VerticalAlignment a)
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
	static bool TryParseVAlign(const std::string& s, VerticalAlignment& out)
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
		case Dock::Fill: return "Fill";
		default: return "Fill";
		}
	}
	static bool TryParseDock(const std::string& s, Dock& out)
	{
		if (s == "Left") { out = Dock::Left; return true; }
		if (s == "Top") { out = Dock::Top; return true; }
		if (s == "Right") { out = Dock::Right; return true; }
		if (s == "Bottom") { out = Dock::Bottom; return true; }
		if (s == "Fill") { out = Dock::Fill; return true; }
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
		case SizeUnit::Percent: return "Percent";
		case SizeUnit::Auto: return "Auto";
		case SizeUnit::Star: return "Star";
		default: return "Pixel";
		}
	}
	static bool TryParseSizeUnit(const std::string& s, SizeUnit& out)
	{
		if (s == "Pixel") { out = SizeUnit::Pixel; return true; }
		if (s == "Percent") { out = SizeUnit::Percent; return true; }
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

	static int GetChildIndex(Control* parent, Control* child)
	{
		if (!parent || !child) return -1;
		for (int i = 0; i < parent->Count; i++)
		{
			if (parent->operator[](i) == child) return i;
		}
		return -1;
	}

	static DesignValue TreeNodesToValue(std::vector<TreeNode*>& nodes)
	{
		DesignValue arr = DesignValue::array();
		for (auto* node : nodes)
		{
			if (!node) continue;
			DesignValue one = DesignValue::object();
			one["text"] = ToUtf8(node->Text);
			one["expand"] = node->Expand;
			if (node->Children.size() > 0)
				one["children"] = TreeNodesToValue(node->Children);
			arr.push_back(one);
		}
		return arr;
	}

	static void ValueToTreeNodes(const DesignValue& j, std::vector<TreeNode*>& outNodes)
	{
		if (!j.is_array()) return;
		for (auto& it : j)
		{
			if (!it.is_object()) continue;
			auto text = FromUtf8(it.value("text", std::string()));
			auto* node = new TreeNode(text);
			node->Expand = it.value("expand", false);
			if (it.contains("children"))
				ValueToTreeNodes(it["children"], node->Children);
			outNodes.push_back(node);
		}
	}

	static DesignValue ListViewItemsToValue(const std::vector<ListViewItem>& items)
	{
		DesignValue arr = DesignValue::array();
		for (const auto& item : items)
		{
			DesignValue one = DesignValue::object();
			one["text"] = ToUtf8(item.Text);
			one["subText"] = ToUtf8(item.SubText);
			one["checked"] = item.Checked;
			one["selected"] = item.Selected;
			one["enabled"] = item.Enabled;
			if (!item.SubItems.empty())
			{
				DesignValue subs = DesignValue::array();
				for (const auto& sub : item.SubItems)
					subs.push_back(ToUtf8(sub));
				one["subItems"] = subs;
			}
			arr.push_back(one);
		}
		return arr;
	}

	static void ValueToListViewItems(const DesignValue& j, std::vector<ListViewItem>& outItems)
	{
		if (!j.is_array()) return;
		for (auto& it : j)
		{
			if (!it.is_object()) continue;
			ListViewItem item(FromUtf8(it.value("text", std::string())));
			item.SubText = FromUtf8(it.value("subText", std::string()));
			item.Checked = it.value("checked", false);
			item.Selected = it.value("selected", false);
			item.Enabled = it.value("enabled", true);
			if (it.contains("subItems") && it["subItems"].is_array())
			{
				for (auto& sj : it["subItems"])
					if (sj.is_string()) item.SubItems.push_back(FromUtf8(sj.get<std::string>()));
			}
			outItems.push_back(std::move(item));
		}
	}

	static DesignValue PropertyGridItemsToValue(const std::vector<PropertyGridItem>& items)
	{
		DesignValue arr = DesignValue::array();
		for (const auto& item : items)
		{
			DesignValue one = DesignValue::object();
			one["category"] = ToUtf8(item.Category);
			one["name"] = ToUtf8(item.Name);
			one["value"] = ToUtf8(item.Value);
			one["description"] = ToUtf8(item.Description);
			one["type"] = (int)item.ValueType;
			one["readOnly"] = item.ReadOnly;
			one["isMixed"] = item.IsMixed;
			one["canReset"] = item.CanReset;
			one["minimum"] = item.Minimum;
			one["maximum"] = item.Maximum;
			one["step"] = item.Step;
			one["tag"] = static_cast<unsigned long long>(item.Tag);
			if (!item.Options.empty())
			{
				DesignValue options = DesignValue::array();
				for (const auto& opt : item.Options)
					options.push_back(ToUtf8(opt));
				one["options"] = options;
			}
			arr.push_back(one);
		}
		return arr;
	}

	static void ValueToPropertyGridItems(const DesignValue& j, std::vector<PropertyGridItem>& outItems)
	{
		if (!j.is_array()) return;
		for (auto& it : j)
		{
			if (!it.is_object()) continue;
			PropertyGridItem item;
			item.Category = FromUtf8(it.value("category", std::string()));
			item.Name = FromUtf8(it.value("name", std::string()));
			item.Value = FromUtf8(it.value("value", std::string()));
			item.Description = FromUtf8(it.value("description", std::string()));
			item.ValueType = (PropertyGridValueType)it.value("type", (int)PropertyGridValueType::Text);
			item.ReadOnly = it.value("readOnly", false);
			item.IsMixed = it.value("isMixed", false);
			item.CanReset = it.value("canReset", false);
			item.Minimum = it.value("minimum", 0.0);
			item.Maximum = it.value("maximum", 1.0);
			item.Step = it.value("step", 0.01);
			item.Tag = static_cast<UINT64>(
				it.value("tag", static_cast<unsigned long long>(0)));
			if (it.contains("options") && it["options"].is_array())
			{
				for (auto& oj : it["options"])
					if (oj.is_string()) item.Options.push_back(FromUtf8(oj.get<std::string>()));
			}
			outItems.push_back(std::move(item));
		}
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
		document.NextStableId = _nextStableControlId;
		document.Form = CaptureDesignedFormModel();
		document.CodeBehind = _codeBehind;
		document.DataContextSchema = _dataContextSchema;
		DesignerDataContextSchemaUtils::Canonicalize(document.DataContextSchema);
		if (!DesignerDataContextSchemaUtils::Validate(document.DataContextSchema, outError))
			return false;
		document.StyleSheet = _documentStyleSheet;
		DesignerStyleSheetUtils::Canonicalize(document.StyleSheet);
		if (!DesignerStyleSheetUtils::Validate(document.StyleSheet, outError))
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

		// TabPage* -> pageId
		std::unordered_map<Control*, std::string> tabPageIdOf;
		tabPageIdOf.reserve(32);
		for (auto& dc : _designerControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			if (dc->Type != UIClass::UI_TabControl) continue;
			auto* tabControl = (TabControl*)dc->ControlInstance;
			for (int i = 0; i < tabControl->Count; i++)
			{
				auto* page = tabControl->operator[](i);
				if (!page) continue;
				std::wstring wid = dc->Name + L"#page" + std::to_wstring(i);
				tabPageIdOf[page] = ToUtf8(wid);
			}
		}

		for (auto& dc : _designerControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			if (dc->Type == UIClass::UI_TabPage) continue;
			auto* c = dc->ControlInstance;

			DesignerModel::DesignNode node;
			node.Id = dc->StableId;
			node.Name = dc->Name;
			node.Type = dc->Type;
			node.CustomType = dc->CustomType;
			node.CustomEvents = dc->CustomEvents;

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
					auto itPage = tabPageIdOf.find(dc->DesignerParent);
					if (itPage != tabPageIdOf.end()) node.ParentRef = FromUtf8(itPage->second);
					else node.ParentRef.clear();
				}
			}

			Control* runtimeParent = c->Parent ? c->Parent : (dc->DesignerParent ? dc->DesignerParent : (_clientSurface ? (Control*)_clientSurface : (Control*)_designSurface));
			node.Order = GetChildIndex(runtimeParent, c);

			DesignValue props = DesignValue::object();
			props["text"] = ToUtf8(c->Text);
			if (!c->GetStyleId().empty())
				props["styleId"] = ToUtf8(c->GetStyleId());
			if (!c->GetStyleClasses().empty())
			{
				DesignValue styleClasses = DesignValue::array();
				for (const auto& styleClass : c->GetStyleClasses())
					styleClasses.push_back(ToUtf8(styleClass));
				props["styleClasses"] = std::move(styleClasses);
			}
			props["location"] = DesignValue{ {"x", c->Location.x}, {"y", c->Location.y} };
			props["size"] = DesignValue{ {"w", c->Size.cx}, {"h", c->Size.cy} };
			// 字体：默认（跟随窗体/框架）不保存，显式字体才保存
			{
				::Font* f = c->Font;
				bool inherited = false;
				if (_designedFormSharedFont)
					inherited = (f == _designedFormSharedFont);
				else
					inherited = (f == GetDefaultFontObject());
				if (!inherited && f)
				{
					props["font"] = DesignValue{ {"name", ToUtf8(f->FontName)}, {"size", f->FontSize} };
				}
			}
			props["enable"] = c->Enable;
			props["visible"] = c->Visible;
			props["backColor"] = ColorToValue(c->BackColor);
			props["foreColor"] = ColorToValue(c->ForeColor);
			props["borderColor"] = ColorToValue(c->BorderColor);
			props["showValidationBorder"] = c->ShowValidationBorder;
			props["showValidationToolTip"] = c->ShowValidationToolTip;
			props["validationBorderThickness"] = c->ValidationBorderThickness;
			props["validationCornerRadius"] = c->ValidationCornerRadius;
			props["validationToolTipMaxWidth"] = c->ValidationToolTipMaxWidth;
			if (!c->AccessibleDescription.empty())
				props["accessibleDescription"] = ToUtf8(c->AccessibleDescription);
			props["margin"] = ThicknessToValue(c->Margin);
			props["padding"] = ThicknessToValue(c->Padding);
			props["anchor"] = (int)c->AnchorStyles;
			props["hAlign"] = HAlignToString(c->HAlign);
			props["vAlign"] = VAlignToString(c->VAlign);
			props["dock"] = DockToString(c->DockPosition);
			props["zIndex"] = c->ZIndex;
			props["gridRow"] = c->GridRow;
			props["gridColumn"] = c->GridColumn;
			props["gridRowSpan"] = c->GridRowSpan;
			props["gridColumnSpan"] = c->GridColumnSpan;
			props["sizeMode"] = (int)c->SizeMode;
			if (!dc->MetadataProperties.empty())
			{
				DesignValue metadataProperties = DesignValue::object();
				for (const auto& [propertyName, storedValue] : dc->MetadataProperties)
				{
					std::wstring canonicalName;
					DesignerStyleValue currentValue;
					if (!dc->CustomType.Empty()
						&& !c->FindPropertyMetadata(propertyName))
					{
						canonicalName = propertyName;
						currentValue = storedValue;
					}
					else
					{
						std::wstring metadataError;
						if (!DesignerPropertyCatalog::CaptureValue(
							*c, propertyName, &canonicalName, currentValue,
							&metadataError))
						{
							if (outError) *outError = L"控件 " + dc->Name
								+ L"：" + metadataError;
							return false;
						}
					}
					metadataProperties[ToUtf8(canonicalName)] = DesignValue{
						{ "kind", ToUtf8(DesignerStyleSheetUtils::ValueKindName(currentValue.Kind)) },
						{ "value", ToUtf8(currentValue.Text) }
					};
				}
				props["metadata"] = std::move(metadataProperties);
			}
			node.Props = std::move(props);

			DesignValue extra = DesignValue::object();
			if (dc->Type == UIClass::UI_ComboBox)
			{
				auto* comboBox = (ComboBox*)c;
				DesignValue items = DesignValue::array();
				for (size_t i = 0; i < comboBox->Items.size(); ++i)
					items.push_back(ToUtf8(comboBox->Items[i]));
				extra["items"] = items;
			}
			else if (dc->Type == UIClass::UI_ProgressBar)
			{
				extra["percentageValue"] = ((ProgressBar*)c)->PercentageValue;
			}
			else if (dc->Type == UIClass::UI_LoadingRing)
			{
				extra["active"] = ((LoadingRing*)c)->Active;
			}
			else if (dc->Type == UIClass::UI_ProgressRing)
			{
				auto* progressRing = (ProgressRing*)c;
				extra["percentageValue"] = progressRing->PercentageValue;
				extra["showPercentage"] = progressRing->ShowPercentage;
			}
			else if (dc->Type == UIClass::UI_DateTimePicker)
			{
				auto* dateTimePicker = (DateTimePicker*)c;
				const SYSTEMTIME st = dateTimePicker->Value;
				extra["value"] = DesignValue{
					{"year", st.wYear},
					{"month", st.wMonth},
					{"day", st.wDay},
					{"hour", st.wHour},
					{"minute", st.wMinute},
					{"second", st.wSecond},
					{"milliseconds", st.wMilliseconds}
				};
				extra["mode"] = (int)dateTimePicker->Mode;
				extra["allowDateSelection"] = dateTimePicker->AllowDateSelection;
				extra["allowTimeSelection"] = dateTimePicker->AllowTimeSelection;
				extra["allowModeSwitch"] = dateTimePicker->AllowModeSwitch;
				extra["expand"] = dateTimePicker->Expand;
			}
			else if (dc->Type == UIClass::UI_ListView || dc->Type == UIClass::UI_ListBox)
			{
				auto* listView = (ListView*)c;
				DesignValue cols = DesignValue::array();
				for (auto& col : listView->Columns)
				{
					DesignValue cj = DesignValue::object();
					cj["header"] = ToUtf8(col.Header);
					cj["width"] = col.Width;
					cj["align"] = (int)col.Align;
					cols.push_back(cj);
				}
				extra["columns"] = cols;
				extra["items"] = ListViewItemsToValue(listView->Items);
			}
			else if (dc->Type == UIClass::UI_GridView)
			{
				auto* gridView = (GridView*)c;
				DesignValue cols = DesignValue::array();
				for (size_t i = 0; i < gridView->ColumnCount(); ++i)
				{
					auto& col = gridView->ColumnAt(static_cast<int>(i));
					DesignValue cj = DesignValue::object();
					cj["name"] = ToUtf8(col.Name);
					cj["width"] = col.Width;
					cj["type"] = (int)col.Type;
					cj["canEdit"] = col.CanEdit;
					if (!col.ButtonText.empty())
						cj["buttonText"] = ToUtf8(col.ButtonText);
					if (!col.ComboBoxItems.empty())
					{
						DesignValue items = DesignValue::array();
						for (const auto& item : col.ComboBoxItems)
							items.push_back(ToUtf8(item));
						cj["comboBoxItems"] = std::move(items);
					}
					cols.push_back(cj);
				}
				extra["columns"] = cols;
			}
			else if (dc->Type == UIClass::UI_PropertyGrid)
			{
				auto* pg = (PropertyGridView*)c;
				extra["items"] = PropertyGridItemsToValue(pg->Items);
			}
			else if (dc->Type == UIClass::UI_TreeView)
			{
				auto* treeView = (TreeView*)c;
				if (treeView->Root)
					extra["nodes"] = TreeNodesToValue(treeView->Root->Children);
				extra["selectedBackColor"] = ColorToValue(treeView->SelectedBackColor);
				extra["underMouseItemBackColor"] = ColorToValue(treeView->UnderMouseItemBackColor);
				extra["selectedForeColor"] = ColorToValue(treeView->SelectedForeColor);
			}
			else if (dc->Type == UIClass::UI_TabControl)
			{
				auto* tabControl = (TabControl*)c;
				DesignValue pages = DesignValue::array();
				for (int i = 0; i < tabControl->Count; i++)
				{
					auto* page = tabControl->operator[](i);
					if (!page) continue;
					DesignValue pj = DesignValue::object();
					std::wstring wid = dc->Name + L"#page" + std::to_wstring(i);
					pj["id"] = ToUtf8(wid);
					pj["text"] = ToUtf8(page->Text);
					pages.push_back(pj);
				}
				extra["pages"] = pages;
			}
			else if (dc->Type == UIClass::UI_GridPanel)
			{
				auto* gridPanel = (GridPanel*)c;
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
			else if (dc->Type == UIClass::UI_StatusBar)
			{
				auto* statusBar = (StatusBar*)c;
				DesignValue parts = DesignValue::array();
				for (int i = 0; i < statusBar->PartCount(); i++)
				{
					DesignValue pj = DesignValue::object();
					pj["text"] = ToUtf8(statusBar->GetPartText(i));
					pj["width"] = statusBar->GetPartWidth(i);
					parts.push_back(pj);
				}
				extra["parts"] = parts;
			}
			else if (dc->Type == UIClass::UI_Menu)
			{
				auto* m = (Menu*)c;
				DesignValue tops = DesignValue::array();
				for (int i = 0; i < m->Count; i++)
				{
					auto* it = dynamic_cast<MenuItem*>(m->operator[](i));
					if (!it) continue;
					tops.push_back(MenuItemToValue(it));
				}
				extra["items"] = tops;
			}
			else if (dc->Type == UIClass::UI_MediaPlayer)
			{
				auto* mediaPlayer = (MediaPlayer*)c;
				// 设计期保存：媒体源路径放在 DesignStrings 中，避免加载文件也能保持可往返。
				auto it = dc->DesignStrings.find(L"mediaFile");
				std::wstring mediaFile = (it != dc->DesignStrings.end()) ? it->second : mediaPlayer->MediaFile;
				if (!mediaFile.empty()) extra["mediaFile"] = ToUtf8(mediaFile);
			}

			if (auto* splitParent = AsSplitContainer(dc->DesignerParent))
			{
				std::string splitRegion = GetSplitRegionKey(splitParent, runtimeParent);
				if (!splitRegion.empty())
				{
					extra["splitRegion"] = splitRegion;
				}
			}

			node.Extra = std::move(extra);

			// Current documents persist the resolved member-function name. Boolean
			// values remain read-compatible for legacy documents only.
			if (!dc->EventHandlers.empty())
			{
				DesignValue ev = DesignValue::object();
				for (const auto& kv : dc->EventHandlers)
				{
					if (kv.first.empty()) continue;
					const auto handlerName =
						DesignerEventCatalog::ResolveHandlerName(
							kv.second, dc->Name, kv.first);
					if (handlerName.empty()) continue;
					ev[ToUtf8(kv.first)] = ToUtf8(handlerName);
				}
				node.Events = std::move(ev);
			}

			if (!dc->DataBindings.empty())
			{
				DesignValue bindings = DesignValue::object();
				for (const auto& [targetProperty, binding] : dc->DataBindings)
				{
					const BindingPropertyMetadata* metadata = nullptr;
					std::wstring validationError;
					const auto* customProperty = FindCustomProperty(
						*dc, targetProperty);
					const bool deferredCustomProperty = customProperty
						&& !c->FindPropertyMetadata(targetProperty);
					const bool valid = deferredCustomProperty
						? DesignerBindingUtils::ValidateTarget(
							CustomBindingTargetMetadata(*customProperty),
							binding, &validationError, &_dataContextSchema)
						: DesignerBindingUtils::Validate(
							*c, targetProperty, binding, &metadata,
							&validationError, &_dataContextSchema);
					if (!valid)
					{
						if (outError) *outError = L"控件 " + dc->Name + L"：" + validationError;
						return false;
					}

					DesignValue bindingDefinition{
						{ "source", ToUtf8(binding.SourceProperty) },
						{ "mode", static_cast<int>(binding.Mode) },
						{ "updateMode", static_cast<int>(binding.UpdateMode) }
					};
					const auto converterName = DesignerBindingUtils::Trim(binding.Converter);
					if (!converterName.empty())
					{
						const auto registered = BindingValueConverterRegistry::Find(converterName);
						bindingDefinition["converter"] = ToUtf8(
							registered ? registered->Name : converterName);
					}
					const auto& persistedTarget = deferredCustomProperty
						? customProperty->Name : metadata->Name();
					bindings[ToUtf8(persistedTarget)] = std::move(bindingDefinition);
				}
				node.Bindings = std::move(bindings);
			}
			document.Nodes.push_back(std::move(node));
		}

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

bool DesignerCanvas::BuildEventHandlerIndex(
	DesignerModel::DesignDocumentEventIndex& index,
	std::wstring* outError) const
{
	DesignerModel::DesignDocument document;
	document.Form.Name = _designedFormName;
	document.Form.EventHandlers = _designedFormEventHandlers;
	document.Nodes.reserve(_designerControls.size());
	for (const auto& control : _designerControls)
	{
		if (!control) continue;
		DesignerModel::DesignNode node;
		node.Id = control->StableId;
		node.Name = control->Name;
		node.Type = control->Type;
		node.CustomType = control->CustomType;
		node.CustomEvents = control->CustomEvents;
		for (const auto& [eventName, handler] : control->EventHandlers)
			if (!eventName.empty() && !handler.empty())
				node.Events[ToUtf8(eventName)] = ToUtf8(handler);
		document.Nodes.push_back(std::move(node));
	}
	return DesignerModel::DesignDocumentEventIndex::Build(
		document, index, outError);
}

bool DesignerCanvas::RenameEventHandler(
	const std::wstring& oldName,
	const std::wstring& newName,
	size_t* outRenamedReferenceCount,
	std::wstring* outError)
{
	if (outRenamedReferenceCount) *outRenamedReferenceCount = 0;
	DesignerModel::DesignDocument document;
	if (!BuildDesignDocument(document, outError)) return false;
	size_t renamed = 0;
	if (!DesignerModel::DesignDocumentEventIndex::RenameHandler(
		document, oldName, newName, &renamed, outError)) return false;
	if (renamed == 0)
	{
		if (outError) outError->clear();
		return true;
	}

	std::unordered_map<int, const DesignerModel::DesignNode*> nodesById;
	nodesById.reserve(document.Nodes.size());
	for (const auto& node : document.Nodes) nodesById.emplace(node.Id, &node);
	for (const auto& control : _designerControls)
	{
		if (!control) continue;
		const auto found = nodesById.find(control->StableId);
		if (found == nodesById.end())
		{
			if (outError) *outError = L"重命名事件时无法按稳定 ID 找到控件："
				+ control->Name;
			return false;
		}
		std::map<std::wstring, std::wstring> nextHandlers;
		const auto& events = found->second->Events;
		if (events.is_object())
		{
			for (const auto& [eventName, value] : events.ObjectItems())
			{
				if (value.is_string())
				{
					auto handler = FromUtf8(value.get<std::string>());
					if (!handler.empty())
						nextHandlers.emplace(FromUtf8(eventName), std::move(handler));
				}
				else if (value.is_boolean() && value.get<bool>())
					nextHandlers.emplace(FromUtf8(eventName), L"1");
			}
		}
		control->EventHandlers = std::move(nextHandlers);
	}
	_designedFormEventHandlers = std::move(document.Form.EventHandlers);
	if (outRenamedReferenceCount) *outRenamedReferenceCount = renamed;
	if (outError) outError->clear();
	return true;
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
	std::wstring* outError)
{
	for (const auto& node : document.Nodes)
	{
		if (node.CustomType.Empty()) continue;
		const auto installed = _customControlDescriptors.find(
			node.CustomType.RegistryKey());
		if (installed == _customControlDescriptors.end()
			|| installed->second.CustomType != node.CustomType
			|| installed->second.Type != node.Type)
			continue;
		for (const auto& [eventKey, handlerValue] : node.Events.ObjectItems())
		{
			if ((handlerValue.is_boolean() && !handlerValue.get<bool>())
				|| (handlerValue.is_string()
					&& TrimWs(FromUtf8(handlerValue.get<std::string>())).empty()))
				continue;
			const auto eventName = FromUtf8(eventKey);
			const auto persistedEvent = std::find_if(
				node.CustomEvents.begin(), node.CustomEvents.end(),
				[&](const auto& event)
				{ return _wcsicmp(event.Name.c_str(), eventName.c_str()) == 0; });
			if (persistedEvent == node.CustomEvents.end()) continue;
			const auto installedEvent = std::find_if(
				installed->second.CustomEvents.begin(),
				installed->second.CustomEvents.end(),
				[&](const auto& event)
				{
					return _wcsicmp(event.Name.c_str(), eventName.c_str()) == 0;
				});
			if (installedEvent == installed->second.CustomEvents.end()
				|| installedEvent->EventField != persistedEvent->EventField
				|| installedEvent->Signature != persistedEvent->Signature)
			{
				if (outError)
					*outError = L"控件“" + node.Name + L"”使用的自定义事件“"
						+ eventName
						+ L"”与当前控件清单不兼容；请恢复原事件名称、field 和 signature，"
							L"或先移除该事件绑定。";
				return false;
			}
		}
	}

	DesignerModel::MaterializedControlTree materialized;
	DesignerModel::DesignDocumentMaterializationOptions materializationOptions;
	materializationOptions.ControlFactory =
		[](UIClass type) { return DesignerControlFactory::Create(type); };
	materializationOptions.CustomControlFactory =
		[this](const DesignerModel::DesignNode& node) -> std::unique_ptr<Control>
		{
			const auto found = _customControlDescriptors.find(
				node.CustomType.RegistryKey());
			if (found == _customControlDescriptors.end()
				|| found->second.CustomType != node.CustomType
				|| found->second.Type != node.Type
				|| !found->second.PreviewFactory)
				return nullptr;
			return found->second.PreviewFactory(0, 0);
		};
	materializationOptions.AllowCustomControlProxy = true;
	materializationOptions.AllowDeferredCustomMetadata = true;
	if (!DesignerModel::DesignDocumentMaterializer::Materialize(
		document, materialized, materializationOptions, outError))
		return false;

	try
	{
		ClearCanvasCore();
		_controlTypeCounters.clear();
		ApplyDesignedFormModel(document.Form);
		if (!SetCodeBehind(document.CodeBehind, outError)) return false;
		_dataContextSchema = document.DataContextSchema;
		DesignerDataContextSchemaUtils::Canonicalize(_dataContextSchema);
		_nextStableControlId = document.NextStableId;

		// A detached runtime tree uses the framework default for controls without
		// an explicit font. The design surface instead previews the form font.
		std::vector<Control*> pending;
		for (const auto& root : materialized.Roots)
			if (root) pending.push_back(root.get());
		while (!pending.empty())
		{
			auto* control = pending.back();
			pending.pop_back();
			if (!control) continue;
			if (control->Font == GetDefaultFontObject())
				control->SetFontEx(_designedFormSharedFont, false);
			for (auto* child : control->Children)
				if (child) pending.push_back(child);
		}

		if (!_clientSurface)
		{
			if (outError) *outError = L"设计器客户区不可用。";
			return false;
		}
		for (auto& root : materialized.Roots)
			if (root) _clientSurface->AddOwned(std::move(root));

		_designerControls = std::move(materialized.Controls);
		for (const auto& control : _designerControls)
		{
			if (!control || control->CustomType.Empty()) continue;
			const auto found = _customControlDescriptors.find(
				control->CustomType.RegistryKey());
			if (found != _customControlDescriptors.end()
				&& found->second.CustomType == control->CustomType)
			{
				control->CustomProperties = found->second.CustomProperties;
				control->CustomEvents = found->second.CustomEvents;
				if (control->ControlInstance)
				{
					AttachPreviewPropertySink(*control);
					for (const auto& property : control->CustomProperties)
					{
						const auto stored = control->MetadataProperties.find(
							property.Name);
						(void)DesignerPreviewBridge::SetValue(
							*control->ControlInstance, property.Name,
							stored == control->MetadataProperties.end()
								? property.DefaultValue : stored->second);
					}
				}
			}
		}
		for (const auto& control : _designerControls)
			if (control)
				UpdateDefaultNameCounterFromName(
					control->Type, control->Name);

		if (!SetDocumentStyleSheet(document.StyleSheet, outError))
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
		OnControlSelected(nullptr);
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
