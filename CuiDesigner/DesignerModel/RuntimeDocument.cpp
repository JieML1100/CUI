#include "RuntimeDocument.h"
#include "DesignDocumentEventIndex.h"
#include "DesignDocumentFileFormat.h"

#include "DesignDocumentMaterializer.h"
#include "DesignDocumentSerializer.h"
#include "RuntimeDocumentTopologyReloader.h"
#include "XamlDocumentParser.h"
#include "../DesignerBindingUtils.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerStyleSheetUtils.h"
#include "../../CUI/include/Form.h"

#include <Convert.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <utility>

namespace DesignerModel
{
std::wstring RuntimeCustomControlRegistry::MakeKey(
	const std::wstring& xamlNamespace,
	const std::wstring& xamlName)
{
	return xamlNamespace + L"|" + xamlName;
}

bool RuntimeCustomControlRegistry::Register(
	std::wstring xamlNamespace,
	std::wstring xamlName,
	Factory factory,
	std::wstring* outError)
{
	if (xamlNamespace.empty() || xamlName.empty() || !factory)
	{
		if (outError)
			*outError = L"自定义控件注册需要非空命名空间、类型名和工厂。";
		return false;
	}
	std::scoped_lock lock(_mutex);
	_factories[MakeKey(xamlNamespace, xamlName)] = std::move(factory);
	if (outError) outError->clear();
	return true;
}

bool RuntimeCustomControlRegistry::Unregister(
	const std::wstring& xamlNamespace,
	const std::wstring& xamlName) noexcept
{
	std::scoped_lock lock(_mutex);
	return _factories.erase(MakeKey(xamlNamespace, xamlName)) != 0;
}

std::unique_ptr<Control> RuntimeCustomControlRegistry::Create(
	const DesignNode& node) const
{
	Factory factory;
	{
		std::scoped_lock lock(_mutex);
		const auto found = _factories.find(node.CustomType.RegistryKey());
		if (found == _factories.end()) return {};
		factory = found->second;
	}
	return factory ? factory(node) : nullptr;
}

namespace
{
	void SetError(std::wstring* output, std::wstring value)
	{
		if (output) *output = std::move(value);
	}

	DesignDocumentMaterializationOptions MaterializationOptionsFor(
		const RuntimeDocumentLoadOptions& options)
	{
		DesignDocumentMaterializationOptions result;
		result.AllowCustomControlProxy = options.AllowCustomControlProxy;
		result.AllowDeferredCustomMetadata = options.AllowCustomControlProxy;
		if (options.CustomControls)
		{
			auto registry = options.CustomControls;
			result.CustomControlFactory =
				[registry](const DesignNode& node)
				{
					return registry->Create(node);
				};
		}
		return result;
	}

	XamlDocumentParseOptions XamlOptionsFor(
		std::shared_ptr<const RuntimeCustomControlRegistry> registry)
	{
		XamlDocumentParseOptions result;
		if (registry)
		{
			result.CustomControlFactory =
				[registry = std::move(registry)](const DesignNode& node)
				{
					return registry->Create(node);
				};
		}
		return result;
	}

	class FormRuntimeDocumentRootHost final
		: public RuntimeDocumentRootHost
	{
	public:
		explicit FormRuntimeDocumentRootHost(::Form& form) noexcept
			: _form(&form)
		{
		}

		bool DetachRoots(
			std::span<Control* const> roots,
			std::vector<std::unique_ptr<Control>>& output,
			std::wstring* outError) override
		{
			if (_transactionOpen)
			{
				SetError(outError, L"根宿主已经存在未完成的替换事务。");
				return false;
			}
			if (!output.empty())
			{
				SetError(outError, L"根宿主分离输出必须为空。");
				return false;
			}

			try
			{
				std::unordered_set<Control*> unique;
				std::vector<size_t> order;
				std::vector<int> slots;
				std::vector<std::unique_ptr<Control>> detached(roots.size());
				unique.reserve(roots.size());
				order.reserve(roots.size());
				slots.reserve(roots.size());
				for (size_t index = 0; index < roots.size(); ++index)
				{
					auto* root = roots[index];
					const auto slot = _form->IndexOfControl(root);
					if (!root || slot < 0 || !unique.insert(root).second)
					{
						SetError(outError,
							L"运行时文档根不再完整属于指定 Form 宿主。");
						return false;
					}
					order.push_back(index);
					slots.push_back(slot);
				}
				std::sort(order.begin(), order.end(), [&](size_t left, size_t right)
				{
					return slots[left] > slots[right];
				});

				for (const auto index : order)
				{
					detached[index] = _form->DetachControl(roots[index]);
					if (detached[index]) continue;
					std::sort(order.begin(), order.end(), [&](size_t left, size_t right)
					{
						return slots[left] < slots[right];
					});
					for (const auto restore : order)
					{
						if (!detached[restore]) continue;
						(void)_form->TryInsertOwned(
							slots[restore], detached[restore]);
					}
					SetError(outError, L"Form 无法原子分离运行时文档根。");
					return false;
				}

				_slots = std::move(slots);
				_transactionOpen = true;
				output = std::move(detached);
				if (outError) outError->clear();
				return true;
			}
			catch (...)
			{
				SetError(outError, L"Form 准备根控件事务时资源分配失败。");
				return false;
			}
		}

		bool AttachRoots(
			std::vector<std::unique_ptr<Control>>& roots,
			RuntimeRootHostAttachMode mode,
			std::wstring* outError) override
		{
			const bool transactionAttach =
				mode != RuntimeRootHostAttachMode::Initial;
			if (transactionAttach != _transactionOpen)
			{
				SetError(outError, transactionAttach
					? L"根宿主没有可提交的分离事务。"
					: L"根宿主事务未完成，不能执行初始挂载。");
				return false;
			}
			if (mode == RuntimeRootHostAttachMode::Rollback
				&& roots.size() != _slots.size())
			{
				SetError(outError, L"回滚根数量与分离快照不一致。");
				return false;
			}
			if (std::any_of(roots.begin(), roots.end(),
				[](const auto& root) { return !root; }))
			{
				SetError(outError, L"根宿主不能挂载空控件所有者。");
				return false;
			}

			try
			{
				_form->Controls.reserve(_form->Controls.size() + roots.size());
				const int anchor = _transactionOpen && !_slots.empty()
					? *std::min_element(_slots.begin(), _slots.end())
					: static_cast<int>(_form->Controls.size());
				std::vector<Control*> attached(roots.size(), nullptr);
				std::vector<size_t> attachOrder(roots.size());
				for (size_t index = 0; index < attachOrder.size(); ++index)
					attachOrder[index] = index;
				if (mode == RuntimeRootHostAttachMode::Rollback)
					std::sort(
						attachOrder.begin(), attachOrder.end(),
						[&](size_t left, size_t right)
						{ return _slots[left] < _slots[right]; });
				for (const auto index : attachOrder)
				{
					const int slot = mode == RuntimeRootHostAttachMode::Rollback
						? _slots[index] : anchor + static_cast<int>(index);
					auto* raw = roots[index].get();
					if (_form->TryInsertOwned(
						(std::clamp)(slot, 0,
							static_cast<int>(_form->Controls.size())),
						roots[index]))
					{
						attached[index] = raw;
						continue;
					}

					for (auto rollback = attachOrder.rbegin();
						rollback != attachOrder.rend(); ++rollback)
					{
						if (!attached[*rollback]) continue;
						roots[*rollback] =
							_form->DetachControl(attached[*rollback]);
					}
					SetError(outError, L"Form 无法原子挂载运行时文档根。");
					return false;
				}

				roots.clear();
				if (transactionAttach)
				{
					_slots.clear();
					_transactionOpen = false;
				}
				if (outError) outError->clear();
				return true;
			}
			catch (...)
			{
				SetError(outError, L"Form 挂载根控件时资源分配失败。");
				return false;
			}
		}

	private:
		::Form* _form = nullptr;
		std::vector<int> _slots;
		bool _transactionOpen = false;
	};

	struct FormPresentationSnapshot
	{
		::Form* Target = nullptr;
		std::wstring Text;
		POINT Location{};
		SIZE Size{};
		D2D1_COLOR_F BackColor{};
		D2D1_COLOR_F ForeColor{};
		bool ShowInTaskBar = true;
		bool TopMost = false;
		bool Enable = true;
		bool Visible = true;
		bool VisibleHead = true;
		int HeadHeight = 24;
		bool MinBox = true;
		bool MaxBox = true;
		bool CloseBox = true;
		bool CenterTitle = true;
		bool AllowResize = true;
		bool UsesDefaultFont = true;
		bool OwnsConfiguredFont = false;
		::Font* ConfiguredFont = nullptr;
		std::unique_ptr<::Font> OwnedFontBackup;

		static FormPresentationSnapshot Capture(::Form& form)
		{
			FormPresentationSnapshot snapshot;
			snapshot.Target = &form;
			snapshot.Text = form.Text;
			snapshot.Location = form.Location;
			snapshot.Size = form.Size;
			snapshot.BackColor = form.BackColor;
			snapshot.ForeColor = form.ForeColor;
			snapshot.ShowInTaskBar = form.ShowInTaskBar;
			snapshot.TopMost = form.TopMost;
			snapshot.Enable = form.Enable;
			snapshot.Visible = form.Visible;
			snapshot.VisibleHead = form.VisibleHead;
			snapshot.HeadHeight = form.HeadHeight;
			snapshot.MinBox = form.MinBox;
			snapshot.MaxBox = form.MaxBox;
			snapshot.CloseBox = form.CloseBox;
			snapshot.CenterTitle = form.CenterTitle;
			snapshot.AllowResize = form.AllowResize;
			snapshot.UsesDefaultFont = form.UsesDefaultFont();
			if (auto* font = form.GetConfiguredFont())
			{
				snapshot.ConfiguredFont = font;
				snapshot.OwnsConfiguredFont = form.OwnsConfiguredFont();
				if (snapshot.OwnsConfiguredFont)
					snapshot.OwnedFontBackup =
						std::make_unique<::Font>(font->FontName, font->FontSize);
			}
			return snapshot;
		}

