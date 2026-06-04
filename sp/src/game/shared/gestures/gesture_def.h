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
		, speedOut(1.0f), curve(1.0f), startCycle(0.0f)
	{
		szName[0] = '\0';
		szModel[0] = '\0';
		szSequence[0] = '\0';
		szNext[0] = '\0';
	}

	// FROZEN IDENTITY KEY. This is the KeyValues block name and the thing you
	// call by (PlayGestureByName). It is also the wire identity via its index.
	// NEVER rename a shipped gesture, and NEVER reorder the manifest/entries
	// both shift the index and break any networked reference. Append-only.
	char  szName[64];

	// Empty  -> weapon-baked gesture: sequence lives in the weapon's own model,
	//           played via PlayGesture (AccumulatePose layer path).
	// Set    -> separate-model gesture: played via PlayGestureFromModel from
	//           this model (VManip-style pose-copy path).
	char  szModel[MAX_PATH];

	// Sequence name (in the weapon model, or in szModel).
	char  szSequence[64];

	bool  bLoop;

	// Optional follow-up: the NAME of another gesture def to auto-queue after this
	// one's sequence finishes (held item: this = pullout one-shot, szNext = idle
	// loop). It's a def reference, not a bare sequence, so the follow-up's loop-ness
	// is just that def's own bLoop -- nothing extra to store here. The queue is a
	// same-model sequence swap, so szNext should name a def whose sequence lives in
	// THIS def's model. Empty = no follow-up.
	char  szNext[64];

	// Envelope / cycle params (defaults match PlayGesture's defaults).
	float speed, peak, speedIn, speedOut, curve, startCycle;

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