#include "../include/RelativeSource.h"
#include "../include/TreeInfrastructure.h"

#include <memory>
#include <utility>
#include <vector>

namespace cui::binding
{
namespace
{
	enum class AncestorMatchKind
	{
		NativeType,
		ExactComponentType,
	};

	struct AncestorMatch final
	{
		AncestorMatchKind Kind = AncestorMatchKind::NativeType;
		UIClass NativeType = UIClass::UI_Base;
		ComponentTypeToken ComponentType;

		bool Valid() const noexcept
		{
			return Kind == AncestorMatchKind::NativeType
				|| static_cast<bool>(ComponentType);
		}

		bool Matches(Control& candidate) const noexcept
		{
			if (Kind == AncestorMatchKind::NativeType)
				return IsUIClassAssignableFrom(
					NativeType, candidate.Type());
			return candidate.GetDeclarativeTypeToken() == ComponentType;
		}
	};

	bool HasRoutedParentCycle(Control* first) noexcept
	{
		auto* slow = first;
		auto* fast = first;
		while (fast)
		{
			fast = fast->GetRoutedParent();
			if (!fast) return false;
			fast = fast->GetRoutedParent();
			slow = slow ? slow->GetRoutedParent() : nullptr;
			if (fast && fast == slow) return true;
		}
		return false;
	}

	Control* FindAncestorCore(
		Control& target,
		const AncestorMatch& match,
		int ancestorLevel) noexcept
	{
		if (ancestorLevel < 1 || !match.Valid()) return nullptr;
		auto* first = target.GetRoutedParent();
		if (HasRoutedParentCycle(first)) return nullptr;
		int remaining = ancestorLevel;
		for (auto* candidate = first; candidate;
			candidate = candidate->GetRoutedParent())
		{
			if (!match.Matches(*candidate)) continue;
			if (--remaining == 0) return candidate;
		}
		return nullptr;
	}

	class FindAncestorBindingSource final : public IBindingSource
	{
	public:
		FindAncestorBindingSource(
			Control& target,
			AncestorMatch match,
			int ancestorLevel)
			: _target(&target),
			_match(std::move(match)),
			_ancestorLevel(ancestorLevel)
		{
			Attach();
		}

#if CUI_ENABLE_DYNAMIC_XAML
		bool TryGetValue(
			const std::wstring& propertyName,
			BindingValue& out) const override
		{
			return _source && _source->TryGetValue(propertyName, out);
		}
#endif

		bool TryGetValue(
			BindingSourcePropertyToken property,
			BindingValue& out) const override
		{
			return _source && _source->TryGetValue(property, out);
		}

#if CUI_ENABLE_DYNAMIC_XAML
		bool TrySetValue(
			const std::wstring& propertyName,
			const BindingValue& value) override
		{
			return _source
				&& _source->TrySetValue(propertyName, value);
		}
#endif

		bool TrySetValue(
			BindingSourcePropertyToken property,
			const BindingValue& value) override
		{
			return _source && _source->TrySetValue(property, value);
		}

#if CUI_ENABLE_DYNAMIC_XAML
		bool TryGetPropertyMetadata(
			const std::wstring& propertyName,
			BindingSourcePropertyMetadata& out) const override
		{
			return _source
				&& _source->TryGetPropertyMetadata(propertyName, out);
		}
#endif

		bool TryGetPropertyMetadata(
			BindingSourcePropertyToken property,
			BindingSourcePropertyMetadata& out) const override
		{
			return _source
				&& _source->TryGetPropertyMetadata(property, out);
		}

#if CUI_ENABLE_DYNAMIC_XAML
		std::vector<BindingSourcePropertyMetadata>
			GetProperties() const override
		{
			return _source ? _source->GetProperties()
				: std::vector<BindingSourcePropertyMetadata>{};
		}

		std::vector<BindingValidationIssue> GetValidationIssues(
			const std::wstring& propertyName) const override
		{
			return _source
				? _source->GetValidationIssues(propertyName)
				: std::vector<BindingValidationIssue>{};
		}
#endif

		std::vector<BindingValidationIssue> GetValidationIssues(
			BindingSourcePropertyToken property) const override
		{
			return _source
				? _source->GetValidationIssues(property)
				: std::vector<BindingValidationIssue>{};
		}

		BindingValidationChangedEvent*
			ValidationChanged() noexcept override
		{
			return &_validationChanged;
		}

