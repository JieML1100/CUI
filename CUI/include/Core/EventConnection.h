#pragma once

#include <functional>
#include <utility>

/** Move-only RAII handle returned by event subscriptions. */
class EventConnection final
{
public:
	EventConnection() = default;
	explicit EventConnection(std::function<void()> disconnect)
		: _disconnect(std::move(disconnect)) {}

	~EventConnection() { Disconnect(); }

	EventConnection(const EventConnection&) = delete;
	EventConnection& operator=(const EventConnection&) = delete;

	EventConnection(EventConnection&& other) noexcept
		: _disconnect(std::move(other._disconnect))
	{
		other._disconnect = {};
	}

	EventConnection& operator=(EventConnection&& other) noexcept
	{
		if (this == &other) return *this;
		Disconnect();
		_disconnect = std::move(other._disconnect);
		other._disconnect = {};
		return *this;
	}

	void Disconnect() noexcept
	{
		if (!_disconnect) return;
		auto disconnect = std::move(_disconnect);
		_disconnect = {};
		disconnect();
	}

	[[nodiscard]] bool Connected() const noexcept
	{
		return static_cast<bool>(_disconnect);
	}

private:
	std::function<void()> _disconnect;
};
