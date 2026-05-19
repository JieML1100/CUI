#include "Binding.h"
#include "Control.h"
#include "ComboBox.h"
#include "ListView.h"
#include "NumericUpDown.h"
#include "ProgressBar.h"
#include "Slider.h"

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <sstream>

namespace
{
	std::wstring Trim(std::wstring value)
	{
		auto isSpace = [](wchar_t ch) { return std::iswspace(ch) != 0; };
		value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](wchar_t ch) { return !isSpace(ch); }));
		value.erase(std::find_if(value.rbegin(), value.rend(), [&](wchar_t ch) { return !isSpace(ch); }).base(), value.end());
		return value;
	}

	std::wstring Lower(std::wstring value)
	{
		for (auto& ch : value)
			ch = (wchar_t)std::towlower(ch);
		return value;
	}

	bool IsSameProperty(const std::wstring& a, const std::wstring& b)
	{
		return Lower(a) == Lower(b);
	}

	bool TryParseBool(const std::wstring& value, bool& out)
	{
		auto text = Lower(Trim(value));
		if (text == L"true" || text == L"1" || text == L"yes" || text == L"on")
		{
			out = true;
			return true;
		}
		if (text == L"false" || text == L"0" || text == L"no" || text == L"off" || text.empty())
		{
			out = false;
			return true;
		}
		return false;
	}

	bool TryParseInt64(const std::wstring& value, long long& out)
	{
		auto text = Trim(value);
		if (text.empty()) return false;
		wchar_t* end = nullptr;
		long long parsed = std::wcstoll(text.c_str(), &end, 10);
		if (!end || *end != L'\0') return false;
		out = parsed;
		return true;
	}

	bool TryParseDouble(const std::wstring& value, double& out)
	{
		auto text = Trim(value);
		if (text.empty()) return false;
		wchar_t* end = nullptr;
		double parsed = std::wcstod(text.c_str(), &end);
		if (!end || *end != L'\0') return false;
		out = parsed;
		return true;
	}

	std::wstring NumberToString(auto value)
	{
		std::wostringstream oss;
		oss << value;
		return oss.str();
	}

	bool BindingValuesEqual(const BindingValue& a, const BindingValue& b)
	{
		if (a.Kind() != b.Kind())
			return false;

		switch (a.Kind())
		{
		case BindingValueKind::Empty:
			return true;
		case BindingValueKind::Bool:
		{
			bool av = false, bv = false;
			return a.TryGetBool(av) && b.TryGetBool(bv) && av == bv;
		}
		case BindingValueKind::Int:
		{
			int av = 0, bv = 0;
			return a.TryGetInt(av) && b.TryGetInt(bv) && av == bv;
		}
		case BindingValueKind::Int64:
		{
			long long av = 0, bv = 0;
			return a.TryGetInt64(av) && b.TryGetInt64(bv) && av == bv;
		}
		case BindingValueKind::Float:
		{
			float av = 0, bv = 0;
			return a.TryGetFloat(av) && b.TryGetFloat(bv) && av == bv;
		}
		case BindingValueKind::Double:
		{
			double av = 0, bv = 0;
			return a.TryGetDouble(av) && b.TryGetDouble(bv) && av == bv;
		}
		case BindingValueKind::String:
		{
			std::wstring av, bv;
			return a.TryGetString(av) && b.TryGetString(bv) && av == bv;
		}
		}
		return false;
	}

	bool IsSourceToTargetMode(BindingMode mode)
	{
		return mode == BindingMode::OneWay || mode == BindingMode::TwoWay || mode == BindingMode::OneTime;
	}

	bool IsTargetToSourceMode(BindingMode mode)
	{
		return mode == BindingMode::TwoWay || mode == BindingMode::OneWayToSource;
	}

	bool ReadTargetValue(Control* target, const std::wstring& propertyName, BindingValue& out)
	{
		if (!target) return false;
		auto prop = Lower(propertyName);

		if (prop == L"text")
		{
			out = target->Text;
			return true;
		}
		if (prop == L"checked")
		{
			out = target->Checked;
			return true;
		}
		if (prop == L"visible")
		{
			out = target->Visible;
			return true;
		}
		if (prop == L"enable" || prop == L"enabled")
		{
			out = target->Enable;
			return true;
		}
		if (prop == L"left")
		{
			out = target->Left;
			return true;
		}
		if (prop == L"top")
		{
			out = target->Top;
			return true;
		}
		if (prop == L"width")
		{
			out = target->Width;
			return true;
		}
		if (prop == L"height")
		{
			out = target->Height;
			return true;
		}
		if (prop == L"selectedindex")
		{
			if (auto* combo = dynamic_cast<ComboBox*>(target))
			{
				out = combo->SelectedIndex;
				return true;
			}
			if (auto* list = dynamic_cast<ListView*>(target))
			{
				out = list->SelectedIndex;
				return true;
			}
			return false;
		}
		if (prop == L"value")
		{
			if (auto* slider = dynamic_cast<Slider*>(target))
			{
				out = slider->Value;
				return true;
			}
			if (auto* number = dynamic_cast<NumericUpDown*>(target))
			{
				out = number->Value;
				return true;
			}
			if (auto* progress = dynamic_cast<ProgressBar*>(target))
			{
				out = progress->Value;
				return true;
			}
			return false;
		}
		if (prop == L"min")
		{
			if (auto* slider = dynamic_cast<Slider*>(target))
			{
				out = slider->Min;
				return true;
			}
			if (auto* number = dynamic_cast<NumericUpDown*>(target))
			{
				out = number->Min;
				return true;
			}
			return false;
		}
		if (prop == L"max")
		{
			if (auto* slider = dynamic_cast<Slider*>(target))
			{
				out = slider->Max;
				return true;
			}
			if (auto* number = dynamic_cast<NumericUpDown*>(target))
			{
				out = number->Max;
				return true;
			}
			return false;
		}
		if (prop == L"maxvalue")
		{
			if (auto* progress = dynamic_cast<ProgressBar*>(target))
			{
				out = progress->MaxValue;
				return true;
			}
			return false;
		}
		if (prop == L"percentagevalue")
		{
			if (auto* progress = dynamic_cast<ProgressBar*>(target))
			{
				out = progress->PercentageValue;
				return true;
			}
			return false;
		}
		return false;
	}

	bool WriteConvertedText(Control* target, const BindingValue& value)
	{
		std::wstring text;
		if (!value.TryGetString(text)) return false;
		target->Text = text;
		return true;
	}

	bool WriteTargetValue(Control* target, const std::wstring& propertyName, const BindingValue& value)
	{
		if (!target) return false;
		auto prop = Lower(propertyName);

		if (prop == L"text")
			return WriteConvertedText(target, value);

		if (prop == L"checked")
		{
			bool checked = false;
			if (!value.TryGetBool(checked)) return false;
			if (target->Checked != checked)
			{
				target->Checked = checked;
				target->InvalidateVisual();
			}
			return true;
		}
		if (prop == L"visible")
		{
			bool visible = false;
			if (!value.TryGetBool(visible)) return false;
			target->Visible = visible;
			return true;
		}
		if (prop == L"enable" || prop == L"enabled")
		{
			bool enabled = false;
			if (!value.TryGetBool(enabled)) return false;
			if (target->Enable != enabled)
			{
				target->Enable = enabled;
				target->InvalidateVisual();
			}
			return true;
		}
		if (prop == L"left")
		{
			int next = 0;
			if (!value.TryGetInt(next)) return false;
			target->Left = next;
			return true;
		}
		if (prop == L"top")
		{
			int next = 0;
			if (!value.TryGetInt(next)) return false;
			target->Top = next;
			return true;
		}
		if (prop == L"width")
		{
			int next = 0;
			if (!value.TryGetInt(next)) return false;
			target->Width = next;
			return true;
		}
		if (prop == L"height")
		{
			int next = 0;
			if (!value.TryGetInt(next)) return false;
			target->Height = next;
			return true;
		}
		if (prop == L"selectedindex")
		{
			int index = -1;
			if (!value.TryGetInt(index)) return false;
			if (auto* combo = dynamic_cast<ComboBox*>(target))
			{
				auto& items = combo->GetItems();
				if (items.empty())
				{
					combo->SelectedIndex = 0;
					combo->Text = L"";
				}
				else
				{
					combo->SelectedIndex = std::clamp(index, 0, (int)items.size() - 1);
					combo->Text = items[(size_t)combo->SelectedIndex];
				}
				combo->InvalidateVisual();
				return true;
			}
			if (auto* list = dynamic_cast<ListView*>(target))
			{
				if (index < 0)
					list->ClearSelection();
				else
					list->SelectItem(index);
				return true;
			}
			return false;
		}
		if (prop == L"value")
		{
			if (auto* slider = dynamic_cast<Slider*>(target))
			{
				float next = 0.0f;
				if (!value.TryGetFloat(next)) return false;
				slider->Value = next;
				return true;
			}
			if (auto* number = dynamic_cast<NumericUpDown*>(target))
			{
				double next = 0.0;
				if (!value.TryGetDouble(next)) return false;
				number->Value = next;
				return true;
			}
			if (auto* progress = dynamic_cast<ProgressBar*>(target))
			{
				float next = 0.0f;
				if (!value.TryGetFloat(next)) return false;
				progress->Value = next;
				return true;
			}
			return false;
		}
		if (prop == L"min")
		{
			if (auto* slider = dynamic_cast<Slider*>(target))
			{
				float next = 0.0f;
				if (!value.TryGetFloat(next)) return false;
				slider->Min = next;
				return true;
			}
			if (auto* number = dynamic_cast<NumericUpDown*>(target))
			{
				double next = 0.0;
				if (!value.TryGetDouble(next)) return false;
				number->Min = next;
				return true;
			}
			return false;
		}
		if (prop == L"max")
		{
			if (auto* slider = dynamic_cast<Slider*>(target))
			{
				float next = 0.0f;
				if (!value.TryGetFloat(next)) return false;
				slider->Max = next;
				return true;
			}
			if (auto* number = dynamic_cast<NumericUpDown*>(target))
			{
				double next = 0.0;
				if (!value.TryGetDouble(next)) return false;
				number->Max = next;
				return true;
			}
			return false;
		}
		if (prop == L"maxvalue")
		{
			if (auto* progress = dynamic_cast<ProgressBar*>(target))
			{
				float next = 0.0f;
				if (!value.TryGetFloat(next)) return false;
				progress->MaxValue = next;
				return true;
			}
			return false;
		}
		if (prop == L"percentagevalue")
		{
			if (auto* progress = dynamic_cast<ProgressBar*>(target))
			{
				float next = 0.0f;
				if (!value.TryGetFloat(next)) return false;
				progress->PercentageValue = next;
				return true;
			}
			return false;
		}
		return false;
	}
}

