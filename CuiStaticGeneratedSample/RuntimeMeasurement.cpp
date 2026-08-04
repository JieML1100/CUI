#include "NamespacedWindow.g.h"

#include <Application.h>
#include <CompiledBindingRecord.h>
#include <CuiBuildFeatures.h>
#include <Style.h>

#include <Windows.h>
#include <Psapi.h>

#include <atomic>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <malloc.h>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>

namespace
{
	struct AllocationSnapshot final
	{
		std::uint64_t Calls = 0;
		std::uint64_t RequestedBytes = 0;
	};

	constinit std::atomic<std::uint64_t> allocationCalls{ 0 };
	constinit std::atomic<std::uint64_t> allocationRequestedBytes{ 0 };

	void RecordAllocation(std::size_t size) noexcept
	{
		allocationCalls.fetch_add(1, std::memory_order_relaxed);
		allocationRequestedBytes.fetch_add(
			static_cast<std::uint64_t>(size), std::memory_order_relaxed);
	}

	void* AllocateUnaligned(std::size_t size)
	{
		if (size == 0) size = 1;
		for (;;)
		{
			if (void* result = std::malloc(size))
			{
				RecordAllocation(size);
				return result;
			}
			const auto handler = std::get_new_handler();
			if (!handler) throw std::bad_alloc();
			handler();
		}
	}

	void* AllocateAligned(std::size_t size, std::size_t alignment)
	{
		if (size == 0) size = 1;
		for (;;)
		{
			if (void* result = _aligned_malloc(size, alignment))
			{
				RecordAllocation(size);
				return result;
			}
			const auto handler = std::get_new_handler();
			if (!handler) throw std::bad_alloc();
			handler();
		}
	}

	AllocationSnapshot CaptureAllocations() noexcept
	{
		return {
			allocationCalls.load(std::memory_order_relaxed),
			allocationRequestedBytes.load(std::memory_order_relaxed)
		};
	}

	struct MemorySnapshot final
	{
		std::uint64_t WorkingSetBytes = 0;
		std::uint64_t PeakWorkingSetBytes = 0;
		std::uint64_t PrivateUsageBytes = 0;
		bool Valid = false;
	};

	MemorySnapshot CaptureMemory() noexcept
	{
		PROCESS_MEMORY_COUNTERS_EX counters{};
		counters.cb = sizeof(counters);
		if (!::GetProcessMemoryInfo(
			::GetCurrentProcess(),
			reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
			static_cast<DWORD>(sizeof(counters)))) return {};
		return {
			static_cast<std::uint64_t>(counters.WorkingSetSize),
			static_cast<std::uint64_t>(counters.PeakWorkingSetSize),
			static_cast<std::uint64_t>(counters.PrivateUsage),
			true
		};
	}

	std::uint64_t FileTimeTicks(const FILETIME& value) noexcept
	{
		ULARGE_INTEGER ticks{};
		ticks.LowPart = value.dwLowDateTime;
		ticks.HighPart = value.dwHighDateTime;
		return ticks.QuadPart;
	}

	struct Marker final
	{
		LARGE_INTEGER Tick{};
		FILETIME WallTime{};
		AllocationSnapshot Allocations{};
		MemorySnapshot Memory{};
	};

	Marker CaptureMarker() noexcept
	{
		Marker result;
		(void)::QueryPerformanceCounter(&result.Tick);
		::GetSystemTimePreciseAsFileTime(&result.WallTime);
		result.Allocations = CaptureAllocations();
		result.Memory = CaptureMemory();
		return result;
	}

	struct RuntimeMetrics final
	{
		LARGE_INTEGER Frequency{};
		LARGE_INTEGER EntryTick{};
		FILETIME ProcessCreationTime{};
		AllocationSnapshot EntryAllocations{};
		MemorySnapshot EntryMemory{};
		Marker ComponentReady{};
		Marker FirstFrame{};
		bool ClockValid = false;
		bool ProcessCreationTimeValid = false;
		bool FirstFrameReached = false;
	};

