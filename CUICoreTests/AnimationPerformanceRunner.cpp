#include "AnimationPerformanceRunner.h"

#include "TestRunner.h"
#include "../CuiDesigner/DesignerModel/AtomicFile.h"
#include "../CuiDesigner/DesignerModel/XamlDocumentParser.h"
#include "../CuiRuntime/include/XamlObjectMaterializer.h"

#include <Border.h>
#include <Canvas.h>
#include <ContextMenu.h>
#include <Convert.h>
#include <Core/Threading.h>
#include <DCompLayeredHost.h>
#include <Graphics.h>
#include <LoadingRing.h>
#include <Menu.h>
#include <Popup.h>
#include <PresentationInfrastructure.h>
#include <TemplateInfrastructure.h>
#include <WebBrowser.h>
#include <Window.h>
#include <WindowInfrastructure.h>
#include <XamlInfrastructure.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <d2d1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <fcntl.h>
#include <io.h>
#include <psapi.h>
#include <shellapi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/base.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "windowsapp.lib")

namespace
{
	struct CapturedWindowFrame final
	{
		struct Pixel final
		{
			uint8_t Blue = 0;
			uint8_t Green = 0;
			uint8_t Red = 0;
			uint8_t Alpha = 0;
		};
		UINT Width = 0;
		UINT Height = 0;
		uint64_t Digest = 0;
		std::vector<uint8_t> Bgra;
		std::string Error;

		bool TryGetPixel(UINT x, UINT y, Pixel& result) const noexcept
		{
			result = {};
			if (x >= Width || y >= Height) return false;
			const size_t index =
				(static_cast<size_t>(y) * Width + x) * 4u;
			if (index + 3u >= Bgra.size()) return false;
			result = { Bgra[index], Bgra[index + 1u],
				Bgra[index + 2u], Bgra[index + 3u] };
			return true;
		}

		size_t CountColor(
			uint8_t blue,
			uint8_t green,
			uint8_t red,
			uint8_t tolerance = 8u) const noexcept
		{
			size_t count = 0;
			for (size_t index = 0; index + 3u < Bgra.size(); index += 4u)
			{
				auto withinTolerance =
					[tolerance](uint8_t value, uint8_t expected)
				{
					return std::abs(static_cast<int>(value)
						- static_cast<int>(expected)) <= tolerance;
				};
				if (Bgra[index + 3u] >= 240u
					&& withinTolerance(Bgra[index], blue)
					&& withinTolerance(Bgra[index + 1u], green)
					&& withinTolerance(Bgra[index + 2u], red)) ++count;
			}
			return count;
		}

		bool TryGetColorBounds(
			uint8_t blue,
			uint8_t green,
			uint8_t red,
			RECT& bounds,
			size_t& count,
			uint8_t tolerance = 8u) const noexcept
		{
			bounds = {};
			count = 0;
			if (Width == 0 || Height == 0
				|| Bgra.size() < static_cast<size_t>(Width) * Height * 4u)
				return false;
			LONG left = (std::numeric_limits<LONG>::max)();
			LONG top = (std::numeric_limits<LONG>::max)();
			LONG right = 0;
			LONG bottom = 0;
			auto withinTolerance =
				[tolerance](uint8_t value, uint8_t expected)
				{
					return std::abs(static_cast<int>(value)
						- static_cast<int>(expected)) <= tolerance;
				};
			for (UINT y = 0; y < Height; ++y)
			{
				for (UINT x = 0; x < Width; ++x)
				{
					const size_t index =
						(static_cast<size_t>(y) * Width + x) * 4u;
					if (Bgra[index + 3u] < 240u
						|| !withinTolerance(Bgra[index], blue)
						|| !withinTolerance(Bgra[index + 1u], green)
						|| !withinTolerance(Bgra[index + 2u], red)) continue;
					++count;
					left = (std::min)(left, static_cast<LONG>(x));
					top = (std::min)(top, static_cast<LONG>(y));
					right = (std::max)(right, static_cast<LONG>(x + 1u));
					bottom = (std::max)(bottom, static_cast<LONG>(y + 1u));
				}
			}
			if (count == 0) return false;
			bounds = RECT{ left, top, right, bottom };
			return true;
		}
	};

	CapturedWindowFrame CaptureWindowComposition(
		HWND window,
		DWORD timeoutMilliseconds = 5000u)
	{
		CapturedWindowFrame result;
		if (!window || !::IsWindow(window))
		{
			result.Error = "Capture target HWND is invalid.";
			return result;
		}
		try
		{
			const HRESULT apartmentResult = ::RoInitialize(
				RO_INIT_SINGLETHREADED);
			if (FAILED(apartmentResult)
				&& apartmentResult != RPC_E_CHANGED_MODE)
				winrt::check_hresult(apartmentResult);
			struct ApartmentScope final
			{
				bool Initialized = false;
				~ApartmentScope() { if (Initialized) ::RoUninitialize(); }
			} apartment{ SUCCEEDED(apartmentResult) };
				using namespace winrt::Windows::Graphics;
				using namespace winrt::Windows::Graphics::Capture;
				using namespace winrt::Windows::Graphics::DirectX;
				using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
				if (!GraphicsCaptureSession::IsSupported())
					throw std::runtime_error(
						"Windows.Graphics.Capture is not supported.");

				UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
				flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
				D3D_FEATURE_LEVEL featureLevel{};
				Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice;
				Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext;
				HRESULT createResult = ::D3D11CreateDevice(
					nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
					nullptr, 0, D3D11_SDK_VERSION,
					d3dDevice.ReleaseAndGetAddressOf(), &featureLevel,
					d3dContext.ReleaseAndGetAddressOf());
#if defined(_DEBUG)
				if (createResult == DXGI_ERROR_SDK_COMPONENT_MISSING)
				{
					flags &= ~D3D11_CREATE_DEVICE_DEBUG;
					createResult = ::D3D11CreateDevice(
						nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
						nullptr, 0, D3D11_SDK_VERSION,
						d3dDevice.ReleaseAndGetAddressOf(), &featureLevel,
						d3dContext.ReleaseAndGetAddressOf());
				}
#endif
				winrt::check_hresult(createResult);
				Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
				winrt::check_hresult(d3dDevice.As(&dxgiDevice));
				winrt::com_ptr<IInspectable> inspectableDevice;
				winrt::check_hresult(
					::CreateDirect3D11DeviceFromDXGIDevice(
						dxgiDevice.Get(), inspectableDevice.put()));
				auto captureDevice = inspectableDevice.as<IDirect3DDevice>();

				GraphicsCaptureItem item{ nullptr };
				auto itemInterop = winrt::get_activation_factory<
					GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
				winrt::check_hresult(itemInterop->CreateForWindow(
					window, winrt::guid_of<GraphicsCaptureItem>(),
					winrt::put_abi(item)));
				const auto itemSize = item.Size();
				if (itemSize.Width <= 0 || itemSize.Height <= 0)
					throw std::runtime_error("Capture item has an empty size.");
				auto framePool = Direct3D11CaptureFramePool::CreateFreeThreaded(
					captureDevice,
					DirectXPixelFormat::B8G8R8A8UIntNormalized,
					2, itemSize);
				auto session = framePool.CreateCaptureSession(item);
				try { session.IsCursorCaptureEnabled(false); }
				catch (...) {}
				struct EventOwner final
				{
					HANDLE Value = nullptr;
					~EventOwner() { if (Value) ::CloseHandle(Value); }
				} arrived{ ::CreateEventW(nullptr, TRUE, FALSE, nullptr) };
				if (!arrived.Value)
					winrt::throw_last_error();
				auto frameArrived = framePool.FrameArrived(
					winrt::auto_revoke,
					[event = arrived.Value](const auto&, const auto&)
					{
						(void)::SetEvent(event);
					});
				session.StartCapture();
				const DWORD waitResult = ::WaitForSingleObject(
					arrived.Value, timeoutMilliseconds);
				if (waitResult != WAIT_OBJECT_0)
					throw std::runtime_error(waitResult == WAIT_TIMEOUT
						? "Window composition capture timed out."
						: "Window composition capture wait failed.");
				auto frame = framePool.TryGetNextFrame();
				if (!frame)
					throw std::runtime_error(
						"Capture frame arrival produced no frame.");
				const auto contentSize = frame.ContentSize();
				auto surfaceAccess = frame.Surface().as<
					::Windows::Graphics::DirectX::Direct3D11::
						IDirect3DDxgiInterfaceAccess>();
				Microsoft::WRL::ComPtr<ID3D11Texture2D> source;
				winrt::check_hresult(surfaceAccess->GetInterface(
					IID_PPV_ARGS(source.ReleaseAndGetAddressOf())));
				D3D11_TEXTURE2D_DESC description{};
				source->GetDesc(&description);
				if (contentSize.Width <= 0 || contentSize.Height <= 0
					|| static_cast<UINT>(contentSize.Width) > description.Width
					|| static_cast<UINT>(contentSize.Height) > description.Height
					|| description.Format != DXGI_FORMAT_B8G8R8A8_UNORM)
					throw std::runtime_error(
						"Capture frame texture contract is invalid.");
				description.Usage = D3D11_USAGE_STAGING;
				description.BindFlags = 0;
				description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
				description.MiscFlags = 0;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
				winrt::check_hresult(d3dDevice->CreateTexture2D(
					&description, nullptr,
					staging.ReleaseAndGetAddressOf()));
				d3dContext->CopyResource(staging.Get(), source.Get());
				D3D11_MAPPED_SUBRESOURCE mapped{};
				winrt::check_hresult(d3dContext->Map(
					staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
				struct UnmapScope final
				{
					ID3D11DeviceContext* Context = nullptr;
					ID3D11Texture2D* Texture = nullptr;
					~UnmapScope()
					{
						if (Context && Texture) Context->Unmap(Texture, 0);
					}
				} unmap{ d3dContext.Get(), staging.Get() };

				result.Width = static_cast<UINT>(contentSize.Width);
				result.Height = static_cast<UINT>(contentSize.Height);
				const size_t rowBytes = static_cast<size_t>(result.Width) * 4u;
				result.Bgra.resize(rowBytes * result.Height);
				constexpr uint64_t FnvOffset = 14695981039346656037ull;
				constexpr uint64_t FnvPrime = 1099511628211ull;
				uint64_t digest = FnvOffset;
				for (UINT row = 0; row < result.Height; ++row)
				{
					const auto* sourceRow =
						static_cast<const uint8_t*>(mapped.pData)
						+ static_cast<size_t>(row) * mapped.RowPitch;
					auto* targetRow = result.Bgra.data()
						+ static_cast<size_t>(row) * rowBytes;
					memcpy(targetRow, sourceRow, rowBytes);
					for (size_t byte = 0; byte < rowBytes; ++byte)
					{
						digest ^= targetRow[byte];
						digest *= FnvPrime;
					}
				}
				result.Digest = digest;
				frame.Close();
				frameArrived.revoke();
				session.Close();
				framePool.Close();
		}
		catch (const winrt::hresult_error& error)
		{
			result.Error = Convert::UnicodeToUtf8(error.message().c_str());
		}
		catch (const std::exception& error)
		{
			result.Error = error.what();
		}
		catch (...)
		{
			result.Error = "Unknown window composition capture failure.";
		}
		return result;
	}

	constexpr std::string_view BenchmarkVersion = "0.6.0";
	constexpr unsigned long long BenchmarkClockOrigin = 2'000'000ull;
	constexpr double SixtyHertzFrameBudgetMicroseconds =
		1'000'000.0 / 60.0;
#if defined(_DEBUG) && defined(_WIN64)
	constexpr std::string_view BenchmarkBuildConfiguration =
		"Debug-x64-Design";
#elif defined(_DEBUG)
	constexpr std::string_view BenchmarkBuildConfiguration =
		"Debug-x86-Design";
#elif defined(_WIN64)
	constexpr std::string_view BenchmarkBuildConfiguration =
		"Release-x64-Design";
#else
	constexpr std::string_view BenchmarkBuildConfiguration =
		"Release-x86-Design";
#endif

	enum class BenchmarkPropertyKind
	{
		TransformX,
		Opacity,
	};

	constexpr std::string_view PropertyName(BenchmarkPropertyKind value) noexcept
	{
		switch (value)
		{
		case BenchmarkPropertyKind::TransformX: return "render-transform-x";
		case BenchmarkPropertyKind::Opacity: return "opacity";
		}
		return "unknown";
	}

	struct BenchmarkParameters final
	{
		std::string Profile;
		std::vector<size_t> AnimationCounts;
		size_t FrameWarmup = 0;
		size_t FrameSamples = 0;
		size_t RestartSamples = 0;
		size_t LifecycleSamples = 0;
		size_t ScanAnimationCount = 0;
		std::vector<size_t> UnrelatedVisualCounts;
		size_t ScanWarmup = 0;
		size_t ScanSamples = 0;
		size_t RetentionAnimationCount = 0;
		size_t RetentionWarmupCycles = 0;
		size_t RetentionCyclesPerPass = 0;
		size_t PresentationAnimationCount = 0;
		size_t PresentationWarmupFrames = 0;
		size_t PresentationFrameSamples = 0;
		unsigned PresentationTimeoutMilliseconds = 0;
	};

	struct Distribution final
	{
		size_t Count = 0;
		double MinimumMicroseconds = 0.0;
		double MeanMicroseconds = 0.0;
		double P50Microseconds = 0.0;
		double P95Microseconds = 0.0;
		double P99Microseconds = 0.0;
		double MaximumMicroseconds = 0.0;
	};

	struct ProcessMemory final
	{
		unsigned long long WorkingSetBytes = 0;
		unsigned long long PrivateUsageBytes = 0;
	};

	struct ScaleResult final
	{
		std::string Property;
		size_t AnimationCount = 0;
		double CreateInstallMicroseconds = 0.0;
		double BeginMicroseconds = 0.0;
		Distribution Advance;
		double EvaluatorFrameBudgetMicroseconds =
			SixtyHertzFrameBudgetMicroseconds;
		size_t EvaluatorFramesWithinBudget = 0;
		double EvaluatorOnTimeRate = 0.0;
		Distribution Restart;
		double StopMicroseconds = 0.0;
		Distribution Lifecycle;
		size_t ActiveLeavesAfterBegin = 0;
		size_t AnimationLayerStacksAfterBegin = 0;
		size_t AnimationLayersAfterBegin = 0;
		size_t AnimationLayerMaxDepthAfterBegin = 0;
		size_t ActiveLeavesAfterRestarts = 0;
		size_t AnimationLayerStacksAfterRestarts = 0;
		size_t AnimationLayersAfterRestarts = 0;
		size_t ActiveLeavesAfterStop = 0;
		size_t AnimationLayerStacksAfterStop = 0;
		size_t AnimationLayersAfterStop = 0;
		bool AnimationSlotsClearedAfterStop = false;
		ProcessMemory MemoryBefore;
		ProcessMemory MemoryAfter;
	};

	struct WindowTickResult final
	{
		std::string Property;
		size_t AnimationCount = 0;
		size_t UnrelatedVisualCount = 0;
		size_t ApproximateVisualCount = 0;
		Distribution WindowTick;
	};

	struct RegistryTickResult final
	{
		std::string Property;
		size_t AnimationCount = 0;
		size_t UnrelatedVisualCount = 0;
		size_t ApproximateVisualCount = 0;
		Distribution RegistryTick;
	};

	struct RetentionResult final
	{
		std::string Property;
		size_t AnimationCount = 0;
		size_t WarmupCycles = 0;
		size_t CyclesPerPass = 0;
		ProcessMemory Before;
		ProcessMemory AfterPass1;
		ProcessMemory AfterPass2;
		double Pass1Microseconds = 0.0;
		double Pass2Microseconds = 0.0;
	};

	struct PresentationCadenceResult final
	{
		std::string Property;
		size_t AnimationCount = 0;
		size_t WarmupFrames = 0;
		size_t FrameSamples = 0;
		double FrameBudgetMicroseconds = SixtyHertzFrameBudgetMicroseconds;
		Distribution CommittedFrameInterval;
		size_t FramesDeliveredByDeadline = 0;
		double OnTimeRate = 0.0;
		uint64_t CommittedFramesBefore = 0;
		uint64_t CommittedFramesAfter = 0;
		uint64_t AbortedFramesBefore = 0;
		uint64_t AbortedFramesAfter = 0;
	};

	struct BenchmarkResult final
	{
		BenchmarkParameters Parameters;
		unsigned long long QpcFrequency = 0;
		unsigned LogicalProcessorCount = 0;
		unsigned ProcessId = 0;
		unsigned PointerBits = 0;
		unsigned ProcessorArchitecture = 0;
		unsigned long long TotalPhysicalMemoryBytes = 0;
		std::string CpuName;
		ProcessMemory ProcessAtStart;
		ProcessMemory ProcessAtEnd;
		std::vector<ScaleResult> Scales;
		std::vector<WindowTickResult> WindowTicks;
		std::vector<RegistryTickResult> RegistryTicks;
		RetentionResult Retention;
		PresentationCadenceResult PresentationCadence;
	};

	struct BenchmarkOptions final
	{
		std::filesystem::path OutputPath;
		std::string Profile = "baseline";
	};

	struct BenchmarkCommandLine final
	{
		bool Requested = false;
		std::optional<BenchmarkOptions> Options;
		std::string Error;
	};

	const DeclarativeEventDefinition& BenchmarkBeginEvent()
	{
		static const DeclarativeEventDefinition value{
			BindingValueKind::Empty, DeclarativeEventRoutingStrategy::Direct };
		return value;
	}

	const DeclarativeEventDefinition& BenchmarkStopEvent()
	{
		static const DeclarativeEventDefinition value{
			BindingValueKind::Empty, DeclarativeEventRoutingStrategy::Direct };
		return value;
	}

	const DeclarativeEventDefinition& BenchmarkRemoveEvent()
	{
		static const DeclarativeEventDefinition value{
			BindingValueKind::Empty, DeclarativeEventRoutingStrategy::Direct };
		return value;
	}

	const DeclarativeEventDefinition& BenchmarkPauseEvent()
	{
		static const DeclarativeEventDefinition value{
			BindingValueKind::Empty, DeclarativeEventRoutingStrategy::Direct };
		return value;
	}

	const DeclarativeEventDefinition& BenchmarkResumeEvent()
	{
		static const DeclarativeEventDefinition value{
			BindingValueKind::Empty, DeclarativeEventRoutingStrategy::Direct };
		return value;
	}

	const DeclarativeEventDefinition& BenchmarkSeekEvent()
	{
		static const DeclarativeEventDefinition value{
			BindingValueKind::Empty, DeclarativeEventRoutingStrategy::Direct };
		return value;
	}

	const DeclarativeEventDefinition& BenchmarkSetSpeedRatioEvent()
	{
		static const DeclarativeEventDefinition value{
			BindingValueKind::Empty, DeclarativeEventRoutingStrategy::Direct };
		return value;
	}

	const DeclarativeEventDefinition& BenchmarkSkipToFillEvent()
	{
		static const DeclarativeEventDefinition value{
			BindingValueKind::Empty, DeclarativeEventRoutingStrategy::Direct };
		return value;
	}

	long long QueryCounter()
	{
		LARGE_INTEGER value{};
		if (!::QueryPerformanceCounter(&value))
			throw std::runtime_error("QueryPerformanceCounter failed.");
		return value.QuadPart;
	}

	unsigned long long QueryFrequency()
	{
		LARGE_INTEGER value{};
		if (!::QueryPerformanceFrequency(&value) || value.QuadPart <= 0)
			throw std::runtime_error("QueryPerformanceFrequency failed.");
		return static_cast<unsigned long long>(value.QuadPart);
	}

	double ElapsedMicroseconds(
		long long start,
		long long end,
		unsigned long long frequency)
	{
		return static_cast<double>(end - start) * 1'000'000.0
			/ static_cast<double>(frequency);
	}

	template<typename TAction>
	double MeasureMicroseconds(
		unsigned long long frequency,
		TAction&& action)
	{
		const auto start = QueryCounter();
		std::forward<TAction>(action)();
		return ElapsedMicroseconds(start, QueryCounter(), frequency);
	}

	double NearestRank(
		const std::vector<double>& sorted,
		double percentile)
	{
		if (sorted.empty()) return 0.0;
		const auto rank = static_cast<size_t>(std::ceil(
			percentile * static_cast<double>(sorted.size())));
		return sorted[(std::max)(size_t{ 1 }, rank) - 1];
	}

	Distribution Summarize(std::vector<double> samples)
	{
		Distribution result;
		result.Count = samples.size();
		if (samples.empty()) return result;
		std::sort(samples.begin(), samples.end());
		result.MinimumMicroseconds = samples.front();
		result.MeanMicroseconds = std::accumulate(
			samples.begin(), samples.end(), 0.0)
			/ static_cast<double>(samples.size());
		result.P50Microseconds = NearestRank(samples, 0.50);
		result.P95Microseconds = NearestRank(samples, 0.95);
		result.P99Microseconds = NearestRank(samples, 0.99);
		result.MaximumMicroseconds = samples.back();
		return result;
	}

	ProcessMemory QueryProcessMemory() noexcept
	{
		PROCESS_MEMORY_COUNTERS_EX counters{};
		counters.cb = sizeof(counters);
		ProcessMemory result;
		if (::GetProcessMemoryInfo(
			::GetCurrentProcess(),
			reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
			static_cast<DWORD>(sizeof(counters))))
		{
			result.WorkingSetBytes = counters.WorkingSetSize;
			result.PrivateUsageBytes = counters.PrivateUsage;
		}
		return result;
	}

	std::string QueryCpuName()
	{
		wchar_t buffer[256]{};
		DWORD bytes = sizeof(buffer);
		const auto status = ::RegGetValueW(
			HKEY_LOCAL_MACHINE,
			L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
			L"ProcessorNameString",
			RRF_RT_REG_SZ,
			nullptr,
			buffer,
			&bytes);
		if (status != ERROR_SUCCESS) return "unavailable";
		auto text = std::wstring(
			buffer, wcsnlen_s(buffer, std::size(buffer)));
		while (!text.empty() && std::iswspace(text.front())) text.erase(text.begin());
		while (!text.empty() && std::iswspace(text.back())) text.pop_back();
		return text.empty() ? "unavailable" : Convert::UnicodeToUtf8(text);
	}

	class BenchmarkProgram final
	{
	public:
		BenchmarkProgram(
			size_t animationCount,
			BenchmarkPropertyKind propertyKind,
			size_t rootCount = 1u,
			bool shareTarget = false,
			bool composeRoots = false,
			bool includeSkipToFill = false,
			std::span<const BenchmarkPropertyKind> propertyKinds = {})
			: _values{
				BindingValue(0.0f), BindingValue(100.0f),
				BindingValue(1.0), BindingValue(0.0) },
			  _propertyOperands(animationCount),
			  _objectPaths((propertyKinds.empty()
				  ? propertyKind == BenchmarkPropertyKind::TransformX
				  : std::find(propertyKinds.begin(), propertyKinds.end(),
					  BenchmarkPropertyKind::TransformX) != propertyKinds.end())
				  ? 1u : 0u),
			  _animations(animationCount),
			  _storyboards(rootCount),
			  _actions(rootCount * (includeSkipToFill ? 8u : 7u)),
			  _eventTriggers(includeSkipToFill ? 8u : 7u)
		{
			if (animationCount == 0
				|| animationCount >= CompiledInteractionInvalidIndex
				|| rootCount == 0 || rootCount > animationCount
				|| rootCount >= CompiledInteractionInvalidIndex
				|| (!propertyKinds.empty()
					&& propertyKinds.size() != animationCount))
				throw std::runtime_error("Invalid benchmark animation count.");
			if (propertyKind != BenchmarkPropertyKind::TransformX
				&& propertyKind != BenchmarkPropertyKind::Opacity)
				throw std::runtime_error("Invalid benchmark property kind.");
			if (!_objectPaths.empty())
			{
				auto& objectPath = _objectPaths.front();
				objectPath.Kind = CompiledStoryboardObjectPathKind::Transform;
				objectPath.Member = CompiledStoryboardObjectPathMember::TransformX;
				objectPath.ExpectedObjectKind = static_cast<uint8_t>(
					cui::drawing::TransformKind::Translate);
				objectPath.Index0 = 0u;
				objectPath.Identity = MakeCompiledInteractionNameToken(
					L"(Control.RenderTransform).(TranslateTransform.X)");
			}
			for (size_t index = 0; index < animationCount; ++index)
			{
				const auto animationProperty = propertyKinds.empty()
					? propertyKind : propertyKinds[index];
				if (animationProperty != BenchmarkPropertyKind::TransformX
					&& animationProperty != BenchmarkPropertyKind::Opacity)
					throw std::runtime_error("Invalid benchmark property kind.");
				_propertyOperands[index].TargetSlot = shareTarget
					? 1u : static_cast<uint32_t>(index + 1);
				_propertyOperands[index].Property = DependencyPropertyReference(
					animationProperty == BenchmarkPropertyKind::Opacity
						? Control::OpacityProperty()
						: Control::RenderTransformProperty());
				auto& animation = _animations[index];
				animation.Kind = DeclarativeAnimationKind::Double;
				animation.OperandIndex = static_cast<uint32_t>(index);
				if (animationProperty == BenchmarkPropertyKind::TransformX)
					animation.ObjectPathIndex = 0u;
				animation.FromValueIndex =
					animationProperty == BenchmarkPropertyKind::Opacity ? 2u : 0u;
				animation.ToValueIndex =
					animationProperty == BenchmarkPropertyKind::Opacity ? 3u : 1u;
				animation.DurationMilliseconds = 1000u;
				animation.RepeatBehavior = includeSkipToFill
					? DeclarativeRepeatBehaviorKind::Count
					: DeclarativeRepeatBehaviorKind::Forever;
			}
			size_t animationOffset = 0;
			for (size_t rootIndex = 0; rootIndex < rootCount; ++rootIndex)
			{
				const auto remaining = animationCount - animationOffset;
				const auto rootsRemaining = rootCount - rootIndex;
				const auto childCount = remaining / rootsRemaining;
				_storyboards[rootIndex].Animations = {
					static_cast<uint32_t>(animationOffset),
					static_cast<uint32_t>(childCount) };
				_actions[rootIndex] = {
					DeclarativeStoryboardActionKind::Begin,
					static_cast<uint32_t>(rootIndex) };
				if (composeRoots && rootIndex > 0u)
					_actions[rootIndex].Handoff =
						DeclarativeHandoffBehavior::Compose;
				_actions[rootCount + rootIndex] = {
					DeclarativeStoryboardActionKind::Pause,
					static_cast<uint32_t>(rootIndex) };
				_actions[rootCount * 2u + rootIndex] = {
					DeclarativeStoryboardActionKind::Resume,
					static_cast<uint32_t>(rootIndex) };
				_actions[rootCount * 3u + rootIndex] = {
					DeclarativeStoryboardActionKind::Stop,
					static_cast<uint32_t>(rootIndex) };
				_actions[rootCount * 4u + rootIndex] = {
					DeclarativeStoryboardActionKind::Remove,
					static_cast<uint32_t>(rootIndex) };
				_actions[rootCount * 5u + rootIndex] = {
					DeclarativeStoryboardActionKind::Seek,
					static_cast<uint32_t>(rootIndex),
					DeclarativeHandoffBehavior::SnapshotAndReplace,
					750u };
				_actions[rootCount * 6u + rootIndex] = {
					DeclarativeStoryboardActionKind::SetSpeedRatio,
					static_cast<uint32_t>(rootIndex),
					DeclarativeHandoffBehavior::SnapshotAndReplace,
					0u,
					2.0 };
				if (includeSkipToFill)
					_actions[rootCount * 7u + rootIndex] = {
						DeclarativeStoryboardActionKind::SkipToFill,
						static_cast<uint32_t>(rootIndex) };
				animationOffset += childCount;
			}
			_eventTriggers[0] = { &BenchmarkBeginEvent(), RoutedEventId::None,
				{ 0u, static_cast<uint32_t>(rootCount) } };
			_eventTriggers[1] = { &BenchmarkStopEvent(), RoutedEventId::None,
				{ static_cast<uint32_t>(rootCount * 3u),
					static_cast<uint32_t>(rootCount) } };
			_eventTriggers[2] = { &BenchmarkPauseEvent(), RoutedEventId::None,
				{ static_cast<uint32_t>(rootCount),
					static_cast<uint32_t>(rootCount) } };
			_eventTriggers[3] = { &BenchmarkResumeEvent(), RoutedEventId::None,
				{ static_cast<uint32_t>(rootCount * 2u),
					static_cast<uint32_t>(rootCount) } };
			_eventTriggers[4] = { &BenchmarkRemoveEvent(), RoutedEventId::None,
				{ static_cast<uint32_t>(rootCount * 4u),
					static_cast<uint32_t>(rootCount) } };
			_eventTriggers[5] = { &BenchmarkSeekEvent(), RoutedEventId::None,
				{ static_cast<uint32_t>(rootCount * 5u),
					static_cast<uint32_t>(rootCount) } };
			_eventTriggers[6] = { &BenchmarkSetSpeedRatioEvent(), RoutedEventId::None,
				{ static_cast<uint32_t>(rootCount * 6u),
					static_cast<uint32_t>(rootCount) } };
			if (includeSkipToFill)
				_eventTriggers[7] = {
					&BenchmarkSkipToFillEvent(), RoutedEventId::None,
					{ static_cast<uint32_t>(rootCount * 7u),
						static_cast<uint32_t>(rootCount) } };
		}

		CompiledInteractionProgramView View() const
		{
			CompiledInteractionProgramView result;
			result.Version = CompiledInteractionProgramViewVersion;
			result.TargetCount = static_cast<uint32_t>(
				_propertyOperands.size() + 1);
			result.PropertyOperands = _propertyOperands;
			result.ObjectPaths = _objectPaths;
			result.Animations = _animations;
			result.Storyboards = _storyboards;
			result.Actions = _actions;
			result.EventTriggers = _eventTriggers;
			return result;
		}

		std::span<const BindingValue> Values() const noexcept
		{
			return _values;
		}

	private:
		std::array<BindingValue, 4> _values;
		std::vector<CompiledInteractionPropertyOperand> _propertyOperands;
		std::vector<CompiledStoryboardObjectPathOp> _objectPaths;
		std::vector<CompiledInteractionAnimationOp> _animations;
		std::vector<CompiledInteractionStoryboardOp> _storyboards;
		std::vector<CompiledInteractionActionOp> _actions;
		std::vector<CompiledInteractionEventTriggerOp> _eventTriggers;
	};

	class BenchmarkNativeCompositionProbe final : public Control
	{
	public:
		PresentationSurfaceKind GetPresentationSurfaceKind()
			const noexcept override
		{
			return PresentationSurfaceKind::NativeComposition;
		}
	};

	class BenchmarkPixelNativeCompositionProbe final : public Control
	{
	public:
		explicit BenchmarkPixelNativeCompositionProbe(D2D1_COLOR_F color)
			: _color(color)
		{
		}

		~BenchmarkPixelNativeCompositionProbe() override
		{
			ReleaseVisual();
		}

		void RebindForTesting()
		{
			ReleaseVisual();
			InvalidateVisual();
		}

		void FailNextPrepareForTesting() noexcept
		{
			_failNextPrepare = true;
			InvalidateVisual();
		}

		size_t VisualGenerationForTesting() const noexcept
		{
			return _visualGeneration;
		}

	protected:
		PresentationSurfaceKind GetPresentationSurfaceKind()
			const noexcept override
		{
			return PresentationSurfaceKind::NativeComposition;
		}

		bool PrepareNativeCompositionVisual() override
		{
			if (_failNextPrepare)
			{
				_failNextPrepare = false;
				return false;
			}
			if (_visual) return UpdateVisualTransform();
			auto* window = GetPresentationWindow();
			auto* device = window ? cui::framework::WindowAccess::
				CompositionDevice(*window) : nullptr;
			if (!window || !device) return false;

			const float dpiScale = window->GetDpiScale();
			const auto actual = GetActualSizeDip();
			const UINT width = static_cast<UINT>((std::max)(1.0,
				std::ceil(static_cast<double>(actual.width) * dpiScale)));
			const UINT height = static_cast<UINT>((std::max)(1.0,
				std::ceil(static_cast<double>(actual.height) * dpiScale)));
			Microsoft::WRL::ComPtr<IDCompositionVisual> visual;
			Microsoft::WRL::ComPtr<IDCompositionSurface> surface;
			if (FAILED(device->CreateVisual(visual.GetAddressOf())) || !visual
				|| FAILED(device->CreateSurface(
					width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
					DXGI_ALPHA_MODE_PREMULTIPLIED, surface.GetAddressOf()))
				|| !surface) return false;

			Microsoft::WRL::ComPtr<IDXGISurface> drawSurface;
			POINT updateOffset{};
			HRESULT result = surface->BeginDraw(
				nullptr, __uuidof(IDXGISurface),
				reinterpret_cast<void**>(drawSurface.GetAddressOf()),
				&updateOffset);
			if (FAILED(result) || !drawSurface) return false;
			struct EndDrawScope final
			{
				IDCompositionSurface* Surface = nullptr;
				bool Active = true;
				~EndDrawScope()
				{
					if (Active && Surface) (void)Surface->EndDraw();
				}
			} endDraw{ surface.Get() };
			Microsoft::WRL::ComPtr<ID2D1Factory> factory;
			result = D2D1CreateFactory(
				D2D1_FACTORY_TYPE_SINGLE_THREADED,
				factory.GetAddressOf());
			Microsoft::WRL::ComPtr<ID2D1RenderTarget> renderTarget;
			if (SUCCEEDED(result))
			{
				const auto properties = D2D1::RenderTargetProperties(
					D2D1_RENDER_TARGET_TYPE_DEFAULT,
					D2D1::PixelFormat(
						DXGI_FORMAT_B8G8R8A8_UNORM,
						D2D1_ALPHA_MODE_PREMULTIPLIED),
					96.0f, 96.0f);
				result = factory->CreateDxgiSurfaceRenderTarget(
					drawSurface.Get(), &properties,
					renderTarget.GetAddressOf());
			}
			if (FAILED(result) || !renderTarget) return false;
			renderTarget->BeginDraw();
			renderTarget->Clear(_color);
			result = renderTarget->EndDraw();
			if (FAILED(result)) return false;
			result = surface->EndDraw();
			endDraw.Active = false;
			if (FAILED(result) || FAILED(visual->SetContent(surface.Get())))
				return false;

			_visual = std::move(visual);
			_surface = std::move(surface);
			if (!UpdateVisualTransform()
				|| !cui::framework::WindowAccess::RegisterCompositionVisual(
					*window, _visual.Get(), PresentationSceneContentLayer, 0))
			{
				_visual.Reset();
				_surface.Reset();
				return false;
			}
			++_visualGeneration;
			return true;
		}

		bool SupportsNativeCompositionVisualLease() const noexcept override
		{
			return true;
		}

		IDCompositionVisual*
		GetNativeCompositionVisual() const noexcept override
		{
			return _visual.Get();
		}

		void OnRender() override
		{
			if (!PrepareNativeCompositionVisual()) return;
			auto* window = GetPresentationWindow();
			if (!window) return;
			int order = 0;
			if (!TryGetPresentationOrderOverride(order))
				order = cui::framework::WindowAccess::PresentationOrder(
					*window, this);
			cui::framework::WindowAccess::UpdateCompositionVisualOrder(
				*window, _visual.Get(), PresentationSceneContentLayer,
				order);
			(void)cui::framework::WindowAccess::CommitComposition(*window);
		}

		void NotifyDeviceResourcesInvalidated() noexcept override
		{
			ReleaseVisual();
			Control::NotifyDeviceResourcesInvalidated();
		}

	private:
		bool UpdateVisualTransform()
		{
			auto* window = GetPresentationWindow();
			if (!window || !_visual) return false;
			const auto transform =
				cui::dcomp_detail::DipTransformToPhysicalPixels(
					GetLocalToRenderTransform(), window->GetDpiScale(),
					static_cast<float>(cui::framework::WindowAccess::
						TitleBarHeightPixelsForTesting(*window)));
			return SUCCEEDED(_visual->SetTransform(transform));
		}

		void ReleaseVisual() noexcept
		{
			if (_visual)
				if (auto* window = GetPresentationWindow())
					cui::framework::WindowAccess::UnregisterCompositionVisual(
						*window, _visual.Get());
			_surface.Reset();
			_visual.Reset();
		}

		D2D1_COLOR_F _color{};
		Microsoft::WRL::ComPtr<IDCompositionVisual> _visual;
		Microsoft::WRL::ComPtr<IDCompositionSurface> _surface;
		size_t _visualGeneration = 0;
		bool _failNextPrepare = false;
	};

	class BenchmarkPresentationMutationProbe final : public Canvas
	{
	public:
		void InvalidateContentDuringNextPrepare() noexcept
		{
			_invalidateContent = true;
		}

	protected:
		void PreparePresentation() override
		{
			Canvas::PreparePresentation();
			if (!_invalidateContent) return;
			_invalidateContent = false;
			Background = D2D1_COLOR_F{ 0.8f, 0.2f, 0.1f, 1.0f };
		}

	private:
		bool _invalidateContent = false;
	};

	class BenchmarkScene final
	{
	public:
		BenchmarkScene(
			size_t animationCount,
			size_t unrelatedVisualCount,
			BenchmarkPropertyKind propertyKind,
			size_t rootCount = 1u,
			bool shareTarget = false,
			bool composeRoots = false,
			bool includeSkipToFill = false,
			std::span<const BenchmarkPropertyKind> propertyKinds = {})
			: _animationCount(animationCount),
			  _rootCount(rootCount),
			  _shareTarget(shareTarget),
			  _composeRoots(composeRoots),
			  _propertyKind(propertyKind),
			  _propertyKinds(propertyKinds.empty()
				  ? std::vector<BenchmarkPropertyKind>(animationCount, propertyKind)
				  : std::vector<BenchmarkPropertyKind>(
					  propertyKinds.begin(), propertyKinds.end())),
			  _program(animationCount, propertyKind, rootCount,
				  shareTarget, composeRoots, includeSkipToFill, propertyKinds)
		{
			auto root = std::make_unique<Canvas>();
			_root = static_cast<Canvas*>(_window.AddOwned(std::move(root)));
			if (!_root) throw std::runtime_error("Benchmark root creation failed.");

			auto host = std::make_unique<Canvas>();
			_host = static_cast<Canvas*>(_root->AddOwned(std::move(host)));
			if (!_host) throw std::runtime_error("Benchmark host creation failed.");
			_targetSlots.reserve(animationCount + 1);
			_targetSlots.push_back(_host);
			_targets.reserve(animationCount);
			for (size_t index = 0; index < animationCount; ++index)
			{
				auto target =
					std::make_unique<BenchmarkPresentationMutationProbe>();
				auto* targetPointer = static_cast<Canvas*>(
					_host->AddOwned(std::move(target)));
				if (!targetPointer)
					throw std::runtime_error(
						"Benchmark animation target creation failed.");
				cui::framework::XamlAccess::SetTemplatedParent(
					*targetPointer, _host);
				const auto partName = L"AnimationBenchmarkTarget"
					+ std::to_wstring(index);
				if (!cui::framework::TemplateAccess::RegisterTemplatePart(
					*_host, MakeTemplatePartToken(partName), targetPointer))
					throw std::runtime_error(
						"Benchmark template-part registration failed.");
				cui::drawing::Transform transform;
				cui::drawing::TransformOperation translation;
				translation.Kind = cui::drawing::TransformKind::Translate;
				transform.Operations.push_back(translation);
				targetPointer->SetRenderTransform(transform);
				targetPointer->Arrange(cui::core::Rect{
					static_cast<float>(index * 14u), 0.0f, 12.0f, 12.0f });
				_targets.push_back(targetPointer);
				_targetSlots.push_back(targetPointer);
			}
			for (size_t index = 0; index < unrelatedVisualCount; ++index)
				if (!_root->AddOwned(std::make_unique<Control>()))
					throw std::runtime_error(
						"Benchmark unrelated visual creation failed.");

			std::wstring error;
			const auto view = _program.View();
			if (!cui::framework::TemplateAccess::InstallCompiledInteractions(
				*_host, view, _program.Values(), _targetSlots, &error))
				throw std::runtime_error("Benchmark interaction install failed: "
					+ Convert::UnicodeToUtf8(error));
			(void)cui::framework::PresentationAccess::
				ExchangeVisualStateAnimationClockOverrideForTesting(
					*_host, BenchmarkClockOrigin);
		}

		~BenchmarkScene()
		{
			ReleaseSceneLayerPixelReadbackLeaseForTesting();
		}

		bool TryAcquireSceneLayerPixelReadbackLeaseForTesting() noexcept
		{
			if (_sceneLayerPixelReadbackLease) return true;
			_sceneLayerPixelReadbackLease = cui::framework::WindowAccess::
				AcquirePresentationSceneLayerPixelReadbackLeaseForTesting(_window);
			return _sceneLayerPixelReadbackLease;
		}

		void AcquireSceneLayerPixelReadbackLeaseForTesting()
		{
			if (!TryAcquireSceneLayerPixelReadbackLeaseForTesting())
				throw std::runtime_error(
					"Scene pixel-readback lease must precede the first surface draw.");
		}

		void ReleaseSceneLayerPixelReadbackLeaseForTesting() noexcept
		{
			if (!_sceneLayerPixelReadbackLease) return;
			cui::framework::WindowAccess::
				ReleasePresentationSceneLayerPixelReadbackLeaseForTesting(_window);
			_sceneLayerPixelReadbackLease = false;
		}

		void Begin()
		{
			if (!_host->RaiseDeclarativeEvent(BenchmarkBeginEvent()))
				throw std::runtime_error("Benchmark Begin event was rejected.");
			const bool mixedProperties = std::adjacent_find(
				_propertyKinds.begin(), _propertyKinds.end(),
				std::not_equal_to<>{}) != _propertyKinds.end();
			const auto replacesExact =
				_shareTarget && !_composeRoots && !mixedProperties;
			const auto retainedAnimationCount = replacesExact ? 1u : _animationCount;
			const auto retainedRootCount = replacesExact ? 1u : _rootCount;
			RequireLeafCount(retainedAnimationCount, "Begin");
			RequireRootClockCount(retainedRootCount, "Begin");
			RequireClockNodeCount(
				retainedAnimationCount + retainedRootCount, "Begin");
			RequireLayerStackCount(_shareTarget
				? (mixedProperties ? 2u : 1u) : _animationCount, "Begin");
			RequireLayerCount(retainedAnimationCount, "Begin");
			RequireLayerMaxDepth(_shareTarget && !mixedProperties
				? retainedAnimationCount : (retainedAnimationCount == 0u ? 0u : 1u),
				"Begin");
			size_t childCount = 0;
			for (size_t rootIndex = 0; rootIndex < retainedRootCount; ++rootIndex)
				childCount += RootClockChildCount(rootIndex);
			if (childCount != retainedAnimationCount)
				throw std::runtime_error(
					"Benchmark Begin produced an incomplete root child range.");
			RequireRegisteredControlCount(1u, "Begin");
			if (!ClockIdentityValid())
				throw std::runtime_error(
					"Benchmark Begin produced invalid clock identity ownership.");
		}

		void Restart()
		{
			if (_rootCount != 1u)
				throw std::runtime_error(
					"Single-root restart token check requires exactly one root.");
			const auto previous = SingleRootClockId();
			const auto previousNode = SingleRootClockNodeToken();
			Begin();
			const auto current = SingleRootClockId();
			const auto currentNode = SingleRootClockNodeToken();
			if (previous == 0 || current == 0 || previous == current)
				throw std::runtime_error(
					"Benchmark restart did not replace the root ClockId (before="
					+ std::to_string(previous) + ", after="
					+ std::to_string(current) + ").");
			if (previousNode == 0 || currentNode == 0
				|| previousNode == currentNode)
				throw std::runtime_error(
					"Benchmark restart did not replace the generation-safe root "
					"ClockNode token (before=" + std::to_string(previousNode)
					+ ", after=" + std::to_string(currentNode) + ").");
		}

		void ReplaceMany(size_t count)
		{
			for (size_t index = 0; index < count; ++index)
				if (!_host->RaiseDeclarativeEvent(BenchmarkBeginEvent()))
					throw std::runtime_error(
						"Benchmark repeated Begin event was rejected.");
			const auto replacesExact = _shareTarget && !_composeRoots;
			const auto retainedAnimationCount = replacesExact ? 1u : _animationCount;
			const auto retainedRootCount = replacesExact ? 1u : _rootCount;
			RequireLeafCount(retainedAnimationCount, "repeated Begin");
			RequireRootClockCount(retainedRootCount, "repeated Begin");
			RequireClockNodeCount(
				retainedAnimationCount + retainedRootCount, "repeated Begin");
			RequireLayerStackCount(
				_shareTarget ? 1u : _animationCount, "repeated Begin");
			RequireLayerCount(retainedAnimationCount, "repeated Begin");
			if (!ClockIdentityValid())
				throw std::runtime_error(
					"Benchmark repeated Begin degraded clock/layer identity.");
		}

		void Stop()
		{
			Remove();
		}

		void StopRetained()
		{
			if (!_host->RaiseDeclarativeEvent(BenchmarkStopEvent()))
				throw std::runtime_error("Benchmark Stop event was rejected.");
			CommitPendingControl(_lastTick + 1u, "Stop");
			const auto replacesExact = _shareTarget && !_composeRoots;
			const auto retainedAnimationCount = replacesExact ? 1u : _animationCount;
			const auto retainedRootCount = replacesExact ? 1u : _rootCount;
			RequireLeafCount(retainedAnimationCount, "retained Stop");
			RequireRootClockCount(retainedRootCount, "retained Stop");
			RequireClockNodeCount(
				retainedAnimationCount + retainedRootCount, "retained Stop");
			RequireLayerStackCount(
				_shareTarget ? 1u : _animationCount, "retained Stop");
			RequireLayerCount(retainedAnimationCount, "retained Stop");
			if (_host->HasActiveVisualStateAnimations()
				|| AnimationSlotsCleared())
				throw std::runtime_error(
					"Benchmark Stop did not retain an inactive Animation source.");
			if (!ClockIdentityValid())
				throw std::runtime_error(
					"Benchmark Stop left invalid clock identity ownership.");
		}

		void Remove()
		{
			if (!_host->RaiseDeclarativeEvent(BenchmarkRemoveEvent()))
				throw std::runtime_error("Benchmark Remove event was rejected.");
			CommitPendingControl(_lastTick + 1u, "Remove");
			RequireLeafCount(0u, "Remove");
			RequireRootClockCount(0u, "Remove");
			RequireClockNodeCount(0u, "Remove");
			RequireLayerStackCount(0u, "Remove");
			RequireLayerCount(0u, "Remove");
			RequireLayerMaxDepth(0u, "Remove");
			RequireRegisteredControlCount(0u, "Remove");
			if (!ClockIdentityValid())
				throw std::runtime_error(
					"Benchmark Remove left invalid clock identity ownership.");
		}

		void Pause(unsigned long long nowMilliseconds)
		{
			(void)cui::framework::PresentationAccess::
				ExchangeVisualStateAnimationClockOverrideForTesting(
					*_host, nowMilliseconds);
			if (!_host->RaiseDeclarativeEvent(BenchmarkPauseEvent()))
				throw std::runtime_error("Benchmark Pause event was rejected.");
			CommitPendingControl(nowMilliseconds, "Pause");
			if (_host->HasActiveVisualStateAnimations()
				|| !ClockIdentityValid())
				throw std::runtime_error(
					"Benchmark Pause did not pause every root range.");
		}

		void Resume(unsigned long long nowMilliseconds)
		{
			(void)cui::framework::PresentationAccess::
				ExchangeVisualStateAnimationClockOverrideForTesting(
					*_host, nowMilliseconds);
			if (!_host->RaiseDeclarativeEvent(BenchmarkResumeEvent()))
				throw std::runtime_error("Benchmark Resume event was rejected.");
			CommitPendingControl(nowMilliseconds, "Resume");
			if (!_host->HasActiveVisualStateAnimations()
				|| !ClockIdentityValid())
				throw std::runtime_error(
					"Benchmark Resume did not resume every root range.");
		}

		void Seek(unsigned long long nowMilliseconds)
		{
			const auto before = cui::framework::PresentationAccess::
				QuerySingleVisualStateAnimationRootForTesting(*_host);
			if (!before || !_host->RaiseDeclarativeEvent(BenchmarkSeekEvent()))
				throw std::runtime_error("Benchmark Seek event was rejected.");
			const auto pending = cui::framework::PresentationAccess::
				QuerySingleVisualStateAnimationRootForTesting(*_host);
			if (pending != before)
				throw std::runtime_error(
					"Benchmark ordinary Seek mutated the Clock before its tick.");
			CommitPendingControl(nowMilliseconds, "Seek");
			const auto committed = cui::framework::PresentationAccess::
				QuerySingleVisualStateAnimationRootForTesting(*_host);
			if (!committed
				|| committed->CurrentTimeMilliseconds.value_or(0ull) != 750ull
				|| !ClockIdentityValid())
				throw std::runtime_error(
					"Benchmark compiled Seek did not commit at 750ms.");
		}

		void SetSpeedRatio(unsigned long long nowMilliseconds)
		{
			const auto before = cui::framework::PresentationAccess::
				QuerySingleVisualStateAnimationRootForTesting(*_host);
			if (!before || !_host->RaiseDeclarativeEvent(
				BenchmarkSetSpeedRatioEvent()))
				throw std::runtime_error(
					"Benchmark SetSpeedRatio event was rejected.");
			const auto pending = cui::framework::PresentationAccess::
				QuerySingleVisualStateAnimationRootForTesting(*_host);
			if (pending != before)
				throw std::runtime_error(
					"Benchmark SetSpeedRatio mutated the Clock before its tick.");
			CommitPendingControl(nowMilliseconds, "SetSpeedRatio");
			const auto committed = cui::framework::PresentationAccess::
				QuerySingleVisualStateAnimationRootForTesting(*_host);
			if (!committed
				|| committed->GlobalSpeed.value_or(-1.0) != 2.0
				|| committed->CurrentTimeMilliseconds
					!= before->CurrentTimeMilliseconds
				|| !ClockIdentityValid())
				throw std::runtime_error(
					"Benchmark compiled SetSpeedRatio did not preserve the time anchor.");
		}

		void SkipToFill(unsigned long long nowMilliseconds)
		{
			const auto before = cui::framework::PresentationAccess::
				QuerySingleVisualStateAnimationRootForTesting(*_host);
			if (!before || !_host->RaiseDeclarativeEvent(BenchmarkSkipToFillEvent()))
				throw std::runtime_error(
					"Benchmark SkipToFill event was rejected.");
			const auto pending = cui::framework::PresentationAccess::
				QuerySingleVisualStateAnimationRootForTesting(*_host);
			if (pending != before)
				throw std::runtime_error(
					"Benchmark SkipToFill mutated the Clock before its tick.");
			CommitPendingControl(nowMilliseconds, "SkipToFill");
			const auto committed = cui::framework::PresentationAccess::
				QuerySingleVisualStateAnimationRootForTesting(*_host);
			if (!committed
				|| committed->State != DeclarativeClockState::Filling
				|| committed->CurrentTimeMilliseconds.value_or(0ull) != 1000ull
				|| committed->GlobalSpeed.value_or(-1.0) != 0.0
				|| !ClockIdentityValid())
				throw std::runtime_error(
					"Benchmark compiled SkipToFill did not enter fill.");
		}

		void Advance(unsigned long long nowMilliseconds)
		{
			(void)cui::framework::PresentationAccess::
				ExchangeVisualStateAnimationClockOverrideForTesting(
					*_host, nowMilliseconds);
			if (!cui::framework::PresentationAccess::
				AdvanceVisualStateAnimations(*_host, nowMilliseconds)
				|| cui::framework::PresentationAccess::
					VisualStateAnimationAdvanceFailedForTesting(*_host))
				throw std::runtime_error("Benchmark animation advance failed.");
			_lastTick = nowMilliseconds;
		}

		bool AdvanceAllowingOwnerDestruction(unsigned long long nowMilliseconds)
		{
			auto* host = _host;
			if (!host) return false;
			(void)cui::framework::PresentationAccess::
				ExchangeVisualStateAnimationClockOverrideForTesting(
					*host, nowMilliseconds);
			const bool advanced = cui::framework::PresentationAccess::
				AdvanceVisualStateAnimations(*host, nowMilliseconds);
			_host = nullptr;
			_lastTick = nowMilliseconds;
			return advanced;
		}

		void TickWindow(
			unsigned long long nowMilliseconds = BenchmarkClockOrigin + 16u)
		{
			(void)cui::framework::PresentationAccess::
				ExchangeVisualStateAnimationClockOverrideForTesting(
					*_host, nowMilliseconds);
			cui::framework::WindowAccess::
				TickPresentationAnimationsForTesting(
					_window, nowMilliseconds);
			if (cui::framework::PresentationAccess::
				VisualStateAnimationAdvanceFailedForTesting(*_host))
				throw std::runtime_error("Benchmark Window tick failed.");
			if (RegistryDegraded())
				throw std::runtime_error(
					"Benchmark Window animation registry degraded.");
			_lastTick = nowMilliseconds;
		}

		void TickRegisteredWindow(unsigned long long nowMilliseconds)
		{
			(void)cui::framework::PresentationAccess::
				ExchangeVisualStateAnimationClockOverrideForTesting(
					*_host, nowMilliseconds);
			cui::framework::WindowAccess::
				TickRegisteredDeclarativeAnimationsForTesting(
					_window, nowMilliseconds);
			if (cui::framework::PresentationAccess::
				VisualStateAnimationAdvanceFailedForTesting(*_host))
				throw std::runtime_error(
					"Benchmark registered Window tick failed.");
			if (RegisteredControlCount() != 1u)
				throw std::runtime_error(
					"Benchmark registered Window tick lost its active Control.");
			_lastTick = nowMilliseconds;
		}

		size_t ActiveLeafCount() const noexcept
		{
			return cui::framework::PresentationAccess::
				VisualStateAnimationLeafCountForTesting(*_host);
		}

		size_t RootClockCount() const noexcept
		{
			return cui::framework::PresentationAccess::
				VisualStateAnimationRootClockCountForTesting(*_host);
		}

		size_t ClockNodeCount() const noexcept
		{
			return cui::framework::PresentationAccess::
				VisualStateAnimationClockNodeCountForTesting(*_host);
		}

		size_t LayerStackCount() const noexcept
		{
			return cui::framework::PresentationAccess::
				VisualStateAnimationLayerStackCountForTesting(*_host);
		}

		size_t LayerCount() const noexcept
		{
			return cui::framework::PresentationAccess::
				VisualStateAnimationLayerCountForTesting(*_host);
		}

		size_t LayerMaxDepth() const noexcept
		{
			return cui::framework::PresentationAccess::
				VisualStateAnimationLayerMaxDepthForTesting(*_host);
		}

		size_t RootClockChildCount(size_t rootIndex) const noexcept
		{
			return cui::framework::PresentationAccess::
				VisualStateAnimationRootClockChildCountForTesting(
					*_host, rootIndex);
		}

		uint64_t SingleRootClockId() const noexcept
		{
			return cui::framework::PresentationAccess::
				VisualStateAnimationSingleRootClockIdForTesting(*_host);
		}

		uint64_t SingleRootClockNodeToken() const noexcept
		{
			return cui::framework::PresentationAccess::
				VisualStateAnimationSingleRootClockNodeTokenForTesting(*_host);
		}

		bool ClockIdentityValid() const noexcept
		{
			return cui::framework::PresentationAccess::
				VisualStateAnimationClockIdentityValidForTesting(*_host);
		}

		size_t RegisteredControlCount()
		{
			return cui::framework::WindowAccess::
				RegisteredDeclarativeAnimationControlCountForTesting(_window);
		}

		bool RegistryDegraded() const noexcept
		{
			return cui::framework::WindowAccess::
				AnimationRegistryDegradedForTesting(_window);
		}

		UINT AnimationTimerInterval() const noexcept
		{
			return cui::framework::WindowAccess::
				AnimationTimerIntervalForTesting(_window);
		}

		bool AnimationFrameSchedulerRunning() const noexcept
		{
			return cui::framework::WindowAccess::
				AnimationFrameSchedulerRunningForTesting(_window);
		}

		bool AnimationUsesLegacyTimer() const noexcept
		{
			return cui::framework::WindowAccess::
				AnimationUsesLegacyTimerForTesting(_window);
		}

		void UseRealtimeClockForTesting()
		{
			const auto now = ::GetTickCount64();
			(void)cui::framework::PresentationAccess::
				ExchangeVisualStateAnimationClockOverrideForTesting(
					*_host, now);
			_lastTick = now;
		}

		void ShowOffscreenWithoutActivationForTesting(
			UINT initialDpi = 0u,
			bool arrangeDefaultTargets = true)
		{
			const auto handle = _window.Handle;
			if (!handle || !::IsWindow(handle))
				throw std::runtime_error(
					"Benchmark Window has no native handle.");
			for (size_t index = 0;
				arrangeDefaultTargets && index < _targets.size(); ++index)
			{
				auto* target = _targets[index];
				if (!target) continue;
				target->Width = 12.0f;
				target->Height = 12.0f;
				target->Background = D2D1_COLOR_F{
					0.15f, 0.45f, 0.85f, 1.0f };
				Canvas::SetLeft(*target,
					static_cast<float>((index % 20u) * 14u));
				Canvas::SetTop(*target,
					static_cast<float>((index / 20u) * 14u));
			}
			cui::framework::WindowAccess::
				EnsureInitialDpiForTesting(_window);
			if (initialDpi != 0u)
				SetWindowDpiForTesting(initialDpi);
			(void)::SetWindowPos(handle, nullptr, -32'000, -32'000, 360, 240,
				SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
			_window.UpdateLayout();
			(void)::ShowWindow(handle, SW_SHOWNOACTIVATE);
			(void)::InvalidateRect(handle, nullptr, FALSE);
			(void)::UpdateWindow(handle);
		}

		void AddNativeCompositionBoundaryForTesting()
		{
			if (!_root || !_root->AddOwned(
				std::make_unique<BenchmarkNativeCompositionProbe>()))
				throw std::runtime_error(
					"Benchmark native composition boundary creation failed.");
		}

		void AddTargetNativeCompositionBoundaryForTesting(size_t index)
		{
			if (index >= _targets.size() || !_targets[index]
				|| !_targets[index]->AddOwned(
					std::make_unique<BenchmarkNativeCompositionProbe>()))
				throw std::runtime_error(
					"Benchmark target native composition boundary creation failed.");
		}

		BenchmarkPixelNativeCompositionProbe*
		AddTargetPixelNativeCompositionChildForTesting(
			size_t index,
			float width,
			float height,
			float left,
			float top,
			D2D1_COLOR_F color)
		{
			if (index >= _targets.size() || !_targets[index]
				|| !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(left) || !std::isfinite(top)
				|| width <= 0.0f || height <= 0.0f)
				throw std::runtime_error(
					"Benchmark pixel native composition child is invalid.");
			auto child =
				std::make_unique<BenchmarkPixelNativeCompositionProbe>(color);
			auto* result = child.get();
			if (_targets[index]->AddOwned(std::move(child)) != result)
				throw std::runtime_error(
					"Benchmark pixel native composition child creation failed.");
			result->Width = width;
			result->Height = height;
			Canvas::SetLeft(*result, left);
			Canvas::SetTop(*result, top);
			_window.UpdateLayout();
			return result;
		}

		WebBrowser* AddTargetWebBrowserChildForTesting(
			size_t index,
			float width,
			float height,
			float left,
			float top)
		{
			if (index >= _targets.size() || !_targets[index]
				|| !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(left) || !std::isfinite(top)
				|| width <= 0.0f || height <= 0.0f)
				throw std::runtime_error(
					"Benchmark WebBrowser child is invalid.");
			auto child = std::make_unique<WebBrowser>();
			auto* result = child.get();
			if (_targets[index]->AddOwned(std::move(child)) != result)
				throw std::runtime_error(
					"Benchmark WebBrowser child creation failed.");
			result->Width = width;
			result->Height = height;
			Canvas::SetLeft(*result, left);
			Canvas::SetTop(*result, top);
			_window.UpdateLayout();
			return result;
		}

		void ForcePresentationUpdateForTesting()
		{
			const auto handle = _window.Handle;
			if (!handle || !::IsWindow(handle))
				throw std::runtime_error(
					"Benchmark Window has no native handle.");
			(void)::InvalidateRect(handle, nullptr, FALSE);
			(void)::UpdateWindow(handle);
		}

		void FlushPendingPresentationUpdateForTesting()
		{
			const auto handle = _window.Handle;
			if (!handle || !::IsWindow(handle))
				throw std::runtime_error(
					"Benchmark Window has no native handle.");
			(void)::UpdateWindow(handle);
		}

		void InvalidateTargetContentDuringNextPrepareForTesting(size_t index)
		{
			if (index >= _targets.size() || !_targets[index])
				throw std::runtime_error("Benchmark target index is invalid.");
			auto* probe = dynamic_cast<BenchmarkPresentationMutationProbe*>(
				_targets[index]);
			if (!probe)
				throw std::runtime_error(
					"Benchmark target does not expose the preparation probe.");
			probe->InvalidateContentDuringNextPrepare();
		}

		void SetTargetBackgroundForTesting(
			size_t index,
			D2D1_COLOR_F color)
		{
			if (index >= _targets.size() || !_targets[index])
				throw std::runtime_error("Benchmark target index is invalid.");
			_targets[index]->Background = color;
		}

		void SetHostBackgroundForTesting(D2D1_COLOR_F color)
		{
			if (!_host) throw std::runtime_error("Benchmark host is unavailable.");
			_host->Background = color;
		}

		void SetRootBackgroundForTesting(D2D1_COLOR_F color)
		{
			if (!_root) throw std::runtime_error("Benchmark root is unavailable.");
			_root->Background = color;
		}

		double TargetOpacityForTesting(size_t index) const
		{
			if (index >= _targets.size() || !_targets[index])
				throw std::runtime_error("Benchmark target index is invalid.");
			return _targets[index]->GetOpacity();
		}

		void SetTargetOpacityForTesting(size_t index, double value)
		{
			if (index >= _targets.size() || !_targets[index])
				throw std::runtime_error("Benchmark target index is invalid.");
			_targets[index]->SetOpacity(value);
		}

		void SetTargetClipToBoundsForTesting(
			size_t index,
			bool value)
		{
			if (index >= _targets.size() || !_targets[index])
				throw std::runtime_error("Benchmark target index is invalid.");
			_targets[index]->ClipToBounds = value;
		}

		void SetTargetCanvasPositionForTesting(
			size_t index,
			float left,
			float top)
		{
			if (index >= _targets.size() || !_targets[index])
				throw std::runtime_error("Benchmark target index is invalid.");
			Canvas::SetLeft(*_targets[index], left);
			Canvas::SetTop(*_targets[index], top);
		}

		void NestTargetForTesting(
			size_t childIndex,
			size_t parentIndex)
		{
			if (childIndex >= _targets.size()
				|| parentIndex >= _targets.size()
				|| childIndex == parentIndex
				|| !_targets[childIndex] || !_targets[parentIndex])
				throw std::runtime_error(
					"Benchmark nested target indices are invalid.");
			auto* previousParent = dynamic_cast<Canvas*>(
				_targets[childIndex]->GetVisualParent());
			auto* newParent = dynamic_cast<Canvas*>(_targets[parentIndex]);
			if (!previousParent || !newParent)
				throw std::runtime_error(
					"Benchmark nested target parents must be Canvas controls.");
			auto detached = previousParent->DetachVisualChild(
				_targets[childIndex]);
			if (!detached || detached.get() != _targets[childIndex]
				|| newParent->AddOwned(std::move(detached))
					!= _targets[childIndex])
				throw std::runtime_error(
					"Benchmark nested target reparent failed.");
		}

		void WrapTargetInNeutralParentForTesting(size_t index)
		{
			if (index >= _targets.size() || !_targets[index] || !_host)
				throw std::runtime_error(
					"Benchmark wrapped target index is invalid.");
			auto* previousParent = dynamic_cast<Canvas*>(
				_targets[index]->GetVisualParent());
			if (previousParent != _host)
				throw std::runtime_error(
					"Benchmark neutral wrapper requires a direct host child.");
			auto detached = previousParent->DetachVisualChild(_targets[index]);
			if (!detached || detached.get() != _targets[index])
				throw std::runtime_error(
					"Benchmark neutral wrapper detach failed.");
			auto wrapper = std::make_unique<Canvas>();
			auto* wrapperPointer = static_cast<Canvas*>(
				_host->AddOwned(std::move(wrapper)));
			if (!wrapperPointer
				|| wrapperPointer->AddOwned(std::move(detached))
					!= _targets[index])
				throw std::runtime_error(
					"Benchmark neutral wrapper attach failed.");
			wrapperPointer->Width = 360.0f;
			wrapperPointer->Height = 240.0f;
			_window.UpdateLayout();
		}

		Control* AddTargetChildRectangleForTesting(
			size_t parentIndex,
			float width,
			float height,
			float left,
			float top,
			D2D1_COLOR_F color)
		{
			if (parentIndex >= _targets.size() || !_targets[parentIndex]
				|| !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(left) || !std::isfinite(top)
				|| width <= 0.0f || height <= 0.0f)
				throw std::runtime_error(
					"Benchmark trailing target child is invalid.");
			auto* parent = dynamic_cast<Canvas*>(_targets[parentIndex]);
			if (!parent)
				throw std::runtime_error(
					"Benchmark trailing target parent must be a Canvas.");
			auto child = std::make_unique<Canvas>();
			auto* result = static_cast<Canvas*>(
				parent->AddOwned(std::move(child)));
			if (!result)
				throw std::runtime_error(
					"Benchmark trailing target child creation failed.");
			result->Width = width;
			result->Height = height;
			result->Background = color;
			Canvas::SetLeft(*result, left);
			Canvas::SetTop(*result, top);
			_window.UpdateLayout();
			return result;
		}

		Control* AddHostChildRectangleForTesting(
			float width,
			float height,
			float left,
			float top,
			D2D1_COLOR_F color)
		{
			if (!_host || !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(left) || !std::isfinite(top)
				|| width <= 0.0f || height <= 0.0f)
				throw std::runtime_error(
					"Benchmark host child rectangle is invalid.");
			auto child = std::make_unique<Canvas>();
			auto* result = static_cast<Canvas*>(
				_host->AddOwned(std::move(child)));
			if (!result)
				throw std::runtime_error(
					"Benchmark host child rectangle creation failed.");
			result->Width = width;
			result->Height = height;
			result->Background = color;
			Canvas::SetLeft(*result, left);
			Canvas::SetTop(*result, top);
			_window.UpdateLayout();
			return result;
		}

		void ConfigureTargetCompositeTransformForTesting(size_t index)
		{
			if (index >= _targets.size() || !_targets[index])
				throw std::runtime_error("Benchmark target index is invalid.");
			cui::drawing::Transform transform;
			cui::drawing::TransformOperation translation;
			translation.Kind = cui::drawing::TransformKind::Translate;
			transform.Operations.push_back(translation);
			cui::drawing::TransformOperation scale;
			scale.Kind = cui::drawing::TransformKind::Scale;
			scale.ScaleX = 1.25f;
			scale.ScaleY = 0.75f;
			transform.Operations.push_back(scale);
			cui::drawing::TransformOperation rotation;
			rotation.Kind = cui::drawing::TransformKind::Rotate;
			rotation.Angle = 30.0f;
			transform.Operations.push_back(rotation);
			_targets[index]->SetRenderTransform(transform);
			_targets[index]->SetRenderTransformOrigin(
				D2D1::Point2F(0.5f, 0.5f));
		}

		void ConfigureHostClipForTesting(float width, float height)
		{
			if (!_host || !std::isfinite(width) || !std::isfinite(height)
				|| width <= 0.0f || height <= 0.0f)
				throw std::runtime_error("Benchmark host clip is invalid.");
			_host->Width = width;
			_host->Height = height;
			_host->ClipToBounds = true;
		}

		void ConfigureHostRotatedClipForTesting(
			float width,
			float height,
			float left,
			float top,
			float angle)
		{
			ConfigureHostClipForTesting(width, height);
			if (!std::isfinite(left) || !std::isfinite(top)
				|| !std::isfinite(angle))
				throw std::runtime_error("Benchmark rotated host clip is invalid.");
			Canvas::SetLeft(*_host, left);
			Canvas::SetTop(*_host, top);
			cui::drawing::Transform transform;
			cui::drawing::TransformOperation rotation;
			rotation.Kind = cui::drawing::TransformKind::Rotate;
			rotation.Angle = angle;
			transform.Operations.push_back(rotation);
			_host->SetRenderTransform(transform);
			_host->SetRenderTransformOrigin(D2D1::Point2F(0.5f, 0.5f));
		}

		void ConfigureTargetRotatedClipForTesting(
			size_t index,
			float width,
			float height,
			float left,
			float top,
			float angle)
		{
			if (index >= _targets.size() || !_targets[index]
				|| !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(left) || !std::isfinite(top)
				|| !std::isfinite(angle)
				|| width <= 0.0f || height <= 0.0f)
				throw std::runtime_error(
					"Benchmark rotated target clip is invalid.");
			auto* target = _targets[index];
			target->Width = width;
			target->Height = height;
			target->ClipToBounds = true;
			Canvas::SetLeft(*target, left);
			Canvas::SetTop(*target, top);
			cui::drawing::Transform transform;
			cui::drawing::TransformOperation rotation;
			rotation.Kind = cui::drawing::TransformKind::Rotate;
			rotation.Angle = angle;
			transform.Operations.push_back(rotation);
			target->SetRenderTransform(transform);
			target->SetRenderTransformOrigin(D2D1::Point2F(0.5f, 0.5f));
		}

		void ConfigureHostRoundedGeometryClipForTesting(
			float width,
			float height,
			float left,
			float top,
			float angle,
			float radiusX,
			float radiusY)
		{
			if (!_host || !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(left) || !std::isfinite(top)
				|| !std::isfinite(angle) || !std::isfinite(radiusX)
				|| !std::isfinite(radiusY) || width <= 0.0f || height <= 0.0f
				|| radiusX < 0.0f || radiusY < 0.0f)
				throw std::runtime_error(
					"Benchmark rounded geometry clip is invalid.");
			_host->Width = width;
			_host->Height = height;
			cui::drawing::Geometry geometry;
			geometry.Kind = cui::drawing::GeometryKind::Rectangle;
			geometry.Rect = D2D1::RectF(0.0f, 0.0f, width, height);
			geometry.RadiusX = radiusX;
			geometry.RadiusY = radiusY;
			cui::drawing::Transform geometryTransform;
			cui::drawing::TransformOperation geometryTranslation;
			geometryTranslation.Kind = cui::drawing::TransformKind::Translate;
			geometryTranslation.X = 3.0f;
			geometryTranslation.Y = -2.0f;
			geometryTransform.Operations.push_back(geometryTranslation);
			geometry.LocalTransform = std::move(geometryTransform);
			_host->SetClip(geometry);
			Canvas::SetLeft(*_host, left);
			Canvas::SetTop(*_host, top);
			cui::drawing::Transform transform;
			cui::drawing::TransformOperation rotation;
			rotation.Kind = cui::drawing::TransformKind::Rotate;
			rotation.Angle = angle;
			transform.Operations.push_back(rotation);
			_host->SetRenderTransform(transform);
			_host->SetRenderTransformOrigin(D2D1::Point2F(0.5f, 0.5f));
		}

		void ConfigureHostEllipseGeometryClipForTesting(
			float width,
			float height,
			float left,
			float top,
			float angle,
			D2D1_POINT_2F center,
			float radiusX,
			float radiusY)
		{
			if (!_host || !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(left) || !std::isfinite(top)
				|| !std::isfinite(angle) || !std::isfinite(center.x)
				|| !std::isfinite(center.y) || !std::isfinite(radiusX)
				|| !std::isfinite(radiusY) || width <= 0.0f || height <= 0.0f
				|| radiusX <= 0.0f || radiusY <= 0.0f)
				throw std::runtime_error(
					"Benchmark ellipse geometry clip is invalid.");
			_host->Width = width;
			_host->Height = height;
			cui::drawing::Geometry geometry;
			geometry.Kind = cui::drawing::GeometryKind::Ellipse;
			geometry.Center = center;
			geometry.RadiusX = radiusX;
			geometry.RadiusY = radiusY;
			cui::drawing::Transform geometryTransform;
			cui::drawing::TransformOperation geometryTranslation;
			geometryTranslation.Kind = cui::drawing::TransformKind::Translate;
			geometryTranslation.X = 3.0f;
			geometryTranslation.Y = -2.0f;
			geometryTransform.Operations.push_back(geometryTranslation);
			geometry.LocalTransform = std::move(geometryTransform);
			_host->SetClip(geometry);
			Canvas::SetLeft(*_host, left);
			Canvas::SetTop(*_host, top);
			cui::drawing::Transform transform;
			cui::drawing::TransformOperation rotation;
			rotation.Kind = cui::drawing::TransformKind::Rotate;
			rotation.Angle = angle;
			transform.Operations.push_back(rotation);
			_host->SetRenderTransform(transform);
			_host->SetRenderTransformOrigin(D2D1::Point2F(0.5f, 0.5f));
		}

		void ConfigureHostPathGeometryClipForTesting(
			float width,
			float height,
			float left,
			float top,
			float angle)
		{
			if (!_host || !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(left) || !std::isfinite(top)
				|| !std::isfinite(angle) || width <= 0.0f || height <= 0.0f)
				throw std::runtime_error(
					"Benchmark path geometry clip is invalid.");
			_host->Width = width;
			_host->Height = height;
			cui::drawing::Geometry geometry;
			geometry.Kind = cui::drawing::GeometryKind::Path;
			cui::drawing::PathFigure figure;
			figure.StartPoint = D2D1::Point2F(5.0f, 5.0f);
			figure.IsClosed = true;
			figure.IsFilled = true;
			cui::drawing::PathSegment line;
			line.Kind = cui::drawing::PathSegmentKind::Line;
			line.Point = D2D1::Point2F(55.0f, 8.0f);
			figure.Segments.push_back(line);
			line.Point = D2D1::Point2F(30.0f, 55.0f);
			figure.Segments.push_back(line);
			geometry.Figures.push_back(std::move(figure));
			cui::drawing::Transform geometryTransform;
			cui::drawing::TransformOperation geometryTranslation;
			geometryTranslation.Kind = cui::drawing::TransformKind::Translate;
			geometryTranslation.X = 3.0f;
			geometryTranslation.Y = -2.0f;
			geometryTransform.Operations.push_back(geometryTranslation);
			geometry.LocalTransform = std::move(geometryTransform);
			_host->SetClip(geometry);
			Canvas::SetLeft(*_host, left);
			Canvas::SetTop(*_host, top);
			cui::drawing::Transform transform;
			cui::drawing::TransformOperation rotation;
			rotation.Kind = cui::drawing::TransformKind::Rotate;
			rotation.Angle = angle;
			transform.Operations.push_back(rotation);
			_host->SetRenderTransform(transform);
			_host->SetRenderTransformOrigin(D2D1::Point2F(0.5f, 0.5f));
		}

		void ConfigureTargetPathGeometryClipForTesting(
			size_t index,
			float width,
			float height,
			float left,
			float top,
			float angle)
		{
			if (index >= _targets.size() || !_targets[index]
				|| !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(left) || !std::isfinite(top)
				|| !std::isfinite(angle)
				|| width <= 0.0f || height <= 0.0f)
				throw std::runtime_error(
					"Benchmark target path geometry clip is invalid.");
			auto* target = _targets[index];
			target->Width = width;
			target->Height = height;
			cui::drawing::Geometry geometry;
			geometry.Kind = cui::drawing::GeometryKind::Path;
			cui::drawing::PathFigure figure;
			figure.StartPoint = D2D1::Point2F(5.0f, 5.0f);
			figure.IsClosed = true;
			figure.IsFilled = true;
			cui::drawing::PathSegment line;
			line.Kind = cui::drawing::PathSegmentKind::Line;
			line.Point = D2D1::Point2F(55.0f, 8.0f);
			figure.Segments.push_back(line);
			line.Point = D2D1::Point2F(30.0f, 55.0f);
			figure.Segments.push_back(line);
			geometry.Figures.push_back(std::move(figure));
			target->SetClip(geometry);
			Canvas::SetLeft(*target, left);
			Canvas::SetTop(*target, top);
			cui::drawing::Transform transform;
			cui::drawing::TransformOperation rotation;
			rotation.Kind = cui::drawing::TransformKind::Rotate;
			rotation.Angle = angle;
			transform.Operations.push_back(rotation);
			target->SetRenderTransform(transform);
			target->SetRenderTransformOrigin(D2D1::Point2F(0.5f, 0.5f));
		}

		void ConfigureHostGeometryGroupClipForTesting(
			float width,
			float height,
			float left,
			float top,
			float angle)
		{
			if (!_host || !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(left) || !std::isfinite(top)
				|| !std::isfinite(angle) || width <= 0.0f || height <= 0.0f)
				throw std::runtime_error(
					"Benchmark geometry group clip is invalid.");
			_host->Width = width;
			_host->Height = height;
			cui::drawing::Geometry geometry;
			geometry.Kind = cui::drawing::GeometryKind::Group;
			geometry.FillRule = cui::drawing::GeometryFillRule::EvenOdd;
			cui::drawing::Geometry rectangle;
			rectangle.Kind = cui::drawing::GeometryKind::Rectangle;
			rectangle.Rect = D2D1::RectF(5.0f, 5.0f, 55.0f, 55.0f);
			geometry.Children.push_back(rectangle);
			cui::drawing::Geometry ellipse;
			ellipse.Kind = cui::drawing::GeometryKind::Ellipse;
			ellipse.Center = D2D1::Point2F(30.0f, 30.0f);
			ellipse.RadiusX = 12.0f;
			ellipse.RadiusY = 10.0f;
			geometry.Children.push_back(ellipse);
			cui::drawing::Transform geometryTransform;
			cui::drawing::TransformOperation geometryTranslation;
			geometryTranslation.Kind = cui::drawing::TransformKind::Translate;
			geometryTranslation.X = 3.0f;
			geometryTranslation.Y = -2.0f;
			geometryTransform.Operations.push_back(geometryTranslation);
			geometry.LocalTransform = std::move(geometryTransform);
			_host->SetClip(geometry);
			Canvas::SetLeft(*_host, left);
			Canvas::SetTop(*_host, top);
			cui::drawing::Transform transform;
			cui::drawing::TransformOperation rotation;
			rotation.Kind = cui::drawing::TransformKind::Rotate;
			rotation.Angle = angle;
			transform.Operations.push_back(rotation);
			_host->SetRenderTransform(transform);
			_host->SetRenderTransformOrigin(D2D1::Point2F(0.5f, 0.5f));
		}

		void ConfigureTargetGeometryGroupClipForTesting(
			size_t index,
			float width,
			float height,
			float left,
			float top,
			float angle)
		{
			if (index >= _targets.size() || !_targets[index]
				|| !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(left) || !std::isfinite(top)
				|| !std::isfinite(angle) || width <= 0.0f || height <= 0.0f)
				throw std::runtime_error(
					"Benchmark target geometry group clip is invalid.");
			auto* target = _targets[index];
			target->Width = width;
			target->Height = height;
			cui::drawing::Geometry geometry;
			geometry.Kind = cui::drawing::GeometryKind::Group;
			geometry.FillRule = cui::drawing::GeometryFillRule::EvenOdd;
			cui::drawing::Geometry rectangle;
			rectangle.Kind = cui::drawing::GeometryKind::Rectangle;
			rectangle.Rect = D2D1::RectF(5.0f, 5.0f, 55.0f, 55.0f);
			geometry.Children.push_back(rectangle);
			cui::drawing::Geometry ellipse;
			ellipse.Kind = cui::drawing::GeometryKind::Ellipse;
			ellipse.Center = D2D1::Point2F(30.0f, 30.0f);
			ellipse.RadiusX = 12.0f;
			ellipse.RadiusY = 10.0f;
			geometry.Children.push_back(ellipse);
			cui::drawing::Transform geometryTransform;
			cui::drawing::TransformOperation geometryTranslation;
			geometryTranslation.Kind = cui::drawing::TransformKind::Translate;
			geometryTranslation.X = 3.0f;
			geometryTranslation.Y = -2.0f;
			geometryTransform.Operations.push_back(geometryTranslation);
			geometry.LocalTransform = std::move(geometryTransform);
			target->SetClip(geometry);
			Canvas::SetLeft(*target, left);
			Canvas::SetTop(*target, top);
			cui::drawing::Transform transform;
			cui::drawing::TransformOperation rotation;
			rotation.Kind = cui::drawing::TransformKind::Rotate;
			rotation.Angle = angle;
			transform.Operations.push_back(rotation);
			target->SetRenderTransform(transform);
			target->SetRenderTransformOrigin(D2D1::Point2F(0.5f, 0.5f));
		}

		void ConfigureTargetRectangleForTesting(
			size_t index,
			float width,
			float height,
			float left,
			float top)
		{
			if (index >= _targets.size() || !_targets[index]
				|| !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(left) || !std::isfinite(top)
				|| width <= 0.0f || height <= 0.0f)
				throw std::runtime_error("Benchmark target rectangle is invalid.");
			_targets[index]->Width = width;
			_targets[index]->Height = height;
			Canvas::SetLeft(*_targets[index], left);
			Canvas::SetTop(*_targets[index], top);
			_window.UpdateLayout();
		}

		void SetHostOpacityForTesting(double value)
		{
			if (!_host)
				throw std::runtime_error("Benchmark host is unavailable.");
			_host->SetOpacity(value);
			_window.UpdateLayout();
		}

		void ConfigureNestedRotatedClipForTesting(
			size_t targetIndex,
			float width,
			float height,
			float left,
			float top,
			float angle)
		{
			if (targetIndex >= _targets.size() || !_targets[targetIndex]
				|| !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(left) || !std::isfinite(top)
				|| !std::isfinite(angle) || width <= 0.0f || height <= 0.0f)
				throw std::runtime_error("Benchmark nested clip is invalid.");
			auto detached = _host->DetachVisualChild(_targets[targetIndex]);
			if (!detached || detached.get() != _targets[targetIndex])
				throw std::runtime_error(
					"Benchmark nested clip could not detach its target.");
			auto nested = std::make_unique<Canvas>();
			auto* nestedPointer = static_cast<Canvas*>(
				_host->AddOwned(std::move(nested)));
			if (!nestedPointer)
				throw std::runtime_error(
					"Benchmark nested clip container creation failed.");
			nestedPointer->Width = width;
			nestedPointer->Height = height;
			nestedPointer->ClipToBounds = true;
			Canvas::SetLeft(*nestedPointer, left);
			Canvas::SetTop(*nestedPointer, top);
			cui::drawing::Transform transform;
			cui::drawing::TransformOperation rotation;
			rotation.Kind = cui::drawing::TransformKind::Rotate;
			rotation.Angle = angle;
			transform.Operations.push_back(rotation);
			nestedPointer->SetRenderTransform(transform);
			nestedPointer->SetRenderTransformOrigin(
				D2D1::Point2F(0.5f, 0.5f));
			if (nestedPointer->AddOwned(std::move(detached))
				!= _targets[targetIndex])
				throw std::runtime_error(
					"Benchmark nested clip could not reparent its target.");
		}

		void HideOffscreenPresentationForTesting() noexcept
		{
			if (const auto handle = _window.Handle;
				handle && ::IsWindow(handle))
				(void)::ShowWindow(handle, SW_HIDE);
		}

		HWND NativeWindowHandleForTesting() noexcept
		{
			return _window.Handle;
		}

		void SetWindowDpiForTesting(UINT dpi)
		{
			const HWND handle = _window.Handle;
			if (!handle || !::IsWindow(handle) || dpi == 0)
				throw std::runtime_error("Benchmark Window DPI target is invalid.");
			RECT suggested{};
			if (!::GetWindowRect(handle, &suggested))
				throw std::runtime_error("Could not query benchmark Window bounds.");
			(void)::SendMessageW(handle, WM_DPICHANGED,
				MAKEWPARAM(dpi, dpi), reinterpret_cast<LPARAM>(&suggested));
			_window.UpdateLayout();
		}

		uint64_t PresentationCommittedFrameCount() const noexcept
		{
			return cui::framework::WindowAccess::
				PresentationCommittedFrameCount(_window);
		}

		uint64_t PresentationAbortedFrameCount() const noexcept
		{
			return cui::framework::WindowAccess::
				PresentationAbortedFrameCount(_window);
		}

		size_t SynchronizePresentationLayerCountForTesting()
		{
			(void)cui::framework::WindowAccess::PresentationOrder(
				_window, _host);
			return cui::framework::WindowAccess::
				PresentationDrawingLayerCount(_window);
		}

		size_t PresentationOpacityGroupCountForTesting() const noexcept
		{
			return cui::framework::WindowAccess::
				PresentationOpacityGroupCount(_window);
		}

		size_t PresentationGroupedNativeVisualCountForTesting() const noexcept
		{
			return cui::framework::WindowAccess::
				PresentationGroupedNativeVisualCount(_window);
		}

		bool PresentationRequiresCompositionForTesting() const noexcept
		{
			return cui::framework::WindowAccess::
				PresentationRequiresComposition(_window);
		}

		bool TryGetTargetPresentationSnapshotForTesting(
			size_t index,
			PresentationNodeSnapshot& snapshot) const noexcept
		{
			return index < _targets.size() && _targets[index]
				&& cui::framework::WindowAccess::
					TryGetPresentationNodeSnapshot(
						_window, _targets[index], snapshot);
		}

		bool TryGetTargetSceneLayerPixelDigestForTesting(
			size_t index,
			UINT& width,
			UINT& height,
			uint64_t& digest,
			size_t& nonTransparentPixels) const noexcept
		{
			PresentationNodeSnapshot snapshot;
			return TryGetTargetPresentationSnapshotForTesting(index, snapshot)
				&& snapshot.SegmentIndex != static_cast<size_t>(-1)
				&& cui::framework::WindowAccess::
					TryGetPresentationSceneLayerPixelDigestForTesting(
						_window, snapshot.SegmentIndex, width, height,
						digest, nonTransparentPixels);
		}

		bool TryGetSceneLayerPixelDigestForTesting(
			Control* control,
			UINT& width,
			UINT& height,
			uint64_t& digest,
			size_t& nonTransparentPixels) const noexcept
		{
			PresentationNodeSnapshot snapshot;
			return TryGetPresentationSnapshotForTesting(control, snapshot)
				&& snapshot.SegmentIndex != static_cast<size_t>(-1)
				&& cui::framework::WindowAccess::
					TryGetPresentationSceneLayerPixelDigestForTesting(
						_window, snapshot.SegmentIndex, width, height,
						digest, nonTransparentPixels);
		}

		PresentationFrameStatistics PresentationFrameForTesting() const noexcept
		{
			return cui::framework::WindowAccess::PresentationFrame(_window);
		}

		PresentationRenderHost::ResourceSnapshot
			PresentationResourcesForTesting() const noexcept
		{
			return cui::framework::WindowAccess::
				PresentationResourcesForTesting(_window);
		}

		uint64_t PresentationDeviceRecoveryCountForTesting() const noexcept
		{
			return cui::framework::WindowAccess::
				PresentationDeviceRecoveryCount(_window);
		}

		void InjectPresentationDeviceLossForTesting() noexcept
		{
			cui::framework::WindowAccess::
				InjectPresentationDeviceLossForTesting(_window);
		}

		Canvas* HostForTesting() const noexcept { return _host; }

		PresentationRevisionSnapshot HostPresentationRevisions() const noexcept
		{
			return _host ? _host->GetPresentationRevisions()
				: PresentationRevisionSnapshot{};
		}

		size_t TargetCountForTesting() const noexcept
		{
			return _targets.size();
		}

		PresentationRevisionSnapshot TargetPresentationRevisions(
			size_t index) const noexcept
		{
			return index < _targets.size() && _targets[index]
				? _targets[index]->GetPresentationRevisions()
				: PresentationRevisionSnapshot{};
		}

		std::unique_ptr<Canvas> DetachHostForTesting()
		{
			auto detached = _root->DetachVisualChild(_host);
			if (!detached || detached.get() != _host)
				throw std::runtime_error(
					"Benchmark host detach did not preserve ownership.");
			return detached;
		}

		void ReattachHostForTesting(std::unique_ptr<Canvas> host)
		{
			if (!host || host.get() != _host
				|| _root->AddOwned(std::move(host)) != _host)
				throw std::runtime_error(
					"Benchmark host reattach did not preserve identity.");
		}

		void OpenHostAsTransientForTesting()
		{
			if (!cui::framework::WindowAccess::OpenTransientPresentation(
				_window, _host, TransientPresentationOptions{}, nullptr))
				throw std::runtime_error(
					"Benchmark transient presentation could not open.");
		}

		void CloseHostAsTransientForTesting()
		{
			if (!cui::framework::WindowAccess::CloseTransientPresentation(
				_window, _host))
				throw std::runtime_error(
					"Benchmark transient presentation could not close.");
		}

		Canvas* AddTransientRootForTesting(
			float width,
			float height,
			float left,
			float top,
			D2D1_COLOR_F color)
		{
			if (!_root || !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(left) || !std::isfinite(top)
				|| width <= 0.0f || height <= 0.0f)
				throw std::runtime_error(
					"Benchmark additional transient root is invalid.");
			auto transient = std::make_unique<Canvas>();
			auto* result = static_cast<Canvas*>(
				_root->AddOwned(std::move(transient)));
			if (!result)
				throw std::runtime_error(
					"Benchmark additional transient root creation failed.");
			result->Width = width;
			result->Height = height;
			result->Background = color;
			Canvas::SetLeft(*result, left);
			Canvas::SetTop(*result, top);
			_window.UpdateLayout();
			TransientPresentationOptions options;
			options.CloseExistingDismissiblePresentation = false;
			if (!cui::framework::WindowAccess::OpenTransientPresentation(
				_window, result, options, nullptr))
				throw std::runtime_error(
					"Benchmark additional transient root could not open.");
			return result;
		}

		std::vector<Canvas*> AddTransientRootsForTesting(
			size_t count, float extent = 4.0f)
		{
			if (!_root || !std::isfinite(extent) || extent <= 0.0f)
				throw std::runtime_error(
					"Benchmark transient-root pressure input is invalid.");
			std::vector<Canvas*> roots;
			roots.reserve(count);
			for (size_t index = 0; index < count; ++index)
			{
				auto transient = std::make_unique<Canvas>();
				auto* root = transient.get();
				root->Width = extent;
				root->Height = extent;
				root->Background = D2D1_COLOR_F{
					0.2f, 0.65f, 0.95f, 1.0f };
				Canvas::SetLeft(*root,
					static_cast<float>(index % 60u) * 5.0f);
				Canvas::SetTop(*root,
					static_cast<float>(index / 60u) * 5.0f);
				if (_root->AddOwned(std::move(transient)) != root)
					throw std::runtime_error(
						"Benchmark transient-root pressure creation failed.");
				roots.push_back(root);
			}
			_window.UpdateLayout();
			TransientPresentationOptions options;
			options.CloseExistingDismissiblePresentation = false;
			for (auto* root : roots)
				if (!cui::framework::WindowAccess::OpenTransientPresentation(
					_window, root, options, nullptr))
					throw std::runtime_error(
						"Benchmark transient-root pressure open failed.");
			return roots;
		}

		void OpenTransientRootForTesting(Control* root)
		{
			TransientPresentationOptions options;
			options.CloseExistingDismissiblePresentation = false;
			if (!root || !cui::framework::WindowAccess::
				OpenTransientPresentation(_window, root, options, nullptr))
				throw std::runtime_error(
					"Benchmark transient root could not reopen.");
		}

		Control* AddRootControlForTesting(std::unique_ptr<Control> control)
		{
			if (!_root || !control)
				throw std::runtime_error(
					"Benchmark root control is invalid.");
			auto* result = control.get();
			if (_root->AddOwned(std::move(control)) != result)
				throw std::runtime_error(
					"Benchmark root control creation failed.");
			_window.UpdateLayout();
			return result;
		}

		Window& WindowForTesting() noexcept { return _window; }

		void CloseTransientRootForTesting(Canvas* root)
		{
			if (!root || !cui::framework::WindowAccess::
				CloseTransientPresentation(_window, root))
				throw std::runtime_error(
					"Benchmark additional transient root could not close.");
		}

		bool TryGetPresentationSnapshotForTesting(
			Control* control,
			PresentationNodeSnapshot& snapshot) const noexcept
		{
			return control && cui::framework::WindowAccess::
				TryGetPresentationNodeSnapshot(
					_window, control, snapshot);
		}

		void CloseWindowForTesting()
		{
			const ControlWeakReference hostLifetime(_host);
			_window.Close();
			if (hostLifetime.Get())
				throw std::runtime_error(
					"Benchmark Window close retained its visual content.");
			_host = nullptr;
			_root = nullptr;
			_targets.clear();
		}

		void SetWindowEnabledForTesting(bool enabled)
		{
			const auto handle = _window.Handle;
			if (!handle || !::IsWindow(handle))
				throw std::runtime_error(
					"Benchmark Window has no native handle.");
			(void)::EnableWindow(handle, enabled ? TRUE : FALSE);
			if ((::IsWindowEnabled(handle) != FALSE) != enabled)
				throw std::runtime_error(
					"Benchmark Window enabled state did not change.");
		}

		bool AnimationSlotsCleared() const
		{
			return std::all_of(_targets.begin(), _targets.end(),
				[this](Canvas* target)
				{
					if (!target) return false;
					for (const auto kind : _propertyKinds)
					{
						const auto& property = kind
							== BenchmarkPropertyKind::Opacity
							? Control::OpacityProperty()
							: Control::RenderTransformProperty();
						if (target->HasPropertyValue(
							property,
							DependencyPropertyValueSource::Animation))
							return false;
					}
					return true;
				});
		}

	private:
		void CommitPendingControl(
			unsigned long long nowMilliseconds,
			std::string_view operation)
		{
			(void)cui::framework::PresentationAccess::
				ExchangeVisualStateAnimationClockOverrideForTesting(
					*_host, nowMilliseconds);
			if (!cui::framework::PresentationAccess::
				AdvanceVisualStateAnimations(*_host, nowMilliseconds)
				|| cui::framework::PresentationAccess::
					VisualStateAnimationAdvanceFailedForTesting(*_host))
				throw std::runtime_error("Benchmark " + std::string(operation)
					+ " pending control tick failed.");
			_lastTick = nowMilliseconds;
		}

		void RequireLeafCount(size_t expected, const char* operation) const
		{
			const auto actual = ActiveLeafCount();
			if (actual != expected)
				throw std::runtime_error(std::string("Benchmark ") + operation
					+ " retained " + std::to_string(actual) + " leaves; expected "
					+ std::to_string(expected) + ".");
		}

		void RequireRootClockCount(size_t expected, const char* operation) const
		{
			const auto actual = RootClockCount();
			if (actual != expected)
				throw std::runtime_error(std::string("Benchmark ") + operation
					+ " retained " + std::to_string(actual)
					+ " root clocks; expected "
					+ std::to_string(expected) + ".");
		}

		void RequireClockNodeCount(size_t expected, const char* operation) const
		{
			const auto actual = ClockNodeCount();
			if (actual != expected)
				throw std::runtime_error(std::string("Benchmark ") + operation
					+ " retained " + std::to_string(actual)
					+ " ClockNode slots; expected "
					+ std::to_string(expected) + ".");
		}

		void RequireLayerStackCount(
			size_t expected, const char* operation) const
		{
			const auto actual = LayerStackCount();
			if (actual != expected)
				throw std::runtime_error(std::string("Benchmark ") + operation
					+ " retained " + std::to_string(actual)
					+ " animation layer stacks; expected "
					+ std::to_string(expected) + ".");
		}

		void RequireLayerCount(size_t expected, const char* operation) const
		{
			const auto actual = LayerCount();
			if (actual != expected)
				throw std::runtime_error(std::string("Benchmark ") + operation
					+ " retained " + std::to_string(actual)
					+ " animation layers; expected "
					+ std::to_string(expected) + ".");
		}

		void RequireLayerMaxDepth(
			size_t expected, const char* operation) const
		{
			const auto actual = LayerMaxDepth();
			if (actual != expected)
				throw std::runtime_error(std::string("Benchmark ") + operation
					+ " retained animation layer depth "
					+ std::to_string(actual) + "; expected "
					+ std::to_string(expected) + ".");
		}

		void RequireRegisteredControlCount(
			size_t expected, const char* operation)
		{
			const auto actual = RegisteredControlCount();
			if (actual != expected)
				throw std::runtime_error(std::string("Benchmark ") + operation
					+ " registered " + std::to_string(actual)
					+ " declarative animation controls; expected "
					+ std::to_string(expected) + ".");
		}

		size_t _animationCount = 0;
		size_t _rootCount = 1;
		bool _shareTarget = false;
		bool _composeRoots = false;
		unsigned long long _lastTick = BenchmarkClockOrigin;
		BenchmarkPropertyKind _propertyKind = BenchmarkPropertyKind::TransformX;
		std::vector<BenchmarkPropertyKind> _propertyKinds;
		BenchmarkProgram _program;
		Window _window;
		Canvas* _root = nullptr;
		Canvas* _host = nullptr;
		std::vector<Canvas*> _targets;
		std::vector<Control*> _targetSlots;
		bool _sceneLayerPixelReadbackLease = false;
	};

	class CaretAnimationProbe final : public Control
	{
	public:
		bool IsAnimationRunning() override
		{
			return IsCaretBlinkAnimating();
		}

		bool HasRetainedNativeAnimation() override
		{
			return HasRetainedCaretBlinkAnimation();
		}

		bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override
		{
			return GetCaretBlinkInvalidRect(outRect);
		}

		void SetCaretState(
			bool focused, int start, int end, bool valid)
		{
			const D2D1_RECT_F rect{ 1.0f, 2.0f, 2.0f, 14.0f };
			UpdateCaretBlinkState(
				focused, start, end, valid, valid ? &rect : nullptr);
		}
	};

	class NativeAnimationQueryTrap final : public Control
	{
	public:
		bool IsAnimationRunning() override
		{
			++Queries;
			return false;
		}

		int Queries = 0;
	};

	class RetainedNativeAnimationQueryProbe final : public Control
	{
	public:
		bool IsAnimationRunning() override
		{
			++ActiveQueries;
			return false;
		}

		bool HasRetainedNativeAnimation() override
		{
			++RetainedQueries;
			return true;
		}

		int ActiveQueries = 0;
		int RetainedQueries = 0;
	};

	BenchmarkParameters ParametersFor(std::string_view profile)
	{
		if (profile == "smoke")
			return {
				"smoke", { 10u, 100u }, 8u, 40u, 20u, 2u,
				100u, { 0u, 100u }, 4u, 10u, 100u, 1u, 2u,
				50u, 4u, 30u, 5'000u };
		if (profile == "baseline")
			return {
				"baseline", { 10u, 100u, 200u, 500u }, 64u, 1000u,
				1000u, 20u, 200u, { 0u, 1000u, 5000u }, 16u, 200u,
				500u, 5u, 50u, 200u, 16u, 600u, 20'000u };
		throw std::runtime_error("Unknown animation benchmark profile.");
	}

	void RunLifecycleCycle(
		size_t animationCount,
		BenchmarkPropertyKind propertyKind)
	{
		BenchmarkScene lifecycle(animationCount, 0u, propertyKind);
		lifecycle.Begin();
		lifecycle.Advance(BenchmarkClockOrigin + 16u);
		lifecycle.Restart();
		lifecycle.Stop();
		if (!lifecycle.AnimationSlotsCleared())
			throw std::runtime_error(
				"Benchmark lifecycle retained Animation DP slots.");
	}

	ScaleResult RunScale(
		size_t animationCount,
		BenchmarkPropertyKind propertyKind,
		const BenchmarkParameters& parameters,
		unsigned long long frequency)
	{
		ScaleResult result;
		result.Property = std::string(PropertyName(propertyKind));
		result.AnimationCount = animationCount;
		result.MemoryBefore = QueryProcessMemory();
		std::unique_ptr<BenchmarkScene> scene;
		result.CreateInstallMicroseconds = MeasureMicroseconds(frequency,
			[&] { scene = std::make_unique<BenchmarkScene>(
				animationCount, 0u, propertyKind); });
		result.BeginMicroseconds = MeasureMicroseconds(
			frequency, [&] { scene->Begin(); });
		result.ActiveLeavesAfterBegin = scene->ActiveLeafCount();
		result.AnimationLayerStacksAfterBegin = scene->LayerStackCount();
		result.AnimationLayersAfterBegin = scene->LayerCount();
		result.AnimationLayerMaxDepthAfterBegin = scene->LayerMaxDepth();

		for (size_t index = 0; index < parameters.FrameWarmup; ++index)
			scene->Advance(BenchmarkClockOrigin + index * 16u);
		std::vector<double> advanceSamples;
		advanceSamples.reserve(parameters.FrameSamples);
		for (size_t index = 0; index < parameters.FrameSamples; ++index)
			advanceSamples.push_back(MeasureMicroseconds(frequency, [&]
				{
					scene->Advance(BenchmarkClockOrigin
						+ (parameters.FrameWarmup + index) * 16u);
				}));
		result.EvaluatorFramesWithinBudget = static_cast<size_t>(
			std::count_if(advanceSamples.begin(), advanceSamples.end(),
				[](double sample)
				{ return sample <= SixtyHertzFrameBudgetMicroseconds; }));
		result.EvaluatorOnTimeRate = advanceSamples.empty() ? 0.0
			: static_cast<double>(result.EvaluatorFramesWithinBudget)
				/ static_cast<double>(advanceSamples.size());
		result.Advance = Summarize(std::move(advanceSamples));

		std::vector<double> restartSamples;
		restartSamples.reserve(parameters.RestartSamples);
		for (size_t index = 0; index < parameters.RestartSamples; ++index)
			restartSamples.push_back(MeasureMicroseconds(
				frequency, [&] { scene->Restart(); }));
		result.Restart = Summarize(std::move(restartSamples));
		result.ActiveLeavesAfterRestarts = scene->ActiveLeafCount();
		result.AnimationLayerStacksAfterRestarts = scene->LayerStackCount();
		result.AnimationLayersAfterRestarts = scene->LayerCount();
		result.StopMicroseconds = MeasureMicroseconds(
			frequency, [&] { scene->Stop(); });
		result.ActiveLeavesAfterStop = scene->ActiveLeafCount();
		result.AnimationLayerStacksAfterStop = scene->LayerStackCount();
		result.AnimationLayersAfterStop = scene->LayerCount();
		result.AnimationSlotsClearedAfterStop = scene->AnimationSlotsCleared();
		scene.reset();

		std::vector<double> lifecycleSamples;
		lifecycleSamples.reserve(parameters.LifecycleSamples);
		for (size_t index = 0; index < parameters.LifecycleSamples; ++index)
			lifecycleSamples.push_back(MeasureMicroseconds(frequency, [&]
				{ RunLifecycleCycle(animationCount, propertyKind); }));
		result.Lifecycle = Summarize(std::move(lifecycleSamples));
		result.MemoryAfter = QueryProcessMemory();
		return result;
	}

	WindowTickResult RunWindowTick(
		size_t animationCount,
		size_t unrelatedVisualCount,
		BenchmarkPropertyKind propertyKind,
		const BenchmarkParameters& parameters,
		unsigned long long frequency)
	{
		BenchmarkScene scene(
			animationCount, unrelatedVisualCount, propertyKind);
		scene.Begin();
		for (size_t index = 0; index < parameters.ScanWarmup; ++index)
			scene.TickWindow(BenchmarkClockOrigin + index * 16u);
		std::vector<double> samples;
		samples.reserve(parameters.ScanSamples);
		for (size_t index = 0; index < parameters.ScanSamples; ++index)
			samples.push_back(MeasureMicroseconds(frequency, [&]
				{
					scene.TickWindow(BenchmarkClockOrigin
						+ (parameters.ScanWarmup + index) * 16u);
				}));
		scene.Stop();
		if (!scene.AnimationSlotsCleared())
			throw std::runtime_error(
				"Benchmark Window tick retained Animation DP slots.");
		return {
			std::string(PropertyName(propertyKind)),
			animationCount,
			unrelatedVisualCount,
			animationCount + unrelatedVisualCount + 3u,
			Summarize(std::move(samples)) };
	}

	RegistryTickResult RunRegistryTick(
		size_t animationCount,
		size_t unrelatedVisualCount,
		BenchmarkPropertyKind propertyKind,
		const BenchmarkParameters& parameters,
		unsigned long long frequency)
	{
		BenchmarkScene scene(
			animationCount, unrelatedVisualCount, propertyKind);
		scene.Begin();
		for (size_t index = 0; index < parameters.ScanWarmup; ++index)
			scene.TickRegisteredWindow(
				BenchmarkClockOrigin + index * 16u);
		std::vector<double> samples;
		samples.reserve(parameters.ScanSamples);
		for (size_t index = 0; index < parameters.ScanSamples; ++index)
			samples.push_back(MeasureMicroseconds(frequency, [&]
				{
					scene.TickRegisteredWindow(BenchmarkClockOrigin
						+ (parameters.ScanWarmup + index) * 16u);
				}));
		scene.Stop();
		if (!scene.AnimationSlotsCleared())
			throw std::runtime_error(
				"Benchmark registry tick retained Animation DP slots.");
		return {
			std::string(PropertyName(propertyKind)),
			animationCount,
			unrelatedVisualCount,
			animationCount + unrelatedVisualCount + 3u,
			Summarize(std::move(samples)) };
	}

	RetentionResult RunRetention(
		const BenchmarkParameters& parameters,
		BenchmarkPropertyKind propertyKind,
		unsigned long long frequency)
	{
		RetentionResult result;
		result.Property = std::string(PropertyName(propertyKind));
		result.AnimationCount = parameters.RetentionAnimationCount;
		result.WarmupCycles = parameters.RetentionWarmupCycles;
		result.CyclesPerPass = parameters.RetentionCyclesPerPass;
		for (size_t index = 0; index < result.WarmupCycles; ++index)
			RunLifecycleCycle(result.AnimationCount, propertyKind);
		result.Before = QueryProcessMemory();
		auto runPass = [&]
		{
			for (size_t index = 0; index < result.CyclesPerPass; ++index)
				RunLifecycleCycle(result.AnimationCount, propertyKind);
		};
		result.Pass1Microseconds = MeasureMicroseconds(frequency, runPass);
		result.AfterPass1 = QueryProcessMemory();
		result.Pass2Microseconds = MeasureMicroseconds(frequency, runPass);
		result.AfterPass2 = QueryProcessMemory();
		return result;
	}

	PresentationCadenceResult RunPresentationCadence(
		const BenchmarkParameters& parameters,
		BenchmarkPropertyKind propertyKind,
		unsigned long long frequency)
	{
		PresentationCadenceResult result;
		result.Property = std::string(PropertyName(propertyKind));
		result.AnimationCount = parameters.PresentationAnimationCount;
		result.WarmupFrames = parameters.PresentationWarmupFrames;
		result.FrameSamples = parameters.PresentationFrameSamples;
		BenchmarkScene scene(
			result.AnimationCount, 0u, propertyKind);
		scene.ShowOffscreenWithoutActivationForTesting();
		result.CommittedFramesBefore =
			scene.PresentationCommittedFrameCount();
		result.AbortedFramesBefore =
			scene.PresentationAbortedFrameCount();
		scene.UseRealtimeClockForTesting();
		scene.Begin();
		if (!scene.AnimationFrameSchedulerRunning()
			|| scene.AnimationUsesLegacyTimer())
			throw std::runtime_error(
				"Presentation cadence benchmark did not arm the "
				"animation frame scheduler.");

		const auto requiredFrames = result.WarmupFrames
			+ result.FrameSamples + 1u;
		std::vector<long long> committedAt;
		committedAt.reserve(requiredFrames);
		auto previousCommitted = result.CommittedFramesBefore;
		const auto timeoutStart = QueryCounter();
		try
		{
			while (committedAt.size() < requiredFrames)
			{
				if (ElapsedMicroseconds(
					timeoutStart, QueryCounter(), frequency)
					> static_cast<double>(
						parameters.PresentationTimeoutMilliseconds) * 1'000.0)
					throw std::runtime_error(
						"Presentation cadence benchmark timed out.");
				const auto wait = ::MsgWaitForMultipleObjectsEx(
					0, nullptr, 50u, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
				if (wait == WAIT_FAILED)
					throw std::runtime_error(
						"Presentation cadence message wait failed.");
				MSG message{};
				while (::PeekMessageW(
					&message, nullptr, 0u, 0u, PM_REMOVE) != FALSE)
				{
					if (message.message == WM_QUIT)
					{
						::PostQuitMessage(static_cast<int>(message.wParam));
						throw std::runtime_error(
							"Presentation cadence observed WM_QUIT.");
					}
					::TranslateMessage(&message);
					::DispatchMessageW(&message);
					cui::PumpUIThreadCallbacks();
					const auto committed =
						scene.PresentationCommittedFrameCount();
					if (committed == previousCommitted) continue;
					if (committed != previousCommitted + 1u)
						throw std::runtime_error(
							"Presentation cadence skipped an observable "
							"render-host commit sequence.");
					previousCommitted = committed;
					committedAt.push_back(QueryCounter());
				}
			}
		}
		catch (...)
		{
			scene.HideOffscreenPresentationForTesting();
			try { scene.Stop(); }
			catch (...) {}
			throw;
		}

		result.CommittedFramesAfter =
			scene.PresentationCommittedFrameCount();
		result.AbortedFramesAfter =
			scene.PresentationAbortedFrameCount();
		std::vector<double> intervals;
		intervals.reserve(result.FrameSamples);
		const auto origin = committedAt[result.WarmupFrames];
		for (size_t index = 1u; index <= result.FrameSamples; ++index)
		{
			const auto current = committedAt[result.WarmupFrames + index];
			const auto previous = committedAt[
				result.WarmupFrames + index - 1u];
			intervals.push_back(ElapsedMicroseconds(
				previous, current, frequency));
			const auto elapsed = ElapsedMicroseconds(
				origin, current, frequency);
			if (elapsed <= result.FrameBudgetMicroseconds
				* static_cast<double>(index))
				++result.FramesDeliveredByDeadline;
		}
		result.OnTimeRate = result.FrameSamples == 0u ? 0.0
			: static_cast<double>(result.FramesDeliveredByDeadline)
				/ static_cast<double>(result.FrameSamples);
		result.CommittedFrameInterval = Summarize(std::move(intervals));
		scene.HideOffscreenPresentationForTesting();
		scene.Stop();
		return result;
	}

	BenchmarkResult RunBenchmark(const BenchmarkParameters& parameters)
	{
		constexpr auto propertyKind = BenchmarkPropertyKind::TransformX;
		BenchmarkResult result;
		result.Parameters = parameters;
		result.QpcFrequency = QueryFrequency();
		result.LogicalProcessorCount = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
		result.ProcessId = ::GetCurrentProcessId();
		result.PointerBits = static_cast<unsigned>(sizeof(void*) * 8u);
		SYSTEM_INFO systemInfo{};
		::GetNativeSystemInfo(&systemInfo);
		result.ProcessorArchitecture = systemInfo.wProcessorArchitecture;
		MEMORYSTATUSEX memoryStatus{};
		memoryStatus.dwLength = sizeof(memoryStatus);
		if (::GlobalMemoryStatusEx(&memoryStatus))
			result.TotalPhysicalMemoryBytes = memoryStatus.ullTotalPhys;
		result.CpuName = QueryCpuName();
		RunLifecycleCycle(1u, propertyKind);
		result.ProcessAtStart = QueryProcessMemory();
		for (const auto count : parameters.AnimationCounts)
		{
			auto scale = RunScale(
				count, propertyKind, parameters, result.QpcFrequency);
			result.Scales.push_back(std::move(scale));
		}
		for (const auto unrelated : parameters.UnrelatedVisualCounts)
			result.WindowTicks.push_back(RunWindowTick(
				parameters.ScanAnimationCount, unrelated,
				propertyKind,
				parameters, result.QpcFrequency));
		for (const auto unrelated : parameters.UnrelatedVisualCounts)
			result.RegistryTicks.push_back(RunRegistryTick(
				parameters.ScanAnimationCount, unrelated,
				propertyKind,
				parameters, result.QpcFrequency));
		result.Retention = RunRetention(
			parameters, propertyKind, result.QpcFrequency);
		result.PresentationCadence = RunPresentationCadence(
			parameters, propertyKind, result.QpcFrequency);
		result.ProcessAtEnd = QueryProcessMemory();
		return result;
	}

	std::string FormatDouble(double value);

	std::optional<std::string> AcceptanceFailure(
		const BenchmarkResult& result)
	{
		if (result.Parameters.Profile != "baseline") return std::nullopt;
		const auto scale = std::find_if(
			result.Scales.begin(), result.Scales.end(),
			[](const auto& candidate)
			{ return candidate.AnimationCount == 200u; });
		if (scale == result.Scales.end())
			return "Baseline result omitted the 200-animation scale case.";
		if (scale->Advance.P95Microseconds > 2'000.0
			|| scale->EvaluatorOnTimeRate < 0.99)
			return "200-animation evaluator missed the p95 or 60 Hz "
				"frame-budget acceptance threshold: p95="
				+ FormatDouble(scale->Advance.P95Microseconds)
				+ "us, onTimeRate="
				+ FormatDouble(scale->EvaluatorOnTimeRate) + ".";
		const auto& cadence = result.PresentationCadence;
		if (cadence.AbortedFramesAfter != cadence.AbortedFramesBefore)
			return "Presentation cadence observed aborted render frames: before="
				+ std::to_string(cadence.AbortedFramesBefore) + ", after="
				+ std::to_string(cadence.AbortedFramesAfter) + ".";
		if (cadence.OnTimeRate < 0.99)
			return "200-animation presentation commits missed the 99% "
				"60 Hz on-time acceptance threshold: delivered="
				+ std::to_string(cadence.FramesDeliveredByDeadline) + "/"
				+ std::to_string(cadence.FrameSamples) + ", onTimeRate="
				+ FormatDouble(cadence.OnTimeRate) + ", p95="
				+ FormatDouble(
					cadence.CommittedFrameInterval.P95Microseconds) + "us.";
		return std::nullopt;
	}

	std::string FormatDouble(double value)
	{
		if (!std::isfinite(value))
			throw std::runtime_error("Benchmark contains a non-finite number.");
		char buffer[128]{};
		const auto converted = std::to_chars(
			std::begin(buffer), std::end(buffer), value,
			std::chars_format::general,
			std::numeric_limits<double>::max_digits10);
		if (converted.ec != std::errc{})
			throw std::runtime_error("Could not format benchmark number.");
		return std::string(buffer, converted.ptr);
	}

	std::string JsonEscape(std::string_view value)
	{
		std::string result = "\"";
		for (const unsigned char character : value)
			switch (character)
			{
			case '"': result += "\\\""; break;
			case '\\': result += "\\\\"; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			default:
				if (character < 0x20)
					throw std::runtime_error(
						"Benchmark JSON string contains a control character.");
				result.push_back(static_cast<char>(character));
				break;
			}
		result += '"';
		return result;
	}

	uint64_t SceneRasterBytes(
		const PresentationRenderHost::ResourceSnapshot& value) noexcept
	{
		return value.EstimatedSceneSwapChainBytes
			+ value.EstimatedSceneCompositionSurfaceBytes
			+ value.EstimatedSceneSubmittedSnapshotBytes;
	}

	std::string ResourceSnapshotJson(
		const PresentationRenderHost::ResourceSnapshot& value)
	{
		const std::wstring description(value.AdapterDescription);
		return std::string("{\"deviceGeneration\":")
			+ std::to_string(value.DeviceGeneration)
			+ ",\"isHardwareAdapter\":"
			+ (value.IsHardwareAdapter ? "true" : "false")
			+ ",\"isSoftwareAdapter\":"
			+ (value.IsSoftwareAdapter ? "true" : "false")
			+ ",\"supportsVideo\":"
			+ (value.SupportsVideo ? "true" : "false")
			+ ",\"featureLevel\":" + std::to_string(value.FeatureLevel)
			+ ",\"vendorId\":" + std::to_string(value.VendorId)
			+ ",\"deviceId\":" + std::to_string(value.DeviceId)
			+ ",\"adapterLuid\":\""
			+ std::to_string(value.AdapterLuid) + "\",\"adapterDescription\":"
			+ JsonEscape(Convert::UnicodeToUtf8(description))
			+ ",\"dedicatedVideoMemoryBytes\":"
			+ std::to_string(value.DedicatedVideoMemoryBytes)
			+ ",\"sharedSystemMemoryBytes\":"
			+ std::to_string(value.SharedSystemMemoryBytes)
			+ ",\"localMemoryBudgetBytes\":"
			+ std::to_string(value.LocalMemoryBudgetBytes)
			+ ",\"localMemoryCurrentUsageBytes\":"
			+ std::to_string(value.LocalMemoryCurrentUsageBytes)
			+ ",\"nonLocalMemoryBudgetBytes\":"
			+ std::to_string(value.NonLocalMemoryBudgetBytes)
			+ ",\"nonLocalMemoryCurrentUsageBytes\":"
			+ std::to_string(value.NonLocalMemoryCurrentUsageBytes)
			+ ",\"sceneLayerCount\":"
			+ std::to_string(value.SceneLayerCount)
			+ ",\"sceneLayerSwapChainCount\":"
			+ std::to_string(value.SceneLayerSwapChainCount)
			+ ",\"sceneLayerCompositionSurfaceCount\":"
			+ std::to_string(value.SceneLayerCompositionSurfaceCount)
			+ ",\"sceneLayerDirect2DSurfaceContextCount\":"
			+ std::to_string(value.SceneLayerDirect2DSurfaceContextCount)
			+ ",\"sceneLayerSubmittedSnapshotTextureCount\":"
			+ std::to_string(value.SceneLayerSubmittedSnapshotTextureCount)
			+ ",\"sceneLayerPixelReadbackLeaseCount\":"
			+ std::to_string(value.SceneLayerPixelReadbackLeaseCount)
			+ ",\"sceneLayerSlotCount\":"
			+ std::to_string(value.SceneLayerSlotCount)
			+ ",\"sceneLayerSlotCapacity\":"
			+ std::to_string(value.SceneLayerSlotCapacity)
			+ ",\"sceneLayerGroupCount\":"
			+ std::to_string(value.SceneLayerGroupCount)
			+ ",\"fullWindowSceneLayerCount\":"
			+ std::to_string(value.FullWindowSceneLayerCount)
			+ ",\"sceneLayerDistinctGraphicsDeviceCount\":"
			+ std::to_string(value.SceneLayerDistinctGraphicsDeviceCount)
			+ ",\"sceneLayerDistinctRecorderDeviceCount\":"
			+ std::to_string(value.SceneLayerDistinctRecorderDeviceCount)
			+ ",\"sceneLayerSharedGraphicsDeviceCount\":"
			+ std::to_string(value.SceneLayerSharedGraphicsDeviceCount)
			+ ",\"sceneLayerSharedRecorderDeviceCount\":"
			+ std::to_string(value.SceneLayerSharedRecorderDeviceCount)
			+ ",\"sceneCommandRecorderCount\":"
			+ std::to_string(value.SceneCommandRecorderCount)
			+ ",\"sceneCommandRecorderReferenceCount\":"
			+ std::to_string(value.SceneCommandRecorderReferenceCount)
			+ ",\"maximumSceneSurfaceWidth\":"
			+ std::to_string(value.MaximumSceneSurfaceWidth)
			+ ",\"maximumSceneSurfaceHeight\":"
			+ std::to_string(value.MaximumSceneSurfaceHeight)
			+ ",\"estimatedSceneSwapChainBytes\":"
			+ std::to_string(value.EstimatedSceneSwapChainBytes)
			+ ",\"estimatedSceneCompositionSurfaceBytes\":"
			+ std::to_string(value.EstimatedSceneCompositionSurfaceBytes)
			+ ",\"estimatedSceneSubmittedSnapshotBytes\":"
			+ std::to_string(value.EstimatedSceneSubmittedSnapshotBytes)
			+ ",\"estimatedSceneLayerSlotBytes\":"
			+ std::to_string(value.EstimatedSceneLayerSlotBytes)
			+ ",\"estimatedSceneRetainedBytes\":"
			+ std::to_string(value.EstimatedSceneRetainedBytes)
			+ ",\"estimatedPrimarySwapChainBytes\":"
			+ std::to_string(value.EstimatedPrimarySwapChainBytes)
			+ ",\"estimatedOverlaySwapChainBytes\":"
			+ std::to_string(value.EstimatedOverlaySwapChainBytes)
			+ ",\"estimatedTotalSwapChainBytes\":"
			+ std::to_string(value.EstimatedTotalSwapChainBytes)
			+ ",\"primaryPresentSyncInterval\":"
			+ std::to_string(value.PrimaryPresentSyncInterval)
			+ ",\"overlayPresentSyncInterval\":"
			+ std::to_string(value.OverlayPresentSyncInterval)
			+ ",\"minimumScenePresentSyncInterval\":"
			+ std::to_string(value.MinimumScenePresentSyncInterval)
			+ ",\"maximumScenePresentSyncInterval\":"
			+ std::to_string(value.MaximumScenePresentSyncInterval)
			+ ",\"peakSceneLayerCount\":"
			+ std::to_string(value.PeakSceneLayerCount)
			+ ",\"peakSceneLayerSlotCount\":"
			+ std::to_string(value.PeakSceneLayerSlotCount)
			+ ",\"peakEstimatedSceneSwapChainBytes\":"
			+ std::to_string(value.PeakEstimatedSceneSwapChainBytes)
			+ ",\"peakEstimatedSceneCompositionSurfaceBytes\":"
			+ std::to_string(value.PeakEstimatedSceneCompositionSurfaceBytes)
			+ ",\"peakEstimatedSceneSubmittedSnapshotBytes\":"
			+ std::to_string(value.PeakEstimatedSceneSubmittedSnapshotBytes)
			+ ",\"peakEstimatedSceneLayerSlotBytes\":"
			+ std::to_string(value.PeakEstimatedSceneLayerSlotBytes)
			+ ",\"sceneLayerCreateCount\":"
			+ std::to_string(value.SceneLayerCreateCount)
			+ ",\"sceneLayerResizeCount\":"
			+ std::to_string(value.SceneLayerResizeCount)
			+ ",\"sceneLayerReleaseCount\":"
			+ std::to_string(value.SceneLayerReleaseCount)
			+ ",\"sceneLayerAllocationFailureCount\":"
			+ std::to_string(value.SceneLayerAllocationFailureCount)
			+ ",\"sceneLayerSubmittedSnapshotCreateCount\":"
			+ std::to_string(value.SceneLayerSubmittedSnapshotCreateCount)
			+ ",\"sceneLayerSubmittedSnapshotUpdateCount\":"
			+ std::to_string(value.SceneLayerSubmittedSnapshotUpdateCount)
			+ ",\"sceneLayerSubmittedSnapshotCopiedBytes\":"
			+ std::to_string(value.SceneLayerSubmittedSnapshotCopiedBytes)
			+ ",\"sceneLayerSubmittedSnapshotCreateMicroseconds\":"
			+ FormatDouble(
				value.SceneLayerSubmittedSnapshotCreateMicroseconds)
			+ ",\"sceneLayerSubmittedSnapshotCopyMicroseconds\":"
			+ FormatDouble(value.SceneLayerSubmittedSnapshotCopyMicroseconds)
			+ ",\"sceneLayerSlotEnsureMicroseconds\":"
			+ FormatDouble(value.SceneLayerSlotEnsureMicroseconds)
			+ ",\"sceneLayerTopologyBatchBeginMicroseconds\":"
			+ FormatDouble(value.SceneLayerTopologyBatchBeginMicroseconds)
			+ ",\"sceneLayerSwapChainCreateMicroseconds\":"
			+ FormatDouble(value.SceneLayerSwapChainCreateMicroseconds)
			+ ",\"sceneLayerCompositionSurfaceCreateMicroseconds\":"
			+ FormatDouble(value.SceneLayerCompositionSurfaceCreateMicroseconds)
			+ ",\"sceneLayerVisualCreateMicroseconds\":"
			+ FormatDouble(value.SceneLayerVisualCreateMicroseconds)
			+ ",\"sceneLayerVisualBindMicroseconds\":"
			+ FormatDouble(value.SceneLayerVisualBindMicroseconds)
			+ ",\"sceneLayerGraphicsCreateMicroseconds\":"
			+ FormatDouble(value.SceneLayerGraphicsCreateMicroseconds)
			+ ",\"sceneLayerRecorderCreateMicroseconds\":"
			+ FormatDouble(value.SceneLayerRecorderCreateMicroseconds)
			+ ",\"sceneLayerDpiSetupMicroseconds\":"
			+ FormatDouble(value.SceneLayerDpiSetupMicroseconds)
			+ ",\"sceneLayerVisualPropertyStageMicroseconds\":"
			+ FormatDouble(value.SceneLayerVisualPropertyStageMicroseconds)
			+ ",\"sceneLayerGroupStageMicroseconds\":"
			+ FormatDouble(value.SceneLayerGroupStageMicroseconds)
			+ ",\"sceneLayerResourcePeakUpdateMicroseconds\":"
			+ FormatDouble(value.SceneLayerResourcePeakUpdateMicroseconds)
			+ ",\"sceneLayerTopologyBatchCommitMicroseconds\":"
			+ FormatDouble(value.SceneLayerTopologyBatchCommitMicroseconds)
			+ ",\"compositionVisualStackRebuildCount\":"
			+ std::to_string(value.CompositionVisualStackRebuildCount)
			+ ",\"compositionVisualStackRebuildEntryCount\":"
			+ std::to_string(value.CompositionVisualStackRebuildEntryCount)
			+ ",\"compositionVisualDeferredMutationCount\":"
			+ std::to_string(value.CompositionVisualDeferredMutationCount)
			+ ",\"compositionVisualBatchCommitCount\":"
			+ std::to_string(value.CompositionVisualBatchCommitCount)
			+ ",\"compositionVisualBatchRollbackCount\":"
			+ std::to_string(value.CompositionVisualBatchRollbackCount)
			+ ",\"compositionVisualBatchRollbackFailureCount\":"
			+ std::to_string(
				value.CompositionVisualBatchRollbackFailureCount) + "}";
	}

	void AppendDistribution(
		std::string& json,
		const Distribution& value,
		std::string_view indent)
	{
		json += std::string(indent) + "{\n";
		json += std::string(indent) + "  \"count\": "
			+ std::to_string(value.Count) + ",\n";
		json += std::string(indent) + "  \"minimumMicroseconds\": "
			+ FormatDouble(value.MinimumMicroseconds) + ",\n";
		json += std::string(indent) + "  \"meanMicroseconds\": "
			+ FormatDouble(value.MeanMicroseconds) + ",\n";
		json += std::string(indent) + "  \"p50Microseconds\": "
			+ FormatDouble(value.P50Microseconds) + ",\n";
		json += std::string(indent) + "  \"p95Microseconds\": "
			+ FormatDouble(value.P95Microseconds) + ",\n";
		json += std::string(indent) + "  \"p99Microseconds\": "
			+ FormatDouble(value.P99Microseconds) + ",\n";
		json += std::string(indent) + "  \"maximumMicroseconds\": "
			+ FormatDouble(value.MaximumMicroseconds) + "\n";
		json += std::string(indent) + "}";
	}

	void AppendMemory(
		std::string& json,
		const ProcessMemory& value,
		std::string_view indent)
	{
		json += std::string(indent) + "{\n";
		json += std::string(indent) + "  \"workingSetBytes\": "
			+ std::to_string(value.WorkingSetBytes) + ",\n";
		json += std::string(indent) + "  \"privateUsageBytes\": "
			+ std::to_string(value.PrivateUsageBytes) + "\n";
		json += std::string(indent) + "}";
	}

	std::string Serialize(const BenchmarkResult& result)
	{
		std::string json;
		json += "{\n";
		json += "  \"schemaVersion\": 1,\n";
		json += "  \"runnerVersion\": " + JsonEscape(BenchmarkVersion) + ",\n";
		json += "  \"engine\": \"CUI-Design-CompiledInteraction\",\n";
		json += "  \"profile\": "
			+ JsonEscape(result.Parameters.Profile) + ",\n";
		json += "  \"percentileMethod\": \"nearest-rank\",\n";
		json += "  \"environment\": {\n";
		json += "    \"qpcFrequency\": "
			+ std::to_string(result.QpcFrequency) + ",\n";
		json += "    \"logicalProcessorCount\": "
			+ std::to_string(result.LogicalProcessorCount) + ",\n";
		json += "    \"processId\": "
			+ std::to_string(result.ProcessId) + ",\n";
		json += "    \"pointerBits\": "
			+ std::to_string(result.PointerBits) + ",\n";
		json += "    \"processorArchitecture\": "
			+ std::to_string(result.ProcessorArchitecture) + ",\n";
		json += "    \"totalPhysicalMemoryBytes\": "
			+ std::to_string(result.TotalPhysicalMemoryBytes) + ",\n";
		json += "    \"cpuName\": " + JsonEscape(result.CpuName) + ",\n";
		json += "    \"buildConfiguration\": "
			+ JsonEscape(BenchmarkBuildConfiguration) + "\n";
		json += "  },\n";
		json += "  \"parameters\": {\n";
		json += "    \"frameWarmup\": "
			+ std::to_string(result.Parameters.FrameWarmup) + ",\n";
		json += "    \"frameSamples\": "
			+ std::to_string(result.Parameters.FrameSamples) + ",\n";
		json += "    \"restartSamples\": "
			+ std::to_string(result.Parameters.RestartSamples) + ",\n";
		json += "    \"lifecycleSamples\": "
			+ std::to_string(result.Parameters.LifecycleSamples) + ",\n";
		json += "    \"scanWarmup\": "
			+ std::to_string(result.Parameters.ScanWarmup) + ",\n";
		json += "    \"scanSamples\": "
			+ std::to_string(result.Parameters.ScanSamples) + ",\n";
		json += "    \"retentionAnimationCount\": "
			+ std::to_string(result.Parameters.RetentionAnimationCount) + ",\n";
		json += "    \"retentionWarmupCycles\": "
			+ std::to_string(result.Parameters.RetentionWarmupCycles) + ",\n";
		json += "    \"retentionCyclesPerPass\": "
			+ std::to_string(result.Parameters.RetentionCyclesPerPass) + ",\n";
		json += "    \"presentationAnimationCount\": "
			+ std::to_string(result.Parameters.PresentationAnimationCount) + ",\n";
		json += "    \"presentationWarmupFrames\": "
			+ std::to_string(result.Parameters.PresentationWarmupFrames) + ",\n";
		json += "    \"presentationFrameSamples\": "
			+ std::to_string(result.Parameters.PresentationFrameSamples) + ",\n";
		json += "    \"presentationTimeoutMilliseconds\": "
			+ std::to_string(
				result.Parameters.PresentationTimeoutMilliseconds) + "\n";
		json += "  },\n";
		json += "  \"processAtStart\": ";
		AppendMemory(json, result.ProcessAtStart, "  ");
		json += ",\n  \"scaleCases\": [\n";
		for (size_t index = 0; index < result.Scales.size(); ++index)
		{
			const auto& item = result.Scales[index];
			json += "    {\n";
			json += "      \"property\": "
				+ JsonEscape(item.Property) + ",\n";
			json += "      \"animationCount\": "
				+ std::to_string(item.AnimationCount) + ",\n";
			json += "      \"createInstallMicroseconds\": "
				+ FormatDouble(item.CreateInstallMicroseconds) + ",\n";
			json += "      \"beginMicroseconds\": "
				+ FormatDouble(item.BeginMicroseconds) + ",\n";
			json += "      \"advance\": ";
			AppendDistribution(json, item.Advance, "      ");
			json += ",\n      \"evaluatorFrameBudgetMicroseconds\": "
				+ FormatDouble(item.EvaluatorFrameBudgetMicroseconds) + ",\n";
			json += "      \"evaluatorFramesWithinBudget\": "
				+ std::to_string(item.EvaluatorFramesWithinBudget) + ",\n";
			json += "      \"evaluatorOnTimeRate\": "
				+ FormatDouble(item.EvaluatorOnTimeRate) + ",\n";
			json += "      \"restartReplace\": ";
			AppendDistribution(json, item.Restart, "      ");
			json += ",\n      \"stopMicroseconds\": "
				+ FormatDouble(item.StopMicroseconds) + ",\n";
			json += "      \"createBeginAdvanceRestartStopDestroy\": ";
			AppendDistribution(json, item.Lifecycle, "      ");
			json += ",\n      \"activeLeavesAfterBegin\": "
				+ std::to_string(item.ActiveLeavesAfterBegin) + ",\n";
			json += "      \"animationLayerStacksAfterBegin\": "
				+ std::to_string(item.AnimationLayerStacksAfterBegin) + ",\n";
			json += "      \"animationLayersAfterBegin\": "
				+ std::to_string(item.AnimationLayersAfterBegin) + ",\n";
			json += "      \"animationLayerMaxDepthAfterBegin\": "
				+ std::to_string(item.AnimationLayerMaxDepthAfterBegin) + ",\n";
			json += "      \"activeLeavesAfterRestarts\": "
				+ std::to_string(item.ActiveLeavesAfterRestarts) + ",\n";
			json += "      \"animationLayerStacksAfterRestarts\": "
				+ std::to_string(item.AnimationLayerStacksAfterRestarts) + ",\n";
			json += "      \"animationLayersAfterRestarts\": "
				+ std::to_string(item.AnimationLayersAfterRestarts) + ",\n";
			json += "      \"activeLeavesAfterStop\": "
				+ std::to_string(item.ActiveLeavesAfterStop) + ",\n";
			json += "      \"animationLayerStacksAfterStop\": "
				+ std::to_string(item.AnimationLayerStacksAfterStop) + ",\n";
			json += "      \"animationLayersAfterStop\": "
				+ std::to_string(item.AnimationLayersAfterStop) + ",\n";
			json += "      \"animationSlotsClearedAfterStop\": "
				+ std::string(item.AnimationSlotsClearedAfterStop
					? "true" : "false") + ",\n";
			json += "      \"memoryBefore\": ";
			AppendMemory(json, item.MemoryBefore, "      ");
			json += ",\n      \"memoryAfter\": ";
			AppendMemory(json, item.MemoryAfter, "      ");
			json += "\n    }";
			if (index + 1 < result.Scales.size()) json += ',';
			json += '\n';
		}
		json += "  ],\n  \"windowScanCases\": [\n";
		for (size_t index = 0; index < result.WindowTicks.size(); ++index)
		{
			const auto& item = result.WindowTicks[index];
			json += "    {\n";
			json += "      \"property\": "
				+ JsonEscape(item.Property) + ",\n";
			json += "      \"animationCount\": "
				+ std::to_string(item.AnimationCount) + ",\n";
			json += "      \"unrelatedVisualCount\": "
				+ std::to_string(item.UnrelatedVisualCount) + ",\n";
			json += "      \"approximateVisualCount\": "
				+ std::to_string(item.ApproximateVisualCount) + ",\n";
			json += "      \"windowTick\": ";
			AppendDistribution(json, item.WindowTick, "      ");
			json += "\n    }";
			if (index + 1 < result.WindowTicks.size()) json += ',';
			json += '\n';
		}
		json += "  ],\n  \"registryOnlyCases\": [\n";
		for (size_t index = 0; index < result.RegistryTicks.size(); ++index)
		{
			const auto& item = result.RegistryTicks[index];
			json += "    {\n";
			json += "      \"property\": "
				+ JsonEscape(item.Property) + ",\n";
			json += "      \"animationCount\": "
				+ std::to_string(item.AnimationCount) + ",\n";
			json += "      \"unrelatedVisualCount\": "
				+ std::to_string(item.UnrelatedVisualCount) + ",\n";
			json += "      \"approximateVisualCount\": "
				+ std::to_string(item.ApproximateVisualCount) + ",\n";
			json += "      \"registryTick\": ";
			AppendDistribution(json, item.RegistryTick, "      ");
			json += "\n    }";
			if (index + 1 < result.RegistryTicks.size()) json += ',';
			json += '\n';
		}
		json += "  ],\n  \"lifecycleRetention\": {\n";
		json += "    \"property\": "
			+ JsonEscape(result.Retention.Property) + ",\n";
		json += "    \"animationCount\": "
			+ std::to_string(result.Retention.AnimationCount) + ",\n";
		json += "    \"warmupCycles\": "
			+ std::to_string(result.Retention.WarmupCycles) + ",\n";
		json += "    \"cyclesPerPass\": "
			+ std::to_string(result.Retention.CyclesPerPass) + ",\n";
		json += "    \"before\": ";
		AppendMemory(json, result.Retention.Before, "    ");
		json += ",\n    \"afterPass1\": ";
		AppendMemory(json, result.Retention.AfterPass1, "    ");
		json += ",\n    \"afterPass2\": ";
		AppendMemory(json, result.Retention.AfterPass2, "    ");
		json += ",\n    \"pass1Microseconds\": "
			+ FormatDouble(result.Retention.Pass1Microseconds) + ",\n";
		json += "    \"pass2Microseconds\": "
			+ FormatDouble(result.Retention.Pass2Microseconds) + "\n";
		json += "  },\n  \"presentationCommitCadence\": {\n";
		json += "    \"property\": "
			+ JsonEscape(result.PresentationCadence.Property) + ",\n";
		json += "    \"animationCount\": "
			+ std::to_string(result.PresentationCadence.AnimationCount) + ",\n";
		json += "    \"warmupFrames\": "
			+ std::to_string(result.PresentationCadence.WarmupFrames) + ",\n";
		json += "    \"frameSamples\": "
			+ std::to_string(result.PresentationCadence.FrameSamples) + ",\n";
		json += "    \"frameBudgetMicroseconds\": "
			+ FormatDouble(
				result.PresentationCadence.FrameBudgetMicroseconds) + ",\n";
		json += "    \"committedFrameInterval\": ";
		AppendDistribution(json,
			result.PresentationCadence.CommittedFrameInterval, "    ");
		json += ",\n    \"framesDeliveredByDeadline\": "
			+ std::to_string(
				result.PresentationCadence.FramesDeliveredByDeadline) + ",\n";
		json += "    \"onTimeRate\": "
			+ FormatDouble(result.PresentationCadence.OnTimeRate) + ",\n";
		json += "    \"committedFramesBefore\": "
			+ std::to_string(
				result.PresentationCadence.CommittedFramesBefore) + ",\n";
		json += "    \"committedFramesAfter\": "
			+ std::to_string(
				result.PresentationCadence.CommittedFramesAfter) + ",\n";
		json += "    \"abortedFramesBefore\": "
			+ std::to_string(
				result.PresentationCadence.AbortedFramesBefore) + ",\n";
		json += "    \"abortedFramesAfter\": "
			+ std::to_string(
				result.PresentationCadence.AbortedFramesAfter) + "\n";
		json += "  },\n  \"processAtEnd\": ";
		AppendMemory(json, result.ProcessAtEnd, "  ");
		json += "\n}\n";
		return json;
	}

	std::string WideToUtf8(std::wstring_view value)
	{
		return Convert::UnicodeToUtf8(std::wstring(value));
	}

	BenchmarkCommandLine ParseArguments(
		std::span<const std::wstring_view> arguments)
	{
		BenchmarkCommandLine result;
		result.Requested = std::find(
			arguments.begin(), arguments.end(), L"--animation-benchmark")
			!= arguments.end();
		if (!result.Requested) return result;
		BenchmarkOptions options;
		bool modeSeen = false;
		bool outputSeen = false;
		bool profileSeen = false;
		for (size_t index = 0; index < arguments.size(); ++index)
		{
			const auto argument = arguments[index];
			if (argument == L"--animation-benchmark")
			{
				if (modeSeen)
				{
					result.Error = "--animation-benchmark may be specified only once.";
					return result;
				}
				modeSeen = true;
				continue;
			}
			if (argument != L"--animation-benchmark-output"
				&& argument != L"--animation-benchmark-profile")
			{
				result.Error = "Unknown animation benchmark option: "
					+ WideToUtf8(argument) + ".";
				return result;
			}
			if (index + 1 >= arguments.size() || arguments[index + 1].empty())
			{
				result.Error = "Missing animation benchmark option value.";
				return result;
			}
			const auto value = arguments[++index];
			if (argument == L"--animation-benchmark-output")
			{
				if (outputSeen)
				{
					result.Error = "--animation-benchmark-output may be specified once.";
					return result;
				}
				outputSeen = true;
				options.OutputPath = std::filesystem::path(value);
			}
			else
			{
				if (profileSeen)
				{
					result.Error = "--animation-benchmark-profile may be specified once.";
					return result;
				}
				profileSeen = true;
				options.Profile = WideToUtf8(value);
				if (options.Profile != "smoke" && options.Profile != "baseline")
				{
					result.Error = "Animation benchmark profile must be smoke or baseline.";
					return result;
				}
			}
		}
		if (!outputSeen)
		{
			result.Error = "--animation-benchmark-output is required.";
			return result;
		}
		result.Options = std::move(options);
		return result;
	}

	BenchmarkCommandLine ParseLiveCommandLine()
	{
		int argumentCount = 0;
		wchar_t** arguments = ::CommandLineToArgvW(
			::GetCommandLineW(), &argumentCount);
		if (!arguments || argumentCount < 1)
		{
			if (arguments) ::LocalFree(arguments);
			return {};
		}
		struct Owner final
		{
			wchar_t** Value = nullptr;
			~Owner() { if (Value) ::LocalFree(Value); }
		} owner{ arguments };
		std::vector<std::wstring_view> views;
		views.reserve(static_cast<size_t>(argumentCount - 1));
		for (int index = 1; index < argumentCount; ++index)
			views.emplace_back(arguments[index]);
		return ParseArguments(views);
	}

	bool IsUnderWorkplanRoot(const std::filesystem::path& output)
	{
		std::error_code error;
		const auto root = std::filesystem::absolute(
			std::filesystem::path("CUI-Workplans")
				/ "WPF-Animation-Alignment", error).lexically_normal();
		if (error) return false;
		const auto candidate = std::filesystem::absolute(
			output, error).lexically_normal();
		if (error || candidate == root) return false;
		auto relative = candidate.lexically_relative(root);
		return !relative.empty() && *relative.begin() != ".."
			&& candidate.extension() == ".json";
	}

	void WriteStandardStream(FILE* stream, const std::string& value)
	{
		const int descriptor = ::_fileno(stream);
		if (descriptor >= 0) (void)::_setmode(descriptor, _O_BINARY);
		if (std::fwrite(value.data(), 1, value.size(), stream) != value.size()
			|| std::fflush(stream) != 0)
			throw std::runtime_error("Could not write benchmark status output.");
	}

	void WriteOpacityPixelResultIfRequested(
		const wchar_t* environmentName,
		std::string_view scenario,
		double effectiveOpacity,
		const CapturedWindowFrame::Pixel& redOnly,
		const CapturedWindowFrame::Pixel& overlap,
		uint64_t surfaceDigest = 0)
	{
		size_t length = 0;
		if (::_wgetenv_s(&length, nullptr, 0, environmentName) != 0)
			throw std::runtime_error("Could not query opacity pixel output.");
		if (length <= 1u) return;
		std::vector<wchar_t> value(length);
		if (::_wgetenv_s(&length, value.data(), value.size(),
			environmentName) != 0)
			throw std::runtime_error("Could not read opacity pixel output.");
		const std::filesystem::path output(value.data());
		if (!IsUnderWorkplanRoot(output))
			throw std::runtime_error(
				"Opacity pixel output must be a JSON file under Workplans.");
		auto pixel = [](const CapturedWindowFrame::Pixel& item)
		{
			return std::string("{\"blue\":") + std::to_string(item.Blue)
				+ ",\"green\":" + std::to_string(item.Green)
				+ ",\"red\":" + std::to_string(item.Red)
				+ ",\"alpha\":" + std::to_string(item.Alpha) + "}";
		};
		const std::string json = std::string("{\n")
			+ "  \"schemaVersion\": 1,\n"
			+ "  \"engine\": \"CUI\",\n"
			+ "  \"scenario\": \"" + std::string(scenario) + "\",\n"
			+ "  \"effectiveOpacity\": "
			+ std::to_string(effectiveOpacity) + ",\n"
			+ "  \"redOnly\": " + pixel(redOnly) + ",\n"
			+ "  \"overlap\": " + pixel(overlap) + ",\n"
			+ "  \"surfaceBgraFnv64\": \""
			+ std::to_string(surfaceDigest) + "\"\n}\n";
		std::wstring error;
		if (!DesignerModel::AtomicFile::Write(output.wstring(), json, &error))
			throw std::runtime_error("Could not write CUI opacity pixel JSON: "
				+ Convert::UnicodeToUtf8(error));
	}
}

std::optional<int> TryRunAnimationPerformanceCommandLine()
{
	const auto parsed = ParseLiveCommandLine();
	if (!parsed.Requested) return std::nullopt;
	if (!parsed.Options)
	{
		try { WriteStandardStream(stderr, parsed.Error + "\n"); }
		catch (...) {}
		return 2;
	}
	try
	{
		const auto& options = *parsed.Options;
		if (!IsUnderWorkplanRoot(options.OutputPath))
			throw std::runtime_error(
				"Benchmark output must be a JSON file under "
				"CUI-Workplans/WPF-Animation-Alignment.");
		const auto result = RunBenchmark(ParametersFor(options.Profile));
		const auto json = Serialize(result);
		std::wstring error;
		if (!DesignerModel::AtomicFile::Write(
			options.OutputPath.wstring(), json, &error))
			throw std::runtime_error("Could not write benchmark JSON: "
				+ Convert::UnicodeToUtf8(error));
		if (const auto failure = AcceptanceFailure(result))
		{
			WriteStandardStream(stderr, *failure + "\n");
			return 1;
		}
		WriteStandardStream(stdout,
			"Animation benchmark completed: profile=" + options.Profile
			+ ", scaleCases=" + std::to_string(result.Scales.size())
			+ ", windowCases=" + std::to_string(result.WindowTicks.size())
			+ ", registryCases="
			+ std::to_string(result.RegistryTicks.size()) + ".\n");
		return 0;
	}
	catch (const std::exception& error)
	{
		try { WriteStandardStream(stderr, std::string(error.what()) + "\n"); }
		catch (...) {}
		return 1;
	}
	catch (...)
	{
		try { WriteStandardStream(stderr, "Unknown animation benchmark error.\n"); }
		catch (...) {}
		return 1;
	}
}

void RegisterAnimationPerformanceTests(cui::test::Runner& runner)
{
	runner.Add("Animation whole-Control Opacity validates and preserves input semantics", []
	{
		try
		{
			Control::RegisterDependencyProperties();
			Canvas control;
			CUI_EXPECT_NEAR(1.0, control.GetOpacity(), 0.000001);
			control.SetOpacity(0.0);
			CUI_EXPECT_NEAR(0.0, control.GetOpacity(), 0.000001);
			CUI_EXPECT_TRUE(control.ParticipatesInInputHitTesting());
			control.SetOpacity(-0.01);
			CUI_EXPECT_NEAR(0.0, control.GetOpacity(), 0.000001);
			control.SetOpacity(1.01);
			CUI_EXPECT_NEAR(0.0, control.GetOpacity(), 0.000001);
			control.SetOpacity((std::numeric_limits<double>::quiet_NaN)());
			CUI_EXPECT_NEAR(0.0, control.GetOpacity(), 0.000001);
		}
		catch (const std::exception& error)
		{
			throw std::runtime_error(std::string("Static Opacity contract: ")
				+ error.what());
		}

		try
		{
			BenchmarkScene animated(
				1u, 0u, BenchmarkPropertyKind::Opacity);
			const auto before = animated.TargetPresentationRevisions(0u);
			animated.Begin();
			animated.TickRegisteredWindow(BenchmarkClockOrigin + 500u);
			CUI_EXPECT_NEAR(
				0.5, animated.TargetOpacityForTesting(0u), 0.000001);
			const auto after = animated.TargetPresentationRevisions(0u);
			CUI_EXPECT_EQ(before.Content, after.Content);
			CUI_EXPECT_EQ(before.Geometry, after.Geometry);
			CUI_EXPECT_TRUE(after.Composition > before.Composition);
			animated.Remove();
			CUI_EXPECT_NEAR(
				1.0, animated.TargetOpacityForTesting(0u), 0.000001);
			CUI_EXPECT_TRUE(animated.AnimationSlotsCleared());
		}
		catch (const std::exception& error)
		{
			throw std::runtime_error(std::string("Animated Opacity contract: ")
				+ error.what());
		}
	});

	runner.Add("Animation retained Opacity composites the subtree as one group", []
	{
		BenchmarkScene scene(1u, 0u, BenchmarkPropertyKind::Opacity);
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetHostBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureTargetRectangleForTesting(
			0u, 24.0f, 16.0f, 40.0f, 30.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
		CUI_EXPECT_TRUE(scene.AddTargetChildRectangleForTesting(
			0u, 16.0f, 16.0f, 0.0f, 0.0f,
			D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f }) != nullptr);
		CUI_EXPECT_TRUE(scene.AddTargetChildRectangleForTesting(
			0u, 16.0f, 16.0f, 8.0f, 0.0f,
			D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f }) != nullptr);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 500u);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_FALSE(scene.PresentationRequiresCompositionForTesting());
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationFrameForTesting().OpacityLayerPushCount);

		auto capture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!capture.Error.empty())
			throw std::runtime_error("Retained opacity capture failed: "
				+ capture.Error);
		CapturedWindowFrame::Pixel redOnly;
		CapturedWindowFrame::Pixel overlap;
		CUI_EXPECT_TRUE(capture.TryGetPixel(44u, 58u, redOnly));
		CUI_EXPECT_TRUE(capture.TryGetPixel(52u, 58u, overlap));
		if (std::abs(static_cast<int>(redOnly.Red) - 128) > 2)
			throw std::runtime_error("Retained red-only BGRA was ("
				+ std::to_string(redOnly.Blue) + ","
				+ std::to_string(redOnly.Green) + ","
				+ std::to_string(redOnly.Red) + ","
				+ std::to_string(redOnly.Alpha) + ").");
		CUI_EXPECT_TRUE(std::abs(static_cast<int>(redOnly.Red) - 128) <= 2);
		CUI_EXPECT_TRUE(redOnly.Green <= 2u && redOnly.Blue <= 2u);
		CUI_EXPECT_TRUE(std::abs(static_cast<int>(overlap.Blue) - 128) <= 2);
		CUI_EXPECT_TRUE(overlap.Green <= 2u && overlap.Red <= 2u);
		CUI_EXPECT_EQ(255u, redOnly.Alpha);
		CUI_EXPECT_EQ(255u, overlap.Alpha);
		WriteOpacityPixelResultIfRequested(
			L"CUI_RETAINED_OPACITY_PIXEL_OUTPUT", "retained-at-500ms",
			0.5, redOnly, overlap);
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation managed opacity layers stay exact and bounded", []
	{
		constexpr size_t TargetCount = 32u;
		constexpr size_t SampleCount = 40u;
		BenchmarkScene scene(
			TargetCount, 0u, BenchmarkPropertyKind::TransformX);
		for (size_t index = 0u; index < TargetCount; ++index)
		{
			scene.SetTargetOpacityForTesting(index, 0.5);
			scene.ConfigureTargetRectangleForTesting(
				index, 12.0f, 12.0f,
				20.0f + static_cast<float>((index % 8u) * 16u),
				20.0f + static_cast<float>((index / 8u) * 16u));
		}
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin);
		scene.ForcePresentationUpdateForTesting();
		const auto first = scene.PresentationFrameForTesting();
		CUI_EXPECT_FALSE(scene.PresentationRequiresCompositionForTesting());
		CUI_EXPECT_EQ(TargetCount, first.OpacityLayerPushCount);
		CUI_EXPECT_TRUE(first.ImmediateDrawNodes >= TargetCount);

		std::vector<double> sceneRenderSamples;
		std::vector<double> totalSamples;
		sceneRenderSamples.reserve(SampleCount);
		totalSamples.reserve(SampleCount);
		size_t minimumLayerPushes =
			(std::numeric_limits<size_t>::max)();
		for (size_t sample = 0u; sample < SampleCount; ++sample)
		{
			for (size_t index = 0u; index < TargetCount; ++index)
				scene.SetTargetBackgroundForTesting(index,
					sample % 2u == 0u
					? D2D1_COLOR_F{ 0.8f, 0.2f, 0.1f, 1.0f }
					: D2D1_COLOR_F{ 0.1f, 0.3f, 0.8f, 1.0f });
			scene.TickRegisteredWindow(
				BenchmarkClockOrigin + 16u + sample * 16u);
			scene.ForcePresentationUpdateForTesting();
			const auto frame = scene.PresentationFrameForTesting();
			CUI_EXPECT_EQ(TargetCount, frame.OpacityLayerPushCount);
			CUI_EXPECT_TRUE(frame.ImmediateDrawNodes >= TargetCount);
			minimumLayerPushes = (std::min)(
				minimumLayerPushes, frame.OpacityLayerPushCount);
			sceneRenderSamples.push_back(
				frame.Timing.SceneRenderMicroseconds);
			totalSamples.push_back(frame.Timing.TotalMicroseconds);
		}
		const auto sceneRender = Summarize(std::move(sceneRenderSamples));
		const auto total = Summarize(std::move(totalSamples));

		size_t outputLength = 0u;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_OPACITY_LAYER_PERFORMANCE_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query managed opacity layer output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_OPACITY_LAYER_PERFORMANCE_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read managed opacity layer output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Managed opacity layer output must be under Workplans.");
			auto distribution = [](const Distribution& value)
			{
				return std::string("{\"count\":")
					+ std::to_string(value.Count)
					+ ",\"meanMicroseconds\":"
					+ FormatDouble(value.MeanMicroseconds)
					+ ",\"p50Microseconds\":"
					+ FormatDouble(value.P50Microseconds)
					+ ",\"p95Microseconds\":"
					+ FormatDouble(value.P95Microseconds)
					+ ",\"maximumMicroseconds\":"
					+ FormatDouble(value.MaximumMicroseconds) + "}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\":1,\n"
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"managed-opacity-layer\",\n"
				+ "  \"targetCount\":" + std::to_string(TargetCount) + ",\n"
				+ "  \"sampleCount\":" + std::to_string(SampleCount) + ",\n"
				+ "  \"firstLayerPushes\":"
				+ std::to_string(first.OpacityLayerPushCount) + ",\n"
				+ "  \"minimumSteadyLayerPushes\":"
				+ std::to_string(minimumLayerPushes) + ",\n"
				+ "  \"sceneRender\":" + distribution(sceneRender) + ",\n"
				+ "  \"total\":" + distribution(total) + ",\n"
				+ "  \"result\":\"PASS\"\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write managed opacity layer result: "
					+ Convert::UnicodeToUtf8(writeError));
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation static Opacity updates retained isolation topology", []
	{
		BenchmarkScene scene(1u, 0u, BenchmarkPropertyKind::Opacity);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		PresentationNodeSnapshot snapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, snapshot));
		CUI_EXPECT_FALSE(snapshot.CompositionIsolated);

		scene.SetTargetOpacityForTesting(0u, 0.25);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, snapshot));
		CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
		CUI_EXPECT_NEAR(0.25f, snapshot.CompositionOpacity, 0.000001f);

		scene.SetTargetOpacityForTesting(0u, 1.0);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, snapshot));
		CUI_EXPECT_FALSE(snapshot.CompositionIsolated);
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation DComp Opacity publishes visual-only group alpha", []
	{
		BenchmarkScene scene(1u, 0u, BenchmarkPropertyKind::Opacity);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetHostBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureTargetRectangleForTesting(
			0u, 24.0f, 16.0f, 40.0f, 30.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
		CUI_EXPECT_TRUE(scene.AddTargetChildRectangleForTesting(
			0u, 16.0f, 16.0f, 0.0f, 0.0f,
			D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f }) != nullptr);
		CUI_EXPECT_TRUE(scene.AddTargetChildRectangleForTesting(
			0u, 16.0f, 16.0f, 8.0f, 0.0f,
			D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f }) != nullptr);
		scene.Begin();
		scene.ForcePresentationUpdateForTesting();

		PresentationNodeSnapshot snapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, snapshot));
		CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
		CUI_EXPECT_TRUE(snapshot.CompositionIsolationRoot);
		CUI_EXPECT_NEAR(1.0f, snapshot.CompositionOpacity, 0.000001f);
		UINT width = 0;
		UINT height = 0;
		uint64_t firstSurfaceDigest = 0;
		size_t firstNonTransparent = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, firstSurfaceDigest, firstNonTransparent));

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 500u);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, snapshot));
		CUI_EXPECT_NEAR(0.5f, snapshot.CompositionOpacity, 0.000001f);
		const auto compositionOnly = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, compositionOnly.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, compositionOnly.CommandRecordedNodes);
		uint64_t secondSurfaceDigest = 0;
		size_t secondNonTransparent = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, secondSurfaceDigest, secondNonTransparent));
		CUI_EXPECT_EQ(firstSurfaceDigest, secondSurfaceDigest);
		CUI_EXPECT_EQ(firstNonTransparent, secondNonTransparent);

		auto capture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!capture.Error.empty())
			throw std::runtime_error("DComp opacity capture failed: "
				+ capture.Error);
		CapturedWindowFrame::Pixel redOnly;
		CapturedWindowFrame::Pixel overlap;
		CUI_EXPECT_TRUE(capture.TryGetPixel(44u, 58u, redOnly));
		CUI_EXPECT_TRUE(capture.TryGetPixel(52u, 58u, overlap));
		CUI_EXPECT_TRUE(std::abs(static_cast<int>(redOnly.Red) - 128) <= 2);
		CUI_EXPECT_TRUE(redOnly.Green <= 2u && redOnly.Blue <= 2u);
		CUI_EXPECT_TRUE(std::abs(static_cast<int>(overlap.Blue) - 128) <= 2);
		CUI_EXPECT_TRUE(overlap.Green <= 2u && overlap.Red <= 2u);
		CUI_EXPECT_EQ(255u, redOnly.Alpha);
		CUI_EXPECT_EQ(255u, overlap.Alpha);
		WriteOpacityPixelResultIfRequested(
			L"CUI_DCOMP_OPACITY_PIXEL_OUTPUT", "dcomp-at-500ms",
			0.5, redOnly, overlap, secondSurfaceDigest);

		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		const auto recoveredFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(0ULL, recoveredFrame.CompositionOnlySegments);
		CUI_EXPECT_TRUE(recoveredFrame.CommandRecordedNodes >= 1u);
		auto recovered = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!recovered.Error.empty())
			throw std::runtime_error("Recovered opacity capture failed: "
				+ recovered.Error);
		CapturedWindowFrame::Pixel recoveredOverlap;
		CUI_EXPECT_TRUE(recovered.TryGetPixel(52u, 58u, recoveredOverlap));
		CUI_EXPECT_TRUE(
			std::abs(static_cast<int>(recoveredOverlap.Blue) - 128) <= 2);
		CUI_EXPECT_TRUE(recoveredOverlap.Green <= 2u
			&& recoveredOverlap.Red <= 2u);
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation isolated content damage preserves disjoint siblings", []
	{
		BenchmarkScene scene(1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 40.0f, 16.0f, 40.0f, 30.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
		auto* first = scene.AddTargetChildRectangleForTesting(
			0u, 12.0f, 12.0f, 0.0f, 0.0f,
			D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
		CUI_EXPECT_TRUE(first != nullptr);
		CUI_EXPECT_TRUE(scene.AddTargetChildRectangleForTesting(
			0u, 12.0f, 12.0f, 24.0f, 0.0f,
			D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f }) != nullptr);
		scene.Begin();
		scene.ForcePresentationUpdateForTesting();

		UINT width = 0;
		UINT height = 0;
		uint64_t initialDigest = 0;
		size_t initialOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, initialDigest, initialOpaque));
		CUI_EXPECT_TRUE(initialOpaque >= 288u);

		first->Background = D2D1_COLOR_F{ 0.0f, 1.0f, 0.0f, 1.0f };
		scene.ForcePresentationUpdateForTesting();
		const auto frame = scene.PresentationFrameForTesting();
		uint64_t updatedDigest = 0;
		size_t updatedOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, updatedDigest, updatedOpaque));
		CUI_EXPECT_EQ(initialOpaque, updatedOpaque);
		CUI_EXPECT_TRUE(initialDigest != updatedDigest);
		CUI_EXPECT_EQ(1ULL, frame.CommandRecordedNodes);
		CUI_EXPECT_TRUE(frame.DamageReplayNodes >= 1u);
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation nested Opacity uses shared parent and leaf effects", []
	{
		BenchmarkScene scene(2u, 0u, BenchmarkPropertyKind::Opacity);
		scene.NestTargetForTesting(1u, 0u);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureTargetRectangleForTesting(
			0u, 32.0f, 20.0f, 40.0f, 30.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 16.0f, 16.0f, 8.0f, 0.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			1u, D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f });
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 500u);
		scene.ForcePresentationUpdateForTesting();

		PresentationNodeSnapshot outer;
		PresentationNodeSnapshot inner;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, outer));
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			1u, inner));
		CUI_EXPECT_TRUE(outer.CompositionIsolated);
		CUI_EXPECT_TRUE(inner.CompositionIsolated);
		CUI_EXPECT_EQ(1ULL, outer.CompositionIsolationDepth);
		CUI_EXPECT_EQ(2ULL, inner.CompositionIsolationDepth);
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationFrameForTesting().CompositionOnlySegments);

		auto capture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!capture.Error.empty())
			throw std::runtime_error("Nested opacity capture failed: "
				+ capture.Error);
		CapturedWindowFrame::Pixel outerOnly;
		CapturedWindowFrame::Pixel overlap;
		CUI_EXPECT_TRUE(capture.TryGetPixel(44u, 58u, outerOnly));
		CUI_EXPECT_TRUE(capture.TryGetPixel(52u, 58u, overlap));
		CUI_EXPECT_TRUE(std::abs(static_cast<int>(outerOnly.Red) - 128) <= 2);
		CUI_EXPECT_TRUE(outerOnly.Green <= 2u && outerOnly.Blue <= 2u);
		CUI_EXPECT_TRUE(std::abs(static_cast<int>(overlap.Red) - 64) <= 2);
		CUI_EXPECT_TRUE(overlap.Green <= 2u);
		CUI_EXPECT_TRUE(std::abs(static_cast<int>(overlap.Blue) - 64) <= 2);
		CUI_EXPECT_EQ(255u, outerOnly.Alpha);
		CUI_EXPECT_EQ(255u, overlap.Alpha);
		WriteOpacityPixelResultIfRequested(
			L"CUI_NESTED_OPACITY_PIXEL_OUTPUT", "nested-at-500ms",
			0.25, outerOnly, overlap);
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation nested shared opacity groups compose parent effects", []
	{
		struct Sample final
		{
			int At = 0;
			CapturedWindowFrame::Pixel OuterOnly;
			CapturedWindowFrame::Pixel InnerBackground;
			CapturedWindowFrame::Pixel InnerChild;
			CapturedWindowFrame::Pixel Trailing;
			RECT ChildBounds{};
			size_t ChildPixels = 0;
		};
		auto capture = [](BenchmarkScene& scene, int at)
		{
			scene.TickRegisteredWindow(
				BenchmarkClockOrigin + static_cast<unsigned long long>(at));
			scene.ForcePresentationUpdateForTesting();
			auto frame = CaptureWindowComposition(
				scene.NativeWindowHandleForTesting());
			if (!frame.Error.empty())
				throw std::runtime_error(
					"Nested shared opacity capture failed: " + frame.Error);
			Sample result;
			result.At = at;
			CUI_EXPECT_TRUE(frame.TryGetPixel(24u, 60u, result.OuterOnly));
			CUI_EXPECT_TRUE(frame.TryGetPixel(
				60u, 60u, result.InnerBackground));
			const UINT offset = static_cast<UINT>(at / 10);
			CUI_EXPECT_TRUE(frame.TryGetPixel(
				52u + offset, 60u, result.InnerChild));
			CUI_EXPECT_TRUE(frame.TryGetPixel(204u, 62u, result.Trailing));
			CUI_EXPECT_TRUE(frame.TryGetColorBounds(
				result.InnerChild.Blue, result.InnerChild.Green,
				result.InnerChild.Red, result.ChildBounds,
				result.ChildPixels, 2u));
			CUI_EXPECT_TRUE(result.Trailing.Red >= 253u
				&& result.Trailing.Green >= 253u
				&& result.Trailing.Blue <= 2u
				&& result.Trailing.Alpha == 255u);
			return result;
		};

		const std::array propertyKinds{
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::TransformX };
		BenchmarkScene scene(
			3u, 0u, BenchmarkPropertyKind::Opacity,
			1u, false, false, false, propertyKinds);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.NestTargetForTesting(1u, 0u);
		scene.NestTargetForTesting(2u, 1u);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureTargetRectangleForTesting(
			0u, 160.0f, 24.0f, 20.0f, 30.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 100.0f, 20.0f, 20.0f, 2.0f);
		scene.ConfigureTargetRectangleForTesting(
			2u, 24.0f, 20.0f, 8.0f, 0.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			1u, D2D1_COLOR_F{ 0.0f, 1.0f, 0.0f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			2u, D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f });
		CUI_EXPECT_TRUE(scene.AddHostChildRectangleForTesting(
			8.0f, 16.0f, 200.0f, 34.0f,
			D2D1_COLOR_F{ 1.0f, 1.0f, 0.0f, 1.0f }) != nullptr);
		scene.Begin();
		const auto first = capture(scene, 500);
		CUI_EXPECT_EQ(127u, first.OuterOnly.Red);
		CUI_EXPECT_EQ(0u, first.OuterOnly.Green);
		CUI_EXPECT_EQ(0u, first.OuterOnly.Blue);
		CUI_EXPECT_EQ(64u, first.InnerBackground.Red);
		CUI_EXPECT_EQ(63u, first.InnerBackground.Green);
		CUI_EXPECT_EQ(0u, first.InnerBackground.Blue);
		CUI_EXPECT_EQ(64u, first.InnerChild.Red);
		CUI_EXPECT_EQ(0u, first.InnerChild.Green);
		CUI_EXPECT_EQ(63u, first.InnerChild.Blue);
		CUI_EXPECT_EQ(480ULL, first.ChildPixels);
		CUI_EXPECT_EQ(98L, first.ChildBounds.left);
		CUI_EXPECT_EQ(56L, first.ChildBounds.top);
		CUI_EXPECT_EQ(122L, first.ChildBounds.right);
		CUI_EXPECT_EQ(76L, first.ChildBounds.bottom);
		const auto initialGroups =
			scene.PresentationOpacityGroupCountForTesting();
		if (initialGroups != 2u)
			throw std::runtime_error(
				"Nested shared topology expected two groups, observed "
				+ std::to_string(initialGroups) + ".");
		std::array<uint64_t, 3> digests{};
		for (size_t index = 0; index < 3u; ++index)
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
				index, snapshot));
			CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
			CUI_EXPECT_EQ(index + 1u, snapshot.CompositionIsolationDepth);
			UINT width = 0;
			UINT height = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				index, width, height, digests[index], opaque));
			CUI_EXPECT_TRUE(width > 0u && height > 0u && opaque > 0u);
		}
		const auto second = capture(scene, 700);
		CUI_EXPECT_EQ(76u, second.OuterOnly.Red);
		CUI_EXPECT_EQ(53u, second.InnerBackground.Red);
		CUI_EXPECT_EQ(23u, second.InnerBackground.Green);
		CUI_EXPECT_EQ(0u, second.InnerBackground.Blue);
		CUI_EXPECT_EQ(53u, second.InnerChild.Red);
		CUI_EXPECT_EQ(0u, second.InnerChild.Green);
		CUI_EXPECT_EQ(23u, second.InnerChild.Blue);
		CUI_EXPECT_EQ(480ULL, second.ChildPixels);
		CUI_EXPECT_EQ(118L, second.ChildBounds.left);
		CUI_EXPECT_EQ(56L, second.ChildBounds.top);
		CUI_EXPECT_EQ(142L, second.ChildBounds.right);
		CUI_EXPECT_EQ(76L, second.ChildBounds.bottom);
		const auto secondFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(2ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(3ULL, secondFrame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, secondFrame.CommandRecordedNodes);
		for (size_t index = 0; index < 3u; ++index)
		{
			UINT width = 0;
			UINT height = 0;
			uint64_t digest = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				index, width, height, digest, opaque));
			CUI_EXPECT_EQ(digests[index], digest);
		}

		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		const auto recovered = capture(scene, 700);
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		CUI_EXPECT_EQ(second.OuterOnly.Red, recovered.OuterOnly.Red);
		CUI_EXPECT_EQ(second.InnerBackground.Red,
			recovered.InnerBackground.Red);
		CUI_EXPECT_EQ(second.InnerBackground.Green,
			recovered.InnerBackground.Green);
		CUI_EXPECT_EQ(second.InnerChild.Red, recovered.InnerChild.Red);
		CUI_EXPECT_EQ(second.InnerChild.Blue, recovered.InnerChild.Blue);
		CUI_EXPECT_EQ(second.ChildPixels, recovered.ChildPixels);
		CUI_EXPECT_TRUE(::EqualRect(
			&second.ChildBounds, &recovered.ChildBounds));

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_NESTED_SHARED_OPACITY_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query nested shared opacity output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_NESTED_SHARED_OPACITY_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read nested shared opacity output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Nested shared opacity output must be under Workplans.");
			auto pixelJson = [](const CapturedWindowFrame::Pixel& value)
			{
				return std::string("{\"blue\":")
					+ std::to_string(value.Blue) + ",\"green\":"
					+ std::to_string(value.Green) + ",\"red\":"
					+ std::to_string(value.Red) + ",\"alpha\":"
					+ std::to_string(value.Alpha) + "}";
			};
			auto sampleJson = [&pixelJson](const Sample& value)
			{
				return std::string("{\"atMilliseconds\":")
					+ std::to_string(value.At) + ",\"outerOnly\":"
					+ pixelJson(value.OuterOnly) + ",\"innerBackground\":"
					+ pixelJson(value.InnerBackground) + ",\"innerChild\":"
					+ pixelJson(value.InnerChild) + ",\"trailingSibling\":"
					+ pixelJson(value.Trailing) + ",\"innerChildRegion\":{"
					+ "\"matchingPixels\":"
					+ std::to_string(value.ChildPixels) + ",\"bounds\":{"
					+ "\"left\":" + std::to_string(value.ChildBounds.left)
					+ ",\"top\":" + std::to_string(value.ChildBounds.top)
					+ ",\"right\":" + std::to_string(value.ChildBounds.right)
					+ ",\"bottom\":" + std::to_string(value.ChildBounds.bottom)
					+ "}}}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\": 1,\n  \"engine\": \"CUI\",\n"
				+ "  \"scenario\": \"nested-shared-opacity-groups\",\n"
				+ "  \"samples\": [" + sampleJson(first) + ","
				+ sampleJson(second) + "],\n"
				+ "  \"opacityGroupCount\": 2,\n"
				+ "  \"secondFrameCompositionOnlySegments\": 3,\n"
				+ "  \"surfaceDigestsStable\": true,\n"
				+ "  \"deviceRecoveryMatched\": true,\n"
				+ "  \"comparisonSpace\": \"rgb-over-opaque-black\"\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error(
					"Could not write nested shared opacity JSON: "
					+ Convert::UnicodeToUtf8(error));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation nested shared opacity topology is reversible", []
	{
		BenchmarkScene scene(3u, 0u, BenchmarkPropertyKind::TransformX);
		scene.NestTargetForTesting(1u, 0u);
		scene.NestTargetForTesting(2u, 1u);
		scene.SetTargetOpacityForTesting(0u, 0.5);
		scene.SetTargetOpacityForTesting(1u, 0.5);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 100.0f, 40.0f, 20.0f, 30.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 60.0f, 30.0f, 10.0f, 5.0f);
		scene.ConfigureTargetRectangleForTesting(
			2u, 20.0f, 20.0f, 5.0f, 5.0f);
		scene.SetTargetClipToBoundsForTesting(0u, true);
		scene.SetTargetClipToBoundsForTesting(1u, true);
		scene.Begin();
		CUI_EXPECT_EQ(0.5, scene.TargetOpacityForTesting(0u));
		CUI_EXPECT_EQ(0.5, scene.TargetOpacityForTesting(1u));
		scene.ForcePresentationUpdateForTesting();
		const auto initialTopologyGroups =
			scene.PresentationOpacityGroupCountForTesting();
		if (initialTopologyGroups != 2u)
			throw std::runtime_error(
				"Nested shared topology expected two groups, observed "
				+ std::to_string(initialTopologyGroups) + ".");
		const auto aborts = scene.PresentationAbortedFrameCount();
		scene.SetTargetOpacityForTesting(1u, 1.0);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());
		scene.SetTargetOpacityForTesting(1u, 0.5);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(2ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(aborts, scene.PresentationAbortedFrameCount());
		for (size_t index = 0; index < 3u; ++index)
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
				index, snapshot));
			CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
			CUI_EXPECT_EQ(index + 1u, snapshot.CompositionIsolationDepth);
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation disjoint shared opacity groups stay global siblings", []
	{
		const std::array propertyKinds{
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::TransformX,
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::TransformX };
		BenchmarkScene scene(
			4u, 0u, BenchmarkPropertyKind::Opacity,
			1u, false, false, false, propertyKinds);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.NestTargetForTesting(1u, 0u);
		scene.NestTargetForTesting(3u, 2u);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 64.0f, 20.0f, 20.0f, 30.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 16.0f, 16.0f, 8.0f, 2.0f);
		scene.ConfigureTargetRectangleForTesting(
			2u, 64.0f, 20.0f, 140.0f, 30.0f);
		scene.ConfigureTargetRectangleForTesting(
			3u, 16.0f, 16.0f, 8.0f, 2.0f);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 500u);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(2ULL,
			scene.PresentationOpacityGroupCountForTesting());
		std::array<uint64_t, 4> digests{};
		for (size_t index = 0; index < 4u; ++index)
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
				index, snapshot));
			CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
			CUI_EXPECT_EQ(index % 2u == 0u ? 1u : 2u,
				snapshot.CompositionIsolationDepth);
			UINT width = 0;
			UINT height = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				index, width, height, digests[index], opaque));
		}
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 700u);
		scene.ForcePresentationUpdateForTesting();
		const auto frame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(4ULL, frame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, frame.CommandRecordedNodes);
		for (size_t index = 0; index < 4u; ++index)
		{
			UINT width = 0;
			UINT height = 0;
			uint64_t digest = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				index, width, height, digest, opaque));
			CUI_EXPECT_EQ(digests[index], digest);
		}
		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation depth-three composite DComp chain applies each root once", []
	{
		BenchmarkScene scene(3u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.NestTargetForTesting(1u, 0u);
		scene.NestTargetForTesting(2u, 1u);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 64.0f, 44.0f, 80.0f, 30.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 36.0f, 28.0f, 12.0f, 8.0f);
		scene.ConfigureTargetRectangleForTesting(
			2u, 12.0f, 12.0f, 10.0f, 6.0f);
		scene.SetTargetBackgroundForTesting(0u,
			D2D1_COLOR_F{ 0.15f, 0.45f, 0.85f, 1.0f });
		scene.SetTargetBackgroundForTesting(1u,
			D2D1_COLOR_F{ 0.2f, 0.7f, 0.3f, 1.0f });
		scene.SetTargetBackgroundForTesting(2u,
			D2D1_COLOR_F{ 0.8f, 0.2f, 0.1f, 1.0f });
		for (size_t index = 0; index < 3u; ++index)
			scene.ConfigureTargetCompositeTransformForTesting(index);
		scene.Begin();

		struct Sample final
		{
			int At = 0;
			RECT Outer{};
			RECT Middle{};
			RECT Inner{};
			size_t OuterPixels = 0;
			size_t MiddlePixels = 0;
			size_t InnerPixels = 0;
			uint64_t Digest = 0;
		};
		auto captureAt = [&](int at)
		{
			scene.TickRegisteredWindow(
				BenchmarkClockOrigin + static_cast<unsigned long long>(at));
			scene.ForcePresentationUpdateForTesting();
			auto capture = CaptureWindowComposition(
				scene.NativeWindowHandleForTesting());
			if (!capture.Error.empty())
				throw std::runtime_error("Depth-three capture failed: "
					+ capture.Error);
			Sample sample;
			sample.At = at;
			sample.Digest = capture.Digest;
			CUI_EXPECT_TRUE(capture.TryGetColorBounds(
				217u, 115u, 38u, sample.Outer,
				sample.OuterPixels, 12u));
			CUI_EXPECT_TRUE(capture.TryGetColorBounds(
				76u, 179u, 51u, sample.Middle,
				sample.MiddlePixels, 12u));
			CUI_EXPECT_TRUE(capture.TryGetColorBounds(
				26u, 51u, 204u, sample.Inner,
				sample.InnerPixels, 12u));
			return sample;
		};

		const auto first = captureAt(80);
		std::array<uint64_t, 3> surfaceDigests{};
		for (size_t index = 0; index < 3u; ++index)
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
				index, snapshot));
			CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
			CUI_EXPECT_TRUE(snapshot.CompositionIsolationRoot);
			CUI_EXPECT_EQ(index + 1u, snapshot.CompositionIsolationDepth);
			UINT width = 0;
			UINT height = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				index, width, height, surfaceDigests[index], opaque));
			CUI_EXPECT_TRUE(width > 0u && height > 0u && opaque > 0u);
		}
		const auto second = captureAt(160);
		const auto secondFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(3ULL, secondFrame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, secondFrame.CommandRecordedNodes);
		for (size_t index = 0; index < 3u; ++index)
		{
			UINT width = 0;
			UINT height = 0;
			uint64_t digest = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				index, width, height, digest, opaque));
			CUI_EXPECT_EQ(surfaceDigests[index], digest);
		}

		auto within = [](LONG actual, LONG expected)
			{ return std::abs(actual - expected) <= 1; };
		CUI_EXPECT_TRUE(within(first.Outer.left, 79)
			&& within(first.Outer.top, 47)
			&& within(first.Outer.right, 163)
			&& within(first.Outer.bottom, 114));
		CUI_EXPECT_TRUE(within(first.Middle.left, 101)
			&& within(first.Middle.top, 67)
			&& within(first.Middle.right, 151)
			&& within(first.Middle.bottom, 110));
		CUI_EXPECT_TRUE(within(first.Inner.left, 123)
			&& within(first.Inner.top, 88)
			&& within(first.Inner.right, 138)
			&& within(first.Inner.bottom, 105));
		CUI_EXPECT_TRUE(within(second.Outer.left, 88)
			&& within(second.Outer.top, 53)
			&& within(second.Outer.right, 171)
			&& within(second.Outer.bottom, 109));
		CUI_EXPECT_TRUE(within(second.Middle.left, 117)
			&& within(second.Middle.top, 81)
			&& within(second.Middle.right, 167)
			&& within(second.Middle.bottom, 122));
		CUI_EXPECT_TRUE(within(second.Inner.left, 144)
			&& within(second.Inner.top, 112)
			&& within(second.Inner.right, 159)
			&& within(second.Inner.bottom, 129));
		CUI_EXPECT_TRUE(std::abs(static_cast<long long>(first.OuterPixels)
			- 1615LL) <= 30LL);
		CUI_EXPECT_TRUE(std::abs(static_cast<long long>(first.MiddlePixels)
			- 687LL) <= 20LL);
		CUI_EXPECT_TRUE(std::abs(static_cast<long long>(first.InnerPixels)
			- 96LL) <= 10LL);
		CUI_EXPECT_TRUE(std::abs(static_cast<long long>(second.OuterPixels)
			- 1766LL) <= 30LL);
		CUI_EXPECT_TRUE(std::abs(static_cast<long long>(second.MiddlePixels)
			- 735LL) <= 20LL);
		CUI_EXPECT_TRUE(std::abs(static_cast<long long>(second.InnerPixels)
			- 96LL) <= 10LL);

		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		const auto recovered = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!recovered.Error.empty())
			throw std::runtime_error("Depth-three recovery capture failed: "
				+ recovered.Error);
		RECT recoveredInner{};
		size_t recoveredInnerPixels = 0;
		CUI_EXPECT_TRUE(recovered.TryGetColorBounds(
			26u, 51u, 204u, recoveredInner, recoveredInnerPixels, 12u));
		CUI_EXPECT_EQ(second.Inner.left, recoveredInner.left);
		CUI_EXPECT_EQ(second.Inner.top, recoveredInner.top);
		CUI_EXPECT_EQ(second.Inner.right, recoveredInner.right);
		CUI_EXPECT_EQ(second.Inner.bottom, recoveredInner.bottom);
		CUI_EXPECT_EQ(second.InnerPixels, recoveredInnerPixels);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_DEPTH3_PIXEL_OUTPUT") != 0)
			throw std::runtime_error("Could not query depth-three output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(), L"CUI_DCOMP_DEPTH3_PIXEL_OUTPUT") != 0)
				throw std::runtime_error("Could not read depth-three output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Depth-three output must be under Workplans.");
			auto rect = [](const RECT& value)
			{
				return std::string("{\"left\":") + std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			auto sample = [&rect](const Sample& value)
			{
				return std::string("{\"atMilliseconds\":")
					+ std::to_string(value.At)
					+ ",\"outer\":{\"matchingPixels\":"
					+ std::to_string(value.OuterPixels) + ",\"bounds\":"
					+ rect(value.Outer) + "},\"middle\":{\"matchingPixels\":"
					+ std::to_string(value.MiddlePixels) + ",\"bounds\":"
					+ rect(value.Middle) + "},\"inner\":{\"matchingPixels\":"
					+ std::to_string(value.InnerPixels) + ",\"bounds\":"
					+ rect(value.Inner) + "}}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\": 1,\n  \"engine\": \"CUI\",\n"
				+ "  \"scenario\": \"depth-three-translate-scale-rotate-origin\",\n"
				+ "  \"samples\": [" + sample(first) + "," + sample(second)
				+ "],\n  \"secondFrameCompositionOnlySegments\": 3,\n"
				+ "  \"surfaceDigestsStable\": true,\n"
				+ "  \"deviceRecoveryMatched\": true\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error("Could not write depth-three JSON: "
					+ Convert::UnicodeToUtf8(error));
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation Opacity Transform cross matrix preserves WPF groups", []
	{
		struct CrossSample final
		{
			std::string Scenario;
			int At = 0;
			CapturedWindowFrame::Pixel RedOnly;
			CapturedWindowFrame::Pixel Overlap;
			RECT Bounds{};
			size_t RedPixels = 0;
			size_t BluePixels = 0;
		};
		std::vector<CrossSample> samples;
		auto capture = [&](BenchmarkScene& scene,
			std::string scenario,
			int at,
			UINT redX,
			UINT overlapX,
			uint8_t expectedChannel)
		{
			scene.TickRegisteredWindow(
				BenchmarkClockOrigin + static_cast<unsigned long long>(at));
			scene.ForcePresentationUpdateForTesting();
			auto frame = CaptureWindowComposition(
				scene.NativeWindowHandleForTesting());
			if (!frame.Error.empty())
				throw std::runtime_error("Opacity/Transform capture failed: "
					+ frame.Error);
			CrossSample result;
			result.Scenario = std::move(scenario);
			result.At = at;
			CUI_EXPECT_TRUE(frame.TryGetPixel(redX, 58u, result.RedOnly));
			CUI_EXPECT_TRUE(frame.TryGetPixel(
				overlapX, 58u, result.Overlap));
			CUI_EXPECT_TRUE(std::abs(static_cast<int>(result.RedOnly.Red)
				- expectedChannel) <= 2);
			CUI_EXPECT_TRUE(result.RedOnly.Blue <= 2u
				&& result.RedOnly.Green <= 2u);
			CUI_EXPECT_TRUE(std::abs(static_cast<int>(result.Overlap.Blue)
				- expectedChannel) <= 2);
			CUI_EXPECT_TRUE(result.Overlap.Red <= 2u
				&& result.Overlap.Green <= 2u);
			CUI_EXPECT_EQ(255u, result.RedOnly.Alpha);
			CUI_EXPECT_EQ(255u, result.Overlap.Alpha);
			RECT redBounds{};
			RECT blueBounds{};
			CUI_EXPECT_TRUE(frame.TryGetColorBounds(
				0u, 0u, expectedChannel,
				redBounds, result.RedPixels, 2u));
			CUI_EXPECT_TRUE(frame.TryGetColorBounds(
				expectedChannel, 0u, 0u,
				blueBounds, result.BluePixels, 2u));
			result.Bounds = RECT{
				(std::min)(redBounds.left, blueBounds.left),
				(std::min)(redBounds.top, blueBounds.top),
				(std::max)(redBounds.right, blueBounds.right),
				(std::max)(redBounds.bottom, blueBounds.bottom) };
			samples.push_back(result);
			return result;
		};

		const std::array transformThenOpacity{
			BenchmarkPropertyKind::TransformX,
			BenchmarkPropertyKind::Opacity };
		{
			BenchmarkScene scene(
				2u, 0u, BenchmarkPropertyKind::TransformX,
				1u, false, false, false, transformThenOpacity);
			scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
			scene.NestTargetForTesting(1u, 0u);
			scene.AddNativeCompositionBoundaryForTesting();
			scene.ShowOffscreenWithoutActivationForTesting();
			scene.SetRootBackgroundForTesting(
				D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
			scene.ConfigureTargetRectangleForTesting(
				0u, 24.0f, 16.0f, 40.0f, 30.0f);
			scene.ConfigureTargetRectangleForTesting(
				1u, 24.0f, 16.0f, 0.0f, 0.0f);
			scene.SetTargetBackgroundForTesting(
				0u, D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
			scene.SetTargetBackgroundForTesting(
				1u, D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
			CUI_EXPECT_TRUE(scene.AddTargetChildRectangleForTesting(
				1u, 16.0f, 16.0f, 0.0f, 0.0f,
				D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f }) != nullptr);
			CUI_EXPECT_TRUE(scene.AddTargetChildRectangleForTesting(
				1u, 16.0f, 16.0f, 8.0f, 0.0f,
				D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f }) != nullptr);
			scene.Begin();
			const auto first = capture(scene,
				"transform-ancestor-opacity", 500, 94u, 102u, 128u);
			CUI_EXPECT_EQ(90L, first.Bounds.left);
			CUI_EXPECT_EQ(54L, first.Bounds.top);
			CUI_EXPECT_EQ(114L, first.Bounds.right);
			CUI_EXPECT_EQ(70L, first.Bounds.bottom);
			std::array<uint64_t, 2> digests{};
			for (size_t index = 0; index < 2u; ++index)
			{
				PresentationNodeSnapshot snapshot;
				CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
					index, snapshot));
				CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
				CUI_EXPECT_EQ(index + 1u, snapshot.CompositionIsolationDepth);
				UINT width = 0;
				UINT height = 0;
				size_t opaque = 0;
				CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
					index, width, height, digests[index], opaque));
			}
			const auto second = capture(scene,
				"transform-ancestor-opacity", 700, 114u, 122u, 77u);
			CUI_EXPECT_EQ(110L, second.Bounds.left);
			CUI_EXPECT_EQ(134L, second.Bounds.right);
			const auto secondFrame = scene.PresentationFrameForTesting();
			CUI_EXPECT_EQ(2ULL, secondFrame.CompositionOnlySegments);
			CUI_EXPECT_EQ(0ULL, secondFrame.CommandRecordedNodes);
			for (size_t index = 0; index < 2u; ++index)
			{
				UINT width = 0;
				UINT height = 0;
				uint64_t digest = 0;
				size_t opaque = 0;
				CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
					index, width, height, digest, opaque));
				CUI_EXPECT_EQ(digests[index], digest);
			}
			scene.HideOffscreenPresentationForTesting();
		}

		const std::array opacityThenTransform{
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::TransformX };
		{
			BenchmarkScene scene(
				2u, 0u, BenchmarkPropertyKind::Opacity,
				1u, false, false, false, opacityThenTransform);
			scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
			scene.NestTargetForTesting(1u, 0u);
			scene.AddNativeCompositionBoundaryForTesting();
			scene.ShowOffscreenWithoutActivationForTesting();
			scene.SetRootBackgroundForTesting(
				D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
			scene.ConfigureTargetRectangleForTesting(
				0u, 140.0f, 16.0f, 20.0f, 30.0f);
			scene.ConfigureTargetRectangleForTesting(
				1u, 24.0f, 16.0f, 8.0f, 0.0f);
			scene.SetTargetBackgroundForTesting(
				0u, D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
			scene.SetTargetBackgroundForTesting(
				1u, D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f });
			scene.Begin();
			const auto first = capture(scene,
				"opacity-ancestor-transform", 500, 24u, 82u, 128u);
			CUI_EXPECT_EQ(20L, first.Bounds.left);
			CUI_EXPECT_EQ(160L, first.Bounds.right);
			std::array<uint64_t, 2> digests{};
			for (size_t index = 0; index < 2u; ++index)
			{
				PresentationNodeSnapshot snapshot;
				CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
					index, snapshot));
				CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
				CUI_EXPECT_EQ(index + 1u, snapshot.CompositionIsolationDepth);
				UINT width = 0;
				UINT height = 0;
				size_t opaque = 0;
				CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
					index, width, height, digests[index], opaque));
			}
			CUI_EXPECT_EQ(1ULL,
				scene.PresentationOpacityGroupCountForTesting());
			const auto second = capture(scene,
				"opacity-ancestor-transform", 700, 24u, 102u, 77u);
			CUI_EXPECT_EQ(20L, second.Bounds.left);
			CUI_EXPECT_EQ(160L, second.Bounds.right);
			const auto secondFrame = scene.PresentationFrameForTesting();
			CUI_EXPECT_EQ(2ULL, secondFrame.CompositionOnlySegments);
			CUI_EXPECT_EQ(0ULL, secondFrame.CommandRecordedNodes);
			for (size_t index = 0; index < 2u; ++index)
			{
				UINT width = 0;
				UINT height = 0;
				uint64_t digest = 0;
				size_t opaque = 0;
				CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
					index, width, height, digest, opaque));
				CUI_EXPECT_EQ(digests[index], digest);
			}
			scene.HideOffscreenPresentationForTesting();
		}

		{
			BenchmarkScene scene(
				2u, 0u, BenchmarkPropertyKind::TransformX,
				1u, true, true, false, transformThenOpacity);
			scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
			scene.AddNativeCompositionBoundaryForTesting();
			scene.ShowOffscreenWithoutActivationForTesting();
			scene.SetRootBackgroundForTesting(
				D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
			scene.ConfigureTargetRectangleForTesting(
				0u, 24.0f, 16.0f, 40.0f, 30.0f);
			scene.SetTargetBackgroundForTesting(
				0u, D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
			scene.ConfigureTargetRectangleForTesting(
				1u, 1.0f, 1.0f, 300.0f, 180.0f);
			scene.SetTargetBackgroundForTesting(
				1u, D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
			CUI_EXPECT_TRUE(scene.AddTargetChildRectangleForTesting(
				0u, 16.0f, 16.0f, 0.0f, 0.0f,
				D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f }) != nullptr);
			CUI_EXPECT_TRUE(scene.AddTargetChildRectangleForTesting(
				0u, 16.0f, 16.0f, 8.0f, 0.0f,
				D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f }) != nullptr);
			scene.Begin();
			const auto first = capture(scene,
				"same-target-transform-opacity", 500, 94u, 102u, 128u);
			CUI_EXPECT_EQ(90L, first.Bounds.left);
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
				0u, snapshot));
			CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
			CUI_EXPECT_EQ(1ULL, snapshot.CompositionIsolationDepth);
			CUI_EXPECT_NEAR(0.5f, snapshot.CompositionOpacity, 0.000001f);
			UINT width = 0;
			UINT height = 0;
			uint64_t digest = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				0u, width, height, digest, opaque));
			const auto second = capture(scene,
				"same-target-transform-opacity", 700, 114u, 122u, 77u);
			CUI_EXPECT_EQ(110L, second.Bounds.left);
			const auto secondFrame = scene.PresentationFrameForTesting();
			CUI_EXPECT_EQ(1ULL, secondFrame.CompositionOnlySegments);
			CUI_EXPECT_EQ(0ULL, secondFrame.CommandRecordedNodes);
			uint64_t movedDigest = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				0u, width, height, movedDigest, opaque));
			CUI_EXPECT_EQ(digest, movedDigest);
			const auto recoveries =
				scene.PresentationDeviceRecoveryCountForTesting();
			scene.InjectPresentationDeviceLossForTesting();
			const auto recovered = capture(scene,
				"same-target-transform-opacity-recovered",
				700, 114u, 122u, 77u);
			samples.pop_back();
			CUI_EXPECT_EQ(recoveries + 1u,
				scene.PresentationDeviceRecoveryCountForTesting());
			CUI_EXPECT_EQ(second.Bounds.left, recovered.Bounds.left);
			CUI_EXPECT_EQ(second.Bounds.top, recovered.Bounds.top);
			CUI_EXPECT_EQ(second.Bounds.right, recovered.Bounds.right);
			CUI_EXPECT_EQ(second.Bounds.bottom, recovered.Bounds.bottom);
			CUI_EXPECT_EQ(second.RedOnly.Red, recovered.RedOnly.Red);
			CUI_EXPECT_EQ(second.Overlap.Blue, recovered.Overlap.Blue);
			scene.HideOffscreenPresentationForTesting();
		}

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_OPACITY_TRANSFORM_OUTPUT") != 0)
			throw std::runtime_error("Could not query cross-matrix output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_OPACITY_TRANSFORM_OUTPUT") != 0)
				throw std::runtime_error("Could not read cross-matrix output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Cross-matrix output must be under Workplans.");
			auto pixel = [](const CapturedWindowFrame::Pixel& value)
			{
				return std::string("{\"blue\":") + std::to_string(value.Blue)
					+ ",\"green\":" + std::to_string(value.Green)
					+ ",\"red\":" + std::to_string(value.Red)
					+ ",\"alpha\":" + std::to_string(value.Alpha) + "}";
			};
			auto rect = [](const RECT& value)
			{
				return std::string("{\"left\":") + std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			std::string json = "{\n  \"schemaVersion\": 1,\n"
				"  \"engine\": \"CUI\",\n  \"samples\": [\n";
			for (size_t index = 0; index < samples.size(); ++index)
			{
				const auto& value = samples[index];
				json += "    {\"scenario\":\"" + value.Scenario
					+ "\",\"atMilliseconds\":" + std::to_string(value.At)
					+ ",\"redOnly\":" + pixel(value.RedOnly)
					+ ",\"overlap\":" + pixel(value.Overlap)
					+ ",\"bounds\":" + rect(value.Bounds) + "}"
					+ (index + 1u == samples.size() ? "\n" : ",\n");
			}
			json += "  ],\n"
				"  \"comparisonSpace\": \"premultiplied-rgb-over-opaque-black\",\n"
				"  \"maxRgbChannelDifferenceFromWpf\": 1,\n"
				"  \"matchedWpfRgbWithin8Bit\": true\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error("Could not write cross-matrix JSON: "
					+ Convert::UnicodeToUtf8(error));
		}
	});

	runner.Add("Animation shared parent opacity groups moving sibling surfaces", []
	{
		struct Sample final
		{
			int At = 0;
			CapturedWindowFrame::Pixel RedOnly;
			CapturedWindowFrame::Pixel GreenOnly;
			CapturedWindowFrame::Pixel BlueOnly;
			CapturedWindowFrame::Pixel TripleOverlap;
			CapturedWindowFrame::Pixel TrailingSibling;
			RECT Bounds{};
		};
		std::vector<Sample> samples;
		auto capture = [&](BenchmarkScene& scene, int at,
			uint8_t expectedChannel)
		{
			scene.TickRegisteredWindow(
				BenchmarkClockOrigin + static_cast<unsigned long long>(at));
			scene.ForcePresentationUpdateForTesting();
			auto frame = CaptureWindowComposition(
				scene.NativeWindowHandleForTesting());
			if (!frame.Error.empty())
				throw std::runtime_error("Shared opacity capture failed: "
					+ frame.Error);
			const UINT offset = static_cast<UINT>(at / 10);
			Sample result;
			result.At = at;
			CUI_EXPECT_TRUE(frame.TryGetPixel(24u, 58u, result.RedOnly));
			CUI_EXPECT_TRUE(frame.TryGetPixel(
				30u + offset, 58u, result.GreenOnly));
			CUI_EXPECT_TRUE(frame.TryGetPixel(
				50u + offset, 58u, result.BlueOnly));
			CUI_EXPECT_TRUE(frame.TryGetPixel(
				38u + offset, 58u, result.TripleOverlap));
			CUI_EXPECT_TRUE(frame.TryGetPixel(
				92u, 58u, result.TrailingSibling));
			CUI_EXPECT_TRUE(std::abs(static_cast<int>(result.RedOnly.Red)
				- expectedChannel) <= 2);
			CUI_EXPECT_TRUE(result.RedOnly.Green <= 2u
				&& result.RedOnly.Blue <= 2u);
			CUI_EXPECT_TRUE(std::abs(static_cast<int>(result.GreenOnly.Green)
				- expectedChannel) <= 2);
			CUI_EXPECT_TRUE(result.GreenOnly.Red <= 2u
				&& result.GreenOnly.Blue <= 2u);
			CUI_EXPECT_TRUE(std::abs(static_cast<int>(result.BlueOnly.Blue)
				- expectedChannel) <= 2);
			CUI_EXPECT_TRUE(result.BlueOnly.Red <= 2u
				&& result.BlueOnly.Green <= 2u);
			CUI_EXPECT_TRUE(std::abs(static_cast<int>(
				result.TripleOverlap.Blue) - expectedChannel) <= 2);
			CUI_EXPECT_TRUE(result.TripleOverlap.Red <= 2u
				&& result.TripleOverlap.Green <= 2u);
			CUI_EXPECT_TRUE(result.TrailingSibling.Red >= 253u
				&& result.TrailingSibling.Green >= 253u
				&& result.TrailingSibling.Blue <= 2u
				&& result.TrailingSibling.Alpha == 255u);
			RECT redBounds{};
			RECT greenBounds{};
			RECT blueBounds{};
			size_t pixels = 0;
			CUI_EXPECT_TRUE(frame.TryGetColorBounds(
				0u, 0u, expectedChannel, redBounds, pixels, 2u));
			CUI_EXPECT_TRUE(frame.TryGetColorBounds(
				0u, expectedChannel, 0u, greenBounds, pixels, 2u));
			CUI_EXPECT_TRUE(frame.TryGetColorBounds(
				expectedChannel, 0u, 0u, blueBounds, pixels, 2u));
			result.Bounds = RECT{
				(std::min)({ redBounds.left, greenBounds.left, blueBounds.left }),
				(std::min)({ redBounds.top, greenBounds.top, blueBounds.top }),
				(std::max)({ redBounds.right, greenBounds.right, blueBounds.right }),
				(std::max)({ redBounds.bottom, greenBounds.bottom, blueBounds.bottom }) };
			CUI_EXPECT_EQ(20L, result.Bounds.left);
			CUI_EXPECT_EQ(54L, result.Bounds.top);
			CUI_EXPECT_EQ(160L, result.Bounds.right);
			CUI_EXPECT_EQ(70L, result.Bounds.bottom);
			samples.push_back(result);
			return result;
		};

		const std::array propertyKinds{
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::TransformX,
			BenchmarkPropertyKind::TransformX };
		BenchmarkScene scene(
			3u, 0u, BenchmarkPropertyKind::Opacity,
			1u, false, false, false, propertyKinds);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.NestTargetForTesting(1u, 0u);
		scene.NestTargetForTesting(2u, 0u);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureTargetRectangleForTesting(
			0u, 140.0f, 16.0f, 20.0f, 30.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 16.0f, 16.0f, 8.0f, 0.0f);
		scene.ConfigureTargetRectangleForTesting(
			2u, 16.0f, 16.0f, 16.0f, 0.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			1u, D2D1_COLOR_F{ 0.0f, 1.0f, 0.0f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			2u, D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f });
		CUI_EXPECT_TRUE(scene.AddHostChildRectangleForTesting(
			8.0f, 16.0f, 90.0f, 30.0f,
			D2D1_COLOR_F{ 1.0f, 1.0f, 0.0f, 1.0f }) != nullptr);
		scene.Begin();
		const auto first = capture(scene, 500, 128u);
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());
		std::array<uint64_t, 3> digests{};
		for (size_t index = 0; index < 3u; ++index)
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
				index, snapshot));
			CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
			CUI_EXPECT_TRUE(snapshot.CompositionIsolationRoot);
			CUI_EXPECT_EQ(index == 0u ? 1u : 2u,
				snapshot.CompositionIsolationDepth);
			UINT width = 0;
			UINT height = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				index, width, height, digests[index], opaque));
			CUI_EXPECT_TRUE(width > 0u && height > 0u && opaque > 0u);
		}
		const auto second = capture(scene, 700, 77u);
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());
		const auto secondFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(3ULL, secondFrame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, secondFrame.CommandRecordedNodes);
		for (size_t index = 0; index < 3u; ++index)
		{
			UINT width = 0;
			UINT height = 0;
			uint64_t digest = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				index, width, height, digest, opaque));
			CUI_EXPECT_EQ(digests[index], digest);
		}
		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		const auto recovered = capture(scene, 700, 77u);
		samples.pop_back();
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(second.RedOnly.Red, recovered.RedOnly.Red);
		CUI_EXPECT_EQ(second.GreenOnly.Green, recovered.GreenOnly.Green);
		CUI_EXPECT_EQ(second.BlueOnly.Blue, recovered.BlueOnly.Blue);
		CUI_EXPECT_EQ(second.TripleOverlap.Blue,
			recovered.TripleOverlap.Blue);
		CUI_EXPECT_EQ(second.TrailingSibling.Red,
			recovered.TrailingSibling.Red);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_SHARED_OPACITY_OUTPUT") != 0)
			throw std::runtime_error("Could not query shared opacity output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(), L"CUI_DCOMP_SHARED_OPACITY_OUTPUT") != 0)
				throw std::runtime_error("Could not read shared opacity output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Shared opacity output must be under Workplans.");
			auto pixel = [](const CapturedWindowFrame::Pixel& value)
			{
				return std::string("{\"blue\":") + std::to_string(value.Blue)
					+ ",\"green\":" + std::to_string(value.Green)
					+ ",\"red\":" + std::to_string(value.Red)
					+ ",\"alpha\":" + std::to_string(value.Alpha) + "}";
			};
			auto sample = [&pixel](const Sample& value)
			{
				return std::string("{\"atMilliseconds\":")
					+ std::to_string(value.At)
					+ ",\"redOnly\":" + pixel(value.RedOnly)
					+ ",\"greenOnly\":" + pixel(value.GreenOnly)
					+ ",\"blueOnly\":" + pixel(value.BlueOnly)
					+ ",\"tripleOverlap\":" + pixel(value.TripleOverlap)
					+ ",\"trailingSibling\":" + pixel(value.TrailingSibling)
					+ ",\"bounds\":{\"left\":"
					+ std::to_string(value.Bounds.left) + ",\"top\":"
					+ std::to_string(value.Bounds.top) + ",\"right\":"
					+ std::to_string(value.Bounds.right) + ",\"bottom\":"
					+ std::to_string(value.Bounds.bottom) + "}}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\": 1,\n  \"engine\": \"CUI\",\n"
				+ "  \"scenario\": \"shared-parent-opacity-moving-siblings\",\n"
				+ "  \"samples\": [" + sample(first) + "," + sample(second)
				+ "],\n  \"opacityGroupCount\": 1,\n"
				+ "  \"secondFrameCompositionOnlySegments\": 3,\n"
				+ "  \"surfaceDigestsStable\": true,\n"
				+ "  \"deviceRecoveryMatched\": true,\n"
				+ "  \"comparisonSpace\": \"premultiplied-rgb-over-opaque-black\"\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error("Could not write shared opacity JSON: "
					+ Convert::UnicodeToUtf8(error));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		scene.HideOffscreenPresentationForTesting();

		const std::array nativeKinds{
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::TransformX };
		BenchmarkScene native(
			2u, 0u, BenchmarkPropertyKind::Opacity,
			1u, false, false, false, nativeKinds);
		native.NestTargetForTesting(1u, 0u);
		native.AddTargetNativeCompositionBoundaryForTesting(0u);
		native.ShowOffscreenWithoutActivationForTesting();
		native.ConfigureTargetRectangleForTesting(
			0u, 80.0f, 24.0f, 20.0f, 30.0f);
		native.ConfigureTargetRectangleForTesting(
			1u, 16.0f, 16.0f, 8.0f, 0.0f);
		native.Begin();
		const auto aborts = native.PresentationAbortedFrameCount();
		native.TickRegisteredWindow(BenchmarkClockOrigin + 500u);
		native.ForcePresentationUpdateForTesting();
		PresentationNodeSnapshot outer;
		PresentationNodeSnapshot inner;
		CUI_EXPECT_TRUE(native.TryGetTargetPresentationSnapshotForTesting(
			0u, outer));
		CUI_EXPECT_TRUE(native.TryGetTargetPresentationSnapshotForTesting(
			1u, inner));
		CUI_EXPECT_FALSE(outer.CompositionIsolated);
		CUI_EXPECT_FALSE(inner.CompositionIsolated);
		CUI_EXPECT_EQ(0ULL,
			native.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_TRUE(native.PresentationAbortedFrameCount() > aborts);
		native.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation shared opacity group owns pixel native visual lease", []
	{
		struct Sample final
		{
			int At = 0;
			CapturedWindowFrame::Pixel RedOnly;
			CapturedWindowFrame::Pixel GreenOnly;
			CapturedWindowFrame::Pixel BlueOverlap;
			CapturedWindowFrame::Pixel Trailing;
			RECT GreenBounds{};
			RECT BlueBounds{};
			size_t GreenPixels = 0;
			size_t BluePixels = 0;
		};
		auto capture = [](BenchmarkScene& scene, int at,
			uint8_t expectedChannel)
		{
			scene.TickRegisteredWindow(
				BenchmarkClockOrigin + static_cast<unsigned long long>(at));
			scene.ForcePresentationUpdateForTesting();
			auto frame = CaptureWindowComposition(
				scene.NativeWindowHandleForTesting());
			if (!frame.Error.empty())
				throw std::runtime_error(
					"Mixed native opacity capture failed: " + frame.Error);
			Sample result;
			result.At = at;
			CUI_EXPECT_TRUE(frame.TryGetPixel(24u, 60u, result.RedOnly));
			CUI_EXPECT_TRUE(frame.TryGetPixel(
				at == 500 ? 79u : 110u, 60u, result.GreenOnly));
			CUI_EXPECT_TRUE(frame.TryGetPixel(100u, 60u, result.BlueOverlap));
			CUI_EXPECT_TRUE(frame.TryGetPixel(204u, 62u, result.Trailing));
			CUI_EXPECT_TRUE(frame.TryGetColorBounds(
				0u, expectedChannel, 0u,
				result.GreenBounds, result.GreenPixels, 2u));
			CUI_EXPECT_TRUE(frame.TryGetColorBounds(
				expectedChannel, 0u, 0u,
				result.BlueBounds, result.BluePixels, 2u));
			return result;
		};

		const std::array propertyKinds{
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::TransformX };
		BenchmarkScene scene(
			2u, 0u, BenchmarkPropertyKind::Opacity,
			1u, false, false, false, propertyKinds);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.NestTargetForTesting(1u, 0u);
		auto* native = scene.AddTargetPixelNativeCompositionChildForTesting(
			0u, 24.0f, 20.0f, 60.0f, 0.0f,
			D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f });
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureTargetRectangleForTesting(
			0u, 140.0f, 20.0f, 20.0f, 30.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 24.0f, 20.0f, 8.0f, 0.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			1u, D2D1_COLOR_F{ 0.0f, 1.0f, 0.0f, 1.0f });
		CUI_EXPECT_TRUE(scene.AddHostChildRectangleForTesting(
			8.0f, 16.0f, 200.0f, 34.0f,
			D2D1_COLOR_F{ 1.0f, 1.0f, 0.0f, 1.0f }) != nullptr);
		scene.Begin();
		const auto first = capture(scene, 500, 127u);
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());
		CUI_EXPECT_EQ(1ULL, native->VisualGenerationForTesting());
		CUI_EXPECT_EQ(127u, first.RedOnly.Red);
		CUI_EXPECT_EQ(127u, first.GreenOnly.Green);
		CUI_EXPECT_EQ(127u, first.BlueOverlap.Blue);
		CUI_EXPECT_TRUE(first.BlueOverlap.Red <= 2u
			&& first.BlueOverlap.Green <= 2u);
		CUI_EXPECT_TRUE(first.Trailing.Red >= 253u
			&& first.Trailing.Green >= 253u
			&& first.Trailing.Blue <= 2u);
		const RECT firstGreenBounds{ 78, 54, 80, 74 };
		const RECT expectedBlueBounds{ 80, 54, 104, 74 };
		CUI_EXPECT_EQ(40ULL, first.GreenPixels);
		CUI_EXPECT_EQ(480ULL, first.BluePixels);
		CUI_EXPECT_TRUE(::EqualRect(
			&first.GreenBounds, &firstGreenBounds));
		CUI_EXPECT_TRUE(::EqualRect(
			&first.BlueBounds, &expectedBlueBounds));
		std::array<uint64_t, 2> digests{};
		for (size_t index = 0; index < 2u; ++index)
		{
			UINT width = 0;
			UINT height = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				index, width, height, digests[index], opaque));
		}

		const auto second = capture(scene, 700, 76u);
		const auto secondFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(2ULL, secondFrame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, secondFrame.CommandRecordedNodes);
		CUI_EXPECT_EQ(0ULL, secondFrame.NativeCommitNodes);
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());
		CUI_EXPECT_EQ(76u, second.RedOnly.Red);
		CUI_EXPECT_EQ(76u, second.GreenOnly.Green);
		CUI_EXPECT_EQ(76u, second.BlueOverlap.Blue);
		CUI_EXPECT_TRUE(second.BlueOverlap.Red <= 2u
			&& second.BlueOverlap.Green <= 2u);
		const RECT secondGreenBounds{ 104, 54, 122, 74 };
		CUI_EXPECT_EQ(360ULL, second.GreenPixels);
		CUI_EXPECT_EQ(480ULL, second.BluePixels);
		CUI_EXPECT_TRUE(::EqualRect(
			&second.GreenBounds, &secondGreenBounds));
		CUI_EXPECT_TRUE(::EqualRect(
			&second.BlueBounds, &expectedBlueBounds));
		for (size_t index = 0; index < 2u; ++index)
		{
			UINT width = 0;
			UINT height = 0;
			uint64_t digest = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				index, width, height, digest, opaque));
			CUI_EXPECT_EQ(digests[index], digest);
		}

		native->RebindForTesting();
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());
		const auto rebound = capture(scene, 700, 76u);
		CUI_EXPECT_EQ(2ULL, native->VisualGenerationForTesting());
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());
		CUI_EXPECT_EQ(second.BlueOverlap.Blue, rebound.BlueOverlap.Blue);
		CUI_EXPECT_EQ(second.BluePixels, rebound.BluePixels);
		CUI_EXPECT_TRUE(::EqualRect(&second.BlueBounds, &rebound.BlueBounds));

		const auto committedBeforePrepareFailure =
			scene.PresentationCommittedFrameCount();
		native->FailNextPrepareForTesting();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(committedBeforePrepareFailure,
			scene.PresentationCommittedFrameCount());
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());
		const auto recoveredPrepare = capture(scene, 700, 76u);
		CUI_EXPECT_TRUE(scene.PresentationCommittedFrameCount()
			> committedBeforePrepareFailure);
		CUI_EXPECT_EQ(second.BlueOverlap.Blue,
			recoveredPrepare.BlueOverlap.Blue);
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());

		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		const auto recoveredDevice = capture(scene, 700, 76u);
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		CUI_EXPECT_TRUE(native->VisualGenerationForTesting() >= 3u);
		CUI_EXPECT_EQ(second.BlueOverlap.Blue,
			recoveredDevice.BlueOverlap.Blue);
		CUI_EXPECT_EQ(second.BluePixels, recoveredDevice.BluePixels);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_MIXED_NATIVE_OPACITY_OUTPUT") != 0)
			throw std::runtime_error("Could not query mixed native output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_MIXED_NATIVE_OPACITY_OUTPUT") != 0)
				throw std::runtime_error("Could not read mixed native output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Mixed native opacity output must be under Workplans.");
			auto pixelJson = [](const CapturedWindowFrame::Pixel& value)
			{
				return std::string("{\"blue\":")
					+ std::to_string(value.Blue) + ",\"green\":"
					+ std::to_string(value.Green) + ",\"red\":"
					+ std::to_string(value.Red) + ",\"alpha\":"
					+ std::to_string(value.Alpha) + "}";
			};
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left) + ",\"top\":"
					+ std::to_string(value.top) + ",\"right\":"
					+ std::to_string(value.right) + ",\"bottom\":"
					+ std::to_string(value.bottom) + "}";
			};
			auto sampleJson = [&pixelJson, &boundsJson](const Sample& value)
			{
				return std::string("{\"atMilliseconds\":")
					+ std::to_string(value.At) + ",\"redOnly\":"
					+ pixelJson(value.RedOnly) + ",\"greenOnly\":"
					+ pixelJson(value.GreenOnly) + ",\"blueOverlap\":"
					+ pixelJson(value.BlueOverlap) + ",\"trailingSibling\":"
					+ pixelJson(value.Trailing) + ",\"greenRegion\":{"
					+ "\"matchingPixels\":" + std::to_string(value.GreenPixels)
					+ ",\"bounds\":" + boundsJson(value.GreenBounds)
					+ "},\"blueRegion\":{\"matchingPixels\":"
					+ std::to_string(value.BluePixels) + ",\"bounds\":"
					+ boundsJson(value.BlueBounds) + "}}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\": 1,\n  \"engine\": \"CUI\",\n"
				+ "  \"scenario\": \"mixed-scene-native-opacity-group\",\n"
				+ "  \"samples\": [" + sampleJson(first) + ","
				+ sampleJson(second) + "],\n"
				+ "  \"opacityGroupCount\": 1,\n"
				+ "  \"groupedNativeVisualCount\": 1,\n"
				+ "  \"secondFrameCompositionOnlySegments\": 2,\n"
				+ "  \"surfaceDigestsStable\": true,\n"
				+ "  \"rebindMatched\": true,\n"
				+ "  \"prepareFailureRecovered\": true,\n"
				+ "  \"deviceRecoveryMatched\": true,\n"
				+ "  \"comparisonSpace\": \"rgb-over-opaque-black\"\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error("Could not write mixed native JSON: "
					+ Convert::UnicodeToUtf8(error));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation shared opacity group owns live WebBrowser content", []
	{
		struct Sample final
		{
			int At = 0;
			CapturedWindowFrame::Pixel Blue;
			RECT Bounds{};
			size_t Pixels = 0;
			CapturedWindowFrame::Pixel Trailing;
			RECT TrailingBounds{};
			size_t TrailingPixels = 0;
		};
		auto pumpUntil = [](const std::function<bool()>& predicate,
			DWORD timeoutMilliseconds)
		{
			const auto started = ::GetTickCount64();
			for (;;)
			{
				cui::PumpUIThreadCallbacks();
				MSG message{};
				while (::PeekMessageW(
					&message, nullptr, 0u, 0u, PM_REMOVE) != FALSE)
				{
					if (message.message == WM_QUIT)
					{
						::PostQuitMessage(static_cast<int>(message.wParam));
						throw std::runtime_error(
							"Live WebBrowser authority observed WM_QUIT.");
					}
					::TranslateMessage(&message);
					::DispatchMessageW(&message);
					cui::PumpUIThreadCallbacks();
				}
				if (predicate()) return true;
				if (::GetTickCount64() - started >= timeoutMilliseconds)
					return false;
				const auto wait = ::MsgWaitForMultipleObjectsEx(
					0, nullptr, 25u, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
				if (wait == WAIT_FAILED)
					throw std::runtime_error(
						"Live WebBrowser authority message wait failed.");
			}
		};
		auto capture = [&](BenchmarkScene& scene, int at,
			uint8_t expectedChannel, bool expectTrailingRoot = false)
		{
			Sample result;
			result.At = at;
			CapturedWindowFrame frame;
			bool matched = false;
			for (unsigned attempt = 0; attempt < 20u && !matched; ++attempt)
			{
				scene.TickRegisteredWindow(
					BenchmarkClockOrigin
					+ static_cast<unsigned long long>(at));
				scene.ForcePresentationUpdateForTesting();
				frame = CaptureWindowComposition(
					scene.NativeWindowHandleForTesting());
				if (!frame.Error.empty())
					throw std::runtime_error(
						"Live WebBrowser capture failed: " + frame.Error);
				CUI_EXPECT_TRUE(frame.TryGetPixel(100u, 60u, result.Blue));
				matched = std::abs(static_cast<int>(result.Blue.Blue)
					- static_cast<int>(expectedChannel)) <= 2
					&& result.Blue.Green <= 2u && result.Blue.Red <= 2u;
				if (!matched) (void)pumpUntil([] { return false; }, 50u);
			}
			if (!matched)
				throw std::runtime_error(
					"Live WebBrowser pixels did not reach the shared group: b="
					+ std::to_string(result.Blue.Blue) + ", g="
					+ std::to_string(result.Blue.Green) + ", r="
					+ std::to_string(result.Blue.Red) + ".");
			CUI_EXPECT_TRUE(frame.TryGetColorBounds(
				expectedChannel, 0u, 0u,
				result.Bounds, result.Pixels, 2u));
			if (expectTrailingRoot)
			{
				CUI_EXPECT_TRUE(frame.TryGetPixel(
					204u, 108u, result.Trailing));
				CUI_EXPECT_EQ(255u, result.Trailing.Red);
				CUI_EXPECT_EQ(0u, result.Trailing.Green);
				CUI_EXPECT_EQ(255u, result.Trailing.Blue);
				CUI_EXPECT_TRUE(frame.TryGetColorBounds(
					255u, 0u, 255u, result.TrailingBounds,
					result.TrailingPixels, 0u));
			}
			return result;
		};

		const std::array propertyKinds{
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::TransformX };
		BenchmarkScene scene(
			2u, 0u, BenchmarkPropertyKind::Opacity,
			1u, false, false, false, propertyKinds);
		scene.NestTargetForTesting(1u, 0u);
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureTargetRectangleForTesting(
			0u, 140.0f, 20.0f, 20.0f, 30.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 24.0f, 20.0f, 8.0f, 0.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			1u, D2D1_COLOR_F{ 0.0f, 1.0f, 0.0f, 1.0f });
		auto* browser = scene.AddTargetWebBrowserChildForTesting(
			0u, 24.0f, 20.0f, 60.0f, 0.0f);
		browser->DefaultBackgroundColor =
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f };
		bool navigationCompleted = false;
		bool navigationSucceeded = false;
		bool domLoaded = false;
		auto navigationConnection = browser->OnNavigationCompleted.Subscribe(
			[&](WebBrowser*, const WebBrowser::NavigationCompletedArgs& args)
			{
				navigationCompleted = true;
				navigationSucceeded = args.IsSuccess;
			});
		auto domConnection = browser->OnDOMContentLoaded.Subscribe(
			[&](WebBrowser*, const WebBrowser::DomContentLoadedArgs&)
			{ domLoaded = true; });
		CUI_EXPECT_TRUE(browser->TrySetHtml(
			LR"HTML(<!doctype html><html><head><meta charset="utf-8"><title>cui-live-blue</title><style>html,body{margin:0;width:100%;height:100%;overflow:hidden;background:#0000ff}</style></head><body></body></html>)HTML"));
		CUI_EXPECT_TRUE(scene.AddHostChildRectangleForTesting(
			8.0f, 16.0f, 200.0f, 34.0f,
			D2D1_COLOR_F{ 1.0f, 1.0f, 0.0f, 1.0f }) != nullptr);
		scene.Begin();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_TRUE(pumpUntil([&]
			{
				return browser->GetInitializationState()
					== WebBrowser::InitializationState::Failed
					|| (browser->IsWebViewReady()
						&& navigationCompleted && domLoaded);
			}, 30'000u));
		if (browser->GetInitializationState()
			== WebBrowser::InitializationState::Failed)
			throw std::runtime_error(
				"Live WebBrowser initialization failed: init="
				+ std::to_string(browser->GetLastInitializationError())
				+ ", environment="
				+ std::to_string(browser->GetLastEnvironmentError())
				+ ", controller="
				+ std::to_string(browser->GetLastControllerError()) + ".");
		CUI_EXPECT_TRUE(browser->IsWebViewReady());
		CUI_EXPECT_TRUE(navigationCompleted && navigationSucceeded && domLoaded);
		CUI_EXPECT_EQ(std::wstring(L"cui-live-blue"),
			browser->GetDocumentTitle());
		CUI_EXPECT_TRUE(navigationConnection.Connected());
		CUI_EXPECT_TRUE(domConnection.Connected());

		bool scriptCompleted = false;
		HRESULT scriptResult = E_PENDING;
		browser->ExecuteScriptAsync(
			L"document.body.getBoundingClientRect().width",
			[&](HRESULT result, const std::wstring&)
			{
				scriptResult = result;
				scriptCompleted = true;
			});
		CUI_EXPECT_TRUE(pumpUntil(
			[&] { return scriptCompleted; }, 10'000u));
		CUI_EXPECT_TRUE(SUCCEEDED(scriptResult));

		const auto first = capture(scene, 500, 127u);
		const RECT expectedBounds{ 80, 54, 104, 74 };
		CUI_EXPECT_EQ(480ULL, first.Pixels);
		CUI_EXPECT_TRUE(::EqualRect(&first.Bounds, &expectedBounds));
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());

		const auto second = capture(scene, 700, 76u);
		CUI_EXPECT_EQ(480ULL, second.Pixels);
		CUI_EXPECT_TRUE(::EqualRect(&second.Bounds, &expectedBounds));
		const auto frame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(2ULL, frame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, frame.CommandRecordedNodes);
		CUI_EXPECT_EQ(0ULL, frame.NativeCommitNodes);

		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		const auto recovered = capture(scene, 700, 76u);
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		CUI_EXPECT_EQ(480ULL, recovered.Pixels);
		CUI_EXPECT_TRUE(::EqualRect(&recovered.Bounds, &expectedBounds));
		CUI_EXPECT_TRUE(browser->IsWebViewReady());
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());

		// Exercise the same production WebView2 visual after the animated host
		// becomes one transient root beside a second, independently ordered root.
		scene.OpenHostAsTransientForTesting();
		auto* trailingRoot = scene.AddTransientRootForTesting(
			40.0f, 20.0f, 200.0f, 80.0f,
			D2D1_COLOR_F{ 1.0f, 0.0f, 1.0f, 1.0f });
		const auto overlayFirst = capture(scene, 700, 76u, true);
		const RECT trailingBounds{ 200, 104, 240, 124 };
		CUI_EXPECT_EQ(480ULL, overlayFirst.Pixels);
		CUI_EXPECT_TRUE(::EqualRect(&overlayFirst.Bounds, &expectedBounds));
		CUI_EXPECT_EQ(800ULL, overlayFirst.TrailingPixels);
		CUI_EXPECT_TRUE(::EqualRect(
			&overlayFirst.TrailingBounds, &trailingBounds));
		CUI_EXPECT_EQ(2ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());
		PresentationNodeSnapshot targetSnapshot;
		PresentationNodeSnapshot trailingSnapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, targetSnapshot));
		CUI_EXPECT_TRUE(scene.TryGetPresentationSnapshotForTesting(
			trailingRoot, trailingSnapshot));
		CUI_EXPECT_TRUE(targetSnapshot.Overlay);
		CUI_EXPECT_TRUE(trailingSnapshot.Overlay);
		CUI_EXPECT_TRUE(targetSnapshot.CompositionIsolated);
		CUI_EXPECT_TRUE(trailingSnapshot.CompositionIsolated);
		CUI_EXPECT_EQ(2ULL, targetSnapshot.CompositionIsolationDepth);
		CUI_EXPECT_EQ(1ULL, trailingSnapshot.CompositionIsolationDepth);

		const auto overlaySteady = capture(scene, 700, 76u, true);
		const auto overlayFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_TRUE(overlayFrame.CompositionOnlySegments >= 1u);
		CUI_EXPECT_EQ(0ULL, overlayFrame.CommandRecordedNodes);
		CUI_EXPECT_EQ(0ULL, overlayFrame.NativeCommitNodes);
		const auto overlayRecoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		const auto overlayRecovered = capture(scene, 700, 76u, true);
		CUI_EXPECT_EQ(overlayRecoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		CUI_EXPECT_EQ(overlaySteady.Pixels, overlayRecovered.Pixels);
		CUI_EXPECT_TRUE(::EqualRect(
			&overlaySteady.Bounds, &overlayRecovered.Bounds));
		CUI_EXPECT_EQ(
			overlaySteady.TrailingPixels, overlayRecovered.TrailingPixels);
		CUI_EXPECT_TRUE(::EqualRect(
			&overlaySteady.TrailingBounds,
			&overlayRecovered.TrailingBounds));
		CUI_EXPECT_TRUE(browser->IsWebViewReady());
		CUI_EXPECT_EQ(2ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_LIVE_WEBVIEW2_OPACITY_OUTPUT") != 0)
			throw std::runtime_error("Could not query live WebView2 output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_LIVE_WEBVIEW2_OPACITY_OUTPUT") != 0)
				throw std::runtime_error("Could not read live WebView2 output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Live WebView2 output must be under Workplans.");
			auto sample = [](const Sample& value)
			{
				return std::string("{\"atMilliseconds\":")
					+ std::to_string(value.At) + ",\"blue\":{\"blue\":"
					+ std::to_string(value.Blue.Blue) + ",\"green\":"
					+ std::to_string(value.Blue.Green) + ",\"red\":"
					+ std::to_string(value.Blue.Red) + ",\"alpha\":"
					+ std::to_string(value.Blue.Alpha)
					+ "},\"matchingPixels\":" + std::to_string(value.Pixels)
					+ ",\"bounds\":{\"left\":"
					+ std::to_string(value.Bounds.left) + ",\"top\":"
					+ std::to_string(value.Bounds.top) + ",\"right\":"
					+ std::to_string(value.Bounds.right) + ",\"bottom\":"
					+ std::to_string(value.Bounds.bottom) + "}}";
			};
			auto overlaySample = [](const Sample& value)
			{
				return std::string("{\"atMilliseconds\":")
					+ std::to_string(value.At) + ",\"blue\":{\"blue\":"
					+ std::to_string(value.Blue.Blue) + ",\"green\":"
					+ std::to_string(value.Blue.Green) + ",\"red\":"
					+ std::to_string(value.Blue.Red) + ",\"alpha\":"
					+ std::to_string(value.Blue.Alpha)
					+ "},\"bluePixels\":" + std::to_string(value.Pixels)
					+ ",\"blueBounds\":{\"left\":"
					+ std::to_string(value.Bounds.left) + ",\"top\":"
					+ std::to_string(value.Bounds.top) + ",\"right\":"
					+ std::to_string(value.Bounds.right) + ",\"bottom\":"
					+ std::to_string(value.Bounds.bottom)
					+ "},\"trailing\":{\"blue\":"
					+ std::to_string(value.Trailing.Blue) + ",\"green\":"
					+ std::to_string(value.Trailing.Green) + ",\"red\":"
					+ std::to_string(value.Trailing.Red) + ",\"alpha\":"
					+ std::to_string(value.Trailing.Alpha)
					+ "},\"trailingPixels\":"
					+ std::to_string(value.TrailingPixels)
					+ ",\"trailingBounds\":{\"left\":"
					+ std::to_string(value.TrailingBounds.left) + ",\"top\":"
					+ std::to_string(value.TrailingBounds.top) + ",\"right\":"
					+ std::to_string(value.TrailingBounds.right) + ",\"bottom\":"
					+ std::to_string(value.TrailingBounds.bottom) + "}}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\": 1,\n  \"engine\": \"CUI-WebView2\",\n"
				+ "  \"scenario\": \"live-webview2-shared-opacity-group\",\n"
				+ "  \"samples\": [" + sample(first) + "," + sample(second)
				+ "],\n  \"groupedNativeVisualCount\": 1,\n"
				+ "  \"secondFrameCompositionOnlySegments\": 2,\n"
				+ "  \"deviceRecoveryMatched\": true,\n"
				+ "  \"transientRootPhase\": {\"sample\":"
				+ overlaySample(overlaySteady)
				+ ",\"transientRootCount\":2,\"opacityGroupCount\":2,"
				+ "\"groupedNativeVisualCount\":1,\"targetIsolationDepth\":2,"
				+ "\"trailingIsolationDepth\":1,\"steadyNativeCommitNodes\":0,"
				+ "\"deviceRecoveryMatched\":true}\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error("Could not write live WebView2 JSON: "
					+ Convert::UnicodeToUtf8(error));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());
		scene.CloseTransientRootForTesting(trailingRoot);
		scene.CloseHostAsTransientForTesting();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation shared opacity group owns transformed boundary clip", []
	{
		struct Region
		{
			size_t Pixels = 0;
			RECT Bounds{};
		};
		struct Sample
		{
			int At = 0;
			uint8_t ExpectedChannel = 0;
			Region Red;
			Region Green;
			Region Blue;
			CapturedWindowFrame::Pixel Trailing;
		};
		auto capture = [](BenchmarkScene& scene, int at,
			uint8_t expectedChannel)
		{
			scene.TickRegisteredWindow(
				BenchmarkClockOrigin + static_cast<unsigned long long>(at));
			scene.ForcePresentationUpdateForTesting();
			auto frame = CaptureWindowComposition(
				scene.NativeWindowHandleForTesting());
			if (!frame.Error.empty())
				throw std::runtime_error(
					"Shared opacity clip capture failed: " + frame.Error);
			Sample result;
			result.At = at;
			result.ExpectedChannel = expectedChannel;
			CUI_EXPECT_TRUE(frame.TryGetColorBounds(
				0u, 0u, expectedChannel,
				result.Red.Bounds, result.Red.Pixels, 2u));
			CUI_EXPECT_TRUE(frame.TryGetColorBounds(
				0u, expectedChannel, 0u,
				result.Green.Bounds, result.Green.Pixels, 2u));
			CUI_EXPECT_TRUE(frame.TryGetColorBounds(
				expectedChannel, 0u, 0u,
				result.Blue.Bounds, result.Blue.Pixels, 2u));
			RECT trailingBounds{};
			size_t trailingPixels = 0;
			CUI_EXPECT_TRUE(frame.TryGetColorBounds(
				0u, 255u, 255u, trailingBounds, trailingPixels, 2u));
			CUI_EXPECT_TRUE(trailingPixels > 0u);
			result.Trailing = { 0u, 255u, 255u, 255u };
			return result;
		};
		auto expectRegion = [](const Region& value, size_t pixels,
			LONG left, LONG top, LONG right, LONG bottom)
		{
			CUI_EXPECT_EQ(pixels, value.Pixels);
			CUI_EXPECT_EQ(left, value.Bounds.left);
			CUI_EXPECT_EQ(top, value.Bounds.top);
			CUI_EXPECT_EQ(right, value.Bounds.right);
			CUI_EXPECT_EQ(bottom, value.Bounds.bottom);
		};

		const std::array propertyKinds{
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::TransformX,
			BenchmarkPropertyKind::TransformX };
		BenchmarkScene scene(
			3u, 0u, BenchmarkPropertyKind::Opacity,
			1u, false, false, false, propertyKinds);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.NestTargetForTesting(1u, 0u);
		scene.NestTargetForTesting(2u, 0u);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureTargetRotatedClipForTesting(
			0u, 80.0f, 40.0f, 100.0f, 30.0f, 20.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 24.0f, 24.0f, 8.0f, 8.0f);
		scene.ConfigureTargetRectangleForTesting(
			2u, 24.0f, 24.0f, 16.0f, 8.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			1u, D2D1_COLOR_F{ 0.0f, 1.0f, 0.0f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			2u, D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f });
		CUI_EXPECT_TRUE(scene.AddHostChildRectangleForTesting(
			8.0f, 16.0f, 202.0f, 34.0f,
			D2D1_COLOR_F{ 1.0f, 1.0f, 0.0f, 1.0f }) != nullptr);
		scene.Begin();
		const auto first = capture(scene, 400, 152u);
		expectRegion(first.Red, 2290u, 96L, 42L, 184L, 106L);
		expectRegion(first.Green, 164u, 144L, 66L, 159L, 90L);
		expectRegion(first.Blue, 533u, 152L, 69L, 181L, 98L);
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());
		std::array<uint64_t, 3> digests{};
		for (size_t index = 0; index < 3u; ++index)
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
				index, snapshot));
			CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
			CUI_EXPECT_EQ(index == 0u ? 1u : 2u,
				snapshot.CompositionIsolationDepth);
			UINT width = 0;
			UINT height = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				index, width, height, digests[index], opaque));
			CUI_EXPECT_TRUE(width > 0u && height > 0u && opaque > 0u);
		}
		const auto second = capture(scene, 600, 101u);
		expectRegion(second.Red, 2796u, 96L, 42L, 184L, 106L);
		expectRegion(second.Green, 164u, 163L, 73L, 177L, 97L);
		expectRegion(second.Blue, 80u, 171L, 76L, 181L, 98L);
		const auto secondFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(3ULL, secondFrame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, secondFrame.CommandRecordedNodes);
		for (size_t index = 0; index < 3u; ++index)
		{
			UINT width = 0;
			UINT height = 0;
			uint64_t digest = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				index, width, height, digest, opaque));
			CUI_EXPECT_EQ(digests[index], digest);
		}

		const auto aborts = scene.PresentationAbortedFrameCount();
		scene.SetTargetClipToBoundsForTesting(0u, false);
		const auto unclipped = capture(scene, 600, 101u);
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_TRUE(unclipped.Blue.Pixels > second.Blue.Pixels);
		scene.SetTargetClipToBoundsForTesting(0u, true);
		const auto restored = capture(scene, 600, 101u);
		CUI_EXPECT_EQ(second.Red.Pixels, restored.Red.Pixels);
		CUI_EXPECT_EQ(second.Green.Pixels, restored.Green.Pixels);
		CUI_EXPECT_EQ(second.Blue.Pixels, restored.Blue.Pixels);
		CUI_EXPECT_TRUE(::EqualRect(&second.Red.Bounds, &restored.Red.Bounds));
		CUI_EXPECT_TRUE(::EqualRect(&second.Green.Bounds, &restored.Green.Bounds));
		CUI_EXPECT_TRUE(::EqualRect(&second.Blue.Bounds, &restored.Blue.Bounds));
		CUI_EXPECT_EQ(aborts, scene.PresentationAbortedFrameCount());

		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		const auto recovered = capture(scene, 600, 101u);
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		CUI_EXPECT_EQ(second.Red.Pixels, recovered.Red.Pixels);
		CUI_EXPECT_EQ(second.Green.Pixels, recovered.Green.Pixels);
		CUI_EXPECT_EQ(second.Blue.Pixels, recovered.Blue.Pixels);
		CUI_EXPECT_TRUE(::EqualRect(&second.Red.Bounds, &recovered.Red.Bounds));
		CUI_EXPECT_TRUE(::EqualRect(
			&second.Green.Bounds, &recovered.Green.Bounds));
		CUI_EXPECT_TRUE(::EqualRect(&second.Blue.Bounds, &recovered.Blue.Bounds));

		BenchmarkScene multi(
			3u, 0u, BenchmarkPropertyKind::Opacity,
			1u, false, false, false, propertyKinds);
		multi.AcquireSceneLayerPixelReadbackLeaseForTesting();
		multi.NestTargetForTesting(1u, 0u);
		multi.NestTargetForTesting(2u, 0u);
		multi.AddNativeCompositionBoundaryForTesting();
		multi.ShowOffscreenWithoutActivationForTesting();
		multi.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		multi.ConfigureHostRotatedClipForTesting(
			120.0f, 80.0f, 70.0f, 16.0f, -15.0f);
		multi.ConfigureTargetRotatedClipForTesting(
			0u, 80.0f, 40.0f, 20.0f, 20.0f, 20.0f);
		multi.ConfigureTargetRectangleForTesting(
			1u, 24.0f, 24.0f, 8.0f, 8.0f);
		multi.ConfigureTargetRectangleForTesting(
			2u, 24.0f, 24.0f, 16.0f, 8.0f);
		multi.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
		multi.SetTargetBackgroundForTesting(
			1u, D2D1_COLOR_F{ 0.0f, 1.0f, 0.0f, 1.0f });
		multi.SetTargetBackgroundForTesting(
			2u, D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f });
		CUI_EXPECT_TRUE(multi.AddHostChildRectangleForTesting(
			8.0f, 16.0f, 94.0f, 30.0f,
			D2D1_COLOR_F{ 1.0f, 1.0f, 0.0f, 1.0f }) != nullptr);
		multi.Begin();
		const auto multiFirst = capture(multi, 400, 152u);
		expectRegion(multiFirst.Red, 2100u, 89L, 58L, 171L, 102L);
		expectRegion(multiFirst.Green, 137u, 138L, 70L, 146L, 93L);
		expectRegion(multiFirst.Blue, 460u, 146L, 70L, 170L, 95L);
		CUI_EXPECT_EQ(1ULL,
			multi.PresentationOpacityGroupCountForTesting());
		std::array<uint64_t, 3> multiDigests{};
		for (size_t index = 0; index < 3u; ++index)
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(multi.TryGetTargetPresentationSnapshotForTesting(
				index, snapshot));
			CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
			CUI_EXPECT_EQ(index == 0u ? 1u : 2u,
				snapshot.CompositionIsolationDepth);
			UINT width = 0;
			UINT height = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(multi.TryGetTargetSceneLayerPixelDigestForTesting(
				index, width, height, multiDigests[index], opaque));
		}
		const auto multiSecond = capture(multi, 600, 101u);
		expectRegion(multiSecond.Red, 2626u, 89L, 58L, 171L, 102L);
		expectRegion(multiSecond.Green, 124u, 158L, 71L, 165L, 94L);
		expectRegion(multiSecond.Blue, 55u, 166L, 76L, 170L, 95L);
		const auto multiSecondFrame = multi.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL,
			multi.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(3ULL, multiSecondFrame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, multiSecondFrame.CommandRecordedNodes);
		for (size_t index = 0; index < 3u; ++index)
		{
			UINT width = 0;
			UINT height = 0;
			uint64_t digest = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(multi.TryGetTargetSceneLayerPixelDigestForTesting(
				index, width, height, digest, opaque));
			CUI_EXPECT_EQ(multiDigests[index], digest);
		}

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_SHARED_OPACITY_CLIP_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query shared opacity clip output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_SHARED_OPACITY_CLIP_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read shared opacity clip output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Shared opacity clip output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left) + ",\"top\":"
					+ std::to_string(value.top) + ",\"right\":"
					+ std::to_string(value.right) + ",\"bottom\":"
					+ std::to_string(value.bottom) + "}";
			};
			auto regionJson = [&boundsJson](const Region& value)
			{
				return std::string("{\"matchingPixels\":")
					+ std::to_string(value.Pixels) + ",\"bounds\":"
					+ boundsJson(value.Bounds) + "}";
			};
			auto sampleJson = [&regionJson](const Sample& value)
			{
				return std::string("{\"atMilliseconds\":")
					+ std::to_string(value.At) + ",\"expectedChannel\":"
					+ std::to_string(value.ExpectedChannel) + ",\"red\":"
					+ regionJson(value.Red) + ",\"green\":"
					+ regionJson(value.Green) + ",\"blue\":"
					+ regionJson(value.Blue) + "}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\": 1,\n  \"engine\": \"CUI\",\n"
				+ "  \"scenario\": \"shared-opacity-transformed-boundary-clip\",\n"
				+ "  \"samples\": [" + sampleJson(first) + ","
				+ sampleJson(second) + "],\n"
				+ "  \"multiLevelSamples\": [" + sampleJson(multiFirst) + ","
				+ sampleJson(multiSecond) + "],\n"
				+ "  \"opacityGroupCount\": 1,\n"
				+ "  \"secondFrameCompositionOnlySegments\": 3,\n"
				+ "  \"multiLevelSecondFrameCompositionOnlySegments\": 3,\n"
				+ "  \"surfaceDigestsStable\": true,\n"
				+ "  \"clipTopologyRestored\": true,\n"
				+ "  \"deviceRecoveryMatched\": true,\n"
				+ "  \"comparisonSpace\": \"rgb-over-opaque-black\"\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error(
					"Could not write shared opacity clip JSON: "
					+ Convert::UnicodeToUtf8(error));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		scene.HideOffscreenPresentationForTesting();
		multi.Remove();
		multi.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(0ULL,
			multi.PresentationOpacityGroupCountForTesting());
		multi.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation shared opacity group rejects arbitrary boundary mask", []
	{
		const std::array propertyKinds{
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::TransformX };
		BenchmarkScene scene(
			2u, 0u, BenchmarkPropertyKind::Opacity,
			1u, false, false, false, propertyKinds);
		scene.NestTargetForTesting(1u, 0u);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureTargetPathGeometryClipForTesting(
			0u, 60.0f, 60.0f, 100.0f, 30.0f, 15.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 40.0f, 40.0f, 8.0f, 8.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			1u, D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f });
		scene.Begin();
		const auto aborts = scene.PresentationAbortedFrameCount();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 500u);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(aborts, scene.PresentationAbortedFrameCount());
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		for (size_t index = 0; index < 2u; ++index)
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
				index, snapshot));
			CUI_EXPECT_FALSE(snapshot.CompositionIsolated);
		}
		auto frame = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!frame.Error.empty())
			throw std::runtime_error(
				"Retained arbitrary group clip capture failed: " + frame.Error);
		RECT bounds{};
		size_t pixels = 0;
		CUI_EXPECT_TRUE(frame.TryGetColorBounds(
			0u, 0u, 127u, bounds, pixels, 4u));
		CUI_EXPECT_TRUE(pixels > 0u);
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation nested arbitrary Geometry opacity retains exact mask groups", []
	{
		struct Sample final
		{
			int At = 0;
			CapturedWindowFrame::Pixel OuterHole;
			CapturedWindowFrame::Pixel Inner;
			CapturedWindowFrame::Pixel Child;
			CapturedWindowFrame::Pixel Trailing;
			RECT ChildBounds{};
			size_t ChildPixels = 0;
		};
		auto capture = [](BenchmarkScene& scene, int at)
		{
			scene.TickRegisteredWindow(
				BenchmarkClockOrigin + static_cast<unsigned long long>(at));
			scene.ForcePresentationUpdateForTesting();
			auto frame = CaptureWindowComposition(
				scene.NativeWindowHandleForTesting());
			if (!frame.Error.empty())
				throw std::runtime_error(
					"Nested Geometry opacity capture failed: " + frame.Error);
			Sample result;
			result.At = at;
			CUI_EXPECT_TRUE(frame.TryGetPixel(70u, 84u, result.OuterHole));
			CUI_EXPECT_TRUE(frame.TryGetPixel(70u, 99u, result.Inner));
			CUI_EXPECT_TRUE(frame.TryGetPixel(
				at == 500 ? 60u : 85u, 69u, result.Child));
			CUI_EXPECT_TRUE(frame.TryGetPixel(154u, 62u, result.Trailing));
			CUI_EXPECT_TRUE(frame.TryGetColorBounds(
				result.Child.Blue, result.Child.Green, result.Child.Red,
				result.ChildBounds, result.ChildPixels, 2u));
			CUI_EXPECT_TRUE(result.Trailing.Red >= 253u
				&& result.Trailing.Green >= 253u
				&& result.Trailing.Blue <= 2u
				&& result.Trailing.Alpha == 255u);
			return result;
		};

		const std::array propertyKinds{
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::TransformX };
		BenchmarkScene scene(
			3u, 0u, BenchmarkPropertyKind::Opacity,
			1u, false, false, false, propertyKinds);
		scene.NestTargetForTesting(1u, 0u);
		scene.NestTargetForTesting(2u, 1u);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureTargetPathGeometryClipForTesting(
			0u, 60.0f, 60.0f, 40.0f, 30.0f, 0.0f);
		scene.ConfigureTargetGeometryGroupClipForTesting(
			1u, 60.0f, 60.0f, 0.0f, 0.0f, 0.0f);
		scene.ConfigureTargetRectangleForTesting(
			2u, 20.0f, 20.0f, -40.0f, 8.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			1u, D2D1_COLOR_F{ 0.0f, 1.0f, 0.0f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			2u, D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f });
		CUI_EXPECT_TRUE(scene.AddHostChildRectangleForTesting(
			8.0f, 16.0f, 150.0f, 34.0f,
			D2D1_COLOR_F{ 1.0f, 1.0f, 0.0f, 1.0f }) != nullptr);
		scene.Begin();
		const auto aborts = scene.PresentationAbortedFrameCount();
		const auto first = capture(scene, 500);
		CUI_EXPECT_EQ(aborts, scene.PresentationAbortedFrameCount());
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		for (size_t index = 0; index < 3u; ++index)
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
				index, snapshot));
			CUI_EXPECT_FALSE(snapshot.CompositionIsolated);
		}
		CUI_EXPECT_EQ(127u, first.OuterHole.Red);
		CUI_EXPECT_EQ(0u, first.OuterHole.Green);
		CUI_EXPECT_EQ(0u, first.OuterHole.Blue);
		CUI_EXPECT_EQ(64u, first.Inner.Red);
		CUI_EXPECT_EQ(63u, first.Inner.Green);
		CUI_EXPECT_EQ(0u, first.Inner.Blue);
		CUI_EXPECT_EQ(64u, first.Child.Red);
		CUI_EXPECT_EQ(0u, first.Child.Green);
		CUI_EXPECT_EQ(63u, first.Child.Blue);
		const RECT firstBounds{ 50, 62, 70, 82 };
		CUI_EXPECT_EQ(280ULL, first.ChildPixels);
		CUI_EXPECT_TRUE(::EqualRect(&first.ChildBounds, &firstBounds));

		const auto second = capture(scene, 700);
		CUI_EXPECT_EQ(76u, second.OuterHole.Red);
		CUI_EXPECT_EQ(53u, second.Inner.Red);
		CUI_EXPECT_EQ(23u, second.Inner.Green);
		CUI_EXPECT_EQ(0u, second.Inner.Blue);
		CUI_EXPECT_EQ(53u, second.Child.Red);
		CUI_EXPECT_EQ(0u, second.Child.Green);
		CUI_EXPECT_EQ(23u, second.Child.Blue);
		const RECT secondBounds{ 70, 62, 90, 79 };
		CUI_EXPECT_EQ(234ULL, second.ChildPixels);
		CUI_EXPECT_TRUE(::EqualRect(&second.ChildBounds, &secondBounds));
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationFrameForTesting().CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(aborts, scene.PresentationAbortedFrameCount());

		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		const auto recovered = capture(scene, 700);
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		CUI_EXPECT_EQ(second.OuterHole.Red, recovered.OuterHole.Red);
		CUI_EXPECT_EQ(second.Inner.Red, recovered.Inner.Red);
		CUI_EXPECT_EQ(second.Inner.Green, recovered.Inner.Green);
		CUI_EXPECT_EQ(second.Child.Red, recovered.Child.Red);
		CUI_EXPECT_EQ(second.Child.Blue, recovered.Child.Blue);
		CUI_EXPECT_EQ(second.ChildPixels, recovered.ChildPixels);
		CUI_EXPECT_TRUE(::EqualRect(
			&second.ChildBounds, &recovered.ChildBounds));

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_NESTED_GEOMETRY_OPACITY_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query nested Geometry opacity output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_NESTED_GEOMETRY_OPACITY_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read nested Geometry opacity output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Nested Geometry opacity output must be under Workplans.");
			auto pixel = [](const CapturedWindowFrame::Pixel& value)
			{
				return std::string("{\"blue\":")
					+ std::to_string(value.Blue) + ",\"green\":"
					+ std::to_string(value.Green) + ",\"red\":"
					+ std::to_string(value.Red) + ",\"alpha\":"
					+ std::to_string(value.Alpha) + "}";
			};
			auto sample = [&pixel](const Sample& value)
			{
				return std::string("{\"atMilliseconds\":")
					+ std::to_string(value.At) + ",\"outerThroughInnerHole\":"
					+ pixel(value.OuterHole) + ",\"innerOnly\":"
					+ pixel(value.Inner) + ",\"movingChild\":"
					+ pixel(value.Child) + ",\"trailingSibling\":"
					+ pixel(value.Trailing) + ",\"movingChildRegion\":{"
					+ "\"matchingPixels\":" + std::to_string(value.ChildPixels)
					+ ",\"bounds\":{\"left\":"
					+ std::to_string(value.ChildBounds.left) + ",\"top\":"
					+ std::to_string(value.ChildBounds.top) + ",\"right\":"
					+ std::to_string(value.ChildBounds.right) + ",\"bottom\":"
					+ std::to_string(value.ChildBounds.bottom) + "}}}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\": 1,\n  \"engine\": \"CUI\",\n"
				+ "  \"scenario\": \"nested-arbitrary-geometry-opacity\",\n"
				+ "  \"samples\": [" + sample(first) + "," + sample(second)
				+ "],\n  \"opacityGroupCount\": 0,\n"
				+ "  \"compositionIsolatedTargets\": 0,\n"
				+ "  \"deviceRecoveryMatched\": true,\n"
				+ "  \"fallback\": \"exact-retained-mask\"\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error(
					"Could not write nested Geometry opacity JSON: "
					+ Convert::UnicodeToUtf8(error));
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation transient roots own independent overlay surfaces", []
	{
		BenchmarkScene scene(1u, 0u, BenchmarkPropertyKind::Opacity);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureTargetRectangleForTesting(
			0u, 40.0f, 20.0f, 40.0f, 30.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
		scene.OpenHostAsTransientForTesting();
		auto* trailingRoot = scene.AddTransientRootForTesting(
			40.0f, 20.0f, 140.0f, 30.0f,
			D2D1_COLOR_F{ 1.0f, 1.0f, 0.0f, 1.0f });
		scene.Begin();
		const auto abortsBefore = scene.PresentationAbortedFrameCount();

		auto capture = [&]
		{
			if (scene.PresentationAbortedFrameCount() != abortsBefore)
				throw std::runtime_error(
					"Independent transient-root frame aborted: before="
					+ std::to_string(abortsBefore) + " after="
					+ std::to_string(
						scene.PresentationAbortedFrameCount()));
			auto frame = CaptureWindowComposition(
				scene.NativeWindowHandleForTesting());
			if (!frame.Error.empty())
				throw std::runtime_error(
					"Independent transient-root capture failed: " + frame.Error);
			CapturedWindowFrame::Pixel faded{};
			CapturedWindowFrame::Pixel trailing{};
			CUI_EXPECT_TRUE(frame.TryGetPixel(44u, 58u, faded));
			CUI_EXPECT_TRUE(frame.TryGetPixel(144u, 58u, trailing));
			return std::tuple{ faded, trailing, frame };
		};

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 500u);
		scene.ForcePresentationUpdateForTesting();
		PresentationNodeSnapshot targetSnapshot;
		PresentationNodeSnapshot trailingSnapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, targetSnapshot));
		CUI_EXPECT_TRUE(scene.TryGetPresentationSnapshotForTesting(
			trailingRoot, trailingSnapshot));
		CUI_EXPECT_TRUE(targetSnapshot.Overlay);
		CUI_EXPECT_TRUE(trailingSnapshot.Overlay);
		if (!targetSnapshot.CompositionIsolated)
			throw std::runtime_error(
				"Transient segmentation contract failed: targetSegment="
				+ std::to_string(targetSnapshot.SegmentIndex)
				+ " trailingSegment="
				+ std::to_string(trailingSnapshot.SegmentIndex)
				+ " trailingIsolated="
				+ std::to_string(trailingSnapshot.CompositionIsolated)
				+ " groups=" + std::to_string(
					scene.PresentationOpacityGroupCountForTesting())
				+ " layers=" + std::to_string(
					scene.SynchronizePresentationLayerCountForTesting()));
		CUI_EXPECT_TRUE(trailingSnapshot.CompositionIsolated);
		CUI_EXPECT_EQ(2ULL, targetSnapshot.CompositionIsolationDepth);
		CUI_EXPECT_EQ(1ULL, trailingSnapshot.CompositionIsolationDepth);
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());

		UINT surfaceWidth = 0;
		UINT surfaceHeight = 0;
		uint64_t surfaceDigest = 0;
		size_t surfacePixels = 0;
		if (!scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, surfaceWidth, surfaceHeight, surfaceDigest, surfacePixels))
		{
			const auto diagnosticFrame = scene.PresentationFrameForTesting();
			throw std::runtime_error(
				"Could not read target segmented surface: segment="
				+ std::to_string(targetSnapshot.SegmentIndex)
				+ " depth=" + std::to_string(
					targetSnapshot.CompositionIsolationDepth)
				+ " presented=" + std::to_string(targetSnapshot.HasPresented)
				+ " commands=" + std::to_string(
					targetSnapshot.HasDrawingCommands)
				+ " opened=" + std::to_string(
					diagnosticFrame.SceneSurfacesOpened)
				+ " recorded=" + std::to_string(
					diagnosticFrame.CommandRecordedNodes)
				+ " replayed=" + std::to_string(
					diagnosticFrame.CommandReplayedNodes)
				+ " compositionOnly=" + std::to_string(
					diagnosticFrame.CompositionOnlySegments));
		}
		CUI_EXPECT_TRUE(surfacePixels > 0u);
		UINT trailingWidth = 0;
		UINT trailingHeight = 0;
		uint64_t trailingDigest = 0;
		size_t trailingPixels = 0;
		CUI_EXPECT_TRUE(scene.TryGetSceneLayerPixelDigestForTesting(
			trailingRoot, trailingWidth, trailingHeight,
			trailingDigest, trailingPixels));
		auto first = capture();
		if (std::get<0>(first).Red != 127u
			|| std::get<0>(first).Green > 1u
			|| std::get<0>(first).Blue > 1u
			|| std::get<1>(first).Red != 255u
			|| std::get<1>(first).Green != 255u
			|| std::get<1>(first).Blue != 0u)
			throw std::runtime_error(
				"Segmented overlay pixel mismatch: target="
				+ std::to_string(std::get<0>(first).Blue) + ","
				+ std::to_string(std::get<0>(first).Green) + ","
				+ std::to_string(std::get<0>(first).Red)
				+ " trailing="
				+ std::to_string(std::get<1>(first).Blue) + ","
				+ std::to_string(std::get<1>(first).Green) + ","
				+ std::to_string(std::get<1>(first).Red)
				+ " surfacePixels=" + std::to_string(surfacePixels)
				+ " trailingSurfacePixels=" + std::to_string(trailingPixels));

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 700u);
		scene.ForcePresentationUpdateForTesting();
		auto second = capture();
		CUI_EXPECT_EQ(76u, std::get<0>(second).Red);
		CUI_EXPECT_TRUE(std::get<0>(second).Green <= 1u
			&& std::get<0>(second).Blue <= 1u);
		CUI_EXPECT_EQ(255u, std::get<1>(second).Red);
		CUI_EXPECT_EQ(255u, std::get<1>(second).Green);
		CUI_EXPECT_EQ(0u, std::get<1>(second).Blue);
		const auto secondFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_TRUE(secondFrame.CompositionOnlySegments >= 1u);
		UINT movedWidth = 0;
		UINT movedHeight = 0;
		uint64_t movedDigest = 0;
		size_t movedPixels = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, movedWidth, movedHeight, movedDigest, movedPixels));
		CUI_EXPECT_EQ(surfaceWidth, movedWidth);
		CUI_EXPECT_EQ(surfaceHeight, movedHeight);
		CUI_EXPECT_EQ(surfaceDigest, movedDigest);
		CUI_EXPECT_EQ(surfacePixels, movedPixels);

		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		auto recovered = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		CUI_EXPECT_TRUE(recovered.Error.empty());
		CapturedWindowFrame::Pixel recoveredFaded{};
		CapturedWindowFrame::Pixel recoveredTrailing{};
		CUI_EXPECT_TRUE(recovered.TryGetPixel(44u, 58u, recoveredFaded));
		CUI_EXPECT_TRUE(recovered.TryGetPixel(144u, 58u, recoveredTrailing));
		CUI_EXPECT_EQ(std::get<0>(second).Red, recoveredFaded.Red);
		CUI_EXPECT_EQ(255u, recoveredTrailing.Red);
		CUI_EXPECT_EQ(255u, recoveredTrailing.Green);

		scene.Remove();
		scene.CloseTransientRootForTesting(trailingRoot);
		scene.CloseHostAsTransientForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation pixel native overlay opacity leases segmented root", []
	{
		BenchmarkScene scene(1u, 0u, BenchmarkPropertyKind::Opacity);
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureTargetRectangleForTesting(
			0u, 140.0f, 20.0f, 20.0f, 30.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
		auto* native = scene.AddTargetPixelNativeCompositionChildForTesting(
			0u, 24.0f, 20.0f, 60.0f, 0.0f,
			D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f });
		scene.OpenHostAsTransientForTesting();
		auto* trailingRoot = scene.AddTransientRootForTesting(
			40.0f, 20.0f, 200.0f, 30.0f,
			D2D1_COLOR_F{ 1.0f, 1.0f, 0.0f, 1.0f });
		scene.Begin();
		const auto aborts = scene.PresentationAbortedFrameCount();

		auto capture = [&]
		{
			auto frame = CaptureWindowComposition(
				scene.NativeWindowHandleForTesting());
			if (!frame.Error.empty())
				throw std::runtime_error(
					"Native segmented overlay capture failed: " + frame.Error);
			CapturedWindowFrame::Pixel red{};
			CapturedWindowFrame::Pixel blue{};
			CapturedWindowFrame::Pixel trailing{};
			CUI_EXPECT_TRUE(frame.TryGetPixel(24u, 60u, red));
			CUI_EXPECT_TRUE(frame.TryGetPixel(84u, 60u, blue));
			CUI_EXPECT_TRUE(frame.TryGetPixel(204u, 60u, trailing));
			return std::tuple{ red, blue, trailing, frame };
		};
		auto readRegion = [](const CapturedWindowFrame& frame,
			const CapturedWindowFrame::Pixel& pixel)
		{
			RECT bounds{};
			size_t count = 0;
			if (!frame.TryGetColorBounds(
				pixel.Blue, pixel.Green, pixel.Red, bounds, count, 2u))
				throw std::runtime_error(
					"Segmented overlay color region was not rendered.");
			return std::pair{ bounds, count };
		};

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 500u);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(aborts, scene.PresentationAbortedFrameCount());
		CUI_EXPECT_EQ(2ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());
		PresentationNodeSnapshot targetSnapshot;
		PresentationNodeSnapshot nativeSnapshot;
		PresentationNodeSnapshot trailingSnapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, targetSnapshot));
		CUI_EXPECT_TRUE(scene.TryGetPresentationSnapshotForTesting(
			native, nativeSnapshot));
		CUI_EXPECT_TRUE(scene.TryGetPresentationSnapshotForTesting(
			trailingRoot, trailingSnapshot));
		CUI_EXPECT_TRUE(targetSnapshot.Overlay);
		CUI_EXPECT_TRUE(nativeSnapshot.Overlay);
		CUI_EXPECT_TRUE(trailingSnapshot.Overlay);
		CUI_EXPECT_TRUE(targetSnapshot.CompositionIsolated);
		CUI_EXPECT_TRUE(trailingSnapshot.CompositionIsolated);
		CUI_EXPECT_EQ(2ULL, targetSnapshot.CompositionIsolationDepth);
		CUI_EXPECT_EQ(1ULL, trailingSnapshot.CompositionIsolationDepth);
		auto first = capture();
		CUI_EXPECT_EQ(127u, std::get<0>(first).Red);
		CUI_EXPECT_TRUE(std::get<0>(first).Green <= 1u
			&& std::get<0>(first).Blue <= 1u);
		CUI_EXPECT_EQ(127u, std::get<1>(first).Blue);
		CUI_EXPECT_TRUE(std::get<1>(first).Red <= 1u
			&& std::get<1>(first).Green <= 1u);
		CUI_EXPECT_EQ(255u, std::get<2>(first).Red);
		CUI_EXPECT_EQ(255u, std::get<2>(first).Green);
		CUI_EXPECT_EQ(0u, std::get<2>(first).Blue);
		const auto firstNativeRegion = readRegion(
			std::get<3>(first), std::get<1>(first));
		const auto firstTrailingRegion = readRegion(
			std::get<3>(first), std::get<2>(first));
		const RECT nativeBounds{ 80, 54, 104, 74 };
		const RECT trailingBounds{ 200, 54, 240, 74 };
		CUI_EXPECT_EQ(480ULL, firstNativeRegion.second);
		CUI_EXPECT_TRUE(::EqualRect(
			&firstNativeRegion.first, &nativeBounds));
		CUI_EXPECT_EQ(800ULL, firstTrailingRegion.second);
		CUI_EXPECT_TRUE(::EqualRect(
			&firstTrailingRegion.first, &trailingBounds));

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 700u);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(aborts, scene.PresentationAbortedFrameCount());
		auto second = capture();
		CUI_EXPECT_EQ(76u, std::get<0>(second).Red);
		CUI_EXPECT_EQ(76u, std::get<1>(second).Blue);
		CUI_EXPECT_EQ(255u, std::get<2>(second).Red);
		CUI_EXPECT_EQ(255u, std::get<2>(second).Green);
		const auto secondNativeRegion = readRegion(
			std::get<3>(second), std::get<1>(second));
		const auto secondTrailingRegion = readRegion(
			std::get<3>(second), std::get<2>(second));
		CUI_EXPECT_EQ(480ULL, secondNativeRegion.second);
		CUI_EXPECT_TRUE(::EqualRect(
			&secondNativeRegion.first, &nativeBounds));
		CUI_EXPECT_EQ(800ULL, secondTrailingRegion.second);
		CUI_EXPECT_TRUE(::EqualRect(
			&secondTrailingRegion.first, &trailingBounds));
		const auto steady = scene.PresentationFrameForTesting();
		CUI_EXPECT_TRUE(steady.CompositionOnlySegments >= 1u);
		CUI_EXPECT_EQ(0ULL, steady.CommandRecordedNodes);
		CUI_EXPECT_EQ(0ULL, steady.NativeCommitNodes);

		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		const auto nativeGeneration = native->VisualGenerationForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		CUI_EXPECT_TRUE(
			native->VisualGenerationForTesting() > nativeGeneration);
		CUI_EXPECT_EQ(2ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());
		auto recovered = capture();
		CUI_EXPECT_EQ(std::get<0>(second).Red, std::get<0>(recovered).Red);
		CUI_EXPECT_EQ(std::get<1>(second).Blue, std::get<1>(recovered).Blue);
		CUI_EXPECT_EQ(255u, std::get<2>(recovered).Red);
		const auto recoveredNativeRegion = readRegion(
			std::get<3>(recovered), std::get<1>(recovered));
		const auto recoveredTrailingRegion = readRegion(
			std::get<3>(recovered), std::get<2>(recovered));
		CUI_EXPECT_EQ(secondNativeRegion.second,
			recoveredNativeRegion.second);
		CUI_EXPECT_TRUE(::EqualRect(
			&secondNativeRegion.first, &recoveredNativeRegion.first));
		CUI_EXPECT_EQ(secondTrailingRegion.second,
			recoveredTrailingRegion.second);
		CUI_EXPECT_TRUE(::EqualRect(
			&secondTrailingRegion.first, &recoveredTrailingRegion.first));

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_TRANSIENT_ROOT_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query transient-root output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_TRANSIENT_ROOT_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read transient-root output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Transient-root output must be under Workplans.");
			auto pixel = [](const CapturedWindowFrame::Pixel& value)
			{
				return std::string("{\"blue\":")
					+ std::to_string(value.Blue) + ",\"green\":"
					+ std::to_string(value.Green) + ",\"red\":"
					+ std::to_string(value.Red) + ",\"alpha\":"
					+ std::to_string(value.Alpha) + "}";
			};
			auto region = [](const std::pair<RECT, size_t>& value)
			{
				return std::string("{\"matchingPixels\":")
					+ std::to_string(value.second)
					+ ",\"bounds\":{\"left\":"
					+ std::to_string(value.first.left) + ",\"top\":"
					+ std::to_string(value.first.top) + ",\"right\":"
					+ std::to_string(value.first.right) + ",\"bottom\":"
					+ std::to_string(value.first.bottom) + "}}";
			};
			auto sample = [&pixel, &region](
				int at, const auto& value,
				const std::pair<RECT, size_t>& nativeRegion,
				const std::pair<RECT, size_t>& trailingRegion)
			{
				return std::string("{\"atMilliseconds\":")
					+ std::to_string(at) + ",\"fadedRoot\":"
					+ pixel(std::get<0>(value)) + ",\"nativeEquivalent\":"
					+ pixel(std::get<1>(value)) + ",\"trailingRoot\":"
					+ pixel(std::get<2>(value)) + ",\"nativeRegion\":"
					+ region(nativeRegion) + ",\"trailingRegion\":"
					+ region(trailingRegion) + "}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\": 1,\n  \"engine\": \"CUI\",\n"
				+ "  \"scenario\": \"segmented-transient-root-opacity\",\n"
				+ "  \"samples\": ["
				+ sample(500, first, firstNativeRegion, firstTrailingRegion)
				+ "," + sample(700, second,
					secondNativeRegion, secondTrailingRegion)
				+ "],\n  \"transientRootCount\": 2,\n"
				+ "  \"opacityGroupCount\": 2,\n"
				+ "  \"groupedNativeVisualCount\": 1,\n"
				+ "  \"targetIsolationDepth\": 2,\n"
				+ "  \"trailingIsolationDepth\": 1,\n"
				+ "  \"steadyCompositionOnlySegments\": "
				+ std::to_string(steady.CompositionOnlySegments) + ",\n"
				+ "  \"steadyNativeCommitNodes\": "
				+ std::to_string(steady.NativeCommitNodes) + ",\n"
				+ "  \"deviceRecoveryMatched\": true,\n"
				+ "  \"atomicFallbackVerifiedSeparately\": true\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error(
					"Could not write transient-root JSON: "
					+ Convert::UnicodeToUtf8(error));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());
		scene.CloseTransientRootForTesting(trailingRoot);
		scene.CloseHostAsTransientForTesting();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation overlay segmentation falls back atomically", []
	{
		BenchmarkScene scene(1u, 0u, BenchmarkPropertyKind::Opacity);
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureHostPathGeometryClipForTesting(
			100.0f, 70.0f, 0.0f, 0.0f, 0.0f);
		scene.ConfigureTargetRectangleForTesting(
			0u, 20.0f, 20.0f, 22.0f, 18.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
		scene.OpenHostAsTransientForTesting();
		auto* trailingRoot = scene.AddTransientRootForTesting(
			40.0f, 20.0f, 140.0f, 30.0f,
			D2D1_COLOR_F{ 1.0f, 1.0f, 0.0f, 1.0f });
		scene.Begin();
		const auto aborts = scene.PresentationAbortedFrameCount();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 500u);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(aborts, scene.PresentationAbortedFrameCount());
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		PresentationNodeSnapshot targetSnapshot;
		PresentationNodeSnapshot trailingSnapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, targetSnapshot));
		CUI_EXPECT_TRUE(scene.TryGetPresentationSnapshotForTesting(
			trailingRoot, trailingSnapshot));
		CUI_EXPECT_TRUE(targetSnapshot.Overlay);
		CUI_EXPECT_TRUE(trailingSnapshot.Overlay);
		CUI_EXPECT_FALSE(targetSnapshot.CompositionIsolated);
		CUI_EXPECT_FALSE(trailingSnapshot.CompositionIsolated);
		auto frame = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!frame.Error.empty())
			throw std::runtime_error(
				"Atomic overlay fallback capture failed: " + frame.Error);
		CapturedWindowFrame::Pixel faded{};
		CapturedWindowFrame::Pixel trailing{};
		CUI_EXPECT_TRUE(frame.TryGetPixel(27u, 47u, faded));
		CUI_EXPECT_TRUE(frame.TryGetPixel(144u, 58u, trailing));
		CUI_EXPECT_EQ(127u, faded.Red);
		CUI_EXPECT_TRUE(faded.Green <= 1u && faded.Blue <= 1u);
		CUI_EXPECT_EQ(255u, trailing.Red);
		CUI_EXPECT_EQ(255u, trailing.Green);
		CUI_EXPECT_EQ(0u, trailing.Blue);

		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		auto recovered = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		CUI_EXPECT_TRUE(recovered.Error.empty());
		CapturedWindowFrame::Pixel recoveredFaded{};
		CapturedWindowFrame::Pixel recoveredTrailing{};
		CUI_EXPECT_TRUE(recovered.TryGetPixel(27u, 47u, recoveredFaded));
		CUI_EXPECT_TRUE(recovered.TryGetPixel(
			144u, 58u, recoveredTrailing));
		CUI_EXPECT_EQ(faded.Red, recoveredFaded.Red);
		CUI_EXPECT_EQ(trailing.Red, recoveredTrailing.Red);
		CUI_EXPECT_EQ(trailing.Green, recoveredTrailing.Green);

		scene.Remove();
		scene.CloseTransientRootForTesting(trailingRoot);
		scene.CloseHostAsTransientForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation native overlay opacity stays fail closed on shared surface", []
	{
		BenchmarkScene scene(1u, 0u, BenchmarkPropertyKind::Opacity);
		scene.AddTargetNativeCompositionBoundaryForTesting(0u);
		scene.OpenHostAsTransientForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 60.0f, 20.0f, 20.0f, 30.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 1.0f, 0.0f, 0.0f, 1.0f });
		scene.Begin();
		const auto aborts = scene.PresentationAbortedFrameCount();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 500u);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_TRUE(scene.PresentationAbortedFrameCount() > aborts);
		CUI_EXPECT_TRUE(
			scene.SynchronizePresentationLayerCountForTesting() >= 1u);
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationGroupedNativeVisualCountForTesting());
		PresentationNodeSnapshot snapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, snapshot));
		CUI_EXPECT_TRUE(snapshot.Overlay);
		CUI_EXPECT_FALSE(snapshot.CompositionIsolated);
		scene.Remove();
		scene.CloseHostAsTransientForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation real Popup ContextMenu Menu overlay lifecycle owns segmented roots", []
	{
		BenchmarkScene scene(1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureTargetRectangleForTesting(
			0u, 1.0f, 1.0f, 350.0f, 220.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });

		const std::string xaml = R"XAML(
<Window xmlns="urn:cui"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Width="360" Height="240">
<Canvas Width="360" Height="240">
  <Popup x:Name="popup" Placement="Absolute"
         HorizontalOffset="40" VerticalOffset="30" StaysOpen="true">
    <Canvas x:Name="popupContent" Width="180" Height="100"
            Background="#FFFF00FF"/>
  </Popup>
  <ContextMenu x:Name="context" Width="180" Height="100"
               Background="#FFFF0000" StaysOpen="true"
               Placement="MousePoint">
    <ContextMenu.Items>
      <MenuItem x:Name="contextItem" Header="C" Focusable="false"/>
    </ContextMenu.Items>
  </ContextMenu>
  <Menu x:Name="menu" Canvas.Left="40" Canvas.Top="2"
        Width="220" Height="28">
    <Menu.Items>
      <MenuItem x:Name="file" Header="File">
        <MenuItem.Items>
          <MenuItem Header="I" Focusable="false"/>
        </MenuItem.Items>
      </MenuItem>
    </Menu.Items>
  </Menu>
</Canvas>
</Window>)XAML";
		DesignerModel::DesignDocument document;
		std::wstring materializeError;
		if (!DesignerModel::XamlDocumentParser::FromXaml(
			xaml, document, &materializeError))
			throw std::runtime_error(
				"Could not parse real transient-control authority: "
				+ Convert::UnicodeToUtf8(materializeError));
		CuiRuntime::XamlObjectTree tree;
		if (!CuiRuntime::XamlObjectMaterializer::Materialize(
			document, tree, {}, &materializeError))
			throw std::runtime_error(
				"Could not materialize real transient-control authority: "
				+ Convert::UnicodeToUtf8(materializeError));
		auto find = [&](const wchar_t* name) -> Control*
		{
			const auto found = std::find_if(
				tree.Controls.begin(), tree.Controls.end(),
				[&](const auto& control)
				{ return control && control->Name == name; });
			return found == tree.Controls.end()
				? nullptr : (*found)->ControlInstance;
		};
		auto* popup = dynamic_cast<Popup*>(find(L"popup"));
		auto* popupContent = dynamic_cast<Canvas*>(find(L"popupContent"));
		auto* context = dynamic_cast<ContextMenu*>(find(L"context"));
		auto* contextItem = dynamic_cast<MenuItem*>(find(L"contextItem"));
		auto* menu = dynamic_cast<Menu*>(find(L"menu"));
		auto* file = dynamic_cast<MenuItem*>(find(L"file"));
		CUI_EXPECT_TRUE(tree.ContentRoot && popup && popupContent
			&& context && contextItem && menu && file);
		if (!tree.ContentRoot || !popup || !popupContent
			|| !context || !contextItem || !menu || !file)
			throw std::runtime_error(
				"Real transient-control authority lost a named control.");
		CUI_EXPECT_TRUE(scene.AddRootControlForTesting(
			std::move(tree.ContentRoot)) != nullptr);

		scene.ShowOffscreenWithoutActivationForTesting();
		(void)context->ApplyTemplate();
		(void)menu->ApplyTemplate();
		(void)file->ApplyTemplate();
		scene.ForcePresentationUpdateForTesting();

		popup->IsOpen = true;
		scene.ForcePresentationUpdateForTesting();
		context->ShowAt(40, 30);
		scene.ForcePresentationUpdateForTesting();
		file->IsSubmenuOpen = true;
		scene.ForcePresentationUpdateForTesting();

		auto* menuPopup = dynamic_cast<Popup*>(
			file->FindDeclarativeTemplatePart(L"PART_Popup"));
		auto* submenuBorder = dynamic_cast<Border*>(
			file->FindDeclarativeTemplatePart(L"PART_SubmenuBorder"));
		CUI_EXPECT_TRUE(menuPopup && submenuBorder);
		if (!menuPopup || !submenuBorder)
			throw std::runtime_error(
				"Real Menu submenu template did not expose its Popup and Border.");
		menuPopup->Placement = PlacementMode::Absolute;
		menuPopup->PlacementTarget = nullptr;
		menuPopup->HorizontalOffset = 40.0f;
		menuPopup->VerticalOffset = 30.0f;
		submenuBorder->Width = 180.0f;
		submenuBorder->Height = 100.0f;
		submenuBorder->Background =
			D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f };
		menuPopup->UpdatePlacement();
		scene.ForcePresentationUpdateForTesting();

		auto& window = scene.WindowForTesting();
		auto transientCount = [&]
		{
			return cui::framework::WindowAccess::
				GetTransientPresentationCount(window);
		};
		auto topmost = [&]() -> Control*
		{
			return cui::framework::WindowAccess::
				GetTopmostTransientPresentation(window);
		};
		auto expectPixel = [](const CapturedWindowFrame::Pixel& pixel,
			uint8_t blue, uint8_t green, uint8_t red)
		{
			if (pixel.Blue != blue || pixel.Green != green || pixel.Red != red)
				throw std::runtime_error(
					"Real transient control pixel mismatch: actual="
					+ std::to_string(pixel.Blue) + ","
					+ std::to_string(pixel.Green) + ","
					+ std::to_string(pixel.Red) + " expected="
					+ std::to_string(blue) + ","
					+ std::to_string(green) + ","
					+ std::to_string(red) + ".");
		};
		POINT commonClientPoint{};
		bool hasCommonClientPoint = false;
		auto captureAtCommonPoint = [&]
		{
			if (!hasCommonClientPoint)
			{
				const auto popupBounds = popupContent->GetRenderedAbsoluteRectDip();
				const auto contextBounds = context->GetRenderedAbsoluteRectDip();
				const auto menuBounds = submenuBorder->GetRenderedAbsoluteRectDip();
				const float left = (std::max)({ popupBounds.left,
					contextBounds.left, menuBounds.left });
				const float top = (std::max)({ popupBounds.top,
					contextBounds.top, menuBounds.top });
				const float right = (std::min)({ popupBounds.right,
					contextBounds.right, menuBounds.right });
				const float bottom = (std::min)({ popupBounds.bottom,
					contextBounds.bottom, menuBounds.bottom });
				if (right - left < 60.0f || bottom - top < 50.0f)
					throw std::runtime_error(
						"Real transient controls do not have a stable overlap: popup="
						+ std::to_string(popupBounds.left) + ","
						+ std::to_string(popupBounds.top) + ","
						+ std::to_string(popupBounds.right) + ","
						+ std::to_string(popupBounds.bottom) + " context="
						+ std::to_string(contextBounds.left) + ","
						+ std::to_string(contextBounds.top) + ","
						+ std::to_string(contextBounds.right) + ","
						+ std::to_string(contextBounds.bottom) + " menu="
						+ std::to_string(menuBounds.left) + ","
						+ std::to_string(menuBounds.top) + ","
						+ std::to_string(menuBounds.right) + ","
						+ std::to_string(menuBounds.bottom) + ".");
				const float sampleX = right - 25.0f;
				const float sampleY = bottom - 25.0f;
				const auto client = window.ContentDipRectToClientPixels(
					D2D1::RectF(
						sampleX, sampleY, sampleX + 1.0f, sampleY + 1.0f));
				commonClientPoint = POINT{ client.left, client.top };
				hasCommonClientPoint = true;
			}
			auto frame = CaptureWindowComposition(
				scene.NativeWindowHandleForTesting());
			if (!frame.Error.empty())
				throw std::runtime_error(
					"Real transient control capture failed: " + frame.Error);
			CapturedWindowFrame::Pixel pixel{};
			CUI_EXPECT_TRUE(frame.TryGetPixel(
				static_cast<UINT>(commonClientPoint.x),
				static_cast<UINT>(commonClientPoint.y), pixel));
			return pixel;
		};
		auto readDigest = [&](Control* root)
		{
			UINT width = 0;
			UINT height = 0;
			uint64_t digest = 0;
			size_t pixels = 0;
			if (!scene.TryGetSceneLayerPixelDigestForTesting(
				root, width, height, digest, pixels))
				throw std::runtime_error(
					"Could not read real transient retained surface.");
			return std::tuple{ width, height, digest, pixels };
		};

		CUI_EXPECT_EQ(3ULL, transientCount());
		CUI_EXPECT_TRUE(topmost() == menuPopup);
		PresentationNodeSnapshot popupSnapshot;
		PresentationNodeSnapshot contextSnapshot;
		PresentationNodeSnapshot menuSnapshot;
		CUI_EXPECT_TRUE(scene.TryGetPresentationSnapshotForTesting(
			popup, popupSnapshot));
		CUI_EXPECT_TRUE(scene.TryGetPresentationSnapshotForTesting(
			context, contextSnapshot));
		CUI_EXPECT_TRUE(scene.TryGetPresentationSnapshotForTesting(
			menuPopup, menuSnapshot));
		CUI_EXPECT_TRUE(popupSnapshot.Overlay
			&& contextSnapshot.Overlay && menuSnapshot.Overlay);
		CUI_EXPECT_TRUE(popupSnapshot.CompositionIsolated
			&& contextSnapshot.CompositionIsolated
			&& menuSnapshot.CompositionIsolated);
		CUI_EXPECT_EQ(1ULL, popupSnapshot.CompositionIsolationDepth);
		CUI_EXPECT_EQ(1ULL, contextSnapshot.CompositionIsolationDepth);
		CUI_EXPECT_EQ(1ULL, menuSnapshot.CompositionIsolationDepth);
		const auto initialMenuPixel = captureAtCommonPoint();
		expectPixel(initialMenuPixel, 255u, 0u, 0u);

		const auto popupDigest = readDigest(popup);
		const auto contextDigest = readDigest(context);
		const auto menuDigest = readDigest(menuPopup);
		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		CUI_EXPECT_EQ(3ULL, transientCount());
		CUI_EXPECT_TRUE(topmost() == menuPopup);
		const auto recoveredMenuPixel = captureAtCommonPoint();
		expectPixel(recoveredMenuPixel, 255u, 0u, 0u);
		CUI_EXPECT_EQ(std::get<2>(popupDigest),
			std::get<2>(readDigest(popup)));
		CUI_EXPECT_EQ(std::get<2>(contextDigest),
			std::get<2>(readDigest(context)));
		CUI_EXPECT_EQ(std::get<2>(menuDigest),
			std::get<2>(readDigest(menuPopup)));

		file->IsSubmenuOpen = false;
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(2ULL, transientCount());
		CUI_EXPECT_TRUE(topmost() == context);
		const auto contextPixel = captureAtCommonPoint();
		expectPixel(contextPixel, 0u, 0u, 255u);
		file->IsSubmenuOpen = true;
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(3ULL, transientCount());
		CUI_EXPECT_TRUE(topmost() == menuPopup);
		const auto reopenedMenuPixel = captureAtCommonPoint();
		expectPixel(reopenedMenuPixel, 255u, 0u, 0u);

		const auto popupBeforeMenuDamage = readDigest(popup);
		const auto contextBeforeMenuDamage = readDigest(context);
		const auto menuBeforeDamage = readDigest(menuPopup);
		submenuBorder->Background =
			D2D1_COLOR_F{ 0.0f, 1.0f, 0.0f, 1.0f };
		scene.ForcePresentationUpdateForTesting();
		const auto damageFrame = scene.PresentationFrameForTesting();
		const auto popupAfterMenuDamage = readDigest(popup);
		const auto contextAfterMenuDamage = readDigest(context);
		const auto menuAfterDamage = readDigest(menuPopup);
		CUI_EXPECT_EQ(std::get<2>(popupBeforeMenuDamage),
			std::get<2>(popupAfterMenuDamage));
		CUI_EXPECT_EQ(std::get<2>(contextBeforeMenuDamage),
			std::get<2>(contextAfterMenuDamage));
		CUI_EXPECT_TRUE(std::get<2>(menuBeforeDamage)
			!= std::get<2>(menuAfterDamage));
		// The themed submenu root contains two retained segments. Both may replay,
		// but only the changed Border records; the other two transient roots keep
		// identical surface digests.
		CUI_EXPECT_EQ(2ULL, damageFrame.SceneSurfacesOpened);
		CUI_EXPECT_EQ(1ULL, damageFrame.CommandRecordedNodes);
		const auto menuDamagePixel = captureAtCommonPoint();
		expectPixel(menuDamagePixel, 0u, 255u, 0u);

		popup->IsOpen = false;
		popup->IsOpen = true;
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(3ULL, transientCount());
		CUI_EXPECT_TRUE(topmost() == popup);
		const auto reorderedPopupPixel = captureAtCommonPoint();
		expectPixel(reorderedPopupPixel, 255u, 0u, 255u);

		const auto contextBeforePopupDamage = readDigest(context);
		const auto menuBeforePopupDamage = readDigest(menuPopup);
		const auto popupBeforeDamage = readDigest(popup);
		popupContent->Background =
			D2D1_COLOR_F{ 1.0f, 1.0f, 0.0f, 1.0f };
		scene.ForcePresentationUpdateForTesting();
		const auto popupDamageFrame = scene.PresentationFrameForTesting();
		const auto contextAfterPopupDamage = readDigest(context);
		const auto menuAfterPopupDamage = readDigest(menuPopup);
		const auto popupAfterDamage = readDigest(popup);
		CUI_EXPECT_EQ(std::get<2>(contextBeforePopupDamage),
			std::get<2>(contextAfterPopupDamage));
		CUI_EXPECT_EQ(std::get<2>(menuBeforePopupDamage),
			std::get<2>(menuAfterPopupDamage));
		CUI_EXPECT_TRUE(std::get<2>(popupBeforeDamage)
			!= std::get<2>(popupAfterDamage));
		CUI_EXPECT_EQ(2ULL, popupDamageFrame.SceneSurfacesOpened);
		CUI_EXPECT_EQ(1ULL, popupDamageFrame.CommandRecordedNodes);
		const auto popupDamagePixel = captureAtCommonPoint();
		expectPixel(popupDamagePixel, 0u, 255u, 255u);

		context->Hide();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(2ULL, transientCount());
		CUI_EXPECT_TRUE(topmost() == popup);
		const auto contextClosedPixel = captureAtCommonPoint();
		expectPixel(contextClosedPixel, 0u, 255u, 255u);
		popup->IsOpen = false;
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(1ULL, transientCount());
		CUI_EXPECT_TRUE(topmost() == menuPopup);
		const auto popupClosedPixel = captureAtCommonPoint();
		expectPixel(popupClosedPixel, 0u, 255u, 0u);
		file->IsSubmenuOpen = false;
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(0ULL, transientCount());
		CUI_EXPECT_TRUE(topmost() == nullptr);

		// A common arbitrary Path mask above a nested opacity target cannot be
		// expressed by the DComp group graph. Reopen the same three product roots
		// and require the complete set to fall back to the exact shared overlay.
		cui::drawing::Geometry pathClip;
		pathClip.Kind = cui::drawing::GeometryKind::Path;
		cui::drawing::PathFigure pathFigure;
		pathFigure.StartPoint = D2D1::Point2F(0.0f, 0.0f);
		pathFigure.IsClosed = true;
		pathFigure.IsFilled = true;
		for (const auto point : std::array{
			D2D1::Point2F(180.0f, 0.0f),
			D2D1::Point2F(180.0f, 100.0f),
			D2D1::Point2F(0.0f, 100.0f) })
		{
			cui::drawing::PathSegment segment;
			segment.Kind = cui::drawing::PathSegmentKind::Line;
			segment.Point = point;
			pathFigure.Segments.push_back(segment);
		}
		pathClip.Figures.push_back(std::move(pathFigure));
		context->SetClip(pathClip);
		contextItem->Opacity = 0.5;
		popup->IsOpen = true;
		context->ShowAt(40, 30);
		file->IsSubmenuOpen = true;
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(3ULL, transientCount());
		CUI_EXPECT_TRUE(topmost() == menuPopup);
		CUI_EXPECT_TRUE(scene.TryGetPresentationSnapshotForTesting(
			popup, popupSnapshot));
		CUI_EXPECT_TRUE(scene.TryGetPresentationSnapshotForTesting(
			context, contextSnapshot));
		CUI_EXPECT_TRUE(scene.TryGetPresentationSnapshotForTesting(
			menuPopup, menuSnapshot));
		CUI_EXPECT_TRUE(popupSnapshot.Overlay
			&& contextSnapshot.Overlay && menuSnapshot.Overlay);
		CUI_EXPECT_FALSE(popupSnapshot.CompositionIsolated);
		CUI_EXPECT_FALSE(contextSnapshot.CompositionIsolated);
		CUI_EXPECT_FALSE(menuSnapshot.CompositionIsolated);
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		const auto atomicFallbackPixel = captureAtCommonPoint();
		expectPixel(atomicFallbackPixel, 0u, 255u, 0u);
		const auto fallbackRecoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(fallbackRecoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		expectPixel(captureAtCommonPoint(), 0u, 255u, 0u);
		file->IsSubmenuOpen = false;
		context->Hide();
		popup->IsOpen = false;
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(0ULL, transientCount());
		CUI_EXPECT_TRUE(topmost() == nullptr);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_REAL_TRANSIENT_CONTROLS_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query real transient-control output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_REAL_TRANSIENT_CONTROLS_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read real transient-control output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Real transient-control output must be under Workplans.");
			auto pixel = [](const CapturedWindowFrame::Pixel& value)
			{
				return std::string("{\"blue\":")
					+ std::to_string(value.Blue) + ",\"green\":"
					+ std::to_string(value.Green) + ",\"red\":"
					+ std::to_string(value.Red) + ",\"alpha\":"
					+ std::to_string(value.Alpha) + "}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\": 1,\n  \"engine\": \"CUI\",\n"
				+ "  \"scenario\": \"real-popup-contextmenu-menu-overlay-lifecycle\",\n"
				+ "  \"sampleClientPoint\": {\"x\":"
				+ std::to_string(commonClientPoint.x) + ",\"y\":"
				+ std::to_string(commonClientPoint.y) + "},\n"
				+ "  \"states\": ["
				+ "{\"name\":\"all-open-menu-topmost\",\"rootCount\":3,\"pixel\":"
				+ pixel(initialMenuPixel) + "},"
				+ "{\"name\":\"device-recovered\",\"rootCount\":3,\"pixel\":"
				+ pixel(recoveredMenuPixel) + "},"
				+ "{\"name\":\"menu-closed-context-topmost\",\"rootCount\":2,\"pixel\":"
				+ pixel(contextPixel) + "},"
				+ "{\"name\":\"menu-reopened\",\"rootCount\":3,\"pixel\":"
				+ pixel(reopenedMenuPixel) + "},"
				+ "{\"name\":\"menu-damaged\",\"rootCount\":3,\"pixel\":"
				+ pixel(menuDamagePixel) + "},"
				+ "{\"name\":\"popup-reordered-topmost\",\"rootCount\":3,\"pixel\":"
				+ pixel(reorderedPopupPixel) + "},"
				+ "{\"name\":\"popup-damaged\",\"rootCount\":3,\"pixel\":"
				+ pixel(popupDamagePixel) + "},"
				+ "{\"name\":\"context-closed\",\"rootCount\":2,\"pixel\":"
				+ pixel(contextClosedPixel) + "},"
				+ "{\"name\":\"popup-closed-menu-visible\",\"rootCount\":1,\"pixel\":"
				+ pixel(popupClosedPixel) + "},"
				+ "{\"name\":\"atomic-fallback-all-open\",\"rootCount\":3,\"pixel\":"
				+ pixel(atomicFallbackPixel) + "},"
				+ "{\"name\":\"all-closed\",\"rootCount\":0}],\n"
				+ "  \"isolationDepths\": {\"popup\":1,\"contextMenu\":1,\"menuPopup\":1},\n"
				+ "  \"menuDamage\": {\"sceneSurfacesOpened\":2,\"commandRecordedNodes\":1,"
				+ "\"otherTransientDigestsStable\":true},\n"
				+ "  \"popupDamage\": {\"sceneSurfacesOpened\":2,\"commandRecordedNodes\":1,"
				+ "\"otherTransientDigestsStable\":true},\n"
				+ "  \"deviceRecoveryMatched\": true,\n"
				+ "  \"atomicFallback\": {\"rootCount\":3,\"opacityGroupCount\":0,"
				+ "\"compositionIsolatedRoots\":0,\"deviceRecoveryMatched\":true},\n"
				+ "  \"finalTransientRootCount\": 0\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write real transient-control JSON: "
					+ Convert::UnicodeToUtf8(writeError));
		}

		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation shared opacity group topology reparent is reversible", []
	{
		BenchmarkScene scene(2u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.Begin();
		scene.SetHostOpacityForTesting(0.5);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());
		for (size_t index = 0; index < 2u; ++index)
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
				index, snapshot));
			CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
			CUI_EXPECT_EQ(2ULL, snapshot.CompositionIsolationDepth);
		}
		const auto aborts = scene.PresentationAbortedFrameCount();
		scene.SetHostOpacityForTesting(1.0);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(0ULL,
			scene.PresentationOpacityGroupCountForTesting());
		for (size_t index = 0; index < 2u; ++index)
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
				index, snapshot));
			CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
			CUI_EXPECT_EQ(1ULL, snapshot.CompositionIsolationDepth);
		}
		scene.SetHostOpacityForTesting(0.5);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(1ULL,
			scene.PresentationOpacityGroupCountForTesting());
		CUI_EXPECT_EQ(aborts, scene.PresentationAbortedFrameCount());
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation RenderTransform targets own isolated composition segments", []
	{
		BenchmarkScene scene(
			1u, 2u, BenchmarkPropertyKind::TransformX);
		CUI_EXPECT_EQ(1ULL,
			scene.SynchronizePresentationLayerCountForTesting());
		CUI_EXPECT_FALSE(
			scene.PresentationRequiresCompositionForTesting());

		scene.Begin();
		CUI_EXPECT_EQ(3ULL,
			scene.SynchronizePresentationLayerCountForTesting());
		CUI_EXPECT_FALSE(
			scene.PresentationRequiresCompositionForTesting());
		for (size_t index = 0; index < scene.TargetCountForTesting(); ++index)
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(
				scene.TryGetTargetPresentationSnapshotForTesting(
					index, snapshot));
			CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
			CUI_EXPECT_TRUE(snapshot.CompositionIsolationRoot);
			CUI_EXPECT_TRUE(snapshot.SegmentIndex != static_cast<size_t>(-1));
		}

		scene.StopRetained();
		CUI_EXPECT_EQ(3ULL,
			scene.SynchronizePresentationLayerCountForTesting());
		CUI_EXPECT_FALSE(
			scene.PresentationRequiresCompositionForTesting());
		scene.Remove();
		CUI_EXPECT_EQ(1ULL,
			scene.SynchronizePresentationLayerCountForTesting());
		CUI_EXPECT_FALSE(
			scene.PresentationRequiresCompositionForTesting());

		BenchmarkScene multiple(
			3u, 2u, BenchmarkPropertyKind::TransformX);
		multiple.Begin();
		CUI_EXPECT_EQ(5ULL,
			multiple.SynchronizePresentationLayerCountForTesting());
		CUI_EXPECT_FALSE(
			multiple.PresentationRequiresCompositionForTesting());
		for (size_t index = 0; index < multiple.TargetCountForTesting(); ++index)
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(
				multiple.TryGetTargetPresentationSnapshotForTesting(
					index, snapshot));
			CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
			CUI_EXPECT_TRUE(snapshot.CompositionIsolationRoot);
			CUI_EXPECT_EQ(index + 1u, snapshot.SegmentIndex);
		}
		multiple.Remove();

	});

	runner.Add("Animation isolated DComp render records base commands and commits", []
	{
		const auto invalidCapture = CaptureWindowComposition(nullptr, 1u);
		CUI_EXPECT_FALSE(invalidCapture.Error.empty());
		CUI_EXPECT_EQ(0u, invalidCapture.Width);
		CUI_EXPECT_EQ(0u, invalidCapture.Height);
		CUI_EXPECT_TRUE(invalidCapture.Bgra.empty());

		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		const auto committedBefore = scene.PresentationCommittedFrameCount();
		const auto abortedBefore = scene.PresentationAbortedFrameCount();
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
		scene.ForcePresentationUpdateForTesting();
		const auto firstFrame = scene.PresentationFrameForTesting();

		PresentationNodeSnapshot snapshot;
		CUI_EXPECT_TRUE(
			scene.TryGetTargetPresentationSnapshotForTesting(0u, snapshot));
		CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
		CUI_EXPECT_TRUE(snapshot.CompositionIsolationRoot);
		CUI_EXPECT_TRUE(snapshot.HasDrawingCommands);
		const auto& transform = snapshot.CompositionTransform;
		const bool identity = transform._11 == 1.0f && transform._12 == 0.0f
			&& transform._21 == 0.0f && transform._22 == 1.0f
			&& transform._31 == 0.0f && transform._32 == 0.0f;
		CUI_EXPECT_FALSE(identity);
		CUI_EXPECT_TRUE(snapshot.CompositionSurfacePhysicalWidth > 0u);
		CUI_EXPECT_TRUE(snapshot.CompositionSurfacePhysicalHeight > 0u);
		CUI_EXPECT_TRUE(snapshot.CompositionSurfacePhysicalWidth < 360u);
		CUI_EXPECT_TRUE(snapshot.CompositionSurfacePhysicalHeight < 240u);
		const auto isolatedSurfaceWidth =
			snapshot.CompositionSurfacePhysicalWidth;
		const auto isolatedSurfaceHeight =
			snapshot.CompositionSurfacePhysicalHeight;
		UINT readbackWidth = 0;
		UINT readbackHeight = 0;
		uint64_t firstPixelDigest = 0;
		size_t firstNonTransparentPixels = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, readbackWidth, readbackHeight, firstPixelDigest,
			firstNonTransparentPixels));
		CUI_EXPECT_EQ(isolatedSurfaceWidth, readbackWidth);
		CUI_EXPECT_EQ(isolatedSurfaceHeight, readbackHeight);
		CUI_EXPECT_TRUE(firstPixelDigest != 0u);
		CUI_EXPECT_EQ(144ULL, firstNonTransparentPixels);
		auto firstComposition = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstComposition.Error.empty())
			throw std::runtime_error("Initial final-composition capture failed: "
				+ firstComposition.Error);
		CUI_EXPECT_TRUE(firstComposition.Width >= 360u);
		CUI_EXPECT_TRUE(firstComposition.Height >= 240u);
		RECT firstBlueBounds{};
		size_t firstBluePixels = 0;
		CUI_EXPECT_TRUE(firstComposition.TryGetColorBounds(
			217u, 115u, 38u, firstBlueBounds, firstBluePixels));
		CUI_EXPECT_EQ(144ULL, firstBluePixels);
		CUI_EXPECT_EQ(16L, firstBlueBounds.left);
		CUI_EXPECT_EQ(24L, firstBlueBounds.top);
		CUI_EXPECT_EQ(12L, firstBlueBounds.right - firstBlueBounds.left);
		CUI_EXPECT_EQ(12L, firstBlueBounds.bottom - firstBlueBounds.top);
		CUI_EXPECT_TRUE(scene.PresentationCommittedFrameCount()
			> committedBefore);
		CUI_EXPECT_EQ(abortedBefore,
			scene.PresentationAbortedFrameCount());
		const auto firstBounds = snapshot.RenderedBounds;
		CUI_EXPECT_EQ(0ULL, firstFrame.CompositionOnlySegments);
		CUI_EXPECT_TRUE(firstFrame.SceneSurfacesOpened >= 2u);

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 320u);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_TRUE(
			scene.TryGetTargetPresentationSnapshotForTesting(0u, snapshot));
		const auto frame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(0ULL, frame.CommandRecordedNodes);
		CUI_EXPECT_TRUE(frame.CommandCacheHitNodes >= 1u);
		CUI_EXPECT_TRUE(frame.CompositionTransformOnlyNodes >= 1u);
		CUI_EXPECT_EQ(1ULL, frame.CompositionOnlySegments);
		CUI_EXPECT_TRUE(frame.SceneSurfacesOpened
			< firstFrame.SceneSurfacesOpened);
		uint64_t compositionOnlyPixelDigest = 0;
		size_t compositionOnlyNonTransparentPixels = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, readbackWidth, readbackHeight, compositionOnlyPixelDigest,
			compositionOnlyNonTransparentPixels));
		CUI_EXPECT_EQ(firstPixelDigest, compositionOnlyPixelDigest);
		CUI_EXPECT_EQ(firstNonTransparentPixels,
			compositionOnlyNonTransparentPixels);
		auto compositionOnlyCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!compositionOnlyCapture.Error.empty())
			throw std::runtime_error(
				"Composition-only final capture failed: "
				+ compositionOnlyCapture.Error);
		CUI_EXPECT_EQ(firstComposition.Width,
			compositionOnlyCapture.Width);
		CUI_EXPECT_EQ(firstComposition.Height,
			compositionOnlyCapture.Height);
		RECT compositionOnlyBlueBounds{};
		size_t compositionOnlyBluePixels = 0;
		CUI_EXPECT_TRUE(compositionOnlyCapture.TryGetColorBounds(
			217u, 115u, 38u, compositionOnlyBlueBounds,
			compositionOnlyBluePixels));
		CUI_EXPECT_EQ(144ULL, compositionOnlyBluePixels);
		CUI_EXPECT_EQ(16L, compositionOnlyBlueBounds.left
			- firstBlueBounds.left);
		CUI_EXPECT_EQ(firstBlueBounds.top,
			compositionOnlyBlueBounds.top);
		CUI_EXPECT_TRUE(compositionOnlyCapture.Digest
			!= firstComposition.Digest);
		CUI_EXPECT_TRUE(snapshot.RenderedBounds.left != firstBounds.left
			|| snapshot.RenderedBounds.top != firstBounds.top
			|| snapshot.RenderedBounds.right != firstBounds.right
			|| snapshot.RenderedBounds.bottom != firstBounds.bottom);
		CUI_EXPECT_EQ(abortedBefore,
			scene.PresentationAbortedFrameCount());

		// The preflight must remain fail-closed if PreparePresentation publishes
		// new pixels during an otherwise transform-only tick.
		scene.InvalidateTargetContentDuringNextPrepareForTesting(0u);
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 480u);
		scene.ForcePresentationUpdateForTesting();
		const auto preparedContentFrame =
			scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(0ULL,
			preparedContentFrame.CompositionOnlySegments);
		CUI_EXPECT_TRUE(preparedContentFrame.CommandRecordedNodes >= 1u);
		CUI_EXPECT_TRUE(preparedContentFrame.SceneSurfacesOpened
			> frame.SceneSurfacesOpened);
		uint64_t contentPixelDigest = 0;
		size_t contentNonTransparentPixels = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, readbackWidth, readbackHeight, contentPixelDigest,
			contentNonTransparentPixels));
		CUI_EXPECT_TRUE(contentPixelDigest != firstPixelDigest);
		CUI_EXPECT_TRUE(contentNonTransparentPixels > 0u);
		auto contentComposition = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!contentComposition.Error.empty())
			throw std::runtime_error("Content final capture failed: "
				+ contentComposition.Error);
		RECT contentRedBounds{};
		size_t contentRedPixels = 0;
		CUI_EXPECT_TRUE(contentComposition.TryGetColorBounds(
			26u, 51u, 204u, contentRedBounds, contentRedPixels));
		CUI_EXPECT_EQ(144ULL, contentRedPixels);
		CUI_EXPECT_EQ(16L, contentRedBounds.left
			- compositionOnlyBlueBounds.left);
		CUI_EXPECT_TRUE(contentComposition.Digest
			!= compositionOnlyCapture.Digest);
		const auto recoveriesBefore =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		const auto recoveredFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(0ULL, recoveredFrame.CompositionOnlySegments);
		CUI_EXPECT_TRUE(recoveredFrame.CommandRecordedNodes >= 1u);
		CUI_EXPECT_EQ(recoveriesBefore + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		CUI_EXPECT_TRUE(
			scene.TryGetTargetPresentationSnapshotForTesting(0u, snapshot));
		CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
		CUI_EXPECT_TRUE(snapshot.HasDrawingCommands);
		CUI_EXPECT_EQ(isolatedSurfaceWidth,
			snapshot.CompositionSurfacePhysicalWidth);
		CUI_EXPECT_EQ(isolatedSurfaceHeight,
			snapshot.CompositionSurfacePhysicalHeight);
		uint64_t recoveredPixelDigest = 0;
		size_t recoveredNonTransparentPixels = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, readbackWidth, readbackHeight, recoveredPixelDigest,
			recoveredNonTransparentPixels));
		CUI_EXPECT_EQ(contentPixelDigest, recoveredPixelDigest);
		CUI_EXPECT_EQ(contentNonTransparentPixels,
			recoveredNonTransparentPixels);
		auto recoveredComposition = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!recoveredComposition.Error.empty())
			throw std::runtime_error("Recovered final capture failed: "
				+ recoveredComposition.Error);
		RECT recoveredRedBounds{};
		size_t recoveredRedPixels = 0;
		CUI_EXPECT_TRUE(recoveredComposition.TryGetColorBounds(
			26u, 51u, 204u, recoveredRedBounds, recoveredRedPixels));
		CUI_EXPECT_EQ(144ULL, recoveredRedPixels);
		CUI_EXPECT_EQ(contentRedBounds.left, recoveredRedBounds.left);
		CUI_EXPECT_EQ(contentRedBounds.top, recoveredRedBounds.top);
		CUI_EXPECT_EQ(contentRedBounds.right, recoveredRedBounds.right);
		CUI_EXPECT_EQ(contentRedBounds.bottom, recoveredRedBounds.bottom);
		CUI_EXPECT_EQ(abortedBefore,
			scene.PresentationAbortedFrameCount());

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_PIXEL_OUTPUT") != 0)
			throw std::runtime_error("Could not query CUI_DCOMP_PIXEL_OUTPUT.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(), L"CUI_DCOMP_PIXEL_OUTPUT") != 0)
				throw std::runtime_error("Could not read CUI_DCOMP_PIXEL_OUTPUT.");
			const wchar_t* output = outputBuffer.data();
			const std::filesystem::path outputPath(output);
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"CUI_DCOMP_PIXEL_OUTPUT must be a JSON file under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			auto sampleJson = [&boundsJson](
				int atMilliseconds,
				size_t matchingPixels,
				const RECT& bounds,
				uint64_t finalDigest,
				uint64_t surfaceDigest)
			{
				return std::string("{\"atMilliseconds\":")
					+ std::to_string(atMilliseconds)
					+ ",\"matchingPixels\":"
					+ std::to_string(matchingPixels)
					+ ",\"bounds\":" + boundsJson(bounds)
					+ ",\"finalBgraFnv64\":\""
					+ std::to_string(finalDigest)
					+ "\",\"surfaceBgraFnv64\":\""
					+ std::to_string(surfaceDigest) + "\"}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\": 1,\n")
				+ "  \"engine\": \"CUI\",\n"
				+ "  \"capture\": \"Windows.Graphics.Capture\",\n"
				+ "  \"bitmapWidth\": "
				+ std::to_string(firstComposition.Width) + ",\n"
				+ "  \"bitmapHeight\": "
				+ std::to_string(firstComposition.Height) + ",\n"
				+ "  \"samples\": [\n    "
				+ sampleJson(160, firstBluePixels, firstBlueBounds,
					firstComposition.Digest, firstPixelDigest)
				+ ",\n    "
				+ sampleJson(320, compositionOnlyBluePixels,
					compositionOnlyBlueBounds,
					compositionOnlyCapture.Digest,
					compositionOnlyPixelDigest)
				+ ",\n    "
				+ sampleJson(480, contentRedPixels, contentRedBounds,
					contentComposition.Digest, contentPixelDigest)
				+ "\n  ],\n"
				+ "  \"recoveryBounds\": "
				+ boundsJson(recoveredRedBounds) + ",\n"
				+ "  \"recoveryMatchingPixels\": "
				+ std::to_string(recoveredRedPixels) + "\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error("Could not write CUI pixel result: "
					+ Convert::UnicodeToUtf8(writeError));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(1ULL,
			scene.SynchronizePresentationLayerCountForTesting());
		CUI_EXPECT_TRUE(
			scene.TryGetTargetPresentationSnapshotForTesting(0u, snapshot));
		CUI_EXPECT_FALSE(snapshot.CompositionIsolated);
		CUI_EXPECT_EQ(abortedBefore,
			scene.PresentationAbortedFrameCount());
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation sibling DComp layers preserve overlapping z order", []
	{
		BenchmarkScene scene(
			2u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetTargetCanvasPositionForTesting(0u, 0.0f, 0.0f);
		scene.SetTargetCanvasPositionForTesting(1u, 6.0f, 0.0f);
		scene.SetTargetBackgroundForTesting(0u,
			D2D1_COLOR_F{ 0.15f, 0.45f, 0.85f, 1.0f });
		scene.SetTargetBackgroundForTesting(1u,
			D2D1_COLOR_F{ 0.8f, 0.2f, 0.1f, 1.0f });
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
		scene.ForcePresentationUpdateForTesting();

		PresentationNodeSnapshot firstTarget;
		PresentationNodeSnapshot secondTarget;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, firstTarget));
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			1u, secondTarget));
		CUI_EXPECT_TRUE(firstTarget.CompositionIsolated);
		CUI_EXPECT_TRUE(secondTarget.CompositionIsolated);
		CUI_EXPECT_TRUE(firstTarget.SegmentIndex
			!= secondTarget.SegmentIndex);
		const auto firstFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(0ULL, firstFrame.CompositionOnlySegments);
		CUI_EXPECT_TRUE(firstFrame.SceneSurfacesOpened >= 3u);

		UINT width = 0;
		UINT height = 0;
		uint64_t firstSurfaceDigest = 0;
		uint64_t secondSurfaceDigest = 0;
		size_t firstOpaque = 0;
		size_t secondOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, firstSurfaceDigest, firstOpaque));
		CUI_EXPECT_EQ(144ULL, firstOpaque);
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			1u, width, height, secondSurfaceDigest, secondOpaque));
		CUI_EXPECT_EQ(144ULL, secondOpaque);
		CUI_EXPECT_TRUE(firstSurfaceDigest != secondSurfaceDigest);

		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("Sibling final capture failed: "
				+ firstCapture.Error);
		RECT blueBounds{};
		RECT redBounds{};
		size_t bluePixels = 0;
		size_t redPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u, blueBounds, bluePixels));
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			26u, 51u, 204u, redBounds, redPixels));
		CUI_EXPECT_EQ(72ULL, bluePixels);
		CUI_EXPECT_EQ(144ULL, redPixels);
		CUI_EXPECT_EQ(16L, blueBounds.left);
		CUI_EXPECT_EQ(22L, blueBounds.right);
		CUI_EXPECT_EQ(22L, redBounds.left);
		CUI_EXPECT_EQ(34L, redBounds.right);
		CUI_EXPECT_EQ(24L, blueBounds.top);
		CUI_EXPECT_EQ(24L, redBounds.top);

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 320u);
		scene.ForcePresentationUpdateForTesting();
		const auto secondFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(2ULL, secondFrame.CompositionOnlySegments);
		CUI_EXPECT_TRUE(secondFrame.SceneSurfacesOpened
			< firstFrame.SceneSurfacesOpened);
		uint64_t movedFirstSurfaceDigest = 0;
		uint64_t movedSecondSurfaceDigest = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, movedFirstSurfaceDigest, firstOpaque));
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			1u, width, height, movedSecondSurfaceDigest, secondOpaque));
		CUI_EXPECT_EQ(firstSurfaceDigest, movedFirstSurfaceDigest);
		CUI_EXPECT_EQ(secondSurfaceDigest, movedSecondSurfaceDigest);

		auto movedCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!movedCapture.Error.empty())
			throw std::runtime_error("Moved sibling capture failed: "
				+ movedCapture.Error);
		RECT movedBlueBounds{};
		RECT movedRedBounds{};
		CUI_EXPECT_TRUE(movedCapture.TryGetColorBounds(
			217u, 115u, 38u, movedBlueBounds, bluePixels));
		CUI_EXPECT_TRUE(movedCapture.TryGetColorBounds(
			26u, 51u, 204u, movedRedBounds, redPixels));
		CUI_EXPECT_EQ(72ULL, bluePixels);
		CUI_EXPECT_EQ(144ULL, redPixels);
		CUI_EXPECT_EQ(16L, movedBlueBounds.left - blueBounds.left);
		CUI_EXPECT_EQ(16L, movedRedBounds.left - redBounds.left);
		CUI_EXPECT_TRUE(movedCapture.Digest != firstCapture.Digest);

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(1ULL,
			scene.SynchronizePresentationLayerCountForTesting());
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation nested DComp targets apply each transform once", []
	{
		BenchmarkScene scene(
			2u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.NestTargetForTesting(1u, 0u);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 40.0f, 24.0f, 20.0f, 30.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 12.0f, 12.0f, 10.0f, 6.0f);
		scene.SetTargetBackgroundForTesting(0u,
			D2D1_COLOR_F{ 0.15f, 0.45f, 0.85f, 1.0f });
		scene.SetTargetBackgroundForTesting(1u,
			D2D1_COLOR_F{ 0.8f, 0.2f, 0.1f, 1.0f });
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 80u);
		scene.ForcePresentationUpdateForTesting();

		PresentationNodeSnapshot outerSnapshot;
		PresentationNodeSnapshot innerSnapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, outerSnapshot));
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			1u, innerSnapshot));
		CUI_EXPECT_TRUE(outerSnapshot.CompositionIsolated);
		CUI_EXPECT_TRUE(outerSnapshot.CompositionIsolationRoot);
		CUI_EXPECT_EQ(1ULL, outerSnapshot.CompositionIsolationDepth);
		CUI_EXPECT_TRUE(innerSnapshot.CompositionIsolated);
		CUI_EXPECT_TRUE(innerSnapshot.CompositionIsolationRoot);
		CUI_EXPECT_EQ(2ULL, innerSnapshot.CompositionIsolationDepth);
		CUI_EXPECT_TRUE(outerSnapshot.SegmentIndex
			!= innerSnapshot.SegmentIndex);
		const auto firstFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(0ULL, firstFrame.CompositionOnlySegments);
		CUI_EXPECT_TRUE(firstFrame.SceneSurfacesOpened >= 3u);

		UINT outerWidth = 0;
		UINT outerHeight = 0;
		UINT innerWidth = 0;
		UINT innerHeight = 0;
		uint64_t outerSurfaceDigest = 0;
		uint64_t innerSurfaceDigest = 0;
		size_t outerOpaque = 0;
		size_t innerOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, outerWidth, outerHeight,
			outerSurfaceDigest, outerOpaque));
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			1u, innerWidth, innerHeight,
			innerSurfaceDigest, innerOpaque));
		CUI_EXPECT_EQ(960ULL, outerOpaque);
		CUI_EXPECT_EQ(144ULL, innerOpaque);

		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("Nested transform capture failed: "
				+ firstCapture.Error);
		RECT firstOuterBounds{};
		RECT firstInnerBounds{};
		size_t firstOuterPixels = 0;
		size_t firstInnerPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u,
			firstOuterBounds, firstOuterPixels, 12u));
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			26u, 51u, 204u,
			firstInnerBounds, firstInnerPixels, 12u));
		CUI_EXPECT_EQ(816ULL, firstOuterPixels);
		CUI_EXPECT_EQ(144ULL, firstInnerPixels);
		CUI_EXPECT_EQ(28L, firstOuterBounds.left);
		CUI_EXPECT_EQ(54L, firstOuterBounds.top);
		CUI_EXPECT_EQ(68L, firstOuterBounds.right);
		CUI_EXPECT_EQ(78L, firstOuterBounds.bottom);
		CUI_EXPECT_EQ(46L, firstInnerBounds.left);
		CUI_EXPECT_EQ(60L, firstInnerBounds.top);
		CUI_EXPECT_EQ(58L, firstInnerBounds.right);
		CUI_EXPECT_EQ(72L, firstInnerBounds.bottom);

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
		scene.ForcePresentationUpdateForTesting();
		const auto secondFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(2ULL, secondFrame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, secondFrame.CommandRecordedNodes);
		CUI_EXPECT_TRUE(secondFrame.CommandCacheHitNodes >= 2u);
		uint64_t movedOuterSurfaceDigest = 0;
		uint64_t movedInnerSurfaceDigest = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, outerWidth, outerHeight,
			movedOuterSurfaceDigest, outerOpaque));
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			1u, innerWidth, innerHeight,
			movedInnerSurfaceDigest, innerOpaque));
		CUI_EXPECT_EQ(outerSurfaceDigest, movedOuterSurfaceDigest);
		CUI_EXPECT_EQ(innerSurfaceDigest, movedInnerSurfaceDigest);

		auto secondCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!secondCapture.Error.empty())
			throw std::runtime_error("Moved nested transform capture failed: "
				+ secondCapture.Error);
		RECT secondOuterBounds{};
		RECT secondInnerBounds{};
		size_t secondOuterPixels = 0;
		size_t secondInnerPixels = 0;
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			217u, 115u, 38u,
			secondOuterBounds, secondOuterPixels, 12u));
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			26u, 51u, 204u,
			secondInnerBounds, secondInnerPixels, 12u));
		CUI_EXPECT_EQ(816ULL, secondOuterPixels);
		CUI_EXPECT_EQ(144ULL, secondInnerPixels);
		CUI_EXPECT_EQ(36L, secondOuterBounds.left);
		CUI_EXPECT_EQ(54L, secondOuterBounds.top);
		CUI_EXPECT_EQ(76L, secondOuterBounds.right);
		CUI_EXPECT_EQ(78L, secondOuterBounds.bottom);
		CUI_EXPECT_EQ(62L, secondInnerBounds.left);
		CUI_EXPECT_EQ(60L, secondInnerBounds.top);
		CUI_EXPECT_EQ(74L, secondInnerBounds.right);
		CUI_EXPECT_EQ(72L, secondInnerBounds.bottom);
		CUI_EXPECT_EQ(8L,
			secondOuterBounds.left - firstOuterBounds.left);
		CUI_EXPECT_EQ(16L,
			secondInnerBounds.left - firstInnerBounds.left);

		const auto recoveriesBefore =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		const auto recoveredFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(0ULL, recoveredFrame.CompositionOnlySegments);
		CUI_EXPECT_TRUE(recoveredFrame.CommandRecordedNodes >= 2u);
		CUI_EXPECT_EQ(recoveriesBefore + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		uint64_t recoveredOuterSurfaceDigest = 0;
		uint64_t recoveredInnerSurfaceDigest = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, outerWidth, outerHeight,
			recoveredOuterSurfaceDigest, outerOpaque));
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			1u, innerWidth, innerHeight,
			recoveredInnerSurfaceDigest, innerOpaque));
		CUI_EXPECT_EQ(outerSurfaceDigest, recoveredOuterSurfaceDigest);
		CUI_EXPECT_EQ(innerSurfaceDigest, recoveredInnerSurfaceDigest);
		auto recoveredCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!recoveredCapture.Error.empty())
			throw std::runtime_error(
				"Recovered nested transform capture failed: "
				+ recoveredCapture.Error);
		RECT recoveredOuterBounds{};
		RECT recoveredInnerBounds{};
		size_t recoveredOuterPixels = 0;
		size_t recoveredInnerPixels = 0;
		CUI_EXPECT_TRUE(recoveredCapture.TryGetColorBounds(
			217u, 115u, 38u,
			recoveredOuterBounds, recoveredOuterPixels, 12u));
		CUI_EXPECT_TRUE(recoveredCapture.TryGetColorBounds(
			26u, 51u, 204u,
			recoveredInnerBounds, recoveredInnerPixels, 12u));
		CUI_EXPECT_EQ(secondOuterPixels, recoveredOuterPixels);
		CUI_EXPECT_EQ(secondInnerPixels, recoveredInnerPixels);
		CUI_EXPECT_EQ(secondOuterBounds.left, recoveredOuterBounds.left);
		CUI_EXPECT_EQ(secondOuterBounds.top, recoveredOuterBounds.top);
		CUI_EXPECT_EQ(secondOuterBounds.right, recoveredOuterBounds.right);
		CUI_EXPECT_EQ(secondOuterBounds.bottom, recoveredOuterBounds.bottom);
		CUI_EXPECT_EQ(secondInnerBounds.left, recoveredInnerBounds.left);
		CUI_EXPECT_EQ(secondInnerBounds.top, recoveredInnerBounds.top);
		CUI_EXPECT_EQ(secondInnerBounds.right, recoveredInnerBounds.right);
		CUI_EXPECT_EQ(secondInnerBounds.bottom, recoveredInnerBounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_NESTED_TRANSFORM_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query nested transform output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_NESTED_TRANSFORM_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read nested transform output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Nested transform output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"nested-render-transform\",\n"
				+ "  \"outerSamples\":[\n"
				+ "    {\"atMilliseconds\":80,\"matchingPixels\":"
				+ std::to_string(firstOuterPixels) + ",\"bounds\":"
				+ boundsJson(firstOuterBounds) + "},\n"
				+ "    {\"atMilliseconds\":160,\"matchingPixels\":"
				+ std::to_string(secondOuterPixels) + ",\"bounds\":"
				+ boundsJson(secondOuterBounds) + "}\n  ],\n"
				+ "  \"innerSamples\":[\n"
				+ "    {\"atMilliseconds\":80,\"matchingPixels\":"
				+ std::to_string(firstInnerPixels) + ",\"bounds\":"
				+ boundsJson(firstInnerBounds) + "},\n"
				+ "    {\"atMilliseconds\":160,\"matchingPixels\":"
				+ std::to_string(secondInnerPixels) + ",\"bounds\":"
				+ boundsJson(secondInnerBounds) + "}\n  ],\n"
				+ "  \"outerSurface\":{\"width\":"
				+ std::to_string(outerWidth) + ",\"height\":"
				+ std::to_string(outerHeight) + ",\"opaquePixels\":"
				+ std::to_string(outerOpaque) + "},\n"
				+ "  \"innerSurface\":{\"width\":"
				+ std::to_string(innerWidth) + ",\"height\":"
				+ std::to_string(innerHeight) + ",\"opaquePixels\":"
				+ std::to_string(innerOpaque) + "},\n"
				+ "  \"surfaceDigestsStable\":true,\n"
				+ "  \"secondFrameCompositionOnlySegments\":2,\n"
				+ "  \"secondFrameCommandRecordedNodes\":0,\n"
				+ "  \"deviceRecoveryPixelsStable\":true\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write nested transform result: "
					+ Convert::UnicodeToUtf8(writeError));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(1ULL,
			scene.SynchronizePresentationLayerCountForTesting());
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation nested DComp target inherits moving ancestor clip", []
	{
		BenchmarkScene scene(
			2u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.NestTargetForTesting(1u, 0u);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 40.0f, 24.0f, 20.0f, 30.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 12.0f, 12.0f, 28.0f, 6.0f);
		scene.SetTargetClipToBoundsForTesting(0u, true);
		scene.SetTargetBackgroundForTesting(0u,
			D2D1_COLOR_F{ 0.15f, 0.45f, 0.85f, 1.0f });
		scene.SetTargetBackgroundForTesting(1u,
			D2D1_COLOR_F{ 0.8f, 0.2f, 0.1f, 1.0f });
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 40u);
		scene.ForcePresentationUpdateForTesting();

		PresentationNodeSnapshot outerSnapshot;
		PresentationNodeSnapshot innerSnapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, outerSnapshot));
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			1u, innerSnapshot));
		CUI_EXPECT_EQ(1ULL, outerSnapshot.CompositionIsolationDepth);
		CUI_EXPECT_EQ(2ULL, innerSnapshot.CompositionIsolationDepth);
		CUI_EXPECT_TRUE(outerSnapshot.SegmentIndex
			!= innerSnapshot.SegmentIndex);

		UINT outerWidth = 0;
		UINT outerHeight = 0;
		UINT innerWidth = 0;
		UINT innerHeight = 0;
		uint64_t firstOuterSurfaceDigest = 0;
		uint64_t firstInnerSurfaceDigest = 0;
		size_t outerOpaque = 0;
		size_t innerOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, outerWidth, outerHeight,
			firstOuterSurfaceDigest, outerOpaque));
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			1u, innerWidth, innerHeight,
			firstInnerSurfaceDigest, innerOpaque));
		CUI_EXPECT_EQ(960ULL, outerOpaque);
		CUI_EXPECT_EQ(144ULL, innerOpaque);

		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("Nested clip capture failed: "
				+ firstCapture.Error);
		RECT firstOuterBounds{};
		RECT firstInnerBounds{};
		size_t firstOuterPixels = 0;
		size_t firstInnerPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u,
			firstOuterBounds, firstOuterPixels, 12u));
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			26u, 51u, 204u,
			firstInnerBounds, firstInnerPixels, 12u));
		CUI_EXPECT_EQ(864ULL, firstOuterPixels);
		CUI_EXPECT_EQ(96ULL, firstInnerPixels);
		CUI_EXPECT_EQ(24L, firstOuterBounds.left);
		CUI_EXPECT_EQ(54L, firstOuterBounds.top);
		CUI_EXPECT_EQ(64L, firstOuterBounds.right);
		CUI_EXPECT_EQ(78L, firstOuterBounds.bottom);
		CUI_EXPECT_EQ(56L, firstInnerBounds.left);
		CUI_EXPECT_EQ(60L, firstInnerBounds.top);
		CUI_EXPECT_EQ(64L, firstInnerBounds.right);
		CUI_EXPECT_EQ(72L, firstInnerBounds.bottom);

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 80u);
		scene.ForcePresentationUpdateForTesting();
		const auto secondFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(2ULL, secondFrame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, secondFrame.CommandRecordedNodes);
		CUI_EXPECT_TRUE(secondFrame.CommandCacheHitNodes >= 2u);
		uint64_t secondOuterSurfaceDigest = 0;
		uint64_t secondInnerSurfaceDigest = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, outerWidth, outerHeight,
			secondOuterSurfaceDigest, outerOpaque));
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			1u, innerWidth, innerHeight,
			secondInnerSurfaceDigest, innerOpaque));
		CUI_EXPECT_EQ(firstOuterSurfaceDigest, secondOuterSurfaceDigest);
		CUI_EXPECT_EQ(firstInnerSurfaceDigest, secondInnerSurfaceDigest);

		auto secondCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!secondCapture.Error.empty())
			throw std::runtime_error("Moved nested clip capture failed: "
				+ secondCapture.Error);
		RECT secondOuterBounds{};
		RECT secondInnerBounds{};
		size_t secondOuterPixels = 0;
		size_t secondInnerPixels = 0;
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			217u, 115u, 38u,
			secondOuterBounds, secondOuterPixels, 12u));
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			26u, 51u, 204u,
			secondInnerBounds, secondInnerPixels, 12u));
		CUI_EXPECT_EQ(912ULL, secondOuterPixels);
		CUI_EXPECT_EQ(48ULL, secondInnerPixels);
		CUI_EXPECT_EQ(28L, secondOuterBounds.left);
		CUI_EXPECT_EQ(54L, secondOuterBounds.top);
		CUI_EXPECT_EQ(68L, secondOuterBounds.right);
		CUI_EXPECT_EQ(78L, secondOuterBounds.bottom);
		CUI_EXPECT_EQ(64L, secondInnerBounds.left);
		CUI_EXPECT_EQ(60L, secondInnerBounds.top);
		CUI_EXPECT_EQ(68L, secondInnerBounds.right);
		CUI_EXPECT_EQ(72L, secondInnerBounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_NESTED_CLIP_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query nested clip output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_NESTED_CLIP_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read nested clip output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Nested clip output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"nested-moving-ancestor-clip\",\n"
				+ "  \"outerSamples\":[\n"
				+ "    {\"atMilliseconds\":40,\"matchingPixels\":"
				+ std::to_string(firstOuterPixels) + ",\"bounds\":"
				+ boundsJson(firstOuterBounds) + "},\n"
				+ "    {\"atMilliseconds\":80,\"matchingPixels\":"
				+ std::to_string(secondOuterPixels) + ",\"bounds\":"
				+ boundsJson(secondOuterBounds) + "}\n  ],\n"
				+ "  \"innerSamples\":[\n"
				+ "    {\"atMilliseconds\":40,\"matchingPixels\":"
				+ std::to_string(firstInnerPixels) + ",\"bounds\":"
				+ boundsJson(firstInnerBounds) + "},\n"
				+ "    {\"atMilliseconds\":80,\"matchingPixels\":"
				+ std::to_string(secondInnerPixels) + ",\"bounds\":"
				+ boundsJson(secondInnerBounds) + "}\n  ],\n"
				+ "  \"outerSurface\":{\"width\":"
				+ std::to_string(outerWidth) + ",\"height\":"
				+ std::to_string(outerHeight) + ",\"opaquePixels\":"
				+ std::to_string(outerOpaque) + "},\n"
				+ "  \"innerSurface\":{\"width\":"
				+ std::to_string(innerWidth) + ",\"height\":"
				+ std::to_string(innerHeight) + ",\"opaquePixels\":"
				+ std::to_string(innerOpaque) + "},\n"
				+ "  \"surfaceDigestsStable\":true,\n"
				+ "  \"secondFrameCompositionOnlySegments\":2,\n"
				+ "  \"secondFrameCommandRecordedNodes\":0\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write nested clip result: "
					+ Convert::UnicodeToUtf8(writeError));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation nested DComp flattening preserves trailing z order", []
	{
		BenchmarkScene scene(
			2u, 0u, BenchmarkPropertyKind::TransformX);
		scene.NestTargetForTesting(1u, 0u);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 40.0f, 24.0f, 20.0f, 30.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 12.0f, 12.0f, 10.0f, 6.0f);
		scene.SetTargetBackgroundForTesting(0u,
			D2D1_COLOR_F{ 0.15f, 0.45f, 0.85f, 1.0f });
		scene.SetTargetBackgroundForTesting(1u,
			D2D1_COLOR_F{ 0.8f, 0.2f, 0.1f, 1.0f });
		CUI_EXPECT_TRUE(scene.AddTargetChildRectangleForTesting(
			0u, 8.0f, 12.0f, 22.0f, 6.0f,
			D2D1_COLOR_F{ 0.2f, 0.7f, 0.3f, 1.0f }) != nullptr);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 80u);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(4ULL,
			scene.SynchronizePresentationLayerCountForTesting());
		const auto firstFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_TRUE(firstFrame.SceneSurfacesOpened >= 4u);
		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("Nested z-order capture failed: "
				+ firstCapture.Error);
		RECT firstInnerBounds{};
		RECT firstTrailingBounds{};
		size_t firstInnerPixels = 0;
		size_t firstTrailingPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			26u, 51u, 204u,
			firstInnerBounds, firstInnerPixels, 12u));
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			76u, 179u, 51u,
			firstTrailingBounds, firstTrailingPixels, 12u));
		CUI_EXPECT_EQ(48ULL, firstInnerPixels);
		CUI_EXPECT_EQ(96ULL, firstTrailingPixels);
		CUI_EXPECT_EQ(46L, firstInnerBounds.left);
		CUI_EXPECT_EQ(60L, firstInnerBounds.top);
		CUI_EXPECT_EQ(50L, firstInnerBounds.right);
		CUI_EXPECT_EQ(72L, firstInnerBounds.bottom);
		CUI_EXPECT_EQ(50L, firstTrailingBounds.left);
		CUI_EXPECT_EQ(60L, firstTrailingBounds.top);
		CUI_EXPECT_EQ(58L, firstTrailingBounds.right);
		CUI_EXPECT_EQ(72L, firstTrailingBounds.bottom);

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
		scene.ForcePresentationUpdateForTesting();
		const auto secondFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(3ULL, secondFrame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, secondFrame.CommandRecordedNodes);
		CUI_EXPECT_TRUE(secondFrame.CommandCacheHitNodes >= 3u);
		auto secondCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!secondCapture.Error.empty())
			throw std::runtime_error("Moved nested z-order capture failed: "
				+ secondCapture.Error);
		RECT secondInnerBounds{};
		RECT secondTrailingBounds{};
		size_t secondInnerPixels = 0;
		size_t secondTrailingPixels = 0;
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			26u, 51u, 204u,
			secondInnerBounds, secondInnerPixels, 12u));
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			76u, 179u, 51u,
			secondTrailingBounds, secondTrailingPixels, 12u));
		CUI_EXPECT_EQ(96ULL, secondInnerPixels);
		CUI_EXPECT_EQ(96ULL, secondTrailingPixels);
		CUI_EXPECT_EQ(66L, secondInnerBounds.left);
		CUI_EXPECT_EQ(60L, secondInnerBounds.top);
		CUI_EXPECT_EQ(74L, secondInnerBounds.right);
		CUI_EXPECT_EQ(72L, secondInnerBounds.bottom);
		CUI_EXPECT_EQ(58L, secondTrailingBounds.left);
		CUI_EXPECT_EQ(60L, secondTrailingBounds.top);
		CUI_EXPECT_EQ(66L, secondTrailingBounds.right);
		CUI_EXPECT_EQ(72L, secondTrailingBounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_NESTED_ZORDER_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query nested z-order output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_NESTED_ZORDER_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read nested z-order output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Nested z-order output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"nested-trailing-z-order\",\n"
				+ "  \"innerSamples\":[\n"
				+ "    {\"atMilliseconds\":80,\"matchingPixels\":"
				+ std::to_string(firstInnerPixels) + ",\"bounds\":"
				+ boundsJson(firstInnerBounds) + "},\n"
				+ "    {\"atMilliseconds\":160,\"matchingPixels\":"
				+ std::to_string(secondInnerPixels) + ",\"bounds\":"
				+ boundsJson(secondInnerBounds) + "}\n  ],\n"
				+ "  \"trailingSamples\":[\n"
				+ "    {\"atMilliseconds\":80,\"matchingPixels\":"
				+ std::to_string(firstTrailingPixels) + ",\"bounds\":"
				+ boundsJson(firstTrailingBounds) + "},\n"
				+ "    {\"atMilliseconds\":160,\"matchingPixels\":"
				+ std::to_string(secondTrailingPixels) + ",\"bounds\":"
				+ boundsJson(secondTrailingBounds) + "}\n  ],\n"
				+ "  \"secondFrameCompositionOnlySegments\":3,\n"
				+ "  \"secondFrameCommandRecordedNodes\":0\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write nested z-order result: "
					+ Convert::UnicodeToUtf8(writeError));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation 200 sibling DComp targets skip independent surfaces", []
	{
		BenchmarkScene scene(
			200u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		const auto abortedBefore = scene.PresentationAbortedFrameCount();
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(201ULL,
			scene.SynchronizePresentationLayerCountForTesting());
		const auto firstFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(0ULL, firstFrame.CompositionOnlySegments);
		CUI_EXPECT_TRUE(firstFrame.SceneSurfacesOpened >= 201u);

		for (const size_t targetIndex : std::array<size_t, 2>{ 0u, 199u })
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
				targetIndex, snapshot));
			CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
			CUI_EXPECT_TRUE(snapshot.CompositionSurfacePhysicalWidth <= 16u);
			CUI_EXPECT_TRUE(snapshot.CompositionSurfacePhysicalHeight <= 16u);
			UINT width = 0;
			UINT height = 0;
			uint64_t digest = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				targetIndex, width, height, digest, opaque));
			CUI_EXPECT_EQ(144ULL, opaque);
		}

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 320u);
		scene.ForcePresentationUpdateForTesting();
		const auto secondFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(200ULL, secondFrame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, secondFrame.CommandRecordedNodes);
		CUI_EXPECT_TRUE(secondFrame.CommandCacheHitNodes >= 200u);
		CUI_EXPECT_TRUE(secondFrame.SceneSurfacesOpened
			< firstFrame.SceneSurfacesOpened);
		CUI_EXPECT_EQ(abortedBefore,
			scene.PresentationAbortedFrameCount());

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(1ULL,
			scene.SynchronizePresentationLayerCountForTesting());
		CUI_EXPECT_EQ(abortedBefore,
			scene.PresentationAbortedFrameCount());
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation 500 and 1000 target scene resources stay budgeted", []
	{
		struct ScaleSample final
		{
			size_t TargetCount = 0;
			double FirstFrameMicroseconds = 0.0;
			double SteadyFrameMicroseconds = 0.0;
			ProcessMemory MemoryBefore;
			ProcessMemory MemoryAfter;
			PresentationRenderHost::ResourceSnapshot First;
			PresentationRenderHost::ResourceSnapshot Steady;
			PresentationRenderHost::ResourceSnapshot Recovered;
			PresentationRenderHost::ResourceSnapshot Trimmed;
			PresentationFrameTimingStatistics FirstTiming;
			PresentationFrameTimingStatistics SteadyTiming;
			PresentationPreparationStatistics FirstPreparation;
			PresentationPreparationStatistics SteadyPreparation;
			Distribution Cadence;
			size_t CadenceFramesWithinBudget = 0;
			bool RecoveryExercised = false;
			size_t FirstPreSurfaceCulledNodes = 0;
			size_t FirstSceneSurfacesOpened = 0;
			double FirstSceneSurfaceOpenMicroseconds = 0.0;
			double FirstSceneSurfaceCloseMicroseconds = 0.0;
			double FirstSceneSurfaceEndDrawMicroseconds = 0.0;
			double FirstSceneSurfacePresentMicroseconds = 0.0;
			double FirstSceneSurfaceSubmitMicroseconds = 0.0;
			double FirstSceneCommandRecordMicroseconds = 0.0;
			double FirstSceneCommandReplayMicroseconds = 0.0;
			size_t SteadyCompositionOnlySegments = 0;
			size_t SteadyCulledNodes = 0;
			size_t SteadyPreSurfaceCulledNodes = 0;
			size_t SteadySceneSurfacesOpened = 0;
		};
		const auto frequency = QueryFrequency();
		std::optional<size_t> isolatedScale;
		size_t scaleLength = 0;
		if (::_wgetenv_s(&scaleLength, nullptr, 0,
			L"CUI_DCOMP_TARGET_RESOURCE_SCALE") != 0)
			throw std::runtime_error("Could not query target resource scale.");
		if (scaleLength > 1u)
		{
			std::vector<wchar_t> scaleBuffer(scaleLength);
			if (::_wgetenv_s(&scaleLength, scaleBuffer.data(),
				scaleBuffer.size(), L"CUI_DCOMP_TARGET_RESOURCE_SCALE") != 0)
				throw std::runtime_error("Could not read target resource scale.");
			const std::wstring_view value(scaleBuffer.data());
			if (value == L"500") isolatedScale = 500u;
			else if (value == L"1000") isolatedScale = 1000u;
			else throw std::runtime_error(
				"Target resource scale must be 500 or 1000.");
		}
		auto runScale = [&](size_t targetCount, bool recover)
		{
			ScaleSample sample;
			sample.TargetCount = targetCount;
			sample.MemoryBefore = QueryProcessMemory();
			{
				BenchmarkScene scene(
					targetCount, 0u, BenchmarkPropertyKind::TransformX);
				scene.AddNativeCompositionBoundaryForTesting();
				scene.ShowOffscreenWithoutActivationForTesting();
				const auto aborts = scene.PresentationAbortedFrameCount();
				scene.Begin();
				scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
				sample.FirstFrameMicroseconds = MeasureMicroseconds(
					frequency, [&] { scene.ForcePresentationUpdateForTesting(); });
				sample.First = scene.PresentationResourcesForTesting();
				const auto firstFrame = scene.PresentationFrameForTesting();
				sample.FirstTiming = firstFrame.Timing;
				sample.FirstPreparation = firstFrame.Preparation;
				sample.FirstPreSurfaceCulledNodes =
					firstFrame.PreSurfaceCulledNodes;
				sample.FirstSceneSurfacesOpened =
					firstFrame.SceneSurfacesOpened;
				sample.FirstSceneSurfaceOpenMicroseconds =
					firstFrame.SceneSurfaceOpenMicroseconds;
				sample.FirstSceneSurfaceCloseMicroseconds =
					firstFrame.SceneSurfaceCloseMicroseconds;
				sample.FirstSceneSurfaceEndDrawMicroseconds =
					firstFrame.SceneSurfaceEndDrawMicroseconds;
				sample.FirstSceneSurfacePresentMicroseconds =
					firstFrame.SceneSurfacePresentMicroseconds;
				sample.FirstSceneSurfaceSubmitMicroseconds =
					firstFrame.SceneSurfaceSubmitMicroseconds;
				sample.FirstSceneCommandRecordMicroseconds =
					firstFrame.SceneCommandRecordMicroseconds;
				sample.FirstSceneCommandReplayMicroseconds =
					firstFrame.SceneCommandReplayMicroseconds;
				CUI_EXPECT_EQ(targetCount + 1u,
					sample.First.SceneLayerSlotCount);
				CUI_EXPECT_TRUE(sample.First.SceneLayerSlotCapacity
					>= sample.First.SceneLayerSlotCount);
				CUI_EXPECT_TRUE(
					sample.First.EstimatedSceneLayerSlotBytes > 0u);
				CUI_EXPECT_EQ(
					sample.First.EstimatedSceneSwapChainBytes
						+ sample.First.EstimatedSceneCompositionSurfaceBytes
						+ sample.First.EstimatedSceneSubmittedSnapshotBytes
						+ sample.First.EstimatedSceneLayerSlotBytes,
					sample.First.EstimatedSceneRetainedBytes);
				CUI_EXPECT_EQ(sample.First.SceneLayerCount,
					sample.First.SceneLayerSwapChainCount
						+ sample.First.SceneLayerCompositionSurfaceCount);
				CUI_EXPECT_EQ(
					sample.First.SceneLayerCompositionSurfaceCount,
					sample.First.SceneLayerDirect2DSurfaceContextCount);
				CUI_EXPECT_EQ(0ULL,
					sample.First.SceneLayerPixelReadbackLeaseCount);
				CUI_EXPECT_EQ(0ULL,
					sample.First.SceneLayerSubmittedSnapshotTextureCount);
				CUI_EXPECT_EQ(0ULL,
					sample.First.EstimatedSceneSubmittedSnapshotBytes);
				CUI_EXPECT_EQ(0ULL,
					sample.First.PeakEstimatedSceneSubmittedSnapshotBytes);
				CUI_EXPECT_EQ(0ULL,
					sample.First.SceneLayerSubmittedSnapshotCreateCount);
				CUI_EXPECT_EQ(0ULL,
					sample.First.SceneLayerSubmittedSnapshotUpdateCount);
				CUI_EXPECT_EQ(0ULL,
					sample.First.SceneLayerSubmittedSnapshotCopiedBytes);
				CUI_EXPECT_EQ(0.0,
					sample.First.SceneLayerSubmittedSnapshotCreateMicroseconds);
				CUI_EXPECT_EQ(0.0,
					sample.First.SceneLayerSubmittedSnapshotCopyMicroseconds);
				CUI_EXPECT_TRUE(
					(sample.First.SceneLayerSwapChainCount == sample.First.SceneLayerCount
						&& sample.First.SceneLayerCompositionSurfaceCount == 0u)
					|| (sample.First.SceneLayerCompositionSurfaceCount
							== sample.First.SceneLayerCount
						&& sample.First.SceneLayerSwapChainCount == 0u));
				CUI_EXPECT_TRUE(sample.First.SceneLayerCount
					< sample.First.SceneLayerSlotCount);
				CUI_EXPECT_EQ(1ULL, sample.First.FullWindowSceneLayerCount);
				CUI_EXPECT_EQ(1ULL,
					sample.First.SceneLayerDistinctGraphicsDeviceCount);
				CUI_EXPECT_EQ(1ULL,
					sample.First.SceneLayerDistinctRecorderDeviceCount);
				CUI_EXPECT_EQ(sample.First.SceneLayerCount,
					sample.First.SceneLayerSharedGraphicsDeviceCount);
				CUI_EXPECT_EQ(sample.First.SceneLayerCount,
					sample.First.SceneLayerSharedRecorderDeviceCount);
				CUI_EXPECT_EQ(1ULL,
					sample.First.SceneCommandRecorderCount);
				CUI_EXPECT_EQ(sample.First.SceneLayerCount,
					sample.First.SceneCommandRecorderReferenceCount);
				CUI_EXPECT_EQ(1ULL,
					sample.First.PrimaryPresentSyncInterval);
				CUI_EXPECT_EQ(1ULL,
					sample.First.OverlayPresentSyncInterval);
				CUI_EXPECT_EQ(0ULL,
					sample.First.MinimumScenePresentSyncInterval);
				CUI_EXPECT_EQ(0ULL,
					sample.First.MaximumScenePresentSyncInterval);
				CUI_EXPECT_EQ(sample.First.SceneLayerCount,
					sample.First.PeakSceneLayerCount);
				CUI_EXPECT_EQ(targetCount + 1u,
					sample.First.PeakSceneLayerSlotCount);
				CUI_EXPECT_TRUE(sample.First.DeviceGeneration > 0u);
				CUI_EXPECT_TRUE(sample.First.AdapterLuid != 0u);
				CUI_EXPECT_TRUE(sample.First.FeatureLevel >= 0xA000u);
				CUI_EXPECT_TRUE(sample.First.AdapterDescription[0] != L'\0');
				CUI_EXPECT_TRUE(
					sample.First.CompositionVisualStackRebuildCount <= 4u);
				CUI_EXPECT_TRUE(
					sample.First.CompositionVisualStackRebuildEntryCount
					<= sample.First.SceneLayerCount + 16u);
				CUI_EXPECT_TRUE(
					sample.First.CompositionVisualDeferredMutationCount
					>= sample.First.SceneLayerCount);
				CUI_EXPECT_TRUE(
					sample.First.CompositionVisualBatchCommitCount >= 1u);
				CUI_EXPECT_EQ(0ULL,
					sample.First.CompositionVisualBatchRollbackCount);
				CUI_EXPECT_EQ(0ULL,
					sample.First.CompositionVisualBatchRollbackFailureCount);
				CUI_EXPECT_TRUE(
					sample.First.SceneLayerSlotEnsureMicroseconds > 0.0);
				CUI_EXPECT_TRUE(
					sample.First.SceneLayerTopologyBatchBeginMicroseconds > 0.0);
				CUI_EXPECT_TRUE(
					(sample.First.SceneLayerSwapChainCreateMicroseconds > 0.0
						&& sample.First.SceneLayerCompositionSurfaceCreateMicroseconds
							== 0.0)
					|| (sample.First.SceneLayerCompositionSurfaceCreateMicroseconds
							> 0.0
						&& sample.First.SceneLayerSwapChainCreateMicroseconds == 0.0));
				CUI_EXPECT_TRUE(
					sample.First.SceneLayerVisualCreateMicroseconds > 0.0);
				CUI_EXPECT_TRUE(
					sample.First.SceneLayerVisualBindMicroseconds > 0.0);
				CUI_EXPECT_TRUE(
					sample.First.SceneLayerGraphicsCreateMicroseconds > 0.0);
				CUI_EXPECT_TRUE(
					sample.First.SceneLayerRecorderCreateMicroseconds > 0.0);
				CUI_EXPECT_TRUE(
					sample.First.SceneLayerDpiSetupMicroseconds > 0.0);
				CUI_EXPECT_TRUE(
					sample.First.SceneLayerVisualPropertyStageMicroseconds > 0.0);
				CUI_EXPECT_TRUE(
					sample.First.SceneLayerGroupStageMicroseconds > 0.0);
				CUI_EXPECT_TRUE(
					sample.First.SceneLayerResourcePeakUpdateMicroseconds > 0.0);
				CUI_EXPECT_TRUE(
					sample.First.SceneLayerTopologyBatchCommitMicroseconds > 0.0);
				const double trackedPreparationMicroseconds =
					sample.First.SceneLayerSlotEnsureMicroseconds
					+ sample.First.SceneLayerTopologyBatchBeginMicroseconds
					+ sample.First.SceneLayerSwapChainCreateMicroseconds
					+ sample.First.SceneLayerCompositionSurfaceCreateMicroseconds
					+ sample.First.SceneLayerVisualCreateMicroseconds
					+ sample.First.SceneLayerVisualBindMicroseconds
					+ sample.First.SceneLayerGraphicsCreateMicroseconds
					+ sample.First.SceneLayerRecorderCreateMicroseconds
					+ sample.First.SceneLayerDpiSetupMicroseconds
					+ sample.First.SceneLayerVisualPropertyStageMicroseconds
					+ sample.First.SceneLayerGroupStageMicroseconds
					+ sample.First.SceneLayerResourcePeakUpdateMicroseconds
					+ sample.First.SceneLayerTopologyBatchCommitMicroseconds;
				CUI_EXPECT_TRUE(trackedPreparationMicroseconds
					<= sample.FirstTiming.CompositionPreparationMicroseconds * 1.05);
				CUI_EXPECT_EQ(targetCount + 1u,
					sample.FirstPreparation.SegmentCount);
				CUI_EXPECT_EQ(sample.First.SceneLayerCount,
					sample.FirstPreparation.PhysicalLayerRequiredCount);
				CUI_EXPECT_EQ(firstFrame.PreSurfaceCulledNodes,
					sample.FirstPreparation.DeferredUnmaterializedCount);
				CUI_EXPECT_EQ(
					sample.FirstPreparation.DeferredUnmaterializedCount,
					sample.FirstPreparation.EarlyViewportDeferredCount);
				const double measuredPreparationMicroseconds =
					sample.FirstPreparation.ScratchMicroseconds
					+ sample.FirstPreparation.NodePreparationMicroseconds
					+ sample.FirstPreparation.SegmentMicroseconds
					+ sample.FirstPreparation.TopologyCommitMicroseconds
					+ sample.FirstPreparation.GroupStageMicroseconds;
				CUI_EXPECT_TRUE(measuredPreparationMicroseconds
					<= sample.FirstTiming.CompositionPreparationMicroseconds * 1.05);
				const double measuredSegmentMicroseconds =
					sample.FirstPreparation.RootStateMicroseconds
					+ sample.FirstPreparation.AncestorClipMicroseconds
					+ sample.FirstPreparation.BoundsMicroseconds
					+ sample.FirstPreparation.TransformClassificationMicroseconds
					+ sample.FirstPreparation.LayerAcquireStageMicroseconds;
				CUI_EXPECT_TRUE(measuredSegmentMicroseconds
					<= sample.FirstPreparation.SegmentMicroseconds * 1.05);
				CUI_EXPECT_TRUE(
					sample.First.EstimatedSceneSwapChainBytes
						+ sample.First.EstimatedSceneCompositionSurfaceBytes > 0u);
				CUI_EXPECT_TRUE(SceneRasterBytes(sample.First)
					<= 64ull * 1024ull * 1024ull);
				CUI_EXPECT_TRUE(firstFrame.SceneSurfacesOpened > 0u);
				CUI_EXPECT_TRUE(firstFrame.SceneSurfacesOpened
					<= targetCount + 1u);
				CUI_EXPECT_TRUE(firstFrame.PreSurfaceCulledNodes > 0u);
				CUI_EXPECT_EQ(targetCount + 1u,
					firstFrame.SceneSurfacesOpened
					+ firstFrame.PreSurfaceCulledNodes);
				CUI_EXPECT_EQ(sample.First.SceneLayerCount,
					firstFrame.SceneSurfacesOpened);
				CUI_EXPECT_TRUE(
					sample.FirstSceneSurfaceOpenMicroseconds > 0.0);
				CUI_EXPECT_TRUE(
					sample.FirstSceneSurfaceCloseMicroseconds > 0.0);
				if (sample.First.SceneLayerDirect2DSurfaceContextCount > 0u)
					CUI_EXPECT_EQ(0.0,
						sample.FirstSceneSurfaceEndDrawMicroseconds);
				else
					CUI_EXPECT_TRUE(
						sample.FirstSceneSurfaceEndDrawMicroseconds > 0.0);
				if (sample.First.SceneLayerCompositionSurfaceCount > 0u)
				{
					CUI_EXPECT_EQ(0.0,
						sample.FirstSceneSurfacePresentMicroseconds);
					CUI_EXPECT_TRUE(
						sample.FirstSceneSurfaceSubmitMicroseconds > 0.0);
				}
				else
				{
					CUI_EXPECT_TRUE(
						sample.FirstSceneSurfacePresentMicroseconds > 0.0);
					CUI_EXPECT_EQ(0.0,
						sample.FirstSceneSurfaceSubmitMicroseconds);
				}
				CUI_EXPECT_TRUE(
					sample.FirstSceneSurfaceEndDrawMicroseconds
					+ sample.FirstSceneSurfacePresentMicroseconds
					+ sample.FirstSceneSurfaceSubmitMicroseconds
					<= sample.FirstSceneSurfaceCloseMicroseconds * 1.05);
#if !defined(_DEBUG) && defined(_WIN64)
				CUI_EXPECT_TRUE(sample.FirstFrameMicroseconds <= 75'000.0);
				CUI_EXPECT_TRUE(
					sample.FirstTiming.CompositionPreparationMicroseconds
					<= 20'000.0);
				CUI_EXPECT_TRUE(
					sample.First.SceneLayerGraphicsCreateMicroseconds
					<= 15'000.0);
				CUI_EXPECT_TRUE(
					sample.First.SceneLayerCompositionSurfaceCreateMicroseconds
					<= 5'000.0);
				CUI_EXPECT_TRUE(
					sample.FirstTiming.SceneRenderMicroseconds <= 200'000.0);
				CUI_EXPECT_TRUE(
					sample.FirstSceneSurfacePresentMicroseconds
						+ sample.FirstSceneSurfaceSubmitMicroseconds <= 100'000.0);
#endif

				scene.TickRegisteredWindow(BenchmarkClockOrigin + 320u);
				sample.SteadyFrameMicroseconds = MeasureMicroseconds(
					frequency, [&] { scene.ForcePresentationUpdateForTesting(); });
				sample.Steady = scene.PresentationResourcesForTesting();
				const auto steadyFrame = scene.PresentationFrameForTesting();
				sample.SteadyTiming = steadyFrame.Timing;
				sample.SteadyPreparation = steadyFrame.Preparation;
				sample.SteadyCompositionOnlySegments =
					steadyFrame.CompositionOnlySegments;
				sample.SteadyCulledNodes = steadyFrame.CulledNodes;
				sample.SteadyPreSurfaceCulledNodes =
					steadyFrame.PreSurfaceCulledNodes;
				sample.SteadySceneSurfacesOpened =
					steadyFrame.SceneSurfacesOpened;
				CUI_EXPECT_EQ(targetCount,
					steadyFrame.CompositionOnlySegments
					+ steadyFrame.CulledNodes);
				CUI_EXPECT_TRUE(steadyFrame.CompositionOnlySegments > 0u);
				CUI_EXPECT_EQ(steadyFrame.CulledNodes,
					steadyFrame.PreSurfaceCulledNodes);
				CUI_EXPECT_TRUE(steadyFrame.SceneSurfacesOpened <= 1u);
				CUI_EXPECT_EQ(0ULL, steadyFrame.CommandRecordedNodes);
				CUI_EXPECT_TRUE(sample.SteadyTiming.TotalMicroseconds > 0.0);
				CUI_EXPECT_EQ(sample.First.SceneLayerCount,
					sample.Steady.SceneLayerCount);
				CUI_EXPECT_EQ(sample.First.SceneLayerSlotCount,
					sample.Steady.SceneLayerSlotCount);
				CUI_EXPECT_EQ(sample.First.EstimatedSceneSwapChainBytes,
					sample.Steady.EstimatedSceneSwapChainBytes);
				CUI_EXPECT_EQ(SceneRasterBytes(sample.First),
					SceneRasterBytes(sample.Steady));
				CUI_EXPECT_EQ(1ULL,
					sample.Steady.SceneCommandRecorderCount);
				CUI_EXPECT_EQ(sample.Steady.SceneLayerCount,
					sample.Steady.SceneCommandRecorderReferenceCount);
				CUI_EXPECT_EQ(0ULL,
					sample.Steady.SceneLayerSubmittedSnapshotTextureCount);
				CUI_EXPECT_EQ(0ULL,
					sample.Steady.SceneLayerSubmittedSnapshotUpdateCount);
				if (recover)
				{
					sample.RecoveryExercised = true;
					const auto recoveries =
						scene.PresentationDeviceRecoveryCountForTesting();
					scene.InjectPresentationDeviceLossForTesting();
					scene.ForcePresentationUpdateForTesting();
					sample.Recovered = scene.PresentationResourcesForTesting();
					CUI_EXPECT_EQ(recoveries + 1u,
						scene.PresentationDeviceRecoveryCountForTesting());
					CUI_EXPECT_EQ(sample.First.SceneLayerCount,
						sample.Recovered.SceneLayerCount);
					CUI_EXPECT_EQ(sample.First.SceneLayerSlotCount,
						sample.Recovered.SceneLayerSlotCount);
					CUI_EXPECT_EQ(sample.First.EstimatedSceneSwapChainBytes,
						sample.Recovered.EstimatedSceneSwapChainBytes);
					CUI_EXPECT_EQ(SceneRasterBytes(sample.First),
						SceneRasterBytes(sample.Recovered));
					CUI_EXPECT_EQ(0ULL,
						sample.Recovered.SceneLayerSubmittedSnapshotTextureCount);
					CUI_EXPECT_EQ(0ULL,
						sample.Recovered.SceneLayerSubmittedSnapshotUpdateCount);
					CUI_EXPECT_TRUE(sample.Recovered.SceneLayerReleaseCount
						>= sample.First.SceneLayerCount);
				}
#if !defined(_DEBUG) && defined(_WIN64)
				constexpr size_t CadenceWarmupFrames = 10u;
				constexpr size_t CadenceSampleFrames = 100u;
				constexpr size_t CadenceRequiredFramesWithinBudget = 99u;
#else
				// Non-authoritative configurations keep the lifecycle/culling
				// contract without turning Debug or 32-bit timing into a release gate.
				constexpr size_t CadenceWarmupFrames = 2u;
				constexpr size_t CadenceSampleFrames = 10u;
				constexpr size_t CadenceRequiredFramesWithinBudget = 0u;
#endif
				auto cadenceTick = [](size_t index)
				{
					return BenchmarkClockOrigin + 360u
						+ static_cast<unsigned long long>(index % 25u) * 16u;
				};
				for (size_t index = 0; index < CadenceWarmupFrames; ++index)
				{
					scene.TickRegisteredWindow(cadenceTick(index));
					scene.ForcePresentationUpdateForTesting();
				}
				std::vector<double> cadenceSamples;
				cadenceSamples.reserve(CadenceSampleFrames);
				for (size_t index = 0; index < CadenceSampleFrames; ++index)
				{
					scene.TickRegisteredWindow(cadenceTick(
						index + CadenceWarmupFrames));
					const double elapsed = MeasureMicroseconds(
						frequency,
						[&] { scene.ForcePresentationUpdateForTesting(); });
					cadenceSamples.push_back(elapsed);
					if (elapsed <= SixtyHertzFrameBudgetMicroseconds)
						++sample.CadenceFramesWithinBudget;
					const auto frame = scene.PresentationFrameForTesting();
					CUI_EXPECT_EQ(targetCount,
						frame.CompositionOnlySegments + frame.CulledNodes);
					CUI_EXPECT_EQ(frame.CulledNodes,
						frame.PreSurfaceCulledNodes);
					CUI_EXPECT_TRUE(frame.SceneSurfacesOpened <= 1u);
					CUI_EXPECT_EQ(0ULL, frame.CommandRecordedNodes);
				}
				sample.Cadence = Summarize(std::move(cadenceSamples));
#if !defined(_DEBUG) && defined(_WIN64)
				CUI_EXPECT_TRUE(sample.Cadence.P95Microseconds
					<= SixtyHertzFrameBudgetMicroseconds);
#endif
				CUI_EXPECT_TRUE(sample.CadenceFramesWithinBudget
					>= CadenceRequiredFramesWithinBudget);
				scene.Remove();
				scene.ForcePresentationUpdateForTesting();
				sample.Trimmed = scene.PresentationResourcesForTesting();
				CUI_EXPECT_EQ(1ULL, sample.Trimmed.SceneLayerCount);
				CUI_EXPECT_EQ(1ULL, sample.Trimmed.SceneLayerSlotCount);
				CUI_EXPECT_TRUE(
					sample.Trimmed.EstimatedSceneSwapChainBytes
						+ sample.Trimmed.EstimatedSceneCompositionSurfaceBytes
					< sample.First.EstimatedSceneSwapChainBytes
						+ sample.First.EstimatedSceneCompositionSurfaceBytes);
				CUI_EXPECT_TRUE(sample.Trimmed.SceneLayerReleaseCount
					>= sample.First.SceneLayerCount - 1u);
				CUI_EXPECT_EQ(aborts,
					scene.PresentationAbortedFrameCount());
				scene.HideOffscreenPresentationForTesting();
			}
			sample.MemoryAfter = QueryProcessMemory();
			return sample;
		};

		std::vector<ScaleSample> scaleSamples;
		if (!isolatedScale || *isolatedScale == 500u)
			scaleSamples.push_back(runScale(500u, true));
		if (!isolatedScale || *isolatedScale == 1000u)
			scaleSamples.push_back(runScale(1000u, false));
		CUI_EXPECT_TRUE(!scaleSamples.empty());
		if (!isolatedScale)
		{
			const auto& fiveHundred = scaleSamples[0];
			const auto& oneThousand = scaleSamples[1];
			CUI_EXPECT_EQ(fiveHundred.First.SceneLayerCount,
				oneThousand.First.SceneLayerCount);
			CUI_EXPECT_EQ(fiveHundred.First.EstimatedSceneSwapChainBytes,
				oneThousand.First.EstimatedSceneSwapChainBytes);
			CUI_EXPECT_EQ(
				fiveHundred.First.EstimatedSceneCompositionSurfaceBytes,
				oneThousand.First.EstimatedSceneCompositionSurfaceBytes);
			CUI_EXPECT_EQ(
				fiveHundred.First.EstimatedSceneSubmittedSnapshotBytes,
				oneThousand.First.EstimatedSceneSubmittedSnapshotBytes);
			CUI_EXPECT_TRUE(oneThousand.First.EstimatedSceneLayerSlotBytes
				> fiveHundred.First.EstimatedSceneLayerSlotBytes);
			CUI_EXPECT_TRUE(oneThousand.First.EstimatedSceneRetainedBytes
				> fiveHundred.First.EstimatedSceneRetainedBytes);
			CUI_EXPECT_EQ(fiveHundred.First.PeakSceneLayerCount,
				oneThousand.First.PeakSceneLayerCount);
		}

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_TARGET_RESOURCE_OUTPUT") != 0)
			throw std::runtime_error("Could not query target resource output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(), L"CUI_DCOMP_TARGET_RESOURCE_OUTPUT") != 0)
				throw std::runtime_error("Could not read target resource output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Target resource output must be under Workplans.");
			auto sampleJson = [](const ScaleSample& sample)
			{
				auto timingJson = [](const PresentationFrameTimingStatistics& value)
				{
					return std::string("{\"layoutMicroseconds\":")
						+ FormatDouble(value.LayoutMicroseconds)
						+ ",\"sceneSynchronizationMicroseconds\":"
						+ FormatDouble(value.SceneSynchronizationMicroseconds)
						+ ",\"compositionPreparationMicroseconds\":"
						+ FormatDouble(value.CompositionPreparationMicroseconds)
						+ ",\"transactionBeginMicroseconds\":"
						+ FormatDouble(value.TransactionBeginMicroseconds)
						+ ",\"primarySetupMicroseconds\":"
						+ FormatDouble(value.PrimarySetupMicroseconds)
						+ ",\"sceneRenderMicroseconds\":"
						+ FormatDouble(value.SceneRenderMicroseconds)
						+ ",\"surfaceFinalizeMicroseconds\":"
						+ FormatDouble(value.SurfaceFinalizeMicroseconds)
						+ ",\"compositionCommitMicroseconds\":"
						+ FormatDouble(value.CompositionCommitMicroseconds)
						+ ",\"totalMicroseconds\":"
						+ FormatDouble(value.TotalMicroseconds) + "}";
				};
				auto distributionJson = [](const Distribution& value)
				{
					return std::string("{\"count\":")
						+ std::to_string(value.Count)
						+ ",\"minimumMicroseconds\":"
						+ FormatDouble(value.MinimumMicroseconds)
						+ ",\"meanMicroseconds\":"
						+ FormatDouble(value.MeanMicroseconds)
						+ ",\"p50Microseconds\":"
						+ FormatDouble(value.P50Microseconds)
						+ ",\"p95Microseconds\":"
						+ FormatDouble(value.P95Microseconds)
						+ ",\"p99Microseconds\":"
						+ FormatDouble(value.P99Microseconds)
						+ ",\"maximumMicroseconds\":"
						+ FormatDouble(value.MaximumMicroseconds) + "}";
				};
				auto preparationJson = [](
					const PresentationPreparationStatistics& value)
				{
					return std::string("{\"scratchMicroseconds\":")
						+ FormatDouble(value.ScratchMicroseconds)
						+ ",\"nodePreparationMicroseconds\":"
						+ FormatDouble(value.NodePreparationMicroseconds)
						+ ",\"segmentMicroseconds\":"
						+ FormatDouble(value.SegmentMicroseconds)
						+ ",\"rootStateMicroseconds\":"
						+ FormatDouble(value.RootStateMicroseconds)
						+ ",\"ancestorClipMicroseconds\":"
						+ FormatDouble(value.AncestorClipMicroseconds)
						+ ",\"boundsMicroseconds\":"
						+ FormatDouble(value.BoundsMicroseconds)
						+ ",\"transformClassificationMicroseconds\":"
						+ FormatDouble(value.TransformClassificationMicroseconds)
						+ ",\"layerAcquireStageMicroseconds\":"
						+ FormatDouble(value.LayerAcquireStageMicroseconds)
						+ ",\"topologyCommitMicroseconds\":"
						+ FormatDouble(value.TopologyCommitMicroseconds)
						+ ",\"groupStageMicroseconds\":"
						+ FormatDouble(value.GroupStageMicroseconds)
						+ ",\"preparedNodeCount\":"
						+ std::to_string(value.PreparedNodeCount)
						+ ",\"segmentCount\":"
						+ std::to_string(value.SegmentCount)
						+ ",\"physicalLayerRequiredCount\":"
						+ std::to_string(value.PhysicalLayerRequiredCount)
						+ ",\"deferredUnmaterializedCount\":"
						+ std::to_string(value.DeferredUnmaterializedCount)
						+ ",\"earlyViewportDeferredCount\":"
						+ std::to_string(value.EarlyViewportDeferredCount)
						+ ",\"ancestorGeometryMaskMaterializationCount\":"
						+ std::to_string(
							value.AncestorGeometryMaskMaterializationCount)
						+ ",\"ancestorGeometryMaskReuseCount\":"
						+ std::to_string(
							value.AncestorGeometryMaskReuseCount)
						+ ",\"geometryRasterGroupCount\":"
						+ std::to_string(value.GeometryRasterGroupCount)
						+ ",\"geometryRasterMemberCount\":"
						+ std::to_string(value.GeometryRasterMemberCount) + "}";
				};
				return std::string("{\"targetCount\":")
					+ std::to_string(sample.TargetCount)
					+ ",\"firstFrameMicroseconds\":"
					+ FormatDouble(sample.FirstFrameMicroseconds)
					+ ",\"firstFrameBudgetMicroseconds\":"
					+ FormatDouble(SixtyHertzFrameBudgetMicroseconds)
					+ ",\"firstFrameWithinBudget\":"
					+ (sample.FirstFrameMicroseconds
						<= SixtyHertzFrameBudgetMicroseconds ? "true" : "false")
					+ ",\"steadyFrameMicroseconds\":"
					+ FormatDouble(sample.SteadyFrameMicroseconds)
					+ ",\"firstPipeline\":"
					+ timingJson(sample.FirstTiming)
					+ ",\"steadyPipeline\":"
					+ timingJson(sample.SteadyTiming)
					+ ",\"firstPreparation\":"
					+ preparationJson(sample.FirstPreparation)
					+ ",\"steadyPreparation\":"
					+ preparationJson(sample.SteadyPreparation)
					+ ",\"cadence\":"
					+ distributionJson(sample.Cadence)
					+ ",\"cadenceFrameBudgetMicroseconds\":"
					+ FormatDouble(SixtyHertzFrameBudgetMicroseconds)
					+ ",\"cadenceFramesWithinBudget\":"
					+ std::to_string(sample.CadenceFramesWithinBudget)
					+ ",\"steadyCompositionOnlySegments\":"
					+ std::to_string(sample.SteadyCompositionOnlySegments)
					+ ",\"steadyCulledNodes\":"
					+ std::to_string(sample.SteadyCulledNodes)
					+ ",\"steadyPreSurfaceCulledNodes\":"
					+ std::to_string(sample.SteadyPreSurfaceCulledNodes)
					+ ",\"steadySceneSurfacesOpened\":"
					+ std::to_string(sample.SteadySceneSurfacesOpened)
					+ ",\"firstPreSurfaceCulledNodes\":"
					+ std::to_string(sample.FirstPreSurfaceCulledNodes)
					+ ",\"firstSceneSurfacesOpened\":"
					+ std::to_string(sample.FirstSceneSurfacesOpened)
					+ ",\"firstSceneSurfaceOpenMicroseconds\":"
					+ FormatDouble(sample.FirstSceneSurfaceOpenMicroseconds)
					+ ",\"firstSceneSurfaceCloseMicroseconds\":"
					+ FormatDouble(sample.FirstSceneSurfaceCloseMicroseconds)
					+ ",\"firstSceneSurfaceEndDrawMicroseconds\":"
					+ FormatDouble(sample.FirstSceneSurfaceEndDrawMicroseconds)
					+ ",\"firstSceneSurfacePresentMicroseconds\":"
					+ FormatDouble(sample.FirstSceneSurfacePresentMicroseconds)
					+ ",\"firstSceneSurfaceSubmitMicroseconds\":"
					+ FormatDouble(sample.FirstSceneSurfaceSubmitMicroseconds)
					+ ",\"firstSceneCommandRecordMicroseconds\":"
					+ FormatDouble(sample.FirstSceneCommandRecordMicroseconds)
					+ ",\"firstSceneCommandReplayMicroseconds\":"
					+ FormatDouble(sample.FirstSceneCommandReplayMicroseconds)
					+ ",\"memoryBeforePrivateBytes\":"
					+ std::to_string(sample.MemoryBefore.PrivateUsageBytes)
					+ ",\"memoryAfterPrivateBytes\":"
					+ std::to_string(sample.MemoryAfter.PrivateUsageBytes)
					+ ",\"first\":" + ResourceSnapshotJson(sample.First)
					+ ",\"steady\":" + ResourceSnapshotJson(sample.Steady)
					+ ",\"recoveryExercised\":"
					+ (sample.RecoveryExercised ? "true" : "false")
					+ (sample.RecoveryExercised
						? ",\"recovered\":" + ResourceSnapshotJson(sample.Recovered)
						: std::string{})
					+ ",\"trimmed\":" + ResourceSnapshotJson(sample.Trimmed)
					+ "}";
			};
			std::string samplesJson;
			for (const auto& sample : scaleSamples)
			{
				if (!samplesJson.empty()) samplesJson += ",";
				samplesJson += sampleJson(sample);
			}
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\":2,\n  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"target-scene-resource-pressure\",\n"
				+ "  \"samplingMode\":\""
				+ (isolatedScale ? "single-scale-process" : "paired-process")
				+ "\",\n"
				+ (isolatedScale
					? "  \"isolatedScale\":"
						+ std::to_string(*isolatedScale) + ",\n"
					: std::string{})
				+ "  \"samples\":[" + samplesJson + "],\n"
				+ "  \"estimatedSceneBudgetBytes\":67108864,\n"
				+ "  \"result\":\"PASS\"\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error("Could not write target resource JSON: "
					+ Convert::UnicodeToUtf8(error));
		}
	});

	runner.Add("Animation production composition surfaces omit submitted snapshots", []
	{
		struct SurfaceBackendScope final
		{
			bool Previous = false;
			~SurfaceBackendScope()
			{
				PresentationRenderHost::
					ExchangeSceneCompositionSurfaceBackendForTesting(Previous);
			}
		} backendScope{
			PresentationRenderHost::
				ExchangeSceneCompositionSurfaceBackendForTesting(true) };

		BenchmarkScene scene(1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.ConfigureTargetRectangleForTesting(
			0u, 12.0f, 10.0f, 24.0f, 32.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 0.15f, 0.45f, 0.85f, 1.0f });
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 80u);
		scene.ForcePresentationUpdateForTesting();
		const auto resources = scene.PresentationResourcesForTesting();
		CUI_EXPECT_TRUE(resources.SceneLayerCompositionSurfaceCount > 0u);
		CUI_EXPECT_EQ(resources.SceneLayerCount,
			resources.SceneLayerCompositionSurfaceCount);
		CUI_EXPECT_EQ(resources.SceneLayerCompositionSurfaceCount,
			resources.SceneLayerDirect2DSurfaceContextCount);
		CUI_EXPECT_EQ(0ULL, resources.SceneLayerSwapChainCount);
		CUI_EXPECT_EQ(0ULL,
			resources.SceneLayerSubmittedSnapshotTextureCount);
		CUI_EXPECT_EQ(0ULL, resources.SceneLayerPixelReadbackLeaseCount);
		CUI_EXPECT_EQ(0ULL,
			resources.EstimatedSceneSubmittedSnapshotBytes);
		CUI_EXPECT_EQ(0ULL,
			resources.PeakEstimatedSceneSubmittedSnapshotBytes);
		CUI_EXPECT_EQ(0ULL,
			resources.SceneLayerSubmittedSnapshotCreateCount);
		CUI_EXPECT_EQ(0ULL,
			resources.SceneLayerSubmittedSnapshotUpdateCount);
		CUI_EXPECT_EQ(0ULL,
			resources.SceneLayerSubmittedSnapshotCopiedBytes);
		CUI_EXPECT_EQ(0.0,
			resources.SceneLayerSubmittedSnapshotCreateMicroseconds);
		CUI_EXPECT_EQ(0.0,
			resources.SceneLayerSubmittedSnapshotCopyMicroseconds);
		UINT width = 0;
		UINT height = 0;
		uint64_t digest = 0;
		size_t opaque = 0;
		CUI_EXPECT_FALSE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, digest, opaque));
		CUI_EXPECT_FALSE(
			scene.TryAcquireSceneLayerPixelReadbackLeaseForTesting());
		auto capture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!capture.Error.empty())
			throw std::runtime_error(
				"Snapshot-free composition capture failed: " + capture.Error);
		RECT bounds{};
		size_t pixels = 0;
		CUI_EXPECT_TRUE(capture.TryGetColorBounds(
			217u, 115u, 38u, bounds, pixels, 12u));
		CUI_EXPECT_TRUE(pixels > 0u);
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation composition surface resize preserves submitted pixels", []
	{
		struct SurfaceBackendScope final
		{
			bool Previous = false;
			~SurfaceBackendScope()
			{
				PresentationRenderHost::
					ExchangeSceneCompositionSurfaceBackendForTesting(Previous);
			}
		} backendScope{
			PresentationRenderHost::
				ExchangeSceneCompositionSurfaceBackendForTesting(true) };

		BenchmarkScene scene(1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 12.0f, 10.0f, 24.0f, 32.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 0.15f, 0.45f, 0.85f, 1.0f });
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 80u);
		scene.ForcePresentationUpdateForTesting();
		const auto initialResources = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(initialResources.SceneLayerCount,
			initialResources.SceneLayerCompositionSurfaceCount);
		CUI_EXPECT_EQ(0ULL, initialResources.SceneLayerSwapChainCount);
		CUI_EXPECT_EQ(1ULL,
			initialResources.SceneLayerPixelReadbackLeaseCount);
		CUI_EXPECT_EQ(initialResources.SceneLayerCompositionSurfaceCount,
			initialResources.SceneLayerSubmittedSnapshotTextureCount);
		CUI_EXPECT_EQ(initialResources.EstimatedSceneCompositionSurfaceBytes,
			initialResources.EstimatedSceneSubmittedSnapshotBytes);
		CUI_EXPECT_EQ(
			initialResources.SceneLayerSubmittedSnapshotTextureCount,
			initialResources.SceneLayerSubmittedSnapshotCreateCount);
		CUI_EXPECT_TRUE(
			initialResources.SceneLayerSubmittedSnapshotUpdateCount
				>= initialResources.SceneLayerSubmittedSnapshotTextureCount);
		CUI_EXPECT_TRUE(
			initialResources.SceneLayerSubmittedSnapshotCopiedBytes
				>= initialResources.EstimatedSceneSubmittedSnapshotBytes);
		CUI_EXPECT_TRUE(
			initialResources.SceneLayerSubmittedSnapshotCreateMicroseconds > 0.0);
		CUI_EXPECT_TRUE(
			initialResources.SceneLayerSubmittedSnapshotCopyMicroseconds > 0.0);
		UINT width = 0;
		UINT height = 0;
		uint64_t initialDigest = 0;
		size_t initialOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, initialDigest, initialOpaque));
		PresentationNodeSnapshot initialSnapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, initialSnapshot));
		CUI_EXPECT_EQ(initialSnapshot.CompositionSurfacePhysicalWidth, width);
		CUI_EXPECT_EQ(initialSnapshot.CompositionSurfacePhysicalHeight, height);
		CUI_EXPECT_TRUE(initialOpaque > 0u);
		CUI_EXPECT_TRUE(initialOpaque
			<= static_cast<size_t>(width) * height);
		const UINT initialWidth = width;
		const UINT initialHeight = height;

		scene.ConfigureTargetRectangleForTesting(
			0u, 20.0f, 8.0f, 24.0f, 32.0f);
		scene.ForcePresentationUpdateForTesting();
		const auto resizedResources = scene.PresentationResourcesForTesting();
		CUI_EXPECT_TRUE(resizedResources.SceneLayerResizeCount
			>= initialResources.SceneLayerResizeCount + 1u);
		CUI_EXPECT_TRUE(
			resizedResources.SceneLayerSubmittedSnapshotCreateCount
				> initialResources.SceneLayerSubmittedSnapshotCreateCount);
		CUI_EXPECT_TRUE(
			resizedResources.SceneLayerSubmittedSnapshotUpdateCount
				> initialResources.SceneLayerSubmittedSnapshotUpdateCount);
		CUI_EXPECT_TRUE(
			resizedResources.SceneLayerSubmittedSnapshotCopiedBytes
				> initialResources.SceneLayerSubmittedSnapshotCopiedBytes);
		uint64_t resizedDigest = 0;
		size_t resizedOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, resizedDigest, resizedOpaque));
		PresentationNodeSnapshot resizedSnapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, resizedSnapshot));
		CUI_EXPECT_EQ(resizedSnapshot.CompositionSurfacePhysicalWidth, width);
		CUI_EXPECT_EQ(resizedSnapshot.CompositionSurfacePhysicalHeight, height);
		CUI_EXPECT_TRUE(width != initialWidth || height != initialHeight);
		CUI_EXPECT_TRUE(resizedOpaque > 0u);
		CUI_EXPECT_TRUE(resizedOpaque
			<= static_cast<size_t>(width) * height);
		CUI_EXPECT_TRUE(initialOpaque != resizedOpaque);
		CUI_EXPECT_TRUE(initialDigest != resizedDigest);

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
		scene.ForcePresentationUpdateForTesting();
		const auto stableFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, stableFrame.CompositionOnlySegments);
		uint64_t stableDigest = 0;
		size_t stableOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, stableDigest, stableOpaque));
		CUI_EXPECT_EQ(resizedDigest, stableDigest);
		CUI_EXPECT_EQ(resizedOpaque, stableOpaque);

		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		uint64_t recoveredDigest = 0;
		size_t recoveredOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, recoveredDigest, recoveredOpaque));
		CUI_EXPECT_EQ(resizedDigest, recoveredDigest);
		CUI_EXPECT_EQ(resizedOpaque, recoveredOpaque);
		const uint64_t acceptedRecoveredDigest = recoveredDigest;
		const auto recoveredResources = scene.PresentationResourcesForTesting();
		CUI_EXPECT_TRUE(
			recoveredResources.SceneLayerSubmittedSnapshotCreateCount
				> resizedResources.SceneLayerSubmittedSnapshotCreateCount);
		CUI_EXPECT_TRUE(
			recoveredResources.SceneLayerSubmittedSnapshotUpdateCount
				> resizedResources.SceneLayerSubmittedSnapshotUpdateCount);
		scene.ReleaseSceneLayerPixelReadbackLeaseForTesting();
		const auto releasedResources = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(0ULL,
			releasedResources.SceneLayerPixelReadbackLeaseCount);
		CUI_EXPECT_EQ(0ULL,
			releasedResources.SceneLayerSubmittedSnapshotTextureCount);
		CUI_EXPECT_EQ(0ULL,
			releasedResources.EstimatedSceneSubmittedSnapshotBytes);
		CUI_EXPECT_TRUE(
			releasedResources.PeakEstimatedSceneSubmittedSnapshotBytes
				>= initialResources.EstimatedSceneSubmittedSnapshotBytes);
		CUI_EXPECT_FALSE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, recoveredDigest, recoveredOpaque));
		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_READBACK_LEASE_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query readback-lease output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(), L"CUI_DCOMP_READBACK_LEASE_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read readback-lease output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Readback-lease output must be under Workplans.");
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\":1,\n"
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"composition-surface-readback-lease\",\n"
				+ "  \"initial\":" + ResourceSnapshotJson(initialResources) + ",\n"
				+ "  \"resized\":" + ResourceSnapshotJson(resizedResources) + ",\n"
				+ "  \"recovered\":" + ResourceSnapshotJson(recoveredResources) + ",\n"
				+ "  \"released\":" + ResourceSnapshotJson(releasedResources) + ",\n"
				+ "  \"initialDigest\":\"" + std::to_string(initialDigest) + "\",\n"
				+ "  \"resizedDigest\":\"" + std::to_string(resizedDigest) + "\",\n"
				+ "  \"stableDigest\":\"" + std::to_string(stableDigest) + "\",\n"
				+ "  \"recoveredDigest\":\""
				+ std::to_string(acceptedRecoveredDigest) + "\",\n"
				+ "  \"result\":\"PASS\"\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error(
					"Could not write readback-lease JSON: "
					+ Convert::UnicodeToUtf8(error));
		}
		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation pre-surface culling draws isolated target on viewport reentry", []
	{
		BenchmarkScene scene(1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.ConfigureTargetRectangleForTesting(
			0u, 12.0f, 12.0f, 500.0f, 40.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 0.15f, 0.45f, 0.85f, 1.0f });
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting(0u, false);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
		scene.ForcePresentationUpdateForTesting();
		const auto outside = scene.PresentationFrameForTesting();
		PresentationNodeSnapshot snapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, snapshot));
		CUI_EXPECT_EQ(1ULL, outside.CulledNodes);
		CUI_EXPECT_EQ(1ULL, outside.PreSurfaceCulledNodes);
		CUI_EXPECT_TRUE(outside.SceneSurfacesOpened <= 1u);
		CUI_EXPECT_EQ(2ULL, outside.Preparation.SegmentCount);
		CUI_EXPECT_EQ(1ULL,
			outside.Preparation.PhysicalLayerRequiredCount);
		CUI_EXPECT_EQ(1ULL,
			outside.Preparation.DeferredUnmaterializedCount);
		CUI_EXPECT_EQ(1ULL,
			outside.Preparation.EarlyViewportDeferredCount);
		CUI_EXPECT_FALSE(snapshot.HasPresented);
		CUI_EXPECT_FALSE(snapshot.HasDrawingCommands);
		const auto outsideResources = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(1ULL, outsideResources.SceneLayerCount);
		CUI_EXPECT_EQ(2ULL, outsideResources.SceneLayerSlotCount);
		const size_t stableSegmentIndex = snapshot.SegmentIndex;

		scene.ConfigureTargetRectangleForTesting(
			0u, 12.0f, 12.0f, 20.0f, 40.0f);
		scene.ForcePresentationUpdateForTesting();
		const auto entered = scene.PresentationFrameForTesting();
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, snapshot));
		CUI_EXPECT_TRUE(entered.SceneSurfacesOpened >= 1u);
		CUI_EXPECT_TRUE(entered.CommandRecordedNodes >= 1u);
		CUI_EXPECT_EQ(2ULL,
			entered.Preparation.PhysicalLayerRequiredCount);
		CUI_EXPECT_EQ(0ULL,
			entered.Preparation.DeferredUnmaterializedCount);
		CUI_EXPECT_EQ(0ULL,
			entered.Preparation.EarlyViewportDeferredCount);
		CUI_EXPECT_TRUE(snapshot.HasPresented);
		CUI_EXPECT_TRUE(snapshot.HasDrawingCommands);
		CUI_EXPECT_EQ(stableSegmentIndex, snapshot.SegmentIndex);
		const auto enteredResources = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(2ULL, enteredResources.SceneLayerCount);
		CUI_EXPECT_EQ(2ULL, enteredResources.SceneLayerSlotCount);
		CUI_EXPECT_EQ(outsideResources.SceneLayerCreateCount + 1u,
			enteredResources.SceneLayerCreateCount);
		CUI_EXPECT_EQ(outsideResources.SceneLayerReleaseCount,
			enteredResources.SceneLayerReleaseCount);
		CUI_EXPECT_TRUE(
			enteredResources.CompositionVisualStackRebuildCount
			<= outsideResources.CompositionVisualStackRebuildCount + 1u);

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 320u);
		scene.ForcePresentationUpdateForTesting();
		const auto retained = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, retained.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, retained.CommandRecordedNodes);
		CUI_EXPECT_EQ(0ULL, retained.PreSurfaceCulledNodes);
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation sparse scene-layer gap materializes without reordering siblings", []
	{
		BenchmarkScene scene(3u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		for (const size_t index : std::array<size_t, 2>{ 0u, 2u })
			scene.ConfigureTargetRectangleForTesting(
				index, 12.0f, 12.0f, 20.0f, 40.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 12.0f, 12.0f, 500.0f, 40.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 0.15f, 0.45f, 0.85f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			1u, D2D1_COLOR_F{ 0.1f, 0.8f, 0.2f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			2u, D2D1_COLOR_F{ 0.8f, 0.2f, 0.1f, 1.0f });
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
		scene.ForcePresentationUpdateForTesting();

		std::array<PresentationNodeSnapshot, 3> initial;
		for (size_t index = 0; index < initial.size(); ++index)
			CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
				index, initial[index]));
		CUI_EXPECT_TRUE(initial[0].HasPresented);
		CUI_EXPECT_FALSE(initial[1].HasPresented);
		CUI_EXPECT_TRUE(initial[2].HasPresented);
		CUI_EXPECT_TRUE(initial[0].SegmentIndex < initial[1].SegmentIndex
			&& initial[1].SegmentIndex < initial[2].SegmentIndex);
		const auto sparse = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(3ULL, sparse.SceneLayerCount);
		CUI_EXPECT_EQ(4ULL, sparse.SceneLayerSlotCount);
		auto assertTrailingRed = [&]
		{
			auto capture = CaptureWindowComposition(
				scene.NativeWindowHandleForTesting());
			if (!capture.Error.empty())
				throw std::runtime_error("Sparse z-order capture failed: "
					+ capture.Error);
			RECT bounds{};
			size_t pixels = 0;
			CUI_EXPECT_TRUE(capture.TryGetColorBounds(
				26u, 51u, 204u, bounds, pixels));
			CUI_EXPECT_EQ(144ULL, pixels);
		};
		assertTrailingRed();

		scene.ConfigureTargetRectangleForTesting(
			1u, 12.0f, 12.0f, 20.0f, 40.0f);
		scene.ForcePresentationUpdateForTesting();
		std::array<PresentationNodeSnapshot, 3> entered;
		for (size_t index = 0; index < entered.size(); ++index)
		{
			CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
				index, entered[index]));
			CUI_EXPECT_EQ(initial[index].SegmentIndex,
				entered[index].SegmentIndex);
			CUI_EXPECT_TRUE(entered[index].HasPresented);
		}
		const auto materialized = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(4ULL, materialized.SceneLayerCount);
		CUI_EXPECT_EQ(4ULL, materialized.SceneLayerSlotCount);
		CUI_EXPECT_EQ(sparse.SceneLayerCreateCount + 1u,
			materialized.SceneLayerCreateCount);
		CUI_EXPECT_EQ(sparse.SceneLayerReleaseCount,
			materialized.SceneLayerReleaseCount);
		CUI_EXPECT_TRUE(materialized.CompositionVisualStackRebuildCount
			<= sparse.CompositionVisualStackRebuildCount + 1u);
		assertTrailingRed();

		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		const auto recovered = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(4ULL, recovered.SceneLayerCount);
		CUI_EXPECT_EQ(4ULL, recovered.SceneLayerSlotCount);
		assertTrailingRed();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation 500 transient roots release and rebuild surface budget", []
	{
		BenchmarkScene scene(1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.SetRootBackgroundForTesting(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		scene.ConfigureTargetRectangleForTesting(
			0u, 1.0f, 1.0f, 350.0f, 220.0f);
		auto roots = scene.AddTransientRootsForTesting(500u);
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ForcePresentationUpdateForTesting();
		auto& window = scene.WindowForTesting();
		auto rootCount = [&]
		{
			return cui::framework::WindowAccess::
				GetTransientPresentationCount(window);
		};
		const auto full = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(500ULL, rootCount());
		CUI_EXPECT_EQ(501ULL, full.SceneLayerCount);
		CUI_EXPECT_EQ(1ULL, full.FullWindowSceneLayerCount);
		CUI_EXPECT_TRUE(SceneRasterBytes(full)
			<= 64ull * 1024ull * 1024ull);
		for (const size_t index : std::array<size_t, 2>{ 0u, 499u })
		{
			PresentationNodeSnapshot snapshot;
			CUI_EXPECT_TRUE(scene.TryGetPresentationSnapshotForTesting(
				roots[index], snapshot));
			CUI_EXPECT_TRUE(snapshot.Overlay);
			CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
			CUI_EXPECT_EQ(1ULL, snapshot.CompositionIsolationDepth);
		}

		for (size_t index = 0; index < 250u; ++index)
			scene.CloseTransientRootForTesting(roots[index]);
		scene.ForcePresentationUpdateForTesting();
		const auto half = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(250ULL, rootCount());
		CUI_EXPECT_EQ(251ULL, half.SceneLayerCount);
		CUI_EXPECT_TRUE(SceneRasterBytes(half) < SceneRasterBytes(full));
		for (size_t index = 250u; index > 0u; --index)
			scene.OpenTransientRootForTesting(roots[index - 1u]);
		scene.ForcePresentationUpdateForTesting();
		const auto reopened = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(500ULL, rootCount());
		CUI_EXPECT_EQ(501ULL, reopened.SceneLayerCount);
		CUI_EXPECT_EQ(SceneRasterBytes(full), SceneRasterBytes(reopened));
		CUI_EXPECT_TRUE(reopened.SceneLayerReleaseCount >= 250u);
		CUI_EXPECT_TRUE(reopened.SceneLayerCreateCount
			>= full.SceneLayerCreateCount + 250u);

		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		const auto recovered = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		CUI_EXPECT_EQ(500ULL, rootCount());
		CUI_EXPECT_EQ(501ULL, recovered.SceneLayerCount);
		CUI_EXPECT_EQ(SceneRasterBytes(full), SceneRasterBytes(recovered));
		for (auto* root : roots) scene.CloseTransientRootForTesting(root);
		scene.ForcePresentationUpdateForTesting();
		const auto closed = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(0ULL, rootCount());
		CUI_EXPECT_EQ(1ULL, closed.SceneLayerCount);
		CUI_EXPECT_TRUE(closed.SceneLayerReleaseCount >= 1000u);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_TRANSIENT_RESOURCE_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query transient resource output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(), L"CUI_DCOMP_TRANSIENT_RESOURCE_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read transient resource output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Transient resource output must be under Workplans.");
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\":1,\n  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"transient-root-resource-pressure\",\n"
				+ "  \"full\":" + ResourceSnapshotJson(full) + ",\n"
				+ "  \"half\":" + ResourceSnapshotJson(half) + ",\n"
				+ "  \"reopened\":" + ResourceSnapshotJson(reopened) + ",\n"
				+ "  \"recovered\":" + ResourceSnapshotJson(recovered) + ",\n"
				+ "  \"closed\":" + ResourceSnapshotJson(closed) + ",\n"
				+ "  \"result\":\"PASS\"\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error(
					"Could not write transient resource JSON: "
					+ Convert::UnicodeToUtf8(error));
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation scene allocation failure and WARP adapter stay recoverable", []
	{
		struct AllocationFailureScope final
		{
			~AllocationFailureScope()
			{
				cui::framework::WindowAccess::
					ClearSceneLayerAllocationFailureForTesting();
			}
		} allocationScope;
		PresentationRenderHost::ResourceSnapshot failed;
		PresentationRenderHost::ResourceSnapshot recoveredAllocation;
		{
			BenchmarkScene scene(20u, 0u, BenchmarkPropertyKind::TransformX);
			scene.AddNativeCompositionBoundaryForTesting();
			scene.ShowOffscreenWithoutActivationForTesting();
			scene.Begin();
			scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
			const auto committed = scene.PresentationCommittedFrameCount();
			const auto aborted = scene.PresentationAbortedFrameCount();
			cui::framework::WindowAccess::
				FailSceneLayerAllocationAfterForTesting(5u);
			scene.ForcePresentationUpdateForTesting();
			failed = scene.PresentationResourcesForTesting();
			CUI_EXPECT_EQ(1ULL, failed.SceneLayerCount);
			CUI_EXPECT_EQ(21ULL, failed.SceneLayerSlotCount);
			CUI_EXPECT_EQ(1ULL,
				failed.SceneLayerAllocationFailureCount);
			CUI_EXPECT_EQ(6ULL, failed.SceneLayerCreateCount);
			CUI_EXPECT_TRUE(failed.SceneLayerReleaseCount >= 5u);
			CUI_EXPECT_TRUE(
				failed.CompositionVisualBatchRollbackCount >= 1u);
			CUI_EXPECT_EQ(0ULL,
				failed.CompositionVisualBatchRollbackFailureCount);
			CUI_EXPECT_EQ(committed,
				scene.PresentationCommittedFrameCount());
			CUI_EXPECT_EQ(aborted,
				scene.PresentationAbortedFrameCount());
			cui::framework::WindowAccess::
				ClearSceneLayerAllocationFailureForTesting();
			scene.ForcePresentationUpdateForTesting();
			recoveredAllocation = scene.PresentationResourcesForTesting();
			CUI_EXPECT_EQ(21ULL, recoveredAllocation.SceneLayerCount);
			CUI_EXPECT_EQ(21ULL, recoveredAllocation.SceneLayerSlotCount);
			CUI_EXPECT_EQ(1ULL,
				recoveredAllocation.SceneLayerAllocationFailureCount);
			CUI_EXPECT_TRUE(recoveredAllocation.SceneLayerCreateCount >= 26u);
			CUI_EXPECT_TRUE(
				recoveredAllocation.CompositionVisualBatchCommitCount >= 2u);
			CUI_EXPECT_TRUE(scene.PresentationCommittedFrameCount() > committed);
			scene.Remove();
			scene.ForcePresentationUpdateForTesting();
			CUI_EXPECT_EQ(1ULL,
				scene.PresentationResourcesForTesting().SceneLayerCount);
			scene.HideOffscreenPresentationForTesting();
		}

		GraphicsSharedD3DDeviceInfo initialAdapter{};
		CUI_EXPECT_TRUE(SUCCEEDED(Graphics_AcquireSharedD3DDevice(
			nullptr, nullptr, nullptr, nullptr, &initialAdapter)));
		struct WarpRestoreScope final
		{
			bool Active = true;
			~WarpRestoreScope()
			{
				if (!Active) return;
				Graphics_SetForceWarpSharedD3DDeviceForTesting(false);
				(void)Graphics_RotateSharedD3DDeviceForTesting(nullptr);
			}
		} warpRestore;
		Graphics_SetForceWarpSharedD3DDeviceForTesting(true);
		GraphicsSharedD3DDeviceInfo warpAdapter{};
		CUI_EXPECT_TRUE(SUCCEEDED(
			Graphics_RotateSharedD3DDeviceForTesting(&warpAdapter)));
		CUI_EXPECT_FALSE(warpAdapter.IsHardware);
		CUI_EXPECT_TRUE(warpAdapter.IsSoftwareAdapter);
		CUI_EXPECT_TRUE(warpAdapter.AdapterLuid != 0u);
		CUI_EXPECT_TRUE(warpAdapter.AdapterDescription[0] != L'\0');
		PresentationRenderHost::ResourceSnapshot warpResources;
		{
			BenchmarkScene scene(32u, 0u, BenchmarkPropertyKind::TransformX);
			scene.AddNativeCompositionBoundaryForTesting();
			scene.ShowOffscreenWithoutActivationForTesting();
			scene.Begin();
			scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
			scene.ForcePresentationUpdateForTesting();
			warpResources = scene.PresentationResourcesForTesting();
			CUI_EXPECT_EQ(warpAdapter.AdapterLuid,
				warpResources.AdapterLuid);
			CUI_EXPECT_FALSE(warpResources.IsHardwareAdapter);
			CUI_EXPECT_TRUE(warpResources.IsSoftwareAdapter);
			CUI_EXPECT_EQ(33ULL, warpResources.SceneLayerCount);
			CUI_EXPECT_EQ(33ULL, warpResources.SceneLayerSlotCount);
			scene.TickRegisteredWindow(BenchmarkClockOrigin + 320u);
			scene.ForcePresentationUpdateForTesting();
			CUI_EXPECT_EQ(32ULL,
				scene.PresentationFrameForTesting().CompositionOnlySegments);
			scene.HideOffscreenPresentationForTesting();
		}
		Graphics_SetForceWarpSharedD3DDeviceForTesting(false);
		GraphicsSharedD3DDeviceInfo restoredAdapter{};
		CUI_EXPECT_TRUE(SUCCEEDED(
			Graphics_RotateSharedD3DDeviceForTesting(&restoredAdapter)));
		warpRestore.Active = false;
		if (initialAdapter.IsHardware)
		{
			CUI_EXPECT_TRUE(restoredAdapter.IsHardware);
			CUI_EXPECT_EQ(initialAdapter.AdapterLuid,
				restoredAdapter.AdapterLuid);
			CUI_EXPECT_TRUE(initialAdapter.AdapterLuid
				!= warpAdapter.AdapterLuid);
		}

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_ADAPTER_RESOURCE_OUTPUT") != 0)
			throw std::runtime_error("Could not query adapter resource output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(), L"CUI_DCOMP_ADAPTER_RESOURCE_OUTPUT") != 0)
				throw std::runtime_error("Could not read adapter resource output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Adapter resource output must be under Workplans.");
			auto adapterJson = [](const GraphicsSharedD3DDeviceInfo& value)
			{
				return std::string("{\"generation\":")
					+ std::to_string(value.Generation)
					+ ",\"isHardware\":"
					+ (value.IsHardware ? "true" : "false")
					+ ",\"isSoftwareAdapter\":"
					+ (value.IsSoftwareAdapter ? "true" : "false")
					+ ",\"featureLevel\":"
					+ std::to_string(value.FeatureLevel)
					+ ",\"adapterLuid\":\""
					+ std::to_string(value.AdapterLuid)
					+ "\",\"description\":"
					+ JsonEscape(Convert::UnicodeToUtf8(
						std::wstring(value.AdapterDescription))) + "}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\":1,\n  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"adapter-and-allocation-recovery\",\n"
				+ "  \"initialAdapter\":" + adapterJson(initialAdapter) + ",\n"
				+ "  \"warpAdapter\":" + adapterJson(warpAdapter) + ",\n"
				+ "  \"restoredAdapter\":" + adapterJson(restoredAdapter) + ",\n"
				+ "  \"warpResources\":" + ResourceSnapshotJson(warpResources) + ",\n"
				+ "  \"failedAllocation\":" + ResourceSnapshotJson(failed) + ",\n"
				+ "  \"recoveredAllocation\":"
				+ ResourceSnapshotJson(recoveredAllocation) + ",\n"
				+ "  \"result\":\"PASS\"\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error("Could not write adapter resource JSON: "
					+ Convert::UnicodeToUtf8(error));
		}
	});

	runner.Add("Animation scene topology batch commit rolls back atomically", []
	{
		struct BatchFailureScope final
		{
			~BatchFailureScope()
			{
				cui::framework::WindowAccess::
					ClearSceneLayerTopologyBatchCommitFailureForTesting();
			}
		} failureScope;
		BenchmarkScene scene(20u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		const auto initial = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(1ULL, initial.SceneLayerCount);
		CUI_EXPECT_EQ(1ULL, initial.SceneLayerSlotCount);
		const auto committed = scene.PresentationCommittedFrameCount();
		const auto aborted = scene.PresentationAbortedFrameCount();
		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
		cui::framework::WindowAccess::
			FailNextSceneLayerTopologyBatchCommitForTesting();
		scene.ForcePresentationUpdateForTesting();
		const auto rolledBack = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(initial.SceneLayerCount, rolledBack.SceneLayerCount);
		CUI_EXPECT_EQ(21ULL, rolledBack.SceneLayerSlotCount);
		CUI_EXPECT_EQ(initial.SceneLayerCreateCount + 20u,
			rolledBack.SceneLayerCreateCount);
		CUI_EXPECT_TRUE(rolledBack.SceneLayerReleaseCount
			>= initial.SceneLayerReleaseCount + 20u);
		CUI_EXPECT_EQ(initial.CompositionVisualBatchRollbackCount + 1u,
			rolledBack.CompositionVisualBatchRollbackCount);
		CUI_EXPECT_EQ(0ULL,
			rolledBack.CompositionVisualBatchRollbackFailureCount);
		CUI_EXPECT_EQ(committed,
			scene.PresentationCommittedFrameCount());
		CUI_EXPECT_EQ(aborted,
			scene.PresentationAbortedFrameCount());
		CUI_EXPECT_EQ(recoveries,
			scene.PresentationDeviceRecoveryCountForTesting());

		scene.ForcePresentationUpdateForTesting();
		const auto recovered = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(21ULL, recovered.SceneLayerCount);
		CUI_EXPECT_EQ(21ULL, recovered.SceneLayerSlotCount);
		CUI_EXPECT_EQ(rolledBack.CompositionVisualBatchRollbackCount,
			recovered.CompositionVisualBatchRollbackCount);
		CUI_EXPECT_EQ(0ULL,
			recovered.CompositionVisualBatchRollbackFailureCount);
		CUI_EXPECT_EQ(initial.CompositionVisualBatchCommitCount + 1u,
			recovered.CompositionVisualBatchCommitCount);
		CUI_EXPECT_TRUE(recovered.CompositionVisualDeferredMutationCount
			>= initial.CompositionVisualDeferredMutationCount + 40u);
		CUI_EXPECT_TRUE(recovered.CompositionVisualStackRebuildCount
			<= initial.CompositionVisualStackRebuildCount + 1u);
		CUI_EXPECT_TRUE(scene.PresentationCommittedFrameCount() > committed);
		CUI_EXPECT_EQ(recoveries,
			scene.PresentationDeviceRecoveryCountForTesting());
		PresentationNodeSnapshot first;
		PresentationNodeSnapshot last;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, first));
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			19u, last));
		CUI_EXPECT_TRUE(first.HasPresented && last.HasPresented);
		CUI_EXPECT_TRUE(first.SegmentIndex < last.SegmentIndex);

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 320u);
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(20ULL,
			scene.PresentationFrameForTesting().CompositionOnlySegments);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_TOPOLOGY_BATCH_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query topology batch output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(), L"CUI_DCOMP_TOPOLOGY_BATCH_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read topology batch output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Topology batch output must be under Workplans.");
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\":1,\n  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"scene-topology-batch-rollback\",\n"
				+ "  \"initial\":" + ResourceSnapshotJson(initial) + ",\n"
				+ "  \"rolledBack\":"
				+ ResourceSnapshotJson(rolledBack) + ",\n"
				+ "  \"recovered\":"
				+ ResourceSnapshotJson(recovered) + ",\n"
				+ "  \"deviceRecoveryDelta\":0,\n"
				+ "  \"result\":\"PASS\"\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error(
					"Could not write topology batch JSON: "
					+ Convert::UnicodeToUtf8(error));
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation complex sparse-parent graph failures stay unpublished", []
	{
		struct FailureScope final
		{
			~FailureScope()
			{
				cui::framework::WindowAccess::
					ClearSceneLayerAllocationFailureForTesting();
				cui::framework::WindowAccess::
					ClearSceneLayerTopologyBatchCommitFailureForTesting();
				cui::framework::WindowAccess::
					ClearSceneLayerGroupTopologyStageFailureForTesting();
			}
		} failureScope;
		enum class FailureKind
		{
			LayerAllocation,
			TopologyCommit,
			GroupTopologyStage
		};
		struct CaseResult final
		{
			const char* Name = nullptr;
			PresentationRenderHost::ResourceSnapshot Failed;
			PresentationRenderHost::ResourceSnapshot Recovered;
			uint64_t DeviceRecoveryDelta = 0;
		};
		std::array<CaseResult, 3> results{};
		const std::array failureKinds{
			FailureKind::LayerAllocation,
			FailureKind::TopologyCommit,
			FailureKind::GroupTopologyStage };
		const std::array propertyKinds{
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::Opacity,
			BenchmarkPropertyKind::TransformX };
		for (size_t caseIndex = 0; caseIndex < failureKinds.size(); ++caseIndex)
		{
			const auto failureKind = failureKinds[caseIndex];
			BenchmarkScene scene(
				3u, 0u, BenchmarkPropertyKind::Opacity,
				1u, false, false, false, propertyKinds);
			scene.NestTargetForTesting(1u, 0u);
			scene.NestTargetForTesting(2u, 1u);
			auto* native = scene.AddTargetPixelNativeCompositionChildForTesting(
				0u, 24.0f, 20.0f, 60.0f, 0.0f,
				D2D1_COLOR_F{ 0.0f, 0.0f, 1.0f, 1.0f });
			scene.ShowOffscreenWithoutActivationForTesting();
			scene.SetRootBackgroundForTesting(
				D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
			scene.ConfigureTargetRectangleForTesting(
				0u, 160.0f, 24.0f, 20.0f, 30.0f);
			scene.ConfigureTargetRectangleForTesting(
				1u, 100.0f, 20.0f, 20.0f, 2.0f);
			// The deepest member is intentionally outside the viewport. Group
			// ownership makes it eager even though simple layers may stay sparse.
			scene.ConfigureTargetRectangleForTesting(
				2u, 24.0f, 20.0f, 500.0f, 0.0f);
			CUI_EXPECT_TRUE(scene.AddHostChildRectangleForTesting(
				8.0f, 16.0f, 200.0f, 34.0f,
				D2D1_COLOR_F{ 1.0f, 1.0f, 0.0f, 1.0f }) != nullptr);
			const auto initial = scene.PresentationResourcesForTesting();
			CUI_EXPECT_EQ(1ULL, initial.SceneLayerCount);
			const auto initialGroupCount =
				scene.PresentationOpacityGroupCountForTesting();
			const auto initialNativeCount =
				scene.PresentationGroupedNativeVisualCountForTesting();
			CUI_EXPECT_EQ(0ULL, initialGroupCount);
			CUI_EXPECT_EQ(0ULL, initialNativeCount);
			scene.Begin();
			if (failureKind == FailureKind::GroupTopologyStage)
			{
				// Establish a published graph. The injected same-topology rebuild
				// detaches these exact nested parents and their native visual lease.
				scene.TickRegisteredWindow(BenchmarkClockOrigin + 500u);
				scene.ForcePresentationUpdateForTesting();
				CUI_EXPECT_EQ(2ULL,
					scene.PresentationOpacityGroupCountForTesting());
				CUI_EXPECT_EQ(1ULL,
					scene.PresentationGroupedNativeVisualCountForTesting());
				bool stableLease = false;
				for (size_t attempt = 0; attempt < 6u && !stableLease; ++attempt)
				{
					const bool before =
						scene.PresentationGroupedNativeVisualCountForTesting() == 1u;
					scene.ForcePresentationUpdateForTesting();
					stableLease = before
						&& scene.PresentationGroupedNativeVisualCountForTesting() == 1u;
				}
				CUI_EXPECT_TRUE(stableLease);
				CUI_EXPECT_EQ(2ULL,
					scene.PresentationOpacityGroupCountForTesting());
			}
			auto committed = scene.PresentationCommittedFrameCount();
			auto aborted = scene.PresentationAbortedFrameCount();
			const auto recoveries =
				scene.PresentationDeviceRecoveryCountForTesting();
			switch (failureKind)
			{
			case FailureKind::LayerAllocation:
				results[caseIndex].Name = "layer-allocation";
				cui::framework::WindowAccess::
					FailSceneLayerAllocationAfterForTesting(1u);
				break;
			case FailureKind::TopologyCommit:
				results[caseIndex].Name = "topology-commit";
				cui::framework::WindowAccess::
					FailNextSceneLayerTopologyBatchCommitForTesting();
				break;
			case FailureKind::GroupTopologyStage:
				results[caseIndex].Name = "group-topology-stage";
				cui::framework::WindowAccess::
					FailNextSceneLayerGroupTopologyStageForTesting();
				break;
			}
			if (failureKind == FailureKind::GroupTopologyStage)
			{
				bool injectedFailureRecovered = false;
				for (size_t attempt = 0;
					attempt < 6u && !injectedFailureRecovered; ++attempt)
				{
					committed = scene.PresentationCommittedFrameCount();
					aborted = scene.PresentationAbortedFrameCount();
					scene.ForcePresentationUpdateForTesting();
					injectedFailureRecovered =
						scene.PresentationDeviceRecoveryCountForTesting()
						== recoveries + 1u;
				}
				CUI_EXPECT_TRUE(injectedFailureRecovered);
			}
			else
				scene.ForcePresentationUpdateForTesting();
			results[caseIndex].Failed =
				scene.PresentationResourcesForTesting();
			if (failureKind == FailureKind::GroupTopologyStage)
				CUI_EXPECT_TRUE(scene.PresentationCommittedFrameCount()
					> committed);
			else
				CUI_EXPECT_EQ(committed,
					scene.PresentationCommittedFrameCount());
			CUI_EXPECT_EQ(aborted,
				scene.PresentationAbortedFrameCount());
			const auto failedGroupCount =
				scene.PresentationOpacityGroupCountForTesting();
			const auto failedNativeCount =
				scene.PresentationGroupedNativeVisualCountForTesting();
			const size_t expectedFailedGroupCount = 2u;
			const size_t expectedFailedNativeCount =
				failureKind == FailureKind::GroupTopologyStage ? 1u : 0u;
			if (failedGroupCount != expectedFailedGroupCount
				|| failedNativeCount != expectedFailedNativeCount)
				throw std::runtime_error(std::string(
					"Complex failure retained-group state mismatched for ")
					+ results[caseIndex].Name + ": groups="
					+ std::to_string(failedGroupCount) + ", native="
					+ std::to_string(failedNativeCount) + ".");
			if (results[caseIndex].Failed.SceneLayerSlotCount != 5u)
				throw std::runtime_error(std::string(
					"Complex failure slot count mismatched for ")
					+ results[caseIndex].Name + ": slots="
					+ std::to_string(results[caseIndex].Failed.
						SceneLayerSlotCount) + ".");
			if (failureKind == FailureKind::GroupTopologyStage)
			{
				// The dangerous midpoint forces recovery before ForcePresentation
				// returns. Only the completely rebuilt graph, including its native
				// lease, is eligible for the retry commit.
				CUI_EXPECT_EQ(recoveries + 1u,
					scene.PresentationDeviceRecoveryCountForTesting());
				CUI_EXPECT_EQ(5ULL,
					results[caseIndex].Failed.SceneLayerCount);
			}
			else
			{
				CUI_EXPECT_EQ(1ULL,
					results[caseIndex].Failed.SceneLayerCount);
				CUI_EXPECT_TRUE(results[caseIndex].Failed.
					CompositionVisualBatchRollbackCount
					> initial.CompositionVisualBatchRollbackCount);
			}

			cui::framework::WindowAccess::
				ClearSceneLayerAllocationFailureForTesting();
			scene.TickRegisteredWindow(BenchmarkClockOrigin + 500u);
			scene.ForcePresentationUpdateForTesting();
			results[caseIndex].Recovered =
				scene.PresentationResourcesForTesting();
			results[caseIndex].DeviceRecoveryDelta =
				scene.PresentationDeviceRecoveryCountForTesting() - recoveries;
			CUI_EXPECT_EQ(
				failureKind == FailureKind::GroupTopologyStage ? 1ULL : 0ULL,
				results[caseIndex].DeviceRecoveryDelta);
			CUI_EXPECT_EQ(5ULL,
				results[caseIndex].Recovered.SceneLayerCount);
			CUI_EXPECT_EQ(5ULL,
				results[caseIndex].Recovered.SceneLayerSlotCount);
			CUI_EXPECT_EQ(2ULL,
				scene.PresentationOpacityGroupCountForTesting());
			CUI_EXPECT_EQ(1ULL,
				scene.PresentationGroupedNativeVisualCountForTesting());
			CUI_EXPECT_TRUE(scene.PresentationCommittedFrameCount() > committed);
			CUI_EXPECT_TRUE(native->VisualGenerationForTesting() >= 1u);
			for (size_t index = 0; index < 3u; ++index)
			{
				PresentationNodeSnapshot snapshot;
				CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
					index, snapshot));
				CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
				CUI_EXPECT_EQ(index < 2u, snapshot.HasPresented);
				CUI_EXPECT_EQ(index + 1u,
					snapshot.CompositionIsolationDepth);
			}
			auto capture = CaptureWindowComposition(
				scene.NativeWindowHandleForTesting());
			if (!capture.Error.empty())
				throw std::runtime_error(
					"Complex graph recovery capture failed: " + capture.Error);
			CapturedWindowFrame::Pixel trailing;
			CUI_EXPECT_TRUE(capture.TryGetPixel(204u, 62u, trailing));
			CUI_EXPECT_TRUE(trailing.Red >= 253u
				&& trailing.Green >= 253u && trailing.Blue <= 2u
				&& trailing.Alpha == 255u);
			scene.TickRegisteredWindow(BenchmarkClockOrigin + 700u);
			scene.ForcePresentationUpdateForTesting();
			const auto steady = scene.PresentationFrameForTesting();
			CUI_EXPECT_EQ(2ULL, steady.CompositionOnlySegments);
			CUI_EXPECT_EQ(1ULL, steady.PreSurfaceCulledNodes);
			scene.HideOffscreenPresentationForTesting();
		}

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_COMPLEX_GRAPH_FAILURE_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query complex graph failure output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_COMPLEX_GRAPH_FAILURE_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read complex graph failure output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Complex graph failure output must be under Workplans.");
			std::string cases;
			for (size_t index = 0; index < results.size(); ++index)
			{
				if (index != 0u) cases += ",";
				cases += std::string("{\"failure\":\"")
					+ results[index].Name + "\",\"failed\":"
					+ ResourceSnapshotJson(results[index].Failed)
					+ ",\"recovered\":"
					+ ResourceSnapshotJson(results[index].Recovered)
					+ ",\"deviceRecoveryDelta\":"
					+ std::to_string(results[index].DeviceRecoveryDelta) + "}";
			}
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\":1,\n  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"complex-sparse-parent-failure-matrix\",\n"
				+ "  \"cases\":[" + cases + "],\n"
				+ "  \"publicationInvariant\":"
					"\"no failed graph reached the composition commit\",\n"
				+ "  \"result\":\"PASS\"\n}\n";
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &error))
				throw std::runtime_error(
					"Could not write complex graph failure JSON: "
					+ Convert::UnicodeToUtf8(error));
		}
	});

	runner.Add("Animation composite DComp transform matches final pixel geometry", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetTargetCanvasPositionForTesting(0u, 80.0f, 80.0f);
		scene.ConfigureTargetCompositeTransformForTesting(0u);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
		scene.ForcePresentationUpdateForTesting();

		PresentationNodeSnapshot snapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, snapshot));
		CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
		CUI_EXPECT_TRUE(snapshot.CompositionTransform._12 != 0.0f);
		CUI_EXPECT_TRUE(snapshot.CompositionTransform._21 != 0.0f);
		UINT surfaceWidth = 0;
		UINT surfaceHeight = 0;
		uint64_t firstSurfaceDigest = 0;
		size_t firstOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, surfaceWidth, surfaceHeight,
			firstSurfaceDigest, firstOpaque));
		CUI_EXPECT_EQ(16u, surfaceWidth);
		CUI_EXPECT_EQ(16u, surfaceHeight);
		CUI_EXPECT_EQ(144ULL, firstOpaque);
		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("Composite capture failed: "
				+ firstCapture.Error);
		RECT firstBounds{};
		size_t firstPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u, firstBounds, firstPixels, 12u));
		CUI_EXPECT_TRUE(firstPixels > 32u);

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 320u);
		scene.ForcePresentationUpdateForTesting();
		const auto frame = scene.PresentationFrameForTesting();
		PresentationNodeSnapshot movedSnapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, movedSnapshot));
		CUI_EXPECT_EQ(1ULL, frame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, frame.CommandRecordedNodes);
		uint64_t secondSurfaceDigest = 0;
		size_t secondOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, surfaceWidth, surfaceHeight,
			secondSurfaceDigest, secondOpaque));
		CUI_EXPECT_EQ(firstSurfaceDigest, secondSurfaceDigest);
		auto secondCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!secondCapture.Error.empty())
			throw std::runtime_error("Moved composite capture failed: "
				+ secondCapture.Error);
		RECT secondBounds{};
		size_t secondPixels = 0;
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			217u, 115u, 38u, secondBounds, secondPixels, 12u));
		CUI_EXPECT_TRUE(secondPixels > 32u);
		CUI_EXPECT_TRUE(secondCapture.Digest != firstCapture.Digest);
		CUI_EXPECT_TRUE(secondBounds.left > firstBounds.left);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_COMPOSITE_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query CUI_DCOMP_COMPOSITE_PIXEL_OUTPUT.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_COMPOSITE_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read CUI_DCOMP_COMPOSITE_PIXEL_OUTPUT.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Composite pixel output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			auto matrixJson = [](const D2D1_MATRIX_3X2_F& value)
			{
				return std::string("{\"m11\":")
					+ std::to_string(value._11)
					+ ",\"m12\":" + std::to_string(value._12)
					+ ",\"m21\":" + std::to_string(value._21)
					+ ",\"m22\":" + std::to_string(value._22)
					+ ",\"offsetX\":" + std::to_string(value._31)
					+ ",\"offsetY\":" + std::to_string(value._32) + "}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\": 1,\n")
				+ "  \"engine\": \"CUI\",\n"
				+ "  \"scenario\": \"translate-scale-rotate-origin\",\n"
				+ "  \"samples\": [\n"
				+ "    {\"atMilliseconds\":160,\"matchingPixels\":"
				+ std::to_string(firstPixels) + ",\"bounds\":"
				+ boundsJson(firstBounds) + "},\n"
				+ "    {\"atMilliseconds\":320,\"matchingPixels\":"
				+ std::to_string(secondPixels) + ",\"bounds\":"
				+ boundsJson(secondBounds) + "}\n  ],\n"
				+ "  \"matrices\": ["
				+ matrixJson(snapshot.CompositionTransform) + ","
				+ matrixJson(movedSnapshot.CompositionTransform) + "],\n"
				+ "  \"surfaceDigestStable\": true\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error("Could not write composite pixel result: "
					+ Convert::UnicodeToUtf8(writeError));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation fractional DPI DComp transform matches final pixel geometry", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting(144u);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 333u);
		scene.ForcePresentationUpdateForTesting();
		PresentationNodeSnapshot firstSnapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, firstSnapshot));
		CUI_EXPECT_TRUE(firstSnapshot.CompositionIsolated);
		CUI_EXPECT_EQ(24u,
			firstSnapshot.CompositionSurfacePhysicalWidth);
		CUI_EXPECT_EQ(24u,
			firstSnapshot.CompositionSurfacePhysicalHeight);
		UINT surfaceWidth = 0;
		UINT surfaceHeight = 0;
		uint64_t firstSurfaceDigest = 0;
		size_t firstOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, surfaceWidth, surfaceHeight,
			firstSurfaceDigest, firstOpaque));
		CUI_EXPECT_EQ(24u, surfaceWidth);
		CUI_EXPECT_EQ(24u, surfaceHeight);
		CUI_EXPECT_EQ(324ULL, firstOpaque);
		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("Fractional DPI capture failed: "
				+ firstCapture.Error);
		RECT firstBounds{};
		size_t firstPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u, firstBounds, firstPixels, 12u));
		CUI_EXPECT_EQ(324ULL, firstPixels);
		CUI_EXPECT_EQ(50L, firstBounds.left);
		CUI_EXPECT_EQ(36L, firstBounds.top);
		CUI_EXPECT_EQ(68L, firstBounds.right);
		CUI_EXPECT_EQ(54L, firstBounds.bottom);

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 666u);
		scene.ForcePresentationUpdateForTesting();
		const auto frame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, frame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, frame.CommandRecordedNodes);
		uint64_t secondSurfaceDigest = 0;
		size_t secondOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, surfaceWidth, surfaceHeight,
			secondSurfaceDigest, secondOpaque));
		CUI_EXPECT_EQ(firstSurfaceDigest, secondSurfaceDigest);
		CUI_EXPECT_EQ(firstOpaque, secondOpaque);
		auto secondCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!secondCapture.Error.empty())
			throw std::runtime_error("Moved fractional DPI capture failed: "
				+ secondCapture.Error);
		RECT secondBounds{};
		size_t secondPixels = 0;
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			217u, 115u, 38u, secondBounds, secondPixels, 12u));
		CUI_EXPECT_EQ(306ULL, secondPixels);
		CUI_EXPECT_EQ(100L, secondBounds.left);
		CUI_EXPECT_EQ(36L, secondBounds.top);
		CUI_EXPECT_EQ(117L, secondBounds.right);
		CUI_EXPECT_EQ(54L, secondBounds.bottom);
		const LONG physicalDelta = secondBounds.left - firstBounds.left;
		CUI_EXPECT_EQ(50L, physicalDelta);
		CUI_EXPECT_EQ(firstBounds.top, secondBounds.top);
		CUI_EXPECT_TRUE(secondCapture.Digest != firstCapture.Digest);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_DPI_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query CUI_DCOMP_DPI_PIXEL_OUTPUT.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(), L"CUI_DCOMP_DPI_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read CUI_DCOMP_DPI_PIXEL_OUTPUT.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Fractional DPI pixel output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\": 1,\n")
				+ "  \"engine\": \"CUI\",\n"
				+ "  \"scenario\": \"translate-fractional-dpi\",\n"
				+ "  \"dpi\": 144,\n"
				+ "  \"samples\": [\n"
				+ "    {\"atMilliseconds\":333,\"matchingPixels\":"
				+ std::to_string(firstPixels) + ",\"bounds\":"
				+ boundsJson(firstBounds) + "},\n"
				+ "    {\"atMilliseconds\":666,\"matchingPixels\":"
				+ std::to_string(secondPixels) + ",\"bounds\":"
				+ boundsJson(secondBounds) + "}\n  ],\n"
				+ "  \"surface\": {\"width\":"
				+ std::to_string(surfaceWidth)
				+ ",\"height\":" + std::to_string(surfaceHeight)
				+ ",\"opaquePixels\":" + std::to_string(firstOpaque)
				+ "},\n  \"surfaceDigestStable\": true\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write fractional DPI pixel result: "
					+ Convert::UnicodeToUtf8(writeError));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation translated DComp target preserves ancestor clip", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.ConfigureHostClipForTesting(22.0f, 12.0f);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 80u);
		scene.ForcePresentationUpdateForTesting();

		PresentationNodeSnapshot snapshot;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, snapshot));
		CUI_EXPECT_TRUE(snapshot.CompositionIsolated);
		UINT surfaceWidth = 0;
		UINT surfaceHeight = 0;
		uint64_t firstSurfaceDigest = 0;
		size_t firstOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, surfaceWidth, surfaceHeight,
			firstSurfaceDigest, firstOpaque));
		CUI_EXPECT_EQ(16u, surfaceWidth);
		CUI_EXPECT_EQ(16u, surfaceHeight);
		CUI_EXPECT_EQ(144ULL, firstOpaque);
		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("Ancestor clip capture failed: "
				+ firstCapture.Error);
		RECT firstBounds{};
		size_t firstPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u, firstBounds, firstPixels, 12u));
		CUI_EXPECT_EQ(144ULL, firstPixels);
		CUI_EXPECT_EQ(8L, firstBounds.left);
		CUI_EXPECT_EQ(24L, firstBounds.top);
		CUI_EXPECT_EQ(20L, firstBounds.right);
		CUI_EXPECT_EQ(36L, firstBounds.bottom);

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
		scene.ForcePresentationUpdateForTesting();
		const auto frame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, frame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, frame.CommandRecordedNodes);
		uint64_t secondSurfaceDigest = 0;
		size_t secondOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, surfaceWidth, surfaceHeight,
			secondSurfaceDigest, secondOpaque));
		CUI_EXPECT_EQ(firstSurfaceDigest, secondSurfaceDigest);
		CUI_EXPECT_EQ(firstOpaque, secondOpaque);
		auto secondCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!secondCapture.Error.empty())
			throw std::runtime_error("Moved ancestor clip capture failed: "
				+ secondCapture.Error);
		RECT secondBounds{};
		size_t secondPixels = 0;
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			217u, 115u, 38u, secondBounds, secondPixels, 12u));
		CUI_EXPECT_EQ(72ULL, secondPixels);
		CUI_EXPECT_EQ(16L, secondBounds.left);
		CUI_EXPECT_EQ(24L, secondBounds.top);
		CUI_EXPECT_EQ(22L, secondBounds.right);
		CUI_EXPECT_EQ(36L, secondBounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_CLIP_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query CUI_DCOMP_CLIP_PIXEL_OUTPUT.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(), L"CUI_DCOMP_CLIP_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read CUI_DCOMP_CLIP_PIXEL_OUTPUT.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Ancestor clip pixel output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\": 1,\n")
				+ "  \"engine\": \"CUI\",\n"
				+ "  \"scenario\": \"translated-ancestor-clip\",\n"
				+ "  \"clip\": {\"left\":0,\"top\":24,"
					"\"right\":22,\"bottom\":36},\n"
				+ "  \"samples\": [\n"
				+ "    {\"atMilliseconds\":80,\"matchingPixels\":"
				+ std::to_string(firstPixels) + ",\"bounds\":"
				+ boundsJson(firstBounds) + "},\n"
				+ "    {\"atMilliseconds\":160,\"matchingPixels\":"
				+ std::to_string(secondPixels) + ",\"bounds\":"
				+ boundsJson(secondBounds) + "}\n  ],\n"
				+ "  \"surfaceDigestStable\": true\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write ancestor clip pixel result: "
					+ Convert::UnicodeToUtf8(writeError));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation composite DComp target preserves ancestor clip", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.ConfigureHostClipForTesting(120.0f, 160.0f);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.SetTargetCanvasPositionForTesting(0u, 80.0f, 80.0f);
		scene.ConfigureTargetCompositeTransformForTesting(0u);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 160u);
		scene.ForcePresentationUpdateForTesting();
		UINT width = 0;
		UINT height = 0;
		uint64_t firstSurfaceDigest = 0;
		size_t firstOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, firstSurfaceDigest, firstOpaque));
		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("Composite clip capture failed: "
				+ firstCapture.Error);
		RECT firstBounds{};
		size_t firstPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u, firstBounds, firstPixels, 12u));

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 320u);
		scene.ForcePresentationUpdateForTesting();
		const auto frame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, frame.CompositionOnlySegments);
		uint64_t secondSurfaceDigest = 0;
		size_t secondOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, secondSurfaceDigest, secondOpaque));
		CUI_EXPECT_EQ(firstSurfaceDigest, secondSurfaceDigest);
		auto secondCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!secondCapture.Error.empty())
			throw std::runtime_error("Moved composite clip capture failed: "
				+ secondCapture.Error);
		RECT secondBounds{};
		size_t secondPixels = 0;
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			217u, 115u, 38u, secondBounds, secondPixels, 12u));
		CUI_EXPECT_EQ(115ULL, firstPixels);
		CUI_EXPECT_EQ(96L, firstBounds.left);
		CUI_EXPECT_EQ(113L, firstBounds.top);
		CUI_EXPECT_EQ(111L, firstBounds.right);
		CUI_EXPECT_EQ(127L, firstBounds.bottom);
		CUI_EXPECT_EQ(52ULL, secondPixels);
		CUI_EXPECT_EQ(113L, secondBounds.left);
		CUI_EXPECT_EQ(123L, secondBounds.top);
		CUI_EXPECT_EQ(120L, secondBounds.right);
		CUI_EXPECT_EQ(134L, secondBounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_COMPOSITE_CLIP_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query CUI_DCOMP_COMPOSITE_CLIP_PIXEL_OUTPUT.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_COMPOSITE_CLIP_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read CUI_DCOMP_COMPOSITE_CLIP_PIXEL_OUTPUT.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Composite clip output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"composite-ancestor-clip\",\n"
				+ "  \"samples\":[\n"
				+ "    {\"atMilliseconds\":160,\"matchingPixels\":"
				+ std::to_string(firstPixels) + ",\"bounds\":"
				+ boundsJson(firstBounds) + "},\n"
				+ "    {\"atMilliseconds\":320,\"matchingPixels\":"
				+ std::to_string(secondPixels) + ",\"bounds\":"
				+ boundsJson(secondBounds) + "}\n  ],\n"
				+ "  \"surface\":{\"width\":" + std::to_string(width)
				+ ",\"height\":" + std::to_string(height)
				+ ",\"opaquePixels\":" + std::to_string(firstOpaque) + "},\n"
				+ "  \"surfaceDigestStable\":true\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error("Could not write composite clip result: "
					+ Convert::UnicodeToUtf8(writeError));
		}
		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation rotated ancestor clip preserves local geometry", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.ConfigureHostRotatedClipForTesting(
			40.0f, 40.0f, 100.0f, 56.0f, 30.0f);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 100.0f, 100.0f, -30.0f, -30.0f);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin);
		scene.ForcePresentationUpdateForTesting();
		UINT width = 0;
		UINT height = 0;
		uint64_t firstSurfaceDigest = 0;
		size_t firstOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, firstSurfaceDigest, firstOpaque));
		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("Rotated clip capture failed: "
				+ firstCapture.Error);
		RECT firstBounds{};
		size_t firstPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u, firstBounds, firstPixels, 12u));

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 40u);
		scene.ForcePresentationUpdateForTesting();
		const auto frame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, frame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, frame.CommandRecordedNodes);
		uint64_t secondSurfaceDigest = 0;
		size_t secondOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, secondSurfaceDigest, secondOpaque));
		CUI_EXPECT_EQ(firstSurfaceDigest, secondSurfaceDigest);
		CUI_EXPECT_EQ(firstOpaque, secondOpaque);
		auto secondCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!secondCapture.Error.empty())
			throw std::runtime_error("Moved rotated clip capture failed: "
				+ secondCapture.Error);
		RECT secondBounds{};
		size_t secondPixels = 0;
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			217u, 115u, 38u, secondBounds, secondPixels, 12u));
		CUI_EXPECT_EQ(1600ULL, firstPixels);
		CUI_EXPECT_EQ(firstPixels, secondPixels);
		CUI_EXPECT_EQ(93L, firstBounds.left);
		CUI_EXPECT_EQ(73L, firstBounds.top);
		CUI_EXPECT_EQ(147L, firstBounds.right);
		CUI_EXPECT_EQ(127L, firstBounds.bottom);
		CUI_EXPECT_EQ(firstBounds.left, secondBounds.left);
		CUI_EXPECT_EQ(firstBounds.top, secondBounds.top);
		CUI_EXPECT_EQ(firstBounds.right, secondBounds.right);
		CUI_EXPECT_EQ(firstBounds.bottom, secondBounds.bottom);

		const auto recoveriesBefore =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		const auto recoveredFrame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(0ULL, recoveredFrame.CompositionOnlySegments);
		CUI_EXPECT_TRUE(recoveredFrame.CommandRecordedNodes >= 1u);
		CUI_EXPECT_EQ(recoveriesBefore + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		auto recoveredCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!recoveredCapture.Error.empty())
			throw std::runtime_error(
				"Recovered retained path mask capture failed: "
				+ recoveredCapture.Error);
		RECT recoveredBounds{};
		size_t recoveredPixels = 0;
		CUI_EXPECT_TRUE(recoveredCapture.TryGetColorBounds(
			217u, 115u, 38u, recoveredBounds, recoveredPixels, 12u));
		CUI_EXPECT_EQ(firstPixels, recoveredPixels);
		CUI_EXPECT_EQ(firstBounds.left, recoveredBounds.left);
		CUI_EXPECT_EQ(firstBounds.top, recoveredBounds.top);
		CUI_EXPECT_EQ(firstBounds.right, recoveredBounds.right);
		CUI_EXPECT_EQ(firstBounds.bottom, recoveredBounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_ROTATED_CLIP_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query CUI_DCOMP_ROTATED_CLIP_PIXEL_OUTPUT.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_ROTATED_CLIP_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read rotated clip output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Rotated clip output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"rotated-ancestor-clip\",\n"
				+ "  \"samples\":[\n"
				+ "    {\"atMilliseconds\":0,\"matchingPixels\":"
				+ std::to_string(firstPixels) + ",\"bounds\":"
				+ boundsJson(firstBounds) + "},\n"
				+ "    {\"atMilliseconds\":40,\"matchingPixels\":"
				+ std::to_string(secondPixels) + ",\"bounds\":"
				+ boundsJson(secondBounds) + "}\n  ],\n"
				+ "  \"surface\":{\"width\":" + std::to_string(width)
				+ ",\"height\":" + std::to_string(height)
				+ ",\"opaquePixels\":" + std::to_string(firstOpaque) + "},\n"
				+ "  \"surfaceDigestStable\":true\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error("Could not write rotated clip result: "
					+ Convert::UnicodeToUtf8(writeError));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation rotated ClipToBounds clips ordinary raster descendants", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.ConfigureHostRotatedClipForTesting(
			40.0f, 40.0f, 100.0f, 56.0f, 30.0f);
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 100.0f, 100.0f, -30.0f, -30.0f);
		scene.ForcePresentationUpdateForTesting();
		auto capture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!capture.Error.empty())
			throw std::runtime_error("Raster rotated clip capture failed: "
				+ capture.Error);
		RECT bounds{};
		size_t pixels = 0;
		CUI_EXPECT_TRUE(capture.TryGetColorBounds(
			217u, 115u, 38u, bounds, pixels, 12u));
		CUI_EXPECT_EQ(1508ULL, pixels);
		CUI_EXPECT_EQ(94L, bounds.left);
		CUI_EXPECT_EQ(74L, bounds.top);
		CUI_EXPECT_EQ(146L, bounds.right);
		CUI_EXPECT_EQ(126L, bounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_RASTER_ROTATED_CLIP_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query raster rotated clip output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_RASTER_ROTATED_CLIP_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read raster rotated clip output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Raster rotated clip output must be under Workplans.");
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"raster-rotated-ancestor-clip\",\n"
				+ "  \"matchingPixels\":" + std::to_string(pixels) + ",\n"
				+ "  \"bounds\":{\"left\":" + std::to_string(bounds.left)
				+ ",\"top\":" + std::to_string(bounds.top)
				+ ",\"right\":" + std::to_string(bounds.right)
				+ ",\"bottom\":" + std::to_string(bounds.bottom) + "}\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write raster rotated clip result: "
					+ Convert::UnicodeToUtf8(writeError));
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation multi-level rotated ancestor clips stay nested", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.ConfigureHostRotatedClipForTesting(
			60.0f, 60.0f, 110.0f, 46.0f, 20.0f);
		scene.ConfigureNestedRotatedClipForTesting(
			0u, 50.0f, 50.0f, 25.0f, 5.0f, -35.0f);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 120.0f, 120.0f, -35.0f, -35.0f);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin);
		scene.ForcePresentationUpdateForTesting();
		UINT width = 0;
		UINT height = 0;
		uint64_t firstSurfaceDigest = 0;
		size_t firstOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, firstSurfaceDigest, firstOpaque));
		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("Multi-level clip capture failed: "
				+ firstCapture.Error);
		RECT firstBounds{};
		size_t firstPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u, firstBounds, firstPixels, 12u));

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 40u);
		scene.ForcePresentationUpdateForTesting();
		const auto frame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, frame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, frame.CommandRecordedNodes);
		uint64_t secondSurfaceDigest = 0;
		size_t secondOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, secondSurfaceDigest, secondOpaque));
		CUI_EXPECT_EQ(firstSurfaceDigest, secondSurfaceDigest);
		CUI_EXPECT_EQ(firstOpaque, secondOpaque);
		auto secondCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!secondCapture.Error.empty())
			throw std::runtime_error("Moved multi-level clip capture failed: "
				+ secondCapture.Error);
		RECT secondBounds{};
		size_t secondPixels = 0;
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			217u, 115u, 38u, secondBounds, secondPixels, 12u));
		CUI_EXPECT_EQ(1793ULL, firstPixels);
		CUI_EXPECT_EQ(firstPixels, secondPixels);
		CUI_EXPECT_EQ(128L, firstBounds.left);
		CUI_EXPECT_EQ(78L, firstBounds.top);
		CUI_EXPECT_EQ(178L, firstBounds.right);
		CUI_EXPECT_EQ(135L, firstBounds.bottom);
		CUI_EXPECT_EQ(firstBounds.left, secondBounds.left);
		CUI_EXPECT_EQ(firstBounds.top, secondBounds.top);
		CUI_EXPECT_EQ(firstBounds.right, secondBounds.right);
		CUI_EXPECT_EQ(firstBounds.bottom, secondBounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_MULTILEVEL_CLIP_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query multi-level clip output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_MULTILEVEL_CLIP_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read multi-level clip output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Multi-level clip output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"multi-level-ancestor-clip\",\n"
				+ "  \"samples\":[\n"
				+ "    {\"atMilliseconds\":0,\"matchingPixels\":"
				+ std::to_string(firstPixels) + ",\"bounds\":"
				+ boundsJson(firstBounds) + "},\n"
				+ "    {\"atMilliseconds\":40,\"matchingPixels\":"
				+ std::to_string(secondPixels) + ",\"bounds\":"
				+ boundsJson(secondBounds) + "}\n  ],\n"
				+ "  \"surface\":{\"width\":" + std::to_string(width)
				+ ",\"height\":" + std::to_string(height)
				+ ",\"opaquePixels\":" + std::to_string(firstOpaque) + "},\n"
				+ "  \"surfaceDigestStable\":true\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error("Could not write multi-level clip result: "
					+ Convert::UnicodeToUtf8(writeError));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation rounded Geometry ancestor clip stays composition-fixed", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.ConfigureHostRoundedGeometryClipForTesting(
			60.0f, 60.0f, 110.0f, 46.0f, 20.0f, 14.0f, 10.0f);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 120.0f, 120.0f, -30.0f, -30.0f);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin);
		scene.ForcePresentationUpdateForTesting();
		UINT width = 0;
		UINT height = 0;
		uint64_t firstSurfaceDigest = 0;
		size_t firstOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, firstSurfaceDigest, firstOpaque));
		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("Rounded Geometry clip capture failed: "
				+ firstCapture.Error);
		RECT firstBounds{};
		size_t firstPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u, firstBounds, firstPixels, 12u));

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 40u);
		scene.ForcePresentationUpdateForTesting();
		const auto frame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, frame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, frame.CommandRecordedNodes);
		uint64_t secondSurfaceDigest = 0;
		size_t secondOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, secondSurfaceDigest, secondOpaque));
		CUI_EXPECT_EQ(firstSurfaceDigest, secondSurfaceDigest);
		CUI_EXPECT_EQ(firstOpaque, secondOpaque);
		auto secondCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!secondCapture.Error.empty())
			throw std::runtime_error("Moved rounded clip capture failed: "
				+ secondCapture.Error);
		RECT secondBounds{};
		size_t secondPixels = 0;
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			217u, 115u, 38u, secondBounds, secondPixels, 12u));
		CUI_EXPECT_EQ(3468ULL, firstPixels);
		CUI_EXPECT_EQ(firstPixels, secondPixels);
		CUI_EXPECT_EQ(108L, firstBounds.left);
		CUI_EXPECT_EQ(64L, firstBounds.top);
		CUI_EXPECT_EQ(179L, firstBounds.right);
		CUI_EXPECT_EQ(134L, firstBounds.bottom);
		CUI_EXPECT_EQ(firstBounds.left, secondBounds.left);
		CUI_EXPECT_EQ(firstBounds.top, secondBounds.top);
		CUI_EXPECT_EQ(firstBounds.right, secondBounds.right);
		CUI_EXPECT_EQ(firstBounds.bottom, secondBounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_ROUNDED_CLIP_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query rounded clip output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_ROUNDED_CLIP_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read rounded clip output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Rounded clip output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"rounded-geometry-ancestor-clip\",\n"
				+ "  \"samples\":[\n"
				+ "    {\"atMilliseconds\":0,\"matchingPixels\":"
				+ std::to_string(firstPixels) + ",\"bounds\":"
				+ boundsJson(firstBounds) + "},\n"
				+ "    {\"atMilliseconds\":40,\"matchingPixels\":"
				+ std::to_string(secondPixels) + ",\"bounds\":"
				+ boundsJson(secondBounds) + "}\n  ],\n"
				+ "  \"surface\":{\"width\":" + std::to_string(width)
				+ ",\"height\":" + std::to_string(height)
				+ ",\"opaquePixels\":" + std::to_string(firstOpaque) + "},\n"
				+ "  \"surfaceDigestStable\":true\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error("Could not write rounded clip result: "
					+ Convert::UnicodeToUtf8(writeError));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation rounded Geometry clips ordinary raster descendants", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.ConfigureHostRoundedGeometryClipForTesting(
			60.0f, 60.0f, 110.0f, 46.0f, 20.0f, 14.0f, 10.0f);
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 120.0f, 120.0f, -30.0f, -30.0f);
		scene.ForcePresentationUpdateForTesting();
		auto capture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!capture.Error.empty())
			throw std::runtime_error("Raster rounded clip capture failed: "
				+ capture.Error);
		RECT bounds{};
		size_t pixels = 0;
		CUI_EXPECT_TRUE(capture.TryGetColorBounds(
			217u, 115u, 38u, bounds, pixels, 12u));
		CUI_EXPECT_EQ(3358ULL, pixels);
		CUI_EXPECT_EQ(109L, bounds.left);
		CUI_EXPECT_EQ(65L, bounds.top);
		CUI_EXPECT_EQ(179L, bounds.right);
		CUI_EXPECT_EQ(133L, bounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_RASTER_ROUNDED_CLIP_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query raster rounded clip output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_RASTER_ROUNDED_CLIP_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read raster rounded clip output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Raster rounded clip output must be under Workplans.");
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"raster-rounded-geometry-clip\",\n"
				+ "  \"matchingPixels\":" + std::to_string(pixels) + ",\n"
				+ "  \"bounds\":{\"left\":" + std::to_string(bounds.left)
				+ ",\"top\":" + std::to_string(bounds.top)
				+ ",\"right\":" + std::to_string(bounds.right)
				+ ",\"bottom\":" + std::to_string(bounds.bottom) + "}\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write raster rounded clip result: "
					+ Convert::UnicodeToUtf8(writeError));
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation EllipseGeometry ancestor clip stays composition-fixed", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.ConfigureHostEllipseGeometryClipForTesting(
			60.0f, 60.0f, 110.0f, 46.0f, 20.0f,
			D2D1::Point2F(30.0f, 30.0f), 26.0f, 18.0f);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 120.0f, 120.0f, -30.0f, -30.0f);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin);
		scene.ForcePresentationUpdateForTesting();
		UINT width = 0;
		UINT height = 0;
		uint64_t firstSurfaceDigest = 0;
		size_t firstOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, firstSurfaceDigest, firstOpaque));
		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("Ellipse Geometry clip capture failed: "
				+ firstCapture.Error);
		RECT firstBounds{};
		size_t firstPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u, firstBounds, firstPixels, 12u));

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 40u);
		scene.ForcePresentationUpdateForTesting();
		const auto frame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, frame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, frame.CommandRecordedNodes);
		uint64_t secondSurfaceDigest = 0;
		size_t secondOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, secondSurfaceDigest, secondOpaque));
		CUI_EXPECT_EQ(firstSurfaceDigest, secondSurfaceDigest);
		CUI_EXPECT_EQ(firstOpaque, secondOpaque);
		auto secondCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!secondCapture.Error.empty())
			throw std::runtime_error("Moved ellipse clip capture failed: "
				+ secondCapture.Error);
		RECT secondBounds{};
		size_t secondPixels = 0;
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			217u, 115u, 38u, secondBounds, secondPixels, 12u));
		CUI_EXPECT_EQ(1453ULL, firstPixels);
		CUI_EXPECT_EQ(firstPixels, secondPixels);
		CUI_EXPECT_EQ(118L, firstBounds.left);
		CUI_EXPECT_EQ(80L, firstBounds.top);
		CUI_EXPECT_EQ(169L, firstBounds.right);
		CUI_EXPECT_EQ(118L, firstBounds.bottom);
		CUI_EXPECT_EQ(firstBounds.left, secondBounds.left);
		CUI_EXPECT_EQ(firstBounds.top, secondBounds.top);
		CUI_EXPECT_EQ(firstBounds.right, secondBounds.right);
		CUI_EXPECT_EQ(firstBounds.bottom, secondBounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_DCOMP_ELLIPSE_CLIP_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query ellipse clip output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_DCOMP_ELLIPSE_CLIP_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read ellipse clip output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Ellipse clip output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"ellipse-geometry-ancestor-clip\",\n"
				+ "  \"samples\":[\n"
				+ "    {\"atMilliseconds\":0,\"matchingPixels\":"
				+ std::to_string(firstPixels) + ",\"bounds\":"
				+ boundsJson(firstBounds) + "},\n"
				+ "    {\"atMilliseconds\":40,\"matchingPixels\":"
				+ std::to_string(secondPixels) + ",\"bounds\":"
				+ boundsJson(secondBounds) + "}\n  ],\n"
				+ "  \"surface\":{\"width\":" + std::to_string(width)
				+ ",\"height\":" + std::to_string(height)
				+ ",\"opaquePixels\":" + std::to_string(firstOpaque) + "},\n"
				+ "  \"surfaceDigestStable\":true\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error("Could not write ellipse clip result: "
					+ Convert::UnicodeToUtf8(writeError));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation EllipseGeometry clips ordinary raster descendants", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.ConfigureHostEllipseGeometryClipForTesting(
			60.0f, 60.0f, 110.0f, 46.0f, 20.0f,
			D2D1::Point2F(30.0f, 30.0f), 26.0f, 18.0f);
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 120.0f, 120.0f, -30.0f, -30.0f);
		scene.ForcePresentationUpdateForTesting();
		auto capture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!capture.Error.empty())
			throw std::runtime_error("Raster ellipse clip capture failed: "
				+ capture.Error);
		RECT bounds{};
		size_t pixels = 0;
		CUI_EXPECT_TRUE(capture.TryGetColorBounds(
			217u, 115u, 38u, bounds, pixels, 12u));
		CUI_EXPECT_EQ(1387ULL, pixels);
		CUI_EXPECT_EQ(119L, bounds.left);
		CUI_EXPECT_EQ(81L, bounds.top);
		CUI_EXPECT_EQ(168L, bounds.right);
		CUI_EXPECT_EQ(118L, bounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_RASTER_ELLIPSE_CLIP_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query raster ellipse clip output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_RASTER_ELLIPSE_CLIP_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read raster ellipse clip output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Raster ellipse clip output must be under Workplans.");
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"raster-ellipse-geometry-clip\",\n"
				+ "  \"matchingPixels\":" + std::to_string(pixels) + ",\n"
				+ "  \"bounds\":{\"left\":" + std::to_string(bounds.left)
				+ ",\"top\":" + std::to_string(bounds.top)
				+ ",\"right\":" + std::to_string(bounds.right)
				+ ",\"bottom\":" + std::to_string(bounds.bottom) + "}\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write raster ellipse clip result: "
					+ Convert::UnicodeToUtf8(writeError));
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation PathGeometry clips ordinary raster descendants", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.ConfigureHostPathGeometryClipForTesting(
			60.0f, 60.0f, 110.0f, 46.0f, 20.0f);
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 120.0f, 120.0f, -30.0f, -30.0f);
		scene.ForcePresentationUpdateForTesting();
		auto capture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!capture.Error.empty())
			throw std::runtime_error("Raster path clip capture failed: "
				+ capture.Error);
		RECT bounds{};
		size_t pixels = 0;
		CUI_EXPECT_TRUE(capture.TryGetColorBounds(
			217u, 115u, 38u, bounds, pixels, 12u));
		CUI_EXPECT_EQ(1130ULL, pixels);
		CUI_EXPECT_EQ(129L, bounds.left);
		CUI_EXPECT_EQ(68L, bounds.top);
		CUI_EXPECT_EQ(173L, bounds.right);
		CUI_EXPECT_EQ(122L, bounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_RASTER_PATH_CLIP_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query raster path clip output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(), L"CUI_RASTER_PATH_CLIP_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read raster path clip output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Raster path clip output must be under Workplans.");
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"raster-path-geometry-clip\",\n"
				+ "  \"matchingPixels\":" + std::to_string(pixels) + ",\n"
				+ "  \"bounds\":{\"left\":" + std::to_string(bounds.left)
				+ ",\"top\":" + std::to_string(bounds.top)
				+ ",\"right\":" + std::to_string(bounds.right)
				+ ",\"bottom\":" + std::to_string(bounds.bottom) + "}\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error("Could not write raster path clip result: "
					+ Convert::UnicodeToUtf8(writeError));
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation shared A8 Geometry coverage is not an exact layer substitute", []
	{
		using Microsoft::WRL::ComPtr;
		constexpr UINT TargetWidth = 160u;
		constexpr UINT TargetHeight = 128u;
		constexpr UINT MaskWidth = 192u;
		constexpr UINT MaskHeight = 160u;
		struct Case final
		{
			const char* Name = nullptr;
			FLOAT Dpi = 96.0f;
			D2D1_MATRIX_3X2_F RootTransform =
				D2D1::Matrix3x2F::Identity();
			LONG SurfaceOriginPixelX = 0;
			LONG SurfaceOriginPixelY = 0;
		};
		struct Result final
		{
			const char* Name = nullptr;
			FLOAT Dpi = 96.0f;
			size_t MismatchedPixels = 0u;
			unsigned MaximumChannelDelta = 0u;
			uint64_t NativeDigest = 0u;
			uint64_t CoverageDigest = 0u;
		};

		auto require = [](HRESULT value, const char* operation)
		{
			if (FAILED(value))
				throw std::runtime_error(std::string(operation)
					+ " failed with HRESULT "
					+ std::to_string(static_cast<unsigned long>(value)) + ".");
		};
		auto createTarget = [&require](ID2D1DeviceContext* context,
			UINT width, UINT height, FLOAT dpi, DXGI_FORMAT format)
		{
			ComPtr<ID2D1Bitmap1> bitmap;
			const auto properties = D2D1::BitmapProperties1(
				D2D1_BITMAP_OPTIONS_TARGET,
				D2D1::PixelFormat(format, D2D1_ALPHA_MODE_PREMULTIPLIED),
				dpi, dpi);
			require(context->CreateBitmap(
				D2D1::SizeU(width, height), nullptr, 0u,
				&properties, bitmap.GetAddressOf()), "Create target bitmap");
			return bitmap;
		};
		auto readPixels = [&require](ID2D1DeviceContext* context,
			ID2D1Bitmap1* source)
		{
			const auto size = source->GetPixelSize();
			FLOAT dpiX = 96.0f;
			FLOAT dpiY = 96.0f;
			source->GetDpi(&dpiX, &dpiY);
			const auto properties = D2D1::BitmapProperties1(
				D2D1_BITMAP_OPTIONS_CPU_READ
					| D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
				source->GetPixelFormat(), dpiX, dpiY);
			ComPtr<ID2D1Bitmap1> staging;
			require(context->CreateBitmap(size, nullptr, 0u,
				&properties, staging.GetAddressOf()), "Create readback bitmap");
			require(staging->CopyFromBitmap(nullptr, source, nullptr),
				"Copy target for readback");
			D2D1_MAPPED_RECT mapped{};
			require(staging->Map(D2D1_MAP_OPTIONS_READ, &mapped),
				"Map readback bitmap");
			struct UnmapScope final
			{
				ID2D1Bitmap1* Bitmap = nullptr;
				~UnmapScope() { if (Bitmap) Bitmap->Unmap(); }
			} unmap{ staging.Get() };
			const size_t rowBytes = static_cast<size_t>(size.width) * 4u;
			std::vector<uint8_t> result(rowBytes * size.height);
			for (UINT row = 0u; row < size.height; ++row)
				memcpy(result.data() + static_cast<size_t>(row) * rowBytes,
					mapped.bits + static_cast<size_t>(row) * mapped.pitch,
					rowBytes);
			return result;
		};
		auto digest = [](const std::vector<uint8_t>& bytes)
		{
			constexpr uint64_t Offset = 14695981039346656037ull;
			constexpr uint64_t Prime = 1099511628211ull;
			uint64_t value = Offset;
			for (const auto byte : bytes)
			{
				value ^= byte;
				value *= Prime;
			}
			return value;
		};

		ComPtr<ID2D1Device> device;
		require(Graphics_AcquireSharedD3DDevice(
			nullptr, nullptr, nullptr, device.GetAddressOf(), nullptr),
			"Acquire shared Direct2D device");
		ComPtr<ID2D1PathGeometry> geometry;
		require(_D2DFactory->CreatePathGeometry(geometry.GetAddressOf()),
			"Create oracle PathGeometry");
		ComPtr<ID2D1GeometrySink> sink;
		require(geometry->Open(sink.GetAddressOf()),
			"Open oracle PathGeometry");
		sink->SetFillMode(D2D1_FILL_MODE_WINDING);
		sink->BeginFigure(D2D1::Point2F(13.25f, 10.75f),
			D2D1_FIGURE_BEGIN_FILLED);
		sink->AddBezier(D2D1::BezierSegment(
			D2D1::Point2F(31.5f, 2.25f),
			D2D1::Point2F(58.75f, 8.5f),
			D2D1::Point2F(56.125f, 27.375f)));
		sink->AddLine(D2D1::Point2F(45.625f, 48.25f));
		sink->AddBezier(D2D1::BezierSegment(
			D2D1::Point2F(30.25f, 56.625f),
			D2D1::Point2F(7.875f, 45.125f),
			D2D1::Point2F(13.25f, 10.75f)));
		sink->EndFigure(D2D1_FIGURE_END_CLOSED);
		require(sink->Close(), "Close oracle PathGeometry");

		const std::array cases{
			Case{ "dpi-96-root-geometry", 96.0f,
				D2D1::Matrix3x2F::Translation(19.375f, 17.625f),
				11, 7 },
			Case{ "dpi-120-root-geometry", 120.0f,
				D2D1::Matrix3x2F::Scale(1.125f, 0.875f,
					D2D1::Point2F(32.0f, 28.0f))
				* D2D1::Matrix3x2F::Translation(24.4f, 19.6f),
				23, 17 },
			Case{ "dpi-144-root-geometry", 144.0f,
				D2D1::Matrix3x2F::Rotation(17.0f,
					D2D1::Point2F(32.0f, 28.0f))
				* D2D1::Matrix3x2F::Translation(27.333333f, 18.666667f),
				31, 19 }
		};
		std::vector<Result> results;
		results.reserve(cases.size());
		for (const auto& testCase : cases)
		{
			ComPtr<ID2D1TransformedGeometry> rootGeometry;
			require(_D2DFactory->CreateTransformedGeometry(
				geometry.Get(), &testCase.RootTransform,
				rootGeometry.GetAddressOf()),
				"Create root-space oracle geometry");
			const FLOAT pixelToDip = 96.0f / testCase.Dpi;
			const auto surfaceTransform = D2D1::Matrix3x2F::Translation(
				-static_cast<FLOAT>(testCase.SurfaceOriginPixelX) * pixelToDip,
				-static_cast<FLOAT>(testCase.SurfaceOriginPixelY) * pixelToDip);
			ComPtr<ID2D1DeviceContext> maskContext;
			ComPtr<ID2D1DeviceContext> nativeContext;
			ComPtr<ID2D1DeviceContext> coverageContext;
			require(device->CreateDeviceContext(
				D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
				maskContext.GetAddressOf()), "Create mask device context");
			require(device->CreateDeviceContext(
				D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
				nativeContext.GetAddressOf()), "Create native device context");
			require(device->CreateDeviceContext(
				D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
				coverageContext.GetAddressOf()), "Create coverage device context");
			maskContext->SetDpi(testCase.Dpi, testCase.Dpi);
			nativeContext->SetDpi(testCase.Dpi, testCase.Dpi);
			coverageContext->SetDpi(testCase.Dpi, testCase.Dpi);

			auto mask = createTarget(maskContext.Get(), MaskWidth, MaskHeight,
				testCase.Dpi, DXGI_FORMAT_A8_UNORM);
			maskContext->SetTarget(mask.Get());
			ComPtr<ID2D1SolidColorBrush> maskBrush;
			require(maskContext->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::White),
				maskBrush.GetAddressOf()), "Create mask brush");
			maskContext->BeginDraw();
			maskContext->SetTransform(D2D1::Matrix3x2F::Identity());
			maskContext->Clear(D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
			maskContext->FillGeometry(rootGeometry.Get(), maskBrush.Get());
			require(maskContext->EndDraw(), "Rasterize A8 Geometry mask");

			auto nativeTarget = createTarget(nativeContext.Get(),
				TargetWidth, TargetHeight, testCase.Dpi,
				DXGI_FORMAT_B8G8R8A8_UNORM);
			nativeContext->SetTarget(nativeTarget.Get());
			ComPtr<ID2D1SolidColorBrush> nativeBrush;
			require(nativeContext->CreateSolidColorBrush(
				D2D1::ColorF(0.149f, 0.451f, 0.851f, 0.729f),
				nativeBrush.GetAddressOf()), "Create native content brush");
			nativeContext->BeginDraw();
			nativeContext->Clear(D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
			nativeContext->SetTransform(surfaceTransform);
			const auto nativeLayer = D2D1::LayerParameters1(
				D2D1::InfiniteRect(), rootGeometry.Get(),
				D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
				D2D1::Matrix3x2F::Identity(), 1.0f, nullptr,
				D2D1_LAYER_OPTIONS1_NONE);
			nativeContext->PushLayer(&nativeLayer, nullptr);
			nativeContext->FillRectangle(
				D2D1::RectF(-64.0f, -64.0f, 192.0f, 160.0f),
				nativeBrush.Get());
			nativeContext->PopLayer();
			require(nativeContext->EndDraw(), "Render native Geometry layer");

			auto coverageTarget = createTarget(coverageContext.Get(),
				TargetWidth, TargetHeight, testCase.Dpi,
				DXGI_FORMAT_B8G8R8A8_UNORM);
			coverageContext->SetTarget(coverageTarget.Get());
			ComPtr<ID2D1SolidColorBrush> coverageBrush;
			require(coverageContext->CreateSolidColorBrush(
				D2D1::ColorF(0.149f, 0.451f, 0.851f, 0.729f),
				coverageBrush.GetAddressOf()), "Create coverage content brush");
			const auto bitmapProperties = D2D1::BitmapBrushProperties1(
				D2D1_EXTEND_MODE_CLAMP, D2D1_EXTEND_MODE_CLAMP,
				D2D1_INTERPOLATION_MODE_LINEAR);
			ComPtr<ID2D1BitmapBrush1> opacityBrush;
			require(coverageContext->CreateBitmapBrush(mask.Get(),
				&bitmapProperties, nullptr, opacityBrush.GetAddressOf()),
				"Create cross-context A8 opacity brush");
			coverageContext->BeginDraw();
			coverageContext->Clear(D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
			coverageContext->SetTransform(surfaceTransform);
			const auto coverageLayer = D2D1::LayerParameters1(
				D2D1::InfiniteRect(), nullptr,
				D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
				D2D1::Matrix3x2F::Identity(), 1.0f, opacityBrush.Get(),
				D2D1_LAYER_OPTIONS1_NONE);
			coverageContext->PushLayer(&coverageLayer, nullptr);
			coverageContext->FillRectangle(
				D2D1::RectF(-64.0f, -64.0f, 192.0f, 160.0f),
				coverageBrush.Get());
			coverageContext->PopLayer();
			require(coverageContext->EndDraw(),
				"Render shared A8 coverage layer");

			const auto nativePixels = readPixels(
				nativeContext.Get(), nativeTarget.Get());
			const auto coveragePixels = readPixels(
				coverageContext.Get(), coverageTarget.Get());
			CUI_EXPECT_EQ(nativePixels.size(), coveragePixels.size());
			Result result{ testCase.Name, testCase.Dpi, 0u, 0u,
				digest(nativePixels), digest(coveragePixels) };
			for (size_t pixel = 0u; pixel + 3u < nativePixels.size(); pixel += 4u)
			{
				bool mismatch = false;
				for (size_t channel = 0u; channel < 4u; ++channel)
				{
					const auto delta = static_cast<unsigned>(std::abs(
						static_cast<int>(nativePixels[pixel + channel])
						- static_cast<int>(coveragePixels[pixel + channel])));
					result.MaximumChannelDelta = (std::max)(
						result.MaximumChannelDelta, delta);
					mismatch = mismatch || delta != 0u;
				}
				if (mismatch) ++result.MismatchedPixels;
			}
			results.push_back(result);
		}

		size_t outputLength = 0u;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_A8_GEOMETRY_MASK_ORACLE_OUTPUT") != 0)
			throw std::runtime_error("Could not query A8 mask oracle output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(), L"CUI_A8_GEOMETRY_MASK_ORACLE_OUTPUT") != 0)
				throw std::runtime_error("Could not read A8 mask oracle output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"A8 mask oracle output must be under Workplans.");
			std::string json = std::string("{\n")
				+ "  \"schemaVersion\":1,\n"
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"shared-a8-geometry-coverage-oracle\",\n"
				+ "  \"resourceDomain\":\"same-ID2D1Device-cross-context\",\n"
				+ "  \"coordinateModel\":\"root-space-mask-plus-physical-pixel-aligned-surface-origin\",\n"
				+ "  \"candidateDecision\":\"REJECTED_NON_IDENTICAL_PIXELS\",\n"
				+ "  \"cases\":[\n";
			for (size_t index = 0u; index < results.size(); ++index)
			{
				const auto& result = results[index];
				json += std::string("    {\"name\":\"") + result.Name
					+ "\",\"dpi\":" + FormatDouble(result.Dpi)
					+ ",\"mismatchedPixels\":"
					+ std::to_string(result.MismatchedPixels)
					+ ",\"maximumChannelDelta\":"
					+ std::to_string(result.MaximumChannelDelta)
					+ ",\"nativeDigest\":\""
					+ std::to_string(result.NativeDigest)
					+ "\",\"coverageDigest\":\""
					+ std::to_string(result.CoverageDigest) + "\"}"
					+ (index + 1u == results.size() ? "\n" : ",\n");
			}
			json += "  ]\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error("Could not write A8 mask oracle result: "
					+ Convert::UnicodeToUtf8(writeError));
		}
		CUI_EXPECT_EQ(cases.size(), results.size());
		size_t mismatchedPixels = 0u;
		unsigned maximumChannelDelta = 0u;
		for (const auto& result : results)
		{
			mismatchedPixels += result.MismatchedPixels;
			maximumChannelDelta = (std::max)(
				maximumChannelDelta, result.MaximumChannelDelta);
		}
		CUI_EXPECT_TRUE(mismatchedPixels > 0u);
		CUI_EXPECT_TRUE(maximumChannelDelta > 0u);
	});

	runner.Add("Animation retained siblings share one exact PathGeometry surface", []
	{
		const std::array propertyKinds{
			BenchmarkPropertyKind::TransformX,
			BenchmarkPropertyKind::TransformX };
		BenchmarkScene scene(
			2u, 0u, BenchmarkPropertyKind::TransformX,
			1u, false, false, false, propertyKinds);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.ConfigureHostPathGeometryClipForTesting(
			60.0f, 60.0f, 110.0f, 46.0f, 20.0f);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting(96u);
		scene.ConfigureTargetRectangleForTesting(
			0u, 44.0f, 44.0f, 0.0f, 0.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 44.0f, 44.0f, 8.0f, 6.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 0.85f, 0.15f, 0.10f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			1u, D2D1_COLOR_F{ 0.15f, 0.45f, 0.85f, 1.0f });
		scene.ConfigureTargetCompositeTransformForTesting(1u);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin);
		scene.ForcePresentationUpdateForTesting();
		const auto first = scene.PresentationFrameForTesting();
		const auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error(
				"First shared Geometry surface capture failed: "
				+ firstCapture.Error);
		RECT firstRedBounds{};
		RECT firstBlueBounds{};
		size_t firstRedPixels = 0u;
		size_t firstBluePixels = 0u;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			26u, 38u, 217u, firstRedBounds, firstRedPixels, 12u));
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u, firstBlueBounds, firstBluePixels, 12u));
		CUI_EXPECT_EQ(107ULL, firstRedPixels);
		CUI_EXPECT_EQ(843ULL, firstBluePixels);
		CUI_EXPECT_EQ(129L, firstRedBounds.left);
		CUI_EXPECT_EQ(68L, firstRedBounds.top);
		CUI_EXPECT_EQ(161L, firstRedBounds.right);
		CUI_EXPECT_EQ(91L, firstRedBounds.bottom);
		CUI_EXPECT_EQ(130L, firstBlueBounds.left);
		CUI_EXPECT_EQ(70L, firstBlueBounds.top);
		CUI_EXPECT_EQ(161L, firstBlueBounds.right);
		CUI_EXPECT_EQ(119L, firstBlueBounds.bottom);
		CUI_EXPECT_EQ(1ULL,
			first.Preparation.AncestorGeometryMaskMaterializationCount);
		CUI_EXPECT_EQ(1ULL,
			first.Preparation.GeometryRasterGroupCount);
		CUI_EXPECT_EQ(2ULL,
			first.Preparation.GeometryRasterMemberCount);
		CUI_EXPECT_EQ(1ULL, first.AncestorGeometryMaskLayerPushCount);
		PresentationNodeSnapshot firstMember;
		PresentationNodeSnapshot secondMember;
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			0u, firstMember));
		CUI_EXPECT_TRUE(scene.TryGetTargetPresentationSnapshotForTesting(
			1u, secondMember));
		CUI_EXPECT_EQ(firstMember.SegmentIndex, secondMember.SegmentIndex);
		CUI_EXPECT_TRUE(
			firstMember.CompositionTransform._11
				!= secondMember.CompositionTransform._11
			|| firstMember.CompositionTransform._12
				!= secondMember.CompositionTransform._12
			|| firstMember.CompositionTransform._21
				!= secondMember.CompositionTransform._21
			|| firstMember.CompositionTransform._22
				!= secondMember.CompositionTransform._22);

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 40u);
		scene.ForcePresentationUpdateForTesting();
		const auto steady = scene.PresentationFrameForTesting();
		const auto steadyCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!steadyCapture.Error.empty())
			throw std::runtime_error(
				"Moved shared Geometry surface capture failed: "
				+ steadyCapture.Error);
		RECT steadyRedBounds{};
		RECT steadyBlueBounds{};
		size_t steadyRedPixels = 0u;
		size_t steadyBluePixels = 0u;
		CUI_EXPECT_TRUE(steadyCapture.TryGetColorBounds(
			26u, 38u, 217u, steadyRedBounds, steadyRedPixels, 12u));
		CUI_EXPECT_TRUE(steadyCapture.TryGetColorBounds(
			217u, 115u, 38u, steadyBlueBounds, steadyBluePixels, 12u));
		CUI_EXPECT_EQ(184ULL, steadyRedPixels);
		CUI_EXPECT_EQ(809ULL, steadyBluePixels);
		CUI_EXPECT_EQ(129L, steadyRedBounds.left);
		CUI_EXPECT_EQ(68L, steadyRedBounds.top);
		CUI_EXPECT_EQ(165L, steadyRedBounds.right);
		CUI_EXPECT_EQ(95L, steadyRedBounds.bottom);
		CUI_EXPECT_EQ(130L, steadyBlueBounds.left);
		CUI_EXPECT_EQ(72L, steadyBlueBounds.top);
		CUI_EXPECT_EQ(161L, steadyBlueBounds.right);
		CUI_EXPECT_EQ(119L, steadyBlueBounds.bottom);
		CUI_EXPECT_EQ(0ULL,
			steady.Preparation.AncestorGeometryMaskMaterializationCount);
		CUI_EXPECT_TRUE(
			steady.Preparation.AncestorGeometryMaskReuseCount >= 1u);
		CUI_EXPECT_EQ(1ULL,
			steady.Preparation.GeometryRasterGroupCount);
		CUI_EXPECT_EQ(2ULL,
			steady.Preparation.GeometryRasterMemberCount);
		CUI_EXPECT_EQ(0ULL, steady.CommandRecordedNodes);
		CUI_EXPECT_TRUE(steady.CommandReplayedNodes >= 2u);
		CUI_EXPECT_EQ(1ULL, steady.AncestorGeometryMaskLayerPushCount);
		const auto recoveries =
			scene.PresentationDeviceRecoveryCountForTesting();
		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		CUI_EXPECT_EQ(recoveries + 1u,
			scene.PresentationDeviceRecoveryCountForTesting());
		const auto recovered = scene.PresentationFrameForTesting();
		const auto recoveredCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!recoveredCapture.Error.empty())
			throw std::runtime_error(
				"Recovered shared Geometry surface capture failed: "
				+ recoveredCapture.Error);
		CUI_EXPECT_EQ(steadyCapture.Width, recoveredCapture.Width);
		CUI_EXPECT_EQ(steadyCapture.Height, recoveredCapture.Height);
		CUI_EXPECT_EQ(steadyCapture.Bgra, recoveredCapture.Bgra);
		CUI_EXPECT_EQ(0ULL,
			recovered.Preparation.AncestorGeometryMaskMaterializationCount);
		CUI_EXPECT_TRUE(
			recovered.Preparation.AncestorGeometryMaskReuseCount >= 1u);
		CUI_EXPECT_EQ(1ULL,
			recovered.Preparation.GeometryRasterGroupCount);
		CUI_EXPECT_EQ(2ULL,
			recovered.Preparation.GeometryRasterMemberCount);
		CUI_EXPECT_EQ(1ULL, recovered.AncestorGeometryMaskLayerPushCount);
		scene.SetWindowDpiForTesting(120u);
		scene.ForcePresentationUpdateForTesting();
		const auto resized = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, resized.Preparation.GeometryRasterGroupCount);
		CUI_EXPECT_EQ(2ULL, resized.Preparation.GeometryRasterMemberCount);
		CUI_EXPECT_EQ(1ULL, resized.AncestorGeometryMaskLayerPushCount);
		CUI_EXPECT_EQ(0ULL, resized.GeometryRasterPartialUpdateCount);
		CUI_EXPECT_EQ(1ULL, resized.GeometryRasterFullReplayCount);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_SHARED_GEOMETRY_MASK_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query shared Geometry mask output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(), L"CUI_SHARED_GEOMETRY_MASK_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read shared Geometry mask output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Shared Geometry mask output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\":1,\n"
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"coalesced-exact-geometry-raster-group\",\n"
				+ "  \"firstMaterializations\":"
				+ std::to_string(first.Preparation.
					AncestorGeometryMaskMaterializationCount) + ",\n"
				+ "  \"firstReuses\":"
				+ std::to_string(first.Preparation.
					AncestorGeometryMaskReuseCount) + ",\n"
				+ "  \"steadyMaterializations\":"
				+ std::to_string(steady.Preparation.
					AncestorGeometryMaskMaterializationCount) + ",\n"
				+ "  \"steadyReuses\":"
				+ std::to_string(steady.Preparation.
					AncestorGeometryMaskReuseCount) + ",\n"
				+ "  \"steadyCommandRecordedNodes\":"
				+ std::to_string(steady.CommandRecordedNodes) + ",\n"
				+ "  \"geometryRasterGroupCount\":"
				+ std::to_string(steady.Preparation.
					GeometryRasterGroupCount) + ",\n"
				+ "  \"geometryRasterMemberCount\":"
				+ std::to_string(steady.Preparation.
					GeometryRasterMemberCount) + ",\n"
				+ "  \"firstLayerPushes\":"
				+ std::to_string(first.AncestorGeometryMaskLayerPushCount) + ",\n"
				+ "  \"steadyLayerPushes\":"
				+ std::to_string(steady.AncestorGeometryMaskLayerPushCount) + ",\n"
				+ "  \"firstRedPixels\":"
				+ std::to_string(firstRedPixels) + ",\n"
				+ "  \"firstBluePixels\":"
				+ std::to_string(firstBluePixels) + ",\n"
				+ "  \"steadyRedPixels\":"
				+ std::to_string(steadyRedPixels) + ",\n"
				+ "  \"steadyBluePixels\":"
				+ std::to_string(steadyBluePixels) + ",\n"
				+ "  \"firstRedBounds\":"
				+ boundsJson(firstRedBounds) + ",\n"
				+ "  \"firstBlueBounds\":"
				+ boundsJson(firstBlueBounds) + ",\n"
				+ "  \"steadyRedBounds\":"
				+ boundsJson(steadyRedBounds) + ",\n"
				+ "  \"steadyBlueBounds\":"
				+ boundsJson(steadyBlueBounds) + ",\n"
				+ "  \"firstCaptureDigest\":\""
				+ std::to_string(firstCapture.Digest) + "\",\n"
				+ "  \"steadyCaptureDigest\":\""
				+ std::to_string(steadyCapture.Digest) + "\",\n"
				+ "  \"recoveredCaptureDigest\":\""
				+ std::to_string(recoveredCapture.Digest) + "\",\n"
				+ "  \"recoveredMaterializations\":"
				+ std::to_string(recovered.Preparation.
					AncestorGeometryMaskMaterializationCount) + ",\n"
				+ "  \"recoveredReuses\":"
				+ std::to_string(recovered.Preparation.
					AncestorGeometryMaskReuseCount) + ",\n"
				+ "  \"deviceRecoveryDelta\":1,\n"
				+ "  \"result\":\"PASS\"\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write shared Geometry mask result: "
					+ Convert::UnicodeToUtf8(writeError));
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation shared Geometry member damage stays exact", []
	{
		constexpr size_t TargetCount = 8u;
		BenchmarkScene scene(
			TargetCount, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.ConfigureHostPathGeometryClipForTesting(
			80.0f, 80.0f, 110.0f, 46.0f, 0.0f);
		for (size_t index = 0; index < TargetCount; ++index)
			scene.ConfigureTargetRectangleForTesting(
				index, 12.0f, 12.0f,
				index < 2u ? 8.0f + static_cast<float>(index * 6u)
					: 45.0f + static_cast<float>((index - 2u) % 2u) * 14.0f,
				index < 2u ? 8.0f + static_cast<float>(index * 4u)
					: 6.0f + static_cast<float>((index - 2u) / 2u) * 16.0f);
		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 0.85f, 0.15f, 0.10f, 1.0f });
		scene.SetTargetBackgroundForTesting(
			1u, D2D1_COLOR_F{ 0.15f, 0.45f, 0.85f, 1.0f });
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting(96u, false);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin);
		scene.ForcePresentationUpdateForTesting();
		const auto first = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, first.Preparation.GeometryRasterGroupCount);
		CUI_EXPECT_EQ(TargetCount,
			first.Preparation.GeometryRasterMemberCount);
		CUI_EXPECT_EQ(0ULL, first.GeometryRasterPartialUpdateCount);
		CUI_EXPECT_EQ(1ULL, first.GeometryRasterFullReplayCount);

		scene.SetTargetBackgroundForTesting(
			0u, D2D1_COLOR_F{ 0.10f, 0.80f, 0.20f, 1.0f });
		scene.FlushPendingPresentationUpdateForTesting();
		const auto content = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, content.GeometryRasterPartialUpdateCount);
		CUI_EXPECT_EQ(0ULL, content.GeometryRasterFullReplayCount);
		CUI_EXPECT_TRUE(content.GeometryRasterPartialDamageArea > 0u);
		CUI_EXPECT_TRUE(content.GeometryRasterPartialReplayNodes >= 2u);
		CUI_EXPECT_TRUE(
			content.GeometryRasterPartialReplayNodes < TargetCount);
		CUI_EXPECT_TRUE(content.GeometryRasterPartialSkippedNodes > 0u);
		CUI_EXPECT_EQ(1ULL, content.CommandRecordedNodes);
		UINT contentWidth = 0;
		UINT contentHeight = 0;
		uint64_t contentSurfaceDigest = 0;
		size_t contentOpaquePixels = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, contentWidth, contentHeight,
			contentSurfaceDigest, contentOpaquePixels));
		const auto contentCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!contentCapture.Error.empty())
			throw std::runtime_error(
				"Shared Geometry content-damage capture failed: "
				+ contentCapture.Error);
		RECT contentBlueBounds{};
		size_t contentBluePixels = 0u;
		CUI_EXPECT_TRUE(contentCapture.TryGetColorBounds(
			217u, 115u, 38u,
			contentBlueBounds, contentBluePixels, 12u));

		auto recoverAndCompare = [&](uint64_t expectedSurfaceDigest,
			const CapturedWindowFrame& expectedCapture)
		{
			const auto recoveries =
				scene.PresentationDeviceRecoveryCountForTesting();
			scene.InjectPresentationDeviceLossForTesting();
			scene.ForcePresentationUpdateForTesting();
			CUI_EXPECT_EQ(recoveries + 1u,
				scene.PresentationDeviceRecoveryCountForTesting());
			const auto recoveredFrame = scene.PresentationFrameForTesting();
			CUI_EXPECT_EQ(0ULL,
				recoveredFrame.GeometryRasterPartialUpdateCount);
			CUI_EXPECT_EQ(1ULL,
				recoveredFrame.GeometryRasterFullReplayCount);
			UINT width = 0;
			UINT height = 0;
			uint64_t digest = 0;
			size_t opaque = 0;
			CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
				0u, width, height, digest, opaque));
			CUI_EXPECT_EQ(contentWidth, width);
			CUI_EXPECT_EQ(contentHeight, height);
			CUI_EXPECT_EQ(expectedSurfaceDigest, digest);
			const auto recoveredCapture = CaptureWindowComposition(
				scene.NativeWindowHandleForTesting());
			if (!recoveredCapture.Error.empty())
				throw std::runtime_error(
					"Shared Geometry full-recovery capture failed: "
					+ recoveredCapture.Error);
			CUI_EXPECT_EQ(expectedCapture.Width, recoveredCapture.Width);
			CUI_EXPECT_EQ(expectedCapture.Height, recoveredCapture.Height);
			CUI_EXPECT_EQ(expectedCapture.Bgra, recoveredCapture.Bgra);
		};
		recoverAndCompare(contentSurfaceDigest, contentCapture);

		scene.SetTargetCanvasPositionForTesting(1u, 34.0f, 12.0f);
		scene.FlushPendingPresentationUpdateForTesting();
		const auto moved = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, moved.GeometryRasterPartialUpdateCount);
		CUI_EXPECT_EQ(0ULL, moved.GeometryRasterFullReplayCount);
		CUI_EXPECT_TRUE(moved.GeometryRasterPartialReplayNodes >= 2u);
		CUI_EXPECT_TRUE(moved.GeometryRasterPartialSkippedNodes > 0u);
		UINT movedWidth = 0;
		UINT movedHeight = 0;
		uint64_t movedSurfaceDigest = 0;
		size_t movedOpaquePixels = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, movedWidth, movedHeight,
			movedSurfaceDigest, movedOpaquePixels));
		const auto movedCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!movedCapture.Error.empty())
			throw std::runtime_error(
				"Shared Geometry moved-member capture failed: "
				+ movedCapture.Error);
		RECT movedBlueBounds{};
		size_t movedBluePixels = 0u;
		CUI_EXPECT_TRUE(movedCapture.TryGetColorBounds(
			217u, 115u, 38u,
			movedBlueBounds, movedBluePixels, 12u));
		CUI_EXPECT_TRUE(movedBlueBounds.left > contentBlueBounds.left);
		CUI_EXPECT_TRUE(movedSurfaceDigest != contentSurfaceDigest);
		recoverAndCompare(movedSurfaceDigest, movedCapture);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_GEOMETRY_RASTER_DAMAGE_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query Geometry raster damage output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_GEOMETRY_RASTER_DAMAGE_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read Geometry raster damage output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Geometry raster damage output must be under Workplans.");
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\":1,\n"
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"exact-geometry-member-damage\",\n"
				+ "  \"targetCount\":" + std::to_string(TargetCount) + ",\n"
				+ "  \"contentPartialUpdates\":"
				+ std::to_string(content.GeometryRasterPartialUpdateCount) + ",\n"
				+ "  \"contentDamageArea\":"
				+ std::to_string(content.GeometryRasterPartialDamageArea) + ",\n"
				+ "  \"contentReplayNodes\":"
				+ std::to_string(content.GeometryRasterPartialReplayNodes) + ",\n"
				+ "  \"contentSkippedNodes\":"
				+ std::to_string(content.GeometryRasterPartialSkippedNodes) + ",\n"
				+ "  \"movedPartialUpdates\":"
				+ std::to_string(moved.GeometryRasterPartialUpdateCount) + ",\n"
				+ "  \"movedDamageArea\":"
				+ std::to_string(moved.GeometryRasterPartialDamageArea) + ",\n"
				+ "  \"movedReplayNodes\":"
				+ std::to_string(moved.GeometryRasterPartialReplayNodes) + ",\n"
				+ "  \"movedSkippedNodes\":"
				+ std::to_string(moved.GeometryRasterPartialSkippedNodes) + ",\n"
				+ "  \"contentSurfaceDigest\":\""
				+ std::to_string(contentSurfaceDigest) + "\",\n"
				+ "  \"movedSurfaceDigest\":\""
				+ std::to_string(movedSurfaceDigest) + "\",\n"
				+ "  \"partialMatchesFullRecovery\":true,\n"
				+ "  \"result\":\"PASS\"\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write Geometry raster damage result: "
					+ Convert::UnicodeToUtf8(writeError));
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation shared Geometry damage covers member subtrees", []
	{
		BenchmarkScene scene(2u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.ConfigureHostPathGeometryClipForTesting(
			80.0f, 80.0f, 110.0f, 46.0f, 0.0f);
		scene.ConfigureTargetRectangleForTesting(
			0u, 30.0f, 30.0f, 6.0f, 6.0f);
		scene.ConfigureTargetRectangleForTesting(
			1u, 30.0f, 30.0f, 38.0f, 8.0f);
		auto* firstChild = scene.AddTargetChildRectangleForTesting(
			0u, 12.0f, 12.0f, 8.0f, 8.0f,
			D2D1_COLOR_F{ 0.85f, 0.15f, 0.10f, 1.0f });
		CUI_EXPECT_TRUE(firstChild != nullptr);
		CUI_EXPECT_TRUE(scene.AddTargetChildRectangleForTesting(
			1u, 12.0f, 12.0f, 8.0f, 8.0f,
			D2D1_COLOR_F{ 0.15f, 0.45f, 0.85f, 1.0f }) != nullptr);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting(96u, false);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin);
		scene.ForcePresentationUpdateForTesting();
		const auto first = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, first.Preparation.GeometryRasterGroupCount);
		CUI_EXPECT_EQ(2ULL, first.Preparation.GeometryRasterMemberCount);

		firstChild->Background =
			D2D1_COLOR_F{ 0.10f, 0.80f, 0.20f, 1.0f };
		scene.FlushPendingPresentationUpdateForTesting();
		const auto partial = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, partial.GeometryRasterPartialUpdateCount);
		CUI_EXPECT_EQ(0ULL, partial.GeometryRasterFullReplayCount);
		CUI_EXPECT_EQ(1ULL, partial.CommandRecordedNodes);
		CUI_EXPECT_TRUE(partial.GeometryRasterPartialReplayNodes >= 1u);
		UINT width = 0;
		UINT height = 0;
		uint64_t partialDigest = 0;
		size_t partialOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, partialDigest, partialOpaque));

		scene.InjectPresentationDeviceLossForTesting();
		scene.ForcePresentationUpdateForTesting();
		const auto recovered = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, recovered.GeometryRasterFullReplayCount);
		UINT recoveredWidth = 0;
		UINT recoveredHeight = 0;
		uint64_t recoveredDigest = 0;
		size_t recoveredOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, recoveredWidth, recoveredHeight,
			recoveredDigest, recoveredOpaque));
		CUI_EXPECT_EQ(width, recoveredWidth);
		CUI_EXPECT_EQ(height, recoveredHeight);
		CUI_EXPECT_EQ(partialDigest, recoveredDigest);
		CUI_EXPECT_EQ(partialOpaque, recoveredOpaque);
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation Geometry raster grouping stays fail closed", []
	{
		auto configure = [](BenchmarkScene& scene)
		{
			scene.ConfigureHostPathGeometryClipForTesting(
				60.0f, 60.0f, 110.0f, 46.0f, 20.0f);
			scene.AddNativeCompositionBoundaryForTesting();
			scene.ShowOffscreenWithoutActivationForTesting(96u);
			scene.ConfigureTargetRectangleForTesting(
				0u, 28.0f, 28.0f, 0.0f, 0.0f);
			scene.ConfigureTargetRectangleForTesting(
				1u, 28.0f, 28.0f, 12.0f, 8.0f);
		};

		{
			const std::array propertyKinds{
				BenchmarkPropertyKind::TransformX,
				BenchmarkPropertyKind::Opacity };
			BenchmarkScene scene(
				2u, 0u, BenchmarkPropertyKind::TransformX,
				1u, false, false, false, propertyKinds);
			configure(scene);
			scene.Begin();
			scene.TickRegisteredWindow(BenchmarkClockOrigin + 40u);
			scene.ForcePresentationUpdateForTesting();
			const auto frame = scene.PresentationFrameForTesting();
			CUI_EXPECT_EQ(0ULL, frame.Preparation.GeometryRasterGroupCount);
			CUI_EXPECT_EQ(0ULL, frame.Preparation.GeometryRasterMemberCount);
			CUI_EXPECT_TRUE(frame.AncestorGeometryMaskLayerPushCount >= 2u);
			CUI_EXPECT_EQ(0ULL, frame.GeometryRasterPartialUpdateCount);
			CUI_EXPECT_EQ(0ULL, frame.GeometryRasterFullReplayCount);
			scene.HideOffscreenPresentationForTesting();
		}

		{
			BenchmarkScene scene(2u, 0u, BenchmarkPropertyKind::TransformX);
			scene.AddTargetNativeCompositionBoundaryForTesting(1u);
			configure(scene);
			scene.Begin();
			scene.TickRegisteredWindow(BenchmarkClockOrigin + 40u);
			scene.ForcePresentationUpdateForTesting();
			const auto frame = scene.PresentationFrameForTesting();
			CUI_EXPECT_EQ(0ULL, frame.Preparation.GeometryRasterGroupCount);
			CUI_EXPECT_EQ(0ULL, frame.Preparation.GeometryRasterMemberCount);
			CUI_EXPECT_EQ(1ULL, frame.AncestorGeometryMaskLayerPushCount);
			CUI_EXPECT_EQ(0ULL, frame.GeometryRasterPartialUpdateCount);
			CUI_EXPECT_EQ(0ULL, frame.GeometryRasterFullReplayCount);
			scene.HideOffscreenPresentationForTesting();
		}

		{
			BenchmarkScene scene(2u, 0u, BenchmarkPropertyKind::TransformX);
			scene.WrapTargetInNeutralParentForTesting(1u);
			configure(scene);
			scene.Begin();
			scene.TickRegisteredWindow(BenchmarkClockOrigin + 40u);
			scene.ForcePresentationUpdateForTesting();
			const auto frame = scene.PresentationFrameForTesting();
			CUI_EXPECT_EQ(0ULL, frame.Preparation.GeometryRasterGroupCount);
			CUI_EXPECT_EQ(0ULL, frame.Preparation.GeometryRasterMemberCount);
			CUI_EXPECT_TRUE(frame.AncestorGeometryMaskLayerPushCount >= 2u);
			CUI_EXPECT_EQ(0ULL, frame.GeometryRasterPartialUpdateCount);
			CUI_EXPECT_EQ(0ULL, frame.GeometryRasterFullReplayCount);
			scene.HideOffscreenPresentationForTesting();
		}
	});

	runner.Add("Animation Geometry raster group allocation rollback is atomic", []
	{
		struct AllocationFailureScope final
		{
			~AllocationFailureScope()
			{
				cui::framework::WindowAccess::
					ClearSceneLayerAllocationFailureForTesting();
			}
		} allocationScope;
		BenchmarkScene scene(2u, 0u, BenchmarkPropertyKind::TransformX);
		scene.ConfigureHostPathGeometryClipForTesting(
			60.0f, 60.0f, 110.0f, 46.0f, 20.0f);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting(96u);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 40u);
		const auto committed = scene.PresentationCommittedFrameCount();
		const auto aborted = scene.PresentationAbortedFrameCount();
		cui::framework::WindowAccess::
			FailSceneLayerAllocationAfterForTesting(0u);
		scene.ForcePresentationUpdateForTesting();
		const auto failed = scene.PresentationResourcesForTesting();
		CUI_EXPECT_EQ(1ULL, failed.SceneLayerAllocationFailureCount);
		CUI_EXPECT_EQ(1ULL, failed.SceneLayerCount);
		CUI_EXPECT_EQ(2ULL, failed.SceneLayerSlotCount);
		CUI_EXPECT_TRUE(
			failed.CompositionVisualBatchRollbackCount >= 1u);
		CUI_EXPECT_EQ(0ULL,
			failed.CompositionVisualBatchRollbackFailureCount);
		CUI_EXPECT_EQ(committed,
			scene.PresentationCommittedFrameCount());
		CUI_EXPECT_EQ(aborted,
			scene.PresentationAbortedFrameCount());
		cui::framework::WindowAccess::
			ClearSceneLayerAllocationFailureForTesting();
		scene.ForcePresentationUpdateForTesting();
		const auto recovered = scene.PresentationResourcesForTesting();
		const auto frame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(2ULL, recovered.SceneLayerCount);
		CUI_EXPECT_EQ(2ULL, recovered.SceneLayerSlotCount);
		CUI_EXPECT_EQ(1ULL, recovered.SceneLayerAllocationFailureCount);
		CUI_EXPECT_EQ(1ULL, frame.Preparation.GeometryRasterGroupCount);
		CUI_EXPECT_EQ(2ULL, frame.Preparation.GeometryRasterMemberCount);
		CUI_EXPECT_EQ(1ULL, frame.AncestorGeometryMaskLayerPushCount);
		CUI_EXPECT_EQ(0ULL, frame.GeometryRasterPartialUpdateCount);
		CUI_EXPECT_EQ(1ULL, frame.GeometryRasterFullReplayCount);
		CUI_EXPECT_TRUE(scene.PresentationCommittedFrameCount() > committed);
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation shared Geometry raster group scales as one surface", []
	{
		constexpr size_t TargetCount = 32u;
		constexpr size_t SampleCount = 40u;
		BenchmarkScene scene(
			TargetCount, 0u, BenchmarkPropertyKind::TransformX);
		scene.ConfigureHostPathGeometryClipForTesting(
			80.0f, 80.0f, 10.0f, 10.0f, 0.0f);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting(96u);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin);
		scene.ForcePresentationUpdateForTesting();
		const auto first = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL,
			first.Preparation.AncestorGeometryMaskMaterializationCount);
		CUI_EXPECT_EQ(1ULL, first.Preparation.GeometryRasterGroupCount);
		CUI_EXPECT_EQ(TargetCount,
			first.Preparation.GeometryRasterMemberCount);
		CUI_EXPECT_EQ(1ULL, first.AncestorGeometryMaskLayerPushCount);
		CUI_EXPECT_TRUE(first.SceneSurfacesOpened <= 2u);

		std::vector<double> sceneRenderSamples;
		std::vector<double> surfaceCloseSamples;
		std::vector<double> totalSamples;
		sceneRenderSamples.reserve(SampleCount);
		surfaceCloseSamples.reserve(SampleCount);
		totalSamples.reserve(SampleCount);
		size_t maximumSurfacesOpened = 0u;
		size_t maximumLayerPushes = 0u;
		for (size_t sample = 0; sample < SampleCount; ++sample)
		{
			for (size_t index = 0; index < TargetCount; ++index)
				scene.SetTargetBackgroundForTesting(index,
					sample % 2u == 0u
					? D2D1_COLOR_F{ 0.8f, 0.2f, 0.1f, 1.0f }
					: D2D1_COLOR_F{ 0.1f, 0.3f, 0.8f, 1.0f });
			scene.TickRegisteredWindow(
				BenchmarkClockOrigin + 40u + sample * 16u);
			scene.ForcePresentationUpdateForTesting();
			const auto frame = scene.PresentationFrameForTesting();
			CUI_EXPECT_EQ(0ULL,
				frame.Preparation.AncestorGeometryMaskMaterializationCount);
			CUI_EXPECT_TRUE(
				frame.Preparation.AncestorGeometryMaskReuseCount >= 1u);
			CUI_EXPECT_EQ(1ULL,
				frame.Preparation.GeometryRasterGroupCount);
			CUI_EXPECT_EQ(TargetCount,
				frame.Preparation.GeometryRasterMemberCount);
			CUI_EXPECT_EQ(1ULL,
				frame.AncestorGeometryMaskLayerPushCount);
			CUI_EXPECT_TRUE(frame.SceneSurfacesOpened <= 2u);
			CUI_EXPECT_TRUE(frame.CommandRecordedNodes >= TargetCount);
			maximumSurfacesOpened = (std::max)(
				maximumSurfacesOpened, frame.SceneSurfacesOpened);
			maximumLayerPushes = (std::max)(
				maximumLayerPushes,
				frame.AncestorGeometryMaskLayerPushCount);
			sceneRenderSamples.push_back(
				frame.Timing.SceneRenderMicroseconds);
			surfaceCloseSamples.push_back(
				frame.SceneSurfaceCloseMicroseconds);
			totalSamples.push_back(frame.Timing.TotalMicroseconds);
		}
		const auto sceneRender = Summarize(std::move(sceneRenderSamples));
		const auto surfaceClose = Summarize(std::move(surfaceCloseSamples));
		const auto total = Summarize(std::move(totalSamples));

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_GEOMETRY_RASTER_GROUP_PERFORMANCE_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query Geometry raster group output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_GEOMETRY_RASTER_GROUP_PERFORMANCE_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read Geometry raster group output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Geometry raster group output must be under Workplans.");
			auto distribution = [](const Distribution& value)
			{
				return std::string("{\"count\":")
					+ std::to_string(value.Count)
					+ ",\"meanMicroseconds\":"
					+ FormatDouble(value.MeanMicroseconds)
					+ ",\"p50Microseconds\":"
					+ FormatDouble(value.P50Microseconds)
					+ ",\"p95Microseconds\":"
					+ FormatDouble(value.P95Microseconds)
					+ ",\"maximumMicroseconds\":"
					+ FormatDouble(value.MaximumMicroseconds) + "}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\":1,\n"
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"coalesced-geometry-raster-pressure\",\n"
				+ "  \"targetCount\":" + std::to_string(TargetCount) + ",\n"
				+ "  \"sampleCount\":" + std::to_string(SampleCount) + ",\n"
				+ "  \"geometryRasterGroupCount\":1,\n"
				+ "  \"geometryRasterMemberCount\":"
				+ std::to_string(TargetCount) + ",\n"
				+ "  \"maximumSceneSurfacesOpened\":"
				+ std::to_string(maximumSurfacesOpened) + ",\n"
				+ "  \"maximumGeometryLayerPushes\":"
				+ std::to_string(maximumLayerPushes) + ",\n"
				+ "  \"sceneRender\":" + distribution(sceneRender) + ",\n"
				+ "  \"surfaceClose\":" + distribution(surfaceClose) + ",\n"
				+ "  \"total\":" + distribution(total) + ",\n"
				+ "  \"result\":\"PASS\"\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write Geometry raster group result: "
					+ Convert::UnicodeToUtf8(writeError));
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation shared Geometry sparse member damage stays bounded", []
	{
		constexpr size_t TargetCount = 32u;
		constexpr size_t SampleCount = 40u;
		BenchmarkScene scene(
			TargetCount, 0u, BenchmarkPropertyKind::TransformX);
		scene.ConfigureHostPathGeometryClipForTesting(
			80.0f, 80.0f, 10.0f, 10.0f, 0.0f);
		for (size_t index = 0; index < TargetCount; ++index)
			scene.ConfigureTargetRectangleForTesting(
				index, 7.0f, 7.0f,
				6.0f + static_cast<float>(index % 6u) * 10.0f,
				6.0f + static_cast<float>(index / 6u) * 10.0f);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting(96u, false);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin);
		scene.ForcePresentationUpdateForTesting();
		const auto first = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL, first.Preparation.GeometryRasterGroupCount);
		CUI_EXPECT_EQ(TargetCount,
			first.Preparation.GeometryRasterMemberCount);
		CUI_EXPECT_EQ(1ULL, first.GeometryRasterFullReplayCount);

		std::vector<double> sceneRenderSamples;
		std::vector<double> totalSamples;
		sceneRenderSamples.reserve(SampleCount);
		totalSamples.reserve(SampleCount);
		size_t maximumReplayNodes = 0u;
		size_t minimumSkippedNodes =
			(std::numeric_limits<size_t>::max)();
		uint64_t maximumDamageArea = 0u;
		for (size_t sample = 0; sample < SampleCount; ++sample)
		{
			scene.SetTargetBackgroundForTesting(
				0u, sample % 2u == 0u
					? D2D1_COLOR_F{ 0.8f, 0.2f, 0.1f, 1.0f }
					: D2D1_COLOR_F{ 0.1f, 0.8f, 0.2f, 1.0f });
			scene.FlushPendingPresentationUpdateForTesting();
			const auto frame = scene.PresentationFrameForTesting();
			CUI_EXPECT_EQ(1ULL,
				frame.GeometryRasterPartialUpdateCount);
			CUI_EXPECT_EQ(0ULL, frame.GeometryRasterFullReplayCount);
			CUI_EXPECT_EQ(1ULL, frame.CommandRecordedNodes);
			CUI_EXPECT_EQ(1ULL,
				frame.AncestorGeometryMaskLayerPushCount);
			CUI_EXPECT_TRUE(
				frame.GeometryRasterPartialReplayNodes <= 4u);
			CUI_EXPECT_TRUE(
				frame.GeometryRasterPartialSkippedNodes
					>= TargetCount - 4u);
			maximumReplayNodes = (std::max)(maximumReplayNodes,
				frame.GeometryRasterPartialReplayNodes);
			minimumSkippedNodes = (std::min)(minimumSkippedNodes,
				frame.GeometryRasterPartialSkippedNodes);
			maximumDamageArea = (std::max)(maximumDamageArea,
				frame.GeometryRasterPartialDamageArea);
			sceneRenderSamples.push_back(
				frame.Timing.SceneRenderMicroseconds);
			totalSamples.push_back(frame.Timing.TotalMicroseconds);
		}
		const auto sceneRender = Summarize(std::move(sceneRenderSamples));
		const auto total = Summarize(std::move(totalSamples));

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_GEOMETRY_MEMBER_DAMAGE_PERFORMANCE_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query Geometry member damage performance output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_GEOMETRY_MEMBER_DAMAGE_PERFORMANCE_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read Geometry member damage performance output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Geometry member damage output must be under Workplans.");
			auto distribution = [](const Distribution& value)
			{
				return std::string("{\"count\":")
					+ std::to_string(value.Count)
					+ ",\"meanMicroseconds\":"
					+ FormatDouble(value.MeanMicroseconds)
					+ ",\"p50Microseconds\":"
					+ FormatDouble(value.P50Microseconds)
					+ ",\"p95Microseconds\":"
					+ FormatDouble(value.P95Microseconds)
					+ ",\"maximumMicroseconds\":"
					+ FormatDouble(value.MaximumMicroseconds) + "}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\":1,\n"
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"sparse-geometry-member-damage\",\n"
				+ "  \"targetCount\":" + std::to_string(TargetCount) + ",\n"
				+ "  \"sampleCount\":" + std::to_string(SampleCount) + ",\n"
				+ "  \"partialUpdateCountPerFrame\":1,\n"
				+ "  \"maximumReplayNodes\":"
				+ std::to_string(maximumReplayNodes) + ",\n"
				+ "  \"minimumSkippedNodes\":"
				+ std::to_string(minimumSkippedNodes) + ",\n"
				+ "  \"maximumDamageArea\":"
				+ std::to_string(maximumDamageArea) + ",\n"
				+ "  \"sceneRender\":" + distribution(sceneRender) + ",\n"
				+ "  \"total\":" + distribution(total) + ",\n"
				+ "  \"result\":\"PASS\"\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write Geometry member damage performance: "
					+ Convert::UnicodeToUtf8(writeError));
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation shared Geometry layer management stays exact and bounded", []
	{
		constexpr size_t TargetCount = 32u;
		constexpr size_t SampleCount = 40u;
		BenchmarkScene scene(
			TargetCount, 0u, BenchmarkPropertyKind::Opacity);
		scene.ConfigureHostPathGeometryClipForTesting(
			80.0f, 80.0f, 10.0f, 10.0f, 0.0f);
		for (size_t index = 0; index < TargetCount; ++index)
			scene.ConfigureTargetRectangleForTesting(
				index, 12.0f, 12.0f, 20.0f, 15.0f);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin);
		scene.ForcePresentationUpdateForTesting();
		const auto first = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(1ULL,
			first.Preparation.AncestorGeometryMaskMaterializationCount);
		CUI_EXPECT_TRUE(
			first.Preparation.AncestorGeometryMaskReuseCount
				>= TargetCount - 1u);
		CUI_EXPECT_TRUE(first.SceneSurfacesOpened >= TargetCount);
		CUI_EXPECT_TRUE(
			first.AncestorGeometryMaskLayerPushCount >= TargetCount);
		CUI_EXPECT_EQ(0ULL, first.Preparation.GeometryRasterGroupCount);
		CUI_EXPECT_EQ(0ULL, first.Preparation.GeometryRasterMemberCount);

		std::vector<double> sceneRenderSamples;
		std::vector<double> surfaceCloseSamples;
		std::vector<double> totalSamples;
		size_t minimumLayerPushes =
			(std::numeric_limits<size_t>::max)();
		sceneRenderSamples.reserve(SampleCount);
		surfaceCloseSamples.reserve(SampleCount);
		totalSamples.reserve(SampleCount);
		for (size_t sample = 0; sample < SampleCount; ++sample)
		{
			for (size_t index = 0; index < TargetCount; ++index)
				scene.SetTargetBackgroundForTesting(index,
					sample % 2u == 0u
					? D2D1_COLOR_F{ 0.8f, 0.2f, 0.1f, 1.0f }
					: D2D1_COLOR_F{ 0.1f, 0.3f, 0.8f, 1.0f });
			scene.TickRegisteredWindow(
				BenchmarkClockOrigin + 40u + sample * 16u);
			scene.ForcePresentationUpdateForTesting();
			const auto frame = scene.PresentationFrameForTesting();
			CUI_EXPECT_EQ(0ULL,
				frame.Preparation.AncestorGeometryMaskMaterializationCount);
			CUI_EXPECT_TRUE(
				frame.Preparation.AncestorGeometryMaskReuseCount >= TargetCount);
			CUI_EXPECT_TRUE(frame.SceneSurfacesOpened >= TargetCount);
			CUI_EXPECT_TRUE(frame.CommandRecordedNodes >= TargetCount);
			CUI_EXPECT_TRUE(
				frame.AncestorGeometryMaskLayerPushCount >= TargetCount);
			CUI_EXPECT_EQ(0ULL,
				frame.Preparation.GeometryRasterGroupCount);
			CUI_EXPECT_EQ(0ULL,
				frame.Preparation.GeometryRasterMemberCount);
			minimumLayerPushes = (std::min)(minimumLayerPushes,
				frame.AncestorGeometryMaskLayerPushCount);
			sceneRenderSamples.push_back(
				frame.Timing.SceneRenderMicroseconds);
			surfaceCloseSamples.push_back(
				frame.SceneSurfaceCloseMicroseconds);
			totalSamples.push_back(frame.Timing.TotalMicroseconds);
		}
		const auto sceneRender = Summarize(std::move(sceneRenderSamples));
		const auto surfaceClose = Summarize(std::move(surfaceCloseSamples));
		const auto total = Summarize(std::move(totalSamples));

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_GEOMETRY_LAYER_PERFORMANCE_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query Geometry layer performance output.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_GEOMETRY_LAYER_PERFORMANCE_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read Geometry layer performance output.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Geometry layer output must be under Workplans.");
			auto distribution = [](const Distribution& value)
			{
				return std::string("{\"count\":")
					+ std::to_string(value.Count)
					+ ",\"meanMicroseconds\":"
					+ FormatDouble(value.MeanMicroseconds)
					+ ",\"p50Microseconds\":"
					+ FormatDouble(value.P50Microseconds)
					+ ",\"p95Microseconds\":"
					+ FormatDouble(value.P95Microseconds)
					+ ",\"maximumMicroseconds\":"
					+ FormatDouble(value.MaximumMicroseconds) + "}";
			};
			const std::string json = std::string("{\n")
				+ "  \"schemaVersion\":1,\n"
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"shared-geometry-layer-management\",\n"
				+ "  \"targetCount\":" + std::to_string(TargetCount) + ",\n"
				+ "  \"sampleCount\":" + std::to_string(SampleCount) + ",\n"
				+ "  \"firstMaterializations\":"
				+ std::to_string(first.Preparation.
					AncestorGeometryMaskMaterializationCount) + ",\n"
				+ "  \"firstReuses\":"
				+ std::to_string(first.Preparation.
					AncestorGeometryMaskReuseCount) + ",\n"
				+ "  \"firstLayerPushes\":"
				+ std::to_string(
					first.AncestorGeometryMaskLayerPushCount) + ",\n"
				+ "  \"minimumSteadyLayerPushes\":"
				+ std::to_string(minimumLayerPushes) + ",\n"
				+ "  \"sceneRender\":" + distribution(sceneRender) + ",\n"
				+ "  \"surfaceClose\":" + distribution(surfaceClose) + ",\n"
				+ "  \"total\":" + distribution(total) + ",\n"
				+ "  \"result\":\"PASS\"\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error(
					"Could not write Geometry layer performance result: "
					+ Convert::UnicodeToUtf8(writeError));
		}
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation PathGeometry ancestor clip uses retained mask replay", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.ConfigureHostPathGeometryClipForTesting(
			60.0f, 60.0f, 110.0f, 46.0f, 20.0f);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 120.0f, 120.0f, -30.0f, -30.0f);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin);
		scene.ForcePresentationUpdateForTesting();
		const auto pathMaskPreparationFrame =
			scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(2ULL,
			pathMaskPreparationFrame.Preparation.PhysicalLayerRequiredCount);
		CUI_EXPECT_EQ(0ULL,
			pathMaskPreparationFrame.Preparation.DeferredUnmaterializedCount);
		CUI_EXPECT_EQ(0ULL,
			pathMaskPreparationFrame.Preparation.EarlyViewportDeferredCount);
		UINT width = 0;
		UINT height = 0;
		uint64_t firstSurfaceDigest = 0;
		size_t firstOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, firstSurfaceDigest, firstOpaque));
		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("Retained path mask capture failed: "
				+ firstCapture.Error);
		RECT firstBounds{};
		size_t firstPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u, firstBounds, firstPixels, 12u));

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 40u);
		scene.ForcePresentationUpdateForTesting();
		const auto frame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(0ULL, frame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, frame.CommandRecordedNodes);
		CUI_EXPECT_TRUE(frame.CommandCacheHitNodes >= 1u);
		CUI_EXPECT_TRUE(frame.CommandReplayedNodes >= 1u);
		CUI_EXPECT_TRUE(frame.SceneSurfacesOpened >= 1u);
		uint64_t secondSurfaceDigest = 0;
		size_t secondOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, secondSurfaceDigest, secondOpaque));
		CUI_EXPECT_EQ(firstSurfaceDigest, secondSurfaceDigest);
		CUI_EXPECT_EQ(firstOpaque, secondOpaque);
		auto secondCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!secondCapture.Error.empty())
			throw std::runtime_error("Moved retained path mask capture failed: "
				+ secondCapture.Error);
		RECT secondBounds{};
		size_t secondPixels = 0;
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			217u, 115u, 38u, secondBounds, secondPixels, 12u));
		CUI_EXPECT_EQ(1130ULL, firstPixels);
		CUI_EXPECT_EQ(firstPixels, secondPixels);
		CUI_EXPECT_EQ(129L, firstBounds.left);
		CUI_EXPECT_EQ(68L, firstBounds.top);
		CUI_EXPECT_EQ(173L, firstBounds.right);
		CUI_EXPECT_EQ(122L, firstBounds.bottom);
		CUI_EXPECT_EQ(firstBounds.left, secondBounds.left);
		CUI_EXPECT_EQ(firstBounds.top, secondBounds.top);
		CUI_EXPECT_EQ(firstBounds.right, secondBounds.right);
		CUI_EXPECT_EQ(firstBounds.bottom, secondBounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_RETAINED_PATH_CLIP_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query retained path mask output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_RETAINED_PATH_CLIP_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read retained path mask output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Retained path output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"retained-path-geometry-mask\",\n"
				+ "  \"samples\":[\n"
				+ "    {\"atMilliseconds\":0,\"matchingPixels\":"
				+ std::to_string(firstPixels) + ",\"bounds\":"
				+ boundsJson(firstBounds) + "},\n"
				+ "    {\"atMilliseconds\":40,\"matchingPixels\":"
				+ std::to_string(secondPixels) + ",\"bounds\":"
				+ boundsJson(secondBounds) + "}\n  ],\n"
				+ "  \"surface\":{\"width\":" + std::to_string(width)
				+ ",\"height\":" + std::to_string(height)
				+ ",\"opaquePixels\":" + std::to_string(firstOpaque) + "},\n"
				+ "  \"surfaceDigestStable\":true,\n"
				+ "  \"secondFrameCompositionOnly\":false,\n"
				+ "  \"secondFrameCommandRecordedNodes\":0\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error("Could not write retained path result: "
					+ Convert::UnicodeToUtf8(writeError));
		}

		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation retained PathGeometry mask replays live transform", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.ConfigureHostPathGeometryClipForTesting(
			60.0f, 60.0f, 110.0f, 46.0f, 20.0f);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 24.0f, 24.0f, 10.0f, 15.0f);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin);
		scene.ForcePresentationUpdateForTesting();
		UINT width = 0;
		UINT height = 0;
		uint64_t firstSurfaceDigest = 0;
		size_t firstOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, firstSurfaceDigest, firstOpaque));
		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("Moving path mask capture failed: "
				+ firstCapture.Error);
		RECT firstBounds{};
		size_t firstPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u, firstBounds, firstPixels, 12u));

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 40u);
		scene.ForcePresentationUpdateForTesting();
		const auto frame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(0ULL, frame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, frame.CommandRecordedNodes);
		CUI_EXPECT_TRUE(frame.CommandCacheHitNodes >= 1u);
		CUI_EXPECT_TRUE(frame.CommandReplayedNodes >= 1u);
		uint64_t secondSurfaceDigest = 0;
		size_t secondOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, secondSurfaceDigest, secondOpaque));
		CUI_EXPECT_TRUE(firstSurfaceDigest != secondSurfaceDigest);
		auto secondCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!secondCapture.Error.empty())
			throw std::runtime_error("Moved path mask capture failed: "
				+ secondCapture.Error);
		RECT secondBounds{};
		size_t secondPixels = 0;
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			217u, 115u, 38u, secondBounds, secondPixels, 12u));
		CUI_EXPECT_EQ(301ULL, firstPixels);
		CUI_EXPECT_EQ(131L, firstBounds.left);
		CUI_EXPECT_EQ(81L, firstBounds.top);
		CUI_EXPECT_EQ(148L, firstBounds.right);
		CUI_EXPECT_EQ(109L, firstBounds.bottom);
		CUI_EXPECT_EQ(393ULL, secondPixels);
		CUI_EXPECT_EQ(131L, secondBounds.left);
		CUI_EXPECT_EQ(81L, secondBounds.top);
		CUI_EXPECT_EQ(152L, secondBounds.right);
		CUI_EXPECT_EQ(110L, secondBounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_RETAINED_PATH_MOVING_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query moving path mask output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_RETAINED_PATH_MOVING_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read moving path mask output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"Moving path output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"retained-path-moving-content\",\n"
				+ "  \"samples\":[\n"
				+ "    {\"atMilliseconds\":0,\"matchingPixels\":"
				+ std::to_string(firstPixels) + ",\"bounds\":"
				+ boundsJson(firstBounds) + "},\n"
				+ "    {\"atMilliseconds\":40,\"matchingPixels\":"
				+ std::to_string(secondPixels) + ",\"bounds\":"
				+ boundsJson(secondBounds) + "}\n  ],\n"
				+ "  \"surface\":{\"width\":" + std::to_string(width)
				+ ",\"height\":" + std::to_string(height) + "},\n"
				+ "  \"surfaceDigestChanged\":true,\n"
				+ "  \"secondFrameCommandRecordedNodes\":0\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error("Could not write moving path result: "
					+ Convert::UnicodeToUtf8(writeError));
		}
		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation GeometryGroup ancestor clip uses retained mask replay", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.AcquireSceneLayerPixelReadbackLeaseForTesting();
		scene.ConfigureHostGeometryGroupClipForTesting(
			60.0f, 60.0f, 110.0f, 46.0f, 20.0f);
		scene.AddNativeCompositionBoundaryForTesting();
		scene.ShowOffscreenWithoutActivationForTesting();
		scene.ConfigureTargetRectangleForTesting(
			0u, 120.0f, 120.0f, -30.0f, -30.0f);
		scene.Begin();
		scene.TickRegisteredWindow(BenchmarkClockOrigin);
		scene.ForcePresentationUpdateForTesting();
		UINT width = 0;
		UINT height = 0;
		uint64_t firstSurfaceDigest = 0;
		size_t firstOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, firstSurfaceDigest, firstOpaque));
		auto firstCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!firstCapture.Error.empty())
			throw std::runtime_error("GeometryGroup mask capture failed: "
				+ firstCapture.Error);
		RECT firstBounds{};
		size_t firstPixels = 0;
		CUI_EXPECT_TRUE(firstCapture.TryGetColorBounds(
			217u, 115u, 38u, firstBounds, firstPixels, 12u));

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 40u);
		scene.ForcePresentationUpdateForTesting();
		const auto frame = scene.PresentationFrameForTesting();
		CUI_EXPECT_EQ(0ULL, frame.CompositionOnlySegments);
		CUI_EXPECT_EQ(0ULL, frame.CommandRecordedNodes);
		CUI_EXPECT_TRUE(frame.CommandCacheHitNodes >= 1u);
		CUI_EXPECT_TRUE(frame.CommandReplayedNodes >= 1u);
		uint64_t secondSurfaceDigest = 0;
		size_t secondOpaque = 0;
		CUI_EXPECT_TRUE(scene.TryGetTargetSceneLayerPixelDigestForTesting(
			0u, width, height, secondSurfaceDigest, secondOpaque));
		CUI_EXPECT_EQ(firstSurfaceDigest, secondSurfaceDigest);
		CUI_EXPECT_EQ(firstOpaque, secondOpaque);
		auto secondCapture = CaptureWindowComposition(
			scene.NativeWindowHandleForTesting());
		if (!secondCapture.Error.empty())
			throw std::runtime_error("Moved GeometryGroup mask capture failed: "
				+ secondCapture.Error);
		RECT secondBounds{};
		size_t secondPixels = 0;
		CUI_EXPECT_TRUE(secondCapture.TryGetColorBounds(
			217u, 115u, 38u, secondBounds, secondPixels, 12u));
		CUI_EXPECT_EQ(2001ULL, firstPixels);
		CUI_EXPECT_EQ(firstPixels, secondPixels);
		CUI_EXPECT_EQ(112L, firstBounds.left);
		CUI_EXPECT_EQ(68L, firstBounds.top);
		CUI_EXPECT_EQ(175L, firstBounds.right);
		CUI_EXPECT_EQ(130L, firstBounds.bottom);
		CUI_EXPECT_EQ(firstBounds.left, secondBounds.left);
		CUI_EXPECT_EQ(firstBounds.top, secondBounds.top);
		CUI_EXPECT_EQ(firstBounds.right, secondBounds.right);
		CUI_EXPECT_EQ(firstBounds.bottom, secondBounds.bottom);

		size_t outputLength = 0;
		if (::_wgetenv_s(&outputLength, nullptr, 0,
			L"CUI_RETAINED_GROUP_CLIP_PIXEL_OUTPUT") != 0)
			throw std::runtime_error(
				"Could not query GeometryGroup mask output path.");
		std::vector<wchar_t> outputBuffer(outputLength);
		if (outputLength > 1u)
		{
			if (::_wgetenv_s(&outputLength, outputBuffer.data(),
				outputBuffer.size(),
				L"CUI_RETAINED_GROUP_CLIP_PIXEL_OUTPUT") != 0)
				throw std::runtime_error(
					"Could not read GeometryGroup mask output path.");
			const std::filesystem::path outputPath(outputBuffer.data());
			if (!IsUnderWorkplanRoot(outputPath))
				throw std::runtime_error(
					"GeometryGroup output must be under Workplans.");
			auto boundsJson = [](const RECT& value)
			{
				return std::string("{\"left\":")
					+ std::to_string(value.left)
					+ ",\"top\":" + std::to_string(value.top)
					+ ",\"right\":" + std::to_string(value.right)
					+ ",\"bottom\":" + std::to_string(value.bottom) + "}";
			};
			const std::string json =
				std::string("{\n  \"schemaVersion\":1,\n")
				+ "  \"engine\":\"CUI\",\n"
				+ "  \"scenario\":\"retained-geometry-group-mask\",\n"
				+ "  \"samples\":[\n"
				+ "    {\"atMilliseconds\":0,\"matchingPixels\":"
				+ std::to_string(firstPixels) + ",\"bounds\":"
				+ boundsJson(firstBounds) + "},\n"
				+ "    {\"atMilliseconds\":40,\"matchingPixels\":"
				+ std::to_string(secondPixels) + ",\"bounds\":"
				+ boundsJson(secondBounds) + "}\n  ],\n"
				+ "  \"surface\":{\"width\":" + std::to_string(width)
				+ ",\"height\":" + std::to_string(height)
				+ ",\"opaquePixels\":" + std::to_string(firstOpaque) + "},\n"
				+ "  \"surfaceDigestStable\":true,\n"
				+ "  \"secondFrameCommandRecordedNodes\":0\n}\n";
			std::wstring writeError;
			if (!DesignerModel::AtomicFile::Write(
				outputPath.wstring(), json, &writeError))
				throw std::runtime_error("Could not write GeometryGroup result: "
					+ Convert::UnicodeToUtf8(writeError));
		}
		scene.Remove();
		scene.ForcePresentationUpdateForTesting();
		scene.HideOffscreenPresentationForTesting();
	});

	runner.Add("Animation benchmark metrics use nearest-rank percentiles", []
	{
		const auto summary = Summarize({ 5.0, 1.0, 4.0, 2.0, 3.0 });
		CUI_EXPECT_EQ(5ULL, summary.Count);
		CUI_EXPECT_NEAR(1.0, summary.MinimumMicroseconds, 1e-12);
		CUI_EXPECT_NEAR(3.0, summary.MeanMicroseconds, 1e-12);
		CUI_EXPECT_NEAR(3.0, summary.P50Microseconds, 1e-12);
		CUI_EXPECT_NEAR(5.0, summary.P95Microseconds, 1e-12);
		CUI_EXPECT_NEAR(5.0, summary.P99Microseconds, 1e-12);
	});

	runner.Add("Animation benchmark Begin replace Remove owns exact leaves", []
	{
		BenchmarkScene scene(
			10u, 25u, BenchmarkPropertyKind::TransformX);
		CUI_EXPECT_EQ(0ULL, scene.ActiveLeafCount());
		scene.Begin();
		CUI_EXPECT_EQ(10ULL, scene.ActiveLeafCount());
		scene.Advance(BenchmarkClockOrigin + 16u);
		scene.Restart();
		CUI_EXPECT_EQ(10ULL, scene.ActiveLeafCount());
		scene.Stop();
		CUI_EXPECT_EQ(0ULL, scene.ActiveLeafCount());
		CUI_EXPECT_TRUE(scene.AnimationSlotsCleared());

		const std::vector<std::wstring_view> unrelated{ L"--not-animation" };
		CUI_EXPECT_FALSE(ParseArguments(unrelated).Requested);
		const std::vector<std::wstring_view> requested{
			L"--animation-benchmark",
			L"--animation-benchmark-output",
			L"CUI-Workplans/WPF-Animation-Alignment/smoke.json",
			L"--animation-benchmark-profile", L"smoke" };
		const auto parsed = ParseArguments(requested);
		CUI_EXPECT_TRUE(parsed.Requested);
		CUI_EXPECT_TRUE(parsed.Options.has_value());
		CUI_EXPECT_EQ(std::string("smoke"), parsed.Options->Profile);
		CUI_EXPECT_TRUE(IsUnderWorkplanRoot(
			parsed.Options->OutputPath));
		CUI_EXPECT_FALSE(IsUnderWorkplanRoot(
			L"AnimationConformance/benchmark.json"));
	});

	runner.Add("Animation benchmark Stop retains controllable clock", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.Begin();
		scene.Advance(BenchmarkClockOrigin + 400u);
		scene.StopRetained();
		CUI_EXPECT_EQ(1ULL, scene.ActiveLeafCount());
		CUI_EXPECT_EQ(1ULL, scene.RootClockCount());
		CUI_EXPECT_EQ(2ULL, scene.ClockNodeCount());
		CUI_EXPECT_EQ(1ULL, scene.LayerStackCount());
		CUI_EXPECT_EQ(1ULL, scene.LayerCount());
		CUI_EXPECT_FALSE(scene.AnimationSlotsCleared());
		scene.Remove();
		CUI_EXPECT_EQ(0ULL, scene.RootClockCount());
		CUI_EXPECT_TRUE(scene.AnimationSlotsCleared());
	});

	runner.Add("Animation benchmark compiled Seek commits on tick", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.Begin();
		scene.Advance(BenchmarkClockOrigin + 100u);
		scene.Seek(BenchmarkClockOrigin + 101u);
		CUI_EXPECT_EQ(1ULL, scene.RootClockCount());
		CUI_EXPECT_TRUE(scene.ClockIdentityValid());
		scene.Remove();
	});

	runner.Add("Animation benchmark compiled SetSpeedRatio commits on tick", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.Begin();
		scene.Advance(BenchmarkClockOrigin + 100u);
		scene.SetSpeedRatio(BenchmarkClockOrigin + 100u);
		scene.Advance(BenchmarkClockOrigin + 150u);
		scene.Seek(BenchmarkClockOrigin + 151u);
		CUI_EXPECT_EQ(1ULL, scene.RootClockCount());
		CUI_EXPECT_TRUE(scene.ClockIdentityValid());
		scene.Remove();
	});

	runner.Add("Animation benchmark compiled SkipToFill commits on tick", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX,
			1u, false, false, true);
		scene.Begin();
		scene.Advance(BenchmarkClockOrigin + 400u);
		scene.SkipToFill(BenchmarkClockOrigin + 401u);
		CUI_EXPECT_EQ(1ULL, scene.RootClockCount());
		CUI_EXPECT_TRUE(scene.ClockIdentityValid());
		scene.Remove();
	});

	runner.Add("Animation benchmark ClockNode ranges own multiple roots", []
	{
		BenchmarkScene scene(
			6u, 0u, BenchmarkPropertyKind::TransformX, 2u);
		std::vector<DeclarativeClockTimingEventArgs> timingEvents;
		auto timingConnection = scene.HostForTesting()->
			OnStoryboardTimingEvent.Subscribe(
				[&](Control*, const DeclarativeClockTimingEventArgs& args)
				{ timingEvents.push_back(args); });
		scene.Begin();
		CUI_EXPECT_EQ(2ULL, scene.RootClockCount());
		CUI_EXPECT_EQ(8ULL, scene.ClockNodeCount());
		CUI_EXPECT_EQ(6ULL, scene.LayerStackCount());
		CUI_EXPECT_EQ(6ULL, scene.LayerCount());
		CUI_EXPECT_EQ(1ULL, scene.LayerMaxDepth());
		CUI_EXPECT_EQ(3ULL, scene.RootClockChildCount(0u));
		CUI_EXPECT_EQ(3ULL, scene.RootClockChildCount(1u));
		CUI_EXPECT_TRUE(scene.ClockIdentityValid());
		scene.Advance(BenchmarkClockOrigin + 16u);
		CUI_EXPECT_EQ(6ULL, timingEvents.size());
		for (size_t rootIndex = 0; rootIndex < 2u; ++rootIndex)
		{
			const auto offset = rootIndex * 3u;
			CUI_EXPECT_TRUE(timingEvents[offset].Kind
				== DeclarativeClockTimingEventKind::CurrentTimeInvalidated);
			CUI_EXPECT_TRUE(timingEvents[offset + 1u].Kind
				== DeclarativeClockTimingEventKind::CurrentGlobalSpeedInvalidated);
			CUI_EXPECT_TRUE(timingEvents[offset + 2u].Kind
				== DeclarativeClockTimingEventKind::CurrentStateInvalidated);
			CUI_EXPECT_TRUE(timingEvents[offset].ClockInstanceId != 0u);
			CUI_EXPECT_TRUE(timingEvents[offset].OwnerKind
				== DeclarativeClockOwnerKind::CompiledInteractionStoryboard);
			CUI_EXPECT_EQ(timingEvents[offset].ClockInstanceId,
				timingEvents[offset + 1u].ClockInstanceId);
			CUI_EXPECT_EQ(timingEvents[offset].ClockInstanceId,
				timingEvents[offset + 2u].ClockInstanceId);
		}
		CUI_EXPECT_TRUE(timingEvents[0].ClockInstanceId
			!= timingEvents[3].ClockInstanceId);
		timingEvents.clear();
		scene.Pause(BenchmarkClockOrigin + 20u);
		CUI_EXPECT_EQ(4ULL, timingEvents.size());
		for (size_t rootIndex = 0; rootIndex < 2u; ++rootIndex)
		{
			const auto offset = rootIndex * 2u;
			CUI_EXPECT_TRUE(timingEvents[offset].Kind
				== DeclarativeClockTimingEventKind::CurrentTimeInvalidated);
			CUI_EXPECT_TRUE(timingEvents[offset + 1u].Kind
				== DeclarativeClockTimingEventKind::CurrentGlobalSpeedInvalidated);
			CUI_EXPECT_EQ(timingEvents[offset].ClockInstanceId,
				timingEvents[offset + 1u].ClockInstanceId);
		}
		CUI_EXPECT_EQ(3ULL, scene.RootClockChildCount(0u));
		CUI_EXPECT_EQ(3ULL, scene.RootClockChildCount(1u));
		scene.Resume(BenchmarkClockOrigin + 120u);
		scene.Advance(BenchmarkClockOrigin + 136u);
		scene.Begin();
		CUI_EXPECT_EQ(3ULL, scene.RootClockChildCount(0u));
		CUI_EXPECT_EQ(3ULL, scene.RootClockChildCount(1u));
		CUI_EXPECT_EQ(6ULL, scene.LayerStackCount());
		CUI_EXPECT_EQ(6ULL, scene.LayerCount());
		CUI_EXPECT_EQ(1ULL, scene.LayerMaxDepth());
		CUI_EXPECT_TRUE(scene.ClockIdentityValid());
		scene.Stop();
		CUI_EXPECT_EQ(0ULL, scene.ClockNodeCount());
		CUI_EXPECT_EQ(0ULL, scene.LayerStackCount());
		CUI_EXPECT_EQ(0ULL, scene.LayerCount());
		CUI_EXPECT_TRUE(scene.AnimationSlotsCleared());
	});

	runner.Add("Animation timing event callback queues reentrant Remove", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		std::vector<DeclarativeClockTimingEventArgs> timingEvents;
		bool removeAccepted = false;
		auto timingConnection = scene.HostForTesting()->
			OnStoryboardTimingEvent.Subscribe(
				[&](Control* sender, const DeclarativeClockTimingEventArgs& args)
				{
					timingEvents.push_back(args);
					if (!removeAccepted && args.Kind
						== DeclarativeClockTimingEventKind::CurrentTimeInvalidated)
						removeAccepted = sender->RaiseDeclarativeEvent(
							BenchmarkRemoveEvent());
				});
		scene.Begin();
		scene.Advance(BenchmarkClockOrigin + 16u);
		CUI_EXPECT_TRUE(removeAccepted);
		CUI_EXPECT_EQ(3ULL, timingEvents.size());
		CUI_EXPECT_TRUE(timingEvents[0].Kind
			== DeclarativeClockTimingEventKind::CurrentTimeInvalidated);
		CUI_EXPECT_TRUE(timingEvents[1].Kind
			== DeclarativeClockTimingEventKind::CurrentGlobalSpeedInvalidated);
		CUI_EXPECT_TRUE(timingEvents[2].Kind
			== DeclarativeClockTimingEventKind::CurrentStateInvalidated);
		CUI_EXPECT_EQ(1ULL, scene.RootClockCount());
		timingEvents.clear();
		scene.Advance(BenchmarkClockOrigin + 17u);
		CUI_EXPECT_EQ(5ULL, timingEvents.size());
		CUI_EXPECT_TRUE(timingEvents[0].Kind
			== DeclarativeClockTimingEventKind::CurrentTimeInvalidated);
		CUI_EXPECT_TRUE(timingEvents[1].Kind
			== DeclarativeClockTimingEventKind::CurrentGlobalSpeedInvalidated);
		CUI_EXPECT_TRUE(timingEvents[2].Kind
			== DeclarativeClockTimingEventKind::CurrentStateInvalidated);
		CUI_EXPECT_TRUE(timingEvents[3].Kind
			== DeclarativeClockTimingEventKind::Completed);
		CUI_EXPECT_TRUE(timingEvents[4].Kind
			== DeclarativeClockTimingEventKind::RemoveRequested);
		CUI_EXPECT_EQ(0ULL, scene.RootClockCount());
	});

	runner.Add("Animation timing event callback may destroy its owner", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		size_t eventCount = 0;
		bool ownerDestroyed = false;
		auto timingConnection = scene.HostForTesting()->
			OnStoryboardTimingEvent.Subscribe(
				[&](Control*, const DeclarativeClockTimingEventArgs&)
				{
					++eventCount;
					if (ownerDestroyed) return;
					auto owner = scene.DetachHostForTesting();
					ownerDestroyed = owner != nullptr;
					owner.reset();
				});
		scene.Begin();
		CUI_EXPECT_TRUE(scene.AdvanceAllowingOwnerDestruction(
			BenchmarkClockOrigin + 16u));
		CUI_EXPECT_TRUE(ownerDestroyed);
		// The weak publication loop must stop before Speed/State callbacks.
		CUI_EXPECT_EQ(1ULL, eventCount);
	});

	runner.Add("Animation failed frame commit publishes no timing events", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		std::vector<DeclarativeClockTimingEventArgs> timingEvents;
		auto* host = scene.HostForTesting();
		auto timingConnection = host->OnStoryboardTimingEvent.Subscribe(
			[&](Control*, const DeclarativeClockTimingEventArgs& args)
			{ timingEvents.push_back(args); });
		scene.Begin();
		cui::framework::PresentationAccess::
			FailNextVisualStateAnimationFrameCommitForTesting(*host);
		CUI_EXPECT_FALSE(cui::framework::PresentationAccess::
			AdvanceVisualStateAnimations(*host, BenchmarkClockOrigin + 16u));
		CUI_EXPECT_TRUE(cui::framework::PresentationAccess::
			VisualStateAnimationAdvanceFailedForTesting(*host));
		CUI_EXPECT_TRUE(timingEvents.empty());
		CUI_EXPECT_EQ(1ULL, scene.RootClockCount());
		CUI_EXPECT_TRUE(scene.ClockIdentityValid());
		scene.Advance(BenchmarkClockOrigin + 17u);
		CUI_EXPECT_EQ(3ULL, timingEvents.size());
		CUI_EXPECT_TRUE(timingEvents[0].Kind
			== DeclarativeClockTimingEventKind::CurrentTimeInvalidated);
		CUI_EXPECT_TRUE(timingEvents[1].Kind
			== DeclarativeClockTimingEventKind::CurrentGlobalSpeedInvalidated);
		CUI_EXPECT_TRUE(timingEvents[2].Kind
			== DeclarativeClockTimingEventKind::CurrentStateInvalidated);
		scene.Remove();
	});

	runner.Add("Animation benchmark exact DP layer replaces overlapping roots", []
	{
		// Six independently controllable Storyboards target the same exact
		// object path. SnapshotAndReplace leaves only the last root/layer alive.
		BenchmarkScene scene(
			6u, 0u, BenchmarkPropertyKind::TransformX, 6u, true);
		scene.Begin();
		CUI_EXPECT_EQ(1ULL, scene.RootClockCount());
		CUI_EXPECT_EQ(2ULL, scene.ClockNodeCount());
		CUI_EXPECT_EQ(1ULL, scene.LayerStackCount());
		CUI_EXPECT_EQ(1ULL, scene.LayerCount());
		CUI_EXPECT_EQ(1ULL, scene.LayerMaxDepth());
		CUI_EXPECT_TRUE(scene.ClockIdentityValid());
		scene.Advance(BenchmarkClockOrigin + 16u);
		scene.Pause(BenchmarkClockOrigin + 20u);
		scene.Resume(BenchmarkClockOrigin + 120u);
		scene.Advance(BenchmarkClockOrigin + 136u);
		scene.Begin();
		CUI_EXPECT_EQ(1ULL, scene.RootClockCount());
		CUI_EXPECT_EQ(1ULL, scene.LayerStackCount());
		CUI_EXPECT_EQ(1ULL, scene.LayerCount());
		CUI_EXPECT_EQ(1ULL, scene.LayerMaxDepth());
		CUI_EXPECT_TRUE(scene.ClockIdentityValid());
		scene.Stop();
		CUI_EXPECT_EQ(0ULL, scene.ClockNodeCount());
		CUI_EXPECT_EQ(0ULL, scene.LayerStackCount());
		CUI_EXPECT_EQ(0ULL, scene.LayerCount());
		CUI_EXPECT_TRUE(scene.AnimationSlotsCleared());
	});

	runner.Add("Animation benchmark compiled Compose retains exact DP roots", []
	{
		BenchmarkScene scene(
			6u, 0u, BenchmarkPropertyKind::TransformX, 6u, true, true);
		scene.Begin();
		CUI_EXPECT_EQ(6ULL, scene.RootClockCount());
		CUI_EXPECT_EQ(12ULL, scene.ClockNodeCount());
		CUI_EXPECT_EQ(1ULL, scene.LayerStackCount());
		CUI_EXPECT_EQ(6ULL, scene.LayerCount());
		CUI_EXPECT_EQ(6ULL, scene.LayerMaxDepth());
		CUI_EXPECT_TRUE(scene.ClockIdentityValid());
		scene.Advance(BenchmarkClockOrigin + 250u);
		scene.Remove();
		CUI_EXPECT_EQ(0ULL, scene.RootClockCount());
		CUI_EXPECT_EQ(0ULL, scene.LayerCount());
		CUI_EXPECT_TRUE(scene.AnimationSlotsCleared());
	});

	runner.Add("Animation benchmark layer replacement stays bounded at 10000", []
	{
		BenchmarkScene scene(
			1u, 0u, BenchmarkPropertyKind::TransformX);
		scene.Begin();
		scene.ReplaceMany(10'000u);
		CUI_EXPECT_EQ(1ULL, scene.ActiveLeafCount());
		CUI_EXPECT_EQ(1ULL, scene.RootClockCount());
		CUI_EXPECT_EQ(2ULL, scene.ClockNodeCount());
		CUI_EXPECT_EQ(1ULL, scene.LayerStackCount());
		CUI_EXPECT_EQ(1ULL, scene.LayerCount());
		CUI_EXPECT_EQ(1ULL, scene.LayerMaxDepth());
		CUI_EXPECT_TRUE(scene.ClockIdentityValid());
		scene.Stop();
		CUI_EXPECT_EQ(0ULL, scene.ActiveLeafCount());
		CUI_EXPECT_EQ(0ULL, scene.LayerCount());
		CUI_EXPECT_TRUE(scene.AnimationSlotsCleared());
	});

	runner.Add("Animation benchmark Window registry tracks presentation lifecycle", []
	{
		BenchmarkScene scene(3u, 4u, BenchmarkPropertyKind::TransformX);
		scene.Begin();
		CUI_EXPECT_EQ(1ULL, scene.RegisteredControlCount());
		scene.TickRegisteredWindow(BenchmarkClockOrigin + 8u);
		scene.TickWindow();

		scene.OpenHostAsTransientForTesting();
		scene.TickWindow();
		CUI_EXPECT_EQ(1ULL, scene.RegisteredControlCount());
		CUI_EXPECT_FALSE(scene.RegistryDegraded());
		scene.CloseHostAsTransientForTesting();

		auto detached = scene.DetachHostForTesting();
		CUI_EXPECT_EQ(0ULL, scene.RegisteredControlCount());

		Window alternateWindow;
		auto alternateRootOwner = std::make_unique<Canvas>();
		auto* alternateRoot = static_cast<Canvas*>(
			alternateWindow.AddOwned(std::move(alternateRootOwner)));
		CUI_EXPECT_TRUE(alternateRoot != nullptr);
		auto* host = scene.HostForTesting();
		CUI_EXPECT_EQ(host, alternateRoot->AddOwned(std::move(detached)));
		CUI_EXPECT_EQ(1ULL,
			cui::framework::WindowAccess::
				RegisteredDeclarativeAnimationControlCountForTesting(
					alternateWindow));
		cui::framework::WindowAccess::TickPresentationAnimationsForTesting(
			alternateWindow);
		CUI_EXPECT_FALSE(cui::framework::WindowAccess::
			AnimationRegistryDegradedForTesting(alternateWindow));

		detached = alternateRoot->DetachVisualChild(host);
		CUI_EXPECT_TRUE(detached.get() == host);
		CUI_EXPECT_EQ(0ULL,
			cui::framework::WindowAccess::
				RegisteredDeclarativeAnimationControlCountForTesting(
					alternateWindow));
		scene.ReattachHostForTesting(std::move(detached));
		CUI_EXPECT_EQ(1ULL, scene.RegisteredControlCount());
		scene.TickWindow();
		CUI_EXPECT_FALSE(scene.RegistryDegraded());
		scene.Stop();
		CUI_EXPECT_EQ(0ULL, scene.RegisteredControlCount());
	});

	runner.Add("Animation Window timer follows root and presentation lifecycle", []
	{
		BenchmarkScene scene(1u, 0u, BenchmarkPropertyKind::TransformX);
		CUI_EXPECT_EQ(0u, scene.AnimationTimerInterval());
		CUI_EXPECT_FALSE(scene.AnimationFrameSchedulerRunning());
		CUI_EXPECT_FALSE(scene.AnimationUsesLegacyTimer());

		scene.Begin();
		CUI_EXPECT_EQ(16u, scene.AnimationTimerInterval());
		CUI_EXPECT_TRUE(scene.AnimationFrameSchedulerRunning());
		CUI_EXPECT_FALSE(scene.AnimationUsesLegacyTimer());
		scene.Pause(BenchmarkClockOrigin + 20u);
		CUI_EXPECT_EQ(0u, scene.AnimationTimerInterval());
		CUI_EXPECT_FALSE(scene.AnimationFrameSchedulerRunning());
		CUI_EXPECT_FALSE(scene.AnimationUsesLegacyTimer());
		scene.Resume(BenchmarkClockOrigin + 120u);
		CUI_EXPECT_EQ(16u, scene.AnimationTimerInterval());
		CUI_EXPECT_TRUE(scene.AnimationFrameSchedulerRunning());
		CUI_EXPECT_FALSE(scene.AnimationUsesLegacyTimer());

		auto detached = scene.DetachHostForTesting();
		CUI_EXPECT_EQ(0ULL, scene.RegisteredControlCount());
		CUI_EXPECT_EQ(0u, scene.AnimationTimerInterval());
		CUI_EXPECT_FALSE(scene.AnimationFrameSchedulerRunning());
		scene.ReattachHostForTesting(std::move(detached));
		CUI_EXPECT_EQ(1ULL, scene.RegisteredControlCount());
		CUI_EXPECT_EQ(16u, scene.AnimationTimerInterval());
		CUI_EXPECT_TRUE(scene.AnimationFrameSchedulerRunning());
		CUI_EXPECT_FALSE(scene.AnimationUsesLegacyTimer());
		scene.SetWindowEnabledForTesting(false);
		CUI_EXPECT_EQ(0u, scene.AnimationTimerInterval());
		CUI_EXPECT_FALSE(scene.AnimationFrameSchedulerRunning());
		scene.SetWindowEnabledForTesting(true);
		CUI_EXPECT_EQ(16u, scene.AnimationTimerInterval());
		CUI_EXPECT_TRUE(scene.AnimationFrameSchedulerRunning());
		CUI_EXPECT_FALSE(scene.AnimationUsesLegacyTimer());

		scene.Stop();
		CUI_EXPECT_EQ(0ULL, scene.RegisteredControlCount());
		CUI_EXPECT_EQ(0u, scene.AnimationTimerInterval());
		CUI_EXPECT_FALSE(scene.AnimationFrameSchedulerRunning());

		scene.Begin();
		CUI_EXPECT_EQ(16u, scene.AnimationTimerInterval());
		CUI_EXPECT_TRUE(scene.AnimationFrameSchedulerRunning());
		CUI_EXPECT_FALSE(scene.AnimationUsesLegacyTimer());
		scene.CloseWindowForTesting();
		CUI_EXPECT_EQ(0ULL, scene.RegisteredControlCount());
		CUI_EXPECT_EQ(0u, scene.AnimationTimerInterval());
		CUI_EXPECT_FALSE(scene.AnimationFrameSchedulerRunning());
		CUI_EXPECT_FALSE(scene.AnimationUsesLegacyTimer());
		CUI_EXPECT_FALSE(scene.RegistryDegraded());
	});

	runner.Add("Animation Window ticks preserve exact presentation invalidation lanes", []
	{
		BenchmarkScene scene(3u, 0u, BenchmarkPropertyKind::TransformX);
		const auto hostBeforeBegin = scene.HostPresentationRevisions();
		scene.Begin();
		const auto hostBeforeTick = scene.HostPresentationRevisions();
		CUI_EXPECT_EQ(hostBeforeBegin.Content, hostBeforeTick.Content);
		CUI_EXPECT_EQ(hostBeforeBegin.Geometry, hostBeforeTick.Geometry);
		CUI_EXPECT_EQ(hostBeforeBegin.Composition, hostBeforeTick.Composition);
		std::vector<PresentationRevisionSnapshot> beforeTargets;
		beforeTargets.reserve(scene.TargetCountForTesting());
		for (size_t index = 0; index < scene.TargetCountForTesting(); ++index)
			beforeTargets.push_back(scene.TargetPresentationRevisions(index));

		scene.TickRegisteredWindow(BenchmarkClockOrigin + 16u);
		const auto hostAfterTick = scene.HostPresentationRevisions();
		CUI_EXPECT_EQ(hostBeforeTick.Content, hostAfterTick.Content);
		CUI_EXPECT_EQ(hostBeforeTick.Geometry, hostAfterTick.Geometry);
		CUI_EXPECT_EQ(hostBeforeTick.Composition, hostAfterTick.Composition);
		for (size_t index = 0; index < beforeTargets.size(); ++index)
		{
			const auto after = scene.TargetPresentationRevisions(index);
			CUI_EXPECT_EQ(beforeTargets[index].Content, after.Content);
			CUI_EXPECT_EQ(beforeTargets[index].Geometry + 1u, after.Geometry);
			CUI_EXPECT_EQ(beforeTargets[index].Composition, after.Composition);
		}

		BenchmarkScene composed(
			6u, 0u, BenchmarkPropertyKind::TransformX, 6u, true, true);
		composed.Begin();
		const auto composedHostBefore = composed.HostPresentationRevisions();
		const auto composedTargetBefore =
			composed.TargetPresentationRevisions(0);
		composed.TickRegisteredWindow(BenchmarkClockOrigin + 16u);
		const auto composedHostAfter = composed.HostPresentationRevisions();
		const auto composedTargetAfter =
			composed.TargetPresentationRevisions(0);
		CUI_EXPECT_EQ(composedHostBefore.Content, composedHostAfter.Content);
		CUI_EXPECT_EQ(composedHostBefore.Geometry, composedHostAfter.Geometry);
		CUI_EXPECT_EQ(composedHostBefore.Composition,
			composedHostAfter.Composition);
		CUI_EXPECT_EQ(composedTargetBefore.Content,
			composedTargetAfter.Content);
		CUI_EXPECT_EQ(composedTargetBefore.Geometry + 1u,
			composedTargetAfter.Geometry);
		CUI_EXPECT_EQ(composedTargetBefore.Composition,
			composedTargetAfter.Composition);
	});

	runner.Add("Animation native registry tracks loading and caret lifecycles", []
	{
		Window window;
		auto rootOwner = std::make_unique<Canvas>();
		auto* root = static_cast<Canvas*>(
			window.AddOwned(std::move(rootOwner)));
		CUI_EXPECT_TRUE(root != nullptr);
		auto loadingOwner = std::make_unique<LoadingRing>();
		auto* loading = static_cast<LoadingRing*>(
			root->AddOwned(std::move(loadingOwner)));
		CUI_EXPECT_TRUE(loading != nullptr);
		CUI_EXPECT_EQ(1ULL, cui::framework::WindowAccess::
			RegisteredNativeAnimationControlCountForTesting(window));
		CUI_EXPECT_EQ(16u, cui::framework::WindowAccess::
			AnimationTimerIntervalForTesting(window));

		loading->IsActive = false;
		CUI_EXPECT_EQ(0ULL, cui::framework::WindowAccess::
			RegisteredNativeAnimationControlCountForTesting(window));
		CUI_EXPECT_EQ(0u, cui::framework::WindowAccess::
			AnimationTimerIntervalForTesting(window));

		auto caretOwner = std::make_unique<CaretAnimationProbe>();
		auto* caret = static_cast<CaretAnimationProbe*>(
			root->AddOwned(std::move(caretOwner)));
		CUI_EXPECT_TRUE(caret != nullptr);
		caret->SetCaretState(true, 0, 0, true);
		CUI_EXPECT_EQ(1ULL, cui::framework::WindowAccess::
			RegisteredNativeAnimationControlCountForTesting(window));
		CUI_EXPECT_EQ(caret->IsAnimationRunning() ? 1ULL : 0ULL,
			cui::framework::WindowAccess::
				ActiveNativeAnimationControlCountForTesting(window));
		const UINT caretBlinkTime = ::GetCaretBlinkTime();
		const UINT expectedCaretInterval = caret->IsAnimationRunning()
			? caretBlinkTime : 0u;
		CUI_EXPECT_EQ(expectedCaretInterval,
			cui::framework::WindowAccess::
				AnimationTimerIntervalForTesting(window));
		caret->SetCaretState(true, 0, 1, true);
		CUI_EXPECT_EQ(0ULL, cui::framework::WindowAccess::
			RegisteredNativeAnimationControlCountForTesting(window));

		loading->IsActive = true;
		CUI_EXPECT_EQ(1ULL, cui::framework::WindowAccess::
			RegisteredNativeAnimationControlCountForTesting(window));
		CUI_EXPECT_EQ(16u, cui::framework::WindowAccess::
			AnimationTimerIntervalForTesting(window));
		auto preferences = window.GetSystemVisualPreferences();
		preferences.AnimationsEnabled = false;
		cui::framework::WindowAccess::ApplySystemVisualPreferences(
			window, preferences);
		CUI_EXPECT_EQ(1ULL, cui::framework::WindowAccess::
			RegisteredNativeAnimationControlCountForTesting(window));
		CUI_EXPECT_EQ(loading->IsAnimationRunning() ? 1ULL : 0ULL,
			cui::framework::WindowAccess::
				ActiveNativeAnimationControlCountForTesting(window));
		CUI_EXPECT_EQ(loading->IsAnimationRunning() ? 16u : 0u,
			cui::framework::WindowAccess::
				AnimationTimerIntervalForTesting(window));

		auto detached = root->DetachVisualChild(loading);
		CUI_EXPECT_TRUE(detached.get() == loading);
		CUI_EXPECT_EQ(0ULL, cui::framework::WindowAccess::
			RegisteredNativeAnimationControlCountForTesting(window));
		CUI_EXPECT_EQ(0u, cui::framework::WindowAccess::
			AnimationTimerIntervalForTesting(window));

		auto trapOwner = std::make_unique<NativeAnimationQueryTrap>();
		auto* trap = static_cast<NativeAnimationQueryTrap*>(
			root->AddOwned(std::move(trapOwner)));
		CUI_EXPECT_TRUE(trap != nullptr);
		trap->Queries = 0;
		cui::framework::WindowAccess::TickPresentationAnimationsForTesting(
			window);
		CUI_EXPECT_EQ(0, trap->Queries);
		CUI_EXPECT_FALSE(cui::framework::WindowAccess::
			AnimationRegistryDegradedForTesting(window));

		auto retainedProbeOwner =
			std::make_unique<RetainedNativeAnimationQueryProbe>();
		auto* retainedProbe =
			static_cast<RetainedNativeAnimationQueryProbe*>(
				root->AddOwned(std::move(retainedProbeOwner)));
		CUI_EXPECT_TRUE(retainedProbe != nullptr);
		CUI_EXPECT_EQ(1ULL, cui::framework::WindowAccess::
			RegisteredNativeAnimationControlCountForTesting(window));
		retainedProbe->ActiveQueries = 0;
		retainedProbe->RetainedQueries = 0;
		auto ordinaryOwner = std::make_unique<Canvas>();
		auto* ordinary = root->AddOwned(std::move(ordinaryOwner));
		CUI_EXPECT_TRUE(ordinary != nullptr);
		CUI_EXPECT_EQ(0, retainedProbe->ActiveQueries);
		CUI_EXPECT_EQ(0, retainedProbe->RetainedQueries);
		auto detachedOrdinary = root->DetachVisualChild(ordinary);
		CUI_EXPECT_TRUE(detachedOrdinary.get() == ordinary);
		CUI_EXPECT_EQ(0, retainedProbe->ActiveQueries);
		CUI_EXPECT_EQ(0, retainedProbe->RetainedQueries);
	});
}