BindingValue::BindingValue() : _value(std::monostate{}) {}
BindingValue::BindingValue(bool value) : _value(value) {}
BindingValue::BindingValue(int value) : _value(value) {}
BindingValue::BindingValue(long long value) : _value(value) {}
BindingValue::BindingValue(float value) : _value(value) {}
BindingValue::BindingValue(double value) : _value(value) {}
BindingValue::BindingValue(const wchar_t* value) : _value(std::wstring(value ? value : L"")) {}
BindingValue::BindingValue(const std::wstring& value) : _value(value) {}
BindingValue::BindingValue(std::wstring&& value) : _value(std::move(value)) {}

BindingValueKind BindingValue::Kind() const
{
	switch (_value.index())
	{
	case 1: return BindingValueKind::Bool;
	case 2: return BindingValueKind::Int;
	case 3: return BindingValueKind::Int64;
	case 4: return BindingValueKind::Float;
	case 5: return BindingValueKind::Double;
	case 6: return BindingValueKind::String;
	default: return BindingValueKind::Empty;
	}
}

bool BindingValue::Empty() const
{
	return Kind() == BindingValueKind::Empty;
}

std::wstring BindingValue::ToString() const
{
	std::wstring result;
	if (TryGetString(result))
		return result;
	return L"";
}