	double ElapsedMicroseconds(
		const RuntimeMetrics& metrics,
		const LARGE_INTEGER& end) noexcept
	{
		if (!metrics.ClockValid || end.QuadPart < metrics.EntryTick.QuadPart)
			return 0.0;
		const auto ticks = static_cast<long double>(
			end.QuadPart - metrics.EntryTick.QuadPart);
		return static_cast<double>(
			ticks * 1000000.0L
			/ static_cast<long double>(metrics.Frequency.QuadPart));
	}

	double ProcessToFirstFrameMicroseconds(
		const RuntimeMetrics& metrics) noexcept
	{
		if (!metrics.ProcessCreationTimeValid) return 0.0;
		const auto creation = FileTimeTicks(metrics.ProcessCreationTime);
		const auto firstFrame = FileTimeTicks(metrics.FirstFrame.WallTime);
		if (firstFrame < creation) return 0.0;
		return static_cast<double>(firstFrame - creation) / 10.0;
	}

	std::uint64_t Delta(
		std::uint64_t end,
		std::uint64_t begin) noexcept
	{
		return end >= begin ? end - begin : 0;
	}

#if CUI_RUNTIME_MEASUREMENT_THEME_FULL
	constexpr const char* themeMode = "Full";
#else
	constexpr const char* themeMode = "Closure";
#endif

	constexpr std::uint64_t endpointWarmupIterations = 10000;
	constexpr std::uint64_t endpointIterationsPerSample = 200000;
	constexpr size_t endpointSampleCount = 7;

	class EndpointBenchmarkRecord : public CompiledBindingRecord
	{
	public:
		EndpointBenchmarkRecord()
			: CompiledBindingRecord(Properties()) {}

		void ResetCallbackCounts() noexcept
		{
			ReadCallbackCalls = 0;
			WriteCallbackCalls = 0;
		}

		static BindingSourcePropertyToken ValueToken() noexcept
		{
			static const auto value =
				MakeBindingSourcePropertyToken(L"Value");
			return value;
		}

		mutable std::uint64_t ReadCallbackCalls = 0;
		std::uint64_t WriteCallbackCalls = 0;

	private:
		static std::span<const CompiledBindingRecordProperty> Properties()
		{
			static const std::array<CompiledBindingRecordProperty, 1> values{{
				{
					ValueToken(),
					BindingValueKind::Int,
					std::type_index(typeid(int)),
					true, true, true,
					+[](const CompiledBindingRecord& source, BindingValue& out)
					{
						auto& typed = static_cast<
							const EndpointBenchmarkRecord&>(source);
						++typed.ReadCallbackCalls;
						out = BindingValue(typed._value);
						return true;
					},
					+[](CompiledBindingRecord& source, const BindingValue& value)
					{
						auto& typed = static_cast<EndpointBenchmarkRecord&>(source);
						++typed.WriteCallbackCalls;
						int next = 0;
						if (!value.TryGet(next))
							return CompiledBindingRecordWriteResult::Failed;
						if (typed._value == next)
							return CompiledBindingRecordWriteResult::Unchanged;
						typed._value = next;
						return CompiledBindingRecordWriteResult::Changed;
					}
				}
			}};
			return values;
		}

		int _value = 17;
	};

	struct EndpointBenchmarkRun final
	{
		double ElapsedMicroseconds = 0.0;
		std::uint64_t Checksum = 0;
		bool Succeeded = false;
	};

	struct EndpointBenchmarkMetrics final
	{
		double DirectReadMedianMicroseconds = 0.0;
		double TokenAdapterReadMedianMicroseconds = 0.0;
		double DirectWriteMedianMicroseconds = 0.0;
		double TokenAdapterWriteMedianMicroseconds = 0.0;
		std::uint64_t DirectReadCallbackCalls = 0;
		std::uint64_t TokenAdapterReadCallbackCalls = 0;
		std::uint64_t DirectWriteCallbackCalls = 0;
		std::uint64_t TokenAdapterWriteCallbackCalls = 0;
		std::uint64_t ReadChecksum = 0;
	};

