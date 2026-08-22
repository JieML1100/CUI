#pragma once

#include <optional>

namespace cui::test
{
	class Runner;
}

/** Runs the standalone animation benchmark mode when requested. */
std::optional<int> TryRunAnimationPerformanceCommandLine();

/** Registers fast lifecycle and metric-contract checks, not the full baseline. */
void RegisterAnimationPerformanceTests(cui::test::Runner& runner);
