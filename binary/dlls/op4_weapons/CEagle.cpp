/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"

#include "op4_weapons/CEagleLaser.h"

#include "op4_weapons/CEagle.h"

LINK_ENTITY_TO_CLASS(weapon_eagle, CEagle);

void CEagle::Precache()
{
	PRECACHE_MODEL("models/v_desert_eagle.mdl");
	PRECACHE_MODEL("models/w_desert_eagle.mdl");
	PRECACHE_MODEL("models/p_desert_eagle.mdl");
	m_iShell = PRECACHE_MODEL("models/shell.mdl");
	PRECACHE_SOUND("weapons/desert_eagle_fire.wav");
	PRECACHE_SOUND("weapons/desert_eagle_reload.wav");
	PRECACHE_SOUND("weapons/desert_eagle_sight.wav");
	PRECACHE_SOUND("weapons/desert_eagle_sight2.wav");
	m_usFireEagle = PRECACHE_EVENT(1, "events/eagle.sc");
}

void CEagle::Spawn()
{
	pev->classname = MAKE_STRING("weapon_eagle");

	Precache();

	m_iId = WEAPON_EAGLE;

	SET_MODEL(edict(), "models/w_desert_eagle.mdl");

	m_iDefaultAmmo = EAGLE_MAX_CLIP;

	FallInit();
}

BOOL CEagle::AddToPlayer(CBasePlayer* pPlayer)
{
	if (BaseClass::AddToPlayer(pPlayer))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, nullptr, pPlayer->edict());
		WRITE_BYTE(m_iId);
		MESSAGE_END();
		return true;
	}

	return false;
}

BOOL CEagle::Deploy()
{
	m_bSpotVisible = true;

	return DefaultDeploy(
		"models/v_desert_eagle.mdl", "models/p_desert_eagle.mdl",
		EAGLE_DRAW,
		"onehanded");
}

void CEagle::Holster(int skiplocal)
{
	m_fInReload = false;

#ifndef CLIENT_DLL
	if (m_pLaser)
	{
		m_pLaser->Killed(nullptr, GIB_NEVER);
		m_pLaser = nullptr;
		m_bSpotVisible = false;
	}
#endif

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10.0, 15.0);

	SendWeaponAnim(EAGLE_HOLSTER);
}

void CEagle::WeaponIdle()
{
#ifndef CLIENT_DLL
	UpdateLaser();
#endif

	ResetEmptySound();

	if (m_flTimeWeaponIdle <= UTIL_WeaponTimeBase() && m_iClip)
	{
		const float flNextIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 0.0, 1.0);

		int iAnim;

		if (m_bLaserActive)
		{
			if (flNextIdle > 0.5)
			{
				iAnim = EAGLE_IDLE5;
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flNextIdle + 2.0;
			}
			else
			{
				iAnim = EAGLE_IDLE4;
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flNextIdle + 2.5;
			}
		}
		else
		{
			if (flNextIdle <= 0.3)
			{
				iAnim = EAGLE_IDLE1;
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flNextIdle + 2.5;
			}
			else
			{
				if (flNextIdle > 0.6)
				{
					iAnim = EAGLE_IDLE3;
					m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flNextIdle + 1.633;
				}
				else
				{
					iAnim = EAGLE_IDLE2;
					m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flNextIdle + 2.5;
				}
			}
		}

		SendWeaponAnim(iAnim);
	}
}

