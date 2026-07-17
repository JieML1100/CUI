#include "Designer.h"
#include "DesignerControlCatalog.h"
#include "DesignerPreviewBridge.h"
#include "DesignerPreviewPlugin.h"
#include "DesignerSelfTest.h"
#include <Shellapi.h>
#include <filesystem>
#include <string>
#include <vector>
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#pragma comment(lib, "Shell32.lib")

namespace
{
	void WriteSelfTestReport(const std::wstring& report)
	{
		if (report.empty()) return;
		const int size = ::WideCharToMultiByte(
			CP_UTF8, 0, report.c_str(), static_cast<int>(report.size()),
			nullptr, 0, nullptr, nullptr);
		if (size <= 0) return;
		std::vector<char> utf8(static_cast<size_t>(size) + 2);
		(void)::WideCharToMultiByte(
			CP_UTF8, 0, report.c_str(), static_cast<int>(report.size()),
			utf8.data(), size, nullptr, nullptr);
		utf8[static_cast<size_t>(size)] = '\r';
		utf8[static_cast<size_t>(size) + 1] = '\n';
		const auto output = ::GetStdHandle(STD_OUTPUT_HANDLE);
		if (!output || output == INVALID_HANDLE_VALUE) return;
		DWORD written = 0;
		(void)::WriteFile(output, utf8.data(),
			static_cast<DWORD>(utf8.size()), &written, nullptr);
	}

	struct ProgramOptions
	{
		struct PreviewPluginValidationRequest
		{
			std::wstring Path;
			std::wstring XamlNamespace;
			std::wstring XamlName;
		};

		bool SelfTest = false;
		bool ValidateControls = false;
		bool ShowHelp = false;
		std::vector<std::wstring> ControlCatalogPaths;
		std::vector<std::wstring> PreviewPluginPaths;
		std::vector<PreviewPluginValidationRequest> PreviewPluginValidations;
	};

	bool ParseProgramOptions(ProgramOptions& options, std::wstring& error)
	{
		int count = 0;
		auto** arguments = ::CommandLineToArgvW(::GetCommandLineW(), &count);
		if (!arguments)
		{
			error = L"无法读取命令行参数。";
			return false;
		}
		for (int index = 1; index < count; ++index)
		{
			const std::wstring argument = arguments[index];
			if (argument == L"--self-test") options.SelfTest = true;
			else if (argument == L"--help" || argument == L"-h")
				options.ShowHelp = true;
			else if (argument == L"--controls"
				|| argument == L"--validate-controls")
			{
				if (argument == L"--validate-controls")
					options.ValidateControls = true;
				if (index + 1 >= count || arguments[index + 1][0] == L'-')
				{
					error = argument + L" 需要一个控件清单路径。";
					::LocalFree(arguments);
					return false;
				}
				options.ControlCatalogPaths.emplace_back(arguments[++index]);
			}
			else if (argument == L"--validate-preview-plugin")
			{
				if (index + 3 >= count)
				{
					error = argument
						+ L" 需要 DLL 路径、XAML 命名空间和控件名。";
					::LocalFree(arguments);
					return false;
				}
				ProgramOptions::PreviewPluginValidationRequest request;
				request.Path = arguments[++index];
				request.XamlNamespace = arguments[++index];
				request.XamlName = arguments[++index];
				options.PreviewPluginValidations.push_back(std::move(request));
			}
			else if (argument == L"--preview-plugin")
			{
				if (index + 1 >= count || arguments[index + 1][0] == L'-')
				{
					error = argument + L" 需要一个受信任的 DLL 路径。";
					::LocalFree(arguments);
					return false;
				}
				options.PreviewPluginPaths.emplace_back(arguments[++index]);
			}
			else
			{
				error = L"未知参数：" + argument;
				::LocalFree(arguments);
				return false;
			}
		}
		::LocalFree(arguments);
		if (options.SelfTest
			&& (options.ValidateControls || !options.ControlCatalogPaths.empty()
				|| !options.PreviewPluginPaths.empty()
				|| !options.PreviewPluginValidations.empty()))
		{
			error = L"--self-test 不能与其他操作参数组合。";
			return false;
		}
		return true;
	}
}

