#include "MCMBridge/Core/HelperMessageCallback.h"

#include <cstring>
#include <utility>

namespace
{
	void* Callable(void* a_storage)
	{
		void* result{};
		if (a_storage)
			std::memcpy(&result, static_cast<std::byte*>(a_storage) + 56, sizeof(result));
		return result;
	}

	template <class T>
	T Method(void* a_callable, std::size_t a_slot)
	{
		void** table{};
		std::memcpy(&table, a_callable, sizeof(table));
		T method{};
		std::memcpy(&method, table + a_slot, sizeof(method));
		return method;
	}

	void Destroy(void* a_callable, void* a_storage)
	{
		if (a_callable)
			Method<void (*)(void*, bool)>(a_callable, 4)(a_callable, a_callable != a_storage);
	}
}

namespace MCMBridge
{
	HelperMessageCallback::HelperMessageCallback(void* a_storage)
	{
		if (auto* source = Callable(a_storage))
			callable = Method<void* (*)(void*, void*)>(source, 0)(source, storage.data());
	}

	HelperMessageCallback::~HelperMessageCallback()
	{
		Abandon();
	}

	void HelperMessageCallback::Complete(bool a_result)
	{
		// Remove ownership before invocation: reentrant completion cannot call twice.
		if (auto* target = std::exchange(callable, nullptr)) {
			try {
				Method<void (*)(void*, bool*)>(target, 2)(target, &a_result);
			} catch (...) {
				Destroy(target, storage.data());
				throw;
			}
			Destroy(target, storage.data());
		}
	}

	void HelperMessageCallback::Abandon()
	{
		Destroy(std::exchange(callable, nullptr), storage.data());
	}

	void HelperMessageCallback::DestroyArgument(void* a_storage)
	{
		Destroy(Callable(a_storage), a_storage);
		if (a_storage) {
			void* empty{};
			std::memcpy(static_cast<std::byte*>(a_storage) + 56, &empty, sizeof(empty));
		}
	}
}
