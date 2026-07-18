#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

class BitmapSource;

struct ResourceRequest
{
	std::wstring Uri;
	/** Logical base URI of the document containing Uri. */
	std::wstring BaseUri;
};

struct ResourceDependency
{
	/** Stable source-defined identity for cycle detection and de-duplication. */
	std::wstring Identity;
	/** Optional physical path that can be watched for changes. */
	std::wstring WatchPath;

	bool operator==(const ResourceDependency&) const = default;
};

struct ResolvedResource
{
	std::wstring RequestedUri;
	std::wstring Identity;
	/** Logical base URI for references inside this resource. */
	std::wstring BaseUri;
	/** Optional physical backing file. Consumers must not require it. */
	std::wstring FilePath;
	std::vector<unsigned char> Bytes;
	ResourceDependency Dependency;
};

enum class ResourceResolveStatus
{
	NotHandled,
	Resolved,
	Failed,
};

/** Extensible source contract for files today and packaged resources later. */
class IResourceSource
{
public:
	virtual ~IResourceSource() = default;
	virtual ResourceResolveStatus Resolve(
		const ResourceRequest& request,
		ResolvedResource& output,
		std::wstring* outError = nullptr) const = 0;
};

/** Absolute/document-relative file lookup plus configurable directory roots. */
class FileResourceSource final : public IResourceSource
{
public:
	explicit FileResourceSource(std::vector<std::wstring> searchDirectories = {});
	const std::vector<std::wstring>& SearchDirectories() const noexcept
	{
		return _searchDirectories;
	}
	ResourceResolveStatus Resolve(
		const ResourceRequest& request,
		ResolvedResource& output,
		std::wstring* outError = nullptr) const override;

private:
	std::vector<std::wstring> _searchDirectories;
};

/** Ordered composition of application resource sources. */
class ResourceResolver final
{
public:
	ResourceResolver() = default;
	explicit ResourceResolver(
		std::vector<std::shared_ptr<const IResourceSource>> sources);
	void AddSource(std::shared_ptr<const IResourceSource> source);
	const std::vector<std::shared_ptr<const IResourceSource>>& Sources() const noexcept
	{
		return _sources;
	}
	bool Resolve(
		const ResourceRequest& request,
		ResolvedResource& output,
		std::wstring* outError = nullptr) const;

private:
	std::vector<std::shared_ptr<const IResourceSource>> _sources;
};

/** Per-document resolver snapshot and dependency collector. */
class ResourceLoadContext final
{
public:
	explicit ResourceLoadContext(std::shared_ptr<const ResourceResolver> resolver);
	const std::shared_ptr<const ResourceResolver>& Resolver() const noexcept
	{
		return _resolver;
	}
	bool Resolve(
		const std::wstring& uri,
		const std::wstring& baseUri,
		ResolvedResource& output,
		std::wstring* outError = nullptr) const;
	std::vector<ResourceDependency> Dependencies() const;

private:
	std::shared_ptr<const ResourceResolver> _resolver;
	mutable std::mutex _mutex;
	mutable std::vector<ResourceDependency> _dependencies;
};

namespace cui::resources
{
	/** Resolves through the current Application resource configuration. */
	bool Resolve(
		const std::wstring& uri,
		ResolvedResource& output,
		const std::wstring& baseUri = {},
		std::wstring* outError = nullptr);
	/** Loads raster or SVG image content without requiring a physical file path. */
	std::shared_ptr<BitmapSource> LoadBitmapResource(
		const std::wstring& uri,
		const std::wstring& baseUri = {},
		std::wstring* outError = nullptr);
}
