//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "in_buttons.h"
#include "discwar.h"
#include "IEffects.h"
#include "Sprite.h"

#ifdef CLIENT_DLL
	#include "c_hl2mp_player.h"
#else
	#include "hl2mp_player.h"
	#include "basecombatcharacter.h"
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

float g_iaDiscColors[33][3] =
{
	{ 255, 255, 255, },
	{ 250, 0, 0 },
	{ 0, 0, 250 },
	{ 0, 250, 0 },
	{ 128, 128, 0 },
	{ 128, 0, 128 },
	{ 0, 128, 128 },
	{ 128, 128, 128 },
	{ 64, 128, 0 },
	{ 128, 64, 0 },
	{ 128, 0, 64 },
	{ 64, 0, 128 },
	{ 0, 64, 128 },
	{ 64, 64, 128 },
	{ 128, 64, 64 },
	{ 64, 128, 64 },
	{ 128, 128, 64 },
	{ 128, 64, 128 },
	{ 64, 128, 128 },
	{ 250, 128, 0 },
	{ 128, 250, 0 },
	{ 128, 0, 250 },
	{ 250, 0, 128 },
	{ 0, 250, 128 },
	{ 250, 250, 128 },
	{ 250, 128, 250 },
	{ 128, 250, 250 },
	{ 250, 128, 64 },
	{ 250, 64, 128 },
	{ 128, 250, 64 },
	{ 64, 128, 250 },
	{ 128, 64, 250 },
};

enum disc_e
{
	DISC_IDLE = 0,
	DISC_FIDGET,
	DISC_PINPULL,
	DISC_THROW1,	// toss
	DISC_THROW2,	// medium
	DISC_THROW3,	// hard
	DISC_HOLSTER,
	DISC_DRAW
};

#ifdef GAME_DLL

class CWeaponDisc;

class CDisc : public CBaseCombatCharacter
{
	DECLARE_CLASS(CDisc, CBaseCombatCharacter);

public:

	Class_T Classify(void) { return CLASS_MISSILE; }
	void	Spawn(void);
	void	Precache(void);
	void	DiscTouch(CBaseEntity* pOther);
	void	DiscThink(void);
	static	CDisc* CreateDisc(const Vector& vecOrigin, const QAngle& vecAngles, edict_t* pentOwner, CWeaponDisc* pLauncher, bool bDecapitator, int iPowerupFlags);

	//void	SetObjectCollisionBox( void );
	void	ReturnToThrower(void);

	virtual bool	IsDisc(void) { return true; };

	float		m_fDontTouchEnemies;	// Prevent enemy touches for a bit
	float		m_fDontTouchOwner;		// Prevent friendly touches for a bit
	int			m_iBounces;		// Number of bounces
	CWeaponDisc* m_pLauncher;	// pointer back to the launcher that fired me. 
	int			m_iTrail;
	int			m_iSpriteTexture;
	bool		m_bDecapitate;	// True if this is a decapitating shot
	bool		m_bRemoveSelf;  // True if the owner of this disc has died
	int			m_iPowerupFlags;// Flags for any powerups active on this disc
	bool		m_bTeleported;  // Disc has gone through a teleport

	EHANDLE m_pLockTarget;

	Vector	m_vecActualVelocity;
	Vector	m_vecSideVelocity;
	Vector	m_vecOrg;

private:
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(disc, CDisc);

BEGIN_DATADESC(CDisc)

	// Function Pointers
	DEFINE_FUNCTION(DiscTouch),
	DEFINE_FUNCTION(DiscThink),

END_DATADESC()

//========================================================================================
// DISC
//========================================================================================
void CDisc::Spawn(void)
{
	Precache();

	SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE);
	AddSolidFlags(FSOLID_TRIGGER);

	// Setup model
	if (m_iPowerupFlags & POW_HARD)
	{
		SetModel("models/weapons/disc_hard.mdl");
	}
	else
	{
		SetModel("models/weapons/disc.mdl");
	}

	UTIL_SetSize(this, -Vector(4, 4, 4), Vector(4, 4, 4));

	SetTouch(&CDisc::DiscTouch);
	SetThink(&CDisc::DiscThink);

	m_iBounces = 0;
	m_fDontTouchOwner = gpGlobals->curtime + 0.2;
	m_fDontTouchEnemies = 0;
	m_bRemoveSelf = false;
	m_bTeleported = false;
	m_pLockTarget = NULL;

	QAngle entityAngles;
	entityAngles = GetAbsAngles();
	Vector forward;
	AngleVectors(entityAngles, &forward);

