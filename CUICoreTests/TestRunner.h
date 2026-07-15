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
            Fail(expression.c_str(), file, line, "outside tolerance");
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
            char* timingValue = nullptr;
            std::size_t timingValueSize = 0;
            (void)_dupenv_s(
                &timingValue, &timingValueSize, "CUI_TEST_TIMINGS");
            const bool reportTimings = timingValue && *timingValue
                && !(timingValue[0] == '0' && timingValue[1] == '\0');
            std::free(timingValue);

            for (const auto& test : _tests)
            {
                std::cout << "[ RUN      ] " << test.Name << '\n';
                const auto started = std::chrono::steady_clock::now();
                try
                {
                    test.Body();
                    ++passed;
                    std::cout << "[       OK ] " << test.Name << '\n';
                }
                catch (const std::exception& error)
                {
                    std::cerr << "[  FAILED  ] " << test.Name << ": " << error.what() << '\n';
                }
                catch (...)
                {
                    std::cerr << "[  FAILED  ] " << test.Name << ": unknown exception\n";
                }
                if (reportTimings)
                {
                    const auto elapsed = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - started).count();
                    std::cout << "[ PERF     ] " << test.Name
                        << ": " << elapsed << " ms\n";
                }
            }

            const std::size_t failed = _tests.size() - passed;
            std::cout << "[==========] " << _tests.size() << " test(s), "
                      << passed << " passed, " << failed << " failed\n";
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
