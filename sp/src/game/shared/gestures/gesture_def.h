#ifndef GESTURE_DEF_H
#define GESTURE_DEF_H
#ifdef _WIN32
#pragma once
#endif

#ifdef FP

#include "cbase.h"
#include "utldict.h"
#include "utlstring.h"

// Sentinel for "no def / not found".
#define INVALID_GESTURE_DEF_INDEX ((unsigned short)0xFFFF)

//-----------------------------------------------------------------------------
// Static definition of a viewmodel gesture, loaded from script at startup.
// Immutable shared data; the networked path references these by index, and
// index identity is FROZEN by load order (see the manifest). Both DLLs load
// the same manifest -> matching indices, exactly like AttachmentDef_t.
//-----------------------------------------------------------------------------
struct GestureDef_t
{
	GestureDef_t()
		: bLoop(false)
		, speed(1.0f), peak(0.4f), speedIn(1.0f)
		, speedOut(1.0f), curve(1.0f), startCycle(0.0f), fadeOut(0.0f)
	{
		szName[0] = '\0';
		szModel[0] = '\0';
		szSequence[0] = '\0';
		szNext[0] = '\0';
	}

	// Frozen identity key: the KeyValues block name, the name you call by, and (via its
	// index) the wire identity. Append-only -- never rename or reorder shipped gestures.
	char  szName[64];

	// Empty -> weapon-baked gesture (sequence in the weapon's own model, layer path).
	// Set   -> separate-model gesture (played from this model, pose-copy path).
	char  szModel[MAX_PATH];

	// Sequence name (in the weapon model, or in szModel).
	char  szSequence[64];

	bool  bLoop;

	// Optional follow-up: NAME of another def to auto-queue when this sequence finishes
	// (pullout -> idle). Same-model swap, so it must live in this def's model. Empty = none.
	char  szNext[64];

	// Envelope / cycle params (defaults match PlayGesture's).
	float speed, peak, speedIn, speedOut, curve, startCycle;

	// End fade-out (s): on a one-shot's last frame, ramp weight 1->0 so the bones slerp
	// back to the live weapon pose. 0 = snap on retire.
	float fadeOut;

	bool UsesModel() const { return szModel[0] != '\0'; }
};

//-----------------------------------------------------------------------------
// Singleton registry. Loaded once per DLL from the same manifest, giving
// server and client matching indices. Shared (compiled into both DLLs).
//-----------------------------------------------------------------------------
class CGestureRegistry
{
public:
	static CGestureRegistry& Instance();

	void LoadAll();

	// Loads defs if not already loaded. Idempotent; safe from any context.
	void EnsureLoaded();

	const GestureDef_t* FindByName(const char* pszName) const;
	const GestureDef_t* FindByIndex(unsigned short index) const;
	unsigned short      FindIndexByName(const char* pszName) const;

	int  Count() const { return m_Defs.Count(); }
	const GestureDef_t& Get(unsigned short index) const { return *m_Defs[index]; }
	bool IsValidIndex(unsigned short index) const { return m_Defs.IsValidIndex(index); }

	// Precache every gesture's model. Call from a per-level Precache (e.g. the
	// modular weapon's), alongside the attachment registry's PrecacheAll.
	void PrecacheAll();

private:
	CGestureRegistry() {}
	~CGestureRegistry() { m_Defs.PurgeAndDeleteElements(); }

	bool ParseFile(const char* pszPath);

	CUtlDict< GestureDef_t*, unsigned short > m_Defs;
};

inline const GestureDef_t* GetGestureDef(unsigned short index)
{
	return CGestureRegistry::Instance().FindByIndex(index);
}

#endif // FP

#endif // GESTURE_DEF_H