		void Restore() noexcept
		{
			if (!Target) return;
			try
			{
				Target->Text = Text;
				Target->Location = Location;
				Target->Size = Size;
				Target->BackColor = BackColor;
				Target->ForeColor = ForeColor;
				Target->ShowInTaskBar = ShowInTaskBar;
				Target->TopMost = TopMost;
				Target->Enable = Enable;
				Target->VisibleHead = VisibleHead;
				Target->HeadHeight = HeadHeight;
				Target->MinBox = MinBox;
				Target->MaxBox = MaxBox;
				Target->CloseBox = CloseBox;
				Target->CenterTitle = CenterTitle;
				Target->AllowResize = AllowResize;
				Target->Visible = Visible;
				if (UsesDefaultFont)
					Target->SetFontEx(nullptr, false);
				else if (Target->GetConfiguredFont() == ConfiguredFont)
					Target->SetFontEx(ConfiguredFont, OwnsConfiguredFont);
				else if (OwnsConfiguredFont && OwnedFontBackup)
					Target->SetFontEx(OwnedFontBackup.release(), true);
				else
					Target->SetFontEx(ConfiguredFont, false);
			}
			catch (...) {}
		}
	};

	bool HasSameFormPresentation(
		const DesignFormModel& left,
		const DesignFormModel& right) noexcept
	{
		return left.Text == right.Text
			&& left.FontName == right.FontName
			&& left.FontSize == right.FontSize
			&& left.Size.cx == right.Size.cx
			&& left.Size.cy == right.Size.cy
			&& left.Location.x == right.Location.x
			&& left.Location.y == right.Location.y
			&& left.BackColor.r == right.BackColor.r
			&& left.BackColor.g == right.BackColor.g
			&& left.BackColor.b == right.BackColor.b
			&& left.BackColor.a == right.BackColor.a
			&& left.ForeColor.r == right.ForeColor.r
			&& left.ForeColor.g == right.ForeColor.g
			&& left.ForeColor.b == right.ForeColor.b
			&& left.ForeColor.a == right.ForeColor.a
			&& left.ShowInTaskBar == right.ShowInTaskBar
			&& left.TopMost == right.TopMost
			&& left.Enable == right.Enable
			&& left.Visible == right.Visible
			&& left.VisibleHead == right.VisibleHead
			&& left.HeadHeight == right.HeadHeight
			&& left.MinBox == right.MinBox
			&& left.MaxBox == right.MaxBox
			&& left.CloseBox == right.CloseBox
			&& left.CenterTitle == right.CenterTitle
			&& left.AllowResize == right.AllowResize;
	}

	bool ApplyFormModel(
		const DesignFormModel& model,
		::Form& form,
		std::wstring* outError)
	{
		try
		{
			form.Text = model.Text;
			form.Location = model.Location;
			form.Size = model.Size;
			form.BackColor = model.BackColor;
			form.ForeColor = model.ForeColor;
			form.ShowInTaskBar = model.ShowInTaskBar;
			form.TopMost = model.TopMost;
			form.Enable = model.Enable;
			form.VisibleHead = model.VisibleHead;
			form.HeadHeight = model.HeadHeight;
			form.MinBox = model.MinBox;
			form.MaxBox = model.MaxBox;
			form.CloseBox = model.CloseBox;
			form.CenterTitle = model.CenterTitle;
			form.AllowResize = model.AllowResize;

			auto* defaultFont = GetDefaultFontObject();
			const auto defaultName = defaultFont
				? defaultFont->FontName : std::wstring(L"Arial");
			const float defaultSize = defaultFont
				? defaultFont->FontSize : 18.0f;
			const auto fontName = model.FontName.empty()
				? defaultName : model.FontName;
			if (model.FontName.empty()
				&& fontName == defaultName
				&& std::fabs(model.FontSize - defaultSize) < 1e-6f)
				form.SetFontEx(nullptr, false);
			else
				form.SetFontEx(new ::Font(fontName, model.FontSize), true);
			form.Visible = model.Visible;
			if (outError) outError->clear();
			return true;
		}
		catch (const std::exception&)
		{
			SetError(outError, L"应用动态文档窗体属性时资源初始化失败。");
			return false;
		}
		catch (...)
		{
			SetError(outError, L"应用动态文档窗体属性时发生未知异常。");
			return false;
		}
	}

	bool HasConfiguredControlEvents(const RuntimeDocument& document)
	{
		return std::any_of(
			document.Controls().begin(), document.Controls().end(),
			[](const auto& control)
			{
				return control && !control->EventHandlers.empty();
			});
	}

	std::wstring FromUtf8(const std::string& value)
	{
		return Convert::Utf8ToUnicode(value);
	}

	bool IsSupportedInPlacePropertyKey(const std::string& key)
	{
		static const std::unordered_set<std::string> supported{
			"text", "styleId", "styleClasses", "location", "size",
			"enable", "visible", "backColor", "foreColor", "borderColor",
			"bolderColor", "showValidationBorder", "showValidationToolTip",
			"validationBorderThickness", "validationCornerRadius",
			"validationToolTipMaxWidth", "accessibleDescription", "margin",
			"padding", "anchor", "hAlign", "vAlign", "dock", "zIndex",
			"gridRow", "gridColumn", "gridRowSpan", "gridColumnSpan",
			"sizeMode", "metadata"
		};
		return supported.contains(key);
	}

	bool HasOnlySupportedInPlacePropertyChanges(
		const DesignValue& current,
		const DesignValue& next)
	{
		if (current == next) return true;
		if (!current.is_object() || !next.is_object()) return false;
		std::set<std::string> keys;
		for (const auto& [key, value] : current.ObjectItems())
		{
			(void)value;
			keys.insert(key);
		}
		for (const auto& [key, value] : next.ObjectItems())
		{
			(void)value;
			keys.insert(key);
		}
		for (const auto& key : keys)
		{
			if (current[key] != next[key]
				&& !IsSupportedInPlacePropertyKey(key)) return false;
		}
		return true;
	}

	bool SameNodeShapeForInPlaceReload(
		const DesignNode& left,
		const DesignNode& right)
	{
		return left.Id == right.Id
			&& left.ParentId == right.ParentId
			&& left.ParentRef == right.ParentRef
			&& left.Name == right.Name
			&& left.Type == right.Type
			&& left.CustomType == right.CustomType
			&& left.Order == right.Order
			&& left.Extra == right.Extra
			&& HasOnlySupportedInPlacePropertyChanges(left.Props, right.Props);
	}

	bool CanReloadInPlace(
		const DesignDocument& current,
		const DesignDocument& next)
	{
		if (current.Schema != next.Schema
			|| current.Form.Name != next.Form.Name
			|| current.Form.EventHandlers != next.Form.EventHandlers
			|| current.Nodes.size() != next.Nodes.size())
			return false;
		std::unordered_map<int, const DesignNode*> currentById;
		currentById.reserve(current.Nodes.size());
		for (const auto& node : current.Nodes)
			currentById.emplace(node.Id, &node);
		for (const auto& node : next.Nodes)
		{
			const auto found = currentById.find(node.Id);
			if (found == currentById.end()
				|| !SameNodeShapeForInPlaceReload(*found->second, node)) return false;
		}
		return true;
	}

	bool PropertyChanged(
		const DesignNode& current,
		const DesignNode& next,
		const char* key)
	{
		return current.Props[key] != next.Props[key];
	}

	std::vector<std::wstring> CopyStyleClasses(Control& target)
	{
		const auto classes = target.GetStyleClasses();
		return { classes.begin(), classes.end() };
	}

	void SetStyleClasses(
		Control& target,
		const std::vector<std::wstring>& classes)
	{
		target.ClearStyleClasses();
		for (const auto& value : classes) (void)target.AddStyleClass(value);
	}

	struct InPlaceControlSnapshot
	{
		DesignerControl* Record = nullptr;
		Control* Target = nullptr;
		std::wstring Text;
		std::wstring StyleId;
		std::vector<std::wstring> StyleClasses;
		POINT Location{};
		SIZE Size{};
		bool Enable = true;
		bool Visible = true;
		D2D1_COLOR_F BackColor{};
		D2D1_COLOR_F ForeColor{};
		D2D1_COLOR_F BorderColor{};
		bool ShowValidationBorder = true;
		bool ShowValidationToolTip = true;
		float ValidationBorderThickness = 0.0f;
		float ValidationCornerRadius = 0.0f;
		float ValidationToolTipMaxWidth = 0.0f;
		std::wstring AccessibleDescription;
		Thickness Margin{};
		Thickness Padding{};
		uint8_t AnchorStyles = 0;
		HorizontalAlignment HAlign = HorizontalAlignment::Left;
		VerticalAlignment VAlign = VerticalAlignment::Top;
		Dock DockPosition = Dock::Left;
		int ZIndex = 0;
		int GridRow = 0;
		int GridColumn = 0;
		int GridRowSpan = 1;
		int GridColumnSpan = 1;
		ImageSizeMode SizeMode = ImageSizeMode::Zoom;
		std::map<std::wstring, DesignerStyleValue> MetadataProperties;
		std::map<std::wstring, BindingValue> LocalValues;

		static InPlaceControlSnapshot Capture(DesignerControl& record)
		{
			InPlaceControlSnapshot result;
			result.Record = &record;
			result.Target = record.ControlInstance;
			if (!result.Target) return result;
			auto& target = *result.Target;
			result.Text = target.Text;
			result.StyleId = target.GetStyleId();
			result.StyleClasses = CopyStyleClasses(target);
			result.Location = target.Location;
			result.Size = target.Size;
			result.Enable = target.Enable;
			result.Visible = target.Visible;
			result.BackColor = target.BackColor;
			result.ForeColor = target.ForeColor;
			result.BorderColor = target.BorderColor;
			result.ShowValidationBorder = target.ShowValidationBorder;
			result.ShowValidationToolTip = target.ShowValidationToolTip;
			result.ValidationBorderThickness = target.ValidationBorderThickness;
			result.ValidationCornerRadius = target.ValidationCornerRadius;
			result.ValidationToolTipMaxWidth = target.ValidationToolTipMaxWidth;
			result.AccessibleDescription = target.AccessibleDescription;
			result.Margin = target.Margin;
			result.Padding = target.Padding;
			result.AnchorStyles = target.AnchorStyles;
			result.HAlign = target.HAlign;
			result.VAlign = target.VAlign;
			result.DockPosition = target.DockPosition;
			result.ZIndex = target.ZIndex;
			result.GridRow = target.GridRow;
			result.GridColumn = target.GridColumn;
			result.GridRowSpan = target.GridRowSpan;
			result.GridColumnSpan = target.GridColumnSpan;
			result.SizeMode = target.SizeMode;
			result.MetadataProperties = record.MetadataProperties;
			for (const auto* metadata : BindingPropertyRegistry::GetProperties(target))
			{
				if (!metadata) continue;
				BindingValue value;
				if (target.TryGetPropertyValue(
					metadata->Name(), ControlPropertyValueSource::Local, value))
					result.LocalValues.emplace(metadata->Name(), std::move(value));
			}
			return result;
		}

