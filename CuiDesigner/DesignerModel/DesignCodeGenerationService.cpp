#include "DesignCodeGenerationService.h"

#include "DesignDocumentCodeGenInputBuilder.h"
#include "DesignDocumentFileFormat.h"
#include "DesignDocumentSerializer.h"
#include "XamlDocumentParser.h"
#include "../CodeGenerator.h"

#include <exception>
#include <filesystem>
#include <string_view>
#include <utility>

namespace DesignerModel
{
namespace
{
	void SetError(std::wstring* outError, std::wstring message)
	{
		if (outError) *outError = std::move(message);
	}

	std::wstring Widen(std::string_view value)
	{
		return std::wstring(value.begin(), value.end());
	}

	bool ResolveClassName(
		const DesignDocument& document,
		const DesignCodeGenerationOptions& options,
		std::wstring& className,
		std::wstring* outError)
	{
		const auto& requested = options.ClassName.empty()
			? document.CodeBehind.ClassName : options.ClassName;
		if (requested.empty())
		{
			SetError(outError,
				L"设计文件缺少 x:Class；请保存 code-behind 关联或传入 --class。");
			return false;
		}
		if (!DesignCodeBehindModel::TryNormalizeClassName(
			requested, className, outError)) return false;
		return !className.empty();
	}

	bool ValidateOutputBase(
		const std::filesystem::path& value,
		std::wstring* outError)
	{
		const auto fileName = value.filename().wstring();
		if (value.empty() || fileName.empty()
			|| fileName == L"." || fileName == L"..")
		{
			SetError(outError, L"代码生成输出必须包含文件基名。");
			return false;
		}
		if (value.has_extension())
		{
			SetError(outError,
				L"代码生成输出必须是不带 .h/.cpp 扩展名的基路径。");
			return false;
		}
		return true;
	}

	bool ResolveOutputBase(
		const DesignDocument& document,
		const std::wstring& designFilePath,
		const DesignCodeGenerationOptions& options,
		std::filesystem::path& outputBase,
		std::wstring* outError)
	{
		std::error_code error;
		if (!options.OutputBasePath.empty())
		{
			outputBase = std::filesystem::absolute(
				std::filesystem::path(options.OutputBasePath), error);
			if (error)
			{
				SetError(outError, L"无法解析代码生成输出路径："
					+ Widen(error.message()));
				return false;
			}
		}
		else
		{
			if (document.CodeBehind.RelativeBasePath.empty())
			{
				SetError(outError,
					L"设计文件缺少 d:CodeBehind；请保存关联或传入 --output。");
				return false;
			}
			if (designFilePath.empty())
			{
				SetError(outError,
					L"使用 d:CodeBehind 时必须提供设计文件路径。");
				return false;
			}
			auto absoluteDesign = std::filesystem::absolute(
				std::filesystem::path(designFilePath), error);
			if (error)
			{
				SetError(outError, L"无法解析设计文件路径："
					+ Widen(error.message()));
				return false;
			}
			outputBase = absoluteDesign.parent_path()
				/ std::filesystem::path(document.CodeBehind.RelativeBasePath);
		}
		outputBase = outputBase.lexically_normal();
		return ValidateOutputBase(outputBase, outError);
	}

	void PopulateResult(
		const std::wstring& designFilePath,
		const std::filesystem::path& outputBase,
		const std::wstring& className,
		DesignCodeGenerationResult& result)
	{
		std::error_code error;
		result = {};
		if (!designFilePath.empty())
		{
			const auto absoluteDesign = std::filesystem::absolute(
				std::filesystem::path(designFilePath), error);
			result.DesignFilePath = error
				? designFilePath : absoluteDesign.lexically_normal().wstring();
		}
		result.OutputBasePath = outputBase.wstring();
		result.ClassName = className;
		result.UserHeaderPath = result.OutputBasePath + L".h";
		result.UserSourcePath = result.OutputBasePath + L".cpp";
		result.GeneratedHeaderPath = result.OutputBasePath + L".g.h";
		result.GeneratedSourcePath = result.OutputBasePath + L".g.cpp";
		result.HandlerDeclarationsPath =
			result.OutputBasePath + L".handlers.g.inc";
	}
}

std::vector<std::wstring> DesignCodeGenerationResult::OutputFiles() const
{
	return {
		UserHeaderPath,
		UserSourcePath,
		GeneratedHeaderPath,
		GeneratedSourcePath,
		HandlerDeclarationsPath
	};
}

bool DesignCodeGenerationService::LoadDocument(
	const std::wstring& designFilePath,
	DesignDocument& document,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (designFilePath.empty())
	{
		SetError(outError, L"设计文件路径不能为空。");
		return false;
	}
	if (!HasDesignDocumentExtension(designFilePath))
	{
		SetError(outError,
			L"设计文件扩展名必须是 .xml 或 .xaml。");
		return false;
	}
	return DetectDesignDocumentFileFormat(designFilePath)
		== DesignDocumentFileFormat::Xaml
		? XamlDocumentParser::LoadFromFile(
			designFilePath, document, outError)
		: DesignDocumentSerializer::LoadFromFile(
			designFilePath, document, outError);
}

bool DesignCodeGenerationService::Generate(
	const DesignDocument& document,
	const std::wstring& designFilePath,
	const DesignCodeGenerationOptions& options,
	DesignCodeGenerationResult* outResult,
	std::wstring* outError)
{
	if (outResult) *outResult = {};
	if (outError) outError->clear();
	try
	{
		std::wstring className;
		if (!ResolveClassName(document, options, className, outError))
			return false;

		std::filesystem::path outputBase;
		if (!ResolveOutputBase(
			document, designFilePath, options, outputBase, outError))
			return false;

		CodeGenInput input;
		std::wstring error;
		if (!DesignDocumentCodeGenInputBuilder::Build(
			document, input, &error))
		{
			SetError(outError, error.empty()
				? L"无法从设计文档构建代码生成输入。" : std::move(error));
			return false;
		}

		const auto parent = outputBase.parent_path();
		if (!parent.empty())
		{
			std::error_code directoryError;
			std::filesystem::create_directories(parent, directoryError);
			if (directoryError)
			{
				SetError(outError, L"无法创建代码输出目录："
					+ Widen(directoryError.message()));
				return false;
			}
		}

		CodeGenerator generator(className, input);
		if (!generator.GenerateFiles(
			outputBase.wstring() + L".h",
			outputBase.wstring() + L".cpp"))
		{
			SetError(outError, generator.GetLastError().empty()
				? L"代码生成失败。" : generator.GetLastError());
			return false;
		}

		if (outResult)
			PopulateResult(
				designFilePath, outputBase, className, *outResult);
		return true;
	}
	catch (const std::exception& error)
	{
		SetError(outError, L"代码生成失败：" + Widen(error.what()));
		return false;
	}
	catch (...)
	{
		SetError(outError, L"代码生成失败：发生未知异常。");
		return false;
	}
}

bool DesignCodeGenerationService::GenerateFile(
	const std::wstring& designFilePath,
	const DesignCodeGenerationOptions& options,
	DesignCodeGenerationResult* outResult,
	std::wstring* outError)
{
	DesignDocument document;
	if (!LoadDocument(designFilePath, document, outError)) return false;
	return Generate(
		document, designFilePath, options, outResult, outError);
}
}
