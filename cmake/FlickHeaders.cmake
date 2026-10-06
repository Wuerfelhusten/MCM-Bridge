include(FetchContent)

# Headers only. All drawing calls go through the host's function table, never
# through a second linked ImGui library or the other frontend's context.
FetchContent_Declare(flick_api
	GIT_REPOSITORY https://github.com/Fuzzlesz/FUCK_API.git
	GIT_TAG a9ce5d17ebe095e3f11e2d9acd34534fd3d066c9
)
FetchContent_Declare(flick_imgui
	URL https://codeload.github.com/powerof3/imgui/tar.gz/fbbe3efd107e960f864d2944fdf280b465110bad
	URL_HASH SHA512=33909f0161af0052158adc5a3469df750718b65bbedc6df20f50b568a1c9a22c4f71a1f3de215bc0002c4389c6e7598009eca63ba77c1aef2d564ab5a5560d5c
)
FetchContent_MakeAvailable(flick_api flick_imgui)
target_include_directories(MCMBridge SYSTEM PRIVATE "${flick_api_SOURCE_DIR}" "${flick_imgui_SOURCE_DIR}")
