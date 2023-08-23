#include "threading/Waiter.h"

namespace Game3 {
	Waiter::Waiter(size_t remaining_) noexcept:
		remaining(remaining_) {}

	Waiter & Waiter::operator--() noexcept {
		--remaining;
		condition.notify_all();
		return *this;
	}

	void Waiter::wait() {
		std::unique_lock lock(mutex);
		condition.wait(lock, [this] { return remaining == 0; });
	}

	bool Waiter::isDone() const noexcept {
		return remaining == 0;
	}

	void Waiter::reset(size_t new_remaining) {
		if (remaining.exchange(new_remaining) != 0)
			throw std::runtime_error("Reset an unfinished Waiter");
	}
}