		PropertyChangedEvent& PropertyChanged() override
		{
			return _propertyChanged;
		}

		Control* CurrentSource() const noexcept
		{
			return _source;
		}

	private:
		Control* _target = nullptr;
		AncestorMatch _match;
		int _ancestorLevel = 1;
		Control* _source = nullptr;
		std::vector<EventConnection> _parentConnections;
		EventConnection _sourcePropertyConnection;
		EventConnection _sourceValidationConnection;
		PropertyChangedEvent _propertyChanged;
		BindingValidationChangedEvent _validationChanged;
		bool _attaching = false;

		void Attach()
		{
			if (_attaching) return;
			_attaching = true;
			_parentConnections.clear();
			_sourcePropertyConnection.Disconnect();
			_sourceValidationConnection.Disconnect();

			if (_target && !HasRoutedParentCycle(_target))
			{
				for (auto* item = _target; item;
					item = item->GetRoutedParent())
				{
					_parentConnections.push_back(
						cui::framework::TreeAccess::
							SubscribeVisualParentChanged(
								*item,
								[this](Control*, Control*, Control*)
								{
									Attach();
								}));
					_parentConnections.push_back(
						cui::framework::TreeAccess::
							SubscribeLogicalParentChanged(
								*item,
								[this](Control*, Control*, Control*)
								{
									Attach();
								}));
					_parentConnections.push_back(
						cui::framework::TreeAccess::
							SubscribeTemplatedParentChanged(
								*item,
								[this](Control*, Control*, Control*)
								{
									Attach();
								}));
				}
			}

			auto* next = _target
				? FindAncestorCore(
					*_target, _match, _ancestorLevel)
				: nullptr;
			const bool changed = next != _source;
			_source = next;
			if (_source)
			{
				_sourcePropertyConnection =
					_source->PropertyChanged().Subscribe(
						[this](const PropertyChangedEventArgs& e)
						{
							_propertyChanged.Notify(e);
						});
				if (auto* validation = _source->ValidationChanged())
				{
					_sourceValidationConnection =
						validation->Subscribe(
							[this](
								const BindingValidationChangedEventArgs& e)
							{
								_validationChanged.Notify(e);
							});
				}
			}

			_attaching = false;
			if (changed)
			{
#if CUI_ENABLE_DYNAMIC_XAML
				_propertyChanged.Notify(L"");
				_validationChanged.Notify(L"");
#else
				_propertyChanged.Notify(BindingSourcePropertyToken{});
				_validationChanged.Notify(BindingSourcePropertyToken{});
#endif
			}
		}
	};

	FindAncestorBindingSource* CompiledAncestorProvider(
		const CompiledSourceHandle& source) noexcept
	{
		return static_cast<FindAncestorBindingSource*>(source.Object);
	}

	const DependencyProperty* CompiledAncestorProperty(
		const CompiledSourceHandle& source) noexcept
	{
		return static_cast<const DependencyProperty*>(source.Context);
	}

	CompiledBindingPathCapabilities AncestorSourceCapabilities(
		const CompiledSourceHandle& source)
	{
		auto* provider = CompiledAncestorProvider(source);
		const auto* property = CompiledAncestorProperty(source);
		auto* object = provider ? provider->CurrentSource() : nullptr;
		const auto* metadata = object && property
			? object->GetPropertyMetadata(*property) : nullptr;
		if (!metadata) return CompiledBindingPathCapabilities::None;
		auto result = CompiledBindingPathCapabilities::None;
		if (metadata->CanRead())
			result = result | CompiledBindingPathCapabilities::Read;
		if (metadata->CanWrite())
			result = result | CompiledBindingPathCapabilities::Write;
		if (metadata->CanObserve())
			result = result | CompiledBindingPathCapabilities::Observe;
		return result;
	}

	BindingValueKind AncestorSourceValueKind(
		const CompiledSourceHandle& source)
	{
		auto* provider = CompiledAncestorProvider(source);
		const auto* property = CompiledAncestorProperty(source);
		auto* object = provider ? provider->CurrentSource() : nullptr;
		const auto* metadata = object && property
			? object->GetPropertyMetadata(*property) : nullptr;
		return metadata ? metadata->ValueKind() : BindingValueKind::Empty;
	}

	std::weak_ptr<const void> AncestorSourceLifetime(
		const CompiledSourceHandle& source)
	{
		auto* provider = CompiledAncestorProvider(source);
		return provider ? provider->BindingLifetime()
			: std::weak_ptr<const void>{};
	}