bool BindingValue::TryGetBool(bool& out) const
{
	switch (Kind())
	{
	case BindingValueKind::Bool:
		out = std::get<bool>(_value);
		return true;
	case BindingValueKind::Int:
		out = std::get<int>(_value) != 0;
		return true;
	case BindingValueKind::Int64:
		out = std::get<long long>(_value) != 0;
		return true;
	case BindingValueKind::Float:
		out = std::get<float>(_value) != 0.0f;
		return true;
	case BindingValueKind::Double:
		out = std::get<double>(_value) != 0.0;
		return true;
	case BindingValueKind::String:
		return TryParseBool(std::get<std::wstring>(_value), out);
	default:
		return false;
	}
}

bool BindingValue::TryGetInt(int& out) const
{
	long long value = 0;
	if (!TryGetInt64(value)) return false;
	out = (int)value;
	return true;
}

bool BindingValue::TryGetInt64(long long& out) const
{
	switch (Kind())
	{
	case BindingValueKind::Bool:
		out = std::get<bool>(_value) ? 1 : 0;
		return true;
	case BindingValueKind::Int:
		out = std::get<int>(_value);
		return true;
	case BindingValueKind::Int64:
		out = std::get<long long>(_value);
		return true;
	case BindingValueKind::Float:
		out = (long long)std::get<float>(_value);
		return true;
	case BindingValueKind::Double:
		out = (long long)std::get<double>(_value);
		return true;
	case BindingValueKind::String:
		return TryParseInt64(std::get<std::wstring>(_value), out);
	default:
		return false;
	}
}

