#pragma once

#include <future>

template<class Function, class... Args>
void async_wrapper(Function&& f, Args&&... args, std::future<void>& future, std::future<void>&& is_valid, std::promise<void>&& is_moved) {
	is_valid.wait();
	auto our_future = std::move(future);
	is_moved.set_value();
	auto functor = std::bind(f, std::forward<Args>(args)...);
	functor();
	//our_future = std::future<void>();
}

template<class Function, class... Args>
void void_async(Function&& f, Args&&... args) {
	std::future<void> future;
	std::promise<void> valid;
	std::promise<void> is_moved;
	auto valid_future = valid.get_future();
	auto moved_future = is_moved.get_future();
	future = std::async(async_wrapper<Function, Args...>, std::forward<Function>(f), std::forward<Args>(args)..., std::ref(future), std::move(valid_future), std::move(is_moved));
	valid.set_value();
	moved_future.wait();
}