#pragma once

#include <maxversion.h>
// In 2013, one of our key pblock words was renamed
#if MAX_VERSION_MAJOR < 15 // 3ds Max 2012 and below
#define p_end end
#endif

// In 2012, a bunch of functions were set to const.
// Use this build configuration
#if MAX_VERSION_MAJOR > 14
#define U_CONST const
#else
#define U_CONST
#endif

// In 2015, one of the most-implemented fn's in Max changed its signature
#if MAX_VERSION_MAJOR < 17
#define NOTIFY_REF_CHANGED_FN_DECL NotifyRefChanged(Interval changeInt, RefTargetHandle hTarget, PartID& partID,  RefMessage message)
#define NOTIFY_REF_CHANGED_PARAMS changeInt, hTarget, partID, message
#else
#define NOTIFY_REF_CHANGED_FN_DECL NotifyRefChanged(const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,  RefMessage message, BOOL propagate)
#define NOTIFY_REF_CHANGED_PARAMS changeInt, hTarget, partID, message, propagate
#endif

template<typename T> void SAFE_DELETE(T*& a) {
	delete a;
	a = NULL;
}