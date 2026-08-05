#pragma once

#include <cmath>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cui::test
{
    class AssertionFailure final : public std::runtime_error
    {
    public:
        explicit AssertionFailure(std::string message)
            : std::runtime_error(std::move(message))
        {
        }
    };

    [[noreturn]] inline void Fail(
        const char* expression,
        const char* file,
        int line,
        const char* detail = nullptr)
    {
        std::string message = file;
        message += ':';
        message += std::to_string(line);
        message += ": expected ";
        message += expression;
        if (detail && *detail)
        {
            message += " (";
            message += detail;
            message += ')';
        }
        throw AssertionFailure(std::move(message));
    }

    inline void ExpectTrue(
        bool condition,
        const char* expression,
        const char* file,
        int line)
    {
        if (!condition)
            Fail(expression, file, line);
    }

    template<typename TExpected, typename TActual>
    inline void ExpectEqual(
        const TExpected& expected,
        const TActual& actual,
        const char* expectedExpression,
        const char* actualExpression,
        const char* file,
        int line)
    {
        if (!(expected == actual))
        {
            std::string expression = expectedExpression;
            expression += " == ";
            expression += actualExpression;
            Fail(expression.c_str(), file, line);
        }
    }

    inline void ExpectNear(
        double expected,
        double actual,
        double tolerance,
        const char* expectedExpression,
        const char* actualExpression,
        const char* file,
        int line)
    {
        if (tolerance < 0.0 || !std::isfinite(expected) || !std::isfinite(actual) ||
            std::fabs(expected - actual) > tolerance)
        {
            std::string expression = expectedExpression;
            expression += " ~= ";
            expression += actualExpression;
            std::string detail = "expected=" + std::to_string(expected)
                + ", actual=" + std::to_string(actual)
                + ", tolerance=" + std::to_string(tolerance);
            Fail(expression.c_str(), file, line, detail.c_str());
        }
    }

    class Runner final
    {
    public:
        using TestBody = std::function<void()>;

        void Add(std::string name, TestBody body)
        {
            _tests.push_back(TestCase{ std::move(name), std::move(body) });
        }

        int RunAll() const noexcept
        {
            std::size_t passed = 0;
			std::size_t executed = 0;
            char* timingValue = nullptr;
            std::size_t timingValueSize = 0;
            (void)_dupenv_s(
                &timingValue, &timingValueSize, "CUI_TEST_TIMINGS");
            const bool reportTimings = timingValue && *timingValue
                && !(timingValue[0] == '0' && timingValue[1] == '\0');
            std::free(timingValue);
			char* filterValue = nullptr;
			std::size_t filterValueSize = 0;
			(void)_dupenv_s(
				&filterValue, &filterValueSize, "CUI_TEST_FILTER");
			const std::string filter = filterValue ? filterValue : "";
			std::free(filterValue);
            for (const auto& test : _tests)
            {
				if (!filter.empty()
					&& test.Name.find(filter) == std::string::npos) continue;
				++executed;
				std::cout << "[ RUN      ] " << test.Name << '\n' << std::flush;
                const auto started = std::chrono::steady_clock::now();
                try
                {
                    test.Body();
                    ++passed;
					std::cout << "[       OK ] " << test.Name << '\n' << std::flush;
                }
                catch (const std::exception& error)
                {
					std::cerr << "[  FAILED  ] " << test.Name << ": "
						<< error.what() << '\n' << std::flush;
                }
                catch (...)
                {
					std::cerr << "[  FAILED  ] " << test.Name
						<< ": unknown exception\n" << std::flush;
                }
                if (reportTimings)
                {
                    const auto elapsed = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - started).count();
                    std::cout << "[ PERF     ] " << test.Name
                        << ": " << elapsed << " ms\n";
                }
            }

			const std::size_t failed = executed - passed;
			std::cout << "[==========] " << executed << " test(s), "
					  << passed << " passed, " << failed << " failed\n"
					  << std::flush;
			if (!filter.empty() && executed == 0)
			{
				std::cerr << "[  FAILED  ] CUI_TEST_FILTER matched no tests: "
					<< filter << '\n' << std::flush;
				return 2;
			}
            return failed == 0 ? 0 : 1;
        }

    private:
        struct TestCase
        {
            std::string Name;
            TestBody Body;
        };

        std::vector<TestCase> _tests;
    };
}

#define CUI_EXPECT_TRUE(expression) \
    ::cui::test::ExpectTrue(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

#define CUI_EXPECT_FALSE(expression) \
    ::cui::test::ExpectTrue(!static_cast<bool>(expression), "!(" #expression ")", __FILE__, __LINE__)

#define CUI_EXPECT_EQ(expected, actual) \
    ::cui::test::ExpectEqual((expected), (actual), #expected, #actual, __FILE__, __LINE__)

#define CUI_EXPECT_NEAR(expected, actual, tolerance) \
    ::cui::test::ExpectNear( \
        static_cast<double>(expected), \
        static_cast<double>(actual), \
        static_cast<double>(tolerance), \
        #expected, \
        #actual, \
        __FILE__, \
        __LINE__)
