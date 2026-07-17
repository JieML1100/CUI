#include "DesignCodeGenerationService.h"

#include "AtomicFile.h"
#include "DesignDocumentCodeGenInputBuilder.h"
#include "DesignDocumentFileFormat.h"
#include "DesignDocumentSerializer.h"
#include "CppUserCodeIndex.h"
#include "XamlDocumentParser.h"
#include "../CodeGenerator.h"
#include "../DesignerEventCatalog.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <set>
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

	std::string NarrowAscii(std::wstring_view value)
	{
		std::string result;
		result.reserve(value.size());
		for (const auto character : value)
			result.push_back(static_cast<char>(character));
		return result;
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

	bool BuildGenerationPlan(
		const DesignDocument& document,
		const std::wstring& designFilePath,
		const DesignCodeGenerationOptions& options,
		DesignCodeGenerationResult& result,
		std::vector<CodeGeneratorFileContent>& files,
		std::wstring* outError)
	{
		result = {};
		files.clear();
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

		PopulateResult(designFilePath, outputBase, className, result);
		CodeGenerator generator(className, input);
		if (!generator.BuildFilePlan(
			result.UserHeaderPath, result.UserSourcePath, files))
		{
			SetError(outError, generator.GetLastError().empty()
				? L"无法构建代码生成计划。" : generator.GetLastError());
			files.clear();
			return false;
		}
		return true;
	}

	bool FileContentEquals(
		const std::filesystem::path& path,
		const std::string& expected,
		bool& equal,
		std::wstring* outError)
	{
		equal = false;
		std::ifstream stream(path, std::ios::binary | std::ios::ate);
		if (!stream)
		{
			SetError(outError, L"无法读取代码文件：" + path.wstring());
			return false;
		}
		const auto size = stream.tellg();
		if (size < 0)
		{
			SetError(outError, L"无法读取代码文件大小：" + path.wstring());
			return false;
		}
		if (static_cast<unsigned long long>(size)
			!= static_cast<unsigned long long>(expected.size()))
			return true;
		stream.seekg(0, std::ios::beg);
		constexpr size_t chunkSize = 64 * 1024;
		char buffer[chunkSize];
		size_t offset = 0;
		while (offset < expected.size())
		{
			const auto count = std::min(chunkSize, expected.size() - offset);
			stream.read(buffer, static_cast<std::streamsize>(count));
			if (stream.gcount() != static_cast<std::streamsize>(count))
			{
				SetError(outError, L"读取代码文件时发生截断：" + path.wstring());
				return false;
			}
			if (!std::equal(buffer, buffer + count, expected.data() + offset))
				return true;
			offset += count;
		}
		equal = true;
		return true;
	}

	bool EnsureOutputDirectory(
		const DesignCodeGenerationResult& result,
		std::wstring* outError)
	{
		const auto parent =
			std::filesystem::path(result.OutputBasePath).parent_path();
		if (parent.empty()) return true;
		std::error_code directoryError;
		std::filesystem::create_directories(parent, directoryError);
		if (!directoryError) return true;
		SetError(outError, L"无法创建代码输出目录："
			+ Widen(directoryError.message()));
		return false;
	}

	bool CommitGenerationPlan(
		const std::vector<CodeGeneratorFileContent>& files,
		std::wstring* outError)
	{
		std::vector<AtomicFileWriteEntry> writes;
		writes.reserve(files.size());
		for (const auto& file : files)
		{
			AtomicFileWriteEntry write;
			write.FilePath = file.Path;
			write.Content = file.Content;
			write.RequireExpectedState = true;
			write.ExpectedExisted = file.ExpectedExisted;
			write.ExpectedContent = file.ExpectedContent;
			writes.push_back(std::move(write));
		}
		return AtomicFile::WriteBatch(writes, outError);
	}

	bool RestoreGenerationInputs(
		const std::vector<CodeGeneratorFileContent>& committedFiles,
		std::wstring* outError)
	{
		std::vector<AtomicFileWriteEntry> writes;
		writes.reserve(committedFiles.size());
		for (const auto& file : committedFiles)
		{
			AtomicFileWriteEntry write;
			write.FilePath = file.Path;
			write.Content = file.ExpectedContent;
			write.RemoveTarget = !file.ExpectedExisted;
			write.RequireExpectedState = true;
			// A successful generation commit leaves all five targets present.
			write.ExpectedExisted = true;
			write.ExpectedContent = file.Content;
			writes.push_back(std::move(write));
		}
		return AtomicFile::WriteBatch(writes, outError);
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

bool DesignCodeGenerationService::BuildCodeBehindAssociation(
	const std::wstring& className,
	const std::wstring& outputBasePath,
	const std::wstring& designFilePath,
	DesignCodeBehindModel& association,
	std::wstring* outError)
{
	association = {};
	if (outError) outError->clear();
	try
	{
		if (className.empty() || outputBasePath.empty())
		{
			SetError(outError, L"code-behind 关联参数无效。");
			return false;
		}
		DesignCodeBehindModel candidate;
		if (!DesignCodeBehindModel::TryNormalizeClassName(
			className, candidate.ClassName, outError))
			return false;
		if (!ValidateOutputBase(
			std::filesystem::path(outputBasePath), outError))
			return false;

		if (!designFilePath.empty())
		{
			std::error_code error;
			const auto absoluteDesign = std::filesystem::absolute(
				std::filesystem::path(designFilePath), error);
			if (error)
			{
				SetError(outError, L"无法解析设计文件路径："
					+ Widen(error.message()));
				return false;
			}
			const auto absoluteOutput = std::filesystem::absolute(
				std::filesystem::path(outputBasePath), error);
			if (error)
			{
				SetError(outError, L"无法解析代码生成输出路径："
					+ Widen(error.message()));
				return false;
			}
			const auto relative = std::filesystem::relative(
				absoluteOutput, absoluteDesign.parent_path(), error);
			if (error || relative.empty())
			{
				std::wstring message =
					L"代码导出位置无法表示为相对于设计文件的路径。";
				if (error) message += L"\n" + Widen(error.message());
				SetError(outError, std::move(message));
				return false;
			}
			if (!DesignCodeBehindModel::TryNormalizeRelativeBasePath(
				relative.generic_wstring(), candidate.RelativeBasePath, outError))
				return false;
		}
		if (!candidate.Validate(outError)) return false;
		association = std::move(candidate);
		return true;
	}
	catch (const std::exception& error)
	{
		SetError(outError,
			L"无法构建 code-behind 关联：" + Widen(error.what()));
		return false;
	}
	catch (...)
	{
		SetError(outError, L"无法构建 code-behind 关联：发生未知异常。");
		return false;
	}
}

bool DesignCodeGenerationService::BuildCodeExportPlan(
	const DesignCodeBehindModel& existingAssociation,
	const std::wstring& requestedClassName,
	const std::wstring& outputBasePath,
	const std::wstring& designFilePath,
	DesignCodeExportPlan& plan,
	std::wstring* outError)
{
	plan = {};
	if (outError) outError->clear();
	try
	{
		std::wstring validation;
		if (!existingAssociation.Validate(&validation))
		{
			SetError(outError, L"现有 code-behind 关联无效：" + validation);
			return false;
		}

		DesignCodeExportPlan candidate;
		if (!BuildCodeBehindAssociation(
			requestedClassName, outputBasePath, designFilePath,
			candidate.Association, outError))
			return false;

		std::wstring existingClass;
		if (!existingAssociation.ClassName.empty()
			&& !DesignCodeBehindModel::TryNormalizeClassName(
				existingAssociation.ClassName, existingClass, outError))
			return false;
		std::wstring existingRelativePath;
		if (!DesignCodeBehindModel::TryNormalizeRelativeBasePath(
			existingAssociation.RelativeBasePath,
			existingRelativePath, outError))
			return false;

		candidate.CreatesAssociation = existingAssociation.Empty();
		candidate.MigratesClass = !existingClass.empty()
			&& existingClass != candidate.Association.ClassName;
		candidate.ChangesRelativeOutput = !candidate.CreatesAssociation
			&& existingRelativePath != candidate.Association.RelativeBasePath;
		plan = std::move(candidate);
		return true;
	}
	catch (const std::exception& error)
	{
		SetError(outError,
			L"无法构建代码导出计划：" + Widen(error.what()));
		return false;
	}
	catch (...)
	{
		SetError(outError, L"无法构建代码导出计划：发生未知异常。");
		return false;
	}
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
		DesignCodeGenerationResult result;
		std::vector<CodeGeneratorFileContent> files;
		if (!BuildGenerationPlan(
			document, designFilePath, options, result, files, outError))
			return false;

		if (!EnsureOutputDirectory(result, outError)) return false;
		std::wstring writeError;
		if (!CommitGenerationPlan(files, &writeError))
		{
			SetError(outError,
				L"代码文件批次提交失败；已尝试恢复导出前版本。"
				+ (writeError.empty() ? std::wstring{} : L"\n" + writeError));
			return false;
		}

		if (outResult) *outResult = std::move(result);
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

bool DesignCodeGenerationService::InspectFreshness(
	const DesignDocument& document,
	const std::wstring& designFilePath,
	const DesignCodeGenerationOptions& options,
	DesignCodeFreshnessResult& freshness,
	std::wstring* outError)
{
	freshness = {};
	if (outError) outError->clear();
	if (document.CodeBehind.Empty()
		&& options.ClassName.empty() && options.OutputBasePath.empty())
	{
		freshness.State = DesignCodeFreshnessState::Unassociated;
		return true;
	}
	try
	{
		std::vector<CodeGeneratorFileContent> files;
		std::wstring planError;
		if (!BuildGenerationPlan(
			document, designFilePath, options,
			freshness.Target, files, &planError))
		{
			freshness.State = DesignCodeFreshnessState::Blocked;
			freshness.Diagnostic = planError.empty()
				? L"无法构建代码生成计划。" : std::move(planError);
			return true;
		}

		for (const auto& file : files)
		{
			std::error_code existsError;
			const bool exists = std::filesystem::exists(
				std::filesystem::path(file.Path), existsError);
			if (existsError)
			{
				freshness.State = DesignCodeFreshnessState::Blocked;
				freshness.Diagnostic = L"无法检查代码文件：" + file.Path
					+ L"\n" + Widen(existsError.message());
				return true;
			}
			if (!exists)
			{
				freshness.MissingFiles.push_back(file.Path);
				continue;
			}
			bool equal = false;
			std::wstring compareError;
			if (!FileContentEquals(
				std::filesystem::path(file.Path), file.Content,
				equal, &compareError))
			{
				freshness.State = DesignCodeFreshnessState::Blocked;
				freshness.Diagnostic = std::move(compareError);
				return true;
			}
			if (!equal) freshness.ChangedFiles.push_back(file.Path);
		}

		freshness.State = !freshness.MissingFiles.empty()
			? DesignCodeFreshnessState::Missing
			: !freshness.ChangedFiles.empty()
				? DesignCodeFreshnessState::Stale
				: DesignCodeFreshnessState::Current;
		return true;
	}
	catch (const std::exception& error)
	{
		SetError(outError,
			L"检查代码生成新鲜度失败：" + Widen(error.what()));
		freshness = {};
		return false;
	}
	catch (...)
	{
		SetError(outError, L"检查代码生成新鲜度时发生未知异常。");
		freshness = {};
		return false;
	}
}

bool DesignCodeGenerationService::InspectEventHandlers(
	const DesignDocument& document,
	const std::wstring& designFilePath,
	const DesignCodeGenerationOptions& options,
	DesignEventHandlerCodeInspection& inspection,
	std::wstring* outError)
{
	inspection = {};
	if (outError) outError->clear();
	if (document.CodeBehind.Empty()
		&& options.ClassName.empty() && options.OutputBasePath.empty())
		return true;
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
		if (!DesignDocumentCodeGenInputBuilder::Build(document, input, &error))
		{
			SetError(outError, error.empty()
				? L"无法从设计文档构建事件代码检查输入。"
				: std::move(error));
			return false;
		}
		inspection.Associated = true;
		PopulateResult(
			designFilePath, outputBase, className, inspection.Target);

		AtomicFileBatchSnapshot userFiles;
		if (!AtomicFileBatchSnapshot::Capture({
			inspection.Target.UserHeaderPath,
			inspection.Target.UserSourcePath,
		}, userFiles, &error))
		{
			SetError(outError, error.empty()
				? L"无法捕获用户事件代码文件。" : std::move(error));
			inspection = {};
			return false;
		}
		if (userFiles.Entries().size() != 2)
		{
			SetError(outError, L"用户事件代码文件快照不完整。");
			inspection = {};
			return false;
		}
		const auto& headerFile = userFiles.Entries()[0];
		const auto& sourceFile = userFiles.Entries()[1];
		const bool headerExists = headerFile.Existed;
		const bool sourceExists = sourceFile.Existed;
		const auto& header = headerFile.Content;
		const auto& source = sourceFile.Content;

		CodeGenerator generator(className, input);
		std::vector<CodeGeneratorHandlerDefinitionInspection> handlers;
		if (!generator.InspectUserHandlerDefinitions(
			header, source, handlers))
		{
			SetError(outError, generator.GetLastError().empty()
				? L"无法检查用户事件处理函数。" : generator.GetLastError());
			inspection = {};
			return false;
		}
		bool sourceRequired = false;
		for (const auto& handler : handlers)
		{
			DesignEventHandlerCodeEntry entry;
			entry.HandlerName = Widen(handler.Name);
			entry.ParameterList = Widen(handler.ParameterList);
			entry.DefinitionCount = handler.DefinitionCount;
			if (handler.FirstHeaderCompatibleDefinitionLine > 0)
			{
				entry.DefinitionFilePath = inspection.Target.UserHeaderPath;
				entry.DefinitionLine =
					handler.FirstHeaderCompatibleDefinitionLine;
			}
			else if (handler.FirstSourceCompatibleDefinitionLine > 0)
			{
				entry.DefinitionFilePath = inspection.Target.UserSourcePath;
				entry.DefinitionLine =
					handler.FirstSourceCompatibleDefinitionLine;
			}
			else if (handler.FirstHeaderDefinitionLine > 0)
			{
				entry.DefinitionFilePath = inspection.Target.UserHeaderPath;
				entry.DefinitionLine = handler.FirstHeaderDefinitionLine;
			}
			else if (handler.FirstSourceDefinitionLine > 0)
			{
				entry.DefinitionFilePath = inspection.Target.UserSourcePath;
				entry.DefinitionLine = handler.FirstSourceDefinitionLine;
			}
			if (!sourceExists
				&& handler.State == CodeGeneratorHandlerDefinitionState::Missing)
			{
				entry.State = DesignEventHandlerCodeState::SourceMissing;
				entry.Diagnostic =
					L"用户源文件尚不存在且头中没有内联实现；双击事件可生成。";
				sourceRequired = true;
			}
			else switch (handler.State)
			{
			case CodeGeneratorHandlerDefinitionState::Compatible:
				entry.State = DesignEventHandlerCodeState::Current;
				entry.Diagnostic = entry.DefinitionFilePath
					== inspection.Target.UserHeaderPath
					? L"处理函数已在用户头中内联实现；双击可定位。"
					: L"处理函数已在用户源中实现；双击可定位。";
				break;
			case CodeGeneratorHandlerDefinitionState::Incompatible:
				entry.State = DesignEventHandlerCodeState::SignatureMismatch;
				entry.Diagnostic =
					L"用户头或源中的现有定义无法覆盖生成的 void 事件函数"
						L"（返回类型、static/cv/ref 限定或参数不匹配）；"
						L"双击定位后修正。";
				break;
			case CodeGeneratorHandlerDefinitionState::DuplicateCompatible:
				entry.State = DesignEventHandlerCodeState::DuplicateDefinition;
				entry.Diagnostic =
					L"用户头/源合计存在多个相同签名定义；双击定位并仅保留一个。";
				break;
			case CodeGeneratorHandlerDefinitionState::Missing:
			default:
				entry.State = DesignEventHandlerCodeState::DefinitionMissing;
				entry.Diagnostic =
					L"用户头和源文件都缺少定义；双击可补齐并定位。";
				break;
			}
			inspection.Handlers.emplace(entry.HandlerName, std::move(entry));
		}
		if (headerExists || sourceExists)
		{
			CppUserCodeIndex headerIndex;
			CppUserCodeIndex sourceIndex;
			std::wstring indexError;
			const auto qualifiedClassName = NarrowAscii(className);
			if (!CppUserCodeIndex::Build(
				header, qualifiedClassName, headerIndex, &indexError)
				|| !CppUserCodeIndex::Build(
				source, qualifiedClassName, sourceIndex, &indexError))
			{
				SetError(outError, indexError.empty()
					? L"无法发现用户头或源文件中的兼容处理函数。"
					: std::move(indexError));
				inspection = {};
				return false;
			}

			std::set<std::string> parameterLists;
			auto collectParameters = [&](const auto& events)
			{
				for (const auto& event : events)
					parameterLists.insert(event.ParameterList);
			};
			collectParameters(DesignerEventCatalog::GetFormEvents());
			for (const auto& node : document.Nodes)
				collectParameters(DesignerEventCatalog::GetControlEvents(
					node.Type, node.CustomEvents));

			for (const auto& parameters : parameterLists)
			{
				std::set<std::string> names;
				for (auto& name :
					headerIndex.FindCompatibleHandlerNames(parameters))
					names.insert(std::move(name));
				for (auto& name :
					sourceIndex.FindCompatibleHandlerNames(parameters))
					names.insert(std::move(name));
				for (const auto& name : names)
				{
					const auto headerDefinitions =
						headerIndex.InspectHandler(name, parameters);
					const auto sourceDefinitions =
						sourceIndex.InspectHandler(name, parameters);
					if (headerDefinitions.CompatibleDefinitionCount
						+ sourceDefinitions.CompatibleDefinitionCount != 1
						|| headerDefinitions.IncompatibleShapeDefinitionCount
							+ sourceDefinitions.IncompatibleShapeDefinitionCount != 0
						|| headerDefinitions.DeletedCompatibleDefinitionCount
							+ sourceDefinitions.DeletedCompatibleDefinitionCount != 0)
						continue;
					inspection.CompatibleUserHandlers[parameters]
						.push_back(Widen(name));
				}
			}
		}
		if (sourceRequired)
			inspection.Diagnostic = L"用户源文件尚不存在："
				+ inspection.Target.UserSourcePath;
		return true;
	}
	catch (const std::exception& error)
	{
		SetError(outError,
			L"检查事件处理函数失败：" + Widen(error.what()));
	}
	catch (...)
	{
		SetError(outError, L"检查事件处理函数时发生未知异常。");
	}
	inspection = {};
	return false;
}

bool DesignCodeGenerationService::GenerateAndCommit(
	const DesignDocument& document,
	const std::wstring& designFilePath,
	const DesignCodeGenerationOptions& options,
	const CommitCallback& commit,
	DesignCodeGenerationResult* outResult,
	std::wstring* outError)
{
	if (outResult) *outResult = {};
	if (outError) outError->clear();
	if (!commit)
	{
		SetError(outError, L"代码导出缺少关联提交回调。");
		return false;
	}
	try
	{
		DesignCodeGenerationResult generatedResult;
		std::vector<CodeGeneratorFileContent> files;
		if (!BuildGenerationPlan(
			document, designFilePath, options,
			generatedResult, files, outError))
			return false;
		if (!EnsureOutputDirectory(generatedResult, outError)) return false;
		std::wstring generationError;
		if (!CommitGenerationPlan(files, &generationError))
		{
			SetError(outError,
				L"代码文件批次提交失败；已尝试恢复导出前版本。"
				+ (generationError.empty()
					? std::wstring{} : L"\n" + generationError));
			return false;
		}

		std::wstring commitError;
		bool committed = false;
		try
		{
			committed = commit(generatedResult, commitError);
		}
		catch (const std::exception& error)
		{
			commitError = L"关联提交发生异常：" + Widen(error.what());
		}
		catch (...)
		{
			commitError = L"关联提交发生未知异常。";
		}
		if (!committed)
		{
			std::wstring rollbackError;
			const bool restored =
				RestoreGenerationInputs(files, &rollbackError);
			std::wstring failure = commitError.empty()
				? L"code-behind 关联提交失败。" : std::move(commitError);
			if (!restored)
			{
				failure += L"\n代码文件恢复不完整：";
				failure += rollbackError.empty()
					? L"发生未知恢复错误。" : rollbackError;
			}
			SetError(outError, std::move(failure));
			return false;
		}

		if (outResult) *outResult = std::move(generatedResult);
		return true;
	}
	catch (const std::exception& error)
	{
		SetError(outError,
			L"代码导出事务失败：" + Widen(error.what()));
		return false;
	}
	catch (...)
	{
		SetError(outError, L"代码导出事务失败：发生未知异常。");
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
