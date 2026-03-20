#pragma once

#if UE_AUTORTFM_ENABLED
UE_AUTORTFM_API bool autortfm_is_transactional(void) AUTORTFM_NOEXCEPT;
#else
bool autortfm_is_transactional(void)
{
	return false;
}
#endif

namespace AutoRTFM
{
	bool IsTransactional() { return autortfm_is_transactional(); }
}