void CEagle::PrimaryAttack()
{
	if (m_pPlayer->pev->waterlevel == WATERLEVEL_HEAD)
	{
		PlayEmptySound();

		//Note: this is broken in original Op4 since it uses gpGlobals->time when using prediction
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	if (m_iClip <= 0)
	{
		if (!m_fInReload)
		{
			if (m_fFireOnEmpty)
			{
				PlayEmptySound();
				m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.2;
			}
			else
			{
				Reload();
			}
		}

		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	--m_iClip;

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

#ifndef CLIENT_DLL
	if (m_pLaser && m_bLaserActive)
	{
		m_pLaser->pev->effects |= EF_NODRAW;
		m_pLaser->SetThink(&CEagleLaser::Revive);
		m_pLaser->pev->nextthink = gpGlobals->time + 0.6;
	}
#endif

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);

	Vector vecSrc = m_pPlayer->GetGunPosition();

	Vector vecAiming = m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);

	const float flSpread = m_bLaserActive ? 0.001 : 0.1;

	const Vector vecSpread = m_pPlayer->FireBulletsPlayer(
		1,
		vecSrc, vecAiming, Vector(flSpread, flSpread, flSpread),
		8192.0, BULLET_PLAYER_EAGLE, 0, 0,
		m_pPlayer->pev, m_pPlayer->random_seed);

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + (m_bLaserActive ? 0.5 : 0.22);

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL(
		flags, m_pPlayer->edict(), m_usFireEagle, 0,
		g_vecZero, g_vecZero,
		vecSpread.x, vecSpread.y,
		0, 0,
		m_iClip == 0, 0);

	if (!m_iClip)
	{
		if (m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] <= 0)
			m_pPlayer->SetSuitUpdate("!HEV_AMO0", SUIT_SENTENCE, SUIT_REPEAT_OK);
	}

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10.0, 15.0);

#ifndef CLIENT_DLL
	UpdateLaser();
#endif
}

void CEagle::SecondaryAttack()
{
#ifndef CLIENT_DLL
	m_bLaserActive = !m_bLaserActive;

	if (!m_bLaserActive)
	{
		if (m_pLaser)
		{
			m_pLaser->Killed(nullptr, GIB_NEVER);

			m_pLaser = nullptr;

			EMIT_SOUND_DYN(edict(), CHAN_WEAPON, "weapons/desert_eagle_sight2.wav", VOL_NORM, ATTN_NORM, 0, PITCH_NORM);
		}
	}
#endif

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
}

void CEagle::Reload()
{
	if (m_pPlayer->ammo_357 > 0)
	{
		const bool bResult = DefaultReload(EAGLE_MAX_CLIP, m_iClip ? EAGLE_RELOAD : EAGLE_RELOAD_NOSHOT, 1.5, 1);

#ifndef CLIENT_DLL
		if (m_pLaser && m_bLaserActive)
		{
			m_pLaser->pev->effects |= EF_NODRAW;
			m_pLaser->SetThink(&CEagleLaser::Revive);
			m_pLaser->pev->nextthink = gpGlobals->time + 1.6;

			m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.5;
		}
#endif

		if (bResult)
		{
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10.0, 15.0);
		}
	}
}

void CEagle::UpdateLaser()
{
#ifndef CLIENT_DLL
	if (m_bLaserActive && m_bSpotVisible)
	{
		if (!m_pLaser)
		{
			m_pLaser = CEagleLaser::CreateSpot();

			EMIT_SOUND_DYN(edict(), CHAN_WEAPON, "weapons/desert_eagle_sight.wav", VOL_NORM, ATTN_NORM, 0, PITCH_NORM);
		}

		UTIL_MakeVectors(m_pPlayer->pev->v_angle);

		Vector vecSrc = m_pPlayer->GetGunPosition();

		Vector vecEnd = vecSrc + gpGlobals->v_forward * 8192.0;

		TraceResult tr;

		UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, m_pPlayer->edict(), &tr);

		UTIL_SetOrigin(m_pLaser->pev, tr.vecEndPos);
	}
#endif
}

int CEagle::iItemSlot()
{
	return 2;
}

int CEagle::GetItemInfo(ItemInfo* p)
{
	p->pszAmmo1 = "357";
	p->iMaxAmmo1 = _357_MAX_CARRY;
	p->pszName = STRING(pev->classname);
	p->pszAmmo2 = 0;
	p->iMaxAmmo2 = WEAPON_NOCLIP;
	p->iMaxClip = EAGLE_MAX_CLIP;
	p->iSlot = 1;
	p->iPosition = 2;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_EAGLE;
	p->iWeight = EAGLE_WEIGHT;
	return true;
}

void CEagle::GetWeaponData(weapon_data_t& data)
{
	BaseClass::GetWeaponData(data);

	data.iuser1 = m_bLaserActive;
}

void CEagle::SetWeaponData(const weapon_data_t& data)
{
	BaseClass::SetWeaponData(data);

	m_bLaserActive = data.iuser1 != 0;
}