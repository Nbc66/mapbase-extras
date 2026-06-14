#include "cbase.h"

#ifdef FP

#include "gesture_def.h"
#include "filesystem.h"
#include "KeyValues.h"

#include "tier0/memdbgon.h"

CGestureRegistry& CGestureRegistry::Instance()
{
	static CGestureRegistry s_Instance;
	return s_Instance;
}

const GestureDef_t* CGestureRegistry::FindByName(const char* pszName) const
{
	if (!pszName || !*pszName)
		return NULL;

	unsigned short idx = m_Defs.Find(pszName);
	if (idx == m_Defs.InvalidIndex())
		return NULL;

	return m_Defs[idx];
}

const GestureDef_t* CGestureRegistry::FindByIndex(unsigned short index) const
{
	if (index == INVALID_GESTURE_DEF_INDEX)
		return NULL;
	if (!m_Defs.IsValidIndex(index))
		return NULL;
	return m_Defs[index];
}

unsigned short CGestureRegistry::FindIndexByName(const char* pszName) const
{
	if (!pszName || !*pszName)
		return INVALID_GESTURE_DEF_INDEX;

	unsigned short idx = m_Defs.Find(pszName);
	if (idx == m_Defs.InvalidIndex())
		return INVALID_GESTURE_DEF_INDEX;
	return idx;
}

void CGestureRegistry::EnsureLoaded()
{
	// Scripts don't change between levels; load once per process.
	static bool s_bLoaded = false;
	if (!s_bLoaded)
	{
		LoadAll();
		s_bLoaded = true;
	}
}

void CGestureRegistry::PrecacheAll()
{
	EnsureLoaded();

	for (unsigned short i = m_Defs.First(); i != m_Defs.InvalidIndex(); i = m_Defs.Next(i))
	{
		GestureDef_t* pDef = m_Defs[i];
		if (!pDef || !pDef->szModel[0])
			continue;   // weapon-baked gestures have no own model to precache

		if (CBaseEntity::PrecacheModel(pDef->szModel) == -1)
			Warning("CGestureRegistry: failed to precache model '%s' for '%s'\n",
				pDef->szModel, pDef->szName);
	}
}

//-----------------------------------------------------------------------------
// Load files in manifest order so index assignment is deterministic and identical
// on both DLLs (index == wire identity). Append-only: reordering shifts every later index.
//-----------------------------------------------------------------------------
void CGestureRegistry::LoadAll()
{
	m_Defs.PurgeAndDeleteElements();

	KeyValues* pManifest = new KeyValues("gestures_manifest");
	if (!pManifest->LoadFromFile(filesystem, "scripts/gestures/gestures_manifest.txt", "GAME"))
	{
		Warning("CGestureRegistry: no manifest at scripts/gestures/gestures_manifest.txt\n");
		pManifest->deleteThis();
		return;
	}

	// Walk every "file" entry IN ORDER.
	for (KeyValues* pSub = pManifest->GetFirstSubKey(); pSub; pSub = pSub->GetNextKey())
	{
		if (Q_stricmp(pSub->GetName(), "file") != 0)
			continue;

		const char* pszFile = pSub->GetString();
		if (pszFile && *pszFile)
			ParseFile(pszFile);
	}

	pManifest->deleteThis();
	DevMsg("CGestureRegistry: loaded %d gesture(s)\n", m_Defs.Count());
}

bool CGestureRegistry::ParseFile(const char* pszPath)
{
	KeyValues* pKV = new KeyValues("gestures");
	if (!pKV->LoadFromFile(filesystem, pszPath, "GAME"))
	{
		Warning("CGestureRegistry: failed to parse %s\n", pszPath);
		pKV->deleteThis();
		return false;
	}

	// Each top-level key in the file is one gesture definition.
	for (KeyValues* pBlock = pKV; pBlock; pBlock = pBlock->GetNextKey())
	{
		const char* pszName = pBlock->GetName();
		if (!pszName || !*pszName)
			continue;

		if (m_Defs.Find(pszName) != m_Defs.InvalidIndex())
		{
			Warning("CGestureRegistry: duplicate '%s' in %s\n", pszName, pszPath);
			continue;
		}

		GestureDef_t* pDef = new GestureDef_t;
		V_strncpy(pDef->szName, pszName, sizeof(pDef->szName));
		V_strncpy(pDef->szModel, pBlock->GetString("model", ""), sizeof(pDef->szModel));
		V_strncpy(pDef->szSequence, pBlock->GetString("sequence", ""), sizeof(pDef->szSequence));

		if (!pDef->szSequence[0])
		{
			Warning("CGestureRegistry: '%s' in %s has no 'sequence'\n", pszName, pszPath);
			delete pDef;
			continue;
		}

		pDef->bLoop = pBlock->GetInt("loop", 0) != 0;
		V_strncpy(pDef->szNext, pBlock->GetString("next", ""), sizeof(pDef->szNext));
		pDef->speed = pBlock->GetFloat("speed", 1.0f);
		pDef->peak = pBlock->GetFloat("peak", 0.4f);
		pDef->speedIn = pBlock->GetFloat("speed_in", 1.0f);
		pDef->speedOut = pBlock->GetFloat("speed_out", 1.0f);
		pDef->curve = pBlock->GetFloat("curve", 1.0f);
		pDef->startCycle = pBlock->GetFloat("start_cycle", 0.0f);
		pDef->fadeOut = pBlock->GetFloat("fade_out", 0.0f);

		m_Defs.Insert(pszName, pDef);
	}

	pKV->deleteThis();
	return true;
}

#ifndef CLIENT_DLL
CON_COMMAND_F(fp_reload_gestures, "Reload gesture defs from the manifest", FCVAR_CHEAT)
{
	CGestureRegistry::Instance().LoadAll();
}
#endif

#endif // FP