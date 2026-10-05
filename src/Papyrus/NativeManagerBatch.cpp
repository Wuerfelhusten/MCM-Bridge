#include "MCMBridge/Papyrus/NativeManagerBatch.h"
#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Plugin/BridgeController.h"

#include <unordered_set>

namespace
{
	using Object = RE::BSTSmartPointer<RE::BSScript::Object>;
	struct Request
	{
		Object                            manager;
		MCMBridge::NativeMenuRegistration menu;
		std::int32_t                      id;
		std::uint64_t                     session;
	};
	std::vector<Request> pending;
	bool                 scheduled{};

	void Execute(std::span<const Request> a_requests)
	{
		auto&                                          requests = MCMBridge::NativeRegistryRequests();
		auto&                                          controller = MCMBridge::BridgeController::GetSingleton();
		auto&                                          registry = controller.NativeRegistry();
		std::vector<const Request*>                    claimed;
		std::vector<MCMBridge::NativeMenuRegistration> menus;
		claimed.reserve(a_requests.size());
		menus.reserve(a_requests.size());
		for (const auto& request : a_requests) {
			if (!requests.Claim(request.session, request.id))
				continue;
			claimed.push_back(&request);
			menus.push_back(request.menu);
		}
		if (claimed.empty())
			return;
		const auto                revision = registry.Revision();
		std::vector<std::int32_t> results(claimed.size(), -1);
		try {
			if (controller.IsSessionReady() && registry.Owns(claimed.front()->manager) &&
				MCMBridge::NativeFacadeSession().Session() == claimed.front()->session) {
				auto batch = registry.RegisterBatch(menus);
				if (batch) {
					results = std::move(*batch);
				} else if (registry.IsAvailable()) {
					// A rejected candidate published nothing. Isolate invalid inputs without
					// dropping valid registrations; no mod callbacks are invoked here.
					SKSE::log::debug("Native registration batch fallback: {}", batch.error().message);
					for (std::size_t index = 0; index < menus.size(); ++index) {
						const auto single = registry.Register(menus[index].object, menus[index].name);
						if (single)
							results[index] = *single;
					}
				}
			}
		} catch (const std::exception& error) {
			SKSE::log::error("Native registration group failed: {}", error.what());
		}
		if (!registry.IsAvailable())
			std::ranges::fill(results, -1);
		// Publish results only after the manager's compatibility mirrors are installed.
		for (std::size_t index = 0; index < claimed.size(); ++index)
			requests.Complete(claimed[index]->session, claimed[index]->id, results[index]);
		if (registry.Revision() != revision)
			controller.RequestRefresh(true);
		SKSE::log::debug("Native registration group completed: requests={}", claimed.size());
	}
}

namespace MCMBridge
{
	void QueueNativeRegistration(Object a_manager, Object a_menu, std::string a_name,
		std::int32_t a_request, std::uint64_t a_session)
	{
		pending.push_back({ std::move(a_manager), { std::move(a_menu), std::move(a_name) }, a_request, a_session });
		if (!scheduled) {
			try {
				auto* tasks = SKSE::GetTaskInterface();
				if (!tasks)
					throw std::runtime_error("Game task interface unavailable");
				tasks->AddTask([] { FlushNativeRegistrations(); });
				scheduled = true;
			} catch (...) {
				pending.pop_back();
				throw;
			}
		}
	}

	void FlushNativeRegistrations()
	{
		auto batch = std::move(pending);
		pending.clear();
		scheduled = false;
		for (std::size_t start = 0; start < batch.size();) {
			std::size_t end = start + 1;
			try {
				std::unordered_set<const RE::BSScript::Object*> seen{ batch[start].menu.object.get() };
				while (end < batch.size() && batch[end].manager == batch[start].manager && batch[end].session == batch[start].session &&
					   seen.insert(batch[end].menu.object.get()).second)
					++end;
				Execute(std::span(batch).subspan(start, end - start));
			} catch (const std::exception& error) {
				SKSE::log::error("Native registration group preparation failed: {}", error.what());
				for (auto index = start; index < end; ++index) {
					auto& requests = NativeRegistryRequests();
					requests.Claim(batch[index].session, batch[index].id);
					requests.Complete(batch[index].session, batch[index].id, -1);
				}
			}
			start = end;
		}
	}
}