bool BindingValue::TryGetFloat(float& out) const
{
	double value = 0.0;
	if (!TryGetDouble(value)) return false;
	out = (float)value;
	return true;
}

bool BindingValue::TryGetDouble(double& out) const
{
	switch (Kind())
	{
	case BindingValueKind::Bool:
		out = std::get<bool>(_value) ? 1.0 : 0.0;
		return true;
	case BindingValueKind::Int:
		out = (double)std::get<int>(_value);
		return true;
	case BindingValueKind::Int64:
		out = (double)std::get<long long>(_value);
		return true;
	case BindingValueKind::Float:
		out = (double)std::get<float>(_value);
		return true;
	case BindingValueKind::Double:
		out = std::get<double>(_value);
		return true;
	case BindingValueKind::String:
		return TryParseDouble(std::get<std::wstring>(_value), out);
	default:
		return false;
	}
}

bool BindingValue::TryGetString(std::wstring& out) const
{
	switch (Kind())
	{
	case BindingValueKind::Empty:
		out = L"";
		return true;
	case BindingValueKind::Bool:
		out = std::get<bool>(_value) ? L"True" : L"False";
		return true;
	case BindingValueKind::Int:
		out = NumberToString(std::get<int>(_value));
		return true;
	case BindingValueKind::Int64:
		out = NumberToString(std::get<long long>(_value));
		return true;
	case BindingValueKind::Float:
		out = NumberToString(std::get<float>(_value));
		return true;
	case BindingValueKind::Double:
		out = NumberToString(std::get<double>(_value));
		return true;
	case BindingValueKind::String:
		out = std::get<std::wstring>(_value);
		return true;
	}
	return false;
}

