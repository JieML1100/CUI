#include "Resource.h"
#include "Application.h"
#include <BitmapSource.h>

#include <Windows.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace
{
	bool EqualsPath(const std::wstring& left, const std::wstring& right)
	{
		return _wcsicmp(left.c_str(), right.c_str()) == 0;
	}

	bool HasUnsupportedScheme(const std::wstring& uri)
	{
		const auto colon = uri.find(L':');
		if (colon == std::wstring::npos) return false;
		if (colon == 1 && std::iswalpha(uri.front())) return false;
		const auto separator = uri.find_first_of(L"/\\");
		return separator == std::wstring::npos || colon < separator;
	}

	std::filesystem::path NormalizeAbsolute(const std::filesystem::path& value)
	{
		std::error_code error;
		auto absolute = std::filesystem::absolute(value, error);
		if (error) absolute = value;
		return absolute.lexically_normal();
	}
}

FileResourceSource::FileResourceSource(
	std::vector<std::wstring> searchDirectories)
{
	_searchDirectories.reserve(searchDirectories.size());
	for (const auto& directory : searchDirectories)
	{
		if (directory.empty()) continue;
		auto normalized = NormalizeAbsolute(directory).wstring();
		if (std::none_of(_searchDirectories.begin(), _searchDirectories.end(),
			[&](const auto& current) { return EqualsPath(current, normalized); }))
			_searchDirectories.push_back(std::move(normalized));
	}
}

ResourceResolveStatus FileResourceSource::Resolve(
	const ResourceRequest& request,
	ResolvedResource& output,
	std::wstring* outError) const
{
	if (request.Uri.empty())
	{
		if (outError) *outError = L"资源 URI 不能为空。";
		return ResourceResolveStatus::Failed;
	}
	if (HasUnsupportedScheme(request.Uri))
		return ResourceResolveStatus::NotHandled;

	const std::filesystem::path uriPath(request.Uri);
	std::vector<std::filesystem::path> candidates;
	if (uriPath.is_absolute()) candidates.push_back(uriPath);
	else
	{
		if (!request.BaseUri.empty())
			candidates.push_back(std::filesystem::path(request.BaseUri) / uriPath);
		for (const auto& root : _searchDirectories)
			candidates.push_back(std::filesystem::path(root) / uriPath);
	}

	std::vector<std::wstring> visited;
	for (auto candidate : candidates)
	{
		candidate = NormalizeAbsolute(candidate);
		const auto candidateName = candidate.wstring();
		if (std::any_of(visited.begin(), visited.end(),
			[&](const auto& current) { return EqualsPath(current, candidateName); }))
			continue;
		visited.push_back(candidateName);

		std::error_code error;
		if (!std::filesystem::exists(candidate, error)) continue;
		if (error || !std::filesystem::is_regular_file(candidate, error))
		{
			if (outError) *outError = L"资源不是可读取文件：" + candidateName;
			return ResourceResolveStatus::Failed;
		}

		std::ifstream stream(candidate, std::ios::binary);
		if (!stream)
		{
			if (outError) *outError = L"无法打开资源文件：" + candidateName;
			return ResourceResolveStatus::Failed;
		}
		stream.seekg(0, std::ios::end);
		const auto length = stream.tellg();
		if (length < 0)
		{
			if (outError) *outError = L"无法读取资源文件长度：" + candidateName;
			return ResourceResolveStatus::Failed;
		}
		stream.seekg(0, std::ios::beg);
		std::vector<unsigned char> bytes(static_cast<size_t>(length));
		if (!bytes.empty())
			stream.read(reinterpret_cast<char*>(bytes.data()),
				static_cast<std::streamsize>(bytes.size()));
		if (!stream && !stream.eof())
		{
			if (outError) *outError = L"读取资源文件失败：" + candidateName;
			return ResourceResolveStatus::Failed;
		}

		output = {};
		output.RequestedUri = request.Uri;
		output.Identity = candidateName;
		output.BaseUri = candidate.parent_path().wstring();
		output.FilePath = candidateName;
		output.Bytes = std::move(bytes);
		output.Dependency = { candidateName, candidateName };
		if (outError) outError->clear();
		return ResourceResolveStatus::Resolved;
	}
	return ResourceResolveStatus::NotHandled;
}

ResourceResolver::ResourceResolver(
	std::vector<std::shared_ptr<const IResourceSource>> sources)
	: _sources(std::move(sources))
{
	_sources.erase(std::remove(_sources.begin(), _sources.end(), nullptr),
		_sources.end());
}

void ResourceResolver::AddSource(std::shared_ptr<const IResourceSource> source)
{
	if (source) _sources.push_back(std::move(source));
}

bool ResourceResolver::Resolve(
	const ResourceRequest& request,
	ResolvedResource& output,
	std::wstring* outError) const
{
	for (const auto& source : _sources)
	{
		std::wstring error;
		const auto status = source->Resolve(request, output, &error);
		if (status == ResourceResolveStatus::Resolved)
		{
			if (output.RequestedUri.empty()) output.RequestedUri = request.Uri;
			if (output.Identity.empty()) output.Identity = output.RequestedUri;
			if (output.Dependency.Identity.empty())
				output.Dependency.Identity = output.Identity;
			if (outError) outError->clear();
			return true;
		}
		if (status == ResourceResolveStatus::Failed)
		{
			if (outError) *outError = error.empty()
				? L"资源源无法加载：" + request.Uri : std::move(error);
			return false;
		}
	}
	if (outError) *outError = L"未找到资源：" + request.Uri;
	return false;
}

ResourceLoadContext::ResourceLoadContext(
	std::shared_ptr<const ResourceResolver> resolver)
	: _resolver(std::move(resolver))
{
}

bool ResourceLoadContext::Resolve(
	const std::wstring& uri,
	const std::wstring& baseUri,
	ResolvedResource& output,
	std::wstring* outError) const
{
	if (!_resolver)
	{
		if (outError) *outError = L"应用程序尚未配置资源解析器。";
		return false;
	}
	if (!_resolver->Resolve({ uri, baseUri }, output, outError)) return false;
	if (!output.Dependency.Identity.empty())
	{
		std::scoped_lock lock(_mutex);
		if (std::none_of(_dependencies.begin(), _dependencies.end(),
			[&](const auto& current)
			{
				return _wcsicmp(current.Identity.c_str(),
					output.Dependency.Identity.c_str()) == 0;
			}))
			_dependencies.push_back(output.Dependency);
	}
	return true;
}

std::vector<ResourceDependency> ResourceLoadContext::Dependencies() const
{
	std::scoped_lock lock(_mutex);
	return _dependencies;
}

bool cui::resources::Resolve(
	const std::wstring& uri,
	ResolvedResource& output,
	const std::wstring& baseUri,
	std::wstring* outError)
{
	auto context = std::make_shared<ResourceLoadContext>(
		Application::GetResourceResolver());
	return context->Resolve(uri, baseUri, output, outError);
}

std::shared_ptr<BitmapSource> cui::resources::LoadBitmapResource(
	const std::wstring& uri,
	const std::wstring& baseUri,
	std::wstring* outError)
{
	ResolvedResource resource;
	if (!cui::resources::Resolve(uri, resource, baseUri, outError)) return {};
	auto result = BitmapSource::FromBuffer(
		resource.Bytes.data(), resource.Bytes.size(), uri);
	if (!result && outError)
		*outError = L"无法解码图像资源：" + resource.Identity;
	return result;
}