		void Restore() const noexcept
		{
			if (!Target || !Record) return;
			auto& target = *Target;
			try
			{
				target.Text = Text;
				target.SetStyleId(StyleId);
				SetStyleClasses(target, StyleClasses);
				target.Location = Location;
				target.Size = Size;
				target.Enable = Enable;
				target.Visible = Visible;
				target.BackColor = BackColor;
				target.ForeColor = ForeColor;
				target.BorderColor = BorderColor;
				target.ShowValidationBorder = ShowValidationBorder;
				target.ShowValidationToolTip = ShowValidationToolTip;
				target.ValidationBorderThickness = ValidationBorderThickness;
				target.ValidationCornerRadius = ValidationCornerRadius;
				target.ValidationToolTipMaxWidth = ValidationToolTipMaxWidth;
				target.AccessibleDescription = AccessibleDescription;
				target.Margin = Margin;
				target.Padding = Padding;
				target.AnchorStyles = AnchorStyles;
				target.HAlign = HAlign;
				target.VAlign = VAlign;
				target.DockPosition = DockPosition;
				target.ZIndex = ZIndex;
				target.GridRow = GridRow;
				target.GridColumn = GridColumn;
				target.GridRowSpan = GridRowSpan;
				target.GridColumnSpan = GridColumnSpan;
				target.SizeMode = SizeMode;
				for (const auto* metadata : BindingPropertyRegistry::GetProperties(target))
				{
					if (metadata) (void)target.ClearPropertyValue(
						metadata->Name(), ControlPropertyValueSource::Local);
				}
				for (const auto& [name, value] : LocalValues)
					(void)target.TrySetPropertyValue(
						name, value, ControlPropertyValueSource::Local);
				Record->MetadataProperties = MetadataProperties;
			}
			catch (...)
			{
				// Runtime property restoration is best-effort inside a noexcept rollback.
			}
		}
	};

	bool IsRuntimeBound(Control& target, const wchar_t* property)
	{
		return property && target.DataBindings.Find(property) != nullptr;
	}

	bool ApplyMetadataPropertyChanges(
		const DesignValue& currentMetadata,
		const DesignValue& nextMetadata,
		DesignerControl& targetRecord,
		const DesignerControl& candidateRecord,
		std::wstring* outError)
	{
		auto* target = targetRecord.ControlInstance;
		auto* candidate = candidateRecord.ControlInstance;
		if (!target || !candidate)
		{
			SetError(outError, L"增量重载遇到无效控件实例。");
			return false;
		}
		std::map<std::wstring, bool> changedNames;
		for (const auto& [key, value] : currentMetadata.ObjectItems())
		{
			const auto found = nextMetadata.ObjectItems().find(key);
			if (found == nextMetadata.ObjectItems().end()
				|| found->second != value)
				changedNames[FromUtf8(key)] = found != nextMetadata.ObjectItems().end();
		}
		for (const auto& [key, value] : nextMetadata.ObjectItems())
		{
			const auto found = currentMetadata.ObjectItems().find(key);
			if (found == currentMetadata.ObjectItems().end()
				|| found->second != value) changedNames[FromUtf8(key)] = true;
		}
		std::vector<std::wstring> ordered;
		ordered.reserve(changedNames.size());
		for (const auto& [name, present] : changedNames)
		{
			(void)present;
			ordered.push_back(name);
		}
		std::stable_sort(ordered.begin(), ordered.end(), [candidate](
			const std::wstring& left, const std::wstring& right)
		{
			const auto* leftMetadata = candidate->FindPropertyMetadata(left);
			const auto* rightMetadata = candidate->FindPropertyMetadata(right);
			if (leftMetadata && rightMetadata)
			{
				const auto& leftDesign = leftMetadata->Design();
				const auto& rightDesign = rightMetadata->Design();
				if (leftDesign.CategoryOrder != rightDesign.CategoryOrder)
					return leftDesign.CategoryOrder < rightDesign.CategoryOrder;
				if (leftDesign.Order != rightDesign.Order)
					return leftDesign.Order < rightDesign.Order;
			}
			else if (leftMetadata != rightMetadata)
				return leftMetadata != nullptr;
			return _wcsicmp(left.c_str(), right.c_str()) < 0;
		});

		for (const auto& name : ordered)
		{
			const auto* metadata = candidate->FindPropertyMetadata(name);
			const auto canonical = metadata ? metadata->Name() : name;
			if (IsRuntimeBound(*target, canonical.c_str()))
			{
				SetError(outError, L"控件 “" + targetRecord.Name + L"” 的属性 “"
					+ canonical + L"” 正在绑定，当前不能原位覆盖其持久化值。");
				return false;
			}
			if (!changedNames[name])
			{
				BindingValue candidateValue;
				if (candidate->TryGetPropertyValue(
					canonical, ControlPropertyValueSource::Local, candidateValue))
				{
					if (!target->TrySetPropertyValue(
						canonical, candidateValue, ControlPropertyValueSource::Local))
					{
						SetError(outError, L"控件 “" + targetRecord.Name
							+ L"” 无法恢复属性 “" + canonical + L"” 的兼容值。");
						return false;
					}
				}
				else if (target->HasPropertyValue(
					canonical, ControlPropertyValueSource::Local)
					&& !target->ClearPropertyValue(
						canonical, ControlPropertyValueSource::Local))
				{
					SetError(outError, L"控件 “" + targetRecord.Name
						+ L"” 无法清除属性 “" + canonical + L"” 的本地值。");
					return false;
				}
				continue;
			}
			BindingValue value;
			if (!candidate->TryGetPropertyValue(
				canonical, ControlPropertyValueSource::Local, value)
				|| !target->TrySetPropertyValue(
					canonical, value, ControlPropertyValueSource::Local))
			{
				SetError(outError, L"控件 “" + targetRecord.Name
					+ L"” 无法原位应用属性 “" + canonical + L"”。");
				return false;
			}
		}
		targetRecord.MetadataProperties = candidateRecord.MetadataProperties;
		return true;
	}

	bool ApplyControlPropertyChanges(
		const DesignNode& currentNode,
		const DesignNode& nextNode,
		DesignerControl& targetRecord,
		const DesignerControl& candidateRecord,
		std::wstring* outError)
	{
		auto* target = targetRecord.ControlInstance;
		auto* candidate = candidateRecord.ControlInstance;
		if (!target || !candidate)
		{
			SetError(outError, L"增量重载遇到无效控件实例。");
			return false;
		}
		const std::pair<const char*, const wchar_t*> bindableKeys[]{
			{ "text", L"Text" }, { "enable", L"Enable" },
			{ "visible", L"Visible" }, { "backColor", L"BackColor" },
			{ "foreColor", L"ForeColor" }, { "borderColor", L"BorderColor" },
			{ "bolderColor", L"BorderColor" },
			{ "showValidationBorder", L"ShowValidationBorder" },
			{ "showValidationToolTip", L"ShowValidationToolTip" },
			{ "validationBorderThickness", L"ValidationBorderThickness" },
			{ "validationCornerRadius", L"ValidationCornerRadius" },
			{ "validationToolTipMaxWidth", L"ValidationToolTipMaxWidth" },
			{ "accessibleDescription", L"AccessibleDescription" },
			{ "margin", L"Margin" }, { "padding", L"Padding" },
			{ "anchor", L"AnchorStyles" }, { "hAlign", L"HAlign" },
			{ "vAlign", L"VAlign" }, { "dock", L"DockPosition" },
			{ "zIndex", L"ZIndex" }, { "gridRow", L"GridRow" },
			{ "gridColumn", L"GridColumn" }, { "gridRowSpan", L"GridRowSpan" },
			{ "gridColumnSpan", L"GridColumnSpan" }, { "sizeMode", L"SizeMode" }
		};
		for (const auto& [key, property] : bindableKeys)
		{
			if (PropertyChanged(currentNode, nextNode, key)
				&& IsRuntimeBound(*target, property))
			{
				SetError(outError, L"控件 “" + targetRecord.Name + L"” 的属性 “"
					+ property + L"” 正在绑定，当前不能原位覆盖其持久化值。");
				return false;
			}
		}

		if (PropertyChanged(currentNode, nextNode, "text")) target->Text = candidate->Text;
		if (PropertyChanged(currentNode, nextNode, "styleId"))
			target->SetStyleId(candidate->GetStyleId());
		if (PropertyChanged(currentNode, nextNode, "styleClasses"))
			SetStyleClasses(*target, CopyStyleClasses(*candidate));
		if (PropertyChanged(currentNode, nextNode, "location")) target->Location = candidate->Location;
		if (PropertyChanged(currentNode, nextNode, "size")) target->Size = candidate->Size;
		if (PropertyChanged(currentNode, nextNode, "enable")) target->Enable = candidate->Enable;
		if (PropertyChanged(currentNode, nextNode, "visible")) target->Visible = candidate->Visible;
		if (PropertyChanged(currentNode, nextNode, "backColor")) target->BackColor = candidate->BackColor;
		if (PropertyChanged(currentNode, nextNode, "foreColor")) target->ForeColor = candidate->ForeColor;
		if (PropertyChanged(currentNode, nextNode, "borderColor")
			|| PropertyChanged(currentNode, nextNode, "bolderColor"))
			target->BorderColor = candidate->BorderColor;
		if (PropertyChanged(currentNode, nextNode, "showValidationBorder"))
			target->ShowValidationBorder = candidate->ShowValidationBorder;
		if (PropertyChanged(currentNode, nextNode, "showValidationToolTip"))
			target->ShowValidationToolTip = candidate->ShowValidationToolTip;
		if (PropertyChanged(currentNode, nextNode, "validationBorderThickness"))
			target->ValidationBorderThickness = candidate->ValidationBorderThickness;
		if (PropertyChanged(currentNode, nextNode, "validationCornerRadius"))
			target->ValidationCornerRadius = candidate->ValidationCornerRadius;
		if (PropertyChanged(currentNode, nextNode, "validationToolTipMaxWidth"))
			target->ValidationToolTipMaxWidth = candidate->ValidationToolTipMaxWidth;
		if (PropertyChanged(currentNode, nextNode, "accessibleDescription"))
			target->AccessibleDescription = candidate->AccessibleDescription;
		if (PropertyChanged(currentNode, nextNode, "margin")) target->Margin = candidate->Margin;
		if (PropertyChanged(currentNode, nextNode, "padding")) target->Padding = candidate->Padding;
		if (PropertyChanged(currentNode, nextNode, "anchor")) target->AnchorStyles = candidate->AnchorStyles;
		if (PropertyChanged(currentNode, nextNode, "hAlign")) target->HAlign = candidate->HAlign;
		if (PropertyChanged(currentNode, nextNode, "vAlign")) target->VAlign = candidate->VAlign;
		if (PropertyChanged(currentNode, nextNode, "dock")) target->DockPosition = candidate->DockPosition;
		if (PropertyChanged(currentNode, nextNode, "zIndex")) target->ZIndex = candidate->ZIndex;
		if (PropertyChanged(currentNode, nextNode, "gridRow")) target->GridRow = candidate->GridRow;
		if (PropertyChanged(currentNode, nextNode, "gridColumn")) target->GridColumn = candidate->GridColumn;
		if (PropertyChanged(currentNode, nextNode, "gridRowSpan")) target->GridRowSpan = candidate->GridRowSpan;
		if (PropertyChanged(currentNode, nextNode, "gridColumnSpan")) target->GridColumnSpan = candidate->GridColumnSpan;
		if (PropertyChanged(currentNode, nextNode, "sizeMode")) target->SizeMode = candidate->SizeMode;
		if (PropertyChanged(currentNode, nextNode, "metadata")
			&& !ApplyMetadataPropertyChanges(
				currentNode.Props["metadata"],
				nextNode.Props["metadata"],
				targetRecord, candidateRecord, outError)) return false;
		return true;
	}