	// Fast powerup makes discs go faster
	if (m_iPowerupFlags & POW_FAST)
		SetAbsVelocity(forward * DISC_VELOCITY * 1.5);
	else
		SetAbsVelocity(forward * DISC_VELOCITY);

	// Trail
	CPVSFilter filter(GetAbsOrigin());
	te->BeamFollow(filter, 
		0.0f, 
		entindex(), 
		m_iTrail, 
		-1, 
		(m_bDecapitate ? 5 : 3), 
		5, 
		5, 
		0.1f, 
		g_iaDiscColors[GetTeamNumber()][0], 
		g_iaDiscColors[GetTeamNumber()][1], 
		g_iaDiscColors[GetTeamNumber()][2], 
		250);

	// Decapitator's make sound
	if (m_bDecapitate)
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 0.5, 0.5);
	}

	m_nRenderFX = kRenderFxGlowShell;
	SetRenderColor(g_iaDiscColors[GetTeamNumber()][0], 
					g_iaDiscColors[GetTeamNumber()][1], 
					g_iaDiscColors[GetTeamNumber()][2], 100);

	SetNextThink(gpGlobals->curtime + 0.1f);

	AddFlag(FL_OBJECT);
}

void CDisc::Precache(void)
{
	PrecacheModel("models/weapons/disc.mdl");
	PrecacheModel("models/dweapons/isc_hard.mdl");

	PRECACHE_SOUND("weapons/cbar_hitbod1.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod3.wav");
	PRECACHE_SOUND("weapons/altfire.wav");
	PRECACHE_SOUND("items/gunpickup2.wav");
	PRECACHE_SOUND("weapons/electro5.wav");
	PRECACHE_SOUND("weapons/xbow_hit1.wav");
	PRECACHE_SOUND("weapons/xbow_hit2.wav");
	PRECACHE_SOUND("weapons/rocket1.wav");
	PRECACHE_SOUND("dischit.wav");

	m_iTrail = PrecacheModel("sprites/smoke.vmt");
	m_iSpriteTexture = PrecacheModel("sprites/lgtning.vmt");
}

/*
void CDisc::SetObjectCollisionBox( void )
{
	pev->absmin = pev->origin + Vector( -8, -8, 8 );
	pev->absmax = pev->origin + Vector( 8, 8, 8 );
}
*/

// Give the disc back to it's owner
void CDisc::ReturnToThrower(void)
{
	if (m_bDecapitate)
	{
		STOP_SOUND(edict(), CHAN_VOICE, "weapons/rocket1.wav");
		if (!m_bRemoveSelf)
			((CBasePlayer*)(CBaseEntity*)m_hOwner)->GiveAmmo(MAX_DISCS, "disc", MAX_DISCS);
	}
	else
	{
		if (!m_bRemoveSelf)
			((CBasePlayer*)(CBaseEntity*)m_hOwner)->GiveAmmo(1, "disc", MAX_DISCS);
	}

	UTIL_Remove(this);
}

