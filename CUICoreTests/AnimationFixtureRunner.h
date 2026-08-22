#pragma once

#include <optional>

namespace cui::test
{
	class Runner;
}

/**
 * Runs the animation-conformance CLI mode when --animation-fixtures is present.
 * Returns no value when the ordinary CUICoreTests runner should continue.
 */
std::optional<int> TryRunAnimationFixtureCommandLine();

/** Registers in-memory parser, serializer, and deterministic runner checks. */
void RegisterAnimationFixtureTests(cui::test::Runner& runner);