	bool ReadControlEventHandlers(
		const DesignNode& node,
		std::map<std::wstring, std::wstring>& handlers,
		std::wstring* outError)
	{
		handlers.clear();
		if (!node.Events.is_object())
		{
			SetError(outError, L"控件 “" + node.Name + L"” 的事件集合无效。");
			return false;
		}
		for (const auto& [eventName, value] : node.Events.ObjectItems())
		{
			if (value.is_boolean())
			{
				if (value.get<bool>()) handlers.emplace(FromUtf8(eventName), L"1");
				continue;
			}
			if (!value.is_string())
			{
				SetError(outError, L"控件 “" + node.Name + L"” 的事件值无效："
					+ FromUtf8(eventName));
				return false;
			}
			auto handler = FromUtf8(value.get<std::string>());
			if (!handler.empty()) handlers.emplace(FromUtf8(eventName), std::move(handler));
		}
		return true;
	}

}

RuntimeDocument::RuntimeDocument()
	: _referenceState(
		std::make_shared<Detail::RuntimeDocumentReferenceState>())
{
	_referenceState->Document = this;
}

RuntimeDocument::RuntimeDocument(RuntimeDocument&& other) noexcept
{
	*this = std::move(other);
}

RuntimeDocument::~RuntimeDocument()
{
	if (_referenceState) _referenceState->Document = nullptr;
	ClearFormEvents();
	ClearControlEvents();
	ClearDataBindings();
}

RuntimeDocument& RuntimeDocument::operator=(RuntimeDocument&& other) noexcept
{
	if (this == &other) return *this;
	auto referenceState = _referenceState;
	if (!referenceState)
		referenceState = std::move(other._referenceState);
	else if (other._referenceState)
		other._referenceState->Document = nullptr;

	ClearFormEvents();
	ClearControlEvents();
	ClearDataBindings();

	_form = std::move(other._form);
	_dataContextSchema = std::move(other._dataContextSchema);
	_styleSheet = std::move(other._styleSheet);
	_dataContext = std::move(other._dataContext);
	_ownedRoots = std::move(other._ownedRoots);
	_rootControls = std::move(other._rootControls);
	_controls = std::move(other._controls);
	_controlsByDesignId = std::move(other._controlsByDesignId);
	_controlsByName = std::move(other._controlsByName);
	_installedBindings = std::move(other._installedBindings);
	_eventConnections = std::move(other._eventConnections);
	_formEventConnections = std::move(other._formEventConnections);
	_controlEventResolver = std::move(other._controlEventResolver);
	_formEventResolver = std::move(other._formEventResolver);
	_formEventTarget = other._formEventTarget;
	_appliedForm = other._appliedForm;
	_rootHost = std::move(other._rootHost);
	_customControls = std::move(other._customControls);
	_allowCustomControlProxy = other._allowCustomControlProxy;
	_sourceDocument = std::move(other._sourceDocument);
	_rootsReleased = other._rootsReleased;
	_referenceState = std::move(referenceState);
	if (_referenceState) _referenceState->Document = this;
	return *this;
}

Control* RuntimeDocument::FindControlByDesignId(int stableId) noexcept
{
	if (stableId <= 0) return nullptr;
	const auto found = _controlsByDesignId.find(stableId);
	return found == _controlsByDesignId.end() ? nullptr : found->second;
}

const Control* RuntimeDocument::FindControlByDesignId(int stableId) const noexcept
{
	return const_cast<RuntimeDocument*>(this)->FindControlByDesignId(stableId);
}

Control* RuntimeDocument::FindControlByName(const std::wstring& name) noexcept
{
	const auto found = _controlsByName.find(name);
	return found == _controlsByName.end() ? nullptr : found->second;
}

const Control* RuntimeDocument::FindControlByName(
	const std::wstring& name) const noexcept
{
	return const_cast<RuntimeDocument*>(this)->FindControlByName(name);
}

void RuntimeDocument::RebuildControlIndex()
{
	std::unordered_map<int, Control*> byDesignId;
	std::unordered_map<std::wstring, Control*> byName;
	byDesignId.reserve(_controls.size());
	byName.reserve(_controls.size());
	for (const auto& control : _controls)
	{
		if (!control || !control->ControlInstance) continue;
		byDesignId.emplace(control->StableId, control->ControlInstance);
		byName.emplace(control->Name, control->ControlInstance);
	}
	_controlsByDesignId = std::move(byDesignId);
	_controlsByName = std::move(byName);
}

void RuntimeDocument::RemoveDataBindings(
	std::vector<InstalledBinding>& installed) noexcept
{
	for (auto it = installed.rbegin(); it != installed.rend(); ++it)
	{
		if (!it->Target) continue;
		(void)it->Target->DataBindings.Remove(it->Property);
		if (!it->LocalValueWasSuspended) continue;
		if (it->PreviousLocalValue)
		{
			(void)it->Target->TrySetPropertyValue(
				it->Property,
				*it->PreviousLocalValue,
				ControlPropertyValueSource::Local);
		}
		else
		{
			(void)it->Target->ClearPropertyValue(
				it->Property,
				ControlPropertyValueSource::Local);
		}
	}
	installed.clear();
}

bool RuntimeDocument::InstallDataBindings(
	const std::shared_ptr<IBindingSource>& source,
	std::vector<InstalledBinding>& installed,
	std::wstring* outError)
{
	if (!source)
	{
		SetError(outError, L"未提供运行时 DataContext。");
		return false;
	}

	for (const auto& control : _controls)
	{
		if (!control || !control->ControlInstance) continue;
		auto& target = *control->ControlInstance;
		for (const auto& [targetProperty, configuration] : control->DataBindings)
		{
			std::wstring validationError;
			if (!DesignerBindingUtils::Validate(
				target,
				targetProperty,
				configuration,
				nullptr,
				&validationError,
				_dataContextSchema.empty() ? nullptr : &_dataContextSchema))
			{
				SetError(outError, L"控件 " + control->Name + L"：" + validationError);
				RemoveDataBindings(installed);
				return false;
			}

			std::shared_ptr<const IBindingValueConverter> converter;
			const auto converterName = DesignerBindingUtils::Trim(
				configuration.Converter);
			if (!converterName.empty())
			{
				converter = BindingValueConverterRegistry::Create(converterName);
				if (!converter)
				{
					SetError(outError, L"控件 " + control->Name
						+ L"：无法创建 Converter：" + converterName);
					RemoveDataBindings(installed);
					return false;
				}
			}

			InstalledBinding state;
			state.Target = &target;
			state.Property = targetProperty;
			const bool writesTarget =
				configuration.Mode != BindingMode::OneWayToSource;
			if (writesTarget)
			{
				state.LocalValueWasSuspended = true;
				BindingValue localValue;
				const bool hadLocal = target.TryGetPropertyValue(
					targetProperty,
					ControlPropertyValueSource::Local,
					localValue);
				if (hadLocal)
					state.PreviousLocalValue = std::move(localValue);
				if (hadLocal && !target.ClearPropertyValue(
					targetProperty,
					ControlPropertyValueSource::Local))
				{
					SetError(outError, L"控件 " + control->Name
						+ L"：无法暂存目标属性 " + targetProperty + L" 的 Local 值。");
					RemoveDataBindings(installed);
					return false;
				}
			}

			auto* binding = target.DataBindings.Add(
				targetProperty,
				*source,
				configuration.SourceProperty,
				configuration.Mode,
				configuration.UpdateMode,
				std::move(converter));
			if (!binding)
			{
				if (state.PreviousLocalValue)
				{
					(void)target.TrySetPropertyValue(
						targetProperty,
						*state.PreviousLocalValue,
						ControlPropertyValueSource::Local);
				}
				SetError(outError, L"控件 " + control->Name + L"：绑定 "
					+ targetProperty + L" 失败："
					+ target.DataBindings.LastErrorMessage());
				RemoveDataBindings(installed);
				return false;
			}
			installed.push_back(std::move(state));
		}
	}
	if (outError) outError->clear();
	return true;
}

bool RuntimeDocument::BindDataContext(
	std::shared_ptr<IBindingSource> source,
	std::wstring* outError)
{
	if (!source)
	{
		SetError(outError, L"未提供运行时 DataContext。");
		return false;
	}

	auto previousSource = _dataContext;
	RemoveDataBindings(_installedBindings);
	std::vector<InstalledBinding> next;
	if (!InstallDataBindings(source, next, outError))
	{
		if (previousSource)
		{
			std::vector<InstalledBinding> restored;
			(void)InstallDataBindings(previousSource, restored, nullptr);
			_installedBindings = std::move(restored);
		}
		return false;
	}
	SetStyleDataContext(source.get());
	_dataContext = std::move(source);
	_installedBindings = std::move(next);
	if (outError) outError->clear();
	return true;
}

void RuntimeDocument::ClearDataBindings()
{
	RemoveDataBindings(_installedBindings);
	SetStyleDataContext(nullptr);
	_dataContext.reset();
}

void RuntimeDocument::SetStyleDataContext(IBindingSource* source)
{
	std::vector<const ControlStyleSheet*> updated;
	for (auto* root : _rootControls)
	{
		if (!root) continue;
		const auto& sheet = root->GetStyleSheet();
		if (!sheet || std::find(updated.begin(), updated.end(), sheet.get())
			!= updated.end()) continue;
		sheet->SetDataContext(source);
		updated.push_back(sheet.get());
	}
}

bool RuntimeDocument::BindControlEvents(
	const RuntimeControlEventResolver& resolver,
	std::wstring* outError)
{
	if (!resolver)
	{
		SetError(outError, L"未提供控件事件名称解析器。");
		return false;
	}

	std::vector<EventConnection> next;
	for (const auto& control : _controls)
	{
		if (!control || !control->ControlInstance) continue;
		for (const auto& [eventName, storedHandler] : control->EventHandlers)
		{
			const auto descriptor = DesignerEventCatalog::FindControlEvent(
				control->Type, eventName, control->CustomEvents);
			if (!descriptor)
			{
				SetError(outError, L"控件 " + control->Name
					+ L" 包含未知事件：" + eventName);
				return false;
			}
			const auto handlerName = DesignerEventCatalog::ResolveHandlerName(
				storedHandler, control->Name, eventName);
			std::wstring validationError;
			if (handlerName.empty()
				|| !DesignerEventCatalog::ValidateHandlerName(
					handlerName, &validationError))
			{
				SetError(outError, L"控件 " + control->Name + L" 的事件 "
					+ eventName + L"：" + (validationError.empty()
						? std::wstring(L"处理函数名为空。") : validationError));
				return false;
			}

			RuntimeControlEventRequest request{
				*control->ControlInstance,
				control->StableId,
				control->Name,
				control->Type,
				control->CustomType,
				*descriptor,
				handlerName };
			EventConnection connection;
			std::wstring resolverError;
			if (!resolver(request, connection, resolverError)
				|| !connection.Connected())
			{
				SetError(outError, L"控件 " + control->Name + L" 的事件 "
					+ eventName + L" 无法绑定到 " + handlerName
					+ (resolverError.empty() ? std::wstring{} : L"：" + resolverError));
				return false;
			}
			next.push_back(std::move(connection));
		}
	}

	_eventConnections = std::move(next);
	_controlEventResolver = resolver;
	if (outError) outError->clear();
	return true;
}

void RuntimeDocument::ClearControlEvents() noexcept
{
	_eventConnections.clear();
	_controlEventResolver = {};
}

bool RuntimeDocument::ApplyFormProperties(
	::Form& form,
	std::wstring* outError) const
{
	if (!ApplyFormModel(_form, form, outError)) return false;
	_appliedForm = &form;
	return true;
}

bool RuntimeDocument::BindFormEvents(
	::Form& form,
	const RuntimeFormEventResolver& resolver,
	std::wstring* outError)
{
	if (!resolver)
	{
		SetError(outError, L"未提供窗体事件名称解析器。");
		return false;
	}

	std::vector<EventConnection> next;
	for (const auto& [eventName, storedHandler] : _form.EventHandlers)
	{
		const auto descriptor = DesignerEventCatalog::FindFormEvent(eventName);
		if (!descriptor)
		{
			SetError(outError, L"窗体包含未知事件：" + eventName);
			return false;
		}
		const auto handlerName = DesignerEventCatalog::ResolveHandlerName(
			storedHandler, _form.Name, eventName);
		std::wstring validationError;
		if (handlerName.empty()
			|| !DesignerEventCatalog::ValidateHandlerName(
				handlerName, &validationError))
		{
			SetError(outError, L"窗体事件 " + eventName + L"："
				+ (validationError.empty()
					? std::wstring(L"处理函数名为空。") : validationError));
			return false;
		}

		RuntimeFormEventRequest request{
			form, _form.Name, *descriptor, handlerName };
		EventConnection connection;
		std::wstring resolverError;
		if (!resolver(request, connection, resolverError)
			|| !connection.Connected())
		{
			SetError(outError, L"窗体事件 " + eventName + L" 无法绑定到 "
				+ handlerName + (resolverError.empty()
					? std::wstring{} : L"：" + resolverError));
			return false;
		}
		next.push_back(std::move(connection));
	}

	_formEventConnections = std::move(next);
	_formEventTarget = &form;
	_formEventResolver = resolver;
	if (outError) outError->clear();
	return true;
}

void RuntimeDocument::ClearFormEvents() noexcept
{
	_formEventConnections.clear();
	_formEventResolver = {};
	_formEventTarget = nullptr;
}

bool RuntimeDocument::AttachToForm(
	::Form& form,
	const RuntimeFormEventResolver& resolver,
	std::wstring* outError)
{
	try
	{
		return AttachToForm(
			form,
			std::make_shared<FormRuntimeDocumentRootHost>(form),
			resolver,
			outError);
	}
	catch (...)
	{
		SetError(outError, L"无法创建 Form 原子挂载适配器。");
		return false;
	}
}

bool RuntimeDocument::AttachToForm(
	::Form& form,
	std::wstring* outError)
{
	return AttachToForm(form, RuntimeFormEventResolver{}, outError);
}

bool RuntimeDocument::AttachToForm(
	::Form& form,
	std::shared_ptr<RuntimeDocumentRootHost> rootHost,
	const RuntimeFormEventResolver& resolver,
	std::wstring* outError)
{
	if (!rootHost)
	{
		SetError(outError, L"未提供 Form 原子挂载的根宿主适配器。");
		return false;
	}
	if (_rootsReleased || _rootHost)
	{
		SetError(outError, L"运行时文档根已经挂载或转移，不能重复挂载到 Form。");
		return false;
	}
	if (_appliedForm || _formEventTarget || _formEventResolver
		|| !_formEventConnections.empty())
	{
		SetError(outError,
			L"运行时文档已经存在独立 Form 附件；原子挂载只接受未挂载文档。");
		return false;
	}
	if (!resolver && !_form.EventHandlers.empty())
	{
		SetError(outError, L"动态文档包含 Form 事件，但原子挂载未提供名称解析器。");
		return false;
	}

	std::optional<FormPresentationSnapshot> presentation;
	try
	{
		presentation = FormPresentationSnapshot::Capture(form);
	}
	catch (...)
	{
		SetError(outError, L"无法保存 Form 原子挂载前的显示状态。");
		return false;
	}

	if (!ApplyFormModel(_form, form, outError))
	{
		presentation->Restore();
		return false;
	}
	if (resolver && !BindFormEvents(form, resolver, outError))
	{
		presentation->Restore();
		return false;
	}

	if (!TransferRootControlsTo(std::move(rootHost), outError))
	{
		const auto failure = outError ? *outError : std::wstring{};
		ClearFormEvents();
		presentation->Restore();
		_appliedForm = nullptr;
		SetError(outError, failure.empty()
			? std::wstring(L"Form 根宿主拒绝原子挂载。") : failure);
		return false;
	}

	_appliedForm = &form;
	if (outError) outError->clear();
	return true;
}

bool RuntimeDocument::CommitInheritedFormAttachments(
	RuntimeDocument& previous,
	const std::function<bool(std::wstring*)>& finalCommit,
	std::wstring* outError)
{
	std::optional<FormPresentationSnapshot> presentation;
	if (previous._appliedForm)
	{
		try
		{
			presentation =
				FormPresentationSnapshot::Capture(*previous._appliedForm);
		}
		catch (...)
		{
			SetError(outError, L"无法保存热重载前的 Form 显示状态。");
			return false;
		}
		if (!HasSameFormPresentation(previous._form, _form)
			&& !ApplyFormModel(_form, *previous._appliedForm, outError))
		{
			presentation->Restore();
			return false;
		}
		_appliedForm = previous._appliedForm;
	}

	if (previous._formEventTarget && previous._formEventResolver)
	{
		if (!BindFormEvents(
			*previous._formEventTarget,
			previous._formEventResolver,
			outError))
		{
			if (presentation) presentation->Restore();
			return false;
		}
	}
	else if (previous._appliedForm && !_form.EventHandlers.empty())
	{
		if (presentation) presentation->Restore();
		SetError(outError,
			L"热重载后的动态文档包含 Form 事件，但宿主没有保留名称解析器。");
		return false;
	}

	bool committed = true;
	try { if (finalCommit) committed = finalCommit(outError); }
	catch (...)
	{
		committed = false;
		SetError(outError, L"提交 Form 运行时附件时抛出异常。");
	}
	if (committed) return true;

	ClearFormEvents();
	if (presentation) presentation->Restore();
	return false;
}

std::vector<std::unique_ptr<Control>> RuntimeDocument::ReleaseRootControls()
{
	if (_rootsReleased) return {};
	_rootsReleased = true;
	_rootHost.reset();
	return std::move(_ownedRoots);
}

bool RuntimeDocument::TransferRootControlsTo(
	std::shared_ptr<RuntimeDocumentRootHost> host,
	std::wstring* outError)
{
	if (!host)
	{
		SetError(outError, L"未提供运行时根宿主适配器。");
		return false;
	}
	if (_rootsReleased)
	{
		SetError(outError, L"运行时文档根所有权已经转移。");
		return false;
	}
	try
	{
		if (!host->AttachRoots(
			_ownedRoots, RuntimeRootHostAttachMode::Initial, outError)) return false;
	}
	catch (...)
	{
		SetError(outError, L"根宿主初始挂载抛出异常。");
		return false;
	}
	_rootsReleased = true;
	_rootHost = std::move(host);
	if (outError) outError->clear();
	return true;
}

bool RuntimeDocument::TransferRootControlsTo(
	::Form& form,
	std::wstring* outError)
{
	try
	{
		return TransferRootControlsTo(
			std::make_shared<FormRuntimeDocumentRootHost>(form), outError);
	}
	catch (...)
	{
		SetError(outError, L"无法创建 Form 根宿主适配器。");
		return false;
	}
}

CodeGenInput RuntimeDocument::BuildCodeGenInput() const
{
	CodeGenInput input;
	input.Controls = _controls;
	input.FormText = _form.Text;
	input.FormName = _form.Name;
	input.FormSize = _form.Size;
	input.FormLocation = _form.Location;
	input.FormBackColor = _form.BackColor;
	input.FormForeColor = _form.ForeColor;
	input.FormShowInTaskBar = _form.ShowInTaskBar;
	input.FormTopMost = _form.TopMost;
	input.FormEnable = _form.Enable;
	input.FormVisible = _form.Visible;
	input.FormEventHandlers = _form.EventHandlers;
	input.FormVisibleHead = _form.VisibleHead;
	input.FormHeadHeight = _form.HeadHeight;
	input.FormMinBox = _form.MinBox;
	input.FormMaxBox = _form.MaxBox;
	input.FormCloseBox = _form.CloseBox;
	input.FormCenterTitle = _form.CenterTitle;
	input.FormAllowResize = _form.AllowResize;
	input.FormFontName = _form.FontName;
	input.FormFontSize = _form.FontSize;
	input.ResourceBasePath = _sourceDocument
		? _sourceDocument->ResourceBasePath : std::wstring{};
	input.StyleSheet = _styleSheet;
	return input;
}

bool RuntimeDocumentLoader::Load(
	const DesignDocument& document,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	std::wstring* outError)
{
	try
	{
		if (output._rootsReleased
			|| output._appliedForm
			|| output._formEventTarget)
		{
			SetError(outError,
				L"运行时文档已经附加到外部 Form 或根宿主；请使用 Reload 保持宿主事务。");
			return false;
		}
		DesignDocumentEventIndex eventIndex;
		if (!DesignDocumentEventIndex::Build(
			document, eventIndex, outError)) return false;
		MaterializedControlTree materialized;
		if (!DesignDocumentMaterializer::Materialize(
			document, materialized,
			MaterializationOptionsFor(options), outError)) return false;

		RuntimeDocument candidate;
		candidate._sourceDocument = document;
		candidate._form = document.Form;
		candidate._dataContextSchema = document.DataContextSchema;
		DesignerDataContextSchemaUtils::Canonicalize(
			candidate._dataContextSchema);
		candidate._styleSheet = document.StyleSheet;
		candidate._customControls = options.CustomControls;
		candidate._allowCustomControlProxy = options.AllowCustomControlProxy;
		candidate._controls = std::move(materialized.Controls);
		candidate._ownedRoots = std::move(materialized.Roots);
		candidate.RebuildControlIndex();
		candidate._rootControls.reserve(candidate._ownedRoots.size());
		for (const auto& root : candidate._ownedRoots)
			if (root) candidate._rootControls.push_back(root.get());

		if (options.DataContext
			&& !candidate.BindDataContext(options.DataContext, outError))
			return false;
		if (options.ControlEventResolver)
		{
			if (!candidate.BindControlEvents(
				options.ControlEventResolver, outError)) return false;
		}
		else if (options.RequireControlEventResolver
			&& HasConfiguredControlEvents(candidate))
		{
			SetError(outError, L"文档包含控件事件，但加载选项未提供事件名称解析器。");
			return false;
		}

		output = std::move(candidate);
		if (outError) outError->clear();
		return true;
	}
	catch (const std::exception&)
	{
		SetError(outError, L"动态文档加载失败：控件构建或资源初始化抛出异常。");
		return false;
	}
	catch (...)
	{
		SetError(outError, L"动态文档加载失败：发生未知异常。");
		return false;
	}
}

bool RuntimeDocumentLoader::LoadXml(
	const std::string& xml,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	std::wstring* outError)
{
	try
	{
		DesignDocument document;
		if (!DesignDocumentSerializer::FromXml(
			xml, document, outError)) return false;
		return Load(document, output, options, outError);
	}
	catch (const std::exception&)
	{
		SetError(outError, L"动态 XML 加载失败：文档格式无效。");
		return false;
	}
	catch (...)
	{
		SetError(outError, L"动态 XML 加载失败：发生未知解析异常。");
		return false;
	}
}

bool RuntimeDocumentLoader::LoadFile(
	const std::wstring& filePath,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	std::wstring* outError)
{
	try
	{
		DesignDocument document;
		if (!DesignDocumentSerializer::LoadFromFile(
			filePath, document, outError)) return false;
		return Load(document, output, options, outError);
	}
	catch (const std::exception&)
	{
		SetError(outError, L"动态文档文件加载失败：文件内容无效。");
		return false;
	}
	catch (...)
	{
		SetError(outError, L"动态文档文件加载失败：发生未知读取异常。");
		return false;
	}
}

bool RuntimeDocumentLoader::LoadXaml(
	const std::string& xaml,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	std::wstring* outError)
{
	try
	{
		DesignDocument document;
		if (!XamlDocumentParser::FromXaml(
			xaml, document, XamlOptionsFor(options.CustomControls), outError)) return false;
		return Load(document, output, options, outError);
	}
	catch (const std::exception&)
	{
		SetError(outError, L"动态 XAML 加载失败：文档格式无效。");
		return false;
	}
	catch (...)
	{
		SetError(outError, L"动态 XAML 加载失败：发生未知解析异常。");
		return false;
	}
}

bool RuntimeDocumentLoader::LoadXamlFile(
	const std::wstring& filePath,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	std::wstring* outError)
{
	try
	{
		DesignDocument document;
		if (!XamlDocumentParser::LoadFromFile(
			filePath, document, XamlOptionsFor(options.CustomControls), outError)) return false;
		return Load(document, output, options, outError);
	}
	catch (const std::exception&)
	{
		SetError(outError, L"动态 XAML 文件加载失败：文件内容无效。");
		return false;
	}
	catch (...)
	{
		SetError(outError, L"动态 XAML 文件加载失败：发生未知读取异常。");
		return false;
	}
}

bool RuntimeDocumentLoader::LoadIntoForm(
	const DesignDocument& document,
	::Form& form,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	const RuntimeFormEventResolver& formResolver,
	std::wstring* outError)
{
	if (output._rootsReleased
		|| output._appliedForm
		|| output._formEventTarget)
	{
		SetError(outError,
			L"输出运行时文档已经附加到外部 Form 或根宿主；请使用 Reload。");
		return false;
	}

	RuntimeDocument candidate;
	if (!Load(document, candidate, options, outError)) return false;
	if (!candidate.AttachToForm(form, formResolver, outError)) return false;
	output = std::move(candidate);
	if (outError) outError->clear();
	return true;
}

bool RuntimeDocumentLoader::LoadXmlIntoForm(
	const std::string& xml,
	::Form& form,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	const RuntimeFormEventResolver& formResolver,
	std::wstring* outError)
{
	try
	{
		DesignDocument document;
		if (!DesignDocumentSerializer::FromXml(
			xml, document, outError)) return false;
		return LoadIntoForm(
			document, form, output, options, formResolver, outError);
	}
	catch (...)
	{
		SetError(outError, L"动态 XML 原子挂载失败：文档格式无效。");
		return false;
	}
}

bool RuntimeDocumentLoader::LoadFileIntoForm(
	const std::wstring& filePath,
	::Form& form,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	const RuntimeFormEventResolver& formResolver,
	std::wstring* outError)
{
	try
	{
		DesignDocument document;
		if (!DesignDocumentSerializer::LoadFromFile(
			filePath, document, outError)) return false;
		return LoadIntoForm(
			document, form, output, options, formResolver, outError);
	}
	catch (...)
	{
		SetError(outError, L"动态文档文件原子挂载失败：文件内容无效。");
		return false;
	}
}

bool RuntimeDocumentLoader::LoadXamlIntoForm(
	const std::string& xaml,
	::Form& form,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	const RuntimeFormEventResolver& formResolver,
	std::wstring* outError)
{
	try
	{
		DesignDocument document;
		if (!XamlDocumentParser::FromXaml(
			xaml, document, XamlOptionsFor(options.CustomControls), outError)) return false;
		return LoadIntoForm(
			document, form, output, options, formResolver, outError);
	}
	catch (...)
	{
		SetError(outError, L"动态 XAML 原子挂载失败：文档格式无效。");
		return false;
	}
}

bool RuntimeDocumentLoader::LoadXamlFileIntoForm(
	const std::wstring& filePath,
	::Form& form,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	const RuntimeFormEventResolver& formResolver,
	std::wstring* outError)
{
	try
	{
		DesignDocument document;
		if (!XamlDocumentParser::LoadFromFile(
			filePath, document, XamlOptionsFor(options.CustomControls), outError)) return false;
		return LoadIntoForm(
			document, form, output, options, formResolver, outError);
	}
	catch (...)
	{
		SetError(outError, L"动态 XAML 文件原子挂载失败：文件内容无效。");
		return false;
	}
}

bool RuntimeDocumentLoader::ReloadHosted(
	const DesignDocument& document,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& effectiveOptions,
	RuntimeDocumentReloadMode* outMode,
	std::wstring* outError)
{
	auto host = output._rootHost;
	if (!host || !output._rootsReleased)
	{
		SetError(outError, L"运行时文档没有处于外部宿主管理状态。");
		return false;
	}

	std::vector<std::unique_ptr<Control>> previousRoots;
	try
	{
		if (!host->DetachRoots(
			output._rootControls, previousRoots, outError)) return false;
	}
	catch (...)
	{
		if (!previousRoots.empty())
		{
			output._ownedRoots = std::move(previousRoots);
			output._rootsReleased = false;
			output._rootHost.reset();
		}
		SetError(outError, L"根宿主分离旧根时抛出异常。");
		return false;
	}
	output._ownedRoots = std::move(previousRoots);
	output._rootsReleased = false;

	auto restorePreviousRoots = [&](std::wstring failure) -> bool
	{
		auto roots = std::move(output._ownedRoots);
		std::wstring restoreError;
		bool restored = false;
		try
		{
			restored = host->AttachRoots(
				roots, RuntimeRootHostAttachMode::Rollback, &restoreError);
		}
		catch (...)
		{
			restoreError = L"根宿主回滚抛出异常。";
		}
		if (restored)
		{
			output._rootsReleased = true;
			output._ownedRoots.clear();
			SetError(outError, std::move(failure));
			return false;
		}

		output._ownedRoots = std::move(roots);
		output._rootsReleased = false;
		output._rootHost.reset();
		if (!restoreError.empty())
			failure += L"；旧根恢复也失败：" + restoreError;
		SetError(outError, std::move(failure));
		return false;
	};

	auto attachCandidateRoots = [host](
		RuntimeDocument& candidate,
		std::wstring* commitError) -> bool
	{
		auto roots = std::move(candidate._ownedRoots);
		bool committed = false;
		try
		{
			committed = host->AttachRoots(
				roots, RuntimeRootHostAttachMode::Replacement, commitError);
		}
		catch (...)
		{
			SetError(commitError, L"根宿主提交候选根时抛出异常。");
		}
		if (!committed)
		{
			candidate._ownedRoots = std::move(roots);
			return false;
		}
		candidate._rootsReleased = true;
		candidate._rootHost = host;
		candidate._ownedRoots.clear();
		return true;
	};
	auto commitCandidate = [&output, &attachCandidateRoots](
		RuntimeDocument& candidate,
		std::wstring* commitError) -> bool
	{
		return candidate.CommitInheritedFormAttachments(
			output,
			[&](std::wstring* finalError)
			{
				return attachCandidateRoots(candidate, finalError);
			},
			commitError);
	};

	bool recomposed = false;
	size_t reusedControlCount = 0;
	if (!RuntimeDocumentTopologyReloader::TryReload(
		document,
		output,
		effectiveOptions,
		recomposed,
		reusedControlCount,
		outError,
		commitCandidate))
	{
		const auto failure = outError ? *outError
			: std::wstring(L"宿主拓扑重组失败。");
		return restorePreviousRoots(failure);
	}
	if (recomposed)
	{
		(void)reusedControlCount;
		if (outMode) *outMode = RuntimeDocumentReloadMode::Recomposed;
		if (outError) outError->clear();
		return true;
	}

	RuntimeDocument candidate;
	if (!Load(document, candidate, effectiveOptions, outError))
	{
		const auto failure = outError ? *outError
			: std::wstring(L"宿主替换候选加载失败。");
		return restorePreviousRoots(failure);
	}
	bool candidateCommitted = false;
	try { candidateCommitted = commitCandidate(candidate, outError); }
	catch (...)
	{
		SetError(outError, L"宿主提交替换根时抛出异常。");
	}
	if (!candidateCommitted)
	{
		const auto failure = outError ? *outError
			: std::wstring(L"宿主拒绝提交替换根。");
		return restorePreviousRoots(failure);
	}

	output = std::move(candidate);
	if (outMode) *outMode = RuntimeDocumentReloadMode::Replaced;
	if (outError) outError->clear();
	return true;
}

bool RuntimeDocumentLoader::Reload(
	const DesignDocument& document,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	RuntimeDocumentReloadMode* outMode,
	std::wstring* outError)
{
	try
	{
		RuntimeDocumentLoadOptions inheritedOptions = options;
		if (!inheritedOptions.CustomControls)
			inheritedOptions.CustomControls = output._customControls;
		if (!options.CustomControls && output._allowCustomControlProxy)
			inheritedOptions.AllowCustomControlProxy = true;
		DesignDocumentEventIndex eventIndex;
		if (!DesignDocumentEventIndex::Build(
			document, eventIndex, outError)) return false;

		const bool sameSourceDocument = output._sourceDocument
			&& *output._sourceDocument == document;
		const bool hasExplicitRuntimeChange = options.DataContext
			|| options.ControlEventResolver
			|| options.RequireControlEventResolver
			|| options.CustomControls
			|| options.AllowCustomControlProxy;
		if (sameSourceDocument
			&& !options.ForceResourceRefresh
			&& !hasExplicitRuntimeChange)
		{
			if (outMode) *outMode = RuntimeDocumentReloadMode::Unchanged;
			if (outError) outError->clear();
			return true;
		}

		if (output._sourceDocument
			&& !(sameSourceDocument && options.ForceResourceRefresh)
			&& CanReloadInPlace(*output._sourceDocument, document))
		{
			std::unordered_map<int, const DesignNode*> currentById;
			currentById.reserve(output._sourceDocument->Nodes.size());
			for (const auto& node : output._sourceDocument->Nodes)
				currentById.emplace(node.Id, &node);
			std::unordered_map<int, const DesignNode*> nextById;
			nextById.reserve(document.Nodes.size());
			for (const auto& node : document.Nodes)
				nextById.emplace(node.Id, &node);

			const bool hasPropertyChanges = std::any_of(
				document.Nodes.begin(), document.Nodes.end(),
				[&](const DesignNode& node)
				{
					const auto found = currentById.find(node.Id);
					return found == currentById.end()
						|| found->second->Props != node.Props;
				});
			const bool hasBindingChanges = std::any_of(
				document.Nodes.begin(), document.Nodes.end(),
				[&](const DesignNode& node)
				{
					const auto found = currentById.find(node.Id);
					return found == currentById.end()
						|| found->second->Bindings != node.Bindings;
				});
			const bool hasStyleChanges =
				!(output._sourceDocument->StyleSheet == document.StyleSheet);
			const bool hasSchemaChanges =
				output._sourceDocument->DataContextSchema != document.DataContextSchema;
			const bool needsCandidate = hasPropertyChanges
				|| hasBindingChanges || hasStyleChanges || hasSchemaChanges;
			MaterializedControlTree reloadCandidate;
			std::unordered_map<int, const DesignerControl*> candidateById;
			if (needsCandidate)
			{
				if (!DesignDocumentMaterializer::Materialize(
					document, reloadCandidate,
					MaterializationOptionsFor(inheritedOptions), outError)) return false;
				candidateById.reserve(reloadCandidate.Controls.size());
				for (const auto& control : reloadCandidate.Controls)
					if (control) candidateById.emplace(control->StableId, control.get());
			}

			std::vector<std::map<std::wstring, std::wstring>> nextHandlers;
			std::vector<std::map<std::wstring, std::wstring>> previousHandlers;
			std::vector<std::map<std::wstring, DesignerDataBinding>> nextBindings;
			std::vector<std::map<std::wstring, DesignerDataBinding>> previousBindings;
			std::vector<InPlaceControlSnapshot> propertySnapshots;
			nextHandlers.reserve(output._controls.size());
			previousHandlers.reserve(output._controls.size());
			nextBindings.reserve(output._controls.size());
			previousBindings.reserve(output._controls.size());
			propertySnapshots.reserve(output._controls.size());
			auto rollbackProperties = [&]() noexcept
			{
				for (auto it = propertySnapshots.rbegin();
					it != propertySnapshots.rend(); ++it) it->Restore();
			};
			bool hasNextEvents = false;
			for (const auto& control : output._controls)
			{
				if (!control)
				{
					SetError(outError, L"增量重载遇到无效控件记录。");
					rollbackProperties();
					return false;
				}
				const auto found = nextById.find(control->StableId);
				if (found == nextById.end())
				{
					SetError(outError, L"增量重载无法解析控件稳定 ID："
						+ std::to_wstring(control->StableId));
					rollbackProperties();
					return false;
				}
				const auto currentFound = currentById.find(control->StableId);
				if (currentFound == currentById.end())
				{
					SetError(outError, L"增量重载无法解析旧控件稳定 ID："
						+ std::to_wstring(control->StableId));
					rollbackProperties();
					return false;
				}
				if (currentFound->second->Props != found->second->Props)
				{
					const auto candidateFound = candidateById.find(control->StableId);
					if (candidateFound == candidateById.end()
						|| !candidateFound->second)
					{
						SetError(outError, L"增量重载候选树缺少控件稳定 ID："
							+ std::to_wstring(control->StableId));
						rollbackProperties();
						return false;
					}
					propertySnapshots.push_back(
						InPlaceControlSnapshot::Capture(*control));
					if (!ApplyControlPropertyChanges(
						*currentFound->second,
						*found->second,
						*control,
						*candidateFound->second,
						outError))
					{
						const auto inPlaceError = outError ? *outError : std::wstring{};
						rollbackProperties();
						if (!output._rootsReleased)
						{
							RuntimeDocumentLoadOptions effectiveOptions = inheritedOptions;
							if (!effectiveOptions.DataContext)
								effectiveOptions.DataContext = output._dataContext;
							if (!effectiveOptions.ControlEventResolver)
								effectiveOptions.ControlEventResolver =
									output._controlEventResolver;
							RuntimeDocument replacement;
							if (!Load(
								document, replacement, effectiveOptions, outError)) return false;
							if (!replacement.CommitInheritedFormAttachments(
								output, {}, outError)) return false;
							output = std::move(replacement);
							if (outMode) *outMode = RuntimeDocumentReloadMode::Replaced;
							return true;
						}
						if (output._rootHost)
						{
							RuntimeDocumentLoadOptions effectiveOptions = inheritedOptions;
							if (!effectiveOptions.DataContext)
								effectiveOptions.DataContext = output._dataContext;
							if (!effectiveOptions.ControlEventResolver)
								effectiveOptions.ControlEventResolver =
									output._controlEventResolver;
							return ReloadHosted(
								document, output, effectiveOptions, outMode, outError);
						}
						SetError(outError, inPlaceError);
						return false;
					}
				}
				if (hasBindingChanges)
				{
					const auto candidateFound = candidateById.find(control->StableId);
					if (candidateFound == candidateById.end()
						|| !candidateFound->second)
					{
						SetError(outError, L"增量重载候选树缺少绑定控件稳定 ID："
							+ std::to_wstring(control->StableId));
						rollbackProperties();
						return false;
					}
					nextBindings.push_back(candidateFound->second->DataBindings);
					previousBindings.push_back(control->DataBindings);
				}
				std::map<std::wstring, std::wstring> handlers;
				if (!ReadControlEventHandlers(
					*found->second, handlers, outError))
				{
					rollbackProperties();
					return false;
				}
				hasNextEvents = hasNextEvents || !handlers.empty();
				nextHandlers.push_back(std::move(handlers));
				previousHandlers.push_back(control->EventHandlers);
			}

			auto nextSourceDocument = document;
			auto nextForm = document.Form;
			const auto previousDataContext = output._dataContext;
			const auto previousDataContextSchema = output._dataContextSchema;
			auto nextDataContextSchema = document.DataContextSchema;
			DesignerDataContextSchemaUtils::Canonicalize(nextDataContextSchema);
			if (hasSchemaChanges)
				output._dataContextSchema = std::move(nextDataContextSchema);
			const auto nextDataContext = options.DataContext
				? options.DataContext : previousDataContext;
			const bool changeDataContext = options.DataContext
				&& options.DataContext != previousDataContext;
			if (hasBindingChanges)
			{
				for (size_t index = 0; index < output._controls.size(); ++index)
					output._controls[index]->DataBindings = nextBindings[index];
			}
			const bool reboundDataContext = nextDataContext
				&& (hasBindingChanges || hasSchemaChanges || changeDataContext);
			auto rollbackBindings = [&]() noexcept
			{
				try
				{
					if (hasSchemaChanges)
						output._dataContextSchema = previousDataContextSchema;
					if (hasBindingChanges)
					{
						for (size_t index = 0; index < output._controls.size(); ++index)
							output._controls[index]->DataBindings = previousBindings[index];
					}
					if (reboundDataContext)
					{
						if (previousDataContext)
							(void)output.BindDataContext(previousDataContext, nullptr);
						else
							output.ClearDataBindings();
					}
				}
				catch (...)
				{
					// Best-effort rollback; callers still receive the original failure.
				}
			};
			if (reboundDataContext
				&& !output.BindDataContext(nextDataContext, outError))
			{
				const auto reloadError = outError ? *outError : std::wstring{};
				rollbackBindings();
				rollbackProperties();
				SetError(outError, reloadError);
				return false;
			}

			std::shared_ptr<ControlStyleSheet> nextRuntimeStyleSheet;
			std::vector<std::shared_ptr<const ControlStyleSheet>> previousStyleSheets;
			previousStyleSheets.reserve(output._rootControls.size());
			bool styleApplied = false;
			auto rollbackStyles = [&]() noexcept
			{
				if (!styleApplied) return;
				for (size_t index = 0;
					index < output._rootControls.size()
						&& index < previousStyleSheets.size(); ++index)
				{
					if (output._rootControls[index])
						(void)output._rootControls[index]->SetStyleSheet(
							previousStyleSheets[index], true);
				}
				styleApplied = false;
			};
			if (hasStyleChanges)
			{
				if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
					document.StyleSheet, nextRuntimeStyleSheet, outError,
					document.ResourceBasePath, document.Resources))
				{
					rollbackBindings();
					rollbackProperties();
					return false;
				}
				if (nextDataContext)
					nextRuntimeStyleSheet->SetDataContext(nextDataContext.get());
				styleApplied = true;
				for (auto* root : output._rootControls)
				{
					if (!root)
					{
						previousStyleSheets.push_back({});
						continue;
					}
					previousStyleSheets.push_back(root->GetStyleSheet());
					if (!root->SetStyleSheet(nextRuntimeStyleSheet, true))
					{
						const auto reloadError = L"文档样式表无法原位应用到完整控件树。";
						rollbackStyles();
						rollbackBindings();
						rollbackProperties();
						SetError(outError, reloadError);
						return false;
					}
				}
			}

			std::optional<FormPresentationSnapshot> previousFormPresentation;
			auto rollbackFormPresentation = [&]() noexcept
			{
				if (previousFormPresentation)
					previousFormPresentation->Restore();
			};
			if (output._appliedForm
				&& !HasSameFormPresentation(output._form, nextForm))
			{
				try
				{
					previousFormPresentation =
						FormPresentationSnapshot::Capture(*output._appliedForm);
				}
				catch (...)
				{
					rollbackStyles();
					rollbackBindings();
					rollbackProperties();
					SetError(outError,
						L"无法保存原位重载前的 Form 显示状态。");
					return false;
				}
				if (!ApplyFormModel(nextForm, *output._appliedForm, outError))
				{
					const auto reloadError = outError ? *outError : std::wstring{};
					rollbackFormPresentation();
					rollbackStyles();
					rollbackBindings();
					rollbackProperties();
					SetError(outError, reloadError);
					return false;
				}
			}

			for (size_t index = 0; index < output._controls.size(); ++index)
				output._controls[index]->EventHandlers = std::move(nextHandlers[index]);

			const auto resolver = options.ControlEventResolver
				? options.ControlEventResolver : output._controlEventResolver;
			bool eventBindingSucceeded = true;
			if (resolver)
				eventBindingSucceeded = output.BindControlEvents(resolver, outError);
			else if (options.RequireControlEventResolver && hasNextEvents)
			{
				SetError(outError,
					L"增量重载后的文档包含控件事件，但没有事件名称解析器。");
				eventBindingSucceeded = false;
			}
			else
				output.ClearControlEvents();

			if (!eventBindingSucceeded)
			{
				const auto reloadError = outError ? *outError : std::wstring{};
				for (size_t index = 0; index < output._controls.size(); ++index)
					output._controls[index]->EventHandlers =
						std::move(previousHandlers[index]);
				rollbackFormPresentation();
				rollbackStyles();
				rollbackBindings();
				rollbackProperties();
				SetError(outError, reloadError);
				return false;
			}

			output._form = std::move(nextForm);
			output._styleSheet = document.StyleSheet;
			output._customControls = inheritedOptions.CustomControls;
			output._allowCustomControlProxy =
				inheritedOptions.AllowCustomControlProxy;
			output._sourceDocument = std::move(nextSourceDocument);
			if (outMode) *outMode = RuntimeDocumentReloadMode::InPlace;
			if (outError) outError->clear();
			return true;
		}

