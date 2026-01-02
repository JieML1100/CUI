#pragma once
#include "Control.h"
#include <algorithm>

/**
 * @file Slider.h
 * @brief Slider：滑动条控件。
 *
 * 约定：
 * - 通过 Min/Max/Value 表示数值范围
 * - 拖拽滑块会更新 Value，并触发 OnValueChanged
 * - 可启用 SnapToStep 以 Step 为步进对 Value 进行吸附
 */

/**
 * @brief 数值变化事件。
 * @param oldValue 变化前的值。
 * @param newValue 变化后的值。
 */
typedef Event<void(class Control*, float oldValue, float newValue)> ValueChangedEvent;

class Slider : public Control
{
private:
	float _min = 0.0f;
	float _max = 100.0f;
	float _value = 0.0f;
	bool _dragging = false;

	float ClampValue(float v)
	{
		if (_max < _min) return _min;
		return std::clamp(v, _min, _max);
	}
	float TrackLeftLocal() { return 12.0f; }
	float TrackRightLocal() { return (float)this->Width - 12.0f; }
	float TrackYLocal() { return (float)this->Height * 0.5f; }
	float ValueToT()
	{
		float den = (_max - _min);
		if (den <= 0.00001f) return 0.0f;
		return (_value - _min) / den;
	}
	float XToValue(int xof)
	{
		float l = TrackLeftLocal();
		float r = TrackRightLocal();
		if (r <= l) return _min;
		float t = ((float)xof - l) / (r - l);
		t = std::clamp(t, 0.0f, 1.0f);
		return _min + t * (_max - _min);
	}
	void SetValueInternal(float v, bool fireEvent)
	{
		float old = _value;
		float nv = ClampValue(v);
		if (SnapToStep && Step > 0.0f)
		{
			float steps = (nv - _min) / Step;
			float snapped = _min + std::round(steps) * Step;
			nv = ClampValue(snapped);
		}
		if (nv != _value)
		{
			_value = nv;
			this->PostRender();
			if (fireEvent)
				this->OnValueChanged(this, old, _value);
		}
	}

public:
	/** @brief 值变化事件。 */
	ValueChangedEvent OnValueChanged;

	/** @brief 轨道背景色。 */
	D2D1_COLOR_F TrackBackColor = D2D1_COLOR_F{ 0.65f, 0.65f, 0.65f, 0.60f };
	/** @brief 已填充轨道颜色。 */
	D2D1_COLOR_F TrackForeColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.85f };
	/** @brief 滑块填充色。 */
	D2D1_COLOR_F ThumbColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.95f };
	/** @brief 滑块边框色。 */
	D2D1_COLOR_F ThumbBorderColor = D2D1_COLOR_F{ 0.10f, 0.10f, 0.10f, 0.35f };

	/** @brief 轨道高度（像素）。 */
	float TrackHeight = 4.0f;
	/** @brief 滑块半径（像素）。 */
	float ThumbRadius = 8.0f;
	/** @brief 步进值（用于 SnapToStep）。 */
	float Step = 1.0f;
	/** @brief 是否启用步进吸附。 */
	bool SnapToStep = false;

	/** @brief 创建滑动条。 */
	Slider(int x, int y, int width = 240, int height = 32);
	virtual UIClass Type() override;

	PROPERTY(float, Min);
	GET(float, Min);
	SET(float, Min);

	PROPERTY(float, Max);
	GET(float, Max);
	SET(float, Max);

	PROPERTY(float, Value);
	GET(float, Value);
	SET(float, Value);

	CursorKind QueryCursor(int xof, int yof) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};