	double BenchmarkElapsedMicroseconds(
		const LARGE_INTEGER& frequency,
		const LARGE_INTEGER& begin,
		const LARGE_INTEGER& end) noexcept
	{
		if (frequency.QuadPart <= 0 || end.QuadPart < begin.QuadPart)
			return 0.0;
		return static_cast<double>(
			static_cast<long double>(end.QuadPart - begin.QuadPart)
			* 1000000.0L / static_cast<long double>(frequency.QuadPart));
	}

	EndpointBenchmarkRun RunDirectReadBenchmark(
		CompiledSourceHandle source,
		std::uint64_t iterations,
		const LARGE_INTEGER& frequency)
	{
		BindingValue value;
		std::uint64_t checksum = 0;
		bool succeeded = source && source.Ops->Read;
		LARGE_INTEGER begin{};
		LARGE_INTEGER end{};
		(void)::QueryPerformanceCounter(&begin);
		for (std::uint64_t index = 0; succeeded && index < iterations; ++index)
		{
			int current = 0;
			succeeded = source.Ops->Read(source, value)
				&& value.TryGet(current);
			checksum += static_cast<std::uint64_t>(current);
		}
		(void)::QueryPerformanceCounter(&end);
		return {
			BenchmarkElapsedMicroseconds(frequency, begin, end),
			checksum,
			succeeded
		};
	}

	EndpointBenchmarkRun RunTokenAdapterReadBenchmark(
		EndpointBenchmarkRecord& record,
		CompiledBindingPathView path,
		std::uint64_t iterations,
		const LARGE_INTEGER& frequency)
	{
		BindingValue value;
		std::uint64_t checksum = 0;
		bool succeeded = true;
		const IBindingSource& source = record;
		LARGE_INTEGER begin{};
		LARGE_INTEGER end{};
		(void)::QueryPerformanceCounter(&begin);
		for (std::uint64_t index = 0; succeeded && index < iterations; ++index)
		{
			int current = 0;
			succeeded = TryGetBindingPathValue(source, path, value)
				&& value.TryGet(current);
			checksum += static_cast<std::uint64_t>(current);
		}
		(void)::QueryPerformanceCounter(&end);
		return {
			BenchmarkElapsedMicroseconds(frequency, begin, end),
			checksum,
			succeeded
		};
	}

	EndpointBenchmarkRun RunDirectWriteBenchmark(
		CompiledSourceHandle source,
		std::uint64_t iterations,
		const LARGE_INTEGER& frequency)
	{
		bool succeeded = source && source.Ops->Write;
		LARGE_INTEGER begin{};
		LARGE_INTEGER end{};
		(void)::QueryPerformanceCounter(&begin);
		for (std::uint64_t index = 0; succeeded && index < iterations; ++index)
			succeeded = source.Ops->Write(
				source, BindingValue(static_cast<int>(index & 1)));
		(void)::QueryPerformanceCounter(&end);
		return {
			BenchmarkElapsedMicroseconds(frequency, begin, end),
			0,
			succeeded
		};
	}

	__declspec(noinline) bool WriteThroughTokenAdapter(
		IBindingSource& source,
		BindingSourcePropertyToken property,
		const BindingValue& value)
	{
		return source.TrySetValue(property, value);
	}

	EndpointBenchmarkRun RunTokenAdapterWriteBenchmark(
		EndpointBenchmarkRecord& record,
		BindingSourcePropertyToken property,
		std::uint64_t iterations,
		const LARGE_INTEGER& frequency)
	{
		bool succeeded = true;
		IBindingSource& source = record;
		LARGE_INTEGER begin{};
		LARGE_INTEGER end{};
		(void)::QueryPerformanceCounter(&begin);
		for (std::uint64_t index = 0; succeeded && index < iterations; ++index)
			succeeded = WriteThroughTokenAdapter(
				source, property, BindingValue(static_cast<int>(index & 1)));
		(void)::QueryPerformanceCounter(&end);
		return {
			BenchmarkElapsedMicroseconds(frequency, begin, end),
			0,
			succeeded
		};
	}