		RuntimeDocumentLoadOptions effectiveOptions = inheritedOptions;
		if (!effectiveOptions.DataContext)
			effectiveOptions.DataContext = output._dataContext;
		if (!effectiveOptions.ControlEventResolver)
			effectiveOptions.ControlEventResolver = output._controlEventResolver;

		if (output._rootsReleased)
		{
			if (output._rootHost)
				return ReloadHosted(
					document, output, effectiveOptions, outMode, outError);
			SetError(outError,
				L"控件根所有权已经转移；拓扑、Extra、字体或不支持的属性变化"
				L"不能自动替换宿主树。通用属性、Binding、样式、事件和窗体显示"
				L"属性仍可原位重载。");
			return false;
		}

		bool recomposed = false;
		size_t reusedControlCount = 0;
		auto commitFormAttachments = [&output](
			RuntimeDocument& candidate,
			std::wstring* commitError)
		{
			return candidate.CommitInheritedFormAttachments(
				output, {}, commitError);
		};
		if (!RuntimeDocumentTopologyReloader::TryReload(
			document,
			output,
			effectiveOptions,
			recomposed,
			reusedControlCount,
			outError,
			commitFormAttachments)) return false;
		if (recomposed)
		{
			(void)reusedControlCount;
			if (outMode) *outMode = RuntimeDocumentReloadMode::Recomposed;
			return true;
		}
		RuntimeDocument replacement;
		if (!Load(document, replacement, effectiveOptions, outError)) return false;
		if (!replacement.CommitInheritedFormAttachments(
			output, {}, outError)) return false;
		output = std::move(replacement);
		if (outMode) *outMode = RuntimeDocumentReloadMode::Replaced;
		return true;
	}
	catch (const std::exception&)
	{
		SetError(outError, L"动态文档重载失败：构建候选状态时抛出异常。");
		return false;
	}
	catch (...)
	{
		SetError(outError, L"动态文档重载失败：发生未知异常。");
		return false;
	}
}