bool TryConvertBindingValue(const BindingValue& value, BindingValueKind targetKind, BindingValue& out)
{
	switch (targetKind)
	{
	case BindingValueKind::Empty:
		out = BindingValue();
		return true;
	case BindingValueKind::Bool:
	{
		bool result = false;
		if (!value.TryGetBool(result)) return false;
		out = BindingValue(result);
		return true;
	}
	case BindingValueKind::Int:
	{
		int result = 0;
		if (!value.TryGetInt(result)) return false;
		out = BindingValue(result);
		return true;
	}
	case BindingValueKind::Int64:
	{
		long long result = 0;
		if (!value.TryGetInt64(result)) return false;
		out = BindingValue(result);
		return true;
	}
	case BindingValueKind::Float:
	{
		float result = 0.0f;
		if (!value.TryGetFloat(result)) return false;
		out = BindingValue(result);
		return true;
	}
	case BindingValueKind::Double:
	{
		double result = 0.0;
		if (!value.TryGetDouble(result)) return false;
		out = BindingValue(result);
		return true;
	}
	case BindingValueKind::String:
	{
		std::wstring result;
		if (!value.TryGetString(result)) return false;
		out = BindingValue(std::move(result));
		return true;
	}
	}
	return false;
}

PropertyChangedEventArgs::PropertyChangedEventArgs(std::wstring propertyName)
	: PropertyName(std::move(propertyName))
{
}

size_t PropertyChangedEvent::Add(Handler handler)
{
	if (!handler) return 0;
	const size_t token = _nextToken++;
	_handlers.push_back({ token, std::move(handler) });
	return token;
}

void PropertyChangedEvent::Remove(size_t token)
{
	if (token == 0) return;
	_handlers.erase(
		std::remove_if(_handlers.begin(), _handlers.end(), [token](const auto& item) { return item.first == token; }),
		_handlers.end());
}

void PropertyChangedEvent::Notify(const std::wstring& propertyName)
{
	if (_handlers.empty()) return;
	auto snapshot = _handlers;
	PropertyChangedEventArgs args(propertyName);
	for (auto& item : snapshot)
	{
		if (item.second)
			item.second(args);
	}
}

void PropertyChangedEvent::Clear()
{
	_handlers.clear();
}

bool ObservableObject::TryGetValue(const std::wstring& propertyName, BindingValue& out) const
{
	auto it = _values.find(propertyName);
	if (it == _values.end())
		return false;
	out = it->second;
	return true;
}

bool ObservableObject::TrySetValue(const std::wstring& propertyName, const BindingValue& value)
{
	if (propertyName.empty())
		return false;

	BindingValue next = value;
	auto it = _values.find(propertyName);
	if (it != _values.end())
	{
		BindingValue converted;
		if (TryConvertBindingValue(value, it->second.Kind(), converted))
			next = converted;

		if (BindingValuesEqual(it->second, next))
			return true;

		it->second = next;
	}
	else
	{
		_values[propertyName] = next;
	}

	OnPropertyChanged(propertyName);
	return true;
}

void ObservableObject::OnPropertyChanged(const std::wstring& propertyName)
{
	_propertyChanged.Notify(propertyName);
}

Binding::Binding(Control* target,
	std::wstring targetProperty,
	IBindingSource* source,
	std::wstring sourceProperty,
	BindingMode mode,
	DataSourceUpdateMode updateMode)
	: _target(target),
	_source(source),
	_targetProperty(std::move(targetProperty)),
	_sourceProperty(std::move(sourceProperty)),
	_mode(mode),
	_updateMode(updateMode),
	_state(std::make_shared<State>())
{
	_state->Owner = this;
	Attach();
}

Binding::~Binding()
{
	if (_state)
		_state->Owner = nullptr;
}

void Binding::Attach()
{
	AttachSourceChangedHandler();
	AttachTargetChangedHandlers();

	if (_mode == BindingMode::OneWayToSource)
		UpdateSource();
	else if (IsSourceToTargetMode(_mode))
		UpdateTarget();
}

void Binding::AttachSourceChangedHandler()
{
	if (!_source || _mode == BindingMode::OneWayToSource || _mode == BindingMode::OneTime)
		return;

	std::weak_ptr<State> weakState = _state;
	_source->PropertyChanged().Add([weakState](const PropertyChangedEventArgs& e)
		{
			auto state = weakState.lock();
			if (!state || !state->Owner) return;
			state->Owner->OnSourcePropertyChanged(e);
		});
}