	template<size_t Size>
	double Median(std::array<double, Size> values)
	{
		std::sort(values.begin(), values.end());
		return values[Size / 2];
	}

	EndpointBenchmarkMetrics RunEndpointBenchmark(
		const LARGE_INTEGER& frequency)
	{
		EndpointBenchmarkRecord directRecord;
		EndpointBenchmarkRecord tokenAdapterRecord;
		const auto directSource = directRecord.MakeCompiledPropertySource(0);
		const auto property = EndpointBenchmarkRecord::ValueToken();
		const CompiledBindingPathStep pathStep{
			CompiledBindingPathStepKind::Property,
			CompiledBindingPathCapabilities::Read
				| CompiledBindingPathCapabilities::Write
				| CompiledBindingPathCapabilities::Observe,
			BindingValueKind::Int,
			property,
			0
		};
		const CompiledBindingPathView tokenAdapterPath{
			std::span{ &pathStep, size_t{ 1 } }
		};

		if (!RunDirectReadBenchmark(
			directSource, endpointWarmupIterations, frequency).Succeeded
			|| !RunTokenAdapterReadBenchmark(
				tokenAdapterRecord, tokenAdapterPath,
				endpointWarmupIterations, frequency).Succeeded
			|| !RunDirectWriteBenchmark(
				directSource, endpointWarmupIterations, frequency).Succeeded
			|| !RunTokenAdapterWriteBenchmark(
				tokenAdapterRecord, property,
				endpointWarmupIterations, frequency).Succeeded)
			throw std::runtime_error(
				"Compiled record endpoint benchmark warmup failed");

		directRecord.ResetCallbackCounts();
		tokenAdapterRecord.ResetCallbackCounts();
		std::array<double, endpointSampleCount> directReadSamples{};
		std::array<double, endpointSampleCount> tokenAdapterReadSamples{};
		std::array<double, endpointSampleCount> directWriteSamples{};
		std::array<double, endpointSampleCount> tokenAdapterWriteSamples{};
		std::uint64_t directChecksum = 0;
		std::uint64_t tokenAdapterChecksum = 0;

		for (size_t sample = 0; sample < endpointSampleCount; ++sample)
		{
			EndpointBenchmarkRun directRead;
			EndpointBenchmarkRun tokenAdapterRead;
			EndpointBenchmarkRun directWrite;
			EndpointBenchmarkRun tokenAdapterWrite;
			if ((sample & 1) == 0)
			{
				directRead = RunDirectReadBenchmark(
					directSource,
					endpointIterationsPerSample, frequency);
				tokenAdapterRead = RunTokenAdapterReadBenchmark(
					tokenAdapterRecord, tokenAdapterPath,
					endpointIterationsPerSample, frequency);
				directWrite = RunDirectWriteBenchmark(
					directSource,
					endpointIterationsPerSample, frequency);
				tokenAdapterWrite = RunTokenAdapterWriteBenchmark(
					tokenAdapterRecord, property,
					endpointIterationsPerSample, frequency);
			}
			else
			{
				tokenAdapterRead = RunTokenAdapterReadBenchmark(
					tokenAdapterRecord, tokenAdapterPath,
					endpointIterationsPerSample, frequency);
				directRead = RunDirectReadBenchmark(
					directSource,
					endpointIterationsPerSample, frequency);
				tokenAdapterWrite = RunTokenAdapterWriteBenchmark(
					tokenAdapterRecord, property,
					endpointIterationsPerSample, frequency);
				directWrite = RunDirectWriteBenchmark(
					directSource,
					endpointIterationsPerSample, frequency);
			}
			if (!directRead.Succeeded || !tokenAdapterRead.Succeeded
				|| !directWrite.Succeeded || !tokenAdapterWrite.Succeeded)
				throw std::runtime_error(
					"Compiled record endpoint benchmark operation failed");
			directReadSamples[sample] = directRead.ElapsedMicroseconds;
			tokenAdapterReadSamples[sample] =
				tokenAdapterRead.ElapsedMicroseconds;
			directWriteSamples[sample] = directWrite.ElapsedMicroseconds;
			tokenAdapterWriteSamples[sample] =
				tokenAdapterWrite.ElapsedMicroseconds;
			directChecksum += directRead.Checksum;
			tokenAdapterChecksum += tokenAdapterRead.Checksum;
		}

		const auto expectedCallbackCalls =
			endpointIterationsPerSample * endpointSampleCount;
		if (directRecord.ReadCallbackCalls != expectedCallbackCalls
			|| tokenAdapterRecord.ReadCallbackCalls != expectedCallbackCalls
			|| directRecord.WriteCallbackCalls != expectedCallbackCalls
			|| tokenAdapterRecord.WriteCallbackCalls != expectedCallbackCalls
			|| directChecksum != tokenAdapterChecksum)
			throw std::runtime_error(
				"Compiled record endpoint benchmark count mismatch");

		return {
			Median(directReadSamples),
			Median(tokenAdapterReadSamples),
			Median(directWriteSamples),
			Median(tokenAdapterWriteSamples),
			directRecord.ReadCallbackCalls,
			tokenAdapterRecord.ReadCallbackCalls,
			directRecord.WriteCallbackCalls,
			tokenAdapterRecord.WriteCallbackCalls,
			directChecksum
		};
	}

