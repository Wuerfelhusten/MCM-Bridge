#pragma pack(push, 1)
#ifndef MCM_HOST_CONTRACT_HEADER
#	define MCM_HOST_CONTRACT_HEADER "MCMBridge/API/MCMBridgeHost.h"
#endif
#include MCM_HOST_CONTRACT_HEADER

#include <stddef.h>

#ifdef __cplusplus
#	define HOST_ASSERT static_assert
#else
#	define HOST_ASSERT _Static_assert
#endif

struct HostPackingProbe
{
	char     byte;
	uint32_t value;
};
HOST_ASSERT(sizeof(struct HostPackingProbe) == 5, "Host header must restore caller packing");
#pragma pack(pop)

HOST_ASSERT(sizeof(MCMHostContext) == 8, "Context is an opaque 64-bit token");
HOST_ASSERT(sizeof(MCMHostArgument) == 24, "Argument layout changed");
HOST_ASSERT(offsetof(MCMHostArgument, text) == 16, "String pointer alignment changed");
HOST_ASSERT(sizeof(MCMHostCall) == 40, "Call layout changed");
HOST_ASSERT(offsetof(MCMHostCall, timeout_ms) == 28, "Call timeout offset changed");
HOST_ASSERT(sizeof(MCMBridgeHost) == 9 * sizeof(void*), "Host table contains only functions");
HOST_ASSERT(offsetof(MCMHostEvent, value) == 88, "Event value alignment changed");
HOST_ASSERT(sizeof(MCMHostEvent) == 136, "Event layout changed");
HOST_ASSERT(sizeof(MCMHostControlView) == 120, "Control view layout changed");
HOST_ASSERT(offsetof(MCMHostControlView, menu_items) == 112, "Menu view alignment changed");
HOST_ASSERT(sizeof(MCMHostPageView) == 32, "Page view layout changed");
HOST_ASSERT(sizeof(MCMHostModView) == 56, "Mod view layout changed");
HOST_ASSERT(sizeof(MCMHostDataView) == 24, "Data view layout changed");
