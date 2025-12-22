#pragma once
#ifndef THREAD_H
#define THREAD_H
#include "defines.h"
#include <iostream>
#include <future>
#include <functional>
#include <type_traits>
#include <condition_variable>
#include <chrono>

template <typename T>
class TaskRunning {
public:
	explicit TaskRunning(std::future<T> r) : run_(std::move(r)) {}
	bool IsComplete() {
		return run_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
	}
	auto Get() {
		return run_.get();
	}

private:
	std::future<T> run_;
};

class TaskBase {
public:
	template <typename Func, typename... Args>
	static __forceinline TaskRunning<typename std::invoke_result<Func, Args...>::type> Run(Func&& f, Args&&... args) {
		using ReturnType = typename std::invoke_result<Func, Args...>::type;
		return TaskRunning<ReturnType>(
			std::async(std::launch::async, std::forward<Func>(f), std::forward<Args>(args)...));
	}
};

template <typename Func>
class Task {
public:
	explicit Task(Func f) : func_(std::move(f)) {}
	template <typename... Args>
	__forceinline TaskRunning<typename std::invoke_result<Func, Args...>::type> Start(Args&&... args) {
		using ReturnType = typename std::invoke_result<Func, Args...>::type;
		return TaskRunning<ReturnType>(
			std::async(std::launch::async, func_, std::forward<Args>(args)...));
	}

private:
	Func func_;
};
#endif 