	class MeasurementWindow final
		: public Acme::Views::MainWindowGenerated
	{
	public:
		MeasurementWindow(Application& application, RuntimeMetrics& metrics)
			: _application(application), _metrics(metrics)
		{
			InitializeComponent();
		}

	private:
		void HandleWindowContentRendered(Window*) override
		{
			if (_metrics.FirstFrameReached) return;
			_metrics.FirstFrame = CaptureMarker();
			_metrics.FirstFrameReached = true;
			_application.Shutdown();
		}

		void HandleStaticRefreshCanExecute(
			Control*, CanExecuteRoutedEventArgs& e) override
		{
			e.CanExecute = true;
		}

		void HandleStaticRefreshExecuted(
			Control*, ExecutedRoutedEventArgs& e) override
		{
			e.Executed = true;
		}

		void HandleNamespacedClick(Control*, RoutedEventArgs&) override {}
		void HandleNamespacedDrop(Control*, DragEventArgs&) override {}

		Application& _application;
		RuntimeMetrics& _metrics;
	};

	void WriteMetrics(
		const RuntimeMetrics& metrics,
		const EndpointBenchmarkMetrics& endpointBenchmark)
	{
		const auto componentCalls = Delta(
			metrics.ComponentReady.Allocations.Calls,
			metrics.EntryAllocations.Calls);
		const auto componentBytes = Delta(
			metrics.ComponentReady.Allocations.RequestedBytes,
			metrics.EntryAllocations.RequestedBytes);
		const auto firstFrameCalls = Delta(
			metrics.FirstFrame.Allocations.Calls,
			metrics.EntryAllocations.Calls);
		const auto firstFrameBytes = Delta(
			metrics.FirstFrame.Allocations.RequestedBytes,
			metrics.EntryAllocations.RequestedBytes);

		std::cout << std::fixed << std::setprecision(3)
			<< "{\"schema_version\":1"
			<< ",\"theme_mode\":\"" << themeMode << "\""
			<< ",\"dynamic_xaml\":" << CUI_ENABLE_DYNAMIC_XAML
			<< ",\"timing_scope\":\"global_operator_new_instrumented\""
			<< ",\"allocation_scope\":\"global_operator_new_only\""
			<< ",\"process_create_to_first_frame_us\":"
			<< ProcessToFirstFrameMicroseconds(metrics)
			<< ",\"entry_to_component_ready_us\":"
			<< ElapsedMicroseconds(metrics, metrics.ComponentReady.Tick)
			<< ",\"entry_to_first_frame_us\":"
			<< ElapsedMicroseconds(metrics, metrics.FirstFrame.Tick)
			<< ",\"pre_main_cpp_new_calls\":"
			<< metrics.EntryAllocations.Calls
			<< ",\"pre_main_cpp_new_requested_bytes\":"
			<< metrics.EntryAllocations.RequestedBytes
			<< ",\"entry_to_component_ready_cpp_new_calls\":"
			<< componentCalls
			<< ",\"entry_to_component_ready_cpp_new_requested_bytes\":"
			<< componentBytes
			<< ",\"entry_to_first_frame_cpp_new_calls\":"
			<< firstFrameCalls
			<< ",\"entry_to_first_frame_cpp_new_requested_bytes\":"
			<< firstFrameBytes
			<< ",\"first_frame_working_set_bytes\":"
			<< metrics.FirstFrame.Memory.WorkingSetBytes
			<< ",\"first_frame_peak_working_set_bytes\":"
			<< metrics.FirstFrame.Memory.PeakWorkingSetBytes
			<< ",\"first_frame_private_usage_bytes\":"
			<< metrics.FirstFrame.Memory.PrivateUsageBytes
			<< ",\"component_ready_working_set_bytes\":"
			<< metrics.ComponentReady.Memory.WorkingSetBytes
			<< ",\"compiled_record_endpoint_benchmark\":{"
			<< "\"warmup_iterations\":" << endpointWarmupIterations
			<< ",\"iterations_per_sample\":"
			<< endpointIterationsPerSample
			<< ",\"sample_count\":" << endpointSampleCount
			<< ",\"direct_read_median_us\":"
			<< endpointBenchmark.DirectReadMedianMicroseconds
			<< ",\"token_adapter_read_median_us\":"
			<< endpointBenchmark.TokenAdapterReadMedianMicroseconds
			<< ",\"direct_write_median_us\":"
			<< endpointBenchmark.DirectWriteMedianMicroseconds
			<< ",\"token_adapter_write_median_us\":"
			<< endpointBenchmark.TokenAdapterWriteMedianMicroseconds
			<< ",\"direct_read_callback_calls\":"
			<< endpointBenchmark.DirectReadCallbackCalls
			<< ",\"token_adapter_read_callback_calls\":"
			<< endpointBenchmark.TokenAdapterReadCallbackCalls
			<< ",\"direct_write_callback_calls\":"
			<< endpointBenchmark.DirectWriteCallbackCalls
			<< ",\"token_adapter_write_callback_calls\":"
			<< endpointBenchmark.TokenAdapterWriteCallbackCalls
			<< ",\"read_checksum\":" << endpointBenchmark.ReadChecksum
			<< "}"
			<< ",\"object_sizes\":{"
			<< "\"BindingValue\":" << sizeof(BindingValue)
			<< ",\"CompiledSourceHandle\":" << sizeof(CompiledSourceHandle)
			<< ",\"CompiledSourceOps\":" << sizeof(CompiledSourceOps)
			<< ",\"CompiledBindingRecord\":"
			<< sizeof(CompiledBindingRecord)
			<< ",\"CompiledBindingRecordProperty\":"
			<< sizeof(CompiledBindingRecordProperty)
			<< ",\"Binding\":" << sizeof(Binding)
			<< ",\"MultiBindingSource\":" << sizeof(MultiBindingSource)
			<< ",\"DependencyPropertyReference\":"
			<< sizeof(DependencyPropertyReference)
			<< ",\"DependencyObject\":" << sizeof(DependencyObject)
			<< ",\"UIElement\":" << sizeof(UIElement)
			<< ",\"Control\":" << sizeof(Control)
			<< ",\"Button\":" << sizeof(Button)
			<< ",\"Window\":" << sizeof(Window)
			<< ",\"ControlStyleSheet\":" << sizeof(ControlStyleSheet)
			<< ",\"MainWindowGenerated\":"
			<< sizeof(Acme::Views::MainWindowGenerated)
			<< "}}\n";
	}
}

