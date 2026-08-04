#include "DemoWindow.g.h"
#include "Canvas.h"
#include "Layout/Grid.h"
#include "Layout/DockPanel.h"
#include "Layout/RelativePanel.h"
#include "Border.h"
#include "Button.h"
#include "ContentPresenter.h"
#include "ItemsPresenter.h"
#include "Label.h"
#include "Layout/StackPanel.h"
#include "ListBox.h"
#include "ListView.h"
#include "ScrollViewer.h"
#include "ControlTemplate.h"
#include "BindingList.h"
#include "CompiledBindingRecord.h"
#include "ItemTemplate.h"
#include "RelativeSource.h"
#include "TreeView.h"
#include "GroupStyle.h"
#include "DependencyPropertyInfrastructure.h"
#include "StyleInfrastructure.h"
#include "TemplateInfrastructure.h"
#include "TreeInfrastructure.h"
#include "CuiGeneratedFrameworkTheme.h"
#include "HeaderedContentControl.h"
#include "HeaderedItemsControl.h"
#include "Style.h"
#include "Resource.h"
#include "Utils.h"
#include <array>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
	template<typename TValue>
	TValue CuiGeneratedBindingValueAs(const BindingValue& value)
	{
		TValue result{};
		if (value.TryGet(result)) return result;
		if constexpr (std::is_same_v<TValue, cui::drawing::Brush>)
		{
			D2D1_COLOR_F color{};
			if (value.TryGet(color))
				return cui::drawing::MakeSolidColorBrush(color);
		}
		throw std::runtime_error("Generated StaticResource type mismatch");
	}
	// AOT DataList records keep typed fields inline and share one
	// process-static token/metadata/thunk table per record shape.
	class CuiGeneratedDataRecord_DemoChoice_1 final : public CompiledBindingRecord
	{
	public:
		explicit CuiGeneratedDataRecord_DemoChoice_1(std::wstring value0, int value1)
			: CompiledBindingRecord(Properties()),
			  _value0(std::move(value0)),
			  _value1(std::move(value1)) {}

	private:
		static std::span<const CompiledBindingRecordProperty> Properties()
		{
			static const std::array<CompiledBindingRecordProperty, 2> values
			{{
				{ BindingSourcePropertyToken{ 1869304552508798568ULL }, BindingValueKind::String, std::type_index(typeid(std::wstring)), true, true, true,
					+[](const CompiledBindingRecord& record, BindingValue& out)
					{
						const auto& typed = static_cast<const CuiGeneratedDataRecord_DemoChoice_1&>(record);
						out = BindingValue(typed._value0);
						return true;
					},
					+[](CompiledBindingRecord& record, const BindingValue& value)
					{
						std::wstring next{};
						if (!value.TryGet(next))
							return CompiledBindingRecordWriteResult::Failed;
						auto& typed = static_cast<CuiGeneratedDataRecord_DemoChoice_1&>(record);
						if (typed._value0 == next)
							return CompiledBindingRecordWriteResult::Unchanged;
						typed._value0 = std::move(next);
						return CompiledBindingRecordWriteResult::Changed;
					}
				},
				{ BindingSourcePropertyToken{ 13817039935692630216ULL }, BindingValueKind::Int, std::type_index(typeid(int)), true, true, true,
					+[](const CompiledBindingRecord& record, BindingValue& out)
					{
						const auto& typed = static_cast<const CuiGeneratedDataRecord_DemoChoice_1&>(record);
						out = BindingValue(typed._value1);
						return true;
					},
					+[](CompiledBindingRecord& record, const BindingValue& value)
					{
						int next{};
						if (!value.TryGet(next))
							return CompiledBindingRecordWriteResult::Failed;
						auto& typed = static_cast<CuiGeneratedDataRecord_DemoChoice_1&>(record);
						if (typed._value1 == next)
							return CompiledBindingRecordWriteResult::Unchanged;
						typed._value1 = std::move(next);
						return CompiledBindingRecordWriteResult::Changed;
					}
				},
			}};
			return std::span<const CompiledBindingRecordProperty>{ values };
		}

		std::wstring _value0;
		int _value1;
	};

	class CuiGeneratedDataRecord_DemoStatusEntry_2 final : public CompiledBindingRecord
	{
	public:
		explicit CuiGeneratedDataRecord_DemoStatusEntry_2(std::wstring value0)
			: CompiledBindingRecord(Properties()),
			  _value0(std::move(value0)) {}

	private:
		static std::span<const CompiledBindingRecordProperty> Properties()
		{
			static const std::array<CompiledBindingRecordProperty, 1> values
			{{
				{ BindingSourcePropertyToken{ 1869304552508798568ULL }, BindingValueKind::String, std::type_index(typeid(std::wstring)), true, true, true,
					+[](const CompiledBindingRecord& record, BindingValue& out)
					{
						const auto& typed = static_cast<const CuiGeneratedDataRecord_DemoStatusEntry_2&>(record);
						out = BindingValue(typed._value0);
						return true;
					},
					+[](CompiledBindingRecord& record, const BindingValue& value)
					{
						std::wstring next{};
						if (!value.TryGet(next))
							return CompiledBindingRecordWriteResult::Failed;
						auto& typed = static_cast<CuiGeneratedDataRecord_DemoStatusEntry_2&>(record);
						if (typed._value0 == next)
							return CompiledBindingRecordWriteResult::Unchanged;
						typed._value0 = std::move(next);
						return CompiledBindingRecordWriteResult::Changed;
					}
				},
			}};
			return std::span<const CompiledBindingRecordProperty>{ values };
		}

		std::wstring _value0;
	};

	class CuiGeneratedDataRecord_DemoTask_3 final : public CompiledBindingRecord
	{
	public:
		explicit CuiGeneratedDataRecord_DemoTask_3(std::wstring value0, std::wstring value1, int value2, bool value3, int value4)
			: CompiledBindingRecord(Properties()),
			  _value0(std::move(value0)),
			  _value1(std::move(value1)),
			  _value2(std::move(value2)),
			  _value3(std::move(value3)),
			  _value4(std::move(value4)) {}

	private:
		static std::span<const CompiledBindingRecordProperty> Properties()
		{
			static const std::array<CompiledBindingRecordProperty, 5> values
			{{
				{ BindingSourcePropertyToken{ 1869304552508798568ULL }, BindingValueKind::String, std::type_index(typeid(std::wstring)), true, true, true,
					+[](const CompiledBindingRecord& record, BindingValue& out)
					{
						const auto& typed = static_cast<const CuiGeneratedDataRecord_DemoTask_3&>(record);
						out = BindingValue(typed._value0);
						return true;
					},
					+[](CompiledBindingRecord& record, const BindingValue& value)
					{
						std::wstring next{};
						if (!value.TryGet(next))
							return CompiledBindingRecordWriteResult::Failed;
						auto& typed = static_cast<CuiGeneratedDataRecord_DemoTask_3&>(record);
						if (typed._value0 == next)
							return CompiledBindingRecordWriteResult::Unchanged;
						typed._value0 = std::move(next);
						return CompiledBindingRecordWriteResult::Changed;
					}
				},
				{ BindingSourcePropertyToken{ 2922971338924869525ULL }, BindingValueKind::String, std::type_index(typeid(std::wstring)), true, true, true,
					+[](const CompiledBindingRecord& record, BindingValue& out)
					{
						const auto& typed = static_cast<const CuiGeneratedDataRecord_DemoTask_3&>(record);
						out = BindingValue(typed._value1);
						return true;
					},
					+[](CompiledBindingRecord& record, const BindingValue& value)
					{
						std::wstring next{};
						if (!value.TryGet(next))
							return CompiledBindingRecordWriteResult::Failed;
						auto& typed = static_cast<CuiGeneratedDataRecord_DemoTask_3&>(record);
						if (typed._value1 == next)
							return CompiledBindingRecordWriteResult::Unchanged;
						typed._value1 = std::move(next);
						return CompiledBindingRecordWriteResult::Changed;
					}
				},
				{ BindingSourcePropertyToken{ 5396721815081960082ULL }, BindingValueKind::Int, std::type_index(typeid(int)), true, true, true,
					+[](const CompiledBindingRecord& record, BindingValue& out)
					{
						const auto& typed = static_cast<const CuiGeneratedDataRecord_DemoTask_3&>(record);
						out = BindingValue(typed._value2);
						return true;
					},
					+[](CompiledBindingRecord& record, const BindingValue& value)
					{
						int next{};
						if (!value.TryGet(next))
							return CompiledBindingRecordWriteResult::Failed;
						auto& typed = static_cast<CuiGeneratedDataRecord_DemoTask_3&>(record);
						if (typed._value2 == next)
							return CompiledBindingRecordWriteResult::Unchanged;
						typed._value2 = std::move(next);
						return CompiledBindingRecordWriteResult::Changed;
					}
				},
				{ BindingSourcePropertyToken{ 12417925837218019833ULL }, BindingValueKind::Bool, std::type_index(typeid(bool)), true, true, true,
					+[](const CompiledBindingRecord& record, BindingValue& out)
					{
						const auto& typed = static_cast<const CuiGeneratedDataRecord_DemoTask_3&>(record);
						out = BindingValue(typed._value3);
						return true;
					},
					+[](CompiledBindingRecord& record, const BindingValue& value)
					{
						bool next{};
						if (!value.TryGet(next))
							return CompiledBindingRecordWriteResult::Failed;
						auto& typed = static_cast<CuiGeneratedDataRecord_DemoTask_3&>(record);
						if (typed._value3 == next)
							return CompiledBindingRecordWriteResult::Unchanged;
						typed._value3 = std::move(next);
						return CompiledBindingRecordWriteResult::Changed;
					}
				},
				{ BindingSourcePropertyToken{ 13817039935692630216ULL }, BindingValueKind::Int, std::type_index(typeid(int)), true, true, true,
					+[](const CompiledBindingRecord& record, BindingValue& out)
					{
						const auto& typed = static_cast<const CuiGeneratedDataRecord_DemoTask_3&>(record);
						out = BindingValue(typed._value4);
						return true;
					},
					+[](CompiledBindingRecord& record, const BindingValue& value)
					{
						int next{};
						if (!value.TryGet(next))
							return CompiledBindingRecordWriteResult::Failed;
						auto& typed = static_cast<CuiGeneratedDataRecord_DemoTask_3&>(record);
						if (typed._value4 == next)
							return CompiledBindingRecordWriteResult::Unchanged;
						typed._value4 = std::move(next);
						return CompiledBindingRecordWriteResult::Changed;
					}
				},
			}};
			return std::span<const CompiledBindingRecordProperty>{ values };
		}

		std::wstring _value0;
		std::wstring _value1;
		int _value2;
		bool _value3;
		int _value4;
	};

	class CuiGeneratedDataRecord_AnalyticsRow_4 final : public CompiledBindingRecord
	{
	public:
		explicit CuiGeneratedDataRecord_AnalyticsRow_4(std::wstring value0, std::wstring value1, std::wstring value2, std::wstring value3, std::wstring value4)
			: CompiledBindingRecord(Properties()),
			  _value0(std::move(value0)),
			  _value1(std::move(value1)),
			  _value2(std::move(value2)),
			  _value3(std::move(value3)),
			  _value4(std::move(value4)) {}

	private:
		static std::span<const CompiledBindingRecordProperty> Properties()
		{
			static const std::array<CompiledBindingRecordProperty, 5> values
			{{
				{ BindingSourcePropertyToken{ 170753274995538377ULL }, BindingValueKind::String, std::type_index(typeid(std::wstring)), true, true, true,
					+[](const CompiledBindingRecord& record, BindingValue& out)
					{
						const auto& typed = static_cast<const CuiGeneratedDataRecord_AnalyticsRow_4&>(record);
						out = BindingValue(typed._value0);
						return true;
					},
					+[](CompiledBindingRecord& record, const BindingValue& value)
					{
						std::wstring next{};
						if (!value.TryGet(next))
							return CompiledBindingRecordWriteResult::Failed;
						auto& typed = static_cast<CuiGeneratedDataRecord_AnalyticsRow_4&>(record);
						if (typed._value0 == next)
							return CompiledBindingRecordWriteResult::Unchanged;
						typed._value0 = std::move(next);
						return CompiledBindingRecordWriteResult::Changed;
					}
				},
				{ BindingSourcePropertyToken{ 2419962606964216481ULL }, BindingValueKind::String, std::type_index(typeid(std::wstring)), true, true, true,
					+[](const CompiledBindingRecord& record, BindingValue& out)
					{
						const auto& typed = static_cast<const CuiGeneratedDataRecord_AnalyticsRow_4&>(record);
						out = BindingValue(typed._value1);
						return true;
					},
					+[](CompiledBindingRecord& record, const BindingValue& value)
					{
						std::wstring next{};
						if (!value.TryGet(next))
							return CompiledBindingRecordWriteResult::Failed;
						auto& typed = static_cast<CuiGeneratedDataRecord_AnalyticsRow_4&>(record);
						if (typed._value1 == next)
							return CompiledBindingRecordWriteResult::Unchanged;
						typed._value1 = std::move(next);
						return CompiledBindingRecordWriteResult::Changed;
					}
				},
				{ BindingSourcePropertyToken{ 2961787330939152573ULL }, BindingValueKind::String, std::type_index(typeid(std::wstring)), true, true, true,
					+[](const CompiledBindingRecord& record, BindingValue& out)
					{
						const auto& typed = static_cast<const CuiGeneratedDataRecord_AnalyticsRow_4&>(record);
						out = BindingValue(typed._value2);
						return true;
					},
					+[](CompiledBindingRecord& record, const BindingValue& value)
					{
						std::wstring next{};
						if (!value.TryGet(next))
							return CompiledBindingRecordWriteResult::Failed;
						auto& typed = static_cast<CuiGeneratedDataRecord_AnalyticsRow_4&>(record);
						if (typed._value2 == next)
							return CompiledBindingRecordWriteResult::Unchanged;
						typed._value2 = std::move(next);
						return CompiledBindingRecordWriteResult::Changed;
					}
				},
				{ BindingSourcePropertyToken{ 12717390152918474433ULL }, BindingValueKind::String, std::type_index(typeid(std::wstring)), true, true, true,
					+[](const CompiledBindingRecord& record, BindingValue& out)
					{
						const auto& typed = static_cast<const CuiGeneratedDataRecord_AnalyticsRow_4&>(record);
						out = BindingValue(typed._value3);
						return true;
					},
					+[](CompiledBindingRecord& record, const BindingValue& value)
					{
						std::wstring next{};
						if (!value.TryGet(next))
							return CompiledBindingRecordWriteResult::Failed;
						auto& typed = static_cast<CuiGeneratedDataRecord_AnalyticsRow_4&>(record);
						if (typed._value3 == next)
							return CompiledBindingRecordWriteResult::Unchanged;
						typed._value3 = std::move(next);
						return CompiledBindingRecordWriteResult::Changed;
					}
				},
				{ BindingSourcePropertyToken{ 12881601721173548923ULL }, BindingValueKind::String, std::type_index(typeid(std::wstring)), true, true, true,
					+[](const CompiledBindingRecord& record, BindingValue& out)
					{
						const auto& typed = static_cast<const CuiGeneratedDataRecord_AnalyticsRow_4&>(record);
						out = BindingValue(typed._value4);
						return true;
					},
					+[](CompiledBindingRecord& record, const BindingValue& value)
					{
						std::wstring next{};
						if (!value.TryGet(next))
							return CompiledBindingRecordWriteResult::Failed;
						auto& typed = static_cast<CuiGeneratedDataRecord_AnalyticsRow_4&>(record);
						if (typed._value4 == next)
							return CompiledBindingRecordWriteResult::Unchanged;
						typed._value4 = std::move(next);
						return CompiledBindingRecordWriteResult::Changed;
					}
				},
			}};
			return std::span<const CompiledBindingRecordProperty>{ values };
		}

		std::wstring _value0;
		std::wstring _value1;
		std::wstring _value2;
		std::wstring _value3;
		std::wstring _value4;
	};

	class CuiGeneratedControlTemplate final
		: public IControlTemplate,
		  public std::enable_shared_from_this<CuiGeneratedControlTemplate>
	{
	public:
		using ApplyCallback = std::function<bool(
			Control&, std::wstring*)>;
		using HostFactory = std::function<std::unique_ptr<Control>()>;

		CuiGeneratedControlTemplate(
			UIClass targetType,
			std::wstring identity,
			HostFactory hostFactory)
			: _targetType(targetType),
			  _identity(std::move(identity)),
			  _hostFactory(std::move(hostFactory)) {}

		void SetApplyCallback(ApplyCallback value)
		{
			_apply = std::move(value);
		}

		UIClass TargetType() const noexcept override
		{
			return _targetType;
		}

		bool Apply(Control& owner,
			std::wstring* outError = nullptr) const override
		{
			if (!IsUIClassAssignableFrom(_targetType, owner.Type()))
			{
				if (outError) *outError =
					L"生成的 ControlTemplate TargetType 与宿主不兼容：" + _identity;
				return false;
			}
			if (!_apply)
			{
				if (outError) *outError =
					L"生成的 ControlTemplate 尚未完成初始化：" + _identity;
				return false;
			}
			return _apply(owner, outError);
		}

		std::unique_ptr<Control> Build(
			std::wstring* outError = nullptr) const override
		{
			auto owner = _hostFactory ? _hostFactory() : nullptr;
			if (!owner)
			{
				if (outError) *outError =
					L"生成的 ControlTemplate 无法构造宿主：" + _identity;
				return {};
			}
			auto self = std::static_pointer_cast<const IControlTemplate>(shared_from_this());
			if (!cui::framework::TemplateAccess::SetTemplate(
				*owner, ControlTemplateReference(std::move(self)),
				DependencyPropertyValueSource::Local))
			{
				if (outError) *outError =
					L"生成的 ControlTemplate 无法写入宿主：" + _identity;
				return {};
			}
			(void)owner->ApplyTemplate();
			if (!cui::framework::TemplateAccess::GetTemplateRoot(*owner)
				|| !owner->LastTemplateError().empty())
			{
				if (outError) *outError = owner->LastTemplateError().empty()
					? L"生成的 ControlTemplate 未生成视觉根：" + _identity
					: owner->LastTemplateError();
				return {};
			}
			if (outError) outError->clear();
			return owner;
		}

	private:
		UIClass _targetType = UIClass::UI_Base;
		std::wstring _identity;
		HostFactory _hostFactory;
		ApplyCallback _apply;
	};
	class CuiGeneratedItemTemplate final : public IItemTemplate
	{
	public:
		using BuildCallback = std::function<std::unique_ptr<Control>(
			const BindingSourceReference&, size_t, std::wstring*)>;
		using ChildSourceCallback = std::function<bool(
			const BindingSourceReference&, BindingListReference&, std::wstring*)>;
		using ObserveCallback = std::function<BindingPathObservation(
			const BindingSourceReference&, std::function<void()>)>;

		CuiGeneratedItemTemplate(
			DataTypeToken dataType,
			bool hierarchical,
			BuildCallback build,
			ChildSourceCallback childSource = {},
			ObserveCallback observe = {})
			: _dataType(dataType),
			  _hierarchical(hierarchical),
			  _build(std::move(build)),
			  _childSource(std::move(childSource)),
			  _observe(std::move(observe)) {}

		DataTypeToken GetDataTypeToken() const noexcept override
		{
			return _dataType;
		}

		bool IsHierarchical() const noexcept override
		{
			return _hierarchical;
		}

		std::unique_ptr<Control> Build(
			const BindingSourceReference& item,
			size_t index,
			std::wstring* outError = nullptr) const override
		{
			if (_build) return _build(item, index, outError);
			if (outError) *outError = L"生成的 DataTemplate 尚未初始化。";
			return {};
		}

		bool TryGetVisualChildItemsSource(
			const BindingSourceReference& item,
			BindingListReference& out,
			std::wstring* outError = nullptr) const override
		{
			out = {};
			if (_childSource) return _childSource(item, out, outError);
			if (outError) outError->clear();
			return true;
		}

		BindingPathObservation ObserveChildItemsSource(
			const BindingSourceReference& item,
			std::function<void()> changed) const override
		{
			return _observe
				? _observe(item, std::move(changed))
				: BindingPathObservation{};
		}

	private:
		DataTypeToken _dataType;
		bool _hierarchical = false;
		BuildCallback _build;
		ChildSourceCallback _childSource;
		ObserveCallback _observe;
	};
}

ComponentTypeToken DemoWindowGeneratedFeatureCard::ComponentTypeId() noexcept
{
	return ComponentTypeToken{ 25017113223007489ULL };
}

const DependencyProperty& DemoWindowGeneratedFeatureCard::CaptionProperty()
{
	// CUI:AOT dependency-property=static
	static const auto value = []
	{
		DependencyPropertyOptions<DemoWindowGeneratedFeatureCard, std::wstring> options;
		options.DefaultValue = L"FeatureCard";
		options.Flags = static_cast<DependencyPropertyFlags>(0);
		options.DefaultUpdateMode = DataSourceUpdateMode::OnPropertyChanged;
		return DependencyPropertyRegistry::RegisterStatic<DemoWindowGeneratedFeatureCard, std::wstring>(
#if CUI_ENABLE_DYNAMIC_XAML
			L"Caption",
#else
			// CUI:AOT dependency-property-identity=token
			BindingSourcePropertyToken{ 7131070297711251227ULL },
#endif
			std::move(options));
	}();
	if (!value) throw std::logic_error("Generated component property registration failed");
	return *value;
}

std::wstring DemoWindowGeneratedFeatureCard::GetCaption() const
{
	return GetDependencyPropertyValue<std::wstring>(CaptionProperty());
}

void DemoWindowGeneratedFeatureCard::SetCaption(std::wstring value)
{
	(void)SetDependencyPropertyValue(CaptionProperty(), std::move(value));
}

const DependencyPropertyKey& DemoWindowGeneratedFeatureCard::StatePropertyKey()
{
	// CUI:AOT dependency-property=static
	static const auto value = []
	{
		DependencyPropertyOptions<DemoWindowGeneratedFeatureCard, std::wstring> options;
		options.DefaultValue = L"Created";
		options.Flags = static_cast<DependencyPropertyFlags>(0);
		options.DefaultUpdateMode = DataSourceUpdateMode::OnPropertyChanged;
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<DemoWindowGeneratedFeatureCard, std::wstring>(
#if CUI_ENABLE_DYNAMIC_XAML
			L"State",
#else
			// CUI:AOT dependency-property-identity=token
			BindingSourcePropertyToken{ 5396721815081960082ULL },
#endif
			std::move(options));
	}();
	return *value;
}

const DependencyProperty& DemoWindowGeneratedFeatureCard::StateProperty()
{
	return StatePropertyKey().Property();
}

std::wstring DemoWindowGeneratedFeatureCard::GetState() const
{
	return GetDependencyPropertyValue<std::wstring>(StateProperty());
}

bool DemoWindowGeneratedFeatureCard::PublishState(std::wstring value)
{
	return TrySetReadOnlyPropertyValue(StatePropertyKey(), BindingValue(std::move(value)));
}

const DependencyProperty& DemoWindowGeneratedFeatureCard::IsActiveProperty()
{
	// CUI:AOT dependency-property=static
	static const auto value = []
	{
		DependencyPropertyOptions<DemoWindowGeneratedFeatureCard, bool> options;
		options.DefaultValue = false;
		options.Flags = static_cast<DependencyPropertyFlags>(0);
		options.DefaultUpdateMode = DataSourceUpdateMode::OnPropertyChanged;
		return DependencyPropertyRegistry::RegisterStatic<DemoWindowGeneratedFeatureCard, bool>(
#if CUI_ENABLE_DYNAMIC_XAML
			L"IsActive",
#else
			// CUI:AOT dependency-property-identity=token
			BindingSourcePropertyToken{ 6010810381380122003ULL },
#endif
			std::move(options));
	}();
	if (!value) throw std::logic_error("Generated component property registration failed");
	return *value;
}

bool DemoWindowGeneratedFeatureCard::GetIsActive() const
{
	return GetDependencyPropertyValue<bool>(IsActiveProperty());
}

void DemoWindowGeneratedFeatureCard::SetIsActive(bool value)
{
	(void)SetDependencyPropertyValue(IsActiveProperty(), std::move(value));
}

const DependencyProperty& DemoWindowGeneratedFeatureCard::AccentColorProperty()
{
	// CUI:AOT dependency-property=static
	static const auto value = []
	{
		DependencyPropertyOptions<DemoWindowGeneratedFeatureCard, D2D1_COLOR_F> options;
		options.DefaultValue = D2D1_COLOR_F{0.184314f, 0.435294f, 0.894118f, 1.f};
		options.Flags = static_cast<DependencyPropertyFlags>(4);
		options.DefaultUpdateMode = DataSourceUpdateMode::OnPropertyChanged;
		return DependencyPropertyRegistry::RegisterStatic<DemoWindowGeneratedFeatureCard, D2D1_COLOR_F>(
#if CUI_ENABLE_DYNAMIC_XAML
			L"AccentColor",
#else
			// CUI:AOT dependency-property-identity=token
			BindingSourcePropertyToken{ 4593923492146743622ULL },
#endif
			std::move(options));
	}();
	if (!value) throw std::logic_error("Generated component property registration failed");
	return *value;
}

D2D1_COLOR_F DemoWindowGeneratedFeatureCard::GetAccentColor() const
{
	return GetDependencyPropertyValue<D2D1_COLOR_F>(AccentColorProperty());
}

void DemoWindowGeneratedFeatureCard::SetAccentColor(D2D1_COLOR_F value)
{
	(void)SetDependencyPropertyValue(AccentColorProperty(), std::move(value));
}

const DependencyProperty& DemoWindowGeneratedFeatureCard::ContentPaddingProperty()
{
	// CUI:AOT dependency-property=static
	static const auto value = []
	{
		DependencyPropertyOptions<DemoWindowGeneratedFeatureCard, Thickness> options;
		options.DefaultValue = Thickness(12.f, 12.f, 12.f, 12.f);
		options.Flags = static_cast<DependencyPropertyFlags>(1);
		options.DefaultUpdateMode = DataSourceUpdateMode::OnPropertyChanged;
		return DependencyPropertyRegistry::RegisterStatic<DemoWindowGeneratedFeatureCard, Thickness>(
#if CUI_ENABLE_DYNAMIC_XAML
			L"ContentPadding",
#else
			// CUI:AOT dependency-property-identity=token
			BindingSourcePropertyToken{ 12241634644531267661ULL },
#endif
			std::move(options));
	}();
	if (!value) throw std::logic_error("Generated component property registration failed");
	return *value;
}

Thickness DemoWindowGeneratedFeatureCard::GetContentPadding() const
{
	return GetDependencyPropertyValue<Thickness>(ContentPaddingProperty());
}

void DemoWindowGeneratedFeatureCard::SetContentPadding(Thickness value)
{
	(void)SetDependencyPropertyValue(ContentPaddingProperty(), std::move(value));
}

ComponentTypeToken DemoWindowGeneratedFeatureCard::GetCompiledComponentTypeTokenCore() const noexcept
{
	return ComponentTypeId();
}

const DependencyPropertyMetadata* DemoWindowGeneratedFeatureCard::FindCompiledComponentPropertyCore(
	ComponentPropertyToken property) const
{
	switch (property.Value)
	{
	case 7131070297711251227ULL:
		return const_cast<DemoWindowGeneratedFeatureCard*>(this)->GetPropertyMetadata(CaptionProperty());
	case 5396721815081960082ULL:
		return const_cast<DemoWindowGeneratedFeatureCard*>(this)->GetPropertyMetadata(StateProperty());
	case 6010810381380122003ULL:
		return const_cast<DemoWindowGeneratedFeatureCard*>(this)->GetPropertyMetadata(IsActiveProperty());
	case 4593923492146743622ULL:
		return const_cast<DemoWindowGeneratedFeatureCard*>(this)->GetPropertyMetadata(AccentColorProperty());
	case 12241634644531267661ULL:
		return const_cast<DemoWindowGeneratedFeatureCard*>(this)->GetPropertyMetadata(ContentPaddingProperty());
	default:
		return nullptr;
	}
}

bool DemoWindowGeneratedFeatureCard::IsCompiledComponentPropertyCore(
	const DependencyPropertyMetadata& metadata) const noexcept
{
	return metadata.OwnerType() == std::type_index(typeid(DemoWindowGeneratedFeatureCard));
}

const DeclarativeEventDefinition& DemoWindowGeneratedFeatureCard::InvokedEvent() noexcept
{
	// Writable storage prevents Release /OPT:ICF from folding distinct event identities.
	static DeclarativeEventDefinition value(BindingValueKind::Empty, DeclarativeEventRoutingStrategy::Bubble);
	return value;
}

EventConnection DemoWindowGeneratedFeatureCard::SubscribeInvoked(
	DeclarativeEvent::std_function_type handler,
	bool handledEventsToo)
{
	return OnDeclarativeEvent.Subscribe(
		[this, handler = std::move(handler), handledEventsToo](
			Control* sender, DeclarativeEventArgs& args) mutable
		{
			if (args.OriginalSource != this
				|| args.Definition != &InvokedEvent()
				|| (args.Handled && !handledEventsToo)) return;
			if (handler) handler(sender, args);
		});
}

bool DemoWindowGeneratedFeatureCard::RaiseInvoked()
{
	return RaiseDeclarativeEvent(InvokedEvent(), BindingValue{});
}

const DeclarativeEventDefinition& DemoWindowGeneratedFeatureCard::PulseEvent() noexcept
{
	// Writable storage prevents Release /OPT:ICF from folding distinct event identities.
	static DeclarativeEventDefinition value(BindingValueKind::Empty, DeclarativeEventRoutingStrategy::Direct);
	return value;
}

EventConnection DemoWindowGeneratedFeatureCard::SubscribePulse(
	DeclarativeEvent::std_function_type handler,
	bool handledEventsToo)
{
	return OnDeclarativeEvent.Subscribe(
		[this, handler = std::move(handler), handledEventsToo](
			Control* sender, DeclarativeEventArgs& args) mutable
		{
			if (args.OriginalSource != this
				|| args.Definition != &PulseEvent()
				|| (args.Handled && !handledEventsToo)) return;
			if (handler) handler(sender, args);
		});
}

bool DemoWindowGeneratedFeatureCard::RaisePulse()
{
	return RaiseDeclarativeEvent(PulseEvent(), BindingValue{});
}

const DeclarativeEventDefinition& DemoWindowGeneratedFeatureCard::StopPulseEvent() noexcept
{
	// Writable storage prevents Release /OPT:ICF from folding distinct event identities.
	static DeclarativeEventDefinition value(BindingValueKind::Empty, DeclarativeEventRoutingStrategy::Direct);
	return value;
}

EventConnection DemoWindowGeneratedFeatureCard::SubscribeStopPulse(
	DeclarativeEvent::std_function_type handler,
	bool handledEventsToo)
{
	return OnDeclarativeEvent.Subscribe(
		[this, handler = std::move(handler), handledEventsToo](
			Control* sender, DeclarativeEventArgs& args) mutable
		{
			if (args.OriginalSource != this
				|| args.Definition != &StopPulseEvent()
				|| (args.Handled && !handledEventsToo)) return;
			if (handler) handler(sender, args);
		});
}

bool DemoWindowGeneratedFeatureCard::RaiseStopPulse()
{
	return RaiseDeclarativeEvent(StopPulseEvent(), BindingValue{});
}

DemoWindowGeneratedFeatureCard::DemoWindowGeneratedFeatureCard()
	: Canvas()
{
	(void)ClearPropertyValues();
	std::wstring error;
	if (!InitializeGeneratedTemplate(&error))
		throw std::runtime_error("Generated component template initialization failed: " + Convert::WStringToString(error));
}

bool DemoWindowGeneratedFeatureCard::InitializeGeneratedTemplate(std::wstring* outError)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	try
	{
		// PART_Root
		auto __owned_PART_Root = std::make_unique<Border>();
		auto* PART_Root = __owned_PART_Root.get();
		(void)PART_Root->ClearPropertyValues();
		_part_PART_Root = PART_Root;
		// stackPanel1
		auto __owned_stackPanel1 = std::make_unique<StackPanel>();
		auto* stackPanel1 = __owned_stackPanel1.get();
		(void)stackPanel1->ClearPropertyValues();
		_part_stackPanel1 = stackPanel1;
		// PART_Caption
		auto __owned_PART_Caption = std::make_unique<Label>();
		auto* PART_Caption = __owned_PART_Caption.get();
		(void)PART_Caption->ClearPropertyValues();
		_part_PART_Caption = PART_Caption;
		// PART_State
		auto __owned_PART_State = std::make_unique<Label>();
		auto* PART_State = __owned_PART_State.get();
		(void)PART_State->ClearPropertyValues();
		_part_PART_State = PART_State;
		// PART_Content
		auto __owned_PART_Content = std::make_unique<StackPanel>();
		auto* PART_Content = __owned_PART_Content.get();
		(void)PART_Content->ClearPropertyValues();
		_part_PART_Content = PART_Content;
		// PART_Invoke
		auto __owned_PART_Invoke = std::make_unique<Button>();
		auto* PART_Invoke = __owned_PART_Invoke.get();
		(void)PART_Invoke->ClearPropertyValues();
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_Invoke, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
		_part_PART_Invoke = PART_Invoke;
		// PART_Actions
		auto __owned_PART_Actions = std::make_unique<StackPanel>();
		auto* PART_Actions = __owned_PART_Actions.get();
		(void)PART_Actions->ClearPropertyValues();
		_part_PART_Actions = PART_Actions;

		cui::framework::TreeAccess::SetTemplatedParent(*PART_Root, this);
		if (!cui::framework::TemplateAccess::RegisterTemplatePart(*this, TemplatePartToken{ 5054319698700794937ULL }, PART_Root))
			return fail(L"ComponentDefinition 模板部件注册失败。");
		cui::framework::TreeAccess::SetTemplatedParent(*stackPanel1, this);
		if (!cui::framework::TemplateAccess::RegisterTemplatePart(*this, TemplatePartToken{ 4045250499287358562ULL }, stackPanel1))
			return fail(L"ComponentDefinition 模板部件注册失败。");
		cui::framework::TreeAccess::SetTemplatedParent(*PART_Caption, this);
		if (!cui::framework::TemplateAccess::RegisterTemplatePart(*this, TemplatePartToken{ 5592219433126130033ULL }, PART_Caption))
			return fail(L"ComponentDefinition 模板部件注册失败。");
		cui::framework::TreeAccess::SetTemplatedParent(*PART_State, this);
		if (!cui::framework::TemplateAccess::RegisterTemplatePart(*this, TemplatePartToken{ 9017634462464554818ULL }, PART_State))
			return fail(L"ComponentDefinition 模板部件注册失败。");
		cui::framework::TreeAccess::SetTemplatedParent(*PART_Content, this);
		if (!cui::framework::TemplateAccess::RegisterTemplatePart(*this, TemplatePartToken{ 833545370875870774ULL }, PART_Content))
			return fail(L"ComponentDefinition 模板部件注册失败。");
		_presenter_Content = PART_Content;
		cui::framework::TreeAccess::SetTemplatedParent(*PART_Invoke, this);
		if (!cui::framework::TemplateAccess::RegisterTemplatePart(*this, TemplatePartToken{ 4911937847342414057ULL }, PART_Invoke))
			return fail(L"ComponentDefinition 模板部件注册失败。");
		cui::framework::TreeAccess::SetTemplatedParent(*PART_Actions, this);
		if (!cui::framework::TemplateAccess::RegisterTemplatePart(*this, TemplatePartToken{ 15471849649328326384ULL }, PART_Actions))
			return fail(L"ComponentDefinition 模板部件注册失败。");
		_presenter_Actions = PART_Actions;

		// ControlTemplate-authored properties/resources
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_Root, Control::CanvasLeftProperty(), BindingValue(2.f), DependencyPropertyValueSource::Template);
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_Root, Control::BackgroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.968627f, 0.976471f, 0.988235f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_Root, Control::BorderBrushProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.827451f, 0.862745f, 0.909804f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_Root, Control::BorderThicknessProperty(), BindingValue(Thickness(1.f, 1.f, 1.f, 1.f)), DependencyPropertyValueSource::Template);
		// ControlTemplate-authored properties/resources
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_Caption, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(25.f)), DependencyPropertyValueSource::Template);
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_Caption, Control::FontSizeProperty(), BindingValue(16.0), DependencyPropertyValueSource::Template);
		// ControlTemplate-authored properties/resources
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_State, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(22.f)), DependencyPropertyValueSource::Template);
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_State, Control::ForegroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.454902f, 0.513726f, 0.6f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_State, Control::FontSizeProperty(), BindingValue(12.0), DependencyPropertyValueSource::Template);
		// ControlTemplate-authored properties/resources
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_Invoke, Button::ContentProperty(), BindingValue(L"Raise XAML Invoked"), DependencyPropertyValueSource::Template);
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_Invoke, Control::WidthProperty(), BindingValue(cui::layout::Length::Fixed(170.f)), DependencyPropertyValueSource::Template);
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_Invoke, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(28.f)), DependencyPropertyValueSource::Template);
		// ControlTemplate-authored properties/resources
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_Actions, StackPanel::OrientationProperty(), BindingValue(0), DependencyPropertyValueSource::Template);
		(void)cui::framework::DependencyPropertyAccess::SetValue(*PART_Actions, Control::MarginProperty(), BindingValue(Thickness(0.f, 4.f, 0.f, 0.f)), DependencyPropertyValueSource::Template);
		if (!PART_Root->DataBindings.AddTemplateBinding(Border::PaddingProperty(), *this, DemoWindowGeneratedFeatureCard::ContentPaddingProperty()))
			return fail(L"ComponentDefinition TemplateBinding 安装失败。");
		if (!PART_Caption->DataBindings.AddTemplateBinding(Control::ForegroundProperty(), *this, DemoWindowGeneratedFeatureCard::AccentColorProperty()))
			return fail(L"ComponentDefinition TemplateBinding 安装失败。");
		if (!PART_Caption->DataBindings.AddTemplateBinding(Label::TextProperty(), *this, DemoWindowGeneratedFeatureCard::CaptionProperty()))
			return fail(L"ComponentDefinition TemplateBinding 安装失败。");
		if (!PART_State->DataBindings.AddTemplateBinding(Label::TextProperty(), *this, DemoWindowGeneratedFeatureCard::StateProperty()))
			return fail(L"ComponentDefinition TemplateBinding 安装失败。");
		(void)PART_Root->AddInputBinding(KeyBinding{ RoutedCommand(L"Demo.Component.ClassProbe"), KeyGesture{ Key::F10, ModifierKeys::Control }, std::wstring(L"template-native-input"), PART_Root });
		cui::framework::TemplateAccess::RetainTemplateEventConnection(*this,
			PART_Invoke->Click.Subscribe([this](auto&&...)
			{
				(void)RaiseInvoked();
			}));
		if (!cui::framework::TemplateAccess::SetTemplateRoot(*this, std::move(__owned_PART_Root)))
			return fail(L"ComponentDefinition 模板根安装失败。");
		cui::framework::TreeAccess::SetLogicalParent(*PART_Root, nullptr);
		PART_Root->SetChild(std::move(__owned_stackPanel1));
		cui::framework::TreeAccess::SetLogicalParent(*stackPanel1, nullptr);
		stackPanel1->AddOwned(std::move(__owned_PART_Caption));
		cui::framework::TreeAccess::SetLogicalParent(*PART_Caption, nullptr);
		stackPanel1->AddOwned(std::move(__owned_PART_State));
		cui::framework::TreeAccess::SetLogicalParent(*PART_State, nullptr);
		stackPanel1->AddOwned(std::move(__owned_PART_Content));
		cui::framework::TreeAccess::SetLogicalParent(*PART_Content, nullptr);
		stackPanel1->AddOwned(std::move(__owned_PART_Invoke));
		cui::framework::TreeAccess::SetLogicalParent(*PART_Invoke, nullptr);
		stackPanel1->AddOwned(std::move(__owned_PART_Actions));
		cui::framework::TreeAccess::SetLogicalParent(*PART_Actions, nullptr);
		{
			// AOT interaction program: process-static structure plus call-local values and targets.
			const BindingValue __cuiInteraction_values[] = {
				BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.968627f, 0.976471f, 0.988235f, 1.f}; return value; }()),
				BindingValue(true),
				BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.917647f, 0.94902f, 1.f, 1.f}; return value; }()),
				BindingValue(12.f),
				BindingValue(D2D1_COLOR_F{0.082353f, 0.588235f, 0.415686f, 1.f}),
				BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.866667f, 0.913725f, 1.f, 1.f}; return value; }()),
				BindingValue(D2D1_COLOR_F{0.184314f, 0.435294f, 0.894118f, 1.f}),
				BindingValue(18.f),
				BindingValue(24.f)
			};
			static const CompiledInteractionPropertyOperand __cuiInteraction_property_operands[] = {
				{ 1u, DependencyPropertyReference(Control::BackgroundProperty()) },
				{ 0u, DependencyPropertyReference(DemoWindowGeneratedFeatureCard::IsActiveProperty()) },
				{ 1u, DependencyPropertyReference(Control::CanvasLeftProperty()) },
				{ 2u, DependencyPropertyReference(Control::ForegroundProperty()) },
				{ 3u, DependencyPropertyReference(Control::ForegroundProperty()) }
			};
			static constexpr CompiledInteractionConditionOp __cuiInteraction_conditions[] = {
				{ 1u, 1u }
			};
			static constexpr CompiledInteractionSetterOp __cuiInteraction_setters[] = {
				{ 0u, 0u },
				{ 0u, 2u },
				{ 2u, 3u },
				{ 3u, 4u },
				{ 0u, 5u },
				{ 4u, 6u }
			};
			static constexpr CompiledInteractionKeyFrameOp __cuiInteraction_key_frames[] = {
				{ DeclarativeKeyFrameKind::Linear, 160ULL, 7u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f },
				{ DeclarativeKeyFrameKind::Spline, 400ULL, 8u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.25f, 0.1f, 0.25f, 1.f }
			};
			static constexpr CompiledInteractionAnimationOp __cuiInteraction_animations[] = {
				{ DeclarativeAnimationKind::Double, 2u, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, { 0u, 2u }, false, false, 0ULL, 400ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut }
			};
			static const DeclarativeEventDefinition* const __cuiInteraction_state_events[] = {
				&DemoWindowGeneratedFeatureCard::InvokedEvent()
			};
			static constexpr CompiledInteractionStateOp __cuiInteraction_states[] = {
				{ VisualStateToken{ 175525885933743510ULL }, { 0u, 0u }, { 0u, 0u }, { 0u, 1u }, { 0u, 0u } },
				{ VisualStateToken{ 16398036358945220017ULL }, { 0u, 1u }, { 0u, 0u }, { 1u, 3u }, { 0u, 0u } },
				{ VisualStateToken{ 18352349169738189529ULL }, { 1u, 0u }, { 0u, 1u }, { 4u, 2u }, { 0u, 0u } }
			};
			static constexpr CompiledInteractionTransitionOp __cuiInteraction_transitions[] = {
				{ 0u, 1u, 220ULL, DeclarativeEasingKind::Quadratic, DeclarativeEasingMode::EaseInOut, { 0u, 0u } }
			};
			static constexpr uint32_t __cuiInteraction_group_condition_operands[] = {
				1u
			};
			static constexpr CompiledInteractionGroupOp __cuiInteraction_groups[] = {
				{ VisualStateGroupToken{ 410524045902418547ULL }, { 0u, 3u }, { 0u, 1u }, 0u, { 0u, 1u } }
			};
			static constexpr CompiledInteractionStoryboardOp __cuiInteraction_storyboards[] = {
				{ { 0u, 1u } }
			};
			static constexpr CompiledInteractionActionOp __cuiInteraction_actions[] = {
				{ DeclarativeStoryboardActionKind::Begin, 0u },
				{ DeclarativeStoryboardActionKind::Stop, 0u }
			};
			static const CompiledInteractionEventTriggerOp __cuiInteraction_event_triggers[] = {
				{ &DemoWindowGeneratedFeatureCard::PulseEvent(), RoutedEventId::None, { 0u, 1u } },
				{ &DemoWindowGeneratedFeatureCard::StopPulseEvent(), RoutedEventId::None, { 1u, 1u } }
			};
			static const CompiledInteractionProgramView __cuiInteractionProgram{
				CompiledInteractionProgramViewVersion,
				4u,
				std::span<const CompiledInteractionPropertyOperand>{ __cuiInteraction_property_operands }, // PropertyOperands
				{}, // ObjectPathChildIndices
				{}, // ObjectPaths
				std::span<const CompiledInteractionConditionOp>{ __cuiInteraction_conditions }, // Conditions
				std::span<const CompiledInteractionSetterOp>{ __cuiInteraction_setters }, // Setters
				std::span<const CompiledInteractionKeyFrameOp>{ __cuiInteraction_key_frames }, // KeyFrames
				std::span<const CompiledInteractionAnimationOp>{ __cuiInteraction_animations }, // Animations
				std::span<const DeclarativeEventDefinition* const>{ __cuiInteraction_state_events }, // StateEvents
				std::span<const CompiledInteractionStateOp>{ __cuiInteraction_states }, // States
				std::span<const CompiledInteractionTransitionOp>{ __cuiInteraction_transitions }, // Transitions
				std::span<const uint32_t>{ __cuiInteraction_group_condition_operands }, // GroupConditionOperands
				std::span<const CompiledInteractionGroupOp>{ __cuiInteraction_groups }, // Groups
				std::span<const CompiledInteractionStoryboardOp>{ __cuiInteraction_storyboards }, // Storyboards
				std::span<const CompiledInteractionActionOp>{ __cuiInteraction_actions }, // Actions
				std::span<const CompiledInteractionEventTriggerOp>{ __cuiInteraction_event_triggers } // EventTriggers
			};
			std::array<Control*, 4> __cuiInteractionTargets{
				&(*this),
				PART_Root,
				PART_State,
				PART_Caption
			};
			std::wstring interactionError;
			if (!cui::framework::TemplateAccess::InstallCompiledInteractions(*this, __cuiInteractionProgram, std::span<const BindingValue>{ __cuiInteraction_values }, std::span<Control* const>{ __cuiInteractionTargets }, &interactionError))
				return fail(L"ControlTemplate 声明交互安装失败：" + interactionError);
		}
		if (!cui::framework::TemplateAccess::GetTemplateRoot(*this))
			return fail(L"ComponentDefinition 未生成模板根。");
		if (outError) outError->clear();
		return true;
	}
	catch (...)
	{
		return fail(L"ComponentDefinition 模板初始化发生异常。");
	}
}

bool DemoWindowGeneratedFeatureCard::SetContent(std::unique_ptr<Control> value)
{
	if (!value || !_presenter_Content || _content_Content) return false;
	auto* attached = cui::framework::TreeAccess::AddOwnedVisualChild(
		*_presenter_Content, std::move(value), this);
	if (!attached) return false;
	_content_Content = attached;
	return true;
}

bool DemoWindowGeneratedFeatureCard::AddActions(std::unique_ptr<Control> value)
{
	if (!value || !_presenter_Actions) return false;
	auto* attached = cui::framework::TreeAccess::AddOwnedVisualChild(
		*_presenter_Actions, std::move(value), this);
	if (!attached) return false;
	return true;
}

DemoWindowGenerated::DemoWindowGenerated()
	: Window()
{
}

void DemoWindowGenerated::InitializeComponent()
{

	if (_componentInitialized) return;
	_componentInitialized = true;

	// Native constructors are behavior-host implementation details.
	// Begin from the same empty Local-value surface as dynamic XAML.
	(void)this->ClearPropertyValues();

	// WPF StaticResource: one value per document ResourceDictionary instance.
	const auto __documentStaticResource_Accent_2 = BindingValue(D2D1_COLOR_F{0.184314f, 0.435294f, 0.894118f, 1.f});
	const auto __documentStaticResource_SurfaceSoft_4 = BindingValue(D2D1_COLOR_F{0.968627f, 0.976471f, 0.988235f, 1.f});
	const auto __documentStaticResource_Border_5 = BindingValue(D2D1_COLOR_F{0.827451f, 0.862745f, 0.909804f, 1.f});
	const auto __documentStaticResource_ContainerSurface_6 = BindingValue(D2D1_COLOR_F{0.94902f, 0.964706f, 0.984314f, 1.f});
	const auto __documentStaticResource_ContainerBorder_7 = BindingValue(D2D1_COLOR_F{0.623529f, 0.698039f, 0.792157f, 1.f});
	const auto __documentStaticResource_TextMuted_9 = BindingValue(D2D1_COLOR_F{0.454902f, 0.513726f, 0.6f, 1.f});
	const auto __documentStaticResource_OnAccent_10 = BindingValue(D2D1_COLOR_F{1.f, 1.f, 1.f, 1.f});
	const auto __documentStaticResource_Success_11 = BindingValue(D2D1_COLOR_F{0.082353f, 0.588235f, 0.415686f, 1.f});
	const auto __documentStaticResource_DemoImage_12 = BindingValue(cui::resources::LoadBitmapResource(L"Assets/nav-overview.svg"));

	// ItemsPanelTemplate resources are immutable native layout descriptors; no XAML factory is retained.
	auto __itemsPanel_MainToolBarItemsPanel_1 = std::make_shared<ItemsPanelTemplate>();
	__itemsPanel_MainToolBarItemsPanel_1->Kind = ItemsPanelKind::Stack;
	__itemsPanel_MainToolBarItemsPanel_1->Orientation = Orientation::Horizontal;
	__itemsPanel_MainToolBarItemsPanel_1->ItemWidth = 0.f;
	__itemsPanel_MainToolBarItemsPanel_1->ItemHeight = 0.f;
	__itemsPanel_MainToolBarItemsPanel_1->CacheLength = 1.f;
	auto __itemsPanel_DemoTaskItemsPanel_2 = std::make_shared<ItemsPanelTemplate>();
	__itemsPanel_DemoTaskItemsPanel_2->Kind = ItemsPanelKind::VirtualizingStack;
	__itemsPanel_DemoTaskItemsPanel_2->Orientation = Orientation::Vertical;
	__itemsPanel_DemoTaskItemsPanel_2->ItemWidth = 0.f;
	__itemsPanel_DemoTaskItemsPanel_2->ItemHeight = 36.f;
	__itemsPanel_DemoTaskItemsPanel_2->CacheLength = 1.f;
	auto __itemsPanel_WpfLabItemsPanel_3 = std::make_shared<ItemsPanelTemplate>();
	__itemsPanel_WpfLabItemsPanel_3->Kind = ItemsPanelKind::VirtualizingStack;
	__itemsPanel_WpfLabItemsPanel_3->Orientation = Orientation::Vertical;
	__itemsPanel_WpfLabItemsPanel_3->ItemWidth = 0.f;
	__itemsPanel_WpfLabItemsPanel_3->ItemHeight = 30.f;
	__itemsPanel_WpfLabItemsPanel_3->CacheLength = 0.5f;

	// Embedded DataList resources lowered to native CUI binding objects; no runtime XAML schema is retained.
	// Production uses immutable lists and generated typed records; ObservableObject discovery is not linked.
	std::vector<BindingSourceReference> __compiledDataItems_1;
	__compiledDataItems_1.reserve(3);
	{
		auto __compiledDataRecord_1_1_1 = std::make_shared<CuiGeneratedDataRecord_DemoChoice_1>(
			L"AOT XAML",
			1
		);
		__compiledDataItems_1.emplace_back(__compiledDataRecord_1_1_1);
	}
	{
		auto __compiledDataRecord_1_2_1 = std::make_shared<CuiGeneratedDataRecord_DemoChoice_1>(
			L"声明式数据资源",
			2
		);
		__compiledDataItems_1.emplace_back(__compiledDataRecord_1_2_1);
	}
	{
		auto __compiledDataRecord_1_3_1 = std::make_shared<CuiGeneratedDataRecord_DemoChoice_1>(
			L"NativeSurface 扩展",
			3
		);
		__compiledDataItems_1.emplace_back(__compiledDataRecord_1_3_1);
	}
	auto __dataList_BasicChoices_1 = std::make_shared<CompiledBindingList>(std::move(__compiledDataItems_1), DataTypeToken{ 6105125528802023085ULL });
	std::vector<BindingSourceReference> __compiledDataItems_2;
	__compiledDataItems_2.reserve(2);
	{
		auto __compiledDataRecord_2_1_1 = std::make_shared<CuiGeneratedDataRecord_DemoStatusEntry_2>(
			L"XAML ready"
		);
		__compiledDataItems_2.emplace_back(__compiledDataRecord_2_1_1);
	}
	{
		auto __compiledDataRecord_2_2_1 = std::make_shared<CuiGeneratedDataRecord_DemoStatusEntry_2>(
			L"DemoWindow.cui.xaml"
		);
		__compiledDataItems_2.emplace_back(__compiledDataRecord_2_2_1);
	}
	auto __dataList_DemoStatusEntries_2 = std::make_shared<CompiledBindingList>(std::move(__compiledDataItems_2), DataTypeToken{ 4340723975401176726ULL });
	std::vector<BindingSourceReference> __compiledDataItems_3;
	__compiledDataItems_3.reserve(4);
	{
		auto __compiledDataRecord_3_1_1 = std::make_shared<CuiGeneratedDataRecord_DemoTask_3>(
			L"全部任务",
			L"概览",
			12,
			false,
			1
		);
		__compiledDataItems_3.emplace_back(__compiledDataRecord_3_1_1);
	}
	{
		auto __compiledDataRecord_3_2_1 = std::make_shared<CuiGeneratedDataRecord_DemoTask_3>(
			L"今天",
			L"待处理",
			4,
			true,
			2
		);
		__compiledDataItems_3.emplace_back(__compiledDataRecord_3_2_1);
	}
	{
		auto __compiledDataRecord_3_3_1 = std::make_shared<CuiGeneratedDataRecord_DemoTask_3>(
			L"进行中",
			L"待处理",
			6,
			true,
			3
		);
		__compiledDataItems_3.emplace_back(__compiledDataRecord_3_3_1);
	}
	{
		auto __compiledDataRecord_3_4_1 = std::make_shared<CuiGeneratedDataRecord_DemoTask_3>(
			L"已完成",
			L"归档",
			2,
			true,
			4
		);
		__compiledDataItems_3.emplace_back(__compiledDataRecord_3_4_1);
	}
	auto __dataList_DemoTasks_3 = std::make_shared<CompiledBindingList>(std::move(__compiledDataItems_3), DataTypeToken{ 14645570757897767179ULL });
	std::vector<BindingSourceReference> __compiledDataItems_4;
	__compiledDataItems_4.reserve(4);
	{
		auto __compiledDataRecord_4_1_1 = std::make_shared<CuiGeneratedDataRecord_AnalyticsRow_4>(
			L"312.4",
			L"已成交",
			L"华东",
			L"上海云舟",
			L"31%"
		);
		__compiledDataItems_4.emplace_back(__compiledDataRecord_4_1_1);
	}
	{
		auto __compiledDataRecord_4_2_1 = std::make_shared<CuiGeneratedDataRecord_AnalyticsRow_4>(
			L"228.6",
			L"合同中",
			L"华东",
			L"杭州数擎",
			L"28%"
		);
		__compiledDataItems_4.emplace_back(__compiledDataRecord_4_2_1);
	}
	{
		auto __compiledDataRecord_4_3_1 = std::make_shared<CuiGeneratedDataRecord_AnalyticsRow_4>(
			L"276.8",
			L"已成交",
			L"华南",
			L"深圳星河",
			L"29%"
		);
		__compiledDataItems_4.emplace_back(__compiledDataRecord_4_3_1);
	}
	{
		auto __compiledDataRecord_4_4_1 = std::make_shared<CuiGeneratedDataRecord_AnalyticsRow_4>(
			L"162.5",
			L"跟进中",
			L"华南",
			L"广州远航",
			L"25%"
		);
		__compiledDataItems_4.emplace_back(__compiledDataRecord_4_4_1);
	}
	auto __dataList_AnalyticsRows_4 = std::make_shared<CompiledBindingList>(std::move(__compiledDataItems_4), DataTypeToken{ 14210100046756945817ULL });

	// CollectionViewSource is configured entirely from native CUI descriptors before its source is attached.
	auto __collectionView_ActiveDemoTasks_1 = std::make_shared<CollectionViewSource>();
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_1[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 2922971338924869525ULL }, 0u },
	};
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_2[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Int, BindingSourcePropertyToken{ 5396721815081960082ULL }, 0u },
	};
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_3[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Int, BindingSourcePropertyToken{ 5396721815081960082ULL }, 0u },
	};
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_4[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Bool, BindingSourcePropertyToken{ 12417925837218019833ULL }, 0u },
	};
	__collectionView_ActiveDemoTasks_1->SetGroupDescriptions({
		CollectionGroupDescription::FromCompiledPath(CompiledBindingPathView{ __cuiCompiledBindingPath_1 }, CollectionSortDirection::Ascending, true),
	});
	__collectionView_ActiveDemoTasks_1->SetAggregateDescriptions({
		CollectionAggregateDescription::FromCompiledPath(L"TotalState", CompiledBindingPathView{ __cuiCompiledBindingPath_2 }, CollectionAggregateFunction::Sum),
	});
	__collectionView_ActiveDemoTasks_1->SetSortDescriptions({
		CollectionSortDescription::FromCompiledPath(CompiledBindingPathView{ __cuiCompiledBindingPath_3 }, CollectionSortDirection::Descending, true),
	});
	__collectionView_ActiveDemoTasks_1->SetFilterDescriptions({
		CollectionFilterDescription::FromCompiledPath(CompiledBindingPathView{ __cuiCompiledBindingPath_4 }, CollectionFilterOperator::Equals, BindingValue(true), true),
	});
	__collectionView_ActiveDemoTasks_1->SetSource(BindingListReference(__dataList_DemoTasks_3));
	if (__collectionView_ActiveDemoTasks_1->GetSource().Get() != __dataList_DemoTasks_3.get())
		throw std::runtime_error("Generated CollectionViewSource installation failed");

	// DataTemplate resources are repeatable native visual factories. They retain only typed CUI callbacks and values.
	auto __dataTemplate_DemoTaskRow_1 = std::make_shared<CuiGeneratedItemTemplate>(
		DataTypeToken{ 14645570757897767179ULL }, false,
		[__documentStaticResource_Accent_2, __documentStaticResource_SurfaceSoft_4, __documentStaticResource_Border_5, __documentStaticResource_ContainerSurface_6, __documentStaticResource_ContainerBorder_7, __documentStaticResource_TextMuted_9, __documentStaticResource_OnAccent_10, __documentStaticResource_Success_11, __documentStaticResource_DemoImage_12](const BindingSourceReference& item, size_t index, std::wstring* outError) -> std::unique_ptr<Control>
		{
			(void)index;
			auto fail = [outError](std::wstring message) -> std::unique_ptr<Control>
			{
				if (outError) *outError = std::move(message);
				return {};
			};
			if (!item)
				return fail(L"DataTemplate 缺少当前数据项。");
			try
			{
				// stackPanel1
				auto __owned_stackPanel1 = std::make_unique<StackPanel>();
				auto* stackPanel1 = __owned_stackPanel1.get();
				(void)stackPanel1->ClearPropertyValues();
				// textBlock1
				auto __owned_textBlock1 = std::make_unique<Label>();
				auto* textBlock1 = __owned_textBlock1.get();
				(void)textBlock1->ClearPropertyValues();
				// textBlock2
				auto __owned_textBlock2 = std::make_unique<Label>();
				auto* textBlock2 = __owned_textBlock2.get();
				(void)textBlock2->ClearPropertyValues();
				// XAML authored Local properties/resources
				stackPanel1->SetOrientation(static_cast<Orientation>(0));
				stackPanel1->SetHeight(cui::layout::Length::Fixed(32.f));
				// XAML authored Local properties/resources
				textBlock1->SetWidth(cui::layout::Length::Fixed(138.f));
				textBlock1->SetHeight(cui::layout::Length::Fixed(24.f));
				// XAML authored Local properties/resources
				textBlock2->SetWidth(cui::layout::Length::Fixed(36.f));
				textBlock2->SetHeight(cui::layout::Length::Fixed(24.f));
				textBlock2->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));
				stackPanel1->AddOwned(std::move(__owned_textBlock1));
				stackPanel1->AddOwned(std::move(__owned_textBlock2));
				if (!stackPanel1->SetDataContext(item))
					return fail(L"DataTemplate DataContext 安装失败。");
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_5[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 1869304552508798568ULL }, 0u },
					};
					// CUI:AOT binding-source=direct-record
					const bool attached = textBlock1->DataBindings.Add(Label::TextProperty(), cui::binding::MakeCompiledRecordPropertySource<CuiGeneratedDataRecord_DemoTask_3>(*item.Get(), 0, __cuiCompiledBindingPath_5[0]), BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_6[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Int, BindingSourcePropertyToken{ 5396721815081960082ULL }, 0u },
					};
					// CUI:AOT binding-source=direct-record
					const bool attached = textBlock2->DataBindings.Add(Label::TextProperty(), cui::binding::MakeCompiledRecordPropertySource<CuiGeneratedDataRecord_DemoTask_3>(*item.Get(), 2, __cuiCompiledBindingPath_6[0]), BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				if (outError) outError->clear();
				return std::move(__owned_stackPanel1);
			}
			catch (const std::exception& error)
			{
				return fail(L"DataTemplate 静态构造发生运行时异常：" + Convert::StringToWString(error.what()));
			}
			catch (...)
			{
				return fail(L"DataTemplate 静态构造发生未知异常。");
			}
		});
	auto __dataTemplate_AnalyticsRowTemplate_2 = std::make_shared<CuiGeneratedItemTemplate>(
		DataTypeToken{ 14210100046756945817ULL }, false,
		[__documentStaticResource_Accent_2, __documentStaticResource_SurfaceSoft_4, __documentStaticResource_Border_5, __documentStaticResource_ContainerSurface_6, __documentStaticResource_ContainerBorder_7, __documentStaticResource_TextMuted_9, __documentStaticResource_OnAccent_10, __documentStaticResource_Success_11, __documentStaticResource_DemoImage_12](const BindingSourceReference& item, size_t index, std::wstring* outError) -> std::unique_ptr<Control>
		{
			(void)index;
			auto fail = [outError](std::wstring message) -> std::unique_ptr<Control>
			{
				if (outError) *outError = std::move(message);
				return {};
			};
			if (!item)
				return fail(L"DataTemplate 缺少当前数据项。");
			try
			{
				// stackPanel1
				auto __owned_stackPanel1 = std::make_unique<StackPanel>();
				auto* stackPanel1 = __owned_stackPanel1.get();
				(void)stackPanel1->ClearPropertyValues();
				// textBlock1
				auto __owned_textBlock1 = std::make_unique<Label>();
				auto* textBlock1 = __owned_textBlock1.get();
				(void)textBlock1->ClearPropertyValues();
				// textBlock2
				auto __owned_textBlock2 = std::make_unique<Label>();
				auto* textBlock2 = __owned_textBlock2.get();
				(void)textBlock2->ClearPropertyValues();
				// textBlock3
				auto __owned_textBlock3 = std::make_unique<Label>();
				auto* textBlock3 = __owned_textBlock3.get();
				(void)textBlock3->ClearPropertyValues();
				// textBlock4
				auto __owned_textBlock4 = std::make_unique<Label>();
				auto* textBlock4 = __owned_textBlock4.get();
				(void)textBlock4->ClearPropertyValues();
				// textBlock5
				auto __owned_textBlock5 = std::make_unique<Label>();
				auto* textBlock5 = __owned_textBlock5.get();
				(void)textBlock5->ClearPropertyValues();
				// XAML authored Local properties/resources
				stackPanel1->SetOrientation(static_cast<Orientation>(0));
				stackPanel1->SetHeight(cui::layout::Length::Fixed(34.f));
				// XAML authored Local properties/resources
				textBlock1->SetWidth(cui::layout::Length::Fixed(138.f));
				textBlock1->SetHeight(cui::layout::Length::Fixed(24.f));
				// XAML authored Local properties/resources
				textBlock2->SetWidth(cui::layout::Length::Fixed(62.f));
				textBlock2->SetHeight(cui::layout::Length::Fixed(24.f));
				// XAML authored Local properties/resources
				textBlock3->SetWidth(cui::layout::Length::Fixed(76.f));
				textBlock3->SetHeight(cui::layout::Length::Fixed(24.f));
				// XAML authored Local properties/resources
				textBlock4->SetWidth(cui::layout::Length::Fixed(86.f));
				textBlock4->SetHeight(cui::layout::Length::Fixed(24.f));
				// XAML authored Local properties/resources
				textBlock5->SetWidth(cui::layout::Length::Fixed(56.f));
				textBlock5->SetHeight(cui::layout::Length::Fixed(24.f));
				textBlock5->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Success_11));
				stackPanel1->AddOwned(std::move(__owned_textBlock1));
				stackPanel1->AddOwned(std::move(__owned_textBlock2));
				stackPanel1->AddOwned(std::move(__owned_textBlock3));
				stackPanel1->AddOwned(std::move(__owned_textBlock4));
				stackPanel1->AddOwned(std::move(__owned_textBlock5));
				if (!stackPanel1->SetDataContext(item))
					return fail(L"DataTemplate DataContext 安装失败。");
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_7[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 12717390152918474433ULL }, 0u },
					};
					// CUI:AOT binding-source=direct-record
					const bool attached = textBlock1->DataBindings.Add(Label::TextProperty(), cui::binding::MakeCompiledRecordPropertySource<CuiGeneratedDataRecord_AnalyticsRow_4>(*item.Get(), 3, __cuiCompiledBindingPath_7[0]), BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_8[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 2961787330939152573ULL }, 0u },
					};
					// CUI:AOT binding-source=direct-record
					const bool attached = textBlock2->DataBindings.Add(Label::TextProperty(), cui::binding::MakeCompiledRecordPropertySource<CuiGeneratedDataRecord_AnalyticsRow_4>(*item.Get(), 2, __cuiCompiledBindingPath_8[0]), BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_9[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 2419962606964216481ULL }, 0u },
					};
					// CUI:AOT binding-source=direct-record
					const bool attached = textBlock3->DataBindings.Add(Label::TextProperty(), cui::binding::MakeCompiledRecordPropertySource<CuiGeneratedDataRecord_AnalyticsRow_4>(*item.Get(), 1, __cuiCompiledBindingPath_9[0]), BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_10[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 170753274995538377ULL }, 0u },
					};
					// CUI:AOT binding-source=direct-record
					const bool attached = textBlock4->DataBindings.Add(Label::TextProperty(), cui::binding::MakeCompiledRecordPropertySource<CuiGeneratedDataRecord_AnalyticsRow_4>(*item.Get(), 0, __cuiCompiledBindingPath_10[0]), BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_11[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 12881601721173548923ULL }, 0u },
					};
					// CUI:AOT binding-source=direct-record
					const bool attached = textBlock5->DataBindings.Add(Label::TextProperty(), cui::binding::MakeCompiledRecordPropertySource<CuiGeneratedDataRecord_AnalyticsRow_4>(*item.Get(), 4, __cuiCompiledBindingPath_11[0]), BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				if (outError) outError->clear();
				return std::move(__owned_stackPanel1);
			}
			catch (const std::exception& error)
			{
				return fail(L"DataTemplate 静态构造发生运行时异常：" + Convert::StringToWString(error.what()));
			}
			catch (...)
			{
				return fail(L"DataTemplate 静态构造发生未知异常。");
			}
		});
	auto __dataTemplate_DemoListViewRow_3 = std::make_shared<CuiGeneratedItemTemplate>(
		DataTypeToken{ 1527176720216676224ULL }, false,
		[__documentStaticResource_Accent_2, __documentStaticResource_SurfaceSoft_4, __documentStaticResource_Border_5, __documentStaticResource_ContainerSurface_6, __documentStaticResource_ContainerBorder_7, __documentStaticResource_TextMuted_9, __documentStaticResource_OnAccent_10, __documentStaticResource_Success_11, __documentStaticResource_DemoImage_12](const BindingSourceReference& item, size_t index, std::wstring* outError) -> std::unique_ptr<Control>
		{
			(void)index;
			auto fail = [outError](std::wstring message) -> std::unique_ptr<Control>
			{
				if (outError) *outError = std::move(message);
				return {};
			};
			if (!item)
				return fail(L"DataTemplate 缺少当前数据项。");
			try
			{
				// stackPanel1
				auto __owned_stackPanel1 = std::make_unique<StackPanel>();
				auto* stackPanel1 = __owned_stackPanel1.get();
				(void)stackPanel1->ClearPropertyValues();
				// textBlock1
				auto __owned_textBlock1 = std::make_unique<Label>();
				auto* textBlock1 = __owned_textBlock1.get();
				(void)textBlock1->ClearPropertyValues();
				// textBlock2
				auto __owned_textBlock2 = std::make_unique<Label>();
				auto* textBlock2 = __owned_textBlock2.get();
				(void)textBlock2->ClearPropertyValues();
				// XAML authored Local properties/resources
				stackPanel1->SetOrientation(static_cast<Orientation>(0));
				stackPanel1->SetHeight(cui::layout::Length::Fixed(34.f));
				// XAML authored Local properties/resources
				textBlock1->SetWidth(cui::layout::Length::Fixed(238.f));
				textBlock1->SetHeight(cui::layout::Length::Fixed(24.f));
				// XAML authored Local properties/resources
				textBlock2->SetWidth(cui::layout::Length::Fixed(110.f));
				textBlock2->SetHeight(cui::layout::Length::Fixed(24.f));
				textBlock2->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));
				stackPanel1->AddOwned(std::move(__owned_textBlock1));
				stackPanel1->AddOwned(std::move(__owned_textBlock2));
				if (!stackPanel1->SetDataContext(item))
					return fail(L"DataTemplate DataContext 安装失败。");
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_12[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 5250172348753352866ULL }, 0u },
					};
					const bool attached = textBlock1->DataBindings.Add(Label::TextProperty(), textBlock1->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_12 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_13[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 5396721815081960082ULL }, 0u },
					};
					const bool attached = textBlock2->DataBindings.Add(Label::TextProperty(), textBlock2->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_13 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				if (outError) outError->clear();
				return std::move(__owned_stackPanel1);
			}
			catch (const std::exception& error)
			{
				return fail(L"DataTemplate 静态构造发生运行时异常：" + Convert::StringToWString(error.what()));
			}
			catch (...)
			{
				return fail(L"DataTemplate 静态构造发生未知异常。");
			}
		});
	auto __dataTemplate_DemoTaskGroupHeader_4 = std::make_shared<CuiGeneratedItemTemplate>(
		DataTypeToken{ 3525408356366179569ULL }, false,
		[__documentStaticResource_Accent_2, __documentStaticResource_SurfaceSoft_4, __documentStaticResource_Border_5, __documentStaticResource_ContainerSurface_6, __documentStaticResource_ContainerBorder_7, __documentStaticResource_TextMuted_9, __documentStaticResource_OnAccent_10, __documentStaticResource_Success_11, __documentStaticResource_DemoImage_12](const BindingSourceReference& item, size_t index, std::wstring* outError) -> std::unique_ptr<Control>
		{
			(void)index;
			auto fail = [outError](std::wstring message) -> std::unique_ptr<Control>
			{
				if (outError) *outError = std::move(message);
				return {};
			};
			if (!item)
				return fail(L"DataTemplate 缺少当前数据项。");
			try
			{
				// stackPanel1
				auto __owned_stackPanel1 = std::make_unique<StackPanel>();
				auto* stackPanel1 = __owned_stackPanel1.get();
				(void)stackPanel1->ClearPropertyValues();
				// textBlock1
				auto __owned_textBlock1 = std::make_unique<Label>();
				auto* textBlock1 = __owned_textBlock1.get();
				(void)textBlock1->ClearPropertyValues();
				// textBlock2
				auto __owned_textBlock2 = std::make_unique<Label>();
				auto* textBlock2 = __owned_textBlock2.get();
				(void)textBlock2->ClearPropertyValues();
				// textBlock3
				auto __owned_textBlock3 = std::make_unique<Label>();
				auto* textBlock3 = __owned_textBlock3.get();
				(void)textBlock3->ClearPropertyValues();
				// XAML authored Local properties/resources
				stackPanel1->SetOrientation(static_cast<Orientation>(0));
				stackPanel1->SetHeight(cui::layout::Length::Fixed(24.f));
				// XAML authored Local properties/resources
				textBlock1->SetHeight(cui::layout::Length::Fixed(24.f));
				textBlock1->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));
				textBlock1->SetFontSize(13.0);
				// XAML authored Local properties/resources
				textBlock2->SetHeight(cui::layout::Length::Fixed(24.f));
				textBlock2->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));
				textBlock2->SetFontSize(12.0);
				// XAML authored Local properties/resources
				textBlock3->SetHeight(cui::layout::Length::Fixed(24.f));
				textBlock3->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));
				textBlock3->SetFontSize(12.0);
				stackPanel1->AddOwned(std::move(__owned_textBlock1));
				stackPanel1->AddOwned(std::move(__owned_textBlock2));
				stackPanel1->AddOwned(std::move(__owned_textBlock3));
				if (!stackPanel1->SetDataContext(item))
					return fail(L"DataTemplate DataContext 安装失败。");
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_14[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Empty, BindingSourcePropertyToken{ 7606216366218047554ULL }, 0u },
					};
					const bool attached = textBlock1->DataBindings.Add(Label::TextProperty(), textBlock1->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_14 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_15[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Empty, BindingSourcePropertyToken{ 14003265217196705219ULL }, 0u },
					};
					const bool attached = textBlock2->DataBindings.Add(Label::TextProperty(), textBlock2->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_15 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_16[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Empty, BindingSourcePropertyToken{ 17789538396786896295ULL }, 0u },
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Empty, BindingSourcePropertyToken{ 218976443840471840ULL }, 0u },
					};
					const bool attached = textBlock3->DataBindings.Add(Label::TextProperty(), textBlock3->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_16 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				if (outError) outError->clear();
				return std::move(__owned_stackPanel1);
			}
			catch (const std::exception& error)
			{
				return fail(L"DataTemplate 静态构造发生运行时异常：" + Convert::StringToWString(error.what()));
			}
			catch (...)
			{
				return fail(L"DataTemplate 静态构造发生未知异常。");
			}
		});
	auto __dataTemplate_Implicit_DemoFolder_5 = std::make_shared<CuiGeneratedItemTemplate>(
		DataTypeToken{ 9713005176006114176ULL }, true,
		[__documentStaticResource_Accent_2, __documentStaticResource_SurfaceSoft_4, __documentStaticResource_Border_5, __documentStaticResource_ContainerSurface_6, __documentStaticResource_ContainerBorder_7, __documentStaticResource_TextMuted_9, __documentStaticResource_OnAccent_10, __documentStaticResource_Success_11, __documentStaticResource_DemoImage_12](const BindingSourceReference& item, size_t index, std::wstring* outError) -> std::unique_ptr<Control>
		{
			(void)index;
			auto fail = [outError](std::wstring message) -> std::unique_ptr<Control>
			{
				if (outError) *outError = std::move(message);
				return {};
			};
			if (!item)
				return fail(L"DataTemplate 缺少当前数据项。");
			try
			{
				// stackPanel1
				auto __owned_stackPanel1 = std::make_unique<StackPanel>();
				auto* stackPanel1 = __owned_stackPanel1.get();
				(void)stackPanel1->ClearPropertyValues();
				// textBlock1
				auto __owned_textBlock1 = std::make_unique<Label>();
				auto* textBlock1 = __owned_textBlock1.get();
				(void)textBlock1->ClearPropertyValues();
				// textBlock2
				auto __owned_textBlock2 = std::make_unique<Label>();
				auto* textBlock2 = __owned_textBlock2.get();
				(void)textBlock2->ClearPropertyValues();
				// XAML authored Local properties/resources
				stackPanel1->SetOrientation(static_cast<Orientation>(0));
				stackPanel1->SetVerticalAlignment(static_cast<::VerticalAlignment>(1));
				// XAML authored Local properties/resources
				textBlock1->SetText(L"▣");
				textBlock1->SetWidth(cui::layout::Length::Fixed(22.f));
				textBlock1->SetVerticalAlignment(static_cast<::VerticalAlignment>(1));
				textBlock1->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));
				// XAML authored Local properties/resources
				textBlock2->SetVerticalAlignment(static_cast<::VerticalAlignment>(1));
				stackPanel1->AddOwned(std::move(__owned_textBlock1));
				stackPanel1->AddOwned(std::move(__owned_textBlock2));
				if (!stackPanel1->SetDataContext(item))
					return fail(L"DataTemplate DataContext 安装失败。");
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_17[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 5250172348753352866ULL }, 0u },
					};
					const bool attached = textBlock2->DataBindings.Add(Label::TextProperty(), textBlock2->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_17 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				if (outError) outError->clear();
				return std::move(__owned_stackPanel1);
			}
			catch (const std::exception& error)
			{
				return fail(L"DataTemplate 静态构造发生运行时异常：" + Convert::StringToWString(error.what()));
			}
			catch (...)
			{
				return fail(L"DataTemplate 静态构造发生未知异常。");
			}
		},
		[](const BindingSourceReference& item, BindingListReference& out, std::wstring* outError)
		{
			out = {};
			if (!item)
			{
				if (outError) *outError = L"HierarchicalDataTemplate 缺少当前数据项。";
				return false;
			}
			static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_18[] =
			{
				{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Object, BindingSourcePropertyToken{ 11751380407599190198ULL }, 0u },
			};
			BindingValue value;
			if (!TryGetBindingPathValue(*item.Get(), CompiledBindingPathView{ __cuiCompiledBindingPath_18 }, value))
			{
				if (outError) *outError = L"HierarchicalDataTemplate.ItemsSource 无法读取路径。";
				return false;
			}
			if (!value.Empty() && !value.TryGet(out))
			{
				if (outError) *outError = L"HierarchicalDataTemplate.ItemsSource 未返回 BindingList。";
				return false;
			}
			if (outError) outError->clear();
			return true;
		},
		[](const BindingSourceReference& item, std::function<void()> changed)
		{
			static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_19[] =
			{
				{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Object, BindingSourcePropertyToken{ 11751380407599190198ULL }, 0u },
			};
			return ObserveBindingPaths(item, { CompiledBindingPathView{ __cuiCompiledBindingPath_19 } }, std::move(changed));
		});
	auto __dataTemplate_Implicit_DemoFile_6 = std::make_shared<CuiGeneratedItemTemplate>(
		DataTypeToken{ 16771066560557046080ULL }, false,
		[__documentStaticResource_Accent_2, __documentStaticResource_SurfaceSoft_4, __documentStaticResource_Border_5, __documentStaticResource_ContainerSurface_6, __documentStaticResource_ContainerBorder_7, __documentStaticResource_TextMuted_9, __documentStaticResource_OnAccent_10, __documentStaticResource_Success_11, __documentStaticResource_DemoImage_12](const BindingSourceReference& item, size_t index, std::wstring* outError) -> std::unique_ptr<Control>
		{
			(void)index;
			auto fail = [outError](std::wstring message) -> std::unique_ptr<Control>
			{
				if (outError) *outError = std::move(message);
				return {};
			};
			if (!item)
				return fail(L"DataTemplate 缺少当前数据项。");
			try
			{
				// stackPanel1
				auto __owned_stackPanel1 = std::make_unique<StackPanel>();
				auto* stackPanel1 = __owned_stackPanel1.get();
				(void)stackPanel1->ClearPropertyValues();
				// textBlock1
				auto __owned_textBlock1 = std::make_unique<Label>();
				auto* textBlock1 = __owned_textBlock1.get();
				(void)textBlock1->ClearPropertyValues();
				// textBlock2
				auto __owned_textBlock2 = std::make_unique<Label>();
				auto* textBlock2 = __owned_textBlock2.get();
				(void)textBlock2->ClearPropertyValues();
				// textBlock3
				auto __owned_textBlock3 = std::make_unique<Label>();
				auto* textBlock3 = __owned_textBlock3.get();
				(void)textBlock3->ClearPropertyValues();
				// XAML authored Local properties/resources
				stackPanel1->SetOrientation(static_cast<Orientation>(0));
				stackPanel1->SetVerticalAlignment(static_cast<::VerticalAlignment>(1));
				// XAML authored Local properties/resources
				textBlock1->SetText(L"•");
				textBlock1->SetWidth(cui::layout::Length::Fixed(22.f));
				textBlock1->SetVerticalAlignment(static_cast<::VerticalAlignment>(1));
				textBlock1->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));
				// XAML authored Local properties/resources
				textBlock2->SetWidth(cui::layout::Length::Fixed(108.f));
				textBlock2->SetVerticalAlignment(static_cast<::VerticalAlignment>(1));
				// XAML authored Local properties/resources
				textBlock3->SetWidth(cui::layout::Length::Fixed(48.f));
				textBlock3->SetVerticalAlignment(static_cast<::VerticalAlignment>(1));
				textBlock3->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));
				stackPanel1->AddOwned(std::move(__owned_textBlock1));
				stackPanel1->AddOwned(std::move(__owned_textBlock2));
				stackPanel1->AddOwned(std::move(__owned_textBlock3));
				if (!stackPanel1->SetDataContext(item))
					return fail(L"DataTemplate DataContext 安装失败。");
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_20[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 5250172348753352866ULL }, 0u },
					};
					const bool attached = textBlock2->DataBindings.Add(Label::TextProperty(), textBlock2->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_20 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_21[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 808023471034512701ULL }, 0u },
					};
					const bool attached = textBlock3->DataBindings.Add(Label::TextProperty(), textBlock3->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_21 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				if (outError) outError->clear();
				return std::move(__owned_stackPanel1);
			}
			catch (const std::exception& error)
			{
				return fail(L"DataTemplate 静态构造发生运行时异常：" + Convert::StringToWString(error.what()));
			}
			catch (...)
			{
				return fail(L"DataTemplate 静态构造发生未知异常。");
			}
		});
	auto __dataTemplate_WpfLabPersonRow_7 = std::make_shared<CuiGeneratedItemTemplate>(
		DataTypeToken{ 17405733524517076158ULL }, false,
		[__documentStaticResource_Accent_2, __documentStaticResource_SurfaceSoft_4, __documentStaticResource_Border_5, __documentStaticResource_ContainerSurface_6, __documentStaticResource_ContainerBorder_7, __documentStaticResource_TextMuted_9, __documentStaticResource_OnAccent_10, __documentStaticResource_Success_11, __documentStaticResource_DemoImage_12](const BindingSourceReference& item, size_t index, std::wstring* outError) -> std::unique_ptr<Control>
		{
			(void)index;
			auto fail = [outError](std::wstring message) -> std::unique_ptr<Control>
			{
				if (outError) *outError = std::move(message);
				return {};
			};
			if (!item)
				return fail(L"DataTemplate 缺少当前数据项。");
			try
			{
				// stackPanel1
				auto __owned_stackPanel1 = std::make_unique<StackPanel>();
				auto* stackPanel1 = __owned_stackPanel1.get();
				(void)stackPanel1->ClearPropertyValues();
				// textBlock1
				auto __owned_textBlock1 = std::make_unique<Label>();
				auto* textBlock1 = __owned_textBlock1.get();
				(void)textBlock1->ClearPropertyValues();
				// textBlock2
				auto __owned_textBlock2 = std::make_unique<Label>();
				auto* textBlock2 = __owned_textBlock2.get();
				(void)textBlock2->ClearPropertyValues();
				// textBlock3
				auto __owned_textBlock3 = std::make_unique<Label>();
				auto* textBlock3 = __owned_textBlock3.get();
				(void)textBlock3->ClearPropertyValues();
				// XAML authored Local properties/resources
				stackPanel1->SetOrientation(static_cast<Orientation>(0));
				stackPanel1->SetHeight(cui::layout::Length::Fixed(26.f));
				// XAML authored Local properties/resources
				textBlock1->SetWidth(cui::layout::Length::Fixed(70.f));
				textBlock1->SetHeight(cui::layout::Length::Fixed(24.f));
				// XAML authored Local properties/resources
				textBlock2->SetWidth(cui::layout::Length::Fixed(82.f));
				textBlock2->SetHeight(cui::layout::Length::Fixed(24.f));
				// XAML authored Local properties/resources
				textBlock3->SetWidth(cui::layout::Length::Fixed(66.f));
				textBlock3->SetHeight(cui::layout::Length::Fixed(24.f));
				textBlock3->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));
				stackPanel1->AddOwned(std::move(__owned_textBlock1));
				stackPanel1->AddOwned(std::move(__owned_textBlock2));
				stackPanel1->AddOwned(std::move(__owned_textBlock3));
				if (!stackPanel1->SetDataContext(item))
					return fail(L"DataTemplate DataContext 安装失败。");
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_22[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 7420362091733594415ULL }, 0u },
					};
					const bool attached = textBlock1->DataBindings.Add(Label::TextProperty(), textBlock1->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_22 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_23[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 2204516170288939743ULL }, 0u },
					};
					const bool attached = textBlock2->DataBindings.Add(Label::TextProperty(), textBlock2->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_23 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				{
					static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_24[] =
					{
						{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 1367481850725867473ULL }, 0u },
					};
					const bool attached = textBlock3->DataBindings.Add(Label::TextProperty(), textBlock3->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_24 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
					if (!attached)
						return fail(L"DataTemplate Binding 安装失败。");
				}
				if (outError) outError->clear();
				return std::move(__owned_stackPanel1);
			}
			catch (const std::exception& error)
			{
				return fail(L"DataTemplate 静态构造发生运行时异常：" + Convert::StringToWString(error.what()));
			}
			catch (...)
			{
				return fail(L"DataTemplate 静态构造发生未知异常。");
			}
		});

	// GroupStyle resources retain the already-compiled native header DataTemplate identity.
	auto __groupStyle_DemoTaskGroups_1 = std::make_shared<GroupStyle>();
	__groupStyle_DemoTaskGroups_1->HeaderTemplate = ItemTemplateReference(__dataTemplate_DemoTaskGroupHeader_4);

	// 创建控件
	// windowContent
	auto __owned_windowContent = std::make_unique<Grid>();
	windowContent = __owned_windowContent.get();
	(void)windowContent->ClearPropertyValues();
	// mainMenu
	auto __owned_mainMenu = std::make_unique<Menu>();
	mainMenu = __owned_mainMenu.get();
	(void)mainMenu->ClearPropertyValues();
	// menuItem1
	auto __owned_menuItem1 = std::make_unique<MenuItem>();
	menuItem1 = __owned_menuItem1.get();
	(void)menuItem1->ClearPropertyValues();
	// menuItem2
	auto __owned_menuItem2 = std::make_unique<MenuItem>();
	menuItem2 = __owned_menuItem2.get();
	(void)menuItem2->ClearPropertyValues();
	// separator1
	auto __owned_separator1 = std::make_unique<Separator>();
	separator1 = __owned_separator1.get();
	(void)separator1->ClearPropertyValues();
	// menuItem3
	auto __owned_menuItem3 = std::make_unique<MenuItem>();
	menuItem3 = __owned_menuItem3.get();
	(void)menuItem3->ClearPropertyValues();
	// menuItem4
	auto __owned_menuItem4 = std::make_unique<MenuItem>();
	menuItem4 = __owned_menuItem4.get();
	(void)menuItem4->ClearPropertyValues();
	// menuItem5
	auto __owned_menuItem5 = std::make_unique<MenuItem>();
	menuItem5 = __owned_menuItem5.get();
	(void)menuItem5->ClearPropertyValues();
	// mainToolBar
	auto __owned_mainToolBar = std::make_unique<ToolBar>();
	mainToolBar = __owned_mainToolBar.get();
	(void)mainToolBar->ClearPropertyValues();
	// toolBasic
	auto __owned_toolBasic = std::make_unique<Button>();
	toolBasic = __owned_toolBasic.get();
	(void)toolBasic->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*toolBasic, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// toolData
	auto __owned_toolData = std::make_unique<Button>();
	toolData = __owned_toolData.get();
	(void)toolData->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*toolData, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// toolAnalytics
	auto __owned_toolAnalytics = std::make_unique<Button>();
	toolAnalytics = __owned_toolAnalytics.get();
	(void)toolAnalytics->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*toolAnalytics, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// toolSystem
	auto __owned_toolSystem = std::make_unique<Button>();
	toolSystem = __owned_toolSystem.get();
	(void)toolSystem->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*toolSystem, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// toolSeparator
	auto __owned_toolSeparator = std::make_unique<Canvas>();
	toolSeparator = __owned_toolSeparator.get();
	(void)toolSeparator->ClearPropertyValues();
	// toolIcon1
	auto __owned_toolIcon1 = std::make_unique<Button>();
	toolIcon1 = __owned_toolIcon1.get();
	(void)toolIcon1->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*toolIcon1, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// toolIconImage1
	auto __owned_toolIconImage1 = std::make_unique<Image>();
	toolIconImage1 = __owned_toolIconImage1.get();
	(void)toolIconImage1->ClearPropertyValues();
	// toolIcon2
	auto __owned_toolIcon2 = std::make_unique<Button>();
	toolIcon2 = __owned_toolIcon2.get();
	(void)toolIcon2->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*toolIcon2, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// toolIconImage2
	auto __owned_toolIconImage2 = std::make_unique<Image>();
	toolIconImage2 = __owned_toolIconImage2.get();
	(void)toolIconImage2->ClearPropertyValues();
	// toolIcon3
	auto __owned_toolIcon3 = std::make_unique<Button>();
	toolIcon3 = __owned_toolIcon3.get();
	(void)toolIcon3->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*toolIcon3, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// toolIconImage3
	auto __owned_toolIconImage3 = std::make_unique<Image>();
	toolIconImage3 = __owned_toolIconImage3.get();
	(void)toolIconImage3->ClearPropertyValues();
	// canvas1
	auto __owned_canvas1 = std::make_unique<Canvas>();
	canvas1 = __owned_canvas1.get();
	(void)canvas1->ClearPropertyValues();
	// globalProgress
	auto __owned_globalProgress = std::make_unique<Slider>();
	globalProgress = __owned_globalProgress.get();
	(void)globalProgress->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*globalProgress, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// statusText
	auto __owned_statusText = std::make_unique<Label>();
	statusText = __owned_statusText.get();
	(void)statusText->ClearPropertyValues();
	// runtimeBadge
	auto __owned_runtimeBadge = std::make_unique<Label>();
	runtimeBadge = __owned_runtimeBadge.get();
	(void)runtimeBadge->ClearPropertyValues();
	// mainTabs
	auto __owned_mainTabs = std::make_unique<TabControl>();
	mainTabs = __owned_mainTabs.get();
	(void)mainTabs->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*mainTabs, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// tabItem1
	auto __owned_tabItem1 = std::make_unique<TabItem>();
	tabItem1 = __owned_tabItem1.get();
	(void)tabItem1->ClearPropertyValues();
	// border1
	auto __owned_border1 = std::make_unique<Border>();
	border1 = __owned_border1.get();
	(void)border1->ClearPropertyValues();
	// basicSurface
	auto __owned_basicSurface = std::make_unique<Canvas>();
	basicSurface = __owned_basicSurface.get();
	(void)basicSurface->ClearPropertyValues();
	// basicTitle
	auto __owned_basicTitle = std::make_unique<Label>();
	basicTitle = __owned_basicTitle.get();
	(void)basicTitle->ClearPropertyValues();
	// frameworkThemeHint
	auto __owned_frameworkThemeHint = std::make_unique<Label>();
	frameworkThemeHint = __owned_frameworkThemeHint.get();
	(void)frameworkThemeHint->ClearPropertyValues();
	// basicButton
	auto __owned_basicButton = std::make_unique<Button>();
	basicButton = __owned_basicButton.get();
	(void)basicButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*basicButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// enableInput
	auto __owned_enableInput = std::make_unique<CheckBox>();
	enableInput = __owned_enableInput.get();
	(void)enableInput->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*enableInput, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// radioA
	auto __owned_radioA = std::make_unique<RadioButton>();
	radioA = __owned_radioA.get();
	(void)radioA->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*radioA, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// radioB
	auto __owned_radioB = std::make_unique<RadioButton>();
	radioB = __owned_radioB.get();
	(void)radioB->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*radioB, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// nameInput
	auto __owned_nameInput = std::make_unique<TextBox>();
	nameInput = __owned_nameInput.get();
	(void)nameInput->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*nameInput, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// passwordInput
	auto __owned_passwordInput = std::make_unique<PasswordBox>();
	passwordInput = __owned_passwordInput.get();
	(void)passwordInput->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*passwordInput, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// basicCombo
	auto __owned_basicCombo = std::make_unique<ComboBox>();
	basicCombo = __owned_basicCombo.get();
	(void)basicCombo->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*basicCombo, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// dateInput
	auto __owned_dateInput = std::make_unique<TextBox>();
	dateInput = __owned_dateInput.get();
	(void)dateInput->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*dateInput, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// numberInput
	auto __owned_numberInput = std::make_unique<NumericUpDown>();
	numberInput = __owned_numberInput.get();
	(void)numberInput->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*numberInput, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// dialogCancelButton
	auto __owned_dialogCancelButton = std::make_unique<Button>();
	dialogCancelButton = __owned_dialogCancelButton.get();
	(void)dialogCancelButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*dialogCancelButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// docsLink
	auto __owned_docsLink = std::make_unique<Button>();
	docsLink = __owned_docsLink.get();
	(void)docsLink->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*docsLink, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// textBlock1
	auto __owned_textBlock1 = std::make_unique<Label>();
	textBlock1 = __owned_textBlock1.get();
	(void)textBlock1->ClearPropertyValues();
	// verticalThemeSlider
	auto __owned_verticalThemeSlider = std::make_unique<Slider>();
	verticalThemeSlider = __owned_verticalThemeSlider.get();
	(void)verticalThemeSlider->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*verticalThemeSlider, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// verticalThemeProgress
	auto __owned_verticalThemeProgress = std::make_unique<ProgressBar>();
	verticalThemeProgress = __owned_verticalThemeProgress.get();
	(void)verticalThemeProgress->ClearPropertyValues();
	// gradientInput
	auto __owned_gradientInput = std::make_unique<TextBox>();
	gradientInput = __owned_gradientInput.get();
	(void)gradientInput->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*gradientInput, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// gradientLabel
	auto __owned_gradientLabel = std::make_unique<Label>();
	gradientLabel = __owned_gradientLabel.get();
	(void)gradientLabel->ClearPropertyValues();
	// featureCard
	auto __owned_featureCard = std::make_unique<DemoWindowGeneratedFeatureCard>();
	featureCard = __owned_featureCard.get();
	(void)featureCard->ClearPropertyValues();
	// featureCardContent
	auto __owned_featureCardContent = std::make_unique<Label>();
	featureCardContent = __owned_featureCardContent.get();
	(void)featureCardContent->ClearPropertyValues();
	// featureActionA
	auto __owned_featureActionA = std::make_unique<Button>();
	featureActionA = __owned_featureActionA.get();
	(void)featureActionA->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*featureActionA, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// featureActionB
	auto __owned_featureActionB = std::make_unique<Button>();
	featureActionB = __owned_featureActionB.get();
	(void)featureActionB->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*featureActionB, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// basicGroup
	auto __owned_basicGroup = std::make_unique<GroupBox>();
	basicGroup = __owned_basicGroup.get();
	(void)basicGroup->ClearPropertyValues();
	// basicGroupContent
	auto __owned_basicGroupContent = std::make_unique<Canvas>();
	basicGroupContent = __owned_basicGroupContent.get();
	(void)basicGroupContent->ClearPropertyValues();
	// groupHint
	auto __owned_groupHint = std::make_unique<Label>();
	groupHint = __owned_groupHint.get();
	(void)groupHint->ClearPropertyValues();
	// groupName
	auto __owned_groupName = std::make_unique<TextBox>();
	groupName = __owned_groupName.get();
	(void)groupName->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*groupName, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// groupEnabled
	auto __owned_groupEnabled = std::make_unique<CheckBox>();
	groupEnabled = __owned_groupEnabled.get();
	(void)groupEnabled->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*groupEnabled, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// themeNormalButton
	auto __owned_themeNormalButton = std::make_unique<Button>();
	themeNormalButton = __owned_themeNormalButton.get();
	(void)themeNormalButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*themeNormalButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// themeDisabledButton
	auto __owned_themeDisabledButton = std::make_unique<Button>();
	themeDisabledButton = __owned_themeDisabledButton.get();
	(void)themeDisabledButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*themeDisabledButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// basicExpander
	auto __owned_basicExpander = std::make_unique<Expander>();
	basicExpander = __owned_basicExpander.get();
	(void)basicExpander->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*basicExpander, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// basicExpanderContent
	auto __owned_basicExpanderContent = std::make_unique<Canvas>();
	basicExpanderContent = __owned_basicExpanderContent.get();
	(void)basicExpanderContent->ClearPropertyValues();
	// expanderText
	auto __owned_expanderText = std::make_unique<Label>();
	expanderText = __owned_expanderText.get();
	(void)expanderText->ClearPropertyValues();
	// themeContentControlProbe
	auto __owned_themeContentControlProbe = std::make_unique<ContentControl>();
	themeContentControlProbe = __owned_themeContentControlProbe.get();
	(void)themeContentControlProbe->ClearPropertyValues();
	// themeItemsControlProbe
	auto __owned_themeItemsControlProbe = std::make_unique<ItemsControl>();
	themeItemsControlProbe = __owned_themeItemsControlProbe.get();
	(void)themeItemsControlProbe->ClearPropertyValues();
	// textBlock2
	auto __owned_textBlock2 = std::make_unique<Label>();
	textBlock2 = __owned_textBlock2.get();
	(void)textBlock2->ClearPropertyValues();
	// themeSeparatorProbe
	auto __owned_themeSeparatorProbe = std::make_unique<Separator>();
	themeSeparatorProbe = __owned_themeSeparatorProbe.get();
	(void)themeSeparatorProbe->ClearPropertyValues();
	// tabItem2
	auto __owned_tabItem2 = std::make_unique<TabItem>();
	tabItem2 = __owned_tabItem2.get();
	(void)tabItem2->ClearPropertyValues();
	// border2
	auto __owned_border2 = std::make_unique<Border>();
	border2 = __owned_border2.get();
	(void)border2->ClearPropertyValues();
	// containerSurface
	auto __owned_containerSurface = std::make_unique<Canvas>();
	containerSurface = __owned_containerSurface.get();
	(void)containerSurface->ClearPropertyValues();
	// openImageButton
	auto __owned_openImageButton = std::make_unique<Button>();
	openImageButton = __owned_openImageButton.get();
	(void)openImageButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*openImageButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// demoImage
	auto __owned_demoImage = std::make_unique<Image>();
	demoImage = __owned_demoImage.get();
	(void)demoImage->ClearPropertyValues();
	// demoProgress
	auto __owned_demoProgress = std::make_unique<ProgressBar>();
	demoProgress = __owned_demoProgress.get();
	(void)demoProgress->ClearPropertyValues();
	// indeterminateProgress
	auto __owned_indeterminateProgress = std::make_unique<ProgressBar>();
	indeterminateProgress = __owned_indeterminateProgress.get();
	(void)indeterminateProgress->ClearPropertyValues();
	// loadingRing
	auto __owned_loadingRing = std::make_unique<LoadingRing>();
	loadingRing = __owned_loadingRing.get();
	(void)loadingRing->ClearPropertyValues();
	// progressRing
	auto __owned_progressRing = std::make_unique<ProgressRing>();
	progressRing = __owned_progressRing.get();
	(void)progressRing->ClearPropertyValues();
	// imageVisible
	auto __owned_imageVisible = std::make_unique<Switch>();
	imageVisible = __owned_imageVisible.get();
	(void)imageVisible->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*imageVisible, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// imageVisibleLabel
	auto __owned_imageVisibleLabel = std::make_unique<Label>();
	imageVisibleLabel = __owned_imageVisibleLabel.get();
	(void)imageVisibleLabel->ClearPropertyValues();
	// demoScene
	auto __owned_demoScene = std::make_unique<NativeSurface>();
	demoScene = __owned_demoScene.get();
	(void)demoScene->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*demoScene, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// detailGrid
	auto __owned_detailGrid = std::make_unique<Grid>();
	detailGrid = __owned_detailGrid.get();
	(void)detailGrid->ClearPropertyValues();
	// navigationComposition
	auto __owned_navigationComposition = std::make_unique<Canvas>();
	navigationComposition = __owned_navigationComposition.get();
	(void)navigationComposition->ClearPropertyValues();
	// textBlock3
	auto __owned_textBlock3 = std::make_unique<Label>();
	textBlock3 = __owned_textBlock3.get();
	(void)textBlock3->ClearPropertyValues();
	// sideNavigationList
	auto __owned_sideNavigationList = std::make_unique<ListBox>();
	sideNavigationList = __owned_sideNavigationList.get();
	(void)sideNavigationList->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*sideNavigationList, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// canvas2
	auto __owned_canvas2 = std::make_unique<Canvas>();
	canvas2 = __owned_canvas2.get();
	(void)canvas2->ClearPropertyValues();
	// detailComposition
	auto __owned_detailComposition = std::make_unique<Canvas>();
	detailComposition = __owned_detailComposition.get();
	(void)detailComposition->ClearPropertyValues();
	// stackPanel1
	auto __owned_stackPanel1 = std::make_unique<StackPanel>();
	stackPanel1 = __owned_stackPanel1.get();
	(void)stackPanel1->ClearPropertyValues();
	// textBlock4
	auto __owned_textBlock4 = std::make_unique<Label>();
	textBlock4 = __owned_textBlock4.get();
	(void)textBlock4->ClearPropertyValues();
	// textBlock5
	auto __owned_textBlock5 = std::make_unique<Label>();
	textBlock5 = __owned_textBlock5.get();
	(void)textBlock5->ClearPropertyValues();
	// textBlock6
	auto __owned_textBlock6 = std::make_unique<Label>();
	textBlock6 = __owned_textBlock6.get();
	(void)textBlock6->ClearPropertyValues();
	// textBlock7
	auto __owned_textBlock7 = std::make_unique<Label>();
	textBlock7 = __owned_textBlock7.get();
	(void)textBlock7->ClearPropertyValues();
	// textBlock8
	auto __owned_textBlock8 = std::make_unique<Label>();
	textBlock8 = __owned_textBlock8.get();
	(void)textBlock8->ClearPropertyValues();
	// splitNotes
	auto __owned_splitNotes = std::make_unique<RichTextBox>();
	splitNotes = __owned_splitNotes.get();
	(void)splitNotes->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*splitNotes, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// containerGroup
	auto __owned_containerGroup = std::make_unique<GroupBox>();
	containerGroup = __owned_containerGroup.get();
	(void)containerGroup->ClearPropertyValues();
	// containerGroupText
	auto __owned_containerGroupText = std::make_unique<Label>();
	containerGroupText = __owned_containerGroupText.get();
	(void)containerGroupText->ClearPropertyValues();
	// tabItem3
	auto __owned_tabItem3 = std::make_unique<TabItem>();
	tabItem3 = __owned_tabItem3.get();
	(void)tabItem3->ClearPropertyValues();
	// border3
	auto __owned_border3 = std::make_unique<Border>();
	border3 = __owned_border3.get();
	(void)border3->ClearPropertyValues();
	// dataSurface
	auto __owned_dataSurface = std::make_unique<Canvas>();
	dataSurface = __owned_dataSurface.get();
	(void)dataSurface->ClearPropertyValues();
	// demoTree
	auto __owned_demoTree = std::make_unique<TreeView>();
	demoTree = __owned_demoTree.get();
	(void)demoTree->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*demoTree, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// demoListBox
	auto __owned_demoListBox = std::make_unique<ListBox>();
	demoListBox = __owned_demoListBox.get();
	(void)demoListBox->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*demoListBox, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// demoList
	auto __owned_demoList = std::make_unique<ListView>();
	demoList = __owned_demoList.get();
	(void)demoList->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*demoList, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// composedPropertyEditor
	auto __owned_composedPropertyEditor = std::make_unique<GroupBox>();
	composedPropertyEditor = __owned_composedPropertyEditor.get();
	(void)composedPropertyEditor->ClearPropertyValues();
	// canvas3
	auto __owned_canvas3 = std::make_unique<Canvas>();
	canvas3 = __owned_canvas3.get();
	(void)canvas3->ClearPropertyValues();
	// textBlock9
	auto __owned_textBlock9 = std::make_unique<Label>();
	textBlock9 = __owned_textBlock9.get();
	(void)textBlock9->ClearPropertyValues();
	// composedTitleEditor
	auto __owned_composedTitleEditor = std::make_unique<TextBox>();
	composedTitleEditor = __owned_composedTitleEditor.get();
	(void)composedTitleEditor->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*composedTitleEditor, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// textBlock10
	auto __owned_textBlock10 = std::make_unique<Label>();
	textBlock10 = __owned_textBlock10.get();
	(void)textBlock10->ClearPropertyValues();
	// composedEnabledEditor
	auto __owned_composedEnabledEditor = std::make_unique<CheckBox>();
	composedEnabledEditor = __owned_composedEnabledEditor.get();
	(void)composedEnabledEditor->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*composedEnabledEditor, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// textBlock11
	auto __owned_textBlock11 = std::make_unique<Label>();
	textBlock11 = __owned_textBlock11.get();
	(void)textBlock11->ClearPropertyValues();
	// composedDensityEditor
	auto __owned_composedDensityEditor = std::make_unique<ComboBox>();
	composedDensityEditor = __owned_composedDensityEditor.get();
	(void)composedDensityEditor->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*composedDensityEditor, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// comboBoxItem1
	auto __owned_comboBoxItem1 = std::make_unique<ComboBoxItem>();
	comboBoxItem1 = __owned_comboBoxItem1.get();
	(void)comboBoxItem1->ClearPropertyValues();
	// comboBoxItem2
	auto __owned_comboBoxItem2 = std::make_unique<ComboBoxItem>();
	comboBoxItem2 = __owned_comboBoxItem2.get();
	(void)comboBoxItem2->ClearPropertyValues();
	// comboBoxItem3
	auto __owned_comboBoxItem3 = std::make_unique<ComboBoxItem>();
	comboBoxItem3 = __owned_comboBoxItem3.get();
	(void)comboBoxItem3->ClearPropertyValues();
	// textBlock12
	auto __owned_textBlock12 = std::make_unique<Label>();
	textBlock12 = __owned_textBlock12.get();
	(void)textBlock12->ClearPropertyValues();
	// composedScaleEditor
	auto __owned_composedScaleEditor = std::make_unique<Slider>();
	composedScaleEditor = __owned_composedScaleEditor.get();
	(void)composedScaleEditor->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*composedScaleEditor, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// textBlock13
	auto __owned_textBlock13 = std::make_unique<Label>();
	textBlock13 = __owned_textBlock13.get();
	(void)textBlock13->ClearPropertyValues();
	// authoredStateTree
	auto __owned_authoredStateTree = std::make_unique<TreeView>();
	authoredStateTree = __owned_authoredStateTree.get();
	(void)authoredStateTree->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*authoredStateTree, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// treeViewItem1
	auto __owned_treeViewItem1 = std::make_unique<TreeViewItem>();
	treeViewItem1 = __owned_treeViewItem1.get();
	(void)treeViewItem1->ClearPropertyValues();
	// treeViewItem2
	auto __owned_treeViewItem2 = std::make_unique<TreeViewItem>();
	treeViewItem2 = __owned_treeViewItem2.get();
	(void)treeViewItem2->ClearPropertyValues();
	// treeViewItem3
	auto __owned_treeViewItem3 = std::make_unique<TreeViewItem>();
	treeViewItem3 = __owned_treeViewItem3.get();
	(void)treeViewItem3->ClearPropertyValues();
	// treeViewItem4
	auto __owned_treeViewItem4 = std::make_unique<TreeViewItem>();
	treeViewItem4 = __owned_treeViewItem4.get();
	(void)treeViewItem4->ClearPropertyValues();
	// textBlock14
	auto __owned_textBlock14 = std::make_unique<Label>();
	textBlock14 = __owned_textBlock14.get();
	(void)textBlock14->ClearPropertyValues();
	// tabItem4
	auto __owned_tabItem4 = std::make_unique<TabItem>();
	tabItem4 = __owned_tabItem4.get();
	(void)tabItem4->ClearPropertyValues();
	// border4
	auto __owned_border4 = std::make_unique<Border>();
	border4 = __owned_border4.get();
	(void)border4->ClearPropertyValues();
	// analyticsSurface
	auto __owned_analyticsSurface = std::make_unique<Canvas>();
	analyticsSurface = __owned_analyticsSurface.get();
	(void)analyticsSurface->ClearPropertyValues();
	// border5
	auto __owned_border5 = std::make_unique<Border>();
	border5 = __owned_border5.get();
	(void)border5->ClearPropertyValues();
	// analyticsFilterSurface
	auto __owned_analyticsFilterSurface = std::make_unique<Canvas>();
	analyticsFilterSurface = __owned_analyticsFilterSurface.get();
	(void)analyticsFilterSurface->ClearPropertyValues();
	// analyticsQuery
	auto __owned_analyticsQuery = std::make_unique<TextBox>();
	analyticsQuery = __owned_analyticsQuery.get();
	(void)analyticsQuery->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*analyticsQuery, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// analyticsClosed
	auto __owned_analyticsClosed = std::make_unique<CheckBox>();
	analyticsClosed = __owned_analyticsClosed.get();
	(void)analyticsClosed->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*analyticsClosed, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// analyticsContract
	auto __owned_analyticsContract = std::make_unique<CheckBox>();
	analyticsContract = __owned_analyticsContract.get();
	(void)analyticsContract->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*analyticsContract, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// analyticsHighMargin
	auto __owned_analyticsHighMargin = std::make_unique<CheckBox>();
	analyticsHighMargin = __owned_analyticsHighMargin.get();
	(void)analyticsHighMargin->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*analyticsHighMargin, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// analyticsApply
	auto __owned_analyticsApply = std::make_unique<Button>();
	analyticsApply = __owned_analyticsApply.get();
	(void)analyticsApply->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*analyticsApply, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// analyticsReset
	auto __owned_analyticsReset = std::make_unique<Button>();
	analyticsReset = __owned_analyticsReset.get();
	(void)analyticsReset->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*analyticsReset, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// groupBox1
	auto __owned_groupBox1 = std::make_unique<GroupBox>();
	groupBox1 = __owned_groupBox1.get();
	(void)groupBox1->ClearPropertyValues();
	// canvas4
	auto __owned_canvas4 = std::make_unique<Canvas>();
	canvas4 = __owned_canvas4.get();
	(void)canvas4->ClearPropertyValues();
	// textBlock15
	auto __owned_textBlock15 = std::make_unique<Label>();
	textBlock15 = __owned_textBlock15.get();
	(void)textBlock15->ClearPropertyValues();
	// textBlock16
	auto __owned_textBlock16 = std::make_unique<Label>();
	textBlock16 = __owned_textBlock16.get();
	(void)textBlock16->ClearPropertyValues();
	// groupBox2
	auto __owned_groupBox2 = std::make_unique<GroupBox>();
	groupBox2 = __owned_groupBox2.get();
	(void)groupBox2->ClearPropertyValues();
	// canvas5
	auto __owned_canvas5 = std::make_unique<Canvas>();
	canvas5 = __owned_canvas5.get();
	(void)canvas5->ClearPropertyValues();
	// textBlock17
	auto __owned_textBlock17 = std::make_unique<Label>();
	textBlock17 = __owned_textBlock17.get();
	(void)textBlock17->ClearPropertyValues();
	// progressBar1
	auto __owned_progressBar1 = std::make_unique<ProgressBar>();
	progressBar1 = __owned_progressBar1.get();
	(void)progressBar1->ClearPropertyValues();
	// groupBox3
	auto __owned_groupBox3 = std::make_unique<GroupBox>();
	groupBox3 = __owned_groupBox3.get();
	(void)groupBox3->ClearPropertyValues();
	// canvas6
	auto __owned_canvas6 = std::make_unique<Canvas>();
	canvas6 = __owned_canvas6.get();
	(void)canvas6->ClearPropertyValues();
	// textBlock18
	auto __owned_textBlock18 = std::make_unique<Label>();
	textBlock18 = __owned_textBlock18.get();
	(void)textBlock18->ClearPropertyValues();
	// textBlock19
	auto __owned_textBlock19 = std::make_unique<Label>();
	textBlock19 = __owned_textBlock19.get();
	(void)textBlock19->ClearPropertyValues();
	// chartBar
	auto __owned_chartBar = std::make_unique<Button>();
	chartBar = __owned_chartBar.get();
	(void)chartBar->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*chartBar, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// chartPie
	auto __owned_chartPie = std::make_unique<Button>();
	chartPie = __owned_chartPie.get();
	(void)chartPie->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*chartPie, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// chartLine
	auto __owned_chartLine = std::make_unique<Button>();
	chartLine = __owned_chartLine.get();
	(void)chartLine->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*chartLine, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// salesChart
	auto __owned_salesChart = std::make_unique<ChartView>();
	salesChart = __owned_salesChart.get();
	(void)salesChart->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*salesChart, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// analyticsReport
	auto __owned_analyticsReport = std::make_unique<GroupBox>();
	analyticsReport = __owned_analyticsReport.get();
	(void)analyticsReport->ClearPropertyValues();
	// canvas7
	auto __owned_canvas7 = std::make_unique<Canvas>();
	canvas7 = __owned_canvas7.get();
	(void)canvas7->ClearPropertyValues();
	// stackPanel2
	auto __owned_stackPanel2 = std::make_unique<StackPanel>();
	stackPanel2 = __owned_stackPanel2.get();
	(void)stackPanel2->ClearPropertyValues();
	// textBlock20
	auto __owned_textBlock20 = std::make_unique<Label>();
	textBlock20 = __owned_textBlock20.get();
	(void)textBlock20->ClearPropertyValues();
	// textBlock21
	auto __owned_textBlock21 = std::make_unique<Label>();
	textBlock21 = __owned_textBlock21.get();
	(void)textBlock21->ClearPropertyValues();
	// textBlock22
	auto __owned_textBlock22 = std::make_unique<Label>();
	textBlock22 = __owned_textBlock22.get();
	(void)textBlock22->ClearPropertyValues();
	// textBlock23
	auto __owned_textBlock23 = std::make_unique<Label>();
	textBlock23 = __owned_textBlock23.get();
	(void)textBlock23->ClearPropertyValues();
	// textBlock24
	auto __owned_textBlock24 = std::make_unique<Label>();
	textBlock24 = __owned_textBlock24.get();
	(void)textBlock24->ClearPropertyValues();
	// analyticsRows
	auto __owned_analyticsRows = std::make_unique<ListView>();
	analyticsRows = __owned_analyticsRows.get();
	(void)analyticsRows->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*analyticsRows, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// textBlock25
	auto __owned_textBlock25 = std::make_unique<Label>();
	textBlock25 = __owned_textBlock25.get();
	(void)textBlock25->ClearPropertyValues();
	// tabItem5
	auto __owned_tabItem5 = std::make_unique<TabItem>();
	tabItem5 = __owned_tabItem5.get();
	(void)tabItem5->ClearPropertyValues();
	// border6
	auto __owned_border6 = std::make_unique<Border>();
	border6 = __owned_border6.get();
	(void)border6->ClearPropertyValues();
	// layoutSurface
	auto __owned_layoutSurface = std::make_unique<Canvas>();
	layoutSurface = __owned_layoutSurface.get();
	(void)layoutSurface->ClearPropertyValues();
	// layoutTitle
	auto __owned_layoutTitle = std::make_unique<Label>();
	layoutTitle = __owned_layoutTitle.get();
	(void)layoutTitle->ClearPropertyValues();
	// canvasSemanticsProbe
	auto __owned_canvasSemanticsProbe = std::make_unique<Canvas>();
	canvasSemanticsProbe = __owned_canvasSemanticsProbe.get();
	(void)canvasSemanticsProbe->ClearPropertyValues();
	// border7
	auto __owned_border7 = std::make_unique<Border>();
	border7 = __owned_border7.get();
	(void)border7->ClearPropertyValues();
	// canvasLeftWins
	auto __owned_canvasLeftWins = std::make_unique<Label>();
	canvasLeftWins = __owned_canvasLeftWins.get();
	(void)canvasLeftWins->ClearPropertyValues();
	// canvasRightBottom
	auto __owned_canvasRightBottom = std::make_unique<Label>();
	canvasRightBottom = __owned_canvasRightBottom.get();
	(void)canvasRightBottom->ClearPropertyValues();
	// border8
	auto __owned_border8 = std::make_unique<Border>();
	border8 = __owned_border8.get();
	(void)border8->ClearPropertyValues();
	// demoStack
	auto __owned_demoStack = std::make_unique<StackPanel>();
	demoStack = __owned_demoStack.get();
	(void)demoStack->ClearPropertyValues();
	// stackA
	auto __owned_stackA = std::make_unique<Button>();
	stackA = __owned_stackA.get();
	(void)stackA->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*stackA, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// stackB
	auto __owned_stackB = std::make_unique<Button>();
	stackB = __owned_stackB.get();
	(void)stackB->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*stackB, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// stackC
	auto __owned_stackC = std::make_unique<Button>();
	stackC = __owned_stackC.get();
	(void)stackC->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*stackC, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// border9
	auto __owned_border9 = std::make_unique<Border>();
	border9 = __owned_border9.get();
	(void)border9->ClearPropertyValues();
	// demoGrid
	auto __owned_demoGrid = std::make_unique<Grid>();
	demoGrid = __owned_demoGrid.get();
	(void)demoGrid->ClearPropertyValues();
	// gridHeader
	auto __owned_gridHeader = std::make_unique<Button>();
	gridHeader = __owned_gridHeader.get();
	(void)gridHeader->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*gridHeader, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// gridLeft
	auto __owned_gridLeft = std::make_unique<Label>();
	gridLeft = __owned_gridLeft.get();
	(void)gridLeft->ClearPropertyValues();
	// gridEditor
	auto __owned_gridEditor = std::make_unique<TextBox>();
	gridEditor = __owned_gridEditor.get();
	(void)gridEditor->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*gridEditor, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// gridFooter
	auto __owned_gridFooter = std::make_unique<Button>();
	gridFooter = __owned_gridFooter.get();
	(void)gridFooter->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*gridFooter, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// border10
	auto __owned_border10 = std::make_unique<Border>();
	border10 = __owned_border10.get();
	(void)border10->ClearPropertyValues();
	// demoDock
	auto __owned_demoDock = std::make_unique<DockPanel>();
	demoDock = __owned_demoDock.get();
	(void)demoDock->ClearPropertyValues();
	// dockTop
	auto __owned_dockTop = std::make_unique<Button>();
	dockTop = __owned_dockTop.get();
	(void)dockTop->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*dockTop, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// dockLeft
	auto __owned_dockLeft = std::make_unique<Button>();
	dockLeft = __owned_dockLeft.get();
	(void)dockLeft->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*dockLeft, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// dockFill
	auto __owned_dockFill = std::make_unique<Label>();
	dockFill = __owned_dockFill.get();
	(void)dockFill->ClearPropertyValues();
	// border11
	auto __owned_border11 = std::make_unique<Border>();
	border11 = __owned_border11.get();
	(void)border11->ClearPropertyValues();
	// demoWrap
	auto __owned_demoWrap = std::make_unique<WrapPanel>();
	demoWrap = __owned_demoWrap.get();
	(void)demoWrap->ClearPropertyValues();
	// wrap1
	auto __owned_wrap1 = std::make_unique<Button>();
	wrap1 = __owned_wrap1.get();
	(void)wrap1->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wrap1, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wrap2
	auto __owned_wrap2 = std::make_unique<Button>();
	wrap2 = __owned_wrap2.get();
	(void)wrap2->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wrap2, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wrap3
	auto __owned_wrap3 = std::make_unique<Button>();
	wrap3 = __owned_wrap3.get();
	(void)wrap3->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wrap3, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wrap4
	auto __owned_wrap4 = std::make_unique<Button>();
	wrap4 = __owned_wrap4.get();
	(void)wrap4->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wrap4, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wrap5
	auto __owned_wrap5 = std::make_unique<Button>();
	wrap5 = __owned_wrap5.get();
	(void)wrap5->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wrap5, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wrap6
	auto __owned_wrap6 = std::make_unique<Button>();
	wrap6 = __owned_wrap6.get();
	(void)wrap6->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wrap6, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// border12
	auto __owned_border12 = std::make_unique<Border>();
	border12 = __owned_border12.get();
	(void)border12->ClearPropertyValues();
	// demoRelative
	auto __owned_demoRelative = std::make_unique<RelativePanel>();
	demoRelative = __owned_demoRelative.get();
	(void)demoRelative->ClearPropertyValues();
	// relativeCenter
	auto __owned_relativeCenter = std::make_unique<StackPanel>();
	relativeCenter = __owned_relativeCenter.get();
	(void)relativeCenter->ClearPropertyValues();
	// naturalTextProbe
	auto __owned_naturalTextProbe = std::make_unique<Label>();
	naturalTextProbe = __owned_naturalTextProbe.get();
	(void)naturalTextProbe->ClearPropertyValues();
	// wrappedTextProbe
	auto __owned_wrappedTextProbe = std::make_unique<Label>();
	wrappedTextProbe = __owned_wrappedTextProbe.get();
	(void)wrappedTextProbe->ClearPropertyValues();
	// trimmedTextProbe
	auto __owned_trimmedTextProbe = std::make_unique<Label>();
	trimmedTextProbe = __owned_trimmedTextProbe.get();
	(void)trimmedTextProbe->ClearPropertyValues();
	// relativeCenterButton
	auto __owned_relativeCenterButton = std::make_unique<Button>();
	relativeCenterButton = __owned_relativeCenterButton.get();
	(void)relativeCenterButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*relativeCenterButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// border13
	auto __owned_border13 = std::make_unique<Border>();
	border13 = __owned_border13.get();
	(void)border13->ClearPropertyValues();
	// demoScroll
	auto __owned_demoScroll = std::make_unique<ScrollViewer>();
	demoScroll = __owned_demoScroll.get();
	(void)demoScroll->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*demoScroll, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// demoScrollContent
	auto __owned_demoScrollContent = std::make_unique<Canvas>();
	demoScrollContent = __owned_demoScrollContent.get();
	(void)demoScrollContent->ClearPropertyValues();
	// border14
	auto __owned_border14 = std::make_unique<Border>();
	border14 = __owned_border14.get();
	(void)border14->ClearPropertyValues();
	// scrollCard1
	auto __owned_scrollCard1 = std::make_unique<Canvas>();
	scrollCard1 = __owned_scrollCard1.get();
	(void)scrollCard1->ClearPropertyValues();
	// scrollCard1Text
	auto __owned_scrollCard1Text = std::make_unique<Label>();
	scrollCard1Text = __owned_scrollCard1Text.get();
	(void)scrollCard1Text->ClearPropertyValues();
	// border15
	auto __owned_border15 = std::make_unique<Border>();
	border15 = __owned_border15.get();
	(void)border15->ClearPropertyValues();
	// scrollCard2
	auto __owned_scrollCard2 = std::make_unique<Canvas>();
	scrollCard2 = __owned_scrollCard2.get();
	(void)scrollCard2->ClearPropertyValues();
	// scrollCard2Text
	auto __owned_scrollCard2Text = std::make_unique<Label>();
	scrollCard2Text = __owned_scrollCard2Text.get();
	(void)scrollCard2Text->ClearPropertyValues();
	// farButton
	auto __owned_farButton = std::make_unique<Button>();
	farButton = __owned_farButton.get();
	(void)farButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*farButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// tabItem6
	auto __owned_tabItem6 = std::make_unique<TabItem>();
	tabItem6 = __owned_tabItem6.get();
	(void)tabItem6->ClearPropertyValues();
	// border16
	auto __owned_border16 = std::make_unique<Border>();
	border16 = __owned_border16.get();
	(void)border16->ClearPropertyValues();
	// systemSurface
	auto __owned_systemSurface = std::make_unique<Canvas>();
	systemSurface = __owned_systemSurface.get();
	(void)systemSurface->ClearPropertyValues();
	// systemTitle
	auto __owned_systemTitle = std::make_unique<Label>();
	systemTitle = __owned_systemTitle.get();
	(void)systemTitle->ClearPropertyValues();
	// notifyToggle
	auto __owned_notifyToggle = std::make_unique<Button>();
	notifyToggle = __owned_notifyToggle.get();
	(void)notifyToggle->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*notifyToggle, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// notifyBalloon
	auto __owned_notifyBalloon = std::make_unique<Button>();
	notifyBalloon = __owned_notifyBalloon.get();
	(void)notifyBalloon->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*notifyBalloon, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// showDialog
	auto __owned_showDialog = std::make_unique<Button>();
	showDialog = __owned_showDialog.get();
	(void)showDialog->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*showDialog, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// showToast
	auto __owned_showToast = std::make_unique<Button>();
	showToast = __owned_showToast.get();
	(void)showToast->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*showToast, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// systemHint
	auto __owned_systemHint = std::make_unique<Label>();
	systemHint = __owned_systemHint.get();
	(void)systemHint->ClearPropertyValues();
	// border17
	auto __owned_border17 = std::make_unique<Border>();
	border17 = __owned_border17.get();
	(void)border17->ClearPropertyValues();
	// canvas8
	auto __owned_canvas8 = std::make_unique<Canvas>();
	canvas8 = __owned_canvas8.get();
	(void)canvas8->ClearPropertyValues();
	// textBlock26
	auto __owned_textBlock26 = std::make_unique<Label>();
	textBlock26 = __owned_textBlock26.get();
	(void)textBlock26->ClearPropertyValues();
	// commandTargetButton
	auto __owned_commandTargetButton = std::make_unique<Button>();
	commandTargetButton = __owned_commandTargetButton.get();
	(void)commandTargetButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*commandTargetButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// textBlock27
	auto __owned_textBlock27 = std::make_unique<Label>();
	textBlock27 = __owned_textBlock27.get();
	(void)textBlock27->ClearPropertyValues();
	// commandTargetTrace
	auto __owned_commandTargetTrace = std::make_unique<Label>();
	commandTargetTrace = __owned_commandTargetTrace.get();
	(void)commandTargetTrace->ClearPropertyValues();
	// textBlock28
	auto __owned_textBlock28 = std::make_unique<Label>();
	textBlock28 = __owned_textBlock28.get();
	(void)textBlock28->ClearPropertyValues();
	// textBlock29
	auto __owned_textBlock29 = std::make_unique<Label>();
	textBlock29 = __owned_textBlock29.get();
	(void)textBlock29->ClearPropertyValues();
	// textBlock30
	auto __owned_textBlock30 = std::make_unique<Label>();
	textBlock30 = __owned_textBlock30.get();
	(void)textBlock30->ClearPropertyValues();
	// notificationPanel
	auto __owned_notificationPanel = std::make_unique<GroupBox>();
	notificationPanel = __owned_notificationPanel.get();
	(void)notificationPanel->ClearPropertyValues();
	// canvas9
	auto __owned_canvas9 = std::make_unique<Canvas>();
	canvas9 = __owned_canvas9.get();
	(void)canvas9->ClearPropertyValues();
	// textBlock31
	auto __owned_textBlock31 = std::make_unique<Label>();
	textBlock31 = __owned_textBlock31.get();
	(void)textBlock31->ClearPropertyValues();
	// toastMessage
	auto __owned_toastMessage = std::make_unique<Label>();
	toastMessage = __owned_toastMessage.get();
	(void)toastMessage->ClearPropertyValues();
	// progressBar2
	auto __owned_progressBar2 = std::make_unique<ProgressBar>();
	progressBar2 = __owned_progressBar2.get();
	(void)progressBar2->ClearPropertyValues();
	// dismissToast
	auto __owned_dismissToast = std::make_unique<Button>();
	dismissToast = __owned_dismissToast.get();
	(void)dismissToast->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*dismissToast, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// textBlock32
	auto __owned_textBlock32 = std::make_unique<Label>();
	textBlock32 = __owned_textBlock32.get();
	(void)textBlock32->ClearPropertyValues();
	// tabItem7
	auto __owned_tabItem7 = std::make_unique<TabItem>();
	tabItem7 = __owned_tabItem7.get();
	(void)tabItem7->ClearPropertyValues();
	// border18
	auto __owned_border18 = std::make_unique<Border>();
	border18 = __owned_border18.get();
	(void)border18->ClearPropertyValues();
	// webSurface
	auto __owned_webSurface = std::make_unique<Canvas>();
	webSurface = __owned_webSurface.get();
	(void)webSurface->ClearPropertyValues();
	// invokeWeb
	auto __owned_invokeWeb = std::make_unique<Button>();
	invokeWeb = __owned_invokeWeb.get();
	(void)invokeWeb->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*invokeWeb, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// webHint
	auto __owned_webHint = std::make_unique<Label>();
	webHint = __owned_webHint.get();
	(void)webHint->ClearPropertyValues();
	// border19
	auto __owned_border19 = std::make_unique<Border>();
	border19 = __owned_border19.get();
	(void)border19->ClearPropertyValues();
	// webBrowser
	auto __owned_webBrowser = std::make_unique<WebBrowser>();
	webBrowser = __owned_webBrowser.get();
	(void)webBrowser->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*webBrowser, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// tabItem8
	auto __owned_tabItem8 = std::make_unique<TabItem>();
	tabItem8 = __owned_tabItem8.get();
	(void)tabItem8->ClearPropertyValues();
	// border20
	auto __owned_border20 = std::make_unique<Border>();
	border20 = __owned_border20.get();
	(void)border20->ClearPropertyValues();
	// mediaSurface
	auto __owned_mediaSurface = std::make_unique<Canvas>();
	mediaSurface = __owned_mediaSurface.get();
	(void)mediaSurface->ClearPropertyValues();
	// mediaPlayer
	auto __owned_mediaPlayer = std::make_unique<MediaPlayer>();
	mediaPlayer = __owned_mediaPlayer.get();
	(void)mediaPlayer->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*mediaPlayer, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// mediaOpen
	auto __owned_mediaOpen = std::make_unique<Button>();
	mediaOpen = __owned_mediaOpen.get();
	(void)mediaOpen->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*mediaOpen, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// mediaPlay
	auto __owned_mediaPlay = std::make_unique<Button>();
	mediaPlay = __owned_mediaPlay.get();
	(void)mediaPlay->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*mediaPlay, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// mediaPause
	auto __owned_mediaPause = std::make_unique<Button>();
	mediaPause = __owned_mediaPause.get();
	(void)mediaPause->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*mediaPause, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// mediaStop
	auto __owned_mediaStop = std::make_unique<Button>();
	mediaStop = __owned_mediaStop.get();
	(void)mediaStop->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*mediaStop, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// volumeLabel
	auto __owned_volumeLabel = std::make_unique<Label>();
	volumeLabel = __owned_volumeLabel.get();
	(void)volumeLabel->ClearPropertyValues();
	// mediaVolume
	auto __owned_mediaVolume = std::make_unique<Slider>();
	mediaVolume = __owned_mediaVolume.get();
	(void)mediaVolume->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*mediaVolume, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// speedTitle
	auto __owned_speedTitle = std::make_unique<Label>();
	speedTitle = __owned_speedTitle.get();
	(void)speedTitle->ClearPropertyValues();
	// mediaSpeed
	auto __owned_mediaSpeed = std::make_unique<Slider>();
	mediaSpeed = __owned_mediaSpeed.get();
	(void)mediaSpeed->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*mediaSpeed, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// mediaSpeedText
	auto __owned_mediaSpeedText = std::make_unique<Label>();
	mediaSpeedText = __owned_mediaSpeedText.get();
	(void)mediaSpeedText->ClearPropertyValues();
	// mediaLoop
	auto __owned_mediaLoop = std::make_unique<CheckBox>();
	mediaLoop = __owned_mediaLoop.get();
	(void)mediaLoop->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*mediaLoop, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// mediaProgress
	auto __owned_mediaProgress = std::make_unique<Slider>();
	mediaProgress = __owned_mediaProgress.get();
	(void)mediaProgress->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*mediaProgress, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// mediaTime
	auto __owned_mediaTime = std::make_unique<Label>();
	mediaTime = __owned_mediaTime.get();
	(void)mediaTime->ClearPropertyValues();
	// tabItem9
	auto __owned_tabItem9 = std::make_unique<TabItem>();
	tabItem9 = __owned_tabItem9.get();
	(void)tabItem9->ClearPropertyValues();
	// border21
	auto __owned_border21 = std::make_unique<Border>();
	border21 = __owned_border21.get();
	(void)border21->ClearPropertyValues();
	// wpfLabSurface
	auto __owned_wpfLabSurface = std::make_unique<Canvas>();
	wpfLabSurface = __owned_wpfLabSurface.get();
	(void)wpfLabSurface->ClearPropertyValues();
	// wpfLabTitle
	auto __owned_wpfLabTitle = std::make_unique<Label>();
	wpfLabTitle = __owned_wpfLabTitle.get();
	(void)wpfLabTitle->ClearPropertyValues();
	// wpfBindingScope
	auto __owned_wpfBindingScope = std::make_unique<ContentControl>();
	wpfBindingScope = __owned_wpfBindingScope.get();
	(void)wpfBindingScope->ClearPropertyValues();
	// stackPanel3
	auto __owned_stackPanel3 = std::make_unique<StackPanel>();
	stackPanel3 = __owned_stackPanel3.get();
	(void)stackPanel3->ClearPropertyValues();
	// wpfTypographyOverride
	auto __owned_wpfTypographyOverride = std::make_unique<Label>();
	wpfTypographyOverride = __owned_wpfTypographyOverride.get();
	(void)wpfTypographyOverride->ClearPropertyValues();
	// wpfTwoWayEditor
	auto __owned_wpfTwoWayEditor = std::make_unique<TextBox>();
	wpfTwoWayEditor = __owned_wpfTwoWayEditor.get();
	(void)wpfTwoWayEditor->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wpfTwoWayEditor, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wpfElementMirror
	auto __owned_wpfElementMirror = std::make_unique<Label>();
	wpfElementMirror = __owned_wpfElementMirror.get();
	(void)wpfElementMirror->ClearPropertyValues();
	// wpfSelfValue
	auto __owned_wpfSelfValue = std::make_unique<Label>();
	wpfSelfValue = __owned_wpfSelfValue.get();
	(void)wpfSelfValue->ClearPropertyValues();
	// wpfAncestorValue
	auto __owned_wpfAncestorValue = std::make_unique<Label>();
	wpfAncestorValue = __owned_wpfAncestorValue.get();
	(void)wpfAncestorValue->ClearPropertyValues();
	// wpfFallbackValue
	auto __owned_wpfFallbackValue = std::make_unique<Label>();
	wpfFallbackValue = __owned_wpfFallbackValue.get();
	(void)wpfFallbackValue->ClearPropertyValues();
	// wpfNullValue
	auto __owned_wpfNullValue = std::make_unique<Label>();
	wpfNullValue = __owned_wpfNullValue.get();
	(void)wpfNullValue->ClearPropertyValues();
	// wpfIndexerValue
	auto __owned_wpfIndexerValue = std::make_unique<Label>();
	wpfIndexerValue = __owned_wpfIndexerValue.get();
	(void)wpfIndexerValue->ClearPropertyValues();
	// wpfKeyedIndexerValue
	auto __owned_wpfKeyedIndexerValue = std::make_unique<Label>();
	wpfKeyedIndexerValue = __owned_wpfKeyedIndexerValue.get();
	(void)wpfKeyedIndexerValue->ClearPropertyValues();
	// wpfConvertedValue
	auto __owned_wpfConvertedValue = std::make_unique<Label>();
	wpfConvertedValue = __owned_wpfConvertedValue.get();
	(void)wpfConvertedValue->ClearPropertyValues();
	// wpfMultiValue
	auto __owned_wpfMultiValue = std::make_unique<Label>();
	wpfMultiValue = __owned_wpfMultiValue.get();
	(void)wpfMultiValue->ClearPropertyValues();
	// wpfTemplateAndStyleScope
	auto __owned_wpfTemplateAndStyleScope = std::make_unique<Canvas>();
	wpfTemplateAndStyleScope = __owned_wpfTemplateAndStyleScope.get();
	(void)wpfTemplateAndStyleScope->ClearPropertyValues();
	// textBlock33
	auto __owned_textBlock33 = std::make_unique<Label>();
	textBlock33 = __owned_textBlock33.get();
	(void)textBlock33->ClearPropertyValues();
	// wpfTemplateButton
	auto __owned_wpfTemplateButton = std::make_unique<Button>();
	wpfTemplateButton = __owned_wpfTemplateButton.get();
	(void)wpfTemplateButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wpfTemplateButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wpfTriggerButton
	auto __owned_wpfTriggerButton = std::make_unique<Button>();
	wpfTriggerButton = __owned_wpfTriggerButton.get();
	(void)wpfTriggerButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wpfTriggerButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wpfScopeResourceValue
	auto __owned_wpfScopeResourceValue = std::make_unique<Label>();
	wpfScopeResourceValue = __owned_wpfScopeResourceValue.get();
	(void)wpfScopeResourceValue->ClearPropertyValues();
	// wpfInnerResourceScope
	auto __owned_wpfInnerResourceScope = std::make_unique<StackPanel>();
	wpfInnerResourceScope = __owned_wpfInnerResourceScope.get();
	(void)wpfInnerResourceScope->ClearPropertyValues();
	// wpfInnerResourceValue
	auto __owned_wpfInnerResourceValue = std::make_unique<Label>();
	wpfInnerResourceValue = __owned_wpfInnerResourceValue.get();
	(void)wpfInnerResourceValue->ClearPropertyValues();
	// textBlock34
	auto __owned_textBlock34 = std::make_unique<Label>();
	textBlock34 = __owned_textBlock34.get();
	(void)textBlock34->ClearPropertyValues();
	// wpfItemsScope
	auto __owned_wpfItemsScope = std::make_unique<Canvas>();
	wpfItemsScope = __owned_wpfItemsScope.get();
	(void)wpfItemsScope->ClearPropertyValues();
	// textBlock35
	auto __owned_textBlock35 = std::make_unique<Label>();
	textBlock35 = __owned_textBlock35.get();
	(void)textBlock35->ClearPropertyValues();
	// wpfTemplateList
	auto __owned_wpfTemplateList = std::make_unique<ListBox>();
	wpfTemplateList = __owned_wpfTemplateList.get();
	(void)wpfTemplateList->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wpfTemplateList, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wpfRouteOuter
	auto __owned_wpfRouteOuter = std::make_unique<Border>();
	wpfRouteOuter = __owned_wpfRouteOuter.get();
	(void)wpfRouteOuter->ClearPropertyValues();
	// canvas10
	auto __owned_canvas10 = std::make_unique<Canvas>();
	canvas10 = __owned_canvas10.get();
	(void)canvas10->ClearPropertyValues();
	// textBlock36
	auto __owned_textBlock36 = std::make_unique<Label>();
	textBlock36 = __owned_textBlock36.get();
	(void)textBlock36->ClearPropertyValues();
	// wpfRouteMiddle
	auto __owned_wpfRouteMiddle = std::make_unique<Canvas>();
	wpfRouteMiddle = __owned_wpfRouteMiddle.get();
	(void)wpfRouteMiddle->ClearPropertyValues();
	// wpfRouteSource
	auto __owned_wpfRouteSource = std::make_unique<Button>();
	wpfRouteSource = __owned_wpfRouteSource.get();
	(void)wpfRouteSource->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wpfRouteSource, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wpfFocusPeerB
	auto __owned_wpfFocusPeerB = std::make_unique<Button>();
	wpfFocusPeerB = __owned_wpfFocusPeerB.get();
	(void)wpfFocusPeerB->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wpfFocusPeerB, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wpfFocusPeerC
	auto __owned_wpfFocusPeerC = std::make_unique<Button>();
	wpfFocusPeerC = __owned_wpfFocusPeerC.get();
	(void)wpfFocusPeerC->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wpfFocusPeerC, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wpfNoFocusPeer
	auto __owned_wpfNoFocusPeer = std::make_unique<Button>();
	wpfNoFocusPeer = __owned_wpfNoFocusPeer.get();
	(void)wpfNoFocusPeer->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wpfNoFocusPeer, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wpfTextInputSource
	auto __owned_wpfTextInputSource = std::make_unique<TextBox>();
	wpfTextInputSource = __owned_wpfTextInputSource.get();
	(void)wpfTextInputSource->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wpfTextInputSource, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wpfRouteTrace
	auto __owned_wpfRouteTrace = std::make_unique<Label>();
	wpfRouteTrace = __owned_wpfRouteTrace.get();
	(void)wpfRouteTrace->ClearPropertyValues();
	// wpfInputStats
	auto __owned_wpfInputStats = std::make_unique<Label>();
	wpfInputStats = __owned_wpfInputStats.get();
	(void)wpfInputStats->ClearPropertyValues();
	// wpfHierarchyScope
	auto __owned_wpfHierarchyScope = std::make_unique<Canvas>();
	wpfHierarchyScope = __owned_wpfHierarchyScope.get();
	(void)wpfHierarchyScope->ClearPropertyValues();
	// wpfHierarchyChain
	auto __owned_wpfHierarchyChain = std::make_unique<Label>();
	wpfHierarchyChain = __owned_wpfHierarchyChain.get();
	(void)wpfHierarchyChain->ClearPropertyValues();
	// wpfDispatcherProbe
	auto __owned_wpfDispatcherProbe = std::make_unique<Button>();
	wpfDispatcherProbe = __owned_wpfDispatcherProbe.get();
	(void)wpfDispatcherProbe->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*wpfDispatcherProbe, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// wpfDispatcherResult
	auto __owned_wpfDispatcherResult = std::make_unique<Label>();
	wpfDispatcherResult = __owned_wpfDispatcherResult.get();
	(void)wpfDispatcherResult->ClearPropertyValues();
	// tabItem10
	auto __owned_tabItem10 = std::make_unique<TabItem>();
	tabItem10 = __owned_tabItem10.get();
	(void)tabItem10->ClearPropertyValues();
	// border22
	auto __owned_border22 = std::make_unique<Border>();
	border22 = __owned_border22.get();
	(void)border22->ClearPropertyValues();
	// textCompositionLabSurface
	auto __owned_textCompositionLabSurface = std::make_unique<Canvas>();
	textCompositionLabSurface = __owned_textCompositionLabSurface.get();
	(void)textCompositionLabSurface->ClearPropertyValues();
	// textBlock37
	auto __owned_textBlock37 = std::make_unique<Label>();
	textBlock37 = __owned_textBlock37.get();
	(void)textBlock37->ClearPropertyValues();
	// border23
	auto __owned_border23 = std::make_unique<Border>();
	border23 = __owned_border23.get();
	(void)border23->ClearPropertyValues();
	// canvas11
	auto __owned_canvas11 = std::make_unique<Canvas>();
	canvas11 = __owned_canvas11.get();
	(void)canvas11->ClearPropertyValues();
	// textBlock38
	auto __owned_textBlock38 = std::make_unique<Label>();
	textBlock38 = __owned_textBlock38.get();
	(void)textBlock38->ClearPropertyValues();
	// textBlock39
	auto __owned_textBlock39 = std::make_unique<Label>();
	textBlock39 = __owned_textBlock39.get();
	(void)textBlock39->ClearPropertyValues();
	// compositionTextBox
	auto __owned_compositionTextBox = std::make_unique<TextBox>();
	compositionTextBox = __owned_compositionTextBox.get();
	(void)compositionTextBox->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*compositionTextBox, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// textBlock40
	auto __owned_textBlock40 = std::make_unique<Label>();
	textBlock40 = __owned_textBlock40.get();
	(void)textBlock40->ClearPropertyValues();
	// compositionRichTextBox
	auto __owned_compositionRichTextBox = std::make_unique<RichTextBox>();
	compositionRichTextBox = __owned_compositionRichTextBox.get();
	(void)compositionRichTextBox->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*compositionRichTextBox, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// textBlock41
	auto __owned_textBlock41 = std::make_unique<Label>();
	textBlock41 = __owned_textBlock41.get();
	(void)textBlock41->ClearPropertyValues();
	// compositionPasswordBox
	auto __owned_compositionPasswordBox = std::make_unique<PasswordBox>();
	compositionPasswordBox = __owned_compositionPasswordBox.get();
	(void)compositionPasswordBox->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*compositionPasswordBox, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// textBlock42
	auto __owned_textBlock42 = std::make_unique<Label>();
	textBlock42 = __owned_textBlock42.get();
	(void)textBlock42->ClearPropertyValues();
	// border24
	auto __owned_border24 = std::make_unique<Border>();
	border24 = __owned_border24.get();
	(void)border24->ClearPropertyValues();
	// canvas12
	auto __owned_canvas12 = std::make_unique<Canvas>();
	canvas12 = __owned_canvas12.get();
	(void)canvas12->ClearPropertyValues();
	// textBlock43
	auto __owned_textBlock43 = std::make_unique<Label>();
	textBlock43 = __owned_textBlock43.get();
	(void)textBlock43->ClearPropertyValues();
	// compositionStartProbe
	auto __owned_compositionStartProbe = std::make_unique<Button>();
	compositionStartProbe = __owned_compositionStartProbe.get();
	(void)compositionStartProbe->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*compositionStartProbe, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// compositionUpdateProbe
	auto __owned_compositionUpdateProbe = std::make_unique<Button>();
	compositionUpdateProbe = __owned_compositionUpdateProbe.get();
	(void)compositionUpdateProbe->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*compositionUpdateProbe, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// compositionCommitProbe
	auto __owned_compositionCommitProbe = std::make_unique<Button>();
	compositionCommitProbe = __owned_compositionCommitProbe.get();
	(void)compositionCommitProbe->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*compositionCommitProbe, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// compositionCancelProbe
	auto __owned_compositionCancelProbe = std::make_unique<Button>();
	compositionCancelProbe = __owned_compositionCancelProbe.get();
	(void)compositionCancelProbe->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*compositionCancelProbe, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// compositionSurrogateProbe
	auto __owned_compositionSurrogateProbe = std::make_unique<Button>();
	compositionSurrogateProbe = __owned_compositionSurrogateProbe.get();
	(void)compositionSurrogateProbe->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*compositionSurrogateProbe, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// compositionUnicharProbe
	auto __owned_compositionUnicharProbe = std::make_unique<Button>();
	compositionUnicharProbe = __owned_compositionUnicharProbe.get();
	(void)compositionUnicharProbe->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*compositionUnicharProbe, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// compositionFocusProbe
	auto __owned_compositionFocusProbe = std::make_unique<Button>();
	compositionFocusProbe = __owned_compositionFocusProbe.get();
	(void)compositionFocusProbe->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*compositionFocusProbe, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// compositionPreviewHandledProbe
	auto __owned_compositionPreviewHandledProbe = std::make_unique<Button>();
	compositionPreviewHandledProbe = __owned_compositionPreviewHandledProbe.get();
	(void)compositionPreviewHandledProbe->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*compositionPreviewHandledProbe, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// compositionResetProbe
	auto __owned_compositionResetProbe = std::make_unique<Button>();
	compositionResetProbe = __owned_compositionResetProbe.get();
	(void)compositionResetProbe->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*compositionResetProbe, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// compositionState
	auto __owned_compositionState = std::make_unique<Label>();
	compositionState = __owned_compositionState.get();
	(void)compositionState->ClearPropertyValues();
	// compositionStats
	auto __owned_compositionStats = std::make_unique<Label>();
	compositionStats = __owned_compositionStats.get();
	(void)compositionStats->ClearPropertyValues();
	// border25
	auto __owned_border25 = std::make_unique<Border>();
	border25 = __owned_border25.get();
	(void)border25->ClearPropertyValues();
	// canvas13
	auto __owned_canvas13 = std::make_unique<Canvas>();
	canvas13 = __owned_canvas13.get();
	(void)canvas13->ClearPropertyValues();
	// textBlock44
	auto __owned_textBlock44 = std::make_unique<Label>();
	textBlock44 = __owned_textBlock44.get();
	(void)textBlock44->ClearPropertyValues();
	// compositionTrace
	auto __owned_compositionTrace = std::make_unique<Label>();
	compositionTrace = __owned_compositionTrace.get();
	(void)compositionTrace->ClearPropertyValues();
	// tabItem11
	auto __owned_tabItem11 = std::make_unique<TabItem>();
	tabItem11 = __owned_tabItem11.get();
	(void)tabItem11->ClearPropertyValues();
	// border26
	auto __owned_border26 = std::make_unique<Border>();
	border26 = __owned_border26.get();
	(void)border26->ClearPropertyValues();
	// presentationLabSurface
	auto __owned_presentationLabSurface = std::make_unique<Canvas>();
	presentationLabSurface = __owned_presentationLabSurface.get();
	(void)presentationLabSurface->ClearPropertyValues();
	// textBlock45
	auto __owned_textBlock45 = std::make_unique<Label>();
	textBlock45 = __owned_textBlock45.get();
	(void)textBlock45->ClearPropertyValues();
	// presentationProbeSurface
	auto __owned_presentationProbeSurface = std::make_unique<NativeSurface>();
	presentationProbeSurface = __owned_presentationProbeSurface.get();
	(void)presentationProbeSurface->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*presentationProbeSurface, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// presentationTopologyTile
	auto __owned_presentationTopologyTile = std::make_unique<Label>();
	presentationTopologyTile = __owned_presentationTopologyTile.get();
	(void)presentationTopologyTile->ClearPropertyValues();
	// canvas14
	auto __owned_canvas14 = std::make_unique<Canvas>();
	canvas14 = __owned_canvas14.get();
	(void)canvas14->ClearPropertyValues();
	// textBlock46
	auto __owned_textBlock46 = std::make_unique<Label>();
	textBlock46 = __owned_textBlock46.get();
	(void)textBlock46->ClearPropertyValues();
	// textBlock47
	auto __owned_textBlock47 = std::make_unique<Label>();
	textBlock47 = __owned_textBlock47.get();
	(void)textBlock47->ClearPropertyValues();
	// textBlock48
	auto __owned_textBlock48 = std::make_unique<Label>();
	textBlock48 = __owned_textBlock48.get();
	(void)textBlock48->ClearPropertyValues();
	// textBlock49
	auto __owned_textBlock49 = std::make_unique<Label>();
	textBlock49 = __owned_textBlock49.get();
	(void)textBlock49->ClearPropertyValues();
	// textBlock50
	auto __owned_textBlock50 = std::make_unique<Label>();
	textBlock50 = __owned_textBlock50.get();
	(void)textBlock50->ClearPropertyValues();
	// textBlock51
	auto __owned_textBlock51 = std::make_unique<Label>();
	textBlock51 = __owned_textBlock51.get();
	(void)textBlock51->ClearPropertyValues();
	// presentationRegionButton
	auto __owned_presentationRegionButton = std::make_unique<Button>();
	presentationRegionButton = __owned_presentationRegionButton.get();
	(void)presentationRegionButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*presentationRegionButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// presentationGeometryButton
	auto __owned_presentationGeometryButton = std::make_unique<Button>();
	presentationGeometryButton = __owned_presentationGeometryButton.get();
	(void)presentationGeometryButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*presentationGeometryButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// presentationCompositionButton
	auto __owned_presentationCompositionButton = std::make_unique<Button>();
	presentationCompositionButton = __owned_presentationCompositionButton.get();
	(void)presentationCompositionButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*presentationCompositionButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// presentationFullButton
	auto __owned_presentationFullButton = std::make_unique<Button>();
	presentationFullButton = __owned_presentationFullButton.get();
	(void)presentationFullButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*presentationFullButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// presentationTopologyButton
	auto __owned_presentationTopologyButton = std::make_unique<Button>();
	presentationTopologyButton = __owned_presentationTopologyButton.get();
	(void)presentationTopologyButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*presentationTopologyButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// presentationDeviceLossButton
	auto __owned_presentationDeviceLossButton = std::make_unique<Button>();
	presentationDeviceLossButton = __owned_presentationDeviceLossButton.get();
	(void)presentationDeviceLossButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*presentationDeviceLossButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// presentationStatus
	auto __owned_presentationStatus = std::make_unique<Label>();
	presentationStatus = __owned_presentationStatus.get();
	(void)presentationStatus->ClearPropertyValues();
	// textBlock52
	auto __owned_textBlock52 = std::make_unique<Label>();
	textBlock52 = __owned_textBlock52.get();
	(void)textBlock52->ClearPropertyValues();
	// systemContextMenu
	auto __owned_systemContextMenu = std::make_unique<ContextMenu>();
	systemContextMenu = __owned_systemContextMenu.get();
	(void)systemContextMenu->ClearPropertyValues();
	// menuItem6
	auto __owned_menuItem6 = std::make_unique<MenuItem>();
	menuItem6 = __owned_menuItem6.get();
	(void)menuItem6->ClearPropertyValues();
	// menuItem7
	auto __owned_menuItem7 = std::make_unique<MenuItem>();
	menuItem7 = __owned_menuItem7.get();
	(void)menuItem7->ClearPropertyValues();
	// separator2
	auto __owned_separator2 = std::make_unique<Separator>();
	separator2 = __owned_separator2.get();
	(void)separator2->ClearPropertyValues();
	// menuItem8
	auto __owned_menuItem8 = std::make_unique<MenuItem>();
	menuItem8 = __owned_menuItem8.get();
	(void)menuItem8->ClearPropertyValues();
	// menuItem9
	auto __owned_menuItem9 = std::make_unique<MenuItem>();
	menuItem9 = __owned_menuItem9.get();
	(void)menuItem9->ClearPropertyValues();
	// menuItem10
	auto __owned_menuItem10 = std::make_unique<MenuItem>();
	menuItem10 = __owned_menuItem10.get();
	(void)menuItem10->ClearPropertyValues();
	// mainStatusBar
	auto __owned_mainStatusBar = std::make_unique<StatusBar>();
	mainStatusBar = __owned_mainStatusBar.get();
	(void)mainStatusBar->ClearPropertyValues();

	// Install the native ItemsPanel before ItemsSource creates any item containers.
	mainToolBar->SetItemsPanel(ItemsPanelTemplateReference(__itemsPanel_MainToolBarItemsPanel_1));
	if (mainToolBar->GetItemsPanel().Get() != __itemsPanel_MainToolBarItemsPanel_1.get())
		throw std::runtime_error("Generated ItemsPanel installation failed");
	demoListBox->SetItemsPanel(ItemsPanelTemplateReference(__itemsPanel_DemoTaskItemsPanel_2));
	if (demoListBox->GetItemsPanel().Get() != __itemsPanel_DemoTaskItemsPanel_2.get())
		throw std::runtime_error("Generated ItemsPanel installation failed");
	wpfTemplateList->SetItemsPanel(ItemsPanelTemplateReference(__itemsPanel_WpfLabItemsPanel_3));
	if (wpfTemplateList->GetItemsPanel().Get() != __itemsPanel_WpfLabItemsPanel_3.get())
		throw std::runtime_error("Generated ItemsPanel installation failed");

	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_25[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 1869304552508798568ULL }, 0u },
	};
	// Item display/selection member paths are immutable token tables installed before ItemsSource realization.
	basicCombo->SetCompiledDisplayMemberPath(CompiledBindingPathView{ __cuiCompiledBindingPath_25 });
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_26[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Int, BindingSourcePropertyToken{ 13817039935692630216ULL }, 0u },
	};
	basicCombo->SetCompiledSelectedValuePath(CompiledBindingPathView{ __cuiCompiledBindingPath_26 });
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_27[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 1869304552508798568ULL }, 0u },
	};
	sideNavigationList->SetCompiledDisplayMemberPath(CompiledBindingPathView{ __cuiCompiledBindingPath_27 });
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_28[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Int, BindingSourcePropertyToken{ 13817039935692630216ULL }, 0u },
	};
	sideNavigationList->SetCompiledSelectedValuePath(CompiledBindingPathView{ __cuiCompiledBindingPath_28 });
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_29[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 5250172348753352866ULL }, 0u },
	};
	demoTree->SetCompiledDisplayMemberPath(CompiledBindingPathView{ __cuiCompiledBindingPath_29 });
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_30[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 5250172348753352866ULL }, 0u },
	};
	demoTree->SetCompiledSelectedValuePath(CompiledBindingPathView{ __cuiCompiledBindingPath_30 });
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_31[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 1869304552508798568ULL }, 0u },
	};
	demoListBox->SetCompiledDisplayMemberPath(CompiledBindingPathView{ __cuiCompiledBindingPath_31 });
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_32[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 1869304552508798568ULL }, 0u },
	};
	demoListBox->SetCompiledSelectedValuePath(CompiledBindingPathView{ __cuiCompiledBindingPath_32 });
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_33[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 5250172348753352866ULL }, 0u },
	};
	demoList->SetCompiledDisplayMemberPath(CompiledBindingPathView{ __cuiCompiledBindingPath_33 });
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_34[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 5250172348753352866ULL }, 0u },
	};
	demoList->SetCompiledSelectedValuePath(CompiledBindingPathView{ __cuiCompiledBindingPath_34 });
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_35[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 12717390152918474433ULL }, 0u },
	};
	analyticsRows->SetCompiledSelectedValuePath(CompiledBindingPathView{ __cuiCompiledBindingPath_35 });
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_36[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 7420362091733594415ULL }, 0u },
	};
	wpfTemplateList->SetCompiledDisplayMemberPath(CompiledBindingPathView{ __cuiCompiledBindingPath_36 });
	static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_37[] =
	{
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 1869304552508798568ULL }, 0u },
	};
	mainStatusBar->SetCompiledDisplayMemberPath(CompiledBindingPathView{ __cuiCompiledBindingPath_37 });

	// Install native DataTemplate references before ItemsSource can realize any item visuals.
	demoTree->SetCompiledImplicitItemTemplateResolver([__dataTemplate_Implicit_DemoFile_6, __dataTemplate_Implicit_DemoFolder_5](DataTypeToken itemType) -> ItemTemplateReference
	{
		if (itemType == DataTypeToken{ 16771066560557046080ULL }) return ItemTemplateReference(__dataTemplate_Implicit_DemoFile_6);
		if (itemType == DataTypeToken{ 9713005176006114176ULL }) return ItemTemplateReference(__dataTemplate_Implicit_DemoFolder_5);
		return {};
	});
	demoTree->SetItemTemplate(ItemTemplateReference(__dataTemplate_Implicit_DemoFolder_5));
	if (demoTree->GetItemTemplate().Get() != __dataTemplate_Implicit_DemoFolder_5.get())
		throw std::runtime_error("Generated ItemTemplate installation failed");
	demoListBox->SetItemTemplate(ItemTemplateReference(__dataTemplate_DemoTaskRow_1));
	if (demoListBox->GetItemTemplate().Get() != __dataTemplate_DemoTaskRow_1.get())
		throw std::runtime_error("Generated ItemTemplate installation failed");
	demoList->SetItemTemplate(ItemTemplateReference(__dataTemplate_DemoListViewRow_3));
	if (demoList->GetItemTemplate().Get() != __dataTemplate_DemoListViewRow_3.get())
		throw std::runtime_error("Generated ItemTemplate installation failed");
	authoredStateTree->SetCompiledImplicitItemTemplateResolver([__dataTemplate_Implicit_DemoFile_6, __dataTemplate_Implicit_DemoFolder_5](DataTypeToken itemType) -> ItemTemplateReference
	{
		if (itemType == DataTypeToken{ 16771066560557046080ULL }) return ItemTemplateReference(__dataTemplate_Implicit_DemoFile_6);
		if (itemType == DataTypeToken{ 9713005176006114176ULL }) return ItemTemplateReference(__dataTemplate_Implicit_DemoFolder_5);
		return {};
	});
	analyticsRows->SetItemTemplate(ItemTemplateReference(__dataTemplate_AnalyticsRowTemplate_2));
	if (analyticsRows->GetItemTemplate().Get() != __dataTemplate_AnalyticsRowTemplate_2.get())
		throw std::runtime_error("Generated ItemTemplate installation failed");
	wpfTemplateList->SetItemTemplate(ItemTemplateReference(__dataTemplate_WpfLabPersonRow_7));
	if (wpfTemplateList->GetItemTemplate().Get() != __dataTemplate_WpfLabPersonRow_7.get())
		throw std::runtime_error("Generated ItemTemplate installation failed");

	// GroupStyle is installed before ItemsSource realizes grouped headers.
	demoListBox->SetGroupStyle(GroupStyleReference(__groupStyle_DemoTaskGroups_1));
	if (demoListBox->GetGroupStyle().Get() != __groupStyle_DemoTaskGroups_1.get())
		throw std::runtime_error("Generated GroupStyle installation failed");

	std::wstring __frameworkThemeError;

	// Repeatable pure-C++ factories for authored ControlTemplate resources.
	auto __controlTemplate_DemoListViewItemTemplate_1 = std::make_shared<CuiGeneratedControlTemplate>(
		UIClass::UI_ListViewItem, L"DemoListViewItemTemplate", []() -> std::unique_ptr<Control>
		{
			auto result = std::make_unique<ListViewItem>();
			(void)result->ClearPropertyValues();
			return result;
		});
	std::weak_ptr<const IControlTemplate> __weak_controlTemplate_DemoListViewItemTemplate_1 = __controlTemplate_DemoListViewItemTemplate_1;
	auto __controlTemplate_WpfLabButtonTemplate_2 = std::make_shared<CuiGeneratedControlTemplate>(
		UIClass::UI_Button, L"WpfLabButtonTemplate", []() -> std::unique_ptr<Control>
		{
			auto result = std::make_unique<Button>();
			(void)result->ClearPropertyValues();
			(void)cui::framework::DependencyPropertyAccess::SetValue(*result, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
			return result;
		});
	std::weak_ptr<const IControlTemplate> __weak_controlTemplate_WpfLabButtonTemplate_2 = __controlTemplate_WpfLabButtonTemplate_2;
	auto __controlTemplate_WpfLabButtonTemplateAlternate_3 = std::make_shared<CuiGeneratedControlTemplate>(
		UIClass::UI_Button, L"WpfLabButtonTemplateAlternate", []() -> std::unique_ptr<Control>
		{
			auto result = std::make_unique<Button>();
			(void)result->ClearPropertyValues();
			(void)cui::framework::DependencyPropertyAccess::SetValue(*result, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
			return result;
		});
	std::weak_ptr<const IControlTemplate> __weak_controlTemplate_WpfLabButtonTemplateAlternate_3 = __controlTemplate_WpfLabButtonTemplateAlternate_3;
	auto __controlTemplate_WpfLabListTemplate_4 = std::make_shared<CuiGeneratedControlTemplate>(
		UIClass::UI_ListBox, L"WpfLabListTemplate", []() -> std::unique_ptr<Control>
		{
			auto result = std::make_unique<ListBox>();
			(void)result->ClearPropertyValues();
			(void)cui::framework::DependencyPropertyAccess::SetValue(*result, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
			return result;
		});
	std::weak_ptr<const IControlTemplate> __weak_controlTemplate_WpfLabListTemplate_4 = __controlTemplate_WpfLabListTemplate_4;
	auto __controlTemplate_WpfLabListItemTemplate_5 = std::make_shared<CuiGeneratedControlTemplate>(
		UIClass::UI_ListBoxItem, L"WpfLabListItemTemplate", []() -> std::unique_ptr<Control>
		{
			auto result = std::make_unique<ListBoxItem>();
			(void)result->ClearPropertyValues();
			return result;
		});
	std::weak_ptr<const IControlTemplate> __weak_controlTemplate_WpfLabListItemTemplate_5 = __controlTemplate_WpfLabListItemTemplate_5;

	__controlTemplate_DemoListViewItemTemplate_1->SetApplyCallback([this](Control& __templateOwner, std::wstring* outError) -> bool
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		try
		{
			std::wstring __templateThemeError;
			// __cuiStaticTemplateOwner1_template_border1
			auto __owned___cuiStaticTemplateOwner1_template_border1 = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner1_template_border1 = __owned___cuiStaticTemplateOwner1_template_border1.get();
			(void)__cuiStaticTemplateOwner1_template_border1->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_contentPresenter1
			auto __owned___cuiStaticTemplateOwner1_template_contentPresenter1 = std::make_unique<ContentPresenter>();
			auto* __cuiStaticTemplateOwner1_template_contentPresenter1 = __owned___cuiStaticTemplateOwner1_template_contentPresenter1.get();
			(void)__cuiStaticTemplateOwner1_template_contentPresenter1->ClearPropertyValues();

			// Establish a fresh template namescope for this application.
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_border1, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 15324945839337174514ULL }, __cuiStaticTemplateOwner1_template_border1))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_contentPresenter1, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 6516814457299532077ULL }, __cuiStaticTemplateOwner1_template_contentPresenter1))
				return fail(L"ControlTemplate 部件注册失败。");
			{
				auto* contentOwner = dynamic_cast<ContentControl*>(&__templateOwner);
				auto* presenter = dynamic_cast<ContentPresenter*>(__cuiStaticTemplateOwner1_template_contentPresenter1);
				if (!contentOwner || !presenter || !cui::framework::TemplateAccess::RegisterContentPresenter(*contentOwner, presenter))
					return fail(L"ControlTemplate ContentPresenter 注册失败。");
			}
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_border1, Border::PaddingProperty(), BindingValue(Thickness(6.f, 2.f, 6.f, 2.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_border1, Control::BorderBrushProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.827451f, 0.862745f, 0.909804f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_border1, Control::BorderThicknessProperty(), BindingValue(Thickness(1.f, 1.f, 1.f, 1.f)), DependencyPropertyValueSource::Template);
			if (!__cuiStaticTemplateOwner1_template_border1->DataBindings.AddTemplateBinding(Control::BackgroundProperty(), __templateOwner, Control::BackgroundProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_contentPresenter1->DataBindings.AddTemplateBinding(ContentPresenter::ContentProperty(), __templateOwner, ListViewItem::ContentProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_contentPresenter1->DataBindings.AddTemplateBinding(ContentPresenter::ContentTemplateProperty(), __templateOwner, ListViewItem::ContentTemplateProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			__cuiStaticTemplateOwner1_template_contentPresenter1->SetCompiledDisplayMemberPath(static_cast<ListViewItem&>(__templateOwner).GetCompiledDisplayMemberPath());
			cui::framework::TemplateAccess::SetTemplateRoot(__templateOwner, std::move(__owned___cuiStaticTemplateOwner1_template_border1));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_border1, nullptr);
			__cuiStaticTemplateOwner1_template_border1->SetChild(std::move(__owned___cuiStaticTemplateOwner1_template_contentPresenter1));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_contentPresenter1, nullptr);
			if (!cui::framework::TemplateAccess::GetTemplateRoot(__templateOwner))
				return fail(L"ControlTemplate 未生成唯一视觉根。");
			if (outError) outError->clear();
			return true;
		}
		catch (const std::exception&)
		{
			return fail(L"ControlTemplate 静态构造发生运行时异常。");
		}
		catch (...)
		{
			return fail(L"ControlTemplate 静态构造发生未知异常。");
		}
	});

	__controlTemplate_WpfLabButtonTemplate_2->SetApplyCallback([this](Control& __templateOwner, std::wstring* outError) -> bool
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		try
		{
			std::wstring __templateThemeError;
			// __cuiStaticTemplateOwner2_template_wpfButtonChrome
			auto __owned___cuiStaticTemplateOwner2_template_wpfButtonChrome = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner2_template_wpfButtonChrome = __owned___cuiStaticTemplateOwner2_template_wpfButtonChrome.get();
			(void)__cuiStaticTemplateOwner2_template_wpfButtonChrome->ClearPropertyValues();
			// __cuiStaticTemplateOwner2_template_stackPanel1
			auto __owned___cuiStaticTemplateOwner2_template_stackPanel1 = std::make_unique<StackPanel>();
			auto* __cuiStaticTemplateOwner2_template_stackPanel1 = __owned___cuiStaticTemplateOwner2_template_stackPanel1.get();
			(void)__cuiStaticTemplateOwner2_template_stackPanel1->ClearPropertyValues();
			// __cuiStaticTemplateOwner2_template_wpfTemplatedParentValue
			auto __owned___cuiStaticTemplateOwner2_template_wpfTemplatedParentValue = std::make_unique<Label>();
			auto* __cuiStaticTemplateOwner2_template_wpfTemplatedParentValue = __owned___cuiStaticTemplateOwner2_template_wpfTemplatedParentValue.get();
			(void)__cuiStaticTemplateOwner2_template_wpfTemplatedParentValue->ClearPropertyValues();
			// __cuiStaticTemplateOwner2_template_wpfTreeRelationValue
			auto __owned___cuiStaticTemplateOwner2_template_wpfTreeRelationValue = std::make_unique<Label>();
			auto* __cuiStaticTemplateOwner2_template_wpfTreeRelationValue = __owned___cuiStaticTemplateOwner2_template_wpfTreeRelationValue.get();
			(void)__cuiStaticTemplateOwner2_template_wpfTreeRelationValue->ClearPropertyValues();
			// __cuiStaticTemplateOwner2_template_wpfButtonContentPresenter
			auto __owned___cuiStaticTemplateOwner2_template_wpfButtonContentPresenter = std::make_unique<ContentPresenter>();
			auto* __cuiStaticTemplateOwner2_template_wpfButtonContentPresenter = __owned___cuiStaticTemplateOwner2_template_wpfButtonContentPresenter.get();
			(void)__cuiStaticTemplateOwner2_template_wpfButtonContentPresenter->ClearPropertyValues();

			// Establish a fresh template namescope for this application.
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner2_template_wpfButtonChrome, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 12356714753487164146ULL }, __cuiStaticTemplateOwner2_template_wpfButtonChrome))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner2_template_stackPanel1, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 4045250499287358562ULL }, __cuiStaticTemplateOwner2_template_stackPanel1))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner2_template_wpfTemplatedParentValue, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 7344519872359539911ULL }, __cuiStaticTemplateOwner2_template_wpfTemplatedParentValue))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner2_template_wpfTreeRelationValue, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 13699974898486228057ULL }, __cuiStaticTemplateOwner2_template_wpfTreeRelationValue))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner2_template_wpfButtonContentPresenter, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 2717508954126581907ULL }, __cuiStaticTemplateOwner2_template_wpfButtonContentPresenter))
				return fail(L"ControlTemplate 部件注册失败。");
			{
				auto* contentOwner = dynamic_cast<ContentControl*>(&__templateOwner);
				auto* presenter = dynamic_cast<ContentPresenter*>(__cuiStaticTemplateOwner2_template_wpfButtonContentPresenter);
				if (!contentOwner || !presenter || !cui::framework::TemplateAccess::RegisterContentPresenter(*contentOwner, presenter))
					return fail(L"ControlTemplate ContentPresenter 注册失败。");
			}
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_wpfButtonChrome, Border::PaddingProperty(), BindingValue(Thickness(8.f, 8.f, 8.f, 8.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_wpfButtonChrome, Control::BackgroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.968627f, 0.976471f, 0.988235f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_wpfButtonChrome, Control::BorderBrushProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.827451f, 0.862745f, 0.909804f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_wpfButtonChrome, Control::BorderThicknessProperty(), BindingValue(Thickness(1.f, 1.f, 1.f, 1.f)), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_wpfTemplatedParentValue, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(24.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_wpfTemplatedParentValue, Control::ForegroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.184314f, 0.435294f, 0.894118f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_wpfTreeRelationValue, Label::TextProperty(), BindingValue(L"template root: VisualParent=Button · LogicalParent=∅ · TemplatedParent=Button"), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_wpfTreeRelationValue, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(20.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_wpfTreeRelationValue, Control::ForegroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.454902f, 0.513726f, 0.6f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_wpfTreeRelationValue, Control::FontSizeProperty(), BindingValue(11.0), DependencyPropertyValueSource::Template);
			if (!__cuiStaticTemplateOwner2_template_wpfButtonContentPresenter->DataBindings.AddTemplateBinding(ContentPresenter::ContentProperty(), __templateOwner, Button::ContentProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner2_template_wpfButtonContentPresenter->DataBindings.AddTemplateBinding(ContentPresenter::ContentTemplateProperty(), __templateOwner, Button::ContentTemplateProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			__cuiStaticTemplateOwner2_template_wpfButtonContentPresenter->SetCompiledDisplayMemberPath(static_cast<Button&>(__templateOwner).GetCompiledDisplayMemberPath());
			cui::framework::TemplateAccess::SetTemplateRoot(__templateOwner, std::move(__owned___cuiStaticTemplateOwner2_template_wpfButtonChrome));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner2_template_wpfButtonChrome, nullptr);
			__cuiStaticTemplateOwner2_template_wpfButtonChrome->SetChild(std::move(__owned___cuiStaticTemplateOwner2_template_stackPanel1));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner2_template_stackPanel1, nullptr);
			__cuiStaticTemplateOwner2_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner2_template_wpfTemplatedParentValue));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner2_template_wpfTemplatedParentValue, nullptr);
			__cuiStaticTemplateOwner2_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner2_template_wpfTreeRelationValue));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner2_template_wpfTreeRelationValue, nullptr);
			__cuiStaticTemplateOwner2_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner2_template_wpfButtonContentPresenter));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner2_template_wpfButtonContentPresenter, nullptr);
			{
				// CUI:AOT binding-source=direct-dp
				const bool attached = __cuiStaticTemplateOwner2_template_wpfTemplatedParentValue->DataBindings.Add(Label::TextProperty(), cui::binding::MakeCompiledDependencyPropertySource(__templateOwner, Control::AutomationNameProperty()), BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
				if (!attached)
					return fail(L"ControlTemplate Binding 安装失败。");
			}
			if (!cui::framework::TemplateAccess::GetTemplateRoot(__templateOwner))
				return fail(L"ControlTemplate 未生成唯一视觉根。");
			if (outError) outError->clear();
			return true;
		}
		catch (const std::exception&)
		{
			return fail(L"ControlTemplate 静态构造发生运行时异常。");
		}
		catch (...)
		{
			return fail(L"ControlTemplate 静态构造发生未知异常。");
		}
	});

	__controlTemplate_WpfLabButtonTemplateAlternate_3->SetApplyCallback([this](Control& __templateOwner, std::wstring* outError) -> bool
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		try
		{
			std::wstring __templateThemeError;
			// __cuiStaticTemplateOwner3_template_wpfAlternateButtonChrome
			auto __owned___cuiStaticTemplateOwner3_template_wpfAlternateButtonChrome = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner3_template_wpfAlternateButtonChrome = __owned___cuiStaticTemplateOwner3_template_wpfAlternateButtonChrome.get();
			(void)__cuiStaticTemplateOwner3_template_wpfAlternateButtonChrome->ClearPropertyValues();
			// __cuiStaticTemplateOwner3_template_stackPanel1
			auto __owned___cuiStaticTemplateOwner3_template_stackPanel1 = std::make_unique<StackPanel>();
			auto* __cuiStaticTemplateOwner3_template_stackPanel1 = __owned___cuiStaticTemplateOwner3_template_stackPanel1.get();
			(void)__cuiStaticTemplateOwner3_template_stackPanel1->ClearPropertyValues();
			// __cuiStaticTemplateOwner3_template_wpfAlternateTemplateState
			auto __owned___cuiStaticTemplateOwner3_template_wpfAlternateTemplateState = std::make_unique<Label>();
			auto* __cuiStaticTemplateOwner3_template_wpfAlternateTemplateState = __owned___cuiStaticTemplateOwner3_template_wpfAlternateTemplateState.get();
			(void)__cuiStaticTemplateOwner3_template_wpfAlternateTemplateState->ClearPropertyValues();
			// __cuiStaticTemplateOwner3_template_textBlock1
			auto __owned___cuiStaticTemplateOwner3_template_textBlock1 = std::make_unique<Label>();
			auto* __cuiStaticTemplateOwner3_template_textBlock1 = __owned___cuiStaticTemplateOwner3_template_textBlock1.get();
			(void)__cuiStaticTemplateOwner3_template_textBlock1->ClearPropertyValues();
			// __cuiStaticTemplateOwner3_template_wpfAlternateContentPresenter
			auto __owned___cuiStaticTemplateOwner3_template_wpfAlternateContentPresenter = std::make_unique<ContentPresenter>();
			auto* __cuiStaticTemplateOwner3_template_wpfAlternateContentPresenter = __owned___cuiStaticTemplateOwner3_template_wpfAlternateContentPresenter.get();
			(void)__cuiStaticTemplateOwner3_template_wpfAlternateContentPresenter->ClearPropertyValues();

			// Establish a fresh template namescope for this application.
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner3_template_wpfAlternateButtonChrome, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 3414718889109066440ULL }, __cuiStaticTemplateOwner3_template_wpfAlternateButtonChrome))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner3_template_stackPanel1, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 4045250499287358562ULL }, __cuiStaticTemplateOwner3_template_stackPanel1))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner3_template_wpfAlternateTemplateState, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 2386586653477049041ULL }, __cuiStaticTemplateOwner3_template_wpfAlternateTemplateState))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner3_template_textBlock1, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 13143219356311737182ULL }, __cuiStaticTemplateOwner3_template_textBlock1))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner3_template_wpfAlternateContentPresenter, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 7369527155058470709ULL }, __cuiStaticTemplateOwner3_template_wpfAlternateContentPresenter))
				return fail(L"ControlTemplate 部件注册失败。");
			{
				auto* contentOwner = dynamic_cast<ContentControl*>(&__templateOwner);
				auto* presenter = dynamic_cast<ContentPresenter*>(__cuiStaticTemplateOwner3_template_wpfAlternateContentPresenter);
				if (!contentOwner || !presenter || !cui::framework::TemplateAccess::RegisterContentPresenter(*contentOwner, presenter))
					return fail(L"ControlTemplate ContentPresenter 注册失败。");
			}
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner3_template_wpfAlternateButtonChrome, Border::PaddingProperty(), BindingValue(Thickness(8.f, 8.f, 8.f, 8.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner3_template_wpfAlternateButtonChrome, Control::BackgroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.909804f, 0.960784f, 0.933333f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner3_template_wpfAlternateButtonChrome, Control::BorderBrushProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.082353f, 0.588235f, 0.415686f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner3_template_wpfAlternateButtonChrome, Control::BorderThicknessProperty(), BindingValue(Thickness(2.f, 2.f, 2.f, 2.f)), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner3_template_wpfAlternateTemplateState, Label::TextProperty(), BindingValue(L"ApplyTemplate · alternate tree"), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner3_template_wpfAlternateTemplateState, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(24.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner3_template_wpfAlternateTemplateState, Control::ForegroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.082353f, 0.588235f, 0.415686f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner3_template_textBlock1, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(20.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner3_template_textBlock1, Control::ForegroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.454902f, 0.513726f, 0.6f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner3_template_textBlock1, Control::FontSizeProperty(), BindingValue(11.0), DependencyPropertyValueSource::Template);
			if (!__cuiStaticTemplateOwner3_template_wpfAlternateContentPresenter->DataBindings.AddTemplateBinding(ContentPresenter::ContentProperty(), __templateOwner, Button::ContentProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner3_template_wpfAlternateContentPresenter->DataBindings.AddTemplateBinding(ContentPresenter::ContentTemplateProperty(), __templateOwner, Button::ContentTemplateProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			__cuiStaticTemplateOwner3_template_wpfAlternateContentPresenter->SetCompiledDisplayMemberPath(static_cast<Button&>(__templateOwner).GetCompiledDisplayMemberPath());
			cui::framework::TemplateAccess::SetTemplateRoot(__templateOwner, std::move(__owned___cuiStaticTemplateOwner3_template_wpfAlternateButtonChrome));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner3_template_wpfAlternateButtonChrome, nullptr);
			__cuiStaticTemplateOwner3_template_wpfAlternateButtonChrome->SetChild(std::move(__owned___cuiStaticTemplateOwner3_template_stackPanel1));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner3_template_stackPanel1, nullptr);
			__cuiStaticTemplateOwner3_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner3_template_wpfAlternateTemplateState));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner3_template_wpfAlternateTemplateState, nullptr);
			__cuiStaticTemplateOwner3_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner3_template_textBlock1));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner3_template_textBlock1, nullptr);
			__cuiStaticTemplateOwner3_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner3_template_wpfAlternateContentPresenter));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner3_template_wpfAlternateContentPresenter, nullptr);
			{
				// CUI:AOT binding-source=direct-dp
				const bool attached = __cuiStaticTemplateOwner3_template_textBlock1->DataBindings.Add(Label::TextProperty(), cui::binding::MakeCompiledDependencyPropertySource(__templateOwner, Control::AutomationNameProperty()), BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
				if (!attached)
					return fail(L"ControlTemplate Binding 安装失败。");
			}
			if (!cui::framework::TemplateAccess::GetTemplateRoot(__templateOwner))
				return fail(L"ControlTemplate 未生成唯一视觉根。");
			if (outError) outError->clear();
			return true;
		}
		catch (const std::exception&)
		{
			return fail(L"ControlTemplate 静态构造发生运行时异常。");
		}
		catch (...)
		{
			return fail(L"ControlTemplate 静态构造发生未知异常。");
		}
	});

	__controlTemplate_WpfLabListTemplate_4->SetApplyCallback([this](Control& __templateOwner, std::wstring* outError) -> bool
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		try
		{
			std::wstring __templateThemeError;
			// __cuiStaticTemplateOwner4_template_wpfListScrollHost
			auto __owned___cuiStaticTemplateOwner4_template_wpfListScrollHost = std::make_unique<ScrollViewer>();
			auto* __cuiStaticTemplateOwner4_template_wpfListScrollHost = __owned___cuiStaticTemplateOwner4_template_wpfListScrollHost.get();
			(void)__cuiStaticTemplateOwner4_template_wpfListScrollHost->ClearPropertyValues();
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner4_template_wpfListScrollHost, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
			// __cuiStaticTemplateOwner4_template_wpfItemsPresenter
			auto __owned___cuiStaticTemplateOwner4_template_wpfItemsPresenter = std::make_unique<ItemsPresenter>();
			auto* __cuiStaticTemplateOwner4_template_wpfItemsPresenter = __owned___cuiStaticTemplateOwner4_template_wpfItemsPresenter.get();
			(void)__cuiStaticTemplateOwner4_template_wpfItemsPresenter->ClearPropertyValues();

			// Establish a fresh template namescope for this application.
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner4_template_wpfListScrollHost, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 13290996013654508663ULL }, __cuiStaticTemplateOwner4_template_wpfListScrollHost))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner4_template_wpfItemsPresenter, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 6455234130611670664ULL }, __cuiStaticTemplateOwner4_template_wpfItemsPresenter))
				return fail(L"ControlTemplate 部件注册失败。");
			{
				auto* itemsOwner = dynamic_cast<ItemsControl*>(&__templateOwner);
				auto* presenter = dynamic_cast<ItemsPresenter*>(__cuiStaticTemplateOwner4_template_wpfItemsPresenter);
				if (!itemsOwner || !presenter || !cui::framework::TemplateAccess::RegisterItemsPresenter(*itemsOwner, presenter))
					return fail(L"ControlTemplate ItemsPresenter 注册失败。");
			}
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner4_template_wpfListScrollHost, ScrollViewer::VerticalScrollBarVisibilityProperty(), BindingValue(3), DependencyPropertyValueSource::Template);
			cui::framework::TemplateAccess::SetTemplateRoot(__templateOwner, std::move(__owned___cuiStaticTemplateOwner4_template_wpfListScrollHost));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner4_template_wpfListScrollHost, nullptr);
			__cuiStaticTemplateOwner4_template_wpfListScrollHost->SetVisualContent(std::move(__owned___cuiStaticTemplateOwner4_template_wpfItemsPresenter));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner4_template_wpfItemsPresenter, nullptr);
			if (!cui::framework::TemplateAccess::GetTemplateRoot(__templateOwner))
				return fail(L"ControlTemplate 未生成唯一视觉根。");
			if (outError) outError->clear();
			return true;
		}
		catch (const std::exception&)
		{
			return fail(L"ControlTemplate 静态构造发生运行时异常。");
		}
		catch (...)
		{
			return fail(L"ControlTemplate 静态构造发生未知异常。");
		}
	});

	__controlTemplate_WpfLabListItemTemplate_5->SetApplyCallback([this](Control& __templateOwner, std::wstring* outError) -> bool
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		try
		{
			std::wstring __templateThemeError;
			// __cuiStaticTemplateOwner5_template_wpfItemChrome
			auto __owned___cuiStaticTemplateOwner5_template_wpfItemChrome = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner5_template_wpfItemChrome = __owned___cuiStaticTemplateOwner5_template_wpfItemChrome.get();
			(void)__cuiStaticTemplateOwner5_template_wpfItemChrome->ClearPropertyValues();
			// __cuiStaticTemplateOwner5_template_wpfItemContentPresenter
			auto __owned___cuiStaticTemplateOwner5_template_wpfItemContentPresenter = std::make_unique<ContentPresenter>();
			auto* __cuiStaticTemplateOwner5_template_wpfItemContentPresenter = __owned___cuiStaticTemplateOwner5_template_wpfItemContentPresenter.get();
			(void)__cuiStaticTemplateOwner5_template_wpfItemContentPresenter->ClearPropertyValues();

			// Establish a fresh template namescope for this application.
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner5_template_wpfItemChrome, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 7992450824093899917ULL }, __cuiStaticTemplateOwner5_template_wpfItemChrome))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner5_template_wpfItemContentPresenter, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 4913677920074121804ULL }, __cuiStaticTemplateOwner5_template_wpfItemContentPresenter))
				return fail(L"ControlTemplate 部件注册失败。");
			{
				auto* contentOwner = dynamic_cast<ContentControl*>(&__templateOwner);
				auto* presenter = dynamic_cast<ContentPresenter*>(__cuiStaticTemplateOwner5_template_wpfItemContentPresenter);
				if (!contentOwner || !presenter || !cui::framework::TemplateAccess::RegisterContentPresenter(*contentOwner, presenter))
					return fail(L"ControlTemplate ContentPresenter 注册失败。");
			}
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner5_template_wpfItemChrome, Control::BackgroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.f, 0.f, 0.f, 0.f}; return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner5_template_wpfItemChrome, Control::BorderBrushProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.827451f, 0.862745f, 0.909804f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner5_template_wpfItemChrome, Control::BorderThicknessProperty(), BindingValue(Thickness(1.f, 1.f, 1.f, 1.f)), DependencyPropertyValueSource::Template);
			if (!__cuiStaticTemplateOwner5_template_wpfItemChrome->DataBindings.AddTemplateBinding(Border::PaddingProperty(), __templateOwner, Control::PaddingProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner5_template_wpfItemContentPresenter->DataBindings.AddTemplateBinding(ContentPresenter::ContentProperty(), __templateOwner, ListBoxItem::ContentProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner5_template_wpfItemContentPresenter->DataBindings.AddTemplateBinding(ContentPresenter::ContentTemplateProperty(), __templateOwner, ListBoxItem::ContentTemplateProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			__cuiStaticTemplateOwner5_template_wpfItemContentPresenter->SetCompiledDisplayMemberPath(static_cast<ListBoxItem&>(__templateOwner).GetCompiledDisplayMemberPath());
			cui::framework::TemplateAccess::SetTemplateRoot(__templateOwner, std::move(__owned___cuiStaticTemplateOwner5_template_wpfItemChrome));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner5_template_wpfItemChrome, nullptr);
			__cuiStaticTemplateOwner5_template_wpfItemChrome->SetChild(std::move(__owned___cuiStaticTemplateOwner5_template_wpfItemContentPresenter));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner5_template_wpfItemContentPresenter, nullptr);
			{
				// AOT interaction program: process-static structure plus call-local values and targets.
				const BindingValue __cuiInteraction_values[] = {
					BindingValue(true),
					BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.866667f, 0.913725f, 1.f, 1.f}; return value; }())
				};
				static const CompiledInteractionPropertyOperand __cuiInteraction_property_operands[] = {
					{ 0u, DependencyPropertyReference(ListBoxItem::IsSelectedProperty()) },
					{ 1u, DependencyPropertyReference(Control::BackgroundProperty()) }
				};
				static constexpr CompiledInteractionConditionOp __cuiInteraction_conditions[] = {
					{ 0u, 0u }
				};
				static constexpr CompiledInteractionSetterOp __cuiInteraction_setters[] = {
					{ 1u, 1u }
				};
				static constexpr CompiledInteractionStateOp __cuiInteraction_states[] = {
					{ VisualStateToken{ 18369648181900045655ULL }, { 0u, 0u }, { 0u, 0u }, { 0u, 0u }, { 0u, 0u } },
					{ VisualStateToken{ 8176695819304067260ULL }, { 0u, 1u }, { 0u, 0u }, { 0u, 1u }, { 0u, 0u } }
				};
				static constexpr uint32_t __cuiInteraction_group_condition_operands[] = {
					0u
				};
				static constexpr CompiledInteractionGroupOp __cuiInteraction_groups[] = {
					{ VisualStateGroupToken{ 16180994318616346889ULL }, { 0u, 2u }, { 0u, 0u }, 0u, { 0u, 1u } }
				};
				static const CompiledInteractionProgramView __cuiInteractionProgram{
					CompiledInteractionProgramViewVersion,
					2u,
					std::span<const CompiledInteractionPropertyOperand>{ __cuiInteraction_property_operands }, // PropertyOperands
					{}, // ObjectPathChildIndices
					{}, // ObjectPaths
					std::span<const CompiledInteractionConditionOp>{ __cuiInteraction_conditions }, // Conditions
					std::span<const CompiledInteractionSetterOp>{ __cuiInteraction_setters }, // Setters
					{}, // KeyFrames
					{}, // Animations
					{}, // StateEvents
					std::span<const CompiledInteractionStateOp>{ __cuiInteraction_states }, // States
					{}, // Transitions
					std::span<const uint32_t>{ __cuiInteraction_group_condition_operands }, // GroupConditionOperands
					std::span<const CompiledInteractionGroupOp>{ __cuiInteraction_groups }, // Groups
					{}, // Storyboards
					{}, // Actions
					{} // EventTriggers
				};
				std::array<Control*, 2> __cuiInteractionTargets{
					&(__templateOwner),
					__cuiStaticTemplateOwner5_template_wpfItemChrome
				};
				std::wstring interactionError;
				if (!cui::framework::TemplateAccess::InstallCompiledInteractions(__templateOwner, __cuiInteractionProgram, std::span<const BindingValue>{ __cuiInteraction_values }, std::span<Control* const>{ __cuiInteractionTargets }, &interactionError))
					return fail(L"ControlTemplate 声明交互安装失败：" + interactionError);
			}
			if (!cui::framework::TemplateAccess::GetTemplateRoot(__templateOwner))
				return fail(L"ControlTemplate 未生成唯一视觉根。");
			if (outError) outError->clear();
			return true;
		}
		catch (const std::exception&)
		{
			return fail(L"ControlTemplate 静态构造发生运行时异常。");
		}
		catch (...)
		{
			return fail(L"ControlTemplate 静态构造发生未知异常。");
		}
	});

	// Install the native item-container Style identity and its compiled ControlTemplate before ItemsSource can realize containers.
	demoListBox->SetItemContainerStyle(L"DemoTaskContainer");
	if (demoListBox->GetItemContainerStyle() != L"DemoTaskContainer")
		throw std::runtime_error("Generated ItemContainerStyle installation failed");
	demoList->SetItemContainerStyle(L"DemoListViewContainer");
	if (demoList->GetItemContainerStyle() != L"DemoListViewContainer")
		throw std::runtime_error("Generated ItemContainerStyle installation failed");
	wpfTemplateList->SetItemContainerStyle(L"WpfLabContainerStyle");
	if (wpfTemplateList->GetItemContainerStyle() != L"WpfLabContainerStyle")
		throw std::runtime_error("Generated ItemContainerStyle installation failed");
	wpfTemplateList->SetItemContainerTemplate(ControlTemplateReference(__controlTemplate_WpfLabListItemTemplate_5));
	if (wpfTemplateList->GetItemContainerTemplate().Get() != __controlTemplate_WpfLabListItemTemplate_5.get())
		throw std::runtime_error("Generated ItemContainerTemplate installation failed");

	// Strongly typed ItemsSource resource wiring happens after item-container templates and before selection/local properties are applied.
	basicCombo->SetItemsSource(BindingListReference(__dataList_BasicChoices_1));
	if (basicCombo->GetItemsSource().Get() != __dataList_BasicChoices_1.get())
		throw std::runtime_error("Generated ItemsSource installation failed for basicCombo: " + Convert::WStringToString(basicCombo->LastTemplateError()));
	sideNavigationList->SetItemsSource(BindingListReference(__dataList_BasicChoices_1));
	if (sideNavigationList->GetItemsSource().Get() != __dataList_BasicChoices_1.get())
		throw std::runtime_error("Generated ItemsSource installation failed for sideNavigationList: " + Convert::WStringToString(sideNavigationList->LastTemplateError()));
	demoListBox->SetItemsSource(BindingListReference(__collectionView_ActiveDemoTasks_1));
	if (demoListBox->GetItemsSource().Get() != __collectionView_ActiveDemoTasks_1.get())
		throw std::runtime_error("Generated ItemsSource installation failed for demoListBox: " + Convert::WStringToString(demoListBox->LastTemplateError()));
	analyticsRows->SetItemsSource(BindingListReference(__dataList_AnalyticsRows_4));
	if (analyticsRows->GetItemsSource().Get() != __dataList_AnalyticsRows_4.get())
		throw std::runtime_error("Generated ItemsSource installation failed for analyticsRows: " + Convert::WStringToString(analyticsRows->LastTemplateError()));
	mainStatusBar->SetItemsSource(BindingListReference(__dataList_DemoStatusEntries_2));
	if (mainStatusBar->GetItemsSource().Get() != __dataList_DemoStatusEntries_2.get())
		throw std::runtime_error("Generated ItemsSource installation failed for mainStatusBar: " + Convert::WStringToString(mainStatusBar->LastTemplateError()));

	windowContent->ClearRows();
	windowContent->ClearColumns();
	windowContent->AddRow(GridLength::Pixels(28.f), 0.f, FLT_MAX);
	windowContent->AddRow(GridLength::Pixels(40.f), 0.f, FLT_MAX);
	windowContent->AddRow(GridLength::Pixels(40.f), 0.f, FLT_MAX);
	windowContent->AddRow(GridLength::Star(1.f), 0.f, FLT_MAX);
	windowContent->AddRow(GridLength::Auto(), 0.f, FLT_MAX);

	// XAML authored Local properties/resources
	mainMenu->SetHeight(cui::layout::Length::Fixed(28.f));
	Grid::SetRow(*mainMenu, 0);

	// XAML authored Local properties/resources
	menuItem1->SetHeader(BindingValue(L"文件"));

	// XAML authored Local properties/resources
	menuItem2->SetHeader(BindingValue(L"打开"));
	menuItem2->SetCommand(L"Demo.File.Open");
	menuItem2->SetCommandParameter(L"menu-open");
	menuItem2->SetInputGestureText(L"Ctrl+O");


	// XAML authored Local properties/resources
	menuItem3->SetHeader(BindingValue(L"退出"));
	menuItem3->SetCommand(L"Demo.File.Exit");
	menuItem3->SetCommandParameter(L"menu-exit");
	menuItem3->SetInputGestureText(L"Alt+F4");

	// XAML authored Local properties/resources
	menuItem4->SetHeader(BindingValue(L"帮助"));

	// XAML authored Local properties/resources
	menuItem5->SetHeader(BindingValue(L"关于 XAML 模式"));
	menuItem5->SetCommand(L"Demo.Help.About");
	menuItem5->SetCommandParameter(L"menu-about");
	menuItem5->SetInputGestureText(L"F1");

	// XAML authored Local properties/resources
	mainToolBar->SetHeight(cui::layout::Length::Fixed(40.f));
	mainToolBar->SetPadding(Thickness(6.f, 7.f, 6.f, 7.f));
	Grid::SetRow(*mainToolBar, 1);

	// XAML authored Local properties/resources
	toolBasic->SetContent(BindingValue(L"基础"));
	toolBasic->SetWidth(cui::layout::Length::Fixed(88.f));
	toolBasic->SetHeight(cui::layout::Length::Fixed(26.f));

	// XAML authored Local properties/resources
	toolData->SetContent(BindingValue(L"数据"));
	toolData->SetWidth(cui::layout::Length::Fixed(88.f));
	toolData->SetHeight(cui::layout::Length::Fixed(26.f));

	// XAML authored Local properties/resources
	toolAnalytics->SetContent(BindingValue(L"可视化"));
	toolAnalytics->SetWidth(cui::layout::Length::Fixed(88.f));
	toolAnalytics->SetHeight(cui::layout::Length::Fixed(26.f));

	// XAML authored Local properties/resources
	toolSystem->SetContent(BindingValue(L"系统"));
	toolSystem->SetWidth(cui::layout::Length::Fixed(88.f));
	toolSystem->SetHeight(cui::layout::Length::Fixed(26.f));

	// XAML authored Local properties/resources
	toolSeparator->SetWidth(cui::layout::Length::Fixed(1.f));
	toolSeparator->SetHeight(cui::layout::Length::Fixed(20.f));
	toolSeparator->SetBackground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Border_5));

	// XAML authored Local properties/resources
	toolIcon1->SetWidth(cui::layout::Length::Fixed(30.f));
	toolIcon1->SetHeight(cui::layout::Length::Fixed(26.f));
	toolIcon1->SetPadding(Thickness(5.f, 3.f, 5.f, 3.f));
	toolIcon1->SetCommand(L"Demo.File.Open");
	toolIcon1->SetCommandParameter(BindingValue(L"toolbar-open"));

	// XAML authored Local properties/resources
	toolIconImage1->SetWidth(cui::layout::Length::Fixed(18.f));
	toolIconImage1->SetHeight(cui::layout::Length::Fixed(18.f));
	toolIconImage1->SetSource(CuiGeneratedBindingValueAs<std::shared_ptr<BitmapSource>>(__documentStaticResource_DemoImage_12));
	toolIconImage1->SetStretch(static_cast<cui::drawing::ImageBrushStretch>(2));

	// XAML authored Local properties/resources
	toolIcon2->SetWidth(cui::layout::Length::Fixed(30.f));
	toolIcon2->SetHeight(cui::layout::Length::Fixed(26.f));
	toolIcon2->SetPadding(Thickness(5.f, 3.f, 5.f, 3.f));

	// XAML authored Local properties/resources
	toolIconImage2->SetWidth(cui::layout::Length::Fixed(18.f));
	toolIconImage2->SetHeight(cui::layout::Length::Fixed(18.f));
	toolIconImage2->SetSource(CuiGeneratedBindingValueAs<std::shared_ptr<BitmapSource>>(__documentStaticResource_DemoImage_12));
	toolIconImage2->SetStretch(static_cast<cui::drawing::ImageBrushStretch>(2));

	// XAML authored Local properties/resources
	toolIcon3->SetWidth(cui::layout::Length::Fixed(30.f));
	toolIcon3->SetHeight(cui::layout::Length::Fixed(26.f));
	toolIcon3->SetPadding(Thickness(5.f, 3.f, 5.f, 3.f));

	// XAML authored Local properties/resources
	toolIconImage3->SetWidth(cui::layout::Length::Fixed(18.f));
	toolIconImage3->SetHeight(cui::layout::Length::Fixed(18.f));
	toolIconImage3->SetSource(CuiGeneratedBindingValueAs<std::shared_ptr<BitmapSource>>(__documentStaticResource_DemoImage_12));
	toolIconImage3->SetStretch(static_cast<cui::drawing::ImageBrushStretch>(2));

	// XAML authored Local properties/resources
	Grid::SetRow(*canvas1, 2);

	// XAML authored Local properties/resources
	Canvas::SetLeft(*globalProgress, 12.f);
	globalProgress->SetMinimum(0.0);
	Canvas::SetTop(*globalProgress, 6.f);
	globalProgress->SetMaximum(1000.0);
	globalProgress->SetValue(250.0);
	globalProgress->SetWidth(cui::layout::Length::Fixed(310.f));
	globalProgress->SetHeight(cui::layout::Length::Fixed(28.f));

	cui::framework::StyleAccess::SetResourceKey(*statusText, L"MutedLabel", false);
	// XAML authored Local properties/resources
	statusText->SetText(L"布局来自 DemoWindow.cui.xaml，C++ 仅保留数据和业务");
	Canvas::SetLeft(*statusText, 342.f);
	Canvas::SetTop(*statusText, 11.f);
	statusText->SetWidth(cui::layout::Length::Fixed(500.f));
	statusText->SetHeight(cui::layout::Length::Fixed(24.f));
	statusText->SetForeground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.454902f, 0.513726f, 0.6f, 1.f}; return value; }());

	cui::framework::StyleAccess::SetResourceKey(*runtimeBadge, L"ImageText", false);
	// XAML authored Local properties/resources
	runtimeBadge->SetText(L"AOT XAML · Native C++");
	Canvas::SetLeft(*runtimeBadge, 1050.f);
	Canvas::SetTop(*runtimeBadge, 8.f);
	runtimeBadge->SetWidth(cui::layout::Length::Fixed(330.f));
	runtimeBadge->SetHeight(cui::layout::Length::Fixed(26.f));

	// XAML authored Local properties/resources
	mainTabs->SetTabStripPlacement(static_cast<Dock>(1));
	mainTabs->SetMargin(Thickness(10.f, 0.f, 10.f, 10.f));
	Grid::SetRow(*mainTabs, 3);
	mainTabs->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.f, 0.f, 0.f, 0.f}; return value; }());

	// XAML authored Local properties/resources
	tabItem1->SetIsSelected(true);
	tabItem1->SetHeader(BindingValue(L"基础控件"));

	cui::framework::StyleAccess::SetResourceKey(*border1, L"SurfacePanel", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border1, 10.f);
	Canvas::SetTop(*border1, 36.f);
	border1->SetWidth(cui::layout::Length::Fixed(1325.f));
	border1->SetHeight(cui::layout::Length::Fixed(520.f));


	cui::framework::StyleAccess::SetResourceKey(*basicTitle, L"GradientHeader", false);
	// XAML authored Local properties/resources
	basicTitle->SetText(L"基础输入与事件：声明在 XAML，处理函数注册在 C++");
	Canvas::SetLeft(*basicTitle, 12.f);
	Canvas::SetTop(*basicTitle, 10.f);
	basicTitle->SetWidth(cui::layout::Length::Fixed(600.f));
	basicTitle->SetHeight(cui::layout::Length::Fixed(26.f));
	basicTitle->SetFontFamily(L"Arial");
	basicTitle->SetFontSize(18.0);

	// XAML authored Local properties/resources
	frameworkThemeHint->SetText(L"Generic.xaml：Button / CheckBox / RadioButton / ProgressBar / Slider 默认模板");
	Canvas::SetLeft(*frameworkThemeHint, 630.f);
	Canvas::SetTop(*frameworkThemeHint, 12.f);
	frameworkThemeHint->SetWidth(cui::layout::Length::Fixed(650.f));
	frameworkThemeHint->SetHeight(cui::layout::Length::Fixed(24.f));
	frameworkThemeHint->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	cui::framework::StyleAccess::SetResourceKey(*basicButton, L"PrimaryButton", false);
	// XAML authored Local properties/resources
	basicButton->SetContent(BindingValue(L"Enter · IsDefault"));
	Canvas::SetLeft(*basicButton, 14.f);
	Canvas::SetTop(*basicButton, 54.f);
	basicButton->SetWidth(cui::layout::Length::Fixed(150.f));
	basicButton->SetHeight(cui::layout::Length::Fixed(34.f));
	basicButton->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.184314f, 0.435294f, 0.894118f, 1.f}; return value; }());
	basicButton->SetForeground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{1.f, 1.f, 1.f, 1.f}; return value; }());
	basicButton->SetIsDefault(true);

	// XAML authored Local properties/resources
	enableInput->SetContent(BindingValue(L"启用输入框"));
	Canvas::SetLeft(*enableInput, 185.f);
	Canvas::SetTop(*enableInput, 60.f);
	enableInput->SetWidth(cui::layout::Length::Fixed(150.f));
	enableInput->SetHeight(cui::layout::Length::Fixed(26.f));
	enableInput->SetIsChecked(NullableBool(true));

	// XAML authored Local properties/resources
	radioA->SetContent(BindingValue(L"方案 A"));
	Canvas::SetLeft(*radioA, 14.f);
	Canvas::SetTop(*radioA, 108.f);
	radioA->SetWidth(cui::layout::Length::Fixed(110.f));
	radioA->SetHeight(cui::layout::Length::Fixed(26.f));
	radioA->SetIsChecked(NullableBool(true));
	radioA->SetGroupName(L"Plan");

	// XAML authored Local properties/resources
	radioB->SetContent(BindingValue(L"方案 B"));
	Canvas::SetLeft(*radioB, 130.f);
	Canvas::SetTop(*radioB, 108.f);
	radioB->SetWidth(cui::layout::Length::Fixed(110.f));
	radioB->SetHeight(cui::layout::Length::Fixed(26.f));
	radioB->SetGroupName(L"Plan");

	// XAML authored Local properties/resources
	nameInput->SetText(L"可编辑文本");
	Canvas::SetLeft(*nameInput, 14.f);
	Canvas::SetTop(*nameInput, 154.f);
	nameInput->SetWidth(cui::layout::Length::Fixed(260.f));
	nameInput->SetHeight(cui::layout::Length::Fixed(30.f));

	// XAML authored Local properties/resources
	passwordInput->SetPassword(L"cui-xaml");
	Canvas::SetLeft(*passwordInput, 14.f);
	Canvas::SetTop(*passwordInput, 196.f);
	passwordInput->SetWidth(cui::layout::Length::Fixed(260.f));
	passwordInput->SetHeight(cui::layout::Length::Fixed(30.f));

	// XAML authored Local properties/resources
	basicCombo->SetSelectedIndex(0);
	Canvas::SetLeft(*basicCombo, 14.f);
	Canvas::SetTop(*basicCombo, 238.f);
	basicCombo->SetWidth(cui::layout::Length::Fixed(260.f));
	basicCombo->SetHeight(cui::layout::Length::Fixed(30.f));

	// XAML authored Local properties/resources
	dateInput->SetText(L"2026-07-23");
	Canvas::SetLeft(*dateInput, 14.f);
	Canvas::SetTop(*dateInput, 280.f);
	dateInput->SetWidth(cui::layout::Length::Fixed(260.f));
	dateInput->SetHeight(cui::layout::Length::Fixed(30.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*numberInput, 14.f);
	numberInput->SetMinimum(0.0);
	Canvas::SetTop(*numberInput, 322.f);
	numberInput->SetMaximum(100.0);
	numberInput->SetValue(42.0);
	numberInput->SetWidth(cui::layout::Length::Fixed(160.f));
	numberInput->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	dialogCancelButton->SetContent(BindingValue(L"Escape · IsCancel"));
	Canvas::SetLeft(*dialogCancelButton, 185.f);
	Canvas::SetTop(*dialogCancelButton, 322.f);
	dialogCancelButton->SetWidth(cui::layout::Length::Fixed(155.f));
	dialogCancelButton->SetHeight(cui::layout::Length::Fixed(30.f));
	dialogCancelButton->SetIsCancel(true);

	// XAML authored Local properties/resources
	docsLink->SetContent(BindingValue(L"CUI XAML 运行时文档"));
	Canvas::SetLeft(*docsLink, 14.f);
	Canvas::SetTop(*docsLink, 368.f);
	docsLink->SetWidth(cui::layout::Length::Fixed(300.f));
	docsLink->SetHeight(cui::layout::Length::Fixed(26.f));

	// XAML authored Local properties/resources
	textBlock1->SetText(L"Vertical");
	Canvas::SetLeft(*textBlock1, 288.f);
	Canvas::SetTop(*textBlock1, 148.f);
	textBlock1->SetWidth(cui::layout::Length::Fixed(72.f));
	textBlock1->SetHeight(cui::layout::Length::Fixed(22.f));
	textBlock1->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	// XAML authored Local properties/resources
	verticalThemeSlider->SetOrientation(static_cast<Orientation>(1));
	Canvas::SetLeft(*verticalThemeSlider, 298.f);
	verticalThemeSlider->SetMinimum(0.0);
	Canvas::SetTop(*verticalThemeSlider, 174.f);
	verticalThemeSlider->SetMaximum(100.0);
	verticalThemeSlider->SetValue(65.0);
	verticalThemeSlider->SetWidth(cui::layout::Length::Fixed(32.f));
	verticalThemeSlider->SetHeight(cui::layout::Length::Fixed(230.f));

	// XAML authored Local properties/resources
	verticalThemeProgress->SetOrientation(static_cast<Orientation>(1));
	Canvas::SetLeft(*verticalThemeProgress, 340.f);
	verticalThemeProgress->SetMinimum(0.0);
	Canvas::SetTop(*verticalThemeProgress, 174.f);
	verticalThemeProgress->SetMaximum(100.0);
	verticalThemeProgress->SetValue(65.0);
	verticalThemeProgress->SetWidth(cui::layout::Length::Fixed(18.f));
	verticalThemeProgress->SetHeight(cui::layout::Length::Fixed(230.f));

	// XAML authored Local properties/resources
	gradientInput->SetText(L"XAML 画刷输入框：GradientInput");
	Canvas::SetLeft(*gradientInput, 370.f);
	Canvas::SetTop(*gradientInput, 54.f);
	gradientInput->SetWidth(cui::layout::Length::Fixed(360.f));
	gradientInput->SetHeight(cui::layout::Length::Fixed(42.f));
	gradientInput->SetVerticalContentAlignment(static_cast<::VerticalAlignment>(1));
	gradientInput->SetForeground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::LinearGradient; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.StartPoint = D2D1::Point2F(0.f, 0.f); value.EndPoint = D2D1::Point2F(1.f, 1.f); value.GradientStops.push_back({ 0.f, D2D1_COLOR_F{0.890196f, 0.035294f, 0.25098f, 1.f} }); value.GradientStops.push_back({ 0.33f, D2D1_COLOR_F{0.905882f, 0.843137f, 0.007843f, 1.f} }); value.GradientStops.push_back({ 0.66f, D2D1_COLOR_F{0.058824f, 0.658824f, 0.584314f, 1.f} }); value.GradientStops.push_back({ 1.f, D2D1_COLOR_F{0.07451f, 0.45098f, 0.909804f, 1.f} }); return value; }());

	cui::framework::StyleAccess::SetResourceKey(*gradientLabel, L"ResourceGeometryVisual", false);
	// XAML authored Local properties/resources
	gradientLabel->SetText(L"自定义控件的画刷、裁剪与变换均由 XAML 表达");
	Canvas::SetLeft(*gradientLabel, 370.f);
	Canvas::SetTop(*gradientLabel, 112.f);
	gradientLabel->SetWidth(cui::layout::Length::Fixed(440.f));
	gradientLabel->SetHeight(cui::layout::Length::Fixed(34.f));
	gradientLabel->SetPadding(Thickness(12.f, 0.f, 12.f, 0.f));
	gradientLabel->SetForeground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::RadialGradient; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Center = D2D1::Point2F(0.5f, 0.5f); value.GradientOrigin = D2D1::Point2F(0.2f, 0.2f); value.RadiusX = 0.7f; value.RadiusY = 0.9f; value.GradientStops.push_back({ 0.f, D2D1_COLOR_F{0.890196f, 0.035294f, 0.25098f, 1.f} }); value.GradientStops.push_back({ 0.33f, D2D1_COLOR_F{0.905882f, 0.843137f, 0.007843f, 1.f} }); value.GradientStops.push_back({ 0.66f, D2D1_COLOR_F{0.058824f, 0.658824f, 0.584314f, 1.f} }); value.GradientStops.push_back({ 1.f, D2D1_COLOR_F{0.07451f, 0.45098f, 0.909804f, 1.f} }); return value; }());
	gradientLabel->SetRenderTransformOriginDip(cui::core::Point{ 0.5f, 0.5f });

	// XAML authored Local properties/resources
	Canvas::SetLeft(*featureCard, 370.f);
	Canvas::SetTop(*featureCard, 170.f);
	featureCard->SetWidth(cui::layout::Length::Fixed(440.f));
	featureCard->SetHeight(cui::layout::Length::Fixed(270.f));
	featureCard->SetAccentColor(CuiGeneratedBindingValueAs<D2D1_COLOR_F>(__documentStaticResource_Accent_2));
	featureCard->SetCaption(L"FeatureCard · 类型/属性/事件均来自 XAML");
	featureCard->SetContentPadding(Thickness(12.f, 12.f, 12.f, 12.f));
	featureCard->SetIsActive(true);

	// XAML authored Local properties/resources
	featureCardContent->SetWidth(cui::layout::Length::Fixed(395.f));
	featureCardContent->SetHeight(cui::layout::Length::Fixed(42.f));

	// XAML authored Local properties/resources
	featureActionA->SetContent(BindingValue(L"切换 ClassCommand CanExecute"));
	featureActionA->SetWidth(cui::layout::Length::Fixed(240.f));
	featureActionA->SetHeight(cui::layout::Length::Fixed(26.f));
	featureActionA->SetPadding(Thickness(4.f, 3.f, 4.f, 3.f));

	// XAML authored Local properties/resources
	featureActionB->SetContent(BindingValue(L"QName class binding"));
	featureActionB->SetWidth(cui::layout::Length::Fixed(170.f));
	featureActionB->SetHeight(cui::layout::Length::Fixed(26.f));
	featureActionB->SetPadding(Thickness(4.f, 3.f, 4.f, 3.f));

	// XAML authored Local properties/resources
	basicGroup->SetHeader(BindingValue(L"GroupBox · 用户设置"));
	Canvas::SetLeft(*basicGroup, 850.f);
	Canvas::SetTop(*basicGroup, 48.f);
	basicGroup->SetWidth(cui::layout::Length::Fixed(430.f));
	basicGroup->SetHeight(cui::layout::Length::Fixed(180.f));


	// XAML authored Local properties/resources
	groupHint->SetText(L"这些子控件的所有权同样来自 XAML 层级");
	Canvas::SetLeft(*groupHint, 18.f);
	Canvas::SetTop(*groupHint, 12.f);
	groupHint->SetWidth(cui::layout::Length::Fixed(350.f));
	groupHint->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	groupName->SetText(L"XAML User");
	Canvas::SetLeft(*groupName, 18.f);
	Canvas::SetTop(*groupName, 50.f);
	groupName->SetWidth(cui::layout::Length::Fixed(260.f));
	groupName->SetHeight(cui::layout::Length::Fixed(28.f));

	// XAML authored Local properties/resources
	groupEnabled->SetContent(BindingValue(L"启用高级选项"));
	Canvas::SetLeft(*groupEnabled, 18.f);
	Canvas::SetTop(*groupEnabled, 92.f);
	groupEnabled->SetWidth(cui::layout::Length::Fixed(220.f));
	groupEnabled->SetHeight(cui::layout::Length::Fixed(26.f));
	groupEnabled->SetIsChecked(NullableBool(true));

	// XAML authored Local properties/resources
	themeNormalButton->SetContent(BindingValue(L"Generic · hover/press"));
	Canvas::SetLeft(*themeNormalButton, 18.f);
	Canvas::SetTop(*themeNormalButton, 124.f);
	themeNormalButton->SetWidth(cui::layout::Length::Fixed(190.f));
	themeNormalButton->SetHeight(cui::layout::Length::Fixed(28.f));

	// XAML authored Local properties/resources
	themeDisabledButton->SetIsEnabled(false);
	themeDisabledButton->SetContent(BindingValue(L"Disabled state"));
	Canvas::SetLeft(*themeDisabledButton, 210.f);
	Canvas::SetTop(*themeDisabledButton, 124.f);
	themeDisabledButton->SetWidth(cui::layout::Length::Fixed(170.f));
	themeDisabledButton->SetHeight(cui::layout::Length::Fixed(28.f));

	// XAML authored Local properties/resources
	basicExpander->SetHeader(BindingValue(L"展开：运行时能力说明"));
	Canvas::SetLeft(*basicExpander, 850.f);
	Canvas::SetTop(*basicExpander, 250.f);
	basicExpander->SetWidth(cui::layout::Length::Fixed(430.f));
	basicExpander->SetIsExpanded(true);

	// XAML authored Local properties/resources
	basicExpanderContent->SetHeight(cui::layout::Length::Fixed(144.f));

	// XAML authored Local properties/resources
	expanderText->SetText(L"XAML 负责结构、属性、样式和事件名；C++ 负责真实业务对象与函数实现。");
	Canvas::SetLeft(*expanderText, 16.f);
	Canvas::SetTop(*expanderText, 24.f);
	expanderText->SetWidth(cui::layout::Length::Fixed(380.f));
	expanderText->SetHeight(cui::layout::Length::Fixed(80.f));

	// XAML authored Local properties/resources
	themeContentControlProbe->SetVisibility(Visibility::Collapsed);
	themeContentControlProbe->SetContent(BindingValue(L"ContentControl theme probe"));

	// XAML authored Local properties/resources
	themeItemsControlProbe->SetVisibility(Visibility::Collapsed);

	// XAML authored Local properties/resources
	textBlock2->SetText(L"ItemsControl theme probe");

	// XAML authored Local properties/resources
	themeSeparatorProbe->SetVisibility(Visibility::Collapsed);

	// XAML authored Local properties/resources
	tabItem2->SetHeader(BindingValue(L"容器与图像"));

	cui::framework::StyleAccess::SetResourceKey(*border2, L"SurfacePanel", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border2, 10.f);
	Canvas::SetTop(*border2, 36.f);
	border2->SetWidth(cui::layout::Length::Fixed(1325.f));
	border2->SetHeight(cui::layout::Length::Fixed(520.f));


	cui::framework::StyleAccess::SetResourceKey(*openImageButton, L"PrimaryButton", false);
	// XAML authored Local properties/resources
	openImageButton->SetContent(BindingValue(L"打开图片"));
	Canvas::SetLeft(*openImageButton, 14.f);
	Canvas::SetTop(*openImageButton, 14.f);
	openImageButton->SetWidth(cui::layout::Length::Fixed(150.f));
	openImageButton->SetHeight(cui::layout::Length::Fixed(32.f));
	openImageButton->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.184314f, 0.435294f, 0.894118f, 1.f}; return value; }());
	openImageButton->SetForeground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{1.f, 1.f, 1.f, 1.f}; return value; }());

	cui::framework::StyleAccess::SetResourceKey(*demoImage, L"ResourceImage", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*demoImage, 14.f);
	Canvas::SetTop(*demoImage, 60.f);
	demoImage->SetWidth(cui::layout::Length::Fixed(430.f));
	demoImage->SetHeight(cui::layout::Length::Fixed(300.f));
	demoImage->SetAllowDrop(true);

	// XAML authored Local properties/resources
	Canvas::SetLeft(*demoProgress, 14.f);
	Canvas::SetTop(*demoProgress, 384.f);
	demoProgress->SetMaximum(1.0);
	demoProgress->SetValue(0.5);
	demoProgress->SetWidth(cui::layout::Length::Fixed(430.f));
	demoProgress->SetHeight(cui::layout::Length::Fixed(26.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*indeterminateProgress, 14.f);
	Canvas::SetTop(*indeterminateProgress, 424.f);
	indeterminateProgress->SetWidth(cui::layout::Length::Fixed(430.f));
	indeterminateProgress->SetHeight(cui::layout::Length::Fixed(18.f));
	indeterminateProgress->SetIsIndeterminate(true);

	// XAML authored Local properties/resources
	Canvas::SetLeft(*loadingRing, 480.f);
	Canvas::SetTop(*loadingRing, 52.f);
	loadingRing->SetWidth(cui::layout::Length::Fixed(64.f));
	loadingRing->SetHeight(cui::layout::Length::Fixed(64.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*progressRing, 570.f);
	progressRing->SetMinimum(0.0);
	Canvas::SetTop(*progressRing, 38.f);
	progressRing->SetMaximum(100.0);
	progressRing->SetValue(50.0);
	progressRing->SetWidth(cui::layout::Length::Fixed(96.f));
	progressRing->SetHeight(cui::layout::Length::Fixed(96.f));
	progressRing->SetFontFamily(L"Segoe UI");
	progressRing->SetFontSize(16.0);

	// XAML authored Local properties/resources
	Canvas::SetLeft(*imageVisible, 480.f);
	Canvas::SetTop(*imageVisible, 154.f);
	imageVisible->SetWidth(cui::layout::Length::Fixed(64.f));
	imageVisible->SetHeight(cui::layout::Length::Fixed(30.f));
	imageVisible->SetIsChecked(NullableBool(true));

	// XAML authored Local properties/resources
	imageVisibleLabel->SetText(L"Image 可见");
	Canvas::SetLeft(*imageVisibleLabel, 552.f);
	Canvas::SetTop(*imageVisibleLabel, 158.f);
	imageVisibleLabel->SetWidth(cui::layout::Length::Fixed(190.f));
	imageVisibleLabel->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*demoScene, 480.f);
	Canvas::SetTop(*demoScene, 220.f);
	demoScene->SetWidth(cui::layout::Length::Fixed(200.f));
	demoScene->SetHeight(cui::layout::Length::Fixed(190.f));
	demoScene->SetBehaviorKey(L"DemoScene");
	demoScene->SetPlaceholderText(L"DemoScene behavior 未注册");
	demoScene->SetAutomationName(L"NativeSurface C++ render behavior");

	// XAML authored Local properties/resources
	Canvas::SetLeft(*detailGrid, 720.f);
	Canvas::SetTop(*detailGrid, 14.f);
	detailGrid->SetWidth(cui::layout::Length::Fixed(570.f));
	detailGrid->SetHeight(cui::layout::Length::Fixed(270.f));
	detailGrid->ClearRows();
	detailGrid->ClearColumns();
	detailGrid->AddColumn(GridLength::Pixels(180.f), 0.f, FLT_MAX);
	detailGrid->AddColumn(GridLength::Pixels(8.f), 0.f, FLT_MAX);
	detailGrid->AddColumn(GridLength::Star(1.f), 0.f, FLT_MAX);

	// XAML authored Local properties/resources
	navigationComposition->SetWidth(cui::layout::Length::Fixed(180.f));
	navigationComposition->SetHeight(cui::layout::Length::Fixed(248.f));
	Grid::SetColumn(*navigationComposition, 0);

	// XAML authored Local properties/resources
	textBlock3->SetText(L"工作区");
	Canvas::SetLeft(*textBlock3, 10.f);
	Canvas::SetTop(*textBlock3, 10.f);
	textBlock3->SetWidth(cui::layout::Length::Fixed(150.f));
	textBlock3->SetHeight(cui::layout::Length::Fixed(24.f));
	textBlock3->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	// XAML authored Local properties/resources
	sideNavigationList->SetSelectedIndex(1);
	Canvas::SetLeft(*sideNavigationList, 10.f);
	Canvas::SetTop(*sideNavigationList, 42.f);
	sideNavigationList->SetWidth(cui::layout::Length::Fixed(160.f));
	sideNavigationList->SetHeight(cui::layout::Length::Fixed(186.f));

	// XAML authored Local properties/resources
	canvas2->SetWidth(cui::layout::Length::Fixed(8.f));
	canvas2->SetHeight(cui::layout::Length::Fixed(248.f));
	Grid::SetColumn(*canvas2, 1);
	canvas2->SetBackground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Border_5));

	// XAML authored Local properties/resources
	detailComposition->SetWidth(cui::layout::Length::Fixed(382.f));
	detailComposition->SetHeight(cui::layout::Length::Fixed(248.f));
	Grid::SetColumn(*detailComposition, 2);

	// XAML authored Local properties/resources
	Canvas::SetLeft(*stackPanel1, 14.f);
	stackPanel1->SetOrientation(static_cast<Orientation>(0));
	Canvas::SetTop(*stackPanel1, 16.f);
	stackPanel1->SetWidth(cui::layout::Length::Fixed(330.f));
	stackPanel1->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	textBlock4->SetText(L"应用");
	textBlock4->SetWidth(cui::layout::Length::Fixed(46.f));
	textBlock4->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	textBlock5->SetText(L"›");
	textBlock5->SetWidth(cui::layout::Length::Fixed(16.f));
	textBlock5->SetHeight(cui::layout::Length::Fixed(24.f));
	textBlock5->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	// XAML authored Local properties/resources
	textBlock6->SetText(L"资源");
	textBlock6->SetWidth(cui::layout::Length::Fixed(46.f));
	textBlock6->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	textBlock7->SetText(L"›");
	textBlock7->SetWidth(cui::layout::Length::Fixed(16.f));
	textBlock7->SetHeight(cui::layout::Length::Fixed(24.f));
	textBlock7->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	// XAML authored Local properties/resources
	textBlock8->SetText(L"详情");
	textBlock8->SetWidth(cui::layout::Length::Fixed(46.f));
	textBlock8->SetHeight(cui::layout::Length::Fixed(24.f));
	textBlock8->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));

	// XAML authored Local properties/resources
	splitNotes->SetText(L"导航与面包屑现在由 Grid、ListBox、StackPanel、TextBlock 和数据资源组合；不再由高层原生控件私有绘制。");
	Canvas::SetLeft(*splitNotes, 14.f);
	Canvas::SetTop(*splitNotes, 62.f);
	splitNotes->SetWidth(cui::layout::Length::Fixed(340.f));
	splitNotes->SetHeight(cui::layout::Length::Fixed(160.f));

	// XAML authored Local properties/resources
	containerGroup->SetHeader(BindingValue(L"组合容器"));
	Canvas::SetLeft(*containerGroup, 720.f);
	Canvas::SetTop(*containerGroup, 310.f);
	containerGroup->SetWidth(cui::layout::Length::Fixed(570.f));
	containerGroup->SetHeight(cui::layout::Length::Fixed(150.f));

	// XAML authored Local properties/resources
	containerGroupText->SetText(L"编辑 DemoWindow.cui.xaml 后重新启动，可直接观察声明式容器、模板和路由事件行为。");
	Canvas::SetLeft(*containerGroupText, 18.f);
	Canvas::SetTop(*containerGroupText, 32.f);
	containerGroupText->SetWidth(cui::layout::Length::Fixed(500.f));
	containerGroupText->SetHeight(cui::layout::Length::Fixed(50.f));

	// XAML authored Local properties/resources
	tabItem3->SetHeader(BindingValue(L"数据控件"));

	cui::framework::StyleAccess::SetResourceKey(*border3, L"SurfacePanel", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border3, 10.f);
	Canvas::SetTop(*border3, 36.f);
	border3->SetWidth(cui::layout::Length::Fixed(1325.f));
	border3->SetHeight(cui::layout::Length::Fixed(520.f));


	// XAML authored Local properties/resources
	Canvas::SetLeft(*demoTree, 12.f);
	Canvas::SetTop(*demoTree, 14.f);
	demoTree->SetWidth(cui::layout::Length::Fixed(210.f));
	demoTree->SetHeight(cui::layout::Length::Fixed(210.f));

	// XAML authored Local properties/resources
	demoListBox->SetSelectedIndex(0);
	demoListBox->SetIsSynchronizedWithCurrentItem(true);
	Canvas::SetLeft(*demoListBox, 238.f);
	Canvas::SetTop(*demoListBox, 14.f);
	demoListBox->SetWidth(cui::layout::Length::Fixed(210.f));
	demoListBox->SetHeight(cui::layout::Length::Fixed(210.f));

	// XAML authored Local properties/resources
	demoList->SetSelectedIndex(0);
	Canvas::SetLeft(*demoList, 12.f);
	Canvas::SetTop(*demoList, 242.f);
	demoList->SetWidth(cui::layout::Length::Fixed(436.f));
	demoList->SetHeight(cui::layout::Length::Fixed(230.f));

	// XAML authored Local properties/resources
	composedPropertyEditor->SetHeader(BindingValue(L"XAML 组合属性编辑器"));
	Canvas::SetLeft(*composedPropertyEditor, 478.f);
	Canvas::SetTop(*composedPropertyEditor, 14.f);
	composedPropertyEditor->SetWidth(cui::layout::Length::Fixed(560.f));
	composedPropertyEditor->SetHeight(cui::layout::Length::Fixed(458.f));


	// XAML authored Local properties/resources
	textBlock9->SetText(L"标题");
	Canvas::SetLeft(*textBlock9, 18.f);
	Canvas::SetTop(*textBlock9, 26.f);
	textBlock9->SetWidth(cui::layout::Length::Fixed(120.f));

	// XAML authored Local properties/resources
	composedTitleEditor->SetText(L"由基础控件组合，不再使用原生 PropertyGrid");
	Canvas::SetLeft(*composedTitleEditor, 156.f);
	Canvas::SetTop(*composedTitleEditor, 20.f);
	composedTitleEditor->SetWidth(cui::layout::Length::Fixed(360.f));
	composedTitleEditor->SetHeight(cui::layout::Length::Fixed(30.f));

	// XAML authored Local properties/resources
	textBlock10->SetText(L"启用");
	Canvas::SetLeft(*textBlock10, 18.f);
	Canvas::SetTop(*textBlock10, 76.f);
	textBlock10->SetWidth(cui::layout::Length::Fixed(120.f));

	// XAML authored Local properties/resources
	composedEnabledEditor->SetContent(BindingValue(L"启用当前配置"));
	Canvas::SetLeft(*composedEnabledEditor, 156.f);
	Canvas::SetTop(*composedEnabledEditor, 72.f);
	composedEnabledEditor->SetIsChecked(NullableBool(true));

	// XAML authored Local properties/resources
	textBlock11->SetText(L"密度");
	Canvas::SetLeft(*textBlock11, 18.f);
	Canvas::SetTop(*textBlock11, 126.f);
	textBlock11->SetWidth(cui::layout::Length::Fixed(120.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*composedDensityEditor, 156.f);
	composedDensityEditor->SetMaxDropDownHeight(180.f);
	Canvas::SetTop(*composedDensityEditor, 118.f);
	composedDensityEditor->SetWidth(cui::layout::Length::Fixed(220.f));
	composedDensityEditor->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	comboBoxItem1->SetContent(BindingValue(L"紧凑"));

	// XAML authored Local properties/resources
	comboBoxItem2->SetContent(BindingValue(L"舒适"));
	comboBoxItem2->SetIsSelected(true);

	// XAML authored Local properties/resources
	comboBoxItem3->SetContent(BindingValue(L"宽松"));

	// XAML authored Local properties/resources
	textBlock12->SetText(L"缩放");
	Canvas::SetLeft(*textBlock12, 18.f);
	Canvas::SetTop(*textBlock12, 180.f);
	textBlock12->SetWidth(cui::layout::Length::Fixed(120.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*composedScaleEditor, 156.f);
	composedScaleEditor->SetMinimum(50.0);
	Canvas::SetTop(*composedScaleEditor, 170.f);
	composedScaleEditor->SetMaximum(200.0);
	composedScaleEditor->SetValue(100.0);
	composedScaleEditor->SetWidth(cui::layout::Length::Fixed(300.f));
	composedScaleEditor->SetHeight(cui::layout::Length::Fixed(36.f));
	composedScaleEditor->SetTickFrequency(10.0);

	// XAML authored Local properties/resources
	textBlock13->SetText(L"这一块故意展示 WPF 路径：内容控件 + 布局容器 + 标准编辑器。业务属性面板应由 XAML 模板和绑定组合，而不是原生逐行绘制。");
	Canvas::SetLeft(*textBlock13, 18.f);
	Canvas::SetTop(*textBlock13, 232.f);
	textBlock13->SetWidth(cui::layout::Length::Fixed(490.f));
	textBlock13->SetHeight(cui::layout::Length::Fixed(76.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*authoredStateTree, 1055.f);
	Canvas::SetTop(*authoredStateTree, 14.f);
	authoredStateTree->SetWidth(cui::layout::Length::Fixed(250.f));
	authoredStateTree->SetHeight(cui::layout::Length::Fixed(300.f));

	// XAML authored Local properties/resources
	treeViewItem1->SetIsExpanded(true);
	treeViewItem1->SetHeader(BindingValue(L"Workspace"));

	// XAML authored Local properties/resources
	treeViewItem2->SetIsSelected(true);
	treeViewItem2->SetHeader(BindingValue(L"src"));

	// XAML authored Local properties/resources
	treeViewItem3->SetHeader(BindingValue(L"tests"));

	// XAML authored Local properties/resources
	treeViewItem4->SetHeader(BindingValue(L"Artifacts"));

	// XAML authored Local properties/resources
	textBlock14->SetText(L"作者态 TreeViewItem.IsExpanded / IsSelected 与 Selector 容器状态均来自 XAML。交互更新走 SetCurrentValue，保留 Binding、Style 与 Local 来源身份。");
	Canvas::SetLeft(*textBlock14, 1055.f);
	Canvas::SetTop(*textBlock14, 330.f);
	textBlock14->SetWidth(cui::layout::Length::Fixed(250.f));
	textBlock14->SetHeight(cui::layout::Length::Fixed(122.f));

	// XAML authored Local properties/resources
	tabItem4->SetHeader(BindingValue(L"数据可视化"));

	cui::framework::StyleAccess::SetResourceKey(*border4, L"SurfacePanel", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border4, 10.f);
	Canvas::SetTop(*border4, 36.f);
	border4->SetWidth(cui::layout::Length::Fixed(1325.f));
	border4->SetHeight(cui::layout::Length::Fixed(520.f));


	// XAML authored Local properties/resources
	Canvas::SetLeft(*border5, 12.f);
	Canvas::SetTop(*border5, 12.f);
	border5->SetWidth(cui::layout::Length::Fixed(1280.f));
	border5->SetHeight(cui::layout::Length::Fixed(48.f));
	border5->SetBackground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_SurfaceSoft_4));
	border5->SetBorderBrush(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Border_5));
	border5->SetBorderThickness(Thickness(1.f, 1.f, 1.f, 1.f));


	// XAML authored Local properties/resources
	analyticsQuery->SetText(L"华东");
	Canvas::SetLeft(*analyticsQuery, 10.f);
	Canvas::SetTop(*analyticsQuery, 8.f);
	analyticsQuery->SetWidth(cui::layout::Length::Fixed(250.f));
	analyticsQuery->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	analyticsClosed->SetContent(BindingValue(L"已成交"));
	Canvas::SetLeft(*analyticsClosed, 278.f);
	Canvas::SetTop(*analyticsClosed, 10.f);
	analyticsClosed->SetWidth(cui::layout::Length::Fixed(92.f));
	analyticsClosed->SetHeight(cui::layout::Length::Fixed(28.f));
	analyticsClosed->SetIsChecked(NullableBool(true));

	// XAML authored Local properties/resources
	analyticsContract->SetContent(BindingValue(L"合同中"));
	Canvas::SetLeft(*analyticsContract, 378.f);
	Canvas::SetTop(*analyticsContract, 10.f);
	analyticsContract->SetWidth(cui::layout::Length::Fixed(92.f));
	analyticsContract->SetHeight(cui::layout::Length::Fixed(28.f));

	// XAML authored Local properties/resources
	analyticsHighMargin->SetContent(BindingValue(L"高毛利"));
	Canvas::SetLeft(*analyticsHighMargin, 478.f);
	Canvas::SetTop(*analyticsHighMargin, 10.f);
	analyticsHighMargin->SetWidth(cui::layout::Length::Fixed(92.f));
	analyticsHighMargin->SetHeight(cui::layout::Length::Fixed(28.f));

	// XAML authored Local properties/resources
	analyticsApply->SetContent(BindingValue(L"应用"));
	Canvas::SetLeft(*analyticsApply, 1050.f);
	Canvas::SetTop(*analyticsApply, 8.f);
	analyticsApply->SetWidth(cui::layout::Length::Fixed(96.f));
	analyticsApply->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	analyticsReset->SetContent(BindingValue(L"重置"));
	Canvas::SetLeft(*analyticsReset, 1156.f);
	Canvas::SetTop(*analyticsReset, 8.f);
	analyticsReset->SetWidth(cui::layout::Length::Fixed(96.f));
	analyticsReset->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	groupBox1->SetHeader(BindingValue(L"成交额"));
	Canvas::SetLeft(*groupBox1, 12.f);
	Canvas::SetTop(*groupBox1, 76.f);
	groupBox1->SetWidth(cui::layout::Length::Fixed(200.f));
	groupBox1->SetHeight(cui::layout::Length::Fixed(100.f));

	// XAML authored Local properties/resources
	canvas4->SetWidth(cui::layout::Length::Fixed(176.f));
	canvas4->SetHeight(cui::layout::Length::Fixed(64.f));

	// XAML authored Local properties/resources
	textBlock15->SetText(L"1,870.5 万");
	Canvas::SetLeft(*textBlock15, 10.f);
	Canvas::SetTop(*textBlock15, 4.f);
	textBlock15->SetWidth(cui::layout::Length::Fixed(150.f));
	textBlock15->SetHeight(cui::layout::Length::Fixed(32.f));
	textBlock15->SetFontSize(22.0);

	// XAML authored Local properties/resources
	textBlock16->SetText(L"+18.4% · 较上期");
	Canvas::SetLeft(*textBlock16, 10.f);
	Canvas::SetTop(*textBlock16, 38.f);
	textBlock16->SetWidth(cui::layout::Length::Fixed(150.f));
	textBlock16->SetHeight(cui::layout::Length::Fixed(22.f));
	textBlock16->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Success_11));

	// XAML authored Local properties/resources
	groupBox2->SetHeader(BindingValue(L"成交客户"));
	Canvas::SetLeft(*groupBox2, 226.f);
	Canvas::SetTop(*groupBox2, 76.f);
	groupBox2->SetWidth(cui::layout::Length::Fixed(200.f));
	groupBox2->SetHeight(cui::layout::Length::Fixed(100.f));

	// XAML authored Local properties/resources
	canvas5->SetWidth(cui::layout::Length::Fixed(176.f));
	canvas5->SetHeight(cui::layout::Length::Fixed(64.f));

	// XAML authored Local properties/resources
	textBlock17->SetText(L"128");
	Canvas::SetLeft(*textBlock17, 10.f);
	Canvas::SetTop(*textBlock17, 4.f);
	textBlock17->SetWidth(cui::layout::Length::Fixed(150.f));
	textBlock17->SetHeight(cui::layout::Length::Fixed(32.f));
	textBlock17->SetFontSize(22.0);

	// XAML authored Local properties/resources
	Canvas::SetLeft(*progressBar1, 10.f);
	Canvas::SetTop(*progressBar1, 42.f);
	progressBar1->SetMaximum(160.0);
	progressBar1->SetValue(128.0);
	progressBar1->SetWidth(cui::layout::Length::Fixed(150.f));
	progressBar1->SetHeight(cui::layout::Length::Fixed(12.f));

	// XAML authored Local properties/resources
	groupBox3->SetHeader(BindingValue(L"平均毛利率"));
	Canvas::SetLeft(*groupBox3, 440.f);
	Canvas::SetTop(*groupBox3, 76.f);
	groupBox3->SetWidth(cui::layout::Length::Fixed(200.f));
	groupBox3->SetHeight(cui::layout::Length::Fixed(100.f));

	// XAML authored Local properties/resources
	canvas6->SetWidth(cui::layout::Length::Fixed(176.f));
	canvas6->SetHeight(cui::layout::Length::Fixed(64.f));

	// XAML authored Local properties/resources
	textBlock18->SetText(L"29.8%");
	Canvas::SetLeft(*textBlock18, 10.f);
	Canvas::SetTop(*textBlock18, 4.f);
	textBlock18->SetWidth(cui::layout::Length::Fixed(150.f));
	textBlock18->SetHeight(cui::layout::Length::Fixed(32.f));
	textBlock18->SetFontSize(22.0);

	// XAML authored Local properties/resources
	textBlock19->SetText(L"-1.2% · 需关注");
	Canvas::SetLeft(*textBlock19, 10.f);
	Canvas::SetTop(*textBlock19, 38.f);
	textBlock19->SetWidth(cui::layout::Length::Fixed(150.f));
	textBlock19->SetHeight(cui::layout::Length::Fixed(22.f));
	textBlock19->SetForeground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.882353f, 0.356863f, 0.392157f, 1.f}; return value; }());

	// XAML authored Local properties/resources
	chartBar->SetContent(BindingValue(L"柱状图"));
	Canvas::SetLeft(*chartBar, 12.f);
	Canvas::SetTop(*chartBar, 194.f);
	chartBar->SetWidth(cui::layout::Length::Fixed(80.f));
	chartBar->SetHeight(cui::layout::Length::Fixed(28.f));

	// XAML authored Local properties/resources
	chartPie->SetContent(BindingValue(L"饼形图"));
	Canvas::SetLeft(*chartPie, 102.f);
	Canvas::SetTop(*chartPie, 194.f);
	chartPie->SetWidth(cui::layout::Length::Fixed(80.f));
	chartPie->SetHeight(cui::layout::Length::Fixed(28.f));

	// XAML authored Local properties/resources
	chartLine->SetContent(BindingValue(L"曲线图"));
	Canvas::SetLeft(*chartLine, 192.f);
	Canvas::SetTop(*chartLine, 194.f);
	chartLine->SetWidth(cui::layout::Length::Fixed(80.f));
	chartLine->SetHeight(cui::layout::Length::Fixed(28.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*salesChart, 12.f);
	Canvas::SetTop(*salesChart, 232.f);
	salesChart->SetWidth(cui::layout::Length::Fixed(620.f));
	salesChart->SetHeight(cui::layout::Length::Fixed(250.f));
	salesChart->SetTitle(L"成交趋势");
	salesChart->SetSubtitle(L"点击数据点查看明细");
	salesChart->Clear();
	ChartSeries __chartSeries_salesChart_1;
	__chartSeries_salesChart_1.Name = L"零售";
	__chartSeries_salesChart_1.Color = D2D1_COLOR_F{0.168627f, 0.490196f, 0.960784f, 0.94902f};
	__chartSeries_salesChart_1.Points.emplace_back(L"1月", 118.0);
	__chartSeries_salesChart_1.Points.emplace_back(L"2月", 134.5);
	__chartSeries_salesChart_1.Points.emplace_back(L"3月", 126.2);
	__chartSeries_salesChart_1.Points.emplace_back(L"4月", 156.8);
	__chartSeries_salesChart_1.Points.emplace_back(L"5月", 178.4);
	__chartSeries_salesChart_1.Points.emplace_back(L"6月", 172.0);
	__chartSeries_salesChart_1.Points.emplace_back(L"7月", 191.3);
	__chartSeries_salesChart_1.Points.emplace_back(L"8月", 218.5);
	salesChart->AddSeries(__chartSeries_salesChart_1);
	ChartSeries __chartSeries_salesChart_2;
	__chartSeries_salesChart_2.Name = L"企业";
	__chartSeries_salesChart_2.Color = D2D1_COLOR_F{0.101961f, 0.678431f, 0.54902f, 0.94902f};
	__chartSeries_salesChart_2.Points.emplace_back(L"1月", 92.4);
	__chartSeries_salesChart_2.Points.emplace_back(L"2月", 108.0);
	__chartSeries_salesChart_2.Points.emplace_back(L"3月", 131.8);
	__chartSeries_salesChart_2.Points.emplace_back(L"4月", 139.0);
	__chartSeries_salesChart_2.Points.emplace_back(L"5月", 151.2);
	__chartSeries_salesChart_2.Points.emplace_back(L"6月", 169.5);
	__chartSeries_salesChart_2.Points.emplace_back(L"7月", 182.8);
	__chartSeries_salesChart_2.Points.emplace_back(L"8月", 197.0);
	salesChart->AddSeries(__chartSeries_salesChart_2);
	ChartSeries __chartSeries_salesChart_3;
	__chartSeries_salesChart_3.Name = L"渠道";
	__chartSeries_salesChart_3.Color = D2D1_COLOR_F{0.941176f, 0.529412f, 0.180392f, 0.94902f};
	__chartSeries_salesChart_3.Points.emplace_back(L"1月", 66.0);
	__chartSeries_salesChart_3.Points.emplace_back(L"2月", 72.5);
	__chartSeries_salesChart_3.Points.emplace_back(L"3月", 84.0);
	__chartSeries_salesChart_3.Points.emplace_back(L"4月", 90.4);
	__chartSeries_salesChart_3.Points.emplace_back(L"5月", 96.0);
	__chartSeries_salesChart_3.Points.emplace_back(L"6月", 104.3);
	__chartSeries_salesChart_3.Points.emplace_back(L"7月", 112.0);
	__chartSeries_salesChart_3.Points.emplace_back(L"8月", 128.6);
	salesChart->AddSeries(__chartSeries_salesChart_3);

	// XAML authored Local properties/resources
	analyticsReport->SetHeader(BindingValue(L"成交报表 · ItemsSource + DataTemplate"));
	Canvas::SetLeft(*analyticsReport, 650.f);
	Canvas::SetTop(*analyticsReport, 76.f);
	analyticsReport->SetWidth(cui::layout::Length::Fixed(642.f));
	analyticsReport->SetHeight(cui::layout::Length::Fixed(406.f));

	// XAML authored Local properties/resources
	canvas7->SetWidth(cui::layout::Length::Fixed(612.f));
	canvas7->SetHeight(cui::layout::Length::Fixed(366.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*stackPanel2, 12.f);
	stackPanel2->SetOrientation(static_cast<Orientation>(0));
	Canvas::SetTop(*stackPanel2, 8.f);
	stackPanel2->SetWidth(cui::layout::Length::Fixed(570.f));
	stackPanel2->SetHeight(cui::layout::Length::Fixed(28.f));

	// XAML authored Local properties/resources
	textBlock20->SetText(L"客户");
	textBlock20->SetWidth(cui::layout::Length::Fixed(138.f));
	textBlock20->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	// XAML authored Local properties/resources
	textBlock21->SetText(L"区域");
	textBlock21->SetWidth(cui::layout::Length::Fixed(62.f));
	textBlock21->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	// XAML authored Local properties/resources
	textBlock22->SetText(L"阶段");
	textBlock22->SetWidth(cui::layout::Length::Fixed(76.f));
	textBlock22->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	// XAML authored Local properties/resources
	textBlock23->SetText(L"成交额");
	textBlock23->SetWidth(cui::layout::Length::Fixed(86.f));
	textBlock23->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	// XAML authored Local properties/resources
	textBlock24->SetText(L"毛利率");
	textBlock24->SetWidth(cui::layout::Length::Fixed(56.f));
	textBlock24->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	// XAML authored Local properties/resources
	analyticsRows->SetSelectedIndex(0);
	Canvas::SetLeft(*analyticsRows, 12.f);
	Canvas::SetTop(*analyticsRows, 42.f);
	analyticsRows->SetWidth(cui::layout::Length::Fixed(570.f));
	analyticsRows->SetHeight(cui::layout::Length::Fixed(292.f));

	// XAML authored Local properties/resources
	textBlock25->SetText(L"高层报表语义已下沉为通用列表、模板、绑定和资源。");
	Canvas::SetLeft(*textBlock25, 12.f);
	Canvas::SetTop(*textBlock25, 340.f);
	textBlock25->SetWidth(cui::layout::Length::Fixed(570.f));
	textBlock25->SetHeight(cui::layout::Length::Fixed(22.f));
	textBlock25->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	// XAML authored Local properties/resources
	tabItem5->SetHeader(BindingValue(L"布局容器"));

	cui::framework::StyleAccess::SetResourceKey(*border6, L"SurfacePanel", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border6, 10.f);
	Canvas::SetTop(*border6, 36.f);
	border6->SetWidth(cui::layout::Length::Fixed(1325.f));
	border6->SetHeight(cui::layout::Length::Fixed(520.f));


	// XAML authored Local properties/resources
	layoutTitle->SetText(L"StackPanel / Grid / DockPanel / WrapPanel / RelativePanel / ScrollViewer");
	Canvas::SetLeft(*layoutTitle, 12.f);
	Canvas::SetTop(*layoutTitle, 8.f);
	layoutTitle->SetWidth(cui::layout::Length::Fixed(800.f));
	layoutTitle->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	Canvas::SetTop(*canvasSemanticsProbe, 4.5f);
	Canvas::SetRight(*canvasSemanticsProbe, 12.5f);
	canvasSemanticsProbe->SetWidth(cui::layout::Length::Fixed(430.f));
	canvasSemanticsProbe->SetHeight(cui::layout::Length::Fixed(31.f));
	canvasSemanticsProbe->SetClipToBounds(true);
	canvasSemanticsProbe->SetBackground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_ContainerSurface_6));
	canvasSemanticsProbe->SetTag(BindingValue(L"cursor-inheritance-root"));
	canvasSemanticsProbe->SetCursor(static_cast<CursorKind>(1));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*border7, 0.f);
	Canvas::SetTop(*border7, 0.f);
	border7->SetWidth(cui::layout::Length::Fixed(430.f));
	border7->SetHeight(cui::layout::Length::Fixed(31.f));
	border7->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.f, 0.f, 0.f, 0.f}; return value; }());
	border7->SetBorderBrush(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_ContainerBorder_7));
	border7->SetBorderThickness(Thickness(1.f, 1.f, 1.f, 1.f));

	// XAML authored Local properties/resources
	canvasLeftWins->SetText(L"Canvas.Left/Top 优先");
	Canvas::SetLeft(*canvasLeftWins, 0.25f);
	Canvas::SetTop(*canvasLeftWins, 0.5f);
	Canvas::SetRight(*canvasLeftWins, 40.f);
	canvasLeftWins->SetWidth(cui::layout::Length::Fixed(190.f));
	Canvas::SetBottom(*canvasLeftWins, 5.f);
	canvasLeftWins->SetHeight(cui::layout::Length::Fixed(24.f));
	canvasLeftWins->SetMargin(Thickness(1.f, 2.f, 3.f, 4.f));

	// XAML authored Local properties/resources
	canvasRightBottom->SetText(L"Right/Bottom + fractional DIP");
	Canvas::SetRight(*canvasRightBottom, 0.75f);
	canvasRightBottom->SetWidth(cui::layout::Length::Fixed(210.f));
	Canvas::SetBottom(*canvasRightBottom, 0.5f);
	canvasRightBottom->SetHeight(cui::layout::Length::Fixed(24.f));
	canvasRightBottom->SetMargin(Thickness(2.f, 1.f, 4.f, 2.f));

	cui::framework::StyleAccess::SetResourceKey(*border8, L"ContainerFrame", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border8, 12.f);
	Canvas::SetTop(*border8, 42.f);
	border8->SetWidth(cui::layout::Length::Fixed(240.f));
	border8->SetHeight(cui::layout::Length::Fixed(200.f));

	// XAML authored Local properties/resources
	demoStack->SetOrientation(static_cast<Orientation>(1));
	demoStack->SetClipToBounds(true);

	// XAML authored Local properties/resources
	stackA->SetContent(BindingValue(L"Stack A"));
	Canvas::SetLeft(*stackA, 0.f);
	Canvas::SetTop(*stackA, 0.f);
	stackA->SetWidth(cui::layout::Length::Fixed(180.f));
	stackA->SetHeight(cui::layout::Length::Fixed(28.f));

	// XAML authored Local properties/resources
	stackB->SetContent(BindingValue(L"Stack B"));
	Canvas::SetLeft(*stackB, 0.f);
	Canvas::SetTop(*stackB, 0.f);
	stackB->SetWidth(cui::layout::Length::Fixed(200.f));
	stackB->SetHeight(cui::layout::Length::Fixed(28.f));

	// XAML authored Local properties/resources
	stackC->SetContent(BindingValue(L"Stack C"));
	Canvas::SetLeft(*stackC, 0.f);
	Canvas::SetTop(*stackC, 0.f);
	stackC->SetWidth(cui::layout::Length::Fixed(160.f));
	stackC->SetHeight(cui::layout::Length::Fixed(28.f));

	cui::framework::StyleAccess::SetResourceKey(*border9, L"ContainerFrame", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border9, 270.f);
	Canvas::SetTop(*border9, 42.f);
	border9->SetWidth(cui::layout::Length::Fixed(320.f));
	border9->SetHeight(cui::layout::Length::Fixed(200.f));

	// XAML authored Local properties/resources
	demoGrid->SetClipToBounds(true);
	demoGrid->ClearRows();
	demoGrid->ClearColumns();
	demoGrid->AddRow(GridLength::Auto(), 0.f, FLT_MAX);
	demoGrid->AddRow(GridLength::Star(1.f), 0.f, FLT_MAX);
	demoGrid->AddRow(GridLength::Pixels(34.f), 0.f, FLT_MAX);
	demoGrid->AddColumn(GridLength::Star(1.f), 0.f, FLT_MAX);
	demoGrid->AddColumn(GridLength::Star(2.f), 0.f, FLT_MAX);

	// XAML authored Local properties/resources
	gridHeader->SetContent(BindingValue(L"Grid Header"));
	Canvas::SetLeft(*gridHeader, 0.f);
	Canvas::SetTop(*gridHeader, 0.f);
	gridHeader->SetWidth(cui::layout::Length::Fixed(120.f));
	gridHeader->SetHeight(cui::layout::Length::Fixed(30.f));
	Grid::SetColumnSpan(*gridHeader, 2);

	// XAML authored Local properties/resources
	gridLeft->SetText(L"Auto / Star");
	Canvas::SetLeft(*gridLeft, 0.f);
	Canvas::SetTop(*gridLeft, 0.f);
	gridLeft->SetWidth(cui::layout::Length::Fixed(60.f));
	gridLeft->SetHeight(cui::layout::Length::Fixed(21.f));
	Grid::SetRow(*gridLeft, 1);

	// XAML authored Local properties/resources
	gridEditor->SetText(L"Grid cell");
	Canvas::SetLeft(*gridEditor, 0.f);
	Canvas::SetTop(*gridEditor, 0.f);
	gridEditor->SetWidth(cui::layout::Length::Fixed(120.f));
	gridEditor->SetHeight(cui::layout::Length::Fixed(25.f));
	Grid::SetRow(*gridEditor, 1);
	Grid::SetColumn(*gridEditor, 1);

	// XAML authored Local properties/resources
	gridFooter->SetContent(BindingValue(L"Grid Footer"));
	Canvas::SetLeft(*gridFooter, 0.f);
	Canvas::SetTop(*gridFooter, 0.f);
	gridFooter->SetWidth(cui::layout::Length::Fixed(120.f));
	gridFooter->SetHeight(cui::layout::Length::Fixed(30.f));
	Grid::SetRow(*gridFooter, 2);
	Grid::SetColumnSpan(*gridFooter, 2);

	cui::framework::StyleAccess::SetResourceKey(*border10, L"ContainerFrame", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border10, 610.f);
	Canvas::SetTop(*border10, 42.f);
	border10->SetWidth(cui::layout::Length::Fixed(300.f));
	border10->SetHeight(cui::layout::Length::Fixed(200.f));

	// XAML authored Local properties/resources
	demoDock->SetLastChildFill(true);
	demoDock->SetClipToBounds(true);

	// XAML authored Local properties/resources
	dockTop->SetContent(BindingValue(L"Top"));
	Canvas::SetLeft(*dockTop, 0.f);
	Canvas::SetTop(*dockTop, 0.f);
	dockTop->SetWidth(cui::layout::Length::Fixed(120.f));
	dockTop->SetHeight(cui::layout::Length::Fixed(30.f));
	DockPanel::SetDock(*dockTop, static_cast<Dock>(1));

	// XAML authored Local properties/resources
	dockLeft->SetContent(BindingValue(L"Left"));
	Canvas::SetLeft(*dockLeft, 0.f);
	Canvas::SetTop(*dockLeft, 0.f);
	dockLeft->SetWidth(cui::layout::Length::Fixed(70.f));
	dockLeft->SetHeight(cui::layout::Length::Fixed(30.f));
	DockPanel::SetDock(*dockLeft, static_cast<Dock>(0));

	// XAML authored Local properties/resources
	dockFill->SetText(L"Fill");
	Canvas::SetLeft(*dockFill, 0.f);
	Canvas::SetTop(*dockFill, 0.f);
	dockFill->SetWidth(cui::layout::Length::Fixed(110.f));
	dockFill->SetHeight(cui::layout::Length::Fixed(150.f));

	cui::framework::StyleAccess::SetResourceKey(*border11, L"ContainerFrame", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border11, 930.f);
	Canvas::SetTop(*border11, 42.f);
	border11->SetWidth(cui::layout::Length::Fixed(360.f));
	border11->SetHeight(cui::layout::Length::Fixed(200.f));

	// XAML authored Local properties/resources
	demoWrap->SetOrientation(static_cast<Orientation>(0));
	demoWrap->SetItemWidth(80.f);
	demoWrap->SetItemHeight(32.f);
	demoWrap->SetClipToBounds(true);

	// XAML authored Local properties/resources
	wrap1->SetContent(BindingValue(L"One"));
	Canvas::SetLeft(*wrap1, 0.f);
	Canvas::SetTop(*wrap1, 0.f);
	wrap1->SetWidth(cui::layout::Length::Fixed(80.f));
	wrap1->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	wrap2->SetContent(BindingValue(L"Two"));
	Canvas::SetLeft(*wrap2, 0.f);
	Canvas::SetTop(*wrap2, 0.f);
	wrap2->SetWidth(cui::layout::Length::Fixed(80.f));
	wrap2->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	wrap3->SetContent(BindingValue(L"Three"));
	Canvas::SetLeft(*wrap3, 0.f);
	Canvas::SetTop(*wrap3, 0.f);
	wrap3->SetWidth(cui::layout::Length::Fixed(80.f));
	wrap3->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	wrap4->SetContent(BindingValue(L"Four"));
	Canvas::SetLeft(*wrap4, 0.f);
	Canvas::SetTop(*wrap4, 0.f);
	wrap4->SetWidth(cui::layout::Length::Fixed(80.f));
	wrap4->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	wrap5->SetContent(BindingValue(L"Five"));
	Canvas::SetLeft(*wrap5, 0.f);
	Canvas::SetTop(*wrap5, 0.f);
	wrap5->SetWidth(cui::layout::Length::Fixed(80.f));
	wrap5->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	wrap6->SetContent(BindingValue(L"Six"));
	Canvas::SetLeft(*wrap6, 0.f);
	Canvas::SetTop(*wrap6, 0.f);
	wrap6->SetWidth(cui::layout::Length::Fixed(80.f));
	wrap6->SetHeight(cui::layout::Length::Fixed(32.f));

	cui::framework::StyleAccess::SetResourceKey(*border12, L"ContainerFrame", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border12, 12.f);
	Canvas::SetTop(*border12, 270.f);
	border12->SetWidth(cui::layout::Length::Fixed(500.f));
	border12->SetHeight(cui::layout::Length::Fixed(190.f));

	// XAML authored Local properties/resources
	demoRelative->SetClipToBounds(true);

	// XAML authored Local properties/resources
	relativeCenter->SetOrientation(static_cast<Orientation>(1));
	relativeCenter->SetClipToBounds(true);

	// XAML authored Local properties/resources
	naturalTextProbe->SetText(L"Auto DesiredSize · 无 Width / Height");
	naturalTextProbe->SetHorizontalAlignment(static_cast<::HorizontalAlignment>(0));

	// XAML authored Local properties/resources
	wrappedTextProbe->SetText(L"TextBlock 使用父级约束重新排版；换行后的高度会反向参与 StackPanel 与 RelativePanel 测量。");
	wrappedTextProbe->SetTextWrapping(static_cast<TextWrapping>(1));
	wrappedTextProbe->SetMargin(Thickness(0.f, 8.f, 0.f, 0.f));
	wrappedTextProbe->SetHorizontalAlignment(static_cast<::HorizontalAlignment>(0));
	wrappedTextProbe->SetMaxWidth(320.f);

	// XAML authored Local properties/resources
	trimmedTextProbe->SetText(L"CharacterEllipsis · 最终绘制宽度来自 Arrange 槽");
	trimmedTextProbe->SetTextTrimming(static_cast<TextTrimming>(1));
	trimmedTextProbe->SetMargin(Thickness(0.f, 6.f, 0.f, 0.f));
	trimmedTextProbe->SetHorizontalAlignment(static_cast<::HorizontalAlignment>(0));
	trimmedTextProbe->SetMaxWidth(220.f);
	trimmedTextProbe->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	// XAML authored Local properties/resources
	relativeCenterButton->SetContent(BindingValue(L"XAML 居中约束"));
	relativeCenterButton->SetHeight(cui::layout::Length::Fixed(32.f));
	relativeCenterButton->SetMargin(Thickness(0.f, 8.f, 0.f, 0.f));
	relativeCenterButton->SetHorizontalAlignment(static_cast<::HorizontalAlignment>(0));

	cui::framework::StyleAccess::SetResourceKey(*border13, L"ContainerFrame", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border13, 532.f);
	Canvas::SetTop(*border13, 270.f);
	border13->SetWidth(cui::layout::Length::Fixed(758.f));
	border13->SetHeight(cui::layout::Length::Fixed(190.f));

	// XAML authored Local properties/resources
	demoScroll->SetClipToBounds(true);

	// XAML authored Local properties/resources
	demoScrollContent->SetWidth(cui::layout::Length::Fixed(1100.f));
	demoScrollContent->SetHeight(cui::layout::Length::Fixed(360.f));
	demoScrollContent->SetClipToBounds(true);

	cui::framework::StyleAccess::SetResourceKey(*border14, L"SurfacePanel", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border14, 16.f);
	Canvas::SetTop(*border14, 16.f);
	border14->SetWidth(cui::layout::Length::Fixed(220.f));
	border14->SetHeight(cui::layout::Length::Fixed(90.f));

	// XAML authored Local properties/resources
	scrollCard1->SetClipToBounds(true);

	// XAML authored Local properties/resources
	scrollCard1Text->SetText(L"Card 1 · XAML child");
	Canvas::SetLeft(*scrollCard1Text, 0.f);
	Canvas::SetTop(*scrollCard1Text, 0.f);
	scrollCard1Text->SetWidth(cui::layout::Length::Fixed(160.f));
	scrollCard1Text->SetHeight(cui::layout::Length::Fixed(21.f));

	cui::framework::StyleAccess::SetResourceKey(*border15, L"SurfacePanel", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border15, 270.f);
	Canvas::SetTop(*border15, 16.f);
	border15->SetWidth(cui::layout::Length::Fixed(220.f));
	border15->SetHeight(cui::layout::Length::Fixed(90.f));

	// XAML authored Local properties/resources
	scrollCard2->SetClipToBounds(true);

	// XAML authored Local properties/resources
	scrollCard2Text->SetText(L"Card 2 · XAML child");
	Canvas::SetLeft(*scrollCard2Text, 0.f);
	Canvas::SetTop(*scrollCard2Text, 0.f);
	scrollCard2Text->SetWidth(cui::layout::Length::Fixed(160.f));
	scrollCard2Text->SetHeight(cui::layout::Length::Fixed(21.f));

	// XAML authored Local properties/resources
	farButton->SetContent(BindingValue(L"Far Button"));
	Canvas::SetLeft(*farButton, 920.f);
	Canvas::SetTop(*farButton, 280.f);
	farButton->SetWidth(cui::layout::Length::Fixed(140.f));
	farButton->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	tabItem6->SetHeader(BindingValue(L"系统集成"));

	cui::framework::StyleAccess::SetResourceKey(*border16, L"SurfacePanel", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border16, 10.f);
	Canvas::SetTop(*border16, 36.f);
	border16->SetWidth(cui::layout::Length::Fixed(1325.f));
	border16->SetHeight(cui::layout::Length::Fixed(520.f));


	// XAML authored Local properties/resources
	systemTitle->SetText(L"NotifyIcon / Taskbar / ContextMenu / MessageDialog / XAML notification composition");
	Canvas::SetLeft(*systemTitle, 14.f);
	Canvas::SetTop(*systemTitle, 14.f);
	systemTitle->SetWidth(cui::layout::Length::Fixed(900.f));
	systemTitle->SetHeight(cui::layout::Length::Fixed(26.f));

	// XAML authored Local properties/resources
	notifyToggle->SetContent(BindingValue(L"显示/隐藏托盘图标"));
	Canvas::SetLeft(*notifyToggle, 14.f);
	Canvas::SetTop(*notifyToggle, 62.f);
	notifyToggle->SetWidth(cui::layout::Length::Fixed(180.f));
	notifyToggle->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	notifyBalloon->SetContent(BindingValue(L"气泡提示"));
	Canvas::SetLeft(*notifyBalloon, 206.f);
	Canvas::SetTop(*notifyBalloon, 62.f);
	notifyBalloon->SetWidth(cui::layout::Length::Fixed(130.f));
	notifyBalloon->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	showDialog->SetContent(BindingValue(L"CUI 对话框"));
	Canvas::SetLeft(*showDialog, 348.f);
	Canvas::SetTop(*showDialog, 62.f);
	showDialog->SetWidth(cui::layout::Length::Fixed(130.f));
	showDialog->SetHeight(cui::layout::Length::Fixed(32.f));

	cui::framework::StyleAccess::SetResourceKey(*showToast, L"PrimaryButton", false);
	// XAML authored Local properties/resources
	showToast->SetContent(BindingValue(L"显示 Toast"));
	Canvas::SetLeft(*showToast, 490.f);
	Canvas::SetTop(*showToast, 62.f);
	showToast->SetWidth(cui::layout::Length::Fixed(130.f));
	showToast->SetHeight(cui::layout::Length::Fixed(32.f));
	showToast->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.184314f, 0.435294f, 0.894118f, 1.f}; return value; }());
	showToast->SetForeground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{1.f, 1.f, 1.f, 1.f}; return value; }());

	// XAML authored Local properties/resources
	systemHint->SetText(L"在页面空白区域单击右键显示 XAML 定义的 ContextMenu；C++ 仅保留 NotifyIcon / Taskbar 服务与命名事件处理。");
	Canvas::SetLeft(*systemHint, 14.f);
	Canvas::SetTop(*systemHint, 116.f);
	systemHint->SetWidth(cui::layout::Length::Fixed(850.f));
	systemHint->SetHeight(cui::layout::Length::Fixed(54.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*border17, 14.f);
	Canvas::SetTop(*border17, 184.f);
	border17->SetWidth(cui::layout::Length::Fixed(790.f));
	border17->SetHeight(cui::layout::Length::Fixed(252.f));
	border17->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.968627f, 0.976471f, 0.988235f, 1.f}; return value; }());
	border17->SetBorderBrush([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.827451f, 0.862745f, 0.909804f, 1.f}; return value; }());
	border17->SetBorderThickness(Thickness(1.f, 1.f, 1.f, 1.f));


	// XAML authored Local properties/resources
	textBlock26->SetText(L"ICommandSource.CommandTarget · XAML namescope 对象引用");
	Canvas::SetLeft(*textBlock26, 14.f);
	Canvas::SetTop(*textBlock26, 10.f);
	textBlock26->SetWidth(cui::layout::Length::Fixed(744.f));
	textBlock26->SetHeight(cui::layout::Length::Fixed(26.f));
	textBlock26->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));
	textBlock26->SetFontSize(15.0);

	commandTargetButton->CommandTarget = systemSurface;
	// XAML authored Local properties/resources
	commandTargetButton->SetContent(BindingValue(L"Button → systemSurface（与焦点无关）"));
	Canvas::SetLeft(*commandTargetButton, 14.f);
	Canvas::SetTop(*commandTargetButton, 48.f);
	commandTargetButton->SetWidth(cui::layout::Length::Fixed(300.f));
	commandTargetButton->SetHeight(cui::layout::Length::Fixed(34.f));
	commandTargetButton->SetCommand(L"Demo.System.Refresh");
	commandTargetButton->SetCommandParameter(BindingValue(L"target-button-system-surface"));

	// XAML authored Local properties/resources
	textBlock27->SetText(L"焦点可停在任意控件；命令仍从显式目标开始路由。");
	Canvas::SetLeft(*textBlock27, 330.f);
	Canvas::SetTop(*textBlock27, 51.f);
	textBlock27->SetWidth(cui::layout::Length::Fixed(440.f));
	textBlock27->SetHeight(cui::layout::Length::Fixed(30.f));

	// XAML authored Local properties/resources
	commandTargetTrace->SetText(L"CanExecute target=∅ · Executed target=∅");
	Canvas::SetLeft(*commandTargetTrace, 14.f);
	Canvas::SetTop(*commandTargetTrace, 98.f);
	commandTargetTrace->SetWidth(cui::layout::Length::Fixed(744.f));
	commandTargetTrace->SetHeight(cui::layout::Length::Fixed(52.f));
	commandTargetTrace->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.062745f, 0.094118f, 0.12549f, 1.f}; return value; }());
	commandTargetTrace->SetForeground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.839216f, 0.886275f, 0.941176f, 1.f}; return value; }());
	commandTargetTrace->SetFontFamily(L"Consolas");
	commandTargetTrace->SetFontSize(12.0);

	// XAML authored Local properties/resources
	textBlock28->SetText(L"ContextMenu.Refresh → mainMenu：显式目标覆盖 PlacementTarget");
	Canvas::SetLeft(*textBlock28, 14.f);
	Canvas::SetTop(*textBlock28, 164.f);
	textBlock28->SetWidth(cui::layout::Length::Fixed(744.f));
	textBlock28->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	textBlock29->SetText(L"ContextMenu.More.CopyInfo → systemSurface：嵌套 MenuItem 保留独立目标");
	Canvas::SetLeft(*textBlock29, 14.f);
	Canvas::SetTop(*textBlock29, 194.f);
	textBlock29->SetWidth(cui::layout::Length::Fixed(744.f));
	textBlock29->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	textBlock30->SetText(L"XAML 定义类型/属性/目标；追踪按 CommandTransaction 配对，后台 requery 不覆盖完整执行。");
	Canvas::SetLeft(*textBlock30, 14.f);
	Canvas::SetTop(*textBlock30, 222.f);
	textBlock30->SetWidth(cui::layout::Length::Fixed(744.f));
	textBlock30->SetHeight(cui::layout::Length::Fixed(20.f));
	textBlock30->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));
	textBlock30->SetFontSize(11.0);

	// XAML authored Local properties/resources
	notificationPanel->SetHeader(BindingValue(L"通知区域 · XAML 组合"));
	Canvas::SetLeft(*notificationPanel, 850.f);
	Canvas::SetTop(*notificationPanel, 40.f);
	notificationPanel->SetWidth(cui::layout::Length::Fixed(430.f));
	notificationPanel->SetHeight(cui::layout::Length::Fixed(300.f));

	// XAML authored Local properties/resources
	canvas9->SetWidth(cui::layout::Length::Fixed(398.f));
	canvas9->SetHeight(cui::layout::Length::Fixed(250.f));

	// XAML authored Local properties/resources
	textBlock31->SetText(L"CUI XAML");
	Canvas::SetLeft(*textBlock31, 14.f);
	Canvas::SetTop(*textBlock31, 14.f);
	textBlock31->SetWidth(cui::layout::Length::Fixed(350.f));
	textBlock31->SetHeight(cui::layout::Length::Fixed(24.f));
	textBlock31->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));

	// XAML authored Local properties/resources
	toastMessage->SetText(L"通知视觉树由 GroupBox、Canvas、TextBlock 和 Button 构成。");
	Canvas::SetLeft(*toastMessage, 14.f);
	Canvas::SetTop(*toastMessage, 48.f);
	toastMessage->SetWidth(cui::layout::Length::Fixed(350.f));
	toastMessage->SetHeight(cui::layout::Length::Fixed(72.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*progressBar2, 14.f);
	Canvas::SetTop(*progressBar2, 132.f);
	progressBar2->SetMaximum(1.0);
	progressBar2->SetValue(0.72);
	progressBar2->SetWidth(cui::layout::Length::Fixed(350.f));
	progressBar2->SetHeight(cui::layout::Length::Fixed(12.f));

	// XAML authored Local properties/resources
	dismissToast->SetContent(BindingValue(L"清除通知"));
	Canvas::SetLeft(*dismissToast, 268.f);
	Canvas::SetTop(*dismissToast, 168.f);
	dismissToast->SetWidth(cui::layout::Length::Fixed(96.f));
	dismissToast->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	textBlock32->SetText(L"Popup/动画可在后续以模板和触发器加入。");
	Canvas::SetLeft(*textBlock32, 14.f);
	Canvas::SetTop(*textBlock32, 214.f);
	textBlock32->SetWidth(cui::layout::Length::Fixed(350.f));
	textBlock32->SetHeight(cui::layout::Length::Fixed(22.f));
	textBlock32->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	// XAML authored Local properties/resources
	tabItem7->SetHeader(BindingValue(L"WebBrowser"));

	cui::framework::StyleAccess::SetResourceKey(*border18, L"SurfacePanel", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border18, 10.f);
	Canvas::SetTop(*border18, 36.f);
	border18->SetWidth(cui::layout::Length::Fixed(1325.f));
	border18->SetHeight(cui::layout::Length::Fixed(520.f));


	cui::framework::StyleAccess::SetResourceKey(*invokeWeb, L"PrimaryButton", false);
	// XAML authored Local properties/resources
	invokeWeb->SetContent(BindingValue(L"C++ 调用 JavaScript"));
	Canvas::SetLeft(*invokeWeb, 14.f);
	Canvas::SetTop(*invokeWeb, 12.f);
	invokeWeb->SetWidth(cui::layout::Length::Fixed(280.f));
	invokeWeb->SetHeight(cui::layout::Length::Fixed(30.f));
	invokeWeb->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.184314f, 0.435294f, 0.894118f, 1.f}; return value; }());
	invokeWeb->SetForeground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{1.f, 1.f, 1.f, 1.f}; return value; }());

	// XAML authored Local properties/resources
	webHint->SetText(L"WebBrowser 的位置与大小来自 XAML，HTML 与 JS bridge 属于运行时业务数据");
	Canvas::SetLeft(*webHint, 320.f);
	Canvas::SetTop(*webHint, 16.f);
	webHint->SetWidth(cui::layout::Length::Fixed(760.f));
	webHint->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*border19, 14.f);
	Canvas::SetTop(*border19, 56.f);
	border19->SetWidth(cui::layout::Length::Fixed(1270.f));
	border19->SetHeight(cui::layout::Length::Fixed(420.f));
	border19->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.f, 0.f, 0.f, 1.f}; return value; }());


	// XAML authored Local properties/resources
	tabItem8->SetHeader(BindingValue(L"MediaPlayer"));

	cui::framework::StyleAccess::SetResourceKey(*border20, L"SurfacePanel", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border20, 10.f);
	Canvas::SetTop(*border20, 36.f);
	border20->SetWidth(cui::layout::Length::Fixed(1325.f));
	border20->SetHeight(cui::layout::Length::Fixed(520.f));


	// XAML authored Local properties/resources
	Canvas::SetLeft(*mediaPlayer, 14.f);
	Canvas::SetTop(*mediaPlayer, 14.f);
	mediaPlayer->SetWidth(cui::layout::Length::Fixed(1270.f));
	mediaPlayer->SetHeight(cui::layout::Length::Fixed(340.f));
	mediaPlayer->SetAutoPlay(true);
	mediaPlayer->SetLoop(false);

	cui::framework::StyleAccess::SetResourceKey(*mediaOpen, L"PrimaryButton", false);
	// XAML authored Local properties/resources
	mediaOpen->SetContent(BindingValue(L"打开"));
	Canvas::SetLeft(*mediaOpen, 14.f);
	Canvas::SetTop(*mediaOpen, 370.f);
	mediaOpen->SetWidth(cui::layout::Length::Fixed(86.f));
	mediaOpen->SetHeight(cui::layout::Length::Fixed(32.f));
	mediaOpen->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.184314f, 0.435294f, 0.894118f, 1.f}; return value; }());
	mediaOpen->SetForeground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{1.f, 1.f, 1.f, 1.f}; return value; }());

	// XAML authored Local properties/resources
	mediaPlay->SetContent(BindingValue(L"播放"));
	Canvas::SetLeft(*mediaPlay, 110.f);
	Canvas::SetTop(*mediaPlay, 370.f);
	mediaPlay->SetWidth(cui::layout::Length::Fixed(76.f));
	mediaPlay->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	mediaPause->SetContent(BindingValue(L"暂停"));
	Canvas::SetLeft(*mediaPause, 196.f);
	Canvas::SetTop(*mediaPause, 370.f);
	mediaPause->SetWidth(cui::layout::Length::Fixed(76.f));
	mediaPause->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	mediaStop->SetContent(BindingValue(L"停止"));
	Canvas::SetLeft(*mediaStop, 282.f);
	Canvas::SetTop(*mediaStop, 370.f);
	mediaStop->SetWidth(cui::layout::Length::Fixed(76.f));
	mediaStop->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	volumeLabel->SetText(L"音量");
	Canvas::SetLeft(*volumeLabel, 380.f);
	Canvas::SetTop(*volumeLabel, 376.f);
	volumeLabel->SetWidth(cui::layout::Length::Fixed(50.f));
	volumeLabel->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*mediaVolume, 430.f);
	mediaVolume->SetMinimum(0.0);
	Canvas::SetTop(*mediaVolume, 370.f);
	mediaVolume->SetMaximum(100.0);
	mediaVolume->SetValue(80.0);
	mediaVolume->SetWidth(cui::layout::Length::Fixed(170.f));
	mediaVolume->SetHeight(cui::layout::Length::Fixed(30.f));

	// XAML authored Local properties/resources
	speedTitle->SetText(L"速度");
	Canvas::SetLeft(*speedTitle, 620.f);
	Canvas::SetTop(*speedTitle, 376.f);
	speedTitle->SetWidth(cui::layout::Length::Fixed(50.f));
	speedTitle->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*mediaSpeed, 670.f);
	mediaSpeed->SetMinimum(10.0);
	Canvas::SetTop(*mediaSpeed, 370.f);
	mediaSpeed->SetMaximum(400.0);
	mediaSpeed->SetValue(100.0);
	mediaSpeed->SetWidth(cui::layout::Length::Fixed(170.f));
	mediaSpeed->SetHeight(cui::layout::Length::Fixed(30.f));

	// XAML authored Local properties/resources
	mediaSpeedText->SetText(L"1.00x");
	Canvas::SetLeft(*mediaSpeedText, 850.f);
	Canvas::SetTop(*mediaSpeedText, 376.f);
	mediaSpeedText->SetWidth(cui::layout::Length::Fixed(80.f));
	mediaSpeedText->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	mediaLoop->SetContent(BindingValue(L"循环"));
	Canvas::SetLeft(*mediaLoop, 950.f);
	Canvas::SetTop(*mediaLoop, 376.f);
	mediaLoop->SetWidth(cui::layout::Length::Fixed(90.f));
	mediaLoop->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*mediaProgress, 14.f);
	mediaProgress->SetMinimum(0.0);
	Canvas::SetTop(*mediaProgress, 426.f);
	mediaProgress->SetMaximum(1000.0);
	mediaProgress->SetValue(619.5652);
	mediaProgress->SetWidth(cui::layout::Length::Fixed(1040.f));
	mediaProgress->SetHeight(cui::layout::Length::Fixed(30.f));

	// XAML authored Local properties/resources
	mediaTime->SetText(L"00:00 / 00:00");
	Canvas::SetLeft(*mediaTime, 1070.f);
	Canvas::SetTop(*mediaTime, 432.f);
	mediaTime->SetWidth(cui::layout::Length::Fixed(210.f));
	mediaTime->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	tabItem9->SetHeader(BindingValue(L"WPF 语义实验"));

	cui::framework::StyleAccess::SetResourceKey(*border21, L"SurfacePanel", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border21, 10.f);
	Canvas::SetTop(*border21, 36.f);
	border21->SetWidth(cui::layout::Length::Fixed(1325.f));
	border21->SetHeight(cui::layout::Length::Fixed(520.f));

	// 控件级词法资源作用域
	// AOT Style 程序：生成期完成分组、索引和连续池布局
	static constexpr CompiledBindingPathStep __resources_wpfLabSurface_program_data_path_1[] = {
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Observe, BindingValueKind::Empty, BindingSourcePropertyToken{ 13692166754359878880ULL }, 0u }
	};
	static constexpr CompiledBindingPathStep __resources_wpfLabSurface_program_data_path_2[] = {
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Observe, BindingValueKind::Empty, BindingSourcePropertyToken{ 15197647781564744993ULL }, 0u }
	};
	static constexpr CompiledBindingPathView __resources_wpfLabSurface_program_data_paths[] = {
		CompiledBindingPathView{ __resources_wpfLabSurface_program_data_path_1 },
		CompiledBindingPathView{ __resources_wpfLabSurface_program_data_path_2 }
	};
	static constexpr D2D1_COLOR_F __resources_wpfLabSurface_program_values_colors[] = {
		D2D1_COLOR_F{0.078431f, 0.639216f, 0.498039f, 1.f}
	};
	static constexpr std::wstring_view __resources_wpfLabSurface_program_values_string_values[] = {
		L"Consolas",
		L"Ready",
		L"Ready",
		L"true",
		L"Trigger ready"
	};
	static constexpr double __resources_wpfLabSurface_program_values_doubles[] = {
		15.0,
		13.0,
		14.0
	};
	static constexpr bool __resources_wpfLabSurface_program_values_bools[] = {
		false,
		true,
		true,
		true
	};
	static constexpr Thickness __resources_wpfLabSurface_program_values_thicknesses[] = {
		Thickness(1.f, 1.f, 1.f, 1.f),
		Thickness(4.f, 4.f, 4.f, 4.f),
		Thickness(1.f, 1.f, 1.f, 1.f),
		Thickness(2.f, 2.f, 2.f, 2.f)
	};
	static constexpr std::wstring_view __resources_wpfLabSurface_program_strings[] = {
		L"WpfLabAccent",
		L"WpfLabFontFamily",
		L"WpfLabFontSize",
		L"WpfLabLiveButton",
		L"WpfFocusScopeBorder"
	};
	static constexpr CompiledStyleValuePoolView __resources_wpfLabSurface_program_value_pools[] = {
		MakeCompiledStyleValuePoolView(__resources_wpfLabSurface_program_values_colors),
		MakeCompiledStyleValuePoolView(__resources_wpfLabSurface_program_values_string_values),
		MakeCompiledStyleValuePoolView(__resources_wpfLabSurface_program_values_doubles),
		MakeCompiledStyleValuePoolView(__resources_wpfLabSurface_program_values_bools),
		MakeCompiledStyleValuePoolView(__resources_wpfLabSurface_program_values_thicknesses)
	};
	static const CompiledStyleResourceOp __resources_wpfLabSurface_program_resources[] = {
		{ 0u, MakeCompiledStyleStaticValueReference(0u, 0u) },
		{ 1u, MakeCompiledStyleStaticValueReference(1u, 0u) },
		{ 2u, MakeCompiledStyleStaticValueReference(2u, 0u) }
	};
	static constexpr uint32_t __resources_wpfLabSurface_program_resource_lookup[] = {
		0u,
		1u,
		2u
	};
	static const CompiledStylePropertyConditionOp __resources_wpfLabSurface_program_property_conditions[] = {
		{ DependencyPropertyReference(Button::ContentProperty()), MakeCompiledStyleStaticValueReference(1u, 4u) },
		{ DependencyPropertyReference(Button::IsDefaultProperty()), MakeCompiledStyleStaticValueReference(3u, 2u) },
		{ DependencyPropertyReference(Control::IsKeyboardFocusWithinProperty()), MakeCompiledStyleStaticValueReference(3u, 3u) }
	};
	static constexpr CompiledStyleDataConditionOp __resources_wpfLabSurface_program_data_conditions[] = {
		{ 0u, MakeCompiledStyleStaticValueReference(1u, 1u) },
		{ 0u, MakeCompiledStyleStaticValueReference(1u, 2u) },
		{ 1u, MakeCompiledStyleStaticValueReference(1u, 3u) }
	};
	static const CompiledStyleSetterOp __resources_wpfLabSurface_program_setters[] = {
		{ DependencyPropertyReference(Control::FontSizeProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(2u, 1u) } },
		{ DependencyPropertyReference(Button::IsDefaultProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(3u, 0u) } },
		{ DependencyPropertyReference(Control::BorderThicknessProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(4u, 0u) } },
		{ DependencyPropertyReference(Control::BackgroundProperty()), { CompiledStyleOperandKind::DynamicResource, 0u } },
		{ DependencyPropertyReference(Control::ForegroundProperty()), { CompiledStyleOperandKind::Literal, 0u } },
		{ DependencyPropertyReference(Control::FontSizeProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(2u, 2u) } },
		{ DependencyPropertyReference(Button::IsDefaultProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(3u, 1u) } },
		{ DependencyPropertyReference(Control::BorderThicknessProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(4u, 1u) } },
		{ DependencyPropertyReference(Control::BorderBrushProperty()), { CompiledStyleOperandKind::Literal, 3u } },
		{ DependencyPropertyReference(Control::BorderThicknessProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(4u, 2u) } },
		{ DependencyPropertyReference(Control::BorderBrushProperty()), { CompiledStyleOperandKind::Literal, 4u } },
		{ DependencyPropertyReference(Control::BorderThicknessProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(4u, 3u) } }
	};
	static const CompiledInteractionPropertyOperand __resources_wpfLabSurface_program_property_operands[] = {
		{ 0u, DependencyPropertyReference(Control::FontSizeProperty()) }
	};
	static constexpr CompiledInteractionAnimationOp __resources_wpfLabSurface_program_animations[] = {
		{ DeclarativeAnimationKind::Double, 0u, CompiledInteractionInvalidIndex, 1u, 2u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, 0ULL, 400ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut }
	};
	static constexpr CompiledInteractionStoryboardOp __resources_wpfLabSurface_program_storyboards[] = {
		{ { 0u, 1u } }
	};
	static constexpr CompiledInteractionActionOp __resources_wpfLabSurface_program_actions[] = {
		{ DeclarativeStoryboardActionKind::Begin, 0u },
		{ DeclarativeStoryboardActionKind::Stop, 0u }
	};
	static constexpr CompiledStyleRuleOp __resources_wpfLabSurface_program_rules[] = {
		{ 1u, 0u, { 0u, 0u }, { 0u, 0u }, { 0u, 5u }, { 0u, 0u }, { 0u, 0u } },
		{ 2u, 1u, { 0u, 0u }, { 0u, 1u }, { 5u, 1u }, { 0u, 1u }, { 1u, 1u } },
		{ 3u, 2u, { 0u, 0u }, { 1u, 2u }, { 6u, 1u }, { 2u, 0u }, { 2u, 0u } },
		{ 4u, 3u, { 0u, 2u }, { 3u, 0u }, { 7u, 1u }, { 2u, 0u }, { 2u, 0u } },
		{ 5u, 4u, { 2u, 0u }, { 3u, 0u }, { 8u, 2u }, { 2u, 0u }, { 2u, 0u } },
		{ 6u, 5u, { 2u, 1u }, { 3u, 0u }, { 10u, 2u }, { 2u, 0u }, { 2u, 0u } }
	};
	static constexpr uint32_t __resources_wpfLabSurface_program_rule_indexes[] = {
		0u,
		1u,
		2u,
		3u,
		4u,
		5u
	};
	static const DependencyPropertyReference __resources_wpfLabSurface_program_property_watchers[] = {
		DependencyPropertyReference(Button::ContentProperty()),
		DependencyPropertyReference(Button::IsDefaultProperty()),
		DependencyPropertyReference(Control::IsKeyboardFocusWithinProperty())
	};
	static constexpr uint32_t __resources_wpfLabSurface_program_data_path_watchers[] = {
		0u,
		1u
	};
	static constexpr CompiledStyleGroupOp __resources_wpfLabSurface_program_groups[] = {
		{ true, static_cast<UIClass>(7), ComponentTypeToken{ 0ULL }, 3u, { 0u, 4u }, { 0u, 2u }, { 0u, 2u } },
		{ true, static_cast<UIClass>(24), ComponentTypeToken{ 0ULL }, 4u, { 4u, 2u }, { 2u, 1u }, { 2u, 0u } }
	};
	static const DependencyPropertyReference __resources_wpfLabSurface_program_global_property_watchers[] = {
		DependencyPropertyReference(Button::ContentProperty()),
		DependencyPropertyReference(Button::IsDefaultProperty()),
		DependencyPropertyReference(Control::IsKeyboardFocusWithinProperty())
	};
	static constexpr uint32_t __resources_wpfLabSurface_program_global_data_path_watchers[] = {
		0u,
		1u
	};
	auto __resources_wpfLabSurface = ControlStyleSheet::CreateCompiled(
		CompiledStyleProgramView{
			CompiledStyleProgramViewVersion,
			std::span<const std::wstring_view>{ __resources_wpfLabSurface_program_strings }, // Strings
			std::span<const CompiledStyleValuePoolView>{ __resources_wpfLabSurface_program_value_pools }, // ValuePools
			std::span<const CompiledStyleResourceOp>{ __resources_wpfLabSurface_program_resources }, // Resources
			std::span<const uint32_t>{ __resources_wpfLabSurface_program_resource_lookup }, // ResourceLookup
			std::span<const CompiledStylePropertyConditionOp>{ __resources_wpfLabSurface_program_property_conditions }, // PropertyConditions
			std::span<const CompiledStyleDataConditionOp>{ __resources_wpfLabSurface_program_data_conditions }, // DataConditions
			std::span<const CompiledStyleSetterOp>{ __resources_wpfLabSurface_program_setters }, // Setters
			std::span<const CompiledInteractionPropertyOperand>{ __resources_wpfLabSurface_program_property_operands }, // PropertyOperands
			{}, // ObjectPathChildIndices
			{}, // ObjectPaths
			{}, // KeyFrames
			std::span<const CompiledInteractionAnimationOp>{ __resources_wpfLabSurface_program_animations }, // Animations
			std::span<const CompiledInteractionStoryboardOp>{ __resources_wpfLabSurface_program_storyboards }, // Storyboards
			std::span<const CompiledInteractionActionOp>{ __resources_wpfLabSurface_program_actions }, // Actions
			std::span<const CompiledStyleRuleOp>{ __resources_wpfLabSurface_program_rules }, // Rules
			std::span<const uint32_t>{ __resources_wpfLabSurface_program_rule_indexes }, // RuleIndexes
			std::span<const DependencyPropertyReference>{ __resources_wpfLabSurface_program_property_watchers }, // PropertyWatchers
			std::span<const uint32_t>{ __resources_wpfLabSurface_program_data_path_watchers }, // DataPathWatchers
			std::span<const CompiledStyleGroupOp>{ __resources_wpfLabSurface_program_groups }, // Groups
			std::span<const DependencyPropertyReference>{ __resources_wpfLabSurface_program_global_property_watchers }, // GlobalPropertyWatchers
			std::span<const uint32_t>{ __resources_wpfLabSurface_program_global_data_path_watchers }, // GlobalDataPathWatchers
			std::span<const CompiledBindingPathView>{ __resources_wpfLabSurface_program_data_paths }, // DataPaths
		},
		std::vector<BindingValue>{
			BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{1.f, 1.f, 1.f, 1.f}; return value; }()),
			BindingValue(14.0),
			BindingValue(18.0),
			BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.827451f, 0.862745f, 0.909804f, 1.f}; return value; }()),
			BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.207843f, 0.65098f, 0.435294f, 1.f}; return value; }())
		}
	);

	if (!cui::framework::StyleAccess::SetResources(*wpfLabSurface, __resources_wpfLabSurface))
		throw std::runtime_error("Generated local Resources installation failed");

	// XAML authored Local properties/resources
	wpfLabTitle->SetText(L"DispatcherObject → DependencyObject → Visual → UIElement → FrameworkElement → Control · WPF retained object model");
	Canvas::SetLeft(*wpfLabTitle, 12.f);
	Canvas::SetTop(*wpfLabTitle, 8.f);
	wpfLabTitle->SetWidth(cui::layout::Length::Fixed(1260.f));
	wpfLabTitle->SetHeight(cui::layout::Length::Fixed(28.f));
	wpfLabTitle->SetFontSize(18.0);

	// XAML authored Local properties/resources
	Canvas::SetLeft(*wpfBindingScope, 12.f);
	Canvas::SetTop(*wpfBindingScope, 48.f);
	wpfBindingScope->SetWidth(cui::layout::Length::Fixed(390.f));
	wpfBindingScope->SetHeight(cui::layout::Length::Fixed(420.f));
	(void)cui::framework::DependencyPropertyAccess::SetDynamicResource(*wpfBindingScope, Control::FontFamilyProperty(), L"WpfLabFontFamily", DependencyPropertyValueSource::Local);
	(void)cui::framework::DependencyPropertyAccess::SetDynamicResource(*wpfBindingScope, Control::FontSizeProperty(), L"WpfLabFontSize", DependencyPropertyValueSource::Local);

	// XAML authored Local properties/resources
	stackPanel3->SetOrientation(static_cast<Orientation>(1));
	stackPanel3->SetTag(BindingValue(L"FindAncestor · StackPanel"));

	// XAML authored Local properties/resources
	wpfTypographyOverride->SetText(L"Typography: inherited Consolas 15 · local size 14");
	wpfTypographyOverride->SetHeight(cui::layout::Length::Fixed(24.f));
	wpfTypographyOverride->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));
	wpfTypographyOverride->SetFontSize(14.0);

	// XAML authored Local properties/resources
	wpfTwoWayEditor->SetWidth(cui::layout::Length::Fixed(360.f));
	wpfTwoWayEditor->SetHeight(cui::layout::Length::Fixed(28.f));
	wpfTwoWayEditor->SetSelectionBrush([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.486275f, 0.227451f, 0.929412f, 1.f}; return value; }());
	wpfTwoWayEditor->SetSelectionOpacity(0.45);
	wpfTwoWayEditor->SetSelectionTextBrush([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{1.f, 1.f, 1.f, 1.f}; return value; }());
	wpfTwoWayEditor->SetCaretBrush([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.486275f, 0.227451f, 0.929412f, 1.f}; return value; }());

	// XAML authored Local properties/resources
	wpfElementMirror->SetWidth(cui::layout::Length::Fixed(360.f));
	wpfElementMirror->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	wpfSelfValue->SetText(L"RelativeSource Self");
	wpfSelfValue->SetWidth(cui::layout::Length::Fixed(360.f));
	wpfSelfValue->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	wpfAncestorValue->SetText(L"RelativeSource FindAncestor");
	wpfAncestorValue->SetWidth(cui::layout::Length::Fixed(360.f));
	wpfAncestorValue->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	wpfFallbackValue->SetWidth(cui::layout::Length::Fixed(360.f));
	wpfFallbackValue->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	wpfNullValue->SetWidth(cui::layout::Length::Fixed(360.f));
	wpfNullValue->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	wpfIndexerValue->SetWidth(cui::layout::Length::Fixed(360.f));
	wpfIndexerValue->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	wpfKeyedIndexerValue->SetWidth(cui::layout::Length::Fixed(360.f));
	wpfKeyedIndexerValue->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	wpfConvertedValue->SetWidth(cui::layout::Length::Fixed(360.f));
	wpfConvertedValue->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	wpfMultiValue->SetWidth(cui::layout::Length::Fixed(360.f));
	wpfMultiValue->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*wpfTemplateAndStyleScope, 420.f);
	Canvas::SetTop(*wpfTemplateAndStyleScope, 48.f);
	wpfTemplateAndStyleScope->SetWidth(cui::layout::Length::Fixed(390.f));
	wpfTemplateAndStyleScope->SetHeight(cui::layout::Length::Fixed(420.f));

	// XAML authored Local properties/resources
	textBlock33->SetText(L"ControlTemplate + resources + triggers");
	Canvas::SetLeft(*textBlock33, 0.f);
	Canvas::SetTop(*textBlock33, 0.f);
	textBlock33->SetWidth(cui::layout::Length::Fixed(380.f));
	textBlock33->SetHeight(cui::layout::Length::Fixed(24.f));
	textBlock33->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));
	textBlock33->SetFontSize(14.0);

	// XAML authored Local properties/resources
	wpfTemplateButton->SetContent(BindingValue(L"click · swap Control.Template on same Button"));
	Canvas::SetLeft(*wpfTemplateButton, 0.f);
	Canvas::SetTop(*wpfTemplateButton, 36.f);
	wpfTemplateButton->SetWidth(cui::layout::Length::Fixed(360.f));
	wpfTemplateButton->SetHeight(cui::layout::Length::Fixed(112.f));
	wpfTemplateButton->SetAutomationName(L"RelativeSource TemplatedParent");

	cui::framework::StyleAccess::SetResourceKey(*wpfTriggerButton, L"WpfLabLiveButton", false);
	// XAML authored Local properties/resources
	wpfTriggerButton->SetContent(BindingValue(L"Trigger ready"));
	Canvas::SetLeft(*wpfTriggerButton, 0.f);
	Canvas::SetTop(*wpfTriggerButton, 164.f);
	wpfTriggerButton->SetWidth(cui::layout::Length::Fixed(220.f));
	wpfTriggerButton->SetHeight(cui::layout::Length::Fixed(38.f));

	// XAML authored Local properties/resources
	wpfScopeResourceValue->SetText(L"local DynamicResource (green)");
	Canvas::SetLeft(*wpfScopeResourceValue, 0.f);
	Canvas::SetTop(*wpfScopeResourceValue, 220.f);
	wpfScopeResourceValue->SetWidth(cui::layout::Length::Fixed(300.f));
	wpfScopeResourceValue->SetHeight(cui::layout::Length::Fixed(24.f));
	(void)cui::framework::DependencyPropertyAccess::SetDynamicResource(*wpfScopeResourceValue, Control::ForegroundProperty(), L"WpfLabAccent", DependencyPropertyValueSource::Local);

	// 控件级词法资源作用域
	// AOT Style 程序：生成期完成分组、索引和连续池布局
	static constexpr D2D1_COLOR_F __resources_wpfInnerResourceScope_program_values_colors[] = {
		D2D1_COLOR_F{0.941176f, 0.529412f, 0.180392f, 1.f}
	};
	static constexpr std::wstring_view __resources_wpfInnerResourceScope_program_strings[] = {
		L"WpfLabAccent"
	};
	static constexpr CompiledStyleValuePoolView __resources_wpfInnerResourceScope_program_value_pools[] = {
		MakeCompiledStyleValuePoolView(__resources_wpfInnerResourceScope_program_values_colors)
	};
	static const CompiledStyleResourceOp __resources_wpfInnerResourceScope_program_resources[] = {
		{ 0u, MakeCompiledStyleStaticValueReference(0u, 0u) }
	};
	static constexpr uint32_t __resources_wpfInnerResourceScope_program_resource_lookup[] = {
		0u
	};
	auto __resources_wpfInnerResourceScope = ControlStyleSheet::CreateCompiled(
		CompiledStyleProgramView{
			CompiledStyleProgramViewVersion,
			std::span<const std::wstring_view>{ __resources_wpfInnerResourceScope_program_strings }, // Strings
			std::span<const CompiledStyleValuePoolView>{ __resources_wpfInnerResourceScope_program_value_pools }, // ValuePools
			std::span<const CompiledStyleResourceOp>{ __resources_wpfInnerResourceScope_program_resources }, // Resources
			std::span<const uint32_t>{ __resources_wpfInnerResourceScope_program_resource_lookup }, // ResourceLookup
			{}, // PropertyConditions
			{}, // DataConditions
			{}, // Setters
			{}, // PropertyOperands
			{}, // ObjectPathChildIndices
			{}, // ObjectPaths
			{}, // KeyFrames
			{}, // Animations
			{}, // Storyboards
			{}, // Actions
			{}, // Rules
			{}, // RuleIndexes
			{}, // PropertyWatchers
			{}, // DataPathWatchers
			{}, // Groups
			{}, // GlobalPropertyWatchers
			{}, // GlobalDataPathWatchers
			{}, // DataPaths
		},
		std::vector<BindingValue>{}
	);

	if (!cui::framework::StyleAccess::SetResources(*wpfInnerResourceScope, __resources_wpfInnerResourceScope))
		throw std::runtime_error("Generated local Resources installation failed");
	// XAML authored Local properties/resources
	Canvas::SetLeft(*wpfInnerResourceScope, 0.f);
	Canvas::SetTop(*wpfInnerResourceScope, 256.f);
	wpfInnerResourceScope->SetWidth(cui::layout::Length::Fixed(360.f));
	wpfInnerResourceScope->SetHeight(cui::layout::Length::Fixed(90.f));

	// XAML authored Local properties/resources
	wpfInnerResourceValue->SetText(L"nested resource shadow (orange)");
	wpfInnerResourceValue->SetWidth(cui::layout::Length::Fixed(320.f));
	wpfInnerResourceValue->SetHeight(cui::layout::Length::Fixed(24.f));
	(void)cui::framework::DependencyPropertyAccess::SetDynamicResource(*wpfInnerResourceValue, Control::ForegroundProperty(), L"WpfLabAccent", DependencyPropertyValueSource::Local);

	// XAML authored Local properties/resources
	textBlock34->SetText(L"DataTrigger + MultiDataTrigger + MultiTrigger\nEnter/ExitActions run Begin/StopStoryboard");
	Canvas::SetLeft(*textBlock34, 0.f);
	Canvas::SetTop(*textBlock34, 360.f);
	textBlock34->SetWidth(cui::layout::Length::Fixed(360.f));
	textBlock34->SetHeight(cui::layout::Length::Fixed(52.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*wpfItemsScope, 828.f);
	Canvas::SetTop(*wpfItemsScope, 48.f);
	wpfItemsScope->SetWidth(cui::layout::Length::Fixed(465.f));
	wpfItemsScope->SetHeight(cui::layout::Length::Fixed(420.f));

	// XAML authored Local properties/resources
	textBlock35->SetText(L"ItemsPresenter + ListBoxItem + DataTemplate");
	Canvas::SetLeft(*textBlock35, 0.f);
	Canvas::SetTop(*textBlock35, 0.f);
	textBlock35->SetWidth(cui::layout::Length::Fixed(430.f));
	textBlock35->SetHeight(cui::layout::Length::Fixed(24.f));
	textBlock35->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));
	textBlock35->SetFontSize(14.0);

	// XAML authored Local properties/resources
	Canvas::SetLeft(*wpfTemplateList, 0.f);
	Canvas::SetTop(*wpfTemplateList, 36.f);
	wpfTemplateList->SetWidth(cui::layout::Length::Fixed(430.f));
	wpfTemplateList->SetHeight(cui::layout::Length::Fixed(176.f));

	cui::framework::StyleAccess::SetResourceKey(*wpfRouteOuter, L"WpfFocusScopeBorder", false);
	// 控件级词法资源作用域
	// AOT Style 程序：生成期完成分组、索引和连续池布局
	static constexpr Thickness __resources_wpfRouteOuter_program_values_thicknesses[] = {
		Thickness(1.f, 1.f, 1.f, 1.f),
		Thickness(4.f, 4.f, 4.f, 4.f),
		Thickness(3.f, 3.f, 3.f, 3.f)
	};
	static constexpr bool __resources_wpfRouteOuter_program_values_bools[] = {
		true,
		true
	};
	static constexpr CompiledStyleValuePoolView __resources_wpfRouteOuter_program_value_pools[] = {
		MakeCompiledStyleValuePoolView(__resources_wpfRouteOuter_program_values_thicknesses),
		MakeCompiledStyleValuePoolView(__resources_wpfRouteOuter_program_values_bools)
	};
	static const CompiledStylePropertyConditionOp __resources_wpfRouteOuter_program_property_conditions[] = {
		{ DependencyPropertyReference(Control::IsFocusedProperty()), MakeCompiledStyleStaticValueReference(1u, 0u) },
		{ DependencyPropertyReference(Control::IsKeyboardFocusedProperty()), MakeCompiledStyleStaticValueReference(1u, 1u) }
	};
	static const CompiledStyleSetterOp __resources_wpfRouteOuter_program_setters[] = {
		{ DependencyPropertyReference(Control::BorderBrushProperty()), { CompiledStyleOperandKind::Literal, 0u } },
		{ DependencyPropertyReference(Control::BorderThicknessProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(0u, 0u) } },
		{ DependencyPropertyReference(Control::PaddingProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(0u, 1u) } },
		{ DependencyPropertyReference(Control::BorderBrushProperty()), { CompiledStyleOperandKind::Literal, 1u } },
		{ DependencyPropertyReference(Control::BorderBrushProperty()), { CompiledStyleOperandKind::Literal, 2u } },
		{ DependencyPropertyReference(Control::BorderThicknessProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(0u, 2u) } }
	};
	static constexpr CompiledStyleRuleOp __resources_wpfRouteOuter_program_rules[] = {
		{ 1u, 0u, { 0u, 0u }, { 0u, 0u }, { 0u, 3u }, { 0u, 0u }, { 0u, 0u } },
		{ 2u, 1u, { 0u, 1u }, { 0u, 0u }, { 3u, 1u }, { 0u, 0u }, { 0u, 0u } },
		{ 3u, 2u, { 1u, 1u }, { 0u, 0u }, { 4u, 2u }, { 0u, 0u }, { 0u, 0u } }
	};
	static constexpr uint32_t __resources_wpfRouteOuter_program_rule_indexes[] = {
		0u,
		1u,
		2u
	};
	static const DependencyPropertyReference __resources_wpfRouteOuter_program_property_watchers[] = {
		DependencyPropertyReference(Control::IsFocusedProperty()),
		DependencyPropertyReference(Control::IsKeyboardFocusedProperty())
	};
	static constexpr CompiledStyleGroupOp __resources_wpfRouteOuter_program_groups[] = {
		{ true, static_cast<UIClass>(7), ComponentTypeToken{ 0ULL }, CompiledStyleInvalidIndex, { 0u, 3u }, { 0u, 2u }, { 0u, 0u } }
	};
	static const DependencyPropertyReference __resources_wpfRouteOuter_program_global_property_watchers[] = {
		DependencyPropertyReference(Control::IsFocusedProperty()),
		DependencyPropertyReference(Control::IsKeyboardFocusedProperty())
	};
	auto __resources_wpfRouteOuter = ControlStyleSheet::CreateCompiled(
		CompiledStyleProgramView{
			CompiledStyleProgramViewVersion,
			{}, // Strings
			std::span<const CompiledStyleValuePoolView>{ __resources_wpfRouteOuter_program_value_pools }, // ValuePools
			{}, // Resources
			{}, // ResourceLookup
			std::span<const CompiledStylePropertyConditionOp>{ __resources_wpfRouteOuter_program_property_conditions }, // PropertyConditions
			{}, // DataConditions
			std::span<const CompiledStyleSetterOp>{ __resources_wpfRouteOuter_program_setters }, // Setters
			{}, // PropertyOperands
			{}, // ObjectPathChildIndices
			{}, // ObjectPaths
			{}, // KeyFrames
			{}, // Animations
			{}, // Storyboards
			{}, // Actions
			std::span<const CompiledStyleRuleOp>{ __resources_wpfRouteOuter_program_rules }, // Rules
			std::span<const uint32_t>{ __resources_wpfRouteOuter_program_rule_indexes }, // RuleIndexes
			std::span<const DependencyPropertyReference>{ __resources_wpfRouteOuter_program_property_watchers }, // PropertyWatchers
			{}, // DataPathWatchers
			std::span<const CompiledStyleGroupOp>{ __resources_wpfRouteOuter_program_groups }, // Groups
			std::span<const DependencyPropertyReference>{ __resources_wpfRouteOuter_program_global_property_watchers }, // GlobalPropertyWatchers
			{}, // GlobalDataPathWatchers
			{}, // DataPaths
		},
		std::vector<BindingValue>{
			BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.827451f, 0.862745f, 0.909804f, 1.f}; return value; }()),
			BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.941176f, 0.529412f, 0.180392f, 1.f}; return value; }()),
			BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.184314f, 0.435294f, 0.894118f, 1.f}; return value; }())
		}
	);

	if (!cui::framework::StyleAccess::SetResources(*wpfRouteOuter, __resources_wpfRouteOuter))
		throw std::runtime_error("Generated local Resources installation failed");
	// XAML authored Local properties/resources
	Canvas::SetLeft(*wpfRouteOuter, 0.f);
	Canvas::SetTop(*wpfRouteOuter, 224.f);
	wpfRouteOuter->SetWidth(cui::layout::Length::Fixed(430.f));
	wpfRouteOuter->SetHeight(cui::layout::Length::Fixed(188.f));
	wpfRouteOuter->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.968627f, 0.976471f, 0.988235f, 1.f}; return value; }());
	wpfRouteOuter->SetIsFocusScope(true);


	// XAML authored Local properties/resources
	textBlock36->SetText(L"Focusable / IsTabStop + logical / keyboard / within state");
	Canvas::SetLeft(*textBlock36, 10.f);
	Canvas::SetTop(*textBlock36, 7.f);
	textBlock36->SetWidth(cui::layout::Length::Fixed(400.f));
	textBlock36->SetHeight(cui::layout::Length::Fixed(22.f));
	textBlock36->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));
	textBlock36->SetFontSize(13.0);

	// XAML authored Local properties/resources
	Canvas::SetLeft(*wpfRouteMiddle, 10.f);
	Canvas::SetTop(*wpfRouteMiddle, 36.f);
	wpfRouteMiddle->SetWidth(cui::layout::Length::Fixed(408.f));
	wpfRouteMiddle->SetHeight(cui::layout::Length::Fixed(62.f));
	wpfRouteMiddle->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.917647f, 0.94902f, 1.f, 1.f}; return value; }());
	wpfRouteMiddle->SetIsFocusScope(true);
	wpfRouteMiddle->SetTabNavigation(static_cast<KeyboardNavigationMode>(2));
	wpfRouteMiddle->SetDirectionalNavigation(static_cast<KeyboardNavigationMode>(4));

	// XAML authored Local properties/resources
	wpfRouteSource->SetContent(BindingValue(L"Route / Focus A"));
	Canvas::SetLeft(*wpfRouteSource, 8.f);
	Canvas::SetTop(*wpfRouteSource, 13.f);
	wpfRouteSource->SetWidth(cui::layout::Length::Fixed(130.f));
	wpfRouteSource->SetHeight(cui::layout::Length::Fixed(36.f));

	// XAML authored Local properties/resources
	wpfFocusPeerB->SetContent(BindingValue(L"B · no Tab"));
	Canvas::SetLeft(*wpfFocusPeerB, 146.f);
	Canvas::SetTop(*wpfFocusPeerB, 13.f);
	wpfFocusPeerB->SetWidth(cui::layout::Length::Fixed(80.f));
	wpfFocusPeerB->SetHeight(cui::layout::Length::Fixed(36.f));
	wpfFocusPeerB->SetIsTabStop(false);
	wpfFocusPeerB->SetFocusable(true);

	// XAML authored Local properties/resources
	wpfFocusPeerC->SetContent(BindingValue(L"Focus C"));
	Canvas::SetLeft(*wpfFocusPeerC, 234.f);
	Canvas::SetTop(*wpfFocusPeerC, 13.f);
	wpfFocusPeerC->SetWidth(cui::layout::Length::Fixed(80.f));
	wpfFocusPeerC->SetHeight(cui::layout::Length::Fixed(36.f));

	// XAML authored Local properties/resources
	wpfNoFocusPeer->SetContent(BindingValue(L"Blocked"));
	Canvas::SetLeft(*wpfNoFocusPeer, 322.f);
	Canvas::SetTop(*wpfNoFocusPeer, 13.f);
	wpfNoFocusPeer->SetWidth(cui::layout::Length::Fixed(78.f));
	wpfNoFocusPeer->SetHeight(cui::layout::Length::Fixed(36.f));
	wpfNoFocusPeer->SetIsTabStop(true);
	wpfNoFocusPeer->SetFocusable(false);

	// XAML authored Local properties/resources
	wpfTextInputSource->SetText(L"输入文本：PreviewTextInput → behavior → TextInput");
	Canvas::SetLeft(*wpfTextInputSource, 10.f);
	Canvas::SetTop(*wpfTextInputSource, 104.f);
	wpfTextInputSource->SetWidth(cui::layout::Length::Fixed(408.f));
	wpfTextInputSource->SetHeight(cui::layout::Length::Fixed(27.f));

	// XAML authored Local properties/resources
	wpfRouteTrace->SetText(L"T outer → middle → source · B source(H) → outer(too)");
	Canvas::SetLeft(*wpfRouteTrace, 10.f);
	Canvas::SetTop(*wpfRouteTrace, 134.f);
	wpfRouteTrace->SetWidth(cui::layout::Length::Fixed(408.f));
	wpfRouteTrace->SetHeight(cui::layout::Length::Fixed(22.f));
	wpfRouteTrace->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));
	wpfRouteTrace->SetFontSize(10.0);

	// XAML authored Local properties/resources
	wpfInputStats->SetText(L"raw 0 · capture 0/0 · focus 0 · handled skip 0");
	Canvas::SetLeft(*wpfInputStats, 10.f);
	Canvas::SetTop(*wpfInputStats, 158.f);
	wpfInputStats->SetWidth(cui::layout::Length::Fixed(408.f));
	wpfInputStats->SetHeight(cui::layout::Length::Fixed(22.f));
	wpfInputStats->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));
	wpfInputStats->SetFontSize(10.0);

	// XAML authored Local properties/resources
	Canvas::SetLeft(*wpfHierarchyScope, 12.f);
	Canvas::SetTop(*wpfHierarchyScope, 474.f);
	wpfHierarchyScope->SetWidth(cui::layout::Length::Fixed(1280.f));
	wpfHierarchyScope->SetHeight(cui::layout::Length::Fixed(34.f));

	// XAML authored Local properties/resources
	wpfHierarchyChain->SetText(L"FocusScope · logical/keyboard focus · Tab/方向策略 · RoutedCommand");
	Canvas::SetLeft(*wpfHierarchyChain, 0.f);
	Canvas::SetTop(*wpfHierarchyChain, 5.f);
	wpfHierarchyChain->SetWidth(cui::layout::Length::Fixed(690.f));
	wpfHierarchyChain->SetHeight(cui::layout::Length::Fixed(24.f));
	wpfHierarchyChain->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	wpfDispatcherProbe->CommandTarget = wpfHierarchyScope;
	// XAML authored Local properties/resources
	wpfDispatcherProbe->SetContent(BindingValue(L"_Run Dispatcher/命令探针"));
	Canvas::SetLeft(*wpfDispatcherProbe, 700.f);
	Canvas::SetTop(*wpfDispatcherProbe, 0.f);
	wpfDispatcherProbe->SetWidth(cui::layout::Length::Fixed(220.f));
	wpfDispatcherProbe->SetHeight(cui::layout::Length::Fixed(32.f));
	wpfDispatcherProbe->SetPadding(Thickness(6.f, 4.f, 6.f, 4.f));
	wpfDispatcherProbe->SetCommand(L"Demo.Wpf.Probe");
	wpfDispatcherProbe->SetCommandParameter(BindingValue(L"command-button"));

	// XAML authored Local properties/resources
	wpfDispatcherResult->SetText(L"Ready · foreign-thread writes must throw");
	Canvas::SetLeft(*wpfDispatcherResult, 925.f);
	Canvas::SetTop(*wpfDispatcherResult, 5.f);
	wpfDispatcherResult->SetWidth(cui::layout::Length::Fixed(345.f));
	wpfDispatcherResult->SetHeight(cui::layout::Length::Fixed(24.f));

	// XAML authored Local properties/resources
	tabItem10->SetHeader(BindingValue(L"TextComposition/IME"));

	cui::framework::StyleAccess::SetResourceKey(*border22, L"SurfacePanel", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border22, 10.f);
	Canvas::SetTop(*border22, 36.f);
	border22->SetWidth(cui::layout::Length::Fixed(1325.f));
	border22->SetHeight(cui::layout::Length::Fixed(520.f));


	// XAML authored Local properties/resources
	textBlock37->SetText(L"TextCompositionManager · WM_CHAR / WM_UNICHAR / IMM32 → Start / Update / TextInput");
	Canvas::SetLeft(*textBlock37, 12.f);
	Canvas::SetTop(*textBlock37, 8.f);
	textBlock37->SetWidth(cui::layout::Length::Fixed(1260.f));
	textBlock37->SetHeight(cui::layout::Length::Fixed(28.f));
	textBlock37->SetFontSize(18.0);

	// XAML authored Local properties/resources
	Canvas::SetLeft(*border23, 12.f);
	Canvas::SetTop(*border23, 48.f);
	border23->SetWidth(cui::layout::Length::Fixed(620.f));
	border23->SetHeight(cui::layout::Length::Fixed(248.f));
	border23->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.968627f, 0.976471f, 0.988235f, 1.f}; return value; }());
	border23->SetBorderBrush([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.827451f, 0.862745f, 0.909804f, 1.f}; return value; }());
	border23->SetBorderThickness(Thickness(1.f, 1.f, 1.f, 1.f));


	// XAML authored Local properties/resources
	textBlock38->SetText(L"真实系统键盘 / IME 输入客户端");
	Canvas::SetLeft(*textBlock38, 12.f);
	Canvas::SetTop(*textBlock38, 10.f);
	textBlock38->SetWidth(cui::layout::Length::Fixed(580.f));
	textBlock38->SetHeight(cui::layout::Length::Fixed(24.f));
	textBlock38->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));
	textBlock38->SetFontSize(14.0);

	// XAML authored Local properties/resources
	textBlock39->SetText(L"TextBox · 单行 · 完整 UTF-16 一次提交");
	Canvas::SetLeft(*textBlock39, 12.f);
	Canvas::SetTop(*textBlock39, 44.f);
	textBlock39->SetWidth(cui::layout::Length::Fixed(250.f));
	textBlock39->SetHeight(cui::layout::Length::Fixed(22.f));

	// XAML authored Local properties/resources
	compositionTextBox->SetText(L"TextBox: ");
	Canvas::SetLeft(*compositionTextBox, 270.f);
	Canvas::SetTop(*compositionTextBox, 40.f);
	compositionTextBox->SetWidth(cui::layout::Length::Fixed(330.f));
	compositionTextBox->SetHeight(cui::layout::Length::Fixed(30.f));

	// XAML authored Local properties/resources
	textBlock40->SetText(L"RichTextBox · 多行 / Undo / IsReadOnly 规则");
	Canvas::SetLeft(*textBlock40, 12.f);
	Canvas::SetTop(*textBlock40, 86.f);
	textBlock40->SetWidth(cui::layout::Length::Fixed(270.f));
	textBlock40->SetHeight(cui::layout::Length::Fixed(22.f));

	// XAML authored Local properties/resources
	compositionRichTextBox->SetText(L"RichTextBox:\r\n");
	Canvas::SetLeft(*compositionRichTextBox, 270.f);
	Canvas::SetTop(*compositionRichTextBox, 82.f);
	compositionRichTextBox->SetWidth(cui::layout::Length::Fixed(330.f));
	compositionRichTextBox->SetHeight(cui::layout::Length::Fixed(70.f));

	// XAML authored Local properties/resources
	textBlock41->SetText(L"PasswordBox · 提交与日志均不回显正文");
	Canvas::SetLeft(*textBlock41, 12.f);
	Canvas::SetTop(*textBlock41, 166.f);
	textBlock41->SetWidth(cui::layout::Length::Fixed(250.f));
	textBlock41->SetHeight(cui::layout::Length::Fixed(22.f));

	// XAML authored Local properties/resources
	compositionPasswordBox->SetPassword(L"");
	Canvas::SetLeft(*compositionPasswordBox, 270.f);
	Canvas::SetTop(*compositionPasswordBox, 162.f);
	compositionPasswordBox->SetWidth(cui::layout::Length::Fixed(330.f));
	compositionPasswordBox->SetHeight(cui::layout::Length::Fixed(30.f));

	// XAML authored Local properties/resources
	textBlock42->SetText(L"其他客户端：NumericUpDown / ItemsControl / NativeSurface 同链");
	Canvas::SetLeft(*textBlock42, 12.f);
	Canvas::SetTop(*textBlock42, 208.f);
	textBlock42->SetWidth(cui::layout::Length::Fixed(590.f));
	textBlock42->SetHeight(cui::layout::Length::Fixed(22.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*border24, 652.f);
	Canvas::SetTop(*border24, 48.f);
	border24->SetWidth(cui::layout::Length::Fixed(641.f));
	border24->SetHeight(cui::layout::Length::Fixed(248.f));
	border24->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.968627f, 0.976471f, 0.988235f, 1.f}; return value; }());
	border24->SetBorderBrush([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.827451f, 0.862745f, 0.909804f, 1.f}; return value; }());
	border24->SetBorderThickness(Thickness(1.f, 1.f, 1.f, 1.f));


	// XAML authored Local properties/resources
	textBlock43->SetText(L"确定性事务注入（无需安装特定 IME）");
	Canvas::SetLeft(*textBlock43, 12.f);
	Canvas::SetTop(*textBlock43, 10.f);
	textBlock43->SetWidth(cui::layout::Length::Fixed(600.f));
	textBlock43->SetHeight(cui::layout::Length::Fixed(24.f));
	textBlock43->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));
	textBlock43->SetFontSize(14.0);

	// XAML authored Local properties/resources
	compositionStartProbe->SetContent(BindingValue(L"Start"));
	Canvas::SetLeft(*compositionStartProbe, 12.f);
	Canvas::SetTop(*compositionStartProbe, 44.f);
	compositionStartProbe->SetWidth(cui::layout::Length::Fixed(92.f));
	compositionStartProbe->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	compositionUpdateProbe->SetContent(BindingValue(L"Update: ni"));
	Canvas::SetLeft(*compositionUpdateProbe, 112.f);
	Canvas::SetTop(*compositionUpdateProbe, 44.f);
	compositionUpdateProbe->SetWidth(cui::layout::Length::Fixed(110.f));
	compositionUpdateProbe->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	compositionCommitProbe->SetContent(BindingValue(L"Commit: 你😀"));
	Canvas::SetLeft(*compositionCommitProbe, 230.f);
	Canvas::SetTop(*compositionCommitProbe, 44.f);
	compositionCommitProbe->SetWidth(cui::layout::Length::Fixed(124.f));
	compositionCommitProbe->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	compositionCancelProbe->SetContent(BindingValue(L"Cancel"));
	Canvas::SetLeft(*compositionCancelProbe, 362.f);
	Canvas::SetTop(*compositionCancelProbe, 44.f);
	compositionCancelProbe->SetWidth(cui::layout::Length::Fixed(92.f));
	compositionCancelProbe->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	compositionSurrogateProbe->SetContent(BindingValue(L"WM_CHAR 😀"));
	Canvas::SetLeft(*compositionSurrogateProbe, 462.f);
	Canvas::SetTop(*compositionSurrogateProbe, 44.f);
	compositionSurrogateProbe->SetWidth(cui::layout::Length::Fixed(156.f));
	compositionSurrogateProbe->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	compositionUnicharProbe->SetContent(BindingValue(L"WM_UNICHAR 🙂"));
	Canvas::SetLeft(*compositionUnicharProbe, 12.f);
	Canvas::SetTop(*compositionUnicharProbe, 86.f);
	compositionUnicharProbe->SetWidth(cui::layout::Length::Fixed(150.f));
	compositionUnicharProbe->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	compositionFocusProbe->SetContent(BindingValue(L"组合中切换焦点"));
	Canvas::SetLeft(*compositionFocusProbe, 170.f);
	Canvas::SetTop(*compositionFocusProbe, 86.f);
	compositionFocusProbe->SetWidth(cui::layout::Length::Fixed(150.f));
	compositionFocusProbe->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	compositionPreviewHandledProbe->SetContent(BindingValue(L"Preview Handled: off"));
	Canvas::SetLeft(*compositionPreviewHandledProbe, 328.f);
	Canvas::SetTop(*compositionPreviewHandledProbe, 86.f);
	compositionPreviewHandledProbe->SetWidth(cui::layout::Length::Fixed(190.f));
	compositionPreviewHandledProbe->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	compositionResetProbe->SetContent(BindingValue(L"Reset"));
	Canvas::SetLeft(*compositionResetProbe, 526.f);
	Canvas::SetTop(*compositionResetProbe, 86.f);
	compositionResetProbe->SetWidth(cui::layout::Length::Fixed(92.f));
	compositionResetProbe->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	compositionState->SetText(L"Idle · id 0 · caret -1 · source ∅");
	Canvas::SetLeft(*compositionState, 12.f);
	Canvas::SetTop(*compositionState, 134.f);
	compositionState->SetWidth(cui::layout::Length::Fixed(606.f));
	compositionState->SetHeight(cui::layout::Length::Fixed(44.f));

	// XAML authored Local properties/resources
	compositionStats->SetText(L"native 0 · start/update/commit/cancel 0/0/0/0 · applied 0 · echo 0");
	Canvas::SetLeft(*compositionStats, 12.f);
	Canvas::SetTop(*compositionStats, 180.f);
	compositionStats->SetWidth(cui::layout::Length::Fixed(606.f));
	compositionStats->SetHeight(cui::layout::Length::Fixed(44.f));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*border25, 12.f);
	Canvas::SetTop(*border25, 312.f);
	border25->SetWidth(cui::layout::Length::Fixed(1281.f));
	border25->SetHeight(cui::layout::Length::Fixed(190.f));
	border25->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.062745f, 0.094118f, 0.12549f, 1.f}; return value; }());
	border25->SetBorderBrush([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.196078f, 0.262745f, 0.337255f, 1.f}; return value; }());
	border25->SetBorderThickness(Thickness(1.f, 1.f, 1.f, 1.f));


	// XAML authored Local properties/resources
	textBlock44->SetText(L"同一 CompositionId 的 tunnel → source behavior → bubble 时间线");
	Canvas::SetLeft(*textBlock44, 12.f);
	Canvas::SetTop(*textBlock44, 8.f);
	textBlock44->SetWidth(cui::layout::Length::Fixed(780.f));
	textBlock44->SetHeight(cui::layout::Length::Fixed(24.f));
	textBlock44->SetForeground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.552941f, 0.862745f, 0.784314f, 1.f}; return value; }());
	textBlock44->SetFontSize(14.0);

	// XAML authored Local properties/resources
	compositionTrace->SetText(L"等待真实 IME 或确定性探针…");
	Canvas::SetLeft(*compositionTrace, 12.f);
	Canvas::SetTop(*compositionTrace, 38.f);
	compositionTrace->SetWidth(cui::layout::Length::Fixed(1248.f));
	compositionTrace->SetHeight(cui::layout::Length::Fixed(136.f));
	compositionTrace->SetForeground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.839216f, 0.886275f, 0.941176f, 1.f}; return value; }());
	compositionTrace->SetFontFamily(L"Consolas");
	compositionTrace->SetFontSize(12.0);

	// XAML authored Local properties/resources
	tabItem11->SetHeader(BindingValue(L"Presentation/渲染"));

	cui::framework::StyleAccess::SetResourceKey(*border26, L"SurfacePanel", false);
	// XAML authored Local properties/resources
	Canvas::SetLeft(*border26, 10.f);
	Canvas::SetTop(*border26, 36.f);
	border26->SetWidth(cui::layout::Length::Fixed(1325.f));
	border26->SetHeight(cui::layout::Length::Fixed(520.f));


	// XAML authored Local properties/resources
	textBlock45->SetText(L"XAML visual tree → retained PresentationScene → PresentationRenderHost → PlatformWindowHost (HWND)");
	Canvas::SetLeft(*textBlock45, 12.f);
	Canvas::SetTop(*textBlock45, 8.f);
	textBlock45->SetWidth(cui::layout::Length::Fixed(1260.f));
	textBlock45->SetHeight(cui::layout::Length::Fixed(28.f));
	textBlock45->SetFontSize(18.0);

	// XAML authored Local properties/resources
	Canvas::SetLeft(*presentationProbeSurface, 12.f);
	Canvas::SetTop(*presentationProbeSurface, 52.f);
	presentationProbeSurface->SetWidth(cui::layout::Length::Fixed(790.f));
	presentationProbeSurface->SetHeight(cui::layout::Length::Fixed(330.f));
	presentationProbeSurface->SetBehaviorKey(L"PresentationProbe");
	presentationProbeSurface->SetPlaceholderText(L"PresentationProbe behavior 未注册");
	presentationProbeSurface->SetAutomationName(L"Presentation render host dirty-region probe");

	// XAML authored Local properties/resources
	presentationTopologyTile->SetText(L"retained node · ZIndex 5");
	Canvas::SetLeft(*presentationTopologyTile, 606.f);
	Canvas::SetTop(*presentationTopologyTile, 66.f);
	presentationTopologyTile->SetWidth(cui::layout::Length::Fixed(178.f));
	presentationTopologyTile->SetHeight(cui::layout::Length::Fixed(42.f));
	presentationTopologyTile->SetZIndex(5);
	presentationTopologyTile->SetBackground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));
	presentationTopologyTile->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_OnAccent_10));

	// XAML authored Local properties/resources
	Canvas::SetLeft(*canvas14, 828.f);
	Canvas::SetTop(*canvas14, 52.f);
	canvas14->SetWidth(cui::layout::Length::Fixed(465.f));
	canvas14->SetHeight(cui::layout::Length::Fixed(330.f));

	// XAML authored Local properties/resources
	textBlock46->SetText(L"Retained scene snapshot");
	Canvas::SetLeft(*textBlock46, 0.f);
	Canvas::SetTop(*textBlock46, 0.f);
	textBlock46->SetWidth(cui::layout::Length::Fixed(430.f));
	textBlock46->SetHeight(cui::layout::Length::Fixed(26.f));
	textBlock46->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));
	textBlock46->SetFontSize(15.0);

	// XAML authored Local properties/resources
	textBlock47->SetText(L"结构、内容、几何、仅合成拥有独立 revision。VisualChildren / Visibility / ZIndex 才重建结构；布局、Transform、Clip 只使节点几何快照失效。");
	Canvas::SetLeft(*textBlock47, 0.f);
	Canvas::SetTop(*textBlock47, 38.f);
	textBlock47->SetWidth(cui::layout::Length::Fixed(450.f));
	textBlock47->SetHeight(cui::layout::Length::Fixed(66.f));

	// XAML authored Local properties/resources
	textBlock48->SetText(L"Damage replay 与内容重录");
	Canvas::SetLeft(*textBlock48, 0.f);
	Canvas::SetTop(*textBlock48, 118.f);
	textBlock48->SetWidth(cui::layout::Length::Fixed(430.f));
	textBlock48->SetHeight(cui::layout::Length::Fixed(26.f));
	textBlock48->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));
	textBlock48->SetFontSize(15.0);

	// XAML authored Local properties/resources
	textBlock49->SetText(L"同一 segment 清除局部区域后，区域内节点 replay 已改为不可变 command list；只有内容或几何变化才重录，composition-only 和 damage 帧直接复用。");
	Canvas::SetLeft(*textBlock49, 0.f);
	Canvas::SetTop(*textBlock49, 154.f);
	textBlock49->SetWidth(cui::layout::Length::Fixed(450.f));
	textBlock49->SetHeight(cui::layout::Length::Fixed(70.f));

	// XAML authored Local properties/resources
	textBlock50->SetText(L"资源与设备边界");
	Canvas::SetLeft(*textBlock50, 0.f);
	Canvas::SetTop(*textBlock50, 238.f);
	textBlock50->SetWidth(cui::layout::Length::Fixed(430.f));
	textBlock50->SetHeight(cui::layout::Length::Fixed(26.f));
	textBlock50->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_Accent_2));
	textBlock50->SetFontSize(15.0);

	// XAML authored Local properties/resources
	textBlock51->SetText(L"显式 frame transaction 原子管理 primary / scene / overlay。设备恢复提升 resource generation，并统一清空命令与 behavior 设备资源。");
	Canvas::SetLeft(*textBlock51, 0.f);
	Canvas::SetTop(*textBlock51, 274.f);
	textBlock51->SetWidth(cui::layout::Length::Fixed(450.f));
	textBlock51->SetHeight(cui::layout::Length::Fixed(48.f));

	// XAML authored Local properties/resources
	presentationRegionButton->SetContent(BindingValue(L"内容 Pulse"));
	Canvas::SetLeft(*presentationRegionButton, 12.f);
	Canvas::SetTop(*presentationRegionButton, 400.f);
	presentationRegionButton->SetWidth(cui::layout::Length::Fixed(140.f));
	presentationRegionButton->SetHeight(cui::layout::Length::Fixed(34.f));

	// XAML authored Local properties/resources
	presentationGeometryButton->SetContent(BindingValue(L"几何移动"));
	Canvas::SetLeft(*presentationGeometryButton, 162.f);
	Canvas::SetTop(*presentationGeometryButton, 400.f);
	presentationGeometryButton->SetWidth(cui::layout::Length::Fixed(140.f));
	presentationGeometryButton->SetHeight(cui::layout::Length::Fixed(34.f));

	// XAML authored Local properties/resources
	presentationCompositionButton->SetContent(BindingValue(L"仅合成提交"));
	Canvas::SetLeft(*presentationCompositionButton, 312.f);
	Canvas::SetTop(*presentationCompositionButton, 400.f);
	presentationCompositionButton->SetWidth(cui::layout::Length::Fixed(140.f));
	presentationCompositionButton->SetHeight(cui::layout::Length::Fixed(34.f));

	// XAML authored Local properties/resources
	presentationFullButton->SetContent(BindingValue(L"完整帧 replay"));
	Canvas::SetLeft(*presentationFullButton, 462.f);
	Canvas::SetTop(*presentationFullButton, 400.f);
	presentationFullButton->SetWidth(cui::layout::Length::Fixed(140.f));
	presentationFullButton->SetHeight(cui::layout::Length::Fixed(34.f));

	// XAML authored Local properties/resources
	presentationTopologyButton->SetContent(BindingValue(L"切换结构节点"));
	Canvas::SetLeft(*presentationTopologyButton, 612.f);
	Canvas::SetTop(*presentationTopologyButton, 400.f);
	presentationTopologyButton->SetWidth(cui::layout::Length::Fixed(140.f));
	presentationTopologyButton->SetHeight(cui::layout::Length::Fixed(34.f));

	// XAML authored Local properties/resources
	presentationDeviceLossButton->SetContent(BindingValue(L"注入设备丢失"));
	Canvas::SetLeft(*presentationDeviceLossButton, 762.f);
	Canvas::SetTop(*presentationDeviceLossButton, 400.f);
	presentationDeviceLossButton->SetWidth(cui::layout::Length::Fixed(140.f));
	presentationDeviceLossButton->SetHeight(cui::layout::Length::Fixed(34.f));

	// XAML authored Local properties/resources
	presentationStatus->SetText(L"Ready · four independent update lanes");
	Canvas::SetLeft(*presentationStatus, 928.f);
	Canvas::SetTop(*presentationStatus, 398.f);
	presentationStatus->SetWidth(cui::layout::Length::Fixed(365.f));
	presentationStatus->SetHeight(cui::layout::Length::Fixed(44.f));
	presentationStatus->SetForeground(CuiGeneratedBindingValueAs<cui::drawing::Brush>(__documentStaticResource_TextMuted_9));

	// XAML authored Local properties/resources
	textBlock52->SetText(L"依次触发内容重录、几何重录、仅合成复用、完整 replay、结构重建与设备恢复；左侧显示事务、generation 和命令命中统计。类型、属性、事件仍全部由 XAML 定义。");
	Canvas::SetLeft(*textBlock52, 12.f);
	Canvas::SetTop(*textBlock52, 466.f);
	textBlock52->SetWidth(cui::layout::Length::Fixed(1260.f));
	textBlock52->SetHeight(cui::layout::Length::Fixed(32.f));

	// XAML authored Local properties/resources
	Grid::SetRow(*systemContextMenu, 3);

	// XAML authored Local properties/resources
	menuItem6->SetHeader(BindingValue(L"新建项目"));
	menuItem6->SetCommand(L"Demo.System.NewProject");
	menuItem6->SetCommandParameter(L"context-new");

	menuItem7->CommandTarget = mainMenu;
	// XAML authored Local properties/resources
	menuItem7->SetHeader(BindingValue(L"刷新视图"));
	menuItem7->SetCommand(L"Demo.System.Refresh");
	menuItem7->SetCommandParameter(L"context-refresh");
	menuItem7->SetInputGestureText(L"F5");


	// XAML authored Local properties/resources
	menuItem8->SetHeader(BindingValue(L"更多"));

	menuItem9->CommandTarget = systemSurface;
	// XAML authored Local properties/resources
	menuItem9->SetHeader(BindingValue(L"复制信息"));
	menuItem9->SetCommand(L"Demo.System.CopyInfo");
	menuItem9->SetCommandParameter(L"context-copy");

	// XAML authored Local properties/resources
	menuItem10->SetHeader(BindingValue(L"关于此页"));
	menuItem10->SetCommand(L"Demo.System.About");
	menuItem10->SetCommandParameter(L"context-about");

	// XAML authored Local properties/resources
	Grid::SetRow(*mainStatusBar, 4);

	// XAML InputBindings
	(void)this->AddInputBinding(KeyBinding{ RoutedCommand(L"Demo.File.Open"), KeyGesture{ Key::O, ModifierKeys::Control }, std::wstring(L"keyboard-open"), mainMenu });
	(void)this->AddInputBinding(KeyBinding{ RoutedCommand(L"Demo.System.Refresh"), KeyGesture{ Key::F5, ModifierKeys::None }, std::wstring(L"keyboard-refresh"), nullptr });
	(void)this->AddInputBinding(KeyBinding{ RoutedCommand(L"Demo.Help.About"), KeyGesture{ Key::F1, ModifierKeys::None }, std::wstring(L"keyboard-help"), nullptr });
	(void)this->AddInputBinding(MouseBinding{ RoutedCommand(L"Demo.System.Refresh"), MouseGesture{ MouseAction::MiddleClick, ModifierKeys::Control }, std::wstring(L"mouse-refresh"), nullptr });
	(void)featureActionB->AddInputBinding(MouseBinding{ RoutedCommand(L"Demo.Component.ClassProbe"), MouseGesture{ MouseAction::LeftClick, ModifierKeys::None }, std::wstring(L"feature-class-button"), featureCard });
	(void)wpfHierarchyScope->AddInputBinding(KeyBinding{ RoutedCommand(L"Demo.Wpf.Probe"), KeyGesture{ Key::P, ModifierKeys::Control | ModifierKeys::Shift }, std::wstring(L"local-keybinding"), wpfHierarchyScope });
	(void)wpfHierarchyScope->AddInputBinding(MouseBinding{ RoutedCommand(L"Demo.Wpf.Probe"), MouseGesture{ MouseAction::RightClick, ModifierKeys::Alt }, std::wstring(L"local-mousebinding"), wpfHierarchyScope });

	// 绑定事件
	_generatedEventConnections.emplace_back(
		this->OnClosing.Subscribe(std::bind_front(&DemoWindowGenerated::HandleClosing, this)));
	_generatedEventConnections.emplace_back(
		this->ContentRendered.Subscribe(std::bind_front(&DemoWindowGenerated::HandleContentRendered, this)));
	_generatedEventConnections.emplace_back(
		this->OnPreviewExecuted.Subscribe(std::bind_front(&DemoWindowGenerated::HandleCommandPreviewExecuted, this)));
	_generatedEventConnections.emplace_back(
		toolBasic->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleToolBarAction, this)));
	_generatedEventConnections.emplace_back(
		toolData->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleToolBarAction, this)));
	_generatedEventConnections.emplace_back(
		toolAnalytics->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleToolBarAction, this)));
	_generatedEventConnections.emplace_back(
		toolSystem->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleToolBarAction, this)));
	_generatedEventConnections.emplace_back(
		toolIcon1->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleToolBarAction, this)));
	_generatedEventConnections.emplace_back(
		toolIcon2->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleToolBarAction, this)));
	_generatedEventConnections.emplace_back(
		toolIcon3->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleToolBarAction, this)));
	_generatedEventConnections.emplace_back(
		globalProgress->ValueChanged.Subscribe(std::bind_front(&DemoWindowGenerated::HandleGlobalProgress, this)));
	_generatedEventConnections.emplace_back(
		statusText->OnMouseWheel.Subscribe(std::bind_front(&DemoWindowGenerated::HandleMouseWheel, this)));
	_generatedEventConnections.emplace_back(
		basicSurface->OnDeclarativeEvent.Subscribe(
			[this](Control* sender, DeclarativeEventArgs& e)
			{
				if (e.Handled || e.Definition != &DemoWindowGeneratedFeatureCard::InvokedEvent()) return;
				HandleFeatureBubble(sender, e);
			}));
	_generatedEventConnections.emplace_back(
		basicButton->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleBasicClick, this)));
	_generatedEventConnections.emplace_back(
		enableInput->Checked.Subscribe(std::bind_front(&DemoWindowGenerated::HandleEnableInput, this)));
	_generatedEventConnections.emplace_back(
		enableInput->Unchecked.Subscribe(std::bind_front(&DemoWindowGenerated::HandleEnableInput, this)));
	_generatedEventConnections.emplace_back(
		radioA->Checked.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRadio, this)));
	_generatedEventConnections.emplace_back(
		radioA->Unchecked.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRadio, this)));
	_generatedEventConnections.emplace_back(
		radioB->Checked.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRadio, this)));
	_generatedEventConnections.emplace_back(
		radioB->Unchecked.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRadio, this)));
	_generatedEventConnections.emplace_back(
		basicCombo->SelectionChanged.Subscribe(std::bind_front(&DemoWindowGenerated::HandleComboSelection, this)));
	_generatedEventConnections.emplace_back(
		numberInput->ValueChanged.Subscribe(std::bind_front(&DemoWindowGenerated::HandleNumericValue, this)));
	_generatedEventConnections.emplace_back(
		dialogCancelButton->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleBasicClick, this)));
	_generatedEventConnections.emplace_back(
		docsLink->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDocsLink, this)));
	_generatedEventConnections.emplace_back(
		featureCard->SubscribeInvoked(std::bind_front(&DemoWindowGenerated::HandleFeatureInvoked, this)));
	_generatedEventConnections.emplace_back(
		featureActionA->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleCommandAvailabilityToggle, this)));
	_generatedEventConnections.emplace_back(
		basicExpander->Collapsed.Subscribe(std::bind_front(&DemoWindowGenerated::HandleExpander, this)));
	_generatedEventConnections.emplace_back(
		basicExpander->Expanded.Subscribe(std::bind_front(&DemoWindowGenerated::HandleExpander, this)));
	_generatedEventConnections.emplace_back(
		containerSurface->OnDragEnter.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		containerSurface->OnDragLeave.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		containerSurface->OnDragOver.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		containerSurface->OnDrop.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		containerSurface->OnPreviewDragEnter.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		containerSurface->OnPreviewDragLeave.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		containerSurface->OnPreviewDragOver.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		containerSurface->OnPreviewDrop.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		openImageButton->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleOpenImage, this)));
	_generatedEventConnections.emplace_back(
		demoImage->OnDragEnter.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		demoImage->OnDragLeave.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		demoImage->OnDragOver.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		demoImage->OnDrop.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDropImage, this)));
	_generatedEventConnections.emplace_back(
		demoImage->OnPreviewDragEnter.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		demoImage->OnPreviewDragLeave.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		demoImage->OnPreviewDragOver.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		demoImage->OnPreviewDrop.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDragRoute, this)));
	_generatedEventConnections.emplace_back(
		imageVisible->Checked.Subscribe(std::bind_front(&DemoWindowGenerated::HandleImageVisibility, this)));
	_generatedEventConnections.emplace_back(
		imageVisible->Unchecked.Subscribe(std::bind_front(&DemoWindowGenerated::HandleImageVisibility, this)));
	_generatedEventConnections.emplace_back(
		sideNavigationList->SelectionChanged.Subscribe(std::bind_front(&DemoWindowGenerated::HandleListBoxSelection, this)));
	_generatedEventConnections.emplace_back(
		demoTree->SelectedItemChanged.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTreeSelection, this)));
	_generatedEventConnections.emplace_back(
		demoListBox->SelectionChanged.Subscribe(std::bind_front(&DemoWindowGenerated::HandleListBoxSelection, this)));
	_generatedEventConnections.emplace_back(
		demoList->SelectionChanged.Subscribe(std::bind_front(&DemoWindowGenerated::HandleListViewSelection, this)));
	_generatedEventConnections.emplace_back(
		authoredStateTree->SelectedItemChanged.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTreeSelection, this)));
	_generatedEventConnections.emplace_back(
		analyticsApply->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleAnalyticsAction, this)));
	_generatedEventConnections.emplace_back(
		analyticsReset->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleAnalyticsAction, this)));
	_generatedEventConnections.emplace_back(
		chartBar->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleChartKind, this)));
	_generatedEventConnections.emplace_back(
		chartPie->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleChartKind, this)));
	_generatedEventConnections.emplace_back(
		chartLine->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleChartKind, this)));
	_generatedEventConnections.emplace_back(
		salesChart->OnPointClick.Subscribe(std::bind_front(&DemoWindowGenerated::HandleChartPoint, this)));
	_generatedEventConnections.emplace_back(
		analyticsRows->SelectionChanged.Subscribe(std::bind_front(&DemoWindowGenerated::HandleListViewSelection, this)));
	_generatedEventConnections.emplace_back(
		farButton->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleFarButton, this)));
	_generatedEventConnections.emplace_back(
		systemSurface->OnMouseUp.Subscribe(std::bind_front(&DemoWindowGenerated::HandleSystemSurfaceMouseUp, this)));
	_generatedEventConnections.emplace_back(
		notifyToggle->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleSystemAction, this)));
	_generatedEventConnections.emplace_back(
		notifyBalloon->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleSystemAction, this)));
	_generatedEventConnections.emplace_back(
		showDialog->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleSystemAction, this)));
	_generatedEventConnections.emplace_back(
		showToast->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleSystemAction, this)));
	_generatedEventConnections.emplace_back(
		dismissToast->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleSystemAction, this)));
	_generatedEventConnections.emplace_back(
		invokeWeb->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleInvokeWeb, this)));
	_generatedEventConnections.emplace_back(
		mediaPlayer->OnMediaEnded.Subscribe(std::bind_front(&DemoWindowGenerated::HandleMediaEnded, this)));
	_generatedEventConnections.emplace_back(
		mediaPlayer->OnMediaFailed.Subscribe(std::bind_front(&DemoWindowGenerated::HandleMediaFailed, this)));
	_generatedEventConnections.emplace_back(
		mediaPlayer->OnMediaOpened.Subscribe(std::bind_front(&DemoWindowGenerated::HandleMediaOpened, this)));
	_generatedEventConnections.emplace_back(
		mediaPlayer->OnPositionChanged.Subscribe(std::bind_front(&DemoWindowGenerated::HandleMediaPosition, this)));
	_generatedEventConnections.emplace_back(
		mediaOpen->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleMediaCommand, this)));
	_generatedEventConnections.emplace_back(
		mediaPlay->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleMediaCommand, this)));
	_generatedEventConnections.emplace_back(
		mediaPause->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleMediaCommand, this)));
	_generatedEventConnections.emplace_back(
		mediaStop->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleMediaCommand, this)));
	_generatedEventConnections.emplace_back(
		mediaVolume->ValueChanged.Subscribe(std::bind_front(&DemoWindowGenerated::HandleMediaVolume, this)));
	_generatedEventConnections.emplace_back(
		mediaSpeed->ValueChanged.Subscribe(std::bind_front(&DemoWindowGenerated::HandleMediaSpeed, this)));
	_generatedEventConnections.emplace_back(
		mediaLoop->Checked.Subscribe(std::bind_front(&DemoWindowGenerated::HandleMediaLoop, this)));
	_generatedEventConnections.emplace_back(
		mediaLoop->Unchecked.Subscribe(std::bind_front(&DemoWindowGenerated::HandleMediaLoop, this)));
	_generatedEventConnections.emplace_back(
		mediaProgress->ValueChanged.Subscribe(std::bind_front(&DemoWindowGenerated::HandleMediaSeek, this)));
	_generatedEventConnections.emplace_back(
		wpfTemplateButton->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTemplateSwap, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteOuter->OnKeyDown.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRouteKey, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteOuter->OnPreviewKeyDown.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRouteKey, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteOuter->OnPreviewMouseDown.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRouteOuterPreview, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteOuter->OnPreviewTextInput.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTextOuterPreview, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteOuter->OnTextInput.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTextOuterBubble, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteMiddle->OnMouseDown.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRouteMiddleBubble, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteMiddle->OnPreviewMouseDown.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRouteMiddlePreview, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteSource->OnGotFocus.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRouteFocus, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteSource->OnGotKeyboardFocus.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRouteGotKeyboardFocus, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteSource->OnGotMouseCapture.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRouteCaptureChanged, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteSource->OnKeyDown.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRouteKey, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteSource->OnLostKeyboardFocus.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRouteLostKeyboardFocus, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteSource->OnLostMouseCapture.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRouteCaptureChanged, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteSource->OnMouseDown.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRouteSourceBubble, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteSource->OnPreviewGotKeyboardFocus.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRoutePreviewGotKeyboardFocus, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteSource->OnPreviewKeyDown.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRouteKey, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteSource->OnPreviewLostKeyboardFocus.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRoutePreviewLostKeyboardFocus, this)));
	_generatedEventConnections.emplace_back(
		wpfRouteSource->OnPreviewMouseDown.Subscribe(std::bind_front(&DemoWindowGenerated::HandleRouteSourcePreview, this)));
	_generatedEventConnections.emplace_back(
		wpfTextInputSource->OnPreviewTextInput.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTextSourcePreview, this)));
	_generatedEventConnections.emplace_back(
		wpfTextInputSource->OnTextInput.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTextSourceBubble, this)));
	_generatedEventConnections.emplace_back(
		wpfDispatcherProbe->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleDispatcherProbe, this)));
	_generatedEventConnections.emplace_back(
		textCompositionLabSurface->OnPreviewTextInput.Subscribe(std::bind_front(&DemoWindowGenerated::HandleCompositionPreviewCommit, this)));
	_generatedEventConnections.emplace_back(
		textCompositionLabSurface->OnPreviewTextInputStart.Subscribe(std::bind_front(&DemoWindowGenerated::HandleCompositionPreviewStart, this)));
	_generatedEventConnections.emplace_back(
		textCompositionLabSurface->OnPreviewTextInputUpdate.Subscribe(std::bind_front(&DemoWindowGenerated::HandleCompositionPreviewUpdate, this)));
	_generatedEventConnections.emplace_back(
		textCompositionLabSurface->OnTextInput.Subscribe(std::bind_front(&DemoWindowGenerated::HandleCompositionCommit, this)));
	_generatedEventConnections.emplace_back(
		textCompositionLabSurface->OnTextInputStart.Subscribe(std::bind_front(&DemoWindowGenerated::HandleCompositionStart, this)));
	_generatedEventConnections.emplace_back(
		textCompositionLabSurface->OnTextInputUpdate.Subscribe(std::bind_front(&DemoWindowGenerated::HandleCompositionUpdate, this)));
	_generatedEventConnections.emplace_back(
		compositionStartProbe->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTextCompositionProbe, this)));
	_generatedEventConnections.emplace_back(
		compositionUpdateProbe->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTextCompositionProbe, this)));
	_generatedEventConnections.emplace_back(
		compositionCommitProbe->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTextCompositionProbe, this)));
	_generatedEventConnections.emplace_back(
		compositionCancelProbe->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTextCompositionProbe, this)));
	_generatedEventConnections.emplace_back(
		compositionSurrogateProbe->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTextCompositionProbe, this)));
	_generatedEventConnections.emplace_back(
		compositionUnicharProbe->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTextCompositionProbe, this)));
	_generatedEventConnections.emplace_back(
		compositionFocusProbe->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTextCompositionProbe, this)));
	_generatedEventConnections.emplace_back(
		compositionPreviewHandledProbe->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTextCompositionProbe, this)));
	_generatedEventConnections.emplace_back(
		compositionResetProbe->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandleTextCompositionProbe, this)));
	_generatedEventConnections.emplace_back(
		presentationRegionButton->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandlePresentationRegion, this)));
	_generatedEventConnections.emplace_back(
		presentationGeometryButton->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandlePresentationGeometry, this)));
	_generatedEventConnections.emplace_back(
		presentationCompositionButton->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandlePresentationComposition, this)));
	_generatedEventConnections.emplace_back(
		presentationFullButton->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandlePresentationFullFrame, this)));
	_generatedEventConnections.emplace_back(
		presentationTopologyButton->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandlePresentationTopology, this)));
	_generatedEventConnections.emplace_back(
		presentationDeviceLossButton->Click.Subscribe(std::bind_front(&DemoWindowGenerated::HandlePresentationDeviceLoss, this)));

	// XAML CommandBindings
	{
		CommandBinding __commandBinding;
		__commandBinding.Command = RoutedCommand(L"Demo.File.Open");
		__commandBinding.PreviewCanExecute = [this](Control* sender, CanExecuteRoutedEventArgs& e) { HandleCommandPreviewCanExecute(sender, e); };
		__commandBinding.CanExecute = [this](Control* sender, CanExecuteRoutedEventArgs& e) { HandleCommandCanExecute(sender, e); };
		__commandBinding.Executed = [this](Control* sender, ExecutedRoutedEventArgs& e) { HandleCommandExecuted(sender, e); };
		_generatedEventConnections.emplace_back(this->AddCommandBinding(std::move(__commandBinding)));
	}
	{
		CommandBinding __commandBinding;
		__commandBinding.Command = RoutedCommand(L"Demo.File.Exit");
		__commandBinding.CanExecute = [this](Control* sender, CanExecuteRoutedEventArgs& e) { HandleCommandCanExecute(sender, e); };
		__commandBinding.Executed = [this](Control* sender, ExecutedRoutedEventArgs& e) { HandleCommandExecuted(sender, e); };
		_generatedEventConnections.emplace_back(this->AddCommandBinding(std::move(__commandBinding)));
	}
	{
		CommandBinding __commandBinding;
		__commandBinding.Command = RoutedCommand(L"Demo.Help.About");
		__commandBinding.CanExecute = [this](Control* sender, CanExecuteRoutedEventArgs& e) { HandleCommandCanExecute(sender, e); };
		__commandBinding.Executed = [this](Control* sender, ExecutedRoutedEventArgs& e) { HandleCommandExecuted(sender, e); };
		_generatedEventConnections.emplace_back(this->AddCommandBinding(std::move(__commandBinding)));
	}
	{
		CommandBinding __commandBinding;
		__commandBinding.Command = RoutedCommand(L"Demo.System.NewProject");
		__commandBinding.CanExecute = [this](Control* sender, CanExecuteRoutedEventArgs& e) { HandleCommandCanExecute(sender, e); };
		__commandBinding.Executed = [this](Control* sender, ExecutedRoutedEventArgs& e) { HandleCommandExecuted(sender, e); };
		_generatedEventConnections.emplace_back(this->AddCommandBinding(std::move(__commandBinding)));
	}
	{
		CommandBinding __commandBinding;
		__commandBinding.Command = RoutedCommand(L"Demo.System.Refresh");
		__commandBinding.CanExecute = [this](Control* sender, CanExecuteRoutedEventArgs& e) { HandleCommandCanExecute(sender, e); };
		__commandBinding.Executed = [this](Control* sender, ExecutedRoutedEventArgs& e) { HandleCommandExecuted(sender, e); };
		_generatedEventConnections.emplace_back(this->AddCommandBinding(std::move(__commandBinding)));
	}
	{
		CommandBinding __commandBinding;
		__commandBinding.Command = RoutedCommand(L"Demo.System.CopyInfo");
		__commandBinding.CanExecute = [this](Control* sender, CanExecuteRoutedEventArgs& e) { HandleCommandCanExecute(sender, e); };
		__commandBinding.Executed = [this](Control* sender, ExecutedRoutedEventArgs& e) { HandleCommandExecuted(sender, e); };
		_generatedEventConnections.emplace_back(this->AddCommandBinding(std::move(__commandBinding)));
	}
	{
		CommandBinding __commandBinding;
		__commandBinding.Command = RoutedCommand(L"Demo.System.About");
		__commandBinding.CanExecute = [this](Control* sender, CanExecuteRoutedEventArgs& e) { HandleCommandCanExecute(sender, e); };
		__commandBinding.Executed = [this](Control* sender, ExecutedRoutedEventArgs& e) { HandleCommandExecuted(sender, e); };
		_generatedEventConnections.emplace_back(this->AddCommandBinding(std::move(__commandBinding)));
	}
	{
		CommandBinding __commandBinding;
		__commandBinding.Command = RoutedCommand(L"Demo.System.ShowWindow");
		__commandBinding.CanExecute = [this](Control* sender, CanExecuteRoutedEventArgs& e) { HandleCommandCanExecute(sender, e); };
		__commandBinding.Executed = [this](Control* sender, ExecutedRoutedEventArgs& e) { HandleCommandExecuted(sender, e); };
		_generatedEventConnections.emplace_back(this->AddCommandBinding(std::move(__commandBinding)));
	}
	{
		CommandBinding __commandBinding;
		__commandBinding.Command = RoutedCommand(L"Demo.Wpf.Probe");
		__commandBinding.CanExecute = [this](Control* sender, CanExecuteRoutedEventArgs& e) { HandleLocalCommandCanExecute(sender, e); };
		__commandBinding.Executed = [this](Control* sender, ExecutedRoutedEventArgs& e) { HandleLocalCommandExecuted(sender, e); };
		_generatedEventConnections.emplace_back(wpfHierarchyScope->AddCommandBinding(std::move(__commandBinding)));
	}

	// 组装控件层级（包含布局容器）
	this->SetVisualContent(std::move(__owned_windowContent));
	windowContent->AddOwned(std::move(__owned_mainMenu));
	mainMenu->AddItemControl(std::move(__owned_menuItem1));
	menuItem1->AddItemControl(std::move(__owned_menuItem2));

	menuItem1->AddItemControl(std::move(__owned_separator1));

	menuItem1->AddItemControl(std::move(__owned_menuItem3));


	mainMenu->AddItemControl(std::move(__owned_menuItem4));
	menuItem4->AddItemControl(std::move(__owned_menuItem5));



	windowContent->AddOwned(std::move(__owned_mainToolBar));
	mainToolBar->AddItemControl(std::move(__owned_toolBasic));

	mainToolBar->AddItemControl(std::move(__owned_toolData));

	mainToolBar->AddItemControl(std::move(__owned_toolAnalytics));

	mainToolBar->AddItemControl(std::move(__owned_toolSystem));

	mainToolBar->AddItemControl(std::move(__owned_toolSeparator));

	mainToolBar->AddItemControl(std::move(__owned_toolIcon1));
	toolIcon1->SetVisualContent(std::move(__owned_toolIconImage1));


	mainToolBar->AddItemControl(std::move(__owned_toolIcon2));
	toolIcon2->SetVisualContent(std::move(__owned_toolIconImage2));


	mainToolBar->AddItemControl(std::move(__owned_toolIcon3));
	toolIcon3->SetVisualContent(std::move(__owned_toolIconImage3));



	windowContent->AddOwned(std::move(__owned_canvas1));
	canvas1->AddOwned(std::move(__owned_globalProgress));

	canvas1->AddOwned(std::move(__owned_statusText));

	canvas1->AddOwned(std::move(__owned_runtimeBadge));


	windowContent->AddOwned(std::move(__owned_mainTabs));
	mainTabs->AddItemControl(std::move(__owned_tabItem1));
	tabItem1->SetVisualContent(std::move(__owned_border1));
	border1->SetChild(std::move(__owned_basicSurface));
	basicSurface->AddOwned(std::move(__owned_basicTitle));

	basicSurface->AddOwned(std::move(__owned_frameworkThemeHint));

	basicSurface->AddOwned(std::move(__owned_basicButton));

	basicSurface->AddOwned(std::move(__owned_enableInput));

	basicSurface->AddOwned(std::move(__owned_radioA));

	basicSurface->AddOwned(std::move(__owned_radioB));

	basicSurface->AddOwned(std::move(__owned_nameInput));

	basicSurface->AddOwned(std::move(__owned_passwordInput));

	basicSurface->AddOwned(std::move(__owned_basicCombo));

	basicSurface->AddOwned(std::move(__owned_dateInput));

	basicSurface->AddOwned(std::move(__owned_numberInput));

	basicSurface->AddOwned(std::move(__owned_dialogCancelButton));

	basicSurface->AddOwned(std::move(__owned_docsLink));

	basicSurface->AddOwned(std::move(__owned_textBlock1));

	basicSurface->AddOwned(std::move(__owned_verticalThemeSlider));

	basicSurface->AddOwned(std::move(__owned_verticalThemeProgress));

	basicSurface->AddOwned(std::move(__owned_gradientInput));

	basicSurface->AddOwned(std::move(__owned_gradientLabel));

	basicSurface->AddOwned(std::move(__owned_featureCard));
	if (!featureCard->SetContent(std::move(__owned_featureCardContent)))
		throw std::runtime_error("Generated component content attachment failed");

	if (!featureCard->AddActions(std::move(__owned_featureActionA)))
		throw std::runtime_error("Generated component content attachment failed");

	if (!featureCard->AddActions(std::move(__owned_featureActionB)))
		throw std::runtime_error("Generated component content attachment failed");


	basicSurface->AddOwned(std::move(__owned_basicGroup));
	basicGroup->SetVisualContent(std::move(__owned_basicGroupContent));
	basicGroupContent->AddOwned(std::move(__owned_groupHint));

	basicGroupContent->AddOwned(std::move(__owned_groupName));

	basicGroupContent->AddOwned(std::move(__owned_groupEnabled));

	basicGroupContent->AddOwned(std::move(__owned_themeNormalButton));

	basicGroupContent->AddOwned(std::move(__owned_themeDisabledButton));



	basicSurface->AddOwned(std::move(__owned_basicExpander));
	basicExpander->SetVisualContent(std::move(__owned_basicExpanderContent));
	basicExpanderContent->AddOwned(std::move(__owned_expanderText));



	basicSurface->AddOwned(std::move(__owned_themeContentControlProbe));

	basicSurface->AddOwned(std::move(__owned_themeItemsControlProbe));
	themeItemsControlProbe->AddItemControl(std::move(__owned_textBlock2));


	basicSurface->AddOwned(std::move(__owned_themeSeparatorProbe));




	mainTabs->AddItemControl(std::move(__owned_tabItem2));
	tabItem2->SetVisualContent(std::move(__owned_border2));
	border2->SetChild(std::move(__owned_containerSurface));
	containerSurface->AddOwned(std::move(__owned_openImageButton));

	containerSurface->AddOwned(std::move(__owned_demoImage));

	containerSurface->AddOwned(std::move(__owned_demoProgress));

	containerSurface->AddOwned(std::move(__owned_indeterminateProgress));

	containerSurface->AddOwned(std::move(__owned_loadingRing));

	containerSurface->AddOwned(std::move(__owned_progressRing));

	containerSurface->AddOwned(std::move(__owned_imageVisible));

	containerSurface->AddOwned(std::move(__owned_imageVisibleLabel));

	containerSurface->AddOwned(std::move(__owned_demoScene));

	containerSurface->AddOwned(std::move(__owned_detailGrid));
	detailGrid->AddOwned(std::move(__owned_navigationComposition));
	navigationComposition->AddOwned(std::move(__owned_textBlock3));

	navigationComposition->AddOwned(std::move(__owned_sideNavigationList));


	detailGrid->AddOwned(std::move(__owned_canvas2));

	detailGrid->AddOwned(std::move(__owned_detailComposition));
	detailComposition->AddOwned(std::move(__owned_stackPanel1));
	stackPanel1->AddOwned(std::move(__owned_textBlock4));

	stackPanel1->AddOwned(std::move(__owned_textBlock5));

	stackPanel1->AddOwned(std::move(__owned_textBlock6));

	stackPanel1->AddOwned(std::move(__owned_textBlock7));

	stackPanel1->AddOwned(std::move(__owned_textBlock8));


	detailComposition->AddOwned(std::move(__owned_splitNotes));



	containerSurface->AddOwned(std::move(__owned_containerGroup));
	containerGroup->SetVisualContent(std::move(__owned_containerGroupText));





	mainTabs->AddItemControl(std::move(__owned_tabItem3));
	tabItem3->SetVisualContent(std::move(__owned_border3));
	border3->SetChild(std::move(__owned_dataSurface));
	dataSurface->AddOwned(std::move(__owned_demoTree));

	dataSurface->AddOwned(std::move(__owned_demoListBox));

	dataSurface->AddOwned(std::move(__owned_demoList));

	dataSurface->AddOwned(std::move(__owned_composedPropertyEditor));
	composedPropertyEditor->SetVisualContent(std::move(__owned_canvas3));
	canvas3->AddOwned(std::move(__owned_textBlock9));

	canvas3->AddOwned(std::move(__owned_composedTitleEditor));

	canvas3->AddOwned(std::move(__owned_textBlock10));

	canvas3->AddOwned(std::move(__owned_composedEnabledEditor));

	canvas3->AddOwned(std::move(__owned_textBlock11));

	canvas3->AddOwned(std::move(__owned_composedDensityEditor));
	composedDensityEditor->AddItemControl(std::move(__owned_comboBoxItem1));

	composedDensityEditor->AddItemControl(std::move(__owned_comboBoxItem2));

	composedDensityEditor->AddItemControl(std::move(__owned_comboBoxItem3));


	canvas3->AddOwned(std::move(__owned_textBlock12));

	canvas3->AddOwned(std::move(__owned_composedScaleEditor));

	canvas3->AddOwned(std::move(__owned_textBlock13));



	dataSurface->AddOwned(std::move(__owned_authoredStateTree));
	authoredStateTree->AddItemControl(std::move(__owned_treeViewItem1));
	treeViewItem1->AddItemControl(std::move(__owned_treeViewItem2));

	treeViewItem1->AddItemControl(std::move(__owned_treeViewItem3));


	authoredStateTree->AddItemControl(std::move(__owned_treeViewItem4));


	dataSurface->AddOwned(std::move(__owned_textBlock14));




	mainTabs->AddItemControl(std::move(__owned_tabItem4));
	tabItem4->SetVisualContent(std::move(__owned_border4));
	border4->SetChild(std::move(__owned_analyticsSurface));
	analyticsSurface->AddOwned(std::move(__owned_border5));
	border5->SetChild(std::move(__owned_analyticsFilterSurface));
	analyticsFilterSurface->AddOwned(std::move(__owned_analyticsQuery));

	analyticsFilterSurface->AddOwned(std::move(__owned_analyticsClosed));

	analyticsFilterSurface->AddOwned(std::move(__owned_analyticsContract));

	analyticsFilterSurface->AddOwned(std::move(__owned_analyticsHighMargin));

	analyticsFilterSurface->AddOwned(std::move(__owned_analyticsApply));

	analyticsFilterSurface->AddOwned(std::move(__owned_analyticsReset));



	analyticsSurface->AddOwned(std::move(__owned_groupBox1));
	groupBox1->SetVisualContent(std::move(__owned_canvas4));
	canvas4->AddOwned(std::move(__owned_textBlock15));

	canvas4->AddOwned(std::move(__owned_textBlock16));



	analyticsSurface->AddOwned(std::move(__owned_groupBox2));
	groupBox2->SetVisualContent(std::move(__owned_canvas5));
	canvas5->AddOwned(std::move(__owned_textBlock17));

	canvas5->AddOwned(std::move(__owned_progressBar1));



	analyticsSurface->AddOwned(std::move(__owned_groupBox3));
	groupBox3->SetVisualContent(std::move(__owned_canvas6));
	canvas6->AddOwned(std::move(__owned_textBlock18));

	canvas6->AddOwned(std::move(__owned_textBlock19));



	analyticsSurface->AddOwned(std::move(__owned_chartBar));

	analyticsSurface->AddOwned(std::move(__owned_chartPie));

	analyticsSurface->AddOwned(std::move(__owned_chartLine));

	analyticsSurface->AddOwned(std::move(__owned_salesChart));

	analyticsSurface->AddOwned(std::move(__owned_analyticsReport));
	analyticsReport->SetVisualContent(std::move(__owned_canvas7));
	canvas7->AddOwned(std::move(__owned_stackPanel2));
	stackPanel2->AddOwned(std::move(__owned_textBlock20));

	stackPanel2->AddOwned(std::move(__owned_textBlock21));

	stackPanel2->AddOwned(std::move(__owned_textBlock22));

	stackPanel2->AddOwned(std::move(__owned_textBlock23));

	stackPanel2->AddOwned(std::move(__owned_textBlock24));


	canvas7->AddOwned(std::move(__owned_analyticsRows));

	canvas7->AddOwned(std::move(__owned_textBlock25));






	mainTabs->AddItemControl(std::move(__owned_tabItem5));
	tabItem5->SetVisualContent(std::move(__owned_border6));
	border6->SetChild(std::move(__owned_layoutSurface));
	layoutSurface->AddOwned(std::move(__owned_layoutTitle));

	layoutSurface->AddOwned(std::move(__owned_canvasSemanticsProbe));
	canvasSemanticsProbe->AddOwned(std::move(__owned_border7));

	canvasSemanticsProbe->AddOwned(std::move(__owned_canvasLeftWins));

	canvasSemanticsProbe->AddOwned(std::move(__owned_canvasRightBottom));


	layoutSurface->AddOwned(std::move(__owned_border8));
	border8->SetChild(std::move(__owned_demoStack));
	demoStack->AddOwned(std::move(__owned_stackA));

	demoStack->AddOwned(std::move(__owned_stackB));

	demoStack->AddOwned(std::move(__owned_stackC));



	layoutSurface->AddOwned(std::move(__owned_border9));
	border9->SetChild(std::move(__owned_demoGrid));
	demoGrid->AddOwned(std::move(__owned_gridHeader));

	demoGrid->AddOwned(std::move(__owned_gridLeft));

	demoGrid->AddOwned(std::move(__owned_gridEditor));

	demoGrid->AddOwned(std::move(__owned_gridFooter));



	layoutSurface->AddOwned(std::move(__owned_border10));
	border10->SetChild(std::move(__owned_demoDock));
	demoDock->AddOwned(std::move(__owned_dockTop));

	demoDock->AddOwned(std::move(__owned_dockLeft));

	demoDock->AddOwned(std::move(__owned_dockFill));



	layoutSurface->AddOwned(std::move(__owned_border11));
	border11->SetChild(std::move(__owned_demoWrap));
	demoWrap->AddOwned(std::move(__owned_wrap1));

	demoWrap->AddOwned(std::move(__owned_wrap2));

	demoWrap->AddOwned(std::move(__owned_wrap3));

	demoWrap->AddOwned(std::move(__owned_wrap4));

	demoWrap->AddOwned(std::move(__owned_wrap5));

	demoWrap->AddOwned(std::move(__owned_wrap6));



	layoutSurface->AddOwned(std::move(__owned_border12));
	border12->SetChild(std::move(__owned_demoRelative));
	demoRelative->AddOwned(std::move(__owned_relativeCenter));
	relativeCenter->AddOwned(std::move(__owned_naturalTextProbe));

	relativeCenter->AddOwned(std::move(__owned_wrappedTextProbe));

	relativeCenter->AddOwned(std::move(__owned_trimmedTextProbe));

	relativeCenter->AddOwned(std::move(__owned_relativeCenterButton));




	layoutSurface->AddOwned(std::move(__owned_border13));
	border13->SetChild(std::move(__owned_demoScroll));
	demoScroll->SetVisualContent(std::move(__owned_demoScrollContent));
	demoScrollContent->AddOwned(std::move(__owned_border14));
	border14->SetChild(std::move(__owned_scrollCard1));
	scrollCard1->AddOwned(std::move(__owned_scrollCard1Text));



	demoScrollContent->AddOwned(std::move(__owned_border15));
	border15->SetChild(std::move(__owned_scrollCard2));
	scrollCard2->AddOwned(std::move(__owned_scrollCard2Text));



	demoScrollContent->AddOwned(std::move(__owned_farButton));







	mainTabs->AddItemControl(std::move(__owned_tabItem6));
	tabItem6->SetVisualContent(std::move(__owned_border16));
	border16->SetChild(std::move(__owned_systemSurface));
	systemSurface->AddOwned(std::move(__owned_systemTitle));

	systemSurface->AddOwned(std::move(__owned_notifyToggle));

	systemSurface->AddOwned(std::move(__owned_notifyBalloon));

	systemSurface->AddOwned(std::move(__owned_showDialog));

	systemSurface->AddOwned(std::move(__owned_showToast));

	systemSurface->AddOwned(std::move(__owned_systemHint));

	systemSurface->AddOwned(std::move(__owned_border17));
	border17->SetChild(std::move(__owned_canvas8));
	canvas8->AddOwned(std::move(__owned_textBlock26));

	canvas8->AddOwned(std::move(__owned_commandTargetButton));

	canvas8->AddOwned(std::move(__owned_textBlock27));

	canvas8->AddOwned(std::move(__owned_commandTargetTrace));

	canvas8->AddOwned(std::move(__owned_textBlock28));

	canvas8->AddOwned(std::move(__owned_textBlock29));

	canvas8->AddOwned(std::move(__owned_textBlock30));



	systemSurface->AddOwned(std::move(__owned_notificationPanel));
	notificationPanel->SetVisualContent(std::move(__owned_canvas9));
	canvas9->AddOwned(std::move(__owned_textBlock31));

	canvas9->AddOwned(std::move(__owned_toastMessage));

	canvas9->AddOwned(std::move(__owned_progressBar2));

	canvas9->AddOwned(std::move(__owned_dismissToast));

	canvas9->AddOwned(std::move(__owned_textBlock32));






	mainTabs->AddItemControl(std::move(__owned_tabItem7));
	tabItem7->SetVisualContent(std::move(__owned_border18));
	border18->SetChild(std::move(__owned_webSurface));
	webSurface->AddOwned(std::move(__owned_invokeWeb));

	webSurface->AddOwned(std::move(__owned_webHint));

	webSurface->AddOwned(std::move(__owned_border19));
	border19->SetChild(std::move(__owned_webBrowser));





	mainTabs->AddItemControl(std::move(__owned_tabItem8));
	tabItem8->SetVisualContent(std::move(__owned_border20));
	border20->SetChild(std::move(__owned_mediaSurface));
	mediaSurface->AddOwned(std::move(__owned_mediaPlayer));

	mediaSurface->AddOwned(std::move(__owned_mediaOpen));

	mediaSurface->AddOwned(std::move(__owned_mediaPlay));

	mediaSurface->AddOwned(std::move(__owned_mediaPause));

	mediaSurface->AddOwned(std::move(__owned_mediaStop));

	mediaSurface->AddOwned(std::move(__owned_volumeLabel));

	mediaSurface->AddOwned(std::move(__owned_mediaVolume));

	mediaSurface->AddOwned(std::move(__owned_speedTitle));

	mediaSurface->AddOwned(std::move(__owned_mediaSpeed));

	mediaSurface->AddOwned(std::move(__owned_mediaSpeedText));

	mediaSurface->AddOwned(std::move(__owned_mediaLoop));

	mediaSurface->AddOwned(std::move(__owned_mediaProgress));

	mediaSurface->AddOwned(std::move(__owned_mediaTime));




	mainTabs->AddItemControl(std::move(__owned_tabItem9));
	tabItem9->SetVisualContent(std::move(__owned_border21));
	border21->SetChild(std::move(__owned_wpfLabSurface));
	wpfLabSurface->AddOwned(std::move(__owned_wpfLabTitle));

	wpfLabSurface->AddOwned(std::move(__owned_wpfBindingScope));
	wpfBindingScope->SetVisualContent(std::move(__owned_stackPanel3));
	stackPanel3->AddOwned(std::move(__owned_wpfTypographyOverride));

	stackPanel3->AddOwned(std::move(__owned_wpfTwoWayEditor));

	stackPanel3->AddOwned(std::move(__owned_wpfElementMirror));

	stackPanel3->AddOwned(std::move(__owned_wpfSelfValue));

	stackPanel3->AddOwned(std::move(__owned_wpfAncestorValue));

	stackPanel3->AddOwned(std::move(__owned_wpfFallbackValue));

	stackPanel3->AddOwned(std::move(__owned_wpfNullValue));

	stackPanel3->AddOwned(std::move(__owned_wpfIndexerValue));

	stackPanel3->AddOwned(std::move(__owned_wpfKeyedIndexerValue));

	stackPanel3->AddOwned(std::move(__owned_wpfConvertedValue));

	stackPanel3->AddOwned(std::move(__owned_wpfMultiValue));



	wpfLabSurface->AddOwned(std::move(__owned_wpfTemplateAndStyleScope));
	wpfTemplateAndStyleScope->AddOwned(std::move(__owned_textBlock33));

	wpfTemplateAndStyleScope->AddOwned(std::move(__owned_wpfTemplateButton));

	wpfTemplateAndStyleScope->AddOwned(std::move(__owned_wpfTriggerButton));

	wpfTemplateAndStyleScope->AddOwned(std::move(__owned_wpfScopeResourceValue));

	wpfTemplateAndStyleScope->AddOwned(std::move(__owned_wpfInnerResourceScope));
	wpfInnerResourceScope->AddOwned(std::move(__owned_wpfInnerResourceValue));


	wpfTemplateAndStyleScope->AddOwned(std::move(__owned_textBlock34));


	wpfLabSurface->AddOwned(std::move(__owned_wpfItemsScope));
	wpfItemsScope->AddOwned(std::move(__owned_textBlock35));

	wpfItemsScope->AddOwned(std::move(__owned_wpfTemplateList));

	wpfItemsScope->AddOwned(std::move(__owned_wpfRouteOuter));
	wpfRouteOuter->SetChild(std::move(__owned_canvas10));
	canvas10->AddOwned(std::move(__owned_textBlock36));

	canvas10->AddOwned(std::move(__owned_wpfRouteMiddle));
	wpfRouteMiddle->AddOwned(std::move(__owned_wpfRouteSource));

	wpfRouteMiddle->AddOwned(std::move(__owned_wpfFocusPeerB));

	wpfRouteMiddle->AddOwned(std::move(__owned_wpfFocusPeerC));

	wpfRouteMiddle->AddOwned(std::move(__owned_wpfNoFocusPeer));


	canvas10->AddOwned(std::move(__owned_wpfTextInputSource));

	canvas10->AddOwned(std::move(__owned_wpfRouteTrace));

	canvas10->AddOwned(std::move(__owned_wpfInputStats));




	wpfLabSurface->AddOwned(std::move(__owned_wpfHierarchyScope));
	wpfHierarchyScope->AddOwned(std::move(__owned_wpfHierarchyChain));

	wpfHierarchyScope->AddOwned(std::move(__owned_wpfDispatcherProbe));

	wpfHierarchyScope->AddOwned(std::move(__owned_wpfDispatcherResult));





	mainTabs->AddItemControl(std::move(__owned_tabItem10));
	tabItem10->SetVisualContent(std::move(__owned_border22));
	border22->SetChild(std::move(__owned_textCompositionLabSurface));
	textCompositionLabSurface->AddOwned(std::move(__owned_textBlock37));

	textCompositionLabSurface->AddOwned(std::move(__owned_border23));
	border23->SetChild(std::move(__owned_canvas11));
	canvas11->AddOwned(std::move(__owned_textBlock38));

	canvas11->AddOwned(std::move(__owned_textBlock39));

	canvas11->AddOwned(std::move(__owned_compositionTextBox));

	canvas11->AddOwned(std::move(__owned_textBlock40));

	canvas11->AddOwned(std::move(__owned_compositionRichTextBox));

	canvas11->AddOwned(std::move(__owned_textBlock41));

	canvas11->AddOwned(std::move(__owned_compositionPasswordBox));

	canvas11->AddOwned(std::move(__owned_textBlock42));



	textCompositionLabSurface->AddOwned(std::move(__owned_border24));
	border24->SetChild(std::move(__owned_canvas12));
	canvas12->AddOwned(std::move(__owned_textBlock43));

	canvas12->AddOwned(std::move(__owned_compositionStartProbe));

	canvas12->AddOwned(std::move(__owned_compositionUpdateProbe));

	canvas12->AddOwned(std::move(__owned_compositionCommitProbe));

	canvas12->AddOwned(std::move(__owned_compositionCancelProbe));

	canvas12->AddOwned(std::move(__owned_compositionSurrogateProbe));

	canvas12->AddOwned(std::move(__owned_compositionUnicharProbe));

	canvas12->AddOwned(std::move(__owned_compositionFocusProbe));

	canvas12->AddOwned(std::move(__owned_compositionPreviewHandledProbe));

	canvas12->AddOwned(std::move(__owned_compositionResetProbe));

	canvas12->AddOwned(std::move(__owned_compositionState));

	canvas12->AddOwned(std::move(__owned_compositionStats));



	textCompositionLabSurface->AddOwned(std::move(__owned_border25));
	border25->SetChild(std::move(__owned_canvas13));
	canvas13->AddOwned(std::move(__owned_textBlock44));

	canvas13->AddOwned(std::move(__owned_compositionTrace));






	mainTabs->AddItemControl(std::move(__owned_tabItem11));
	tabItem11->SetVisualContent(std::move(__owned_border26));
	border26->SetChild(std::move(__owned_presentationLabSurface));
	presentationLabSurface->AddOwned(std::move(__owned_textBlock45));

	presentationLabSurface->AddOwned(std::move(__owned_presentationProbeSurface));

	presentationLabSurface->AddOwned(std::move(__owned_presentationTopologyTile));

	presentationLabSurface->AddOwned(std::move(__owned_canvas14));
	canvas14->AddOwned(std::move(__owned_textBlock46));

	canvas14->AddOwned(std::move(__owned_textBlock47));

	canvas14->AddOwned(std::move(__owned_textBlock48));

	canvas14->AddOwned(std::move(__owned_textBlock49));

	canvas14->AddOwned(std::move(__owned_textBlock50));

	canvas14->AddOwned(std::move(__owned_textBlock51));


	presentationLabSurface->AddOwned(std::move(__owned_presentationRegionButton));

	presentationLabSurface->AddOwned(std::move(__owned_presentationGeometryButton));

	presentationLabSurface->AddOwned(std::move(__owned_presentationCompositionButton));

	presentationLabSurface->AddOwned(std::move(__owned_presentationFullButton));

	presentationLabSurface->AddOwned(std::move(__owned_presentationTopologyButton));

	presentationLabSurface->AddOwned(std::move(__owned_presentationDeviceLossButton));

	presentationLabSurface->AddOwned(std::move(__owned_presentationStatus));

	presentationLabSurface->AddOwned(std::move(__owned_textBlock52));





	windowContent->AddOwned(std::move(__owned_systemContextMenu));
	systemContextMenu->AddItemControl(std::move(__owned_menuItem6));

	systemContextMenu->AddItemControl(std::move(__owned_menuItem7));

	systemContextMenu->AddItemControl(std::move(__owned_separator2));

	systemContextMenu->AddItemControl(std::move(__owned_menuItem8));
	menuItem8->AddItemControl(std::move(__owned_menuItem9));

	menuItem8->AddItemControl(std::move(__owned_menuItem10));



	windowContent->AddOwned(std::move(__owned_mainStatusBar));


	{
		auto* __relativePanel = dynamic_cast<RelativePanel*>(demoRelative);
		if (!relativeCenter || !__relativePanel || relativeCenter->GetVisualParent() != __relativePanel)
			throw std::runtime_error(Convert::WStringToString(L"控件 relativeCenter 的 RelativePanel 约束只能应用于 RelativePanel 的直接子控件。"));
		RelativeConstraints __relativeConstraints{};
		__relativeConstraints.CenterHorizontal = true;
		__relativeConstraints.CenterVertical = true;
		__relativePanel->SetConstraints(relativeCenter, __relativeConstraints);
	}

	auto __frameworkThemeStyles = CuiGeneratedFrameworkTheme::DefaultStyleSheet(&__frameworkThemeError);
	if (!__frameworkThemeStyles)
		throw std::runtime_error("Generated Generic.xaml theme construction failed");
	// AOT Style 程序：生成期完成分组、索引和连续池布局
	static constexpr CompiledBindingPathStep __styleSheet_program_data_path_1[] = {
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Observe, BindingValueKind::Empty, BindingSourcePropertyToken{ 13692166754359878880ULL }, 0u }
	};
	static constexpr CompiledBindingPathStep __styleSheet_program_data_path_2[] = {
		{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Observe, BindingValueKind::Empty, BindingSourcePropertyToken{ 15197647781564744993ULL }, 0u }
	};
	static constexpr CompiledBindingPathView __styleSheet_program_data_paths[] = {
		CompiledBindingPathView{ __styleSheet_program_data_path_1 },
		CompiledBindingPathView{ __styleSheet_program_data_path_2 }
	};
	static constexpr D2D1_COLOR_F __styleSheet_program_values_colors[] = {
		D2D1_COLOR_F{1.f, 1.f, 1.f, 1.f},
		D2D1_COLOR_F{0.090196f, 0.12549f, 0.2f, 1.f},
		D2D1_COLOR_F{0.184314f, 0.435294f, 0.894118f, 1.f}
	};
	static constexpr Thickness __styleSheet_program_values_thicknesses[] = {
		Thickness(4.f, 2.f, 4.f, 2.f),
		Thickness(6.f, 2.f, 6.f, 2.f),
		Thickness(6.f, 3.f, 6.f, 3.f),
		Thickness(1.f, 1.f, 1.f, 1.f),
		Thickness(4.f, 4.f, 4.f, 4.f),
		Thickness(1.f, 1.f, 1.f, 1.f),
		Thickness(10.f, 10.f, 10.f, 10.f),
		Thickness(1.f, 1.f, 1.f, 1.f),
		Thickness(8.f, 8.f, 8.f, 8.f)
	};
	static constexpr bool __styleSheet_program_values_bools[] = {
		true,
		true,
		true,
		true,
		false,
		true,
		true,
		true,
		true
	};
	static constexpr double __styleSheet_program_values_doubles[] = {
		13.0,
		14.0
	};
	static constexpr std::wstring_view __styleSheet_program_values_string_values[] = {
		L"Ready",
		L"Ready",
		L"true",
		L"Trigger ready"
	};
	static constexpr std::wstring_view __styleSheet_program_strings[] = {
		L"RuntimeBadgeForeground",
		L"Accent",
		L"Surface",
		L"SurfaceSoft",
		L"Border",
		L"ContainerSurface",
		L"ContainerBorder",
		L"TextPrimary",
		L"TextMuted",
		L"OnAccent",
		L"Success",
		L"DemoImage",
		L"HeaderForeground",
		L"GradientLabelClip",
		L"GradientLabelTransform",
		L"WpfLabAccent",
		L"DemoListViewItemTemplate",
		L"WpfLabButtonTemplate",
		L"WpfLabButtonTemplateAlternate",
		L"WpfLabListTemplate",
		L"WpfLabListItemTemplate",
		L"BasicChoices",
		L"DemoStatusEntries",
		L"DemoTasks",
		L"AnalyticsRows",
		L"ActiveDemoTasks",
		L"MainToolBarItemsPanel",
		L"DemoTaskItemsPanel",
		L"WpfLabItemsPanel",
		L"DemoTaskRow",
		L"AnalyticsRowTemplate",
		L"DemoListViewRow",
		L"DemoTaskGroupHeader",
		L"WpfLabPersonRow",
		L"DemoTaskGroups",
		L"ImageText",
		L"DemoListViewContainer",
		L"DemoTaskContainer",
		L"WpfLabContainerStyle",
		L"WpfLabBaseButton",
		L"SurfacePanel",
		L"ContainerFrame",
		L"PrimaryButton",
		L"MutedLabel",
		L"GradientHeader",
		L"ResourceImage",
		L"ResourceGeometryVisual"
	};
	static constexpr CompiledStyleValuePoolView __styleSheet_program_value_pools[] = {
		MakeCompiledStyleValuePoolView(__styleSheet_program_values_colors),
		MakeCompiledStyleValuePoolView(__styleSheet_program_values_thicknesses),
		MakeCompiledStyleValuePoolView(__styleSheet_program_values_bools),
		MakeCompiledStyleValuePoolView(__styleSheet_program_values_doubles),
		MakeCompiledStyleValuePoolView(__styleSheet_program_values_string_values)
	};
	static const CompiledStyleResourceOp __styleSheet_program_resources[] = {
		{ 0u, 0u },
		{ 1u, 1u },
		{ 2u, MakeCompiledStyleStaticValueReference(0u, 0u) },
		{ 3u, 2u },
		{ 4u, 3u },
		{ 5u, 4u },
		{ 6u, 5u },
		{ 7u, MakeCompiledStyleStaticValueReference(0u, 1u) },
		{ 8u, 6u },
		{ 9u, 7u },
		{ 10u, 8u },
		{ 11u, 9u },
		{ 12u, 10u },
		{ 13u, 11u },
		{ 14u, 12u },
		{ 15u, MakeCompiledStyleStaticValueReference(0u, 2u) },
		{ 16u, 13u },
		{ 17u, 14u },
		{ 18u, 15u },
		{ 19u, 16u },
		{ 20u, 17u },
		{ 21u, 18u },
		{ 22u, 19u },
		{ 23u, 20u },
		{ 24u, 21u },
		{ 25u, 22u },
		{ 26u, 23u },
		{ 27u, 24u },
		{ 28u, 25u },
		{ 29u, 26u },
		{ 30u, 27u },
		{ 31u, 28u },
		{ 32u, 29u },
		{ 33u, 30u },
		{ 34u, 31u }
	};
	static constexpr uint32_t __styleSheet_program_resource_lookup[] = {
		1u,
		25u,
		30u,
		24u,
		21u,
		4u,
		6u,
		5u,
		11u,
		16u,
		31u,
		22u,
		32u,
		34u,
		27u,
		29u,
		23u,
		13u,
		14u,
		12u,
		26u,
		9u,
		0u,
		10u,
		2u,
		3u,
		8u,
		7u,
		15u,
		17u,
		18u,
		28u,
		20u,
		19u,
		33u
	};
	static const CompiledStylePropertyConditionOp __styleSheet_program_property_conditions[] = {
		{ DependencyPropertyReference(Control::IsMouseOverProperty()), MakeCompiledStyleStaticValueReference(2u, 0u) },
		{ DependencyPropertyReference(ListViewItem::IsSelectedProperty()), MakeCompiledStyleStaticValueReference(2u, 1u) },
		{ DependencyPropertyReference(Control::IsMouseOverProperty()), MakeCompiledStyleStaticValueReference(2u, 2u) },
		{ DependencyPropertyReference(ListBoxItem::IsSelectedProperty()), MakeCompiledStyleStaticValueReference(2u, 3u) },
		{ DependencyPropertyReference(Button::ContentProperty()), MakeCompiledStyleStaticValueReference(4u, 3u) },
		{ DependencyPropertyReference(Button::IsDefaultProperty()), MakeCompiledStyleStaticValueReference(2u, 6u) }
	};
	static constexpr CompiledStyleDataConditionOp __styleSheet_program_data_conditions[] = {
		{ 0u, MakeCompiledStyleStaticValueReference(4u, 0u) },
		{ 0u, MakeCompiledStyleStaticValueReference(4u, 1u) },
		{ 1u, MakeCompiledStyleStaticValueReference(4u, 2u) }
	};
	static const CompiledStyleSetterOp __styleSheet_program_setters[] = {
		{ DependencyPropertyReference(Control::ForegroundProperty()), { CompiledStyleOperandKind::StaticResource, 0u } },
		{ DependencyPropertyReference(Control::PaddingProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(1u, 0u) } },
		{ DependencyPropertyReference(Control::TemplateProperty()), { CompiledStyleOperandKind::StaticResource, 16u } },
		{ DependencyPropertyReference(Control::BackgroundProperty()), { CompiledStyleOperandKind::StaticResource, 3u } },
		{ DependencyPropertyReference(Control::BackgroundProperty()), { CompiledStyleOperandKind::StaticResource, 1u } },
		{ DependencyPropertyReference(Control::PaddingProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(1u, 1u) } },
		{ DependencyPropertyReference(Control::BackgroundProperty()), { CompiledStyleOperandKind::StaticResource, 3u } },
		{ DependencyPropertyReference(Control::BackgroundProperty()), { CompiledStyleOperandKind::StaticResource, 1u } },
		{ DependencyPropertyReference(Control::PaddingProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(1u, 2u) } },
		{ DependencyPropertyReference(Control::TemplateProperty()), { CompiledStyleOperandKind::StaticResource, 20u } },
		{ DependencyPropertyReference(Control::FontSizeProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(3u, 0u) } },
		{ DependencyPropertyReference(Button::IsDefaultProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(2u, 4u) } },
		{ DependencyPropertyReference(Control::BorderThicknessProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(1u, 3u) } },
		{ DependencyPropertyReference(Control::FontSizeProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(3u, 1u) } },
		{ DependencyPropertyReference(Button::IsDefaultProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(2u, 5u) } },
		{ DependencyPropertyReference(Control::BorderThicknessProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(1u, 4u) } },
		{ DependencyPropertyReference(Control::BackgroundProperty()), { CompiledStyleOperandKind::StaticResource, 2u } },
		{ DependencyPropertyReference(Control::BorderBrushProperty()), { CompiledStyleOperandKind::StaticResource, 4u } },
		{ DependencyPropertyReference(Control::BorderThicknessProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(1u, 5u) } },
		{ DependencyPropertyReference(Control::ClipToBoundsProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(2u, 7u) } },
		{ DependencyPropertyReference(Border::PaddingProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(1u, 6u) } },
		{ DependencyPropertyReference(Control::BackgroundProperty()), { CompiledStyleOperandKind::StaticResource, 5u } },
		{ DependencyPropertyReference(Control::BorderBrushProperty()), { CompiledStyleOperandKind::StaticResource, 6u } },
		{ DependencyPropertyReference(Control::BorderThicknessProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(1u, 7u) } },
		{ DependencyPropertyReference(Control::ClipToBoundsProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(2u, 8u) } },
		{ DependencyPropertyReference(Border::PaddingProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(1u, 8u) } },
		{ DependencyPropertyReference(Control::BackgroundProperty()), { CompiledStyleOperandKind::StaticResource, 1u } },
		{ DependencyPropertyReference(Control::ForegroundProperty()), { CompiledStyleOperandKind::StaticResource, 9u } },
		{ DependencyPropertyReference(Control::ForegroundProperty()), { CompiledStyleOperandKind::StaticResource, 8u } },
		{ DependencyPropertyReference(Control::ForegroundProperty()), { CompiledStyleOperandKind::StaticResource, 12u } },
		{ DependencyPropertyReference(Image::SourceProperty()), { CompiledStyleOperandKind::StaticResource, 11u } },
		{ DependencyPropertyReference(Control::ClipProperty()), { CompiledStyleOperandKind::StaticResource, 13u } },
		{ DependencyPropertyReference(Control::RenderTransformProperty()), { CompiledStyleOperandKind::StaticResource, 14u } }
	};
	static const CompiledInteractionPropertyOperand __styleSheet_program_property_operands[] = {
		{ 0u, DependencyPropertyReference(Control::FontSizeProperty()) }
	};
	static constexpr CompiledInteractionAnimationOp __styleSheet_program_animations[] = {
		{ DeclarativeAnimationKind::Double, 0u, CompiledInteractionInvalidIndex, 32u, 33u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, 0ULL, 400ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut }
	};
	static constexpr CompiledInteractionStoryboardOp __styleSheet_program_storyboards[] = {
		{ { 0u, 1u } }
	};
	static constexpr CompiledInteractionActionOp __styleSheet_program_actions[] = {
		{ DeclarativeStoryboardActionKind::Begin, 0u },
		{ DeclarativeStoryboardActionKind::Stop, 0u }
	};
	static constexpr CompiledStyleRuleOp __styleSheet_program_rules[] = {
		{ 1u, 0u, { 0u, 0u }, { 0u, 0u }, { 0u, 1u }, { 0u, 0u }, { 0u, 0u } },
		{ 2u, 1u, { 0u, 0u }, { 0u, 0u }, { 1u, 2u }, { 0u, 0u }, { 0u, 0u } },
		{ 3u, 2u, { 0u, 1u }, { 0u, 0u }, { 3u, 1u }, { 0u, 0u }, { 0u, 0u } },
		{ 4u, 3u, { 1u, 1u }, { 0u, 0u }, { 4u, 1u }, { 0u, 0u }, { 0u, 0u } },
		{ 5u, 4u, { 2u, 0u }, { 0u, 0u }, { 5u, 1u }, { 0u, 0u }, { 0u, 0u } },
		{ 6u, 5u, { 2u, 1u }, { 0u, 0u }, { 6u, 1u }, { 0u, 0u }, { 0u, 0u } },
		{ 7u, 6u, { 3u, 1u }, { 0u, 0u }, { 7u, 1u }, { 0u, 0u }, { 0u, 0u } },
		{ 8u, 7u, { 4u, 0u }, { 0u, 0u }, { 8u, 2u }, { 0u, 0u }, { 0u, 0u } },
		{ 9u, 8u, { 4u, 0u }, { 0u, 0u }, { 10u, 3u }, { 0u, 0u }, { 0u, 0u } },
		{ 10u, 9u, { 4u, 0u }, { 0u, 1u }, { 13u, 1u }, { 0u, 1u }, { 1u, 1u } },
		{ 11u, 10u, { 4u, 0u }, { 1u, 2u }, { 14u, 1u }, { 2u, 0u }, { 2u, 0u } },
		{ 12u, 11u, { 4u, 2u }, { 3u, 0u }, { 15u, 1u }, { 2u, 0u }, { 2u, 0u } },
		{ 13u, 12u, { 6u, 0u }, { 3u, 0u }, { 16u, 5u }, { 2u, 0u }, { 2u, 0u } },
		{ 14u, 13u, { 6u, 0u }, { 3u, 0u }, { 21u, 5u }, { 2u, 0u }, { 2u, 0u } },
		{ 15u, 14u, { 6u, 0u }, { 3u, 0u }, { 26u, 2u }, { 2u, 0u }, { 2u, 0u } },
		{ 16u, 15u, { 6u, 0u }, { 3u, 0u }, { 28u, 1u }, { 2u, 0u }, { 2u, 0u } },
		{ 17u, 16u, { 6u, 0u }, { 3u, 0u }, { 29u, 1u }, { 2u, 0u }, { 2u, 0u } },
		{ 18u, 17u, { 6u, 0u }, { 3u, 0u }, { 30u, 1u }, { 2u, 0u }, { 2u, 0u } },
		{ 19u, 18u, { 6u, 0u }, { 3u, 0u }, { 31u, 2u }, { 2u, 0u }, { 2u, 0u } }
	};
	static constexpr uint32_t __styleSheet_program_rule_indexes[] = {
		0u,
		1u,
		2u,
		3u,
		4u,
		5u,
		6u,
		7u,
		8u,
		9u,
		10u,
		11u,
		12u,
		13u,
		14u,
		15u,
		16u,
		17u,
		18u
	};
	static const DependencyPropertyReference __styleSheet_program_property_watchers[] = {
		DependencyPropertyReference(Control::IsMouseOverProperty()),
		DependencyPropertyReference(ListViewItem::IsSelectedProperty()),
		DependencyPropertyReference(Control::IsMouseOverProperty()),
		DependencyPropertyReference(ListBoxItem::IsSelectedProperty()),
		DependencyPropertyReference(Button::ContentProperty()),
		DependencyPropertyReference(Button::IsDefaultProperty())
	};
	static constexpr uint32_t __styleSheet_program_data_path_watchers[] = {
		0u,
		1u
	};
	static constexpr CompiledStyleGroupOp __styleSheet_program_groups[] = {
		{ true, static_cast<UIClass>(3), ComponentTypeToken{ 0ULL }, 35u, { 0u, 1u }, { 0u, 0u }, { 0u, 0u } },
		{ true, static_cast<UIClass>(56), ComponentTypeToken{ 0ULL }, 36u, { 1u, 3u }, { 0u, 2u }, { 0u, 0u } },
		{ true, static_cast<UIClass>(55), ComponentTypeToken{ 0ULL }, 37u, { 4u, 3u }, { 2u, 2u }, { 0u, 0u } },
		{ true, static_cast<UIClass>(55), ComponentTypeToken{ 0ULL }, 38u, { 7u, 1u }, { 4u, 0u }, { 0u, 0u } },
		{ true, static_cast<UIClass>(7), ComponentTypeToken{ 0ULL }, 39u, { 8u, 4u }, { 4u, 2u }, { 0u, 2u } },
		{ true, static_cast<UIClass>(24), ComponentTypeToken{ 0ULL }, 40u, { 12u, 1u }, { 6u, 0u }, { 2u, 0u } },
		{ true, static_cast<UIClass>(24), ComponentTypeToken{ 0ULL }, 41u, { 13u, 1u }, { 6u, 0u }, { 2u, 0u } },
		{ true, static_cast<UIClass>(7), ComponentTypeToken{ 0ULL }, 42u, { 14u, 1u }, { 6u, 0u }, { 2u, 0u } },
		{ true, static_cast<UIClass>(3), ComponentTypeToken{ 0ULL }, 43u, { 15u, 1u }, { 6u, 0u }, { 2u, 0u } },
		{ true, static_cast<UIClass>(3), ComponentTypeToken{ 0ULL }, 44u, { 16u, 1u }, { 6u, 0u }, { 2u, 0u } },
		{ true, static_cast<UIClass>(8), ComponentTypeToken{ 0ULL }, 45u, { 17u, 1u }, { 6u, 0u }, { 2u, 0u } },
		{ true, static_cast<UIClass>(3), ComponentTypeToken{ 0ULL }, 46u, { 18u, 1u }, { 6u, 0u }, { 2u, 0u } }
	};
	static const DependencyPropertyReference __styleSheet_program_global_property_watchers[] = {
		DependencyPropertyReference(Control::IsMouseOverProperty()),
		DependencyPropertyReference(ListViewItem::IsSelectedProperty()),
		DependencyPropertyReference(ListBoxItem::IsSelectedProperty()),
		DependencyPropertyReference(Button::ContentProperty()),
		DependencyPropertyReference(Button::IsDefaultProperty())
	};
	static constexpr uint32_t __styleSheet_program_global_data_path_watchers[] = {
		0u,
		1u
	};
	auto __styleSheet = ControlStyleSheet::CreateCompiled(
		CompiledStyleProgramView{
			CompiledStyleProgramViewVersion,
			std::span<const std::wstring_view>{ __styleSheet_program_strings }, // Strings
			std::span<const CompiledStyleValuePoolView>{ __styleSheet_program_value_pools }, // ValuePools
			std::span<const CompiledStyleResourceOp>{ __styleSheet_program_resources }, // Resources
			std::span<const uint32_t>{ __styleSheet_program_resource_lookup }, // ResourceLookup
			std::span<const CompiledStylePropertyConditionOp>{ __styleSheet_program_property_conditions }, // PropertyConditions
			std::span<const CompiledStyleDataConditionOp>{ __styleSheet_program_data_conditions }, // DataConditions
			std::span<const CompiledStyleSetterOp>{ __styleSheet_program_setters }, // Setters
			std::span<const CompiledInteractionPropertyOperand>{ __styleSheet_program_property_operands }, // PropertyOperands
			{}, // ObjectPathChildIndices
			{}, // ObjectPaths
			{}, // KeyFrames
			std::span<const CompiledInteractionAnimationOp>{ __styleSheet_program_animations }, // Animations
			std::span<const CompiledInteractionStoryboardOp>{ __styleSheet_program_storyboards }, // Storyboards
			std::span<const CompiledInteractionActionOp>{ __styleSheet_program_actions }, // Actions
			std::span<const CompiledStyleRuleOp>{ __styleSheet_program_rules }, // Rules
			std::span<const uint32_t>{ __styleSheet_program_rule_indexes }, // RuleIndexes
			std::span<const DependencyPropertyReference>{ __styleSheet_program_property_watchers }, // PropertyWatchers
			std::span<const uint32_t>{ __styleSheet_program_data_path_watchers }, // DataPathWatchers
			std::span<const CompiledStyleGroupOp>{ __styleSheet_program_groups }, // Groups
			std::span<const DependencyPropertyReference>{ __styleSheet_program_global_property_watchers }, // GlobalPropertyWatchers
			std::span<const uint32_t>{ __styleSheet_program_global_data_path_watchers }, // GlobalDataPathWatchers
			std::span<const CompiledBindingPathView>{ __styleSheet_program_data_paths }, // DataPaths
		},
		std::vector<BindingValue>{
			BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Image; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 0.9f; value.ImageSource = cui::resources::LoadBitmapResource(L"Assets/nav-overview.svg"); value.Stretch = cui::drawing::ImageBrushStretch::UniformToFill; value.AlignmentX = cui::drawing::ImageBrushAlignmentX::Center; value.AlignmentY = cui::drawing::ImageBrushAlignmentY::Center; return value; }()),
			__documentStaticResource_Accent_2,
			__documentStaticResource_SurfaceSoft_4,
			__documentStaticResource_Border_5,
			__documentStaticResource_ContainerSurface_6,
			__documentStaticResource_ContainerBorder_7,
			__documentStaticResource_TextMuted_9,
			__documentStaticResource_OnAccent_10,
			__documentStaticResource_Success_11,
			__documentStaticResource_DemoImage_12,
			BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::LinearGradient; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.StartPoint = D2D1::Point2F(0.f, 0.f); value.EndPoint = D2D1::Point2F(1.f, 0.f); value.GradientStops.push_back({ 0.f, D2D1_COLOR_F{0.184314f, 0.435294f, 0.894118f, 1.f} }); value.GradientStops.push_back({ 1.f, D2D1_COLOR_F{0.078431f, 0.639216f, 0.498039f, 1.f} }); return value; }()),
			BindingValue([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Path; value.FillRule = cui::drawing::GeometryFillRule::Nonzero; value.Figures.push_back([] { cui::drawing::PathFigure figure; figure.StartPoint = D2D1::Point2F(12.f, 0.f); figure.IsClosed = true; figure.IsFilled = true; { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::Line; segment.Point = D2D1::Point2F(428.f, 0.f); segment.Point1 = D2D1::Point2F(0.f, 0.f); segment.Point2 = D2D1::Point2F(0.f, 0.f); segment.Point3 = D2D1::Point2F(0.f, 0.f); segment.Size = D2D1::SizeF(0.f, 0.f); segment.RotationAngle = 0.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Counterclockwise; figure.Segments.push_back(segment); } { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::Arc; segment.Point = D2D1::Point2F(440.f, 12.f); segment.Point1 = D2D1::Point2F(0.f, 0.f); segment.Point2 = D2D1::Point2F(0.f, 0.f); segment.Point3 = D2D1::Point2F(0.f, 0.f); segment.Size = D2D1::SizeF(12.f, 12.f); segment.RotationAngle = 0.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Clockwise; figure.Segments.push_back(segment); } { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::Line; segment.Point = D2D1::Point2F(440.f, 22.f); segment.Point1 = D2D1::Point2F(0.f, 0.f); segment.Point2 = D2D1::Point2F(0.f, 0.f); segment.Point3 = D2D1::Point2F(0.f, 0.f); segment.Size = D2D1::SizeF(0.f, 0.f); segment.RotationAngle = 0.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Counterclockwise; figure.Segments.push_back(segment); } { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::Arc; segment.Point = D2D1::Point2F(428.f, 34.f); segment.Point1 = D2D1::Point2F(0.f, 0.f); segment.Point2 = D2D1::Point2F(0.f, 0.f); segment.Point3 = D2D1::Point2F(0.f, 0.f); segment.Size = D2D1::SizeF(12.f, 12.f); segment.RotationAngle = 0.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Clockwise; figure.Segments.push_back(segment); } { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::Line; segment.Point = D2D1::Point2F(12.f, 34.f); segment.Point1 = D2D1::Point2F(0.f, 0.f); segment.Point2 = D2D1::Point2F(0.f, 0.f); segment.Point3 = D2D1::Point2F(0.f, 0.f); segment.Size = D2D1::SizeF(0.f, 0.f); segment.RotationAngle = 0.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Counterclockwise; figure.Segments.push_back(segment); } { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::Arc; segment.Point = D2D1::Point2F(0.f, 22.f); segment.Point1 = D2D1::Point2F(0.f, 0.f); segment.Point2 = D2D1::Point2F(0.f, 0.f); segment.Point3 = D2D1::Point2F(0.f, 0.f); segment.Size = D2D1::SizeF(12.f, 12.f); segment.RotationAngle = 0.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Clockwise; figure.Segments.push_back(segment); } { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::Line; segment.Point = D2D1::Point2F(0.f, 12.f); segment.Point1 = D2D1::Point2F(0.f, 0.f); segment.Point2 = D2D1::Point2F(0.f, 0.f); segment.Point3 = D2D1::Point2F(0.f, 0.f); segment.Size = D2D1::SizeF(0.f, 0.f); segment.RotationAngle = 0.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Counterclockwise; figure.Segments.push_back(segment); } { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::Arc; segment.Point = D2D1::Point2F(12.f, 0.f); segment.Point1 = D2D1::Point2F(0.f, 0.f); segment.Point2 = D2D1::Point2F(0.f, 0.f); segment.Point3 = D2D1::Point2F(0.f, 0.f); segment.Size = D2D1::SizeF(12.f, 12.f); segment.RotationAngle = 0.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Clockwise; figure.Segments.push_back(segment); } return figure; }()); value.LocalTransform = [] { cui::drawing::Transform value; { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Scale; operation.ScaleX = 0.99f; operation.ScaleY = 0.94f; operation.CenterX = 220.f; operation.CenterY = 17.f; value.Operations.push_back(operation); } return value; }(); return value; }()),
			BindingValue([] { cui::drawing::Transform value; { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Rotate; operation.Angle = -2.f; operation.CenterX = 0.f; operation.CenterY = 0.f; value.Operations.push_back(operation); } { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Scale; operation.ScaleX = 1.02f; operation.ScaleY = 1.02f; operation.CenterX = 0.f; operation.CenterY = 0.f; value.Operations.push_back(operation); } return value; }()),
			BindingValue(ControlTemplateReference(__controlTemplate_DemoListViewItemTemplate_1)),
			BindingValue(ControlTemplateReference(__controlTemplate_WpfLabButtonTemplate_2)),
			BindingValue(ControlTemplateReference(__controlTemplate_WpfLabButtonTemplateAlternate_3)),
			BindingValue(ControlTemplateReference(__controlTemplate_WpfLabListTemplate_4)),
			BindingValue(ControlTemplateReference(__controlTemplate_WpfLabListItemTemplate_5)),
			BindingValue(BindingListReference(__dataList_BasicChoices_1)),
			BindingValue(BindingListReference(__dataList_DemoStatusEntries_2)),
			BindingValue(BindingListReference(__dataList_DemoTasks_3)),
			BindingValue(BindingListReference(__dataList_AnalyticsRows_4)),
			BindingValue(BindingListReference(__collectionView_ActiveDemoTasks_1)),
			BindingValue(ItemsPanelTemplateReference(__itemsPanel_MainToolBarItemsPanel_1)),
			BindingValue(ItemsPanelTemplateReference(__itemsPanel_DemoTaskItemsPanel_2)),
			BindingValue(ItemsPanelTemplateReference(__itemsPanel_WpfLabItemsPanel_3)),
			BindingValue(ItemTemplateReference(__dataTemplate_DemoTaskRow_1)),
			BindingValue(ItemTemplateReference(__dataTemplate_AnalyticsRowTemplate_2)),
			BindingValue(ItemTemplateReference(__dataTemplate_DemoListViewRow_3)),
			BindingValue(ItemTemplateReference(__dataTemplate_DemoTaskGroupHeader_4)),
			BindingValue(ItemTemplateReference(__dataTemplate_WpfLabPersonRow_7)),
			BindingValue(GroupStyleReference(__groupStyle_DemoTaskGroups_1)),
			BindingValue(14.0),
			BindingValue(18.0)
		}
	);

	if (!cui::framework::StyleAccess::SetEnvironment(*this, std::move(__frameworkThemeStyles), std::move(__styleSheet), true))
		throw std::runtime_error("Generated Theme/Document style environment installation failed");

	if (!cui::framework::TemplateAccess::SetTemplate(*wpfTemplateButton, ControlTemplateReference(__controlTemplate_WpfLabButtonTemplate_2), DependencyPropertyValueSource::Local))
		throw std::runtime_error("Generated authored Control.Template installation failed");
	if (!cui::framework::TemplateAccess::SetTemplate(*wpfTemplateList, ControlTemplateReference(__controlTemplate_WpfLabListTemplate_4), DependencyPropertyValueSource::Local))
		throw std::runtime_error("Generated authored Control.Template installation failed");

	// XAML Window Local 属性/资源表达式
	this->SetBackground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.909804f, 0.933333f, 0.964706f, 1.f}; return value; }());
	this->SetForeground([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.090196f, 0.12549f, 0.2f, 1.f}; return value; }());
	this->SetHeight(cui::layout::Length::Fixed(800.f));
	this->SetTitle(L"CUI XAML Component Gallery");
	this->SetWidth(cui::layout::Length::Fixed(1400.f));

}

DemoWindowGenerated::~DemoWindowGenerated()
{
}

bool DemoWindowGenerated::BindData(BindingSourceReference dataContext)
{
	if (!dataContext) return false;
	auto __windowDataContext = dataContext;
	if (!SetDataContext(std::move(dataContext))) return false;
	bool success = true;
	featureCardContent->DataBindings.Clear();
	{
		bool cuiBindingAttached = false;
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_38[] =
		{
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 9835099465518400830ULL }, 0u },
		};
		cuiBindingAttached = featureCardContent->DataBindings.Add(Label::TextProperty(), featureCardContent->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_38 }, BindingMode::Default, DataSourceUpdateMode::Default, {}, {}, {}, {}, std::optional<std::wstring>(L"Logical DataContext via FeatureCard: {0}")) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	demoTree->DataBindings.Clear();
	{
		bool cuiBindingAttached = false;
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_39[] =
		{
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Observe, BindingValueKind::Object, BindingSourcePropertyToken{ 2602425173560606198ULL }, 0u },
		};
		cuiBindingAttached = demoTree->DataBindings.Add(ItemsControl::ItemsSourceProperty(), demoTree->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_39 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	demoList->DataBindings.Clear();
	{
		bool cuiBindingAttached = false;
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_40[] =
		{
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Observe, BindingValueKind::Object, BindingSourcePropertyToken{ 13985105387377820950ULL }, 0u },
		};
		cuiBindingAttached = demoList->DataBindings.Add(ItemsControl::ItemsSourceProperty(), demoList->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_40 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	wpfTwoWayEditor->DataBindings.Clear();
	{
		bool cuiBindingAttached = false;
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_41[] =
		{
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 9835099465518400830ULL }, 0u },
		};
		cuiBindingAttached = wpfTwoWayEditor->DataBindings.Add(TextBox::TextProperty(), wpfTwoWayEditor->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_41 }, BindingMode::TwoWay, DataSourceUpdateMode::OnPropertyChanged) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	wpfElementMirror->DataBindings.Clear();
	{
		bool cuiBindingAttached = false;
		// CUI:AOT binding-source=direct-dp
		cuiBindingAttached = wpfElementMirror->DataBindings.Add(Label::TextProperty(), cui::binding::MakeCompiledDependencyPropertySource(*wpfTwoWayEditor, TextBox::TextProperty()), BindingMode::OneWay, DataSourceUpdateMode::Default) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	wpfSelfValue->DataBindings.Clear();
	{
		bool cuiBindingAttached = false;
		// CUI:AOT binding-source=direct-dp
		cuiBindingAttached = wpfSelfValue->DataBindings.Add(Control::AutomationNameProperty(), cui::binding::MakeCompiledDependencyPropertySource(*wpfSelfValue, Label::TextProperty()), BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	wpfAncestorValue->DataBindings.Clear();
	{
		bool cuiBindingAttached = false;
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_42[] =
		{
			// CUI:AOT binding-path-endpoint=exact-dp
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Empty, {}, 0u, +[](IBindingSource& source) noexcept -> CompiledSourceHandle { return cui::binding::ResolveCompiledFindAncestorDependencyPropertySource(source, Control::TagProperty()); } },
		};
		cuiBindingAttached = wpfAncestorValue->DataBindings.Add(Control::AutomationNameProperty(), cui::binding::CreateFindAncestorSource(*wpfAncestorValue, static_cast<UIClass>(41), 1), CompiledBindingPathView{ __cuiCompiledBindingPath_42 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	wpfFallbackValue->DataBindings.Clear();
	{
		bool cuiBindingAttached = false;
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_43[] =
		{
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 9378076335107128704ULL }, 0u },
		};
		cuiBindingAttached = wpfFallbackValue->DataBindings.Add(Label::TextProperty(), wpfFallbackValue->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_43 }, BindingMode::Default, DataSourceUpdateMode::Default, {}, BindingValue(L"Fallback: unavailable"), {}, {}, {}) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	wpfNullValue->DataBindings.Clear();
	{
		bool cuiBindingAttached = false;
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_44[] =
		{
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 16822651602587833845ULL }, 0u },
		};
		cuiBindingAttached = wpfNullValue->DataBindings.Add(Label::TextProperty(), wpfNullValue->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_44 }, BindingMode::Default, DataSourceUpdateMode::Default, {}, BindingValue(L"broken"), BindingValue(L"TargetNull: (none)"), {}, {}) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	wpfIndexerValue->DataBindings.Clear();
	{
		bool cuiBindingAttached = false;
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_45[] =
		{
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Observe, BindingValueKind::Object, BindingSourcePropertyToken{ 3015263015765239239ULL }, 0u },
			{ CompiledBindingPathStepKind::ListIndex, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Observe, BindingValueKind::Object, {}, 0u },
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 7420362091733594415ULL }, 0u },
		};
		cuiBindingAttached = wpfIndexerValue->DataBindings.Add(Label::TextProperty(), wpfIndexerValue->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_45 }, BindingMode::Default, DataSourceUpdateMode::Default, {}, BindingValue(L"No first person"), {}, {}, {}) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	wpfKeyedIndexerValue->DataBindings.Clear();
	{
		bool cuiBindingAttached = false;
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_46[] =
		{
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Object, BindingSourcePropertyToken{ 13417529444444422561ULL }, 0u },
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Empty, BindingSourcePropertyToken{ 15561441929564281592ULL }, 0u },
		};
		cuiBindingAttached = wpfKeyedIndexerValue->DataBindings.Add(Label::TextProperty(), wpfKeyedIndexerValue->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_46 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	wpfConvertedValue->DataBindings.Clear();
	{
		bool cuiBindingAttached = false;
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_47[] =
		{
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 4327259317505158084ULL }, 0u },
		};
		auto cuiConverter = GetBuiltInBindingValueConverter(BuiltInBindingValueConverter::StringTrim);
		cuiBindingAttached = cuiConverter && wpfConvertedValue->DataBindings.Add(Label::TextProperty(), wpfConvertedValue->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_47 }, BindingMode::Default, DataSourceUpdateMode::Default, cuiConverter, {}, {}, {}, std::optional<std::wstring>(L"trimmed: {0}")) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	wpfMultiValue->DataBindings.Clear();
	{
		std::vector<MultiBindingSource> cuiMultiSources;
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_48[] =
		{
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 9835099465518400830ULL }, 0u },
		};
		MultiBindingSource cuiMultiSource1(static_cast<IBindingSource*>(&wpfMultiValue->DataContextSource()), CompiledBindingPathView{ __cuiCompiledBindingPath_48 }, {}, {}, {}, {}, {});
		cuiMultiSource1.Mode = BindingMode::Default;
		cuiMultiSource1.UpdateMode = DataSourceUpdateMode::Default;
		cuiMultiSources.push_back(std::move(cuiMultiSource1));
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_49[] =
		{
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 2002927140446823006ULL }, 0u },
		};
		MultiBindingSource cuiMultiSource2(static_cast<IBindingSource*>(&wpfMultiValue->DataContextSource()), CompiledBindingPathView{ __cuiCompiledBindingPath_49 }, {}, {}, {}, {}, {});
		cuiMultiSource2.Mode = BindingMode::Default;
		cuiMultiSource2.UpdateMode = DataSourceUpdateMode::Default;
		cuiMultiSources.push_back(std::move(cuiMultiSource2));
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_50[] =
		{
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Object, BindingSourcePropertyToken{ 13417529444444422561ULL }, 0u },
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::Empty, BindingSourcePropertyToken{ 477310961522767365ULL }, 0u },
		};
		MultiBindingSource cuiMultiSource3(static_cast<IBindingSource*>(&wpfMultiValue->DataContextSource()), CompiledBindingPathView{ __cuiCompiledBindingPath_50 }, {}, {}, {}, {}, {});
		cuiMultiSource3.Mode = BindingMode::Default;
		cuiMultiSource3.UpdateMode = DataSourceUpdateMode::Default;
		cuiMultiSources.push_back(std::move(cuiMultiSource3));
		const bool cuiBindingAttached = wpfMultiValue->DataBindings.AddMulti(Label::TextProperty(), std::move(cuiMultiSources), BindingMode::Default, DataSourceUpdateMode::Default, {}, {}, {}, {}, std::optional<std::wstring>(L"{}{0} / {1} / {2}")) != nullptr;
		success = success && cuiBindingAttached;
	}
	wpfTemplateList->DataBindings.Clear();
	{
		bool cuiBindingAttached = false;
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_51[] =
		{
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Observe, BindingValueKind::Object, BindingSourcePropertyToken{ 3015263015765239239ULL }, 0u },
		};
		cuiBindingAttached = wpfTemplateList->DataBindings.Add(ItemsControl::ItemsSourceProperty(), wpfTemplateList->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_51 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	return success;
}