	bool ReadAncestorSource(
		const CompiledSourceHandle& source,
		BindingValue& out)
	{
		auto* provider = CompiledAncestorProvider(source);
		const auto* property = CompiledAncestorProperty(source);
		auto* object = provider ? provider->CurrentSource() : nullptr;
		return object && property
			&& object->TryGetPropertyValue(*property, out);
	}

	bool WriteAncestorSource(
		const CompiledSourceHandle& source,
		const BindingValue& value)
	{
		auto* provider = CompiledAncestorProvider(source);
		const auto* property = CompiledAncestorProperty(source);
		auto* object = provider ? provider->CurrentSource() : nullptr;
		return object && property
			&& object->TrySetPropertyValue(*property, value);
	}

	EventConnection SubscribeAncestorSource(
		const CompiledSourceHandle& source,
		DependencyPropertyChangeHandler handler)
	{
		auto* provider = CompiledAncestorProvider(source);
		const auto* property = CompiledAncestorProperty(source);
		if (!provider || !property || !handler) return {};
		const auto expected = property->BindingSourceToken();
		return provider->PropertyChanged().Subscribe(
			[expected, handler = std::move(handler)](
				const PropertyChangedEventArgs& args)
			{
				if (!args.PropertyToken || args.PropertyToken == expected)
					handler();
			});
	}

	const CompiledSourceOps AncestorSourceOps{
		&AncestorSourceCapabilities,
		&AncestorSourceValueKind,
		&AncestorSourceLifetime,
		&ReadAncestorSource,
		&WriteAncestorSource,
		&SubscribeAncestorSource,
		nullptr,
		nullptr
	};

	AncestorMatch NativeMatch(UIClass ancestorType) noexcept
	{
		AncestorMatch result;
		result.Kind = AncestorMatchKind::NativeType;
		result.NativeType = ancestorType;
		return result;
	}

	AncestorMatch ExactMatch(ComponentTypeToken ancestorType) noexcept
	{
		AncestorMatch result;
		result.Kind = AncestorMatchKind::ExactComponentType;
		result.ComponentType = ancestorType;
		return result;
	}
}

Control* FindAncestor(
	Control& target,
	UIClass ancestorType,
	int ancestorLevel) noexcept
{
	return FindAncestorCore(
		target, NativeMatch(ancestorType), ancestorLevel);
}

Control* FindAncestor(
	Control& target,
	ComponentTypeToken ancestorType,
	int ancestorLevel) noexcept
{
	return FindAncestorCore(
		target, ExactMatch(ancestorType), ancestorLevel);
}

#if CUI_ENABLE_DYNAMIC_XAML
Control* FindAncestor(
	Control& target,
	const RuntimeTypeId& ancestorType,
	int ancestorLevel) noexcept
{
	return FindAncestor(target, MakeComponentTypeToken(
		ancestorType.NamespaceUri, ancestorType.LocalName), ancestorLevel);
}
#endif

BindingSourceReference CreateFindAncestorSource(
	Control& target,
	UIClass ancestorType,
	int ancestorLevel)
{
	if (ancestorLevel < 1) return {};
	return BindingSourceReference(
		std::make_shared<FindAncestorBindingSource>(
			target, NativeMatch(ancestorType), ancestorLevel));
}

BindingSourceReference CreateFindAncestorSource(
	Control& target,
	ComponentTypeToken ancestorType,
	int ancestorLevel)
{
	if (ancestorLevel < 1 || !ancestorType) return {};
	return BindingSourceReference(
		std::make_shared<FindAncestorBindingSource>(
			target, ExactMatch(ancestorType), ancestorLevel));
}

CompiledSourceHandle ResolveCompiledFindAncestorDependencyPropertySource(
	IBindingSource& source,
	const DependencyProperty& property) noexcept
{
	auto* provider = dynamic_cast<FindAncestorBindingSource*>(&source);
	return provider
		? CompiledSourceHandle{ provider, &property, &AncestorSourceOps }
		: CompiledSourceHandle{};
}

#if CUI_ENABLE_DYNAMIC_XAML
BindingSourceReference CreateFindAncestorSource(
	Control& target,
	RuntimeTypeId ancestorType,
	int ancestorLevel)
{
	return CreateFindAncestorSource(target, MakeComponentTypeToken(
		ancestorType.NamespaceUri, ancestorType.LocalName), ancestorLevel);
}
#endif
}