void* operator new(std::size_t size)
{
	return AllocateUnaligned(size);
}

void* operator new[](std::size_t size)
{
	return AllocateUnaligned(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
	try { return AllocateUnaligned(size); }
	catch (...) { return nullptr; }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
	try { return AllocateUnaligned(size); }
	catch (...) { return nullptr; }
}

void operator delete(void* value) noexcept
{
	std::free(value);
}

void operator delete[](void* value) noexcept
{
	std::free(value);
}

void operator delete(void* value, std::size_t) noexcept
{
	std::free(value);
}

void operator delete[](void* value, std::size_t) noexcept
{
	std::free(value);
}

void* operator new(std::size_t size, std::align_val_t alignment)
{
	return AllocateAligned(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment)
{
	return AllocateAligned(size, static_cast<std::size_t>(alignment));
}

void* operator new(
	std::size_t size,
	std::align_val_t alignment,
	const std::nothrow_t&) noexcept
{
	try
	{
		return AllocateAligned(size, static_cast<std::size_t>(alignment));
	}
	catch (...)
	{
		return nullptr;
	}
}

void* operator new[](
	std::size_t size,
	std::align_val_t alignment,
	const std::nothrow_t&) noexcept
{
	try
	{
		return AllocateAligned(size, static_cast<std::size_t>(alignment));
	}
	catch (...)
	{
		return nullptr;
	}
}

void operator delete(void* value, std::align_val_t) noexcept
{
	_aligned_free(value);
}

void operator delete[](void* value, std::align_val_t) noexcept
{
	_aligned_free(value);
}

void operator delete(
	void* value,
	std::size_t,
	std::align_val_t) noexcept
{
	_aligned_free(value);
}

void operator delete[](
	void* value,
	std::size_t,
	std::align_val_t) noexcept
{
	_aligned_free(value);
}

int wmain()
{
	RuntimeMetrics metrics;
	metrics.ClockValid = ::QueryPerformanceFrequency(&metrics.Frequency) != FALSE
		&& metrics.Frequency.QuadPart > 0
		&& ::QueryPerformanceCounter(&metrics.EntryTick) != FALSE;
	metrics.EntryAllocations = CaptureAllocations();
	metrics.EntryMemory = CaptureMemory();
	FILETIME exitTime{};
	FILETIME kernelTime{};
	FILETIME userTime{};
	metrics.ProcessCreationTimeValid = ::GetProcessTimes(
		::GetCurrentProcess(),
		&metrics.ProcessCreationTime,
		&exitTime,
		&kernelTime,
		&userTime) != FALSE;

	if (!metrics.ClockValid || !metrics.ProcessCreationTimeValid
		|| !metrics.EntryMemory.Valid)
	{
		std::cerr << "CUI runtime measurement could not initialize counters.\n";
		return 2;
	}

	try
	{
		Application application;
		MeasurementWindow window(application, metrics);
		auto dataContext = std::make_shared<ObservableObject>();
		dataContext->SetValue(
			Acme::Views::MainWindowGenerated::DataContextProperties::Caption,
			std::wstring(L"Runtime measurement"));
		if (!window.BindData(BindingSourceReference(dataContext)))
		{
			std::cerr << "CUI runtime measurement binding failed.\n";
			return 3;
		}
		metrics.ComponentReady = CaptureMarker();
		if (!metrics.ComponentReady.Memory.Valid)
		{
			std::cerr << "CUI runtime measurement could not sample memory.\n";
			return 4;
		}

		const int result = application.Run(window);
		if (result != 0 || !metrics.FirstFrameReached
			|| !metrics.FirstFrame.Memory.Valid)
		{
			std::cerr << "CUI runtime measurement did not reach the first committed frame.\n";
			return result != 0 ? result : 5;
		}
		const auto endpointBenchmark =
			RunEndpointBenchmark(metrics.Frequency);
		WriteMetrics(metrics, endpointBenchmark);
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "CUI runtime measurement failed: " << error.what() << '\n';
		return 6;
	}
	catch (...)
	{
		std::cerr << "CUI runtime measurement failed with an unknown exception.\n";
		return 7;
	}
}