int main()
{
	Application::EnsureDpiAwareness();
	ProgramOptions options;
	std::wstring optionError;
	if (!ParseProgramOptions(options, optionError))
	{
		WriteSelfTestReport(optionError);
		return 2;
	}
	if (options.ShowHelp)
	{
		WriteSelfTestReport(
			L"Designer [--controls <manifest>]...\n"
			L"         [--preview-plugin <trusted-dll>]...\n"
			L"Designer --validate-controls <manifest> [--controls <manifest>]...\n"
			L"         [--preview-plugin <trusted-dll>]...\n"
			L"Designer --validate-preview-plugin <dll> <xaml-namespace> <xaml-name>\n"
			L"Designer --self-test");
		return 0;
	}
	if (options.SelfTest)
	{
		std::wstring report;
		const bool passed = RunDesignerSelfTest(report);
		WriteSelfTestReport(report);
		return passed ? 0 : 1;
	}
	if (!options.PreviewPluginValidations.empty())
	{
		size_t primitiveCount = 0;
		for (const auto& request : options.PreviewPluginValidations)
		{
			std::error_code pathError;
			const auto absolutePath = std::filesystem::absolute(
				std::filesystem::path(request.Path), pathError);
			if (pathError)
			{
				WriteSelfTestReport(L"无法规范化预览插件路径。" );
				return 2;
			}
			DesignerPreviewPluginModule module;
			std::wstring error;
			if (!module.Load(absolutePath.wstring(), {}, &error))
			{
				WriteSelfTestReport(error);
				return 2;
			}
			DesignerPreviewPluginSession session;
			if (!module.CreateSession(
				request.XamlNamespace, request.XamlName, session, &error))
			{
				WriteSelfTestReport(error);
				return 2;
			}
			CuiDesignerPreviewFrameInputV1 input{};
			input.StructSize = sizeof(input);
			input.Width = 150.0f;
			input.Height = 30.0f;
			input.DpiScale = 1.0f;
			input.Enabled = 1;
			std::vector<DesignerPreviewPrimitive> primitives;
			if (!session.Render(input, primitives, &error))
			{
				WriteSelfTestReport(error);
				return 2;
			}
			primitiveCount += primitives.size();
		}
		WriteSelfTestReport(L"预览插件有效；已验证 "
			+ std::to_wstring(options.PreviewPluginValidations.size())
			+ L" 个 session、" + std::to_wstring(primitiveCount)
			+ L" 个绘制原语。" );
		return 0;
	}

	auto descriptors = DesignerControlCatalog::BuiltInDescriptors();
	for (const auto& path : options.ControlCatalogPaths)
	{
		std::wstring error;
		if (!DesignerControlCatalog::AppendFromFile(
			descriptors, path, &error))
		{
			WriteSelfTestReport(error);
			return 2;
		}
	}
	std::vector<DesignerPreviewBridge::Module> previewModules;
	previewModules.reserve(options.PreviewPluginPaths.size());
	for (const auto& path : options.PreviewPluginPaths)
	{
		std::error_code pathError;
		const auto absolutePath = std::filesystem::absolute(
			std::filesystem::path(path), pathError);
		if (pathError)
		{
			WriteSelfTestReport(L"无法规范化预览插件路径。");
			return 2;
		}
		auto module = std::make_shared<DesignerPreviewPluginModule>();
		std::wstring error;
		if (!module->Load(absolutePath.wstring(), {}, &error))
		{
			WriteSelfTestReport(error);
			return 2;
		}
		previewModules.push_back(std::move(module));
	}
	const size_t attachedPreviews =
		DesignerPreviewBridge::AttachFactories(descriptors, previewModules);
	if (options.ValidateControls)
	{
		WriteSelfTestReport(L"控件清单有效；已加载 "
			+ std::to_wstring(descriptors.size()
				- ControlRegistry::GetAvailableControls().size())
			+ L" 个自定义控件，已绑定 "
			+ std::to_wstring(attachedPreviews) + L" 个插件预览。");
		return 0;
	}

	Designer designer(std::move(descriptors));
	// 初始化完成后再显示，确保所有控件的ParentForm都已设置
	designer.InitAndShow();

	while (1)
	{
		Form::DoEvent();
		if (Application::Forms.size() == 0)
			break;
	}
	return 0;
}
