#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace MCMBridge
{
	class ViewLoadState
	{
	public:
		std::uint64_t Request(std::string_view a_mod, std::string_view a_page)
		{
			const std::scoped_lock lock(mutex);
			if (mod != a_mod || page != a_page) {
				mod = a_mod;
				page = a_page;
				++revision;
				ready = false;
				error.clear();
			}
			return revision;
		}

		void Invalidate(bool a_preserveError = false)
		{
			const std::scoped_lock lock(mutex);
			++revision;
			ready = false;
			if (!a_preserveError)
				error.clear();
		}

		bool Suspend(std::uint64_t a_revision)
		{
			const std::scoped_lock lock(mutex);
			if (revision != a_revision)
				return false;
			ready = false;
			return true;
		}

		bool Remap(std::uint64_t a_revision, std::string_view a_page)
		{
			const std::scoped_lock lock(mutex);
			if (revision != a_revision)
				return false;
			page = a_page;
			++revision;
			ready = false;
			error.clear();
			return true;
		}

		bool Complete(std::uint64_t a_revision, std::string a_error = {})
		{
			const std::scoped_lock lock(mutex);
			if (a_revision != revision)
				return false;
			ready = a_error.empty();
			error = std::move(a_error);
			return true;
		}

		bool Current(std::uint64_t a_revision) const
		{
			const std::scoped_lock lock(mutex);
			return a_revision == revision;
		}

		bool Ready(std::string_view a_mod, std::string_view a_page) const
		{
			const std::scoped_lock lock(mutex);
			return ready && mod == a_mod && page == a_page;
		}

		std::optional<std::uint64_t> RevisionFor(std::string_view a_mod, std::string_view a_page) const
		{
			const std::scoped_lock lock(mutex);
			return mod == a_mod && page == a_page ? std::optional{ revision } : std::nullopt;
		}

		std::string Error() const
		{
			const std::scoped_lock lock(mutex);
			return error;
		}

	private:
		mutable std::mutex mutex;
		std::string        mod;
		std::string        page;
		std::string        error;
		std::uint64_t      revision{};
		bool               ready{};
	};
}
