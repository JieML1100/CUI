#pragma once

#include "Control.h"

#include <memory>
#include <utility>

namespace cui::framework
{
	/**
	 * Installs the compiled projection of XAML Style and ResourceDictionary data.
	 *
	 * Applications author Style/Resources through XAML. This bridge deliberately
	 * exposes no second C++ authoring model and never writes Local DP values.
	 */
	struct StyleAccess final
	{
		StyleAccess() = delete;

		static void SetResourceKey(Control& target, std::wstring value)
		{
			target.SetStyleResourceKey(std::move(value));
		}

		static const std::wstring& ResourceKey(
			const Control& target) noexcept
		{
			return target.GetStyleResourceKey();
		}

		static bool SetTheme(
			Control& target,
			std::shared_ptr<const ControlStyleSheet> value,
			bool recursive = true)
		{
			return target.SetThemeStyleSheet(std::move(value), recursive);
		}

		static std::shared_ptr<const ControlStyleSheet> Theme(
			const Control& target) noexcept
		{
			return target.GetThemeStyleSheet();
		}

		static bool SetDocumentStyles(
			Control& target,
			std::shared_ptr<const ControlStyleSheet> value,
			bool recursive = true)
		{
			return target.SetStyleSheet(std::move(value), recursive);
		}

		static std::shared_ptr<const ControlStyleSheet> DocumentStyles(
			const Control& target) noexcept
		{
			return target.GetStyleSheet();
		}

		static bool SetResources(
			Control& target,
			std::shared_ptr<const ControlStyleSheet> value)
		{
			return target.SetResourceDictionary(std::move(value));
		}

		static std::shared_ptr<const ControlStyleSheet> Resources(
			const Control& target) noexcept
		{
			return target.GetResourceDictionary();
		}

		/**
		 * Returns true when refreshing Style values can resolve at least one
		 * visible rule. During XAML staging a child can already carry the
		 * compiler-projected effective values while its document/theme sheets
		 * have not yet been attached; refreshing in that interval would
		 * incorrectly clear those values.
		 */
		static bool HasVisibleStyleRules(const Control& target) noexcept
		{
			return target.HasVisibleStyleRules();
		}

		static bool Refresh(Control& target, bool recursive = true)
		{
			return target.RefreshStyleValues(recursive);
		}
	};
}