void CDisc::DiscTouch(CBaseEntity* pOther)
{
	// Push players backwards
	if (pOther->IsPlayer())
	{
		if (((CBaseEntity*)m_hOwner) == pOther)
		{
			if (m_fDontTouchOwner < gpGlobals->curtime)
			{
				// Play catch sound
				EMIT_SOUND_DYN(pOther->edict(), CHAN_WEAPON, "items/gunpickup2.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));

				ReturnToThrower();
			}

			return;
		}
		else if (m_fDontTouchEnemies < gpGlobals->curtime)
		{
			if (GetTeamNumber() != pOther->GetTeamNumber())
			{
				// Do freeze seperately so you can freeze and shatter a person with a single shot
				if (m_iPowerupFlags & POW_FREEZE && ((CBasePlayer*)pOther)->m_iFrozen == false)
				{
					// Freeze the player and make them glow blue
					EMIT_SOUND_DYN(pOther->edict(), CHAN_WEAPON, "weapons/electro5.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
					((CBasePlayer*)pOther)->Freeze();

					// If it's not a decap, return now. If it's a decap, continue to shatter
					if (!m_bDecapitate)
					{
						m_fDontTouchEnemies = gpGlobals->curtime + 2.0;
						return;
					}
				}

				// Decap or push
				if (m_bDecapitate)
				{
					// Decapitate!
					if (m_bTeleported)
						((CBasePlayer*)pOther)->m_flLastDiscHitTeleport = gpGlobals->curtime;
					((CBasePlayer*)pOther)->Decapitate(((CBaseEntity*)m_hOwner)->pev);

					m_fDontTouchEnemies = gpGlobals->curtime + 0.5;
				}
				else
				{
					// Play thwack sound
					switch (random->RandomInt(0, 2))
					{
						case 0:
							EMIT_SOUND_DYN(pOther->edict(), CHAN_ITEM, "weapons/cbar_hitbod1.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
							break;
						case 1:
							EMIT_SOUND_DYN(pOther->edict(), CHAN_ITEM, "weapons/cbar_hitbod2.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
							break;
						case 2:
							EMIT_SOUND_DYN(pOther->edict(), CHAN_ITEM, "weapons/cbar_hitbod3.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
							break;
					}

					// Push the player
					Vector vecDir = pev->velocity.Normalize();
					pOther->pev->flags &= ~FL_ONGROUND;
					((CBasePlayer*)pOther)->m_vecHitVelocity = vecDir * DISC_PUSH_MULTIPLIER;

					// Shield flash only if the player isnt frozen
					if (((CBasePlayer*)pOther)->m_iFrozen == false)
					{
						pOther->pev->renderfx = kRenderFxGlowShell;
						pOther->pev->rendercolor.x = 255;
						pOther->pev->renderamt = 150;
					}

					((CBasePlayer*)pOther)->m_hLastPlayerToHitMe = m_hOwner;
					((CBasePlayer*)pOther)->m_flLastDiscHit = gpGlobals->curtime;
					((CBasePlayer*)pOther)->m_iLastDiscBounces = m_iBounces;
					if (m_bTeleported)
						((CBasePlayer*)pOther)->m_flLastDiscHitTeleport = gpGlobals->curtime;

					m_fDontTouchEnemies = gpGlobals->curtime + 2.0;
				}
			}
		}
	}
	// Hit a disc?
	else if (pOther)
	{
		// Enemy Discs destroy each other
		if (pOther->GetTeamNumber() != GetTeamNumber())
		{
			// Play a warp sound and sprite
			CSprite* pSprite = CSprite::SpriteCreate("sprites/discreturn.spr", pev->origin, TRUE);
			pSprite->AnimateAndDie(60);
			pSprite->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation);
			pSprite->SetScale(1);
			EMIT_SOUND_DYN(edict(), CHAN_ITEM, "dischit.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));

			// Return both discs to their owners

			CDisc* pOtherDisc = (CDisc*)pOther;

			if (pOtherDisc)
			{
				pOtherDisc->ReturnToThrower();
			}

			ReturnToThrower();
		}

		// Friendly discs just pass through each other
	}
	else
	{
		m_iBounces++;

		switch (RANDOM_LONG(0, 1))
		{
		case 0:	EMIT_SOUND_DYN(edict(), CHAN_ITEM, "weapons/xbow_hit1.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));  break;
		case 1:	EMIT_SOUND_DYN(edict(), CHAN_ITEM, "weapons/xbow_hit2.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));  break;
		}

		g_pEffects->Sparks(GetAbsOrigin());
	}
}

void CDisc::DiscThink()
{
	// Make Freeze discs home towards any player ahead of them
	if ((m_iPowerupFlags & POW_FREEZE) && (m_iBounces == 0))
	{
		// Use an existing target if he's still in the view cone
		if (m_pLockTarget != NULL)
		{
			Vector vecDir = (m_pLockTarget->GetAbsOrigin() - GetAbsOrigin()).Normalized();
			QAngle entityAngles;
			entityAngles = GetAbsAngles();
			Vector forward;
			AngleVectors(entityAngles, &forward);
			float flDot = DotProduct(forward, vecDir);
			if (flDot < 0.6)
				m_pLockTarget = NULL;
		}

		// Get a new target if we don't have one
		if (m_pLockTarget == NULL)
		{
			CBaseEntity* pOther = NULL;

			// Examine all entities within a reasonable radius
			while ((pOther = UTIL_FindEntityByClassname(pOther, "player")) != NULL)
			{
				// Skip the guy who threw this
				if (((CBasePlayer*)GetOwnerEntity()) == pOther)
					continue;

				// Skip observers
				if (((CBasePlayer*)pOther)->IsObserver())
					continue;

				// Make sure the enemy's in a cone ahead of us
				Vector vecDir = (m_pLockTarget->GetAbsOrigin() - GetAbsOrigin()).Normalized();
				QAngle entityAngles;
				entityAngles = GetAbsAngles();
				Vector forward;
				AngleVectors(entityAngles, &forward);
				float flDot = DotProduct(forward, vecDir);
				if (flDot > 0.6)
				{
					m_pLockTarget = pOther;
					break;
				}
			}
		}

		// Track towards our target
		if (m_pLockTarget != NULL)
		{
			// Calculate new velocity
			Vector vecDir = (m_pLockTarget->GetAbsOrigin() - GetAbsOrigin()).Normalized();
			SetAbsVelocity(((GetAbsVelocity().Normalized() + (vecDir.Normalized() * 0.25)).Normalized()) * DISC_VELOCITY);
			QAngle qAngles;
			VectorAngles(GetAbsVelocity(), qAngles);
			SetAbsAngles(qAngles);
		}
	}

	// Track the player if we've bounced 3 or more times ( Fast discs remove immediately )
	if (m_iBounces >= 3 || (m_iPowerupFlags & POW_FAST && m_iBounces >= 1))
	{
		// Remove myself if my owner's died
		if (m_bRemoveSelf)
		{
			STOP_SOUND(edict(), CHAN_VOICE, "weapons/rocket1.wav");
			UTIL_Remove(this);
			return;
		}

		// 7 Bounces, just remove myself
		if (m_iBounces > 7)
		{
			ReturnToThrower();
			return;
		}

		// Start heading for the player
		if (GetOwnerEntity())
		{
			Vector vecDir = (GetOwnerEntity()->GetAbsOrigin() - GetAbsOrigin());
			vecDir = vecDir.Normalized();
			SetAbsVelocity(vecDir * DISC_VELOCITY);
			SetNextThink(gpGlobals->curtime + 0.1f);
		}
		else
		{
			UTIL_Remove(this);
		}
	}

	// Sanity check
	if (GetAbsVelocity() == Vector(0, 0, 0))
	{
		ReturnToThrower();
	}

	SetNextThink(gpGlobals->curtime + 0.1f);
}

CDisc* CDisc::CreateDisc(const Vector& vecOrigin, const QAngle& vecAngles, edict_t* pentOwner, CWeaponDisc* pLauncher, bool bDecapitator, int iPowerupFlags)
{
	CDisc* pDisc = (CDisc*)CBaseEntity::Create("disc", vecOrigin, vecAngles, CBaseEntity::Instance(pentOwner));

	pDisc->m_iPowerupFlags = iPowerupFlags;
	// Hard shots always decapitate
	if (pDisc->m_iPowerupFlags & POW_HARD)
		pDisc->m_bDecapitate = true;
	else
		pDisc->m_bDecapitate = bDecapitator;

	CBaseEntity* pOwner = Instance(pentOwner);

	pDisc->SetOwnerEntity(pOwner);

	pDisc->m_pLauncher = pLauncher;

	pDisc->ChangeTeam(pOwner->GetTeamNumber());

	pDisc->Spawn();

	return pDisc;
}
#endif

#ifdef CLIENT_DLL
#define CWeaponDisc C_WeaponDisc
#endif

//-----------------------------------------------------------------------------
// CWeaponDisc
//-----------------------------------------------------------------------------

class CWeaponDisc : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponDisc, CBaseHL2MPCombatWeapon );

	CWeaponDisc(void);

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	void	Precache( void );
	void	ItemPostFrame( void );
	void	ItemPreFrame( void );
	void	ItemBusyFrame( void );
	void	PrimaryAttack( void );

	virtual float GetFireRate( void ) 
	{
		return 0.5f; 
	}
	
#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif

private:
	CWeaponDisc( const CWeaponDisc & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponDisc, DT_WeaponDisc )

BEGIN_NETWORK_TABLE( CWeaponDisc, DT_WeaponDisc )
#ifdef CLIENT_DLL
#else
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponDisc )
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_disc, CWeaponDisc );
PRECACHE_WEAPON_REGISTER( weapon_disc );

#ifndef CLIENT_DLL
acttable_t CWeaponDisc::m_acttable[] = 
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_PISTOL,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_PISTOL,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_PISTOL,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_PISTOL,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_PISTOL,				false },
};


IMPLEMENT_ACTTABLE( CWeaponDisc );

#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponDisc::CWeaponDisc( void )
{
	m_fMinRange1		= 24;
	m_fMaxRange1		= 1500;
	m_fMinRange2		= 24;
	m_fMaxRange2		= 200;

	m_bFiresUnderwater	= true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponDisc::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponDisc::PrimaryAttack( void )
{
	BaseClass::PrimaryAttack();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponDisc::ItemPreFrame( void )
{
	BaseClass::ItemPreFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponDisc::ItemBusyFrame( void )
{
	BaseClass::ItemBusyFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Allows firing as fast as button is pressed
//-----------------------------------------------------------------------------
void CWeaponDisc::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();
}