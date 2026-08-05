#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

enum class MediaPerformanceParseState
{
	NotRequested,
	Ready,
	Invalid,
};

enum class MediaPerformanceVideoPath
{
	Auto,
	Cpu,
	GpuRequired,
};

struct MediaPerformanceOptions final
{
	std::filesystem::path MediaPath;
	double Rate = 1.0;
	double DurationSeconds = 10.0;
	double InjectPresentationDeviceLossAtSeconds = 0.0;
	double InjectSharedDeviceRotationAtSeconds = 0.0;
	std::filesystem::path PerfJsonPath;
	MediaPerformanceVideoPath VideoPath = MediaPerformanceVideoPath::Auto;
	bool RequireAudio = false;
	std::uint32_t ExpectedVideoWidth = 0;
	std::uint32_t ExpectedVideoHeight = 0;
	double ExpectedVideoFramesPerSecond = 0.0;
};

struct MediaPerformanceCommandLine final
{
	MediaPerformanceParseState State = MediaPerformanceParseState::NotRequested;
	MediaPerformanceOptions Options;
	std::wstring Error;
};

/** Parses the deterministic CUITest MediaPlayer performance command line. */
MediaPerformanceCommandLine ParseMediaPerformanceCommandLine();

/**
 * Runs a visible MediaPlayer presentation for a fixed wall-clock duration.
 * Returns zero on a completed measurement and a non-zero automation exit code
 * on load, playback, timeout or output failure.
 */
int RunMediaPerformance(
	const MediaPerformanceOptions& options,
	std::wstring* error = nullptr);
