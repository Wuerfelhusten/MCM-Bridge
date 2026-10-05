#pragma once

#include "MCMBridge/Core/ClassicParser.h"
#include "MCMBridge/Core/OperationContext.h"

#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

namespace MCMBridge
{
	struct NativeHostDialog
	{
		std::int32_t                request{};
		std::int32_t                optionID{ -1 };
		std::int32_t                type{};
		std::array<float, 5>        slider{ 0, 0, 0, 1, 1 };
		std::array<std::int32_t, 2> menu{ -1, -1 };
		std::array<std::int32_t, 2> color{ -1, -1 };
		std::vector<std::string>    options;
		std::string                 input;
	};

	struct NativeHostPage
	{
		std::uint64_t      session{};
		std::string        modID;
		std::string        page;
		std::int32_t       index{ -1 };
		std::uint64_t      structureRevision{};
		std::uint64_t      valueRevision{};
		ClassicPageBuffers buffers;
		std::string        title;
		std::string        info;
		std::string        customSource;
		float              customX{};
		float              customY{};
		std::uint64_t      resetRevision{};
	};

	struct NativeHostCursorValue
	{
		std::int32_t type{};
		float        value{};
	};

	struct NativeHostPageRequest
	{
		std::string  name;
		std::int32_t index{ -1 };
	};
	struct NativeHostIdentity
	{
		std::uint64_t session{};
		std::string   modID;
	};

	// One execution owner. VM natives mutate only this synchronized, engine-free state.
	// Published pages describe host values, not proof of committed mod settings.
	class NativeHostSession
	{
	public:
		explicit NativeHostSession(const IOperationClock& a_clock = SteadyOperationClock::GetSingleton()) : clock(a_clock) {}
		std::chrono::steady_clock::duration  MessageWaitDuration(std::int32_t a_token) const;
		void                                 Reset(std::uint64_t a_session);
		std::uint64_t                        Session() const;
		std::optional<NativeHostIdentity>    ReadIdentity(std::int32_t a_token) const;
		Result<std::int32_t>                 Open(std::uint64_t a_session, std::string a_modID, std::uint64_t a_externalOwner = 0);
		std::int32_t                         TokenForOwner(std::uint64_t a_owner) const;
		std::int32_t                         ActiveDialogRequest(std::int32_t a_token, std::int32_t a_type) const;
		bool                                 Close(std::int32_t a_token);
		bool                                 SetOptionCursor(std::int32_t a_token, std::int32_t a_slot);
		std::optional<NativeHostCursorValue> ReadOptionCursor(std::int32_t a_token) const;
		bool                                 BeginPage(std::int32_t a_token, std::string a_page, std::int32_t a_index);
		bool                                 SetCursor(std::int32_t a_token, std::int32_t a_position, std::int32_t a_fillMode);
		std::int32_t                         AddOption(std::int32_t a_token, std::int32_t a_type, std::string a_label,
			std::string a_text, float a_value, std::int32_t a_flags, std::string a_state);
		bool                                 SetValue(std::int32_t a_token, std::int32_t a_optionID, std::string a_text, float a_value);
		bool                                 SetFlags(std::int32_t a_token, std::int32_t a_optionID, std::int32_t a_flags);
		// Helper's lower-level setters address the current page by slot, not encoded option ID.
		bool                                    UpdateControl(std::int32_t a_token, std::int32_t a_index, std::optional<std::string> a_text,
			std::optional<float> a_value, std::optional<std::int32_t> a_flags, std::optional<std::int32_t> a_expectedType = {});
		bool                                    ImportBuffers(std::int32_t a_token, ClassicPageBuffers a_buffers);
		bool                                    Publish(std::int32_t a_token);
		bool                                    RequestPage(std::int32_t a_token, std::string a_name, std::int32_t a_index);
		std::optional<NativeHostPageRequest>    TakePageRequest(std::int32_t a_token);
		bool                                    RequestClose(std::int32_t a_token, bool a_closeFrontend);
		std::optional<bool>                     TakeCloseRequest(std::int32_t a_token);
		bool                                    SetNavigation(std::int32_t a_token, std::vector<std::string> a_pages);
		void                                    ClearNavigationOverride(std::int32_t a_token);
		std::optional<std::vector<std::string>> ReadNavigation(std::int32_t a_token) const;
		std::shared_ptr<const NativeHostPage>   Read() const;
		std::uint64_t                           IdentityRevision() const;
		void                                    BeginIdentityCapture(std::int32_t a_token);
		bool                                    IsActive(std::int32_t a_token) const;
		bool                                    SetPresentation(std::int32_t a_token, std::int32_t a_kind, std::string a_text);
		bool                                    SetCustomContent(std::int32_t a_token, std::string a_source, float a_x, float a_y);
		std::optional<NativeHostDialog>         ReadDialog(std::int32_t a_token) const;
		// The execution owner brackets the original metadata callback. Each request
		// has a separate token so late setters cannot affect a later dialog.
		std::int32_t                    BeginDialog(std::int32_t a_token, std::int32_t a_optionID);
		std::optional<NativeHostDialog> FinishDialog(std::int32_t a_token, std::int32_t a_request);
		bool                            CancelDialog(std::int32_t a_token, std::int32_t a_request);
		bool                            SetSliderParameter(std::int32_t a_request, std::int32_t a_index, float a_value);
		bool                            SetDialogIndex(std::int32_t a_request, std::int32_t a_type, std::int32_t a_index, std::int32_t a_value);
		bool                            SetDialogOptions(std::int32_t a_request, std::vector<std::string> a_options);
		bool                            SetDialogInput(std::int32_t a_request, std::string a_text);
		std::int32_t                    BeginMessage(std::int32_t a_token);
		bool                            IsMessageActive(std::int32_t a_token, std::int32_t a_request) const;
		bool                            CompleteMessage(std::int32_t a_token, std::int32_t a_request, bool a_accepted);
		// -2: expired, -1: pending, 0: rejected, 1: accepted. Terminal reads consume the result.
		std::int32_t TakeMessage(std::int32_t a_token, std::int32_t a_request);

	private:
		const IOperationClock&                                             clock;
		std::chrono::steady_clock::duration                                messageWait{};
		IOperationClock::TimePoint                                         messageStarted{};
		bool                                                               Owns(std::int32_t a_token) const;
		std::optional<std::size_t>                                         Resolve(std::int32_t a_optionID) const;
		mutable std::mutex                                                 mutex;
		std::uint64_t                                                      session{};
		std::uint64_t                                                      identityRevision{};
		std::int32_t                                                       nextToken{};
		std::int32_t                                                       nextDialog{};
		std::int32_t                                                       nextMessage{};
		std::int32_t                                                       messageRequest{};
		std::optional<bool>                                                messageResult;
		std::optional<NativeHostDialog>                                    dialog;
		std::optional<NativeHostDialog>                                    completedDialog;
		std::int32_t                                                       token{};
		std::uint64_t                                                      externalOwner{};
		std::int32_t                                                       cursor{};
		std::int32_t                                                       optionCursor{ -1 };
		std::int32_t                                                       fillMode{ 1 };
		bool                                                               building{};
		bool                                                               hasPage{};
		NativeHostPage                                                     working;
		std::optional<NativeHostPageRequest>                               pageRequest;
		std::optional<bool>                                                closeRequest;
		std::optional<std::vector<std::string>>                            navigation;
		bool                                                               navigationOverride{};
		std::shared_ptr<const NativeHostPage>                              published;
		std::map<std::pair<std::string, std::int32_t>, ClassicPageBuffers> identityPages;
	};
}
