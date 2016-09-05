#pragma once

#include <future>

template<typename T>
class shared_promise {
	std::promise<T> p;
	std::shared_future<T> sf;

public:
	shared_promise() : sf(p.get_future().share()) {	}

	template<class Alloc>
	shared_promise(std::allocator_arg_t arg, const Alloc& alloc) : p(arg, alloc), shared_promise() {}
	
	shared_promise(shared_promise&& other) : p(std::move(other.p)), sf(std::move(other.sf)) {}
	
	shared_promise(const shared_promise& other) = delete;

	std::shared_future<T> get_shared_future() {
		return sf;
	}

	void set_value(const T& value) {
		p.set_value(value);
	}
	
	void set_value(T&& value) {
		p.set_value(value);
	}
};