bool RuntimeDocumentLoader::ReloadXml(
	const std::string& xml,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	RuntimeDocumentReloadMode* outMode,
	std::wstring* outError)
{
	DesignDocument document;
	if (!DesignDocumentSerializer::FromXml(xml, document, outError)) return false;
	return Reload(document, output, options, outMode, outError);
}

bool RuntimeDocumentLoader::ReloadFile(
	const std::wstring& filePath,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	RuntimeDocumentReloadMode* outMode,
	std::wstring* outError)
{
	DesignDocument document;
	const bool loaded = DetectDesignDocumentFileFormat(filePath)
		== DesignDocumentFileFormat::Xaml
		? XamlDocumentParser::LoadFromFile(
			filePath, document,
			XamlOptionsFor(options.CustomControls
				? options.CustomControls : output._customControls), outError)
		: DesignDocumentSerializer::LoadFromFile(filePath, document, outError);
	if (!loaded) return false;
	return Reload(document, output, options, outMode, outError);
}

bool RuntimeDocumentLoader::ReloadXaml(
	const std::string& xaml,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	RuntimeDocumentReloadMode* outMode,
	std::wstring* outError)
{
	DesignDocument document;
	if (!XamlDocumentParser::FromXaml(
		xaml, document,
		XamlOptionsFor(options.CustomControls
			? options.CustomControls : output._customControls), outError)) return false;
	return Reload(document, output, options, outMode, outError);
}

bool RuntimeDocumentLoader::ReloadXamlFile(
	const std::wstring& filePath,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& options,
	RuntimeDocumentReloadMode* outMode,
	std::wstring* outError)
{
	DesignDocument document;
	if (!XamlDocumentParser::LoadFromFile(
		filePath, document,
		XamlOptionsFor(options.CustomControls
			? options.CustomControls : output._customControls), outError)) return false;
	return Reload(document, output, options, outMode, outError);
}
}