void Binding::AttachTargetChangedHandlers()
{
	if (!_target || !IsTargetToSourceMode(_mode) || _updateMode == DataSourceUpdateMode::Never)
		return;

	std::weak_ptr<State> weakState = _state;
	auto updateSource = [weakState]()
		{
			auto state = weakState.lock();
			if (!state || !state->Owner) return;
			state->Owner->OnTargetPropertyChanged();
		};

	auto prop = Lower(_targetProperty);
	if (prop == L"text")
	{
		if (_updateMode == DataSourceUpdateMode::OnValidation)
		{
			_target->OnLostFocus += [updateSource](Control*) { updateSource(); };
		}
		else
		{
			_target->OnTextChanged += [updateSource](Control*, std::wstring, std::wstring) { updateSource(); };
		}
		return;
	}

	if (prop == L"checked")
	{
		_target->OnChecked += [updateSource](Control*) { updateSource(); };
		return;
	}

	if (prop == L"selectedindex")
	{
		if (auto* combo = dynamic_cast<ComboBox*>(_target))
			combo->OnSelectionChanged += [updateSource](Control*) { updateSource(); };
		else if (auto* list = dynamic_cast<ListView*>(_target))
			list->SelectionChanged += [updateSource](Control*) { updateSource(); };
		return;
	}

	if (prop == L"value")
	{
		if (auto* slider = dynamic_cast<Slider*>(_target))
			slider->OnValueChanged += [updateSource](Control*, float, float) { updateSource(); };
		else if (auto* number = dynamic_cast<NumericUpDown*>(_target))
			number->OnValueChanged += [updateSource](NumericUpDown*, double, double) { updateSource(); };
		return;
	}
}

void Binding::OnSourcePropertyChanged(const PropertyChangedEventArgs& e)
{
	if (!e.PropertyName.empty() && !IsSameProperty(e.PropertyName, _sourceProperty))
		return;
	UpdateTarget();
}

void Binding::OnTargetPropertyChanged()
{
	UpdateSource();
}

bool Binding::UpdateTarget()
{
	if (!_target || !_source || !IsSourceToTargetMode(_mode) || _updatingSource)
		return false;

	BindingValue value;
	if (!_source->TryGetValue(_sourceProperty, value))
		return false;

	_updatingTarget = true;
	bool ok = WriteTargetValue(_target, _targetProperty, value);
	_updatingTarget = false;
	return ok;
}

bool Binding::UpdateSource()
{
	if (!_target || !_source || !IsTargetToSourceMode(_mode) || _updatingTarget)
		return false;

	BindingValue value;
	if (!ReadTargetValue(_target, _targetProperty, value))
		return false;

	_updatingSource = true;
	bool ok = _source->TrySetValue(_sourceProperty, value);
	_updatingSource = false;
	return ok;
}

BindingCollection::BindingCollection(Control* owner)
	: _owner(owner)
{
}

Binding* BindingCollection::Add(const std::wstring& targetProperty,
	IBindingSource* source,
	const std::wstring& sourceProperty,
	BindingMode mode,
	DataSourceUpdateMode updateMode)
{
	if (!_owner || !source)
		return nullptr;

	auto binding = std::make_unique<Binding>(_owner, targetProperty, source, sourceProperty, mode, updateMode);
	auto* result = binding.get();
	_items.push_back(std::move(binding));
	return result;
}

void BindingCollection::Clear()
{
	_items.clear();
}

size_t BindingCollection::Count() const
{
	return _items.size();
}

Binding* BindingCollection::operator[](size_t index)
{
	return index < _items.size() ? _items[index].get() : nullptr;
}

const Binding* BindingCollection::operator[](size_t index) const
{
	return index < _items.size() ? _items[index].get() : nullptr;
}
