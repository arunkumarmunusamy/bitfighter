//-----------------------------------------------------------------------------------
//
// Bitfighter - A multiplayer vector graphics space game
// Based on Zap demo released for Torque Network Library by GarageGames.com
//
// Derivative work copyright (C) 2008-2009 Chris Eykamp
// Original work copyright (C) 2004 GarageGames.com, Inc.
// Other code copyright as noted
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful (and fun!),
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//------------------------------------------------------------------------------------

#ifndef _GAMEWEAPONS_H_
#define _GAMEWEAPONS_H_

#include "tnlNetStringTable.h"
#include "Point.h"

using namespace TNL;

namespace Zap
{


// Note that WeaponType can be used as an array index
enum WeaponType
{
   WeaponPhaser = 0,
   WeaponBounce,
   WeaponTriple,
   WeaponBurst,          // Grenade
   WeaponHeatSeeker,     // Heatseeker
   WeaponMine,
   WeaponTurret,
   WeaponSpyBug,
   WeaponCount,
   InvalidWeapon,
};


// Note that not all WeaponTypes are Projectile weapons, so don't have ProjectileTypes
enum ProjectileType
{
   ProjectilePhaser,
   ProjectileBounce,
   ProjectileTriple,
//   ProjectileGuided,    // Heatseeker
   ProjectileTurret,
   ProjectileTypeCount,
   NotAProjectile  // Need this so we can fit a non-ProjectileType (like mine) into a constructor intended for proper projectiles
};

struct WeaponInfo
{
   WeaponInfo(StringTableEntry _name, U32 _fireDelay, U32 _minEnergy, U32 _drainEnergy, U32 _projVelocity, S32 _projLiveTime, 
              F32 _damageAmount, F32 _damageSelfMultiplier, bool _canDamageTeammate, ProjectileType _projectileType)
   {
      name = _name;
      fireDelay = _fireDelay;
      minEnergy = _minEnergy;
      drainEnergy = _drainEnergy;
      projVelocity = _projVelocity;
      projLiveTime = _projLiveTime;
      damageAmount = _damageAmount;
      damageSelfMultiplier = _damageSelfMultiplier;
      canDamageTeammate = _canDamageTeammate;
      projectileType = _projectileType;
   }

   static const char *getWeaponName(WeaponType weaponType)
   {
      switch((S32)weaponType)
      {
         case WeaponPhaser:
            return "Phaser";
         case WeaponBounce:
            return "Bouncer";
         case WeaponTriple:
            return "Triple";
         case WeaponBurst:
            return "Burst";
         case WeaponHeatSeeker:
            return "Heat Seeker";
         case WeaponMine:
            return "Mine";
         case WeaponTurret:
            return "Turret";
         case WeaponSpyBug:
            return "Spy Bug";
      }
      return "INVALID WEAPON!";
   }


   StringTableEntry name;     // Display name of the weapon.
   U32 fireDelay;             // Delay between shots.
   S32 minEnergy;             // Minimum energy to fire.  (Use S32 to avoid compiler warnings when comparing with other S32s)
   U32 drainEnergy;           // Amount of energy to drain per shot.
   U32 projVelocity;          // How fast shot travels (pix/second?)
   S32 projLiveTime;          // How long shot lives (millisecs)
   F32 damageAmount;          // Damage shot does
   F32 damageSelfMultiplier;  // Adjust damage if you shoot yourself
   bool canDamageTeammate;
   ProjectileType projectileType;   // If this is a projectile item, which sort is it?  If not, use NotAProjectile
};

extern WeaponInfo gWeapons[WeaponCount];
class GameObject;
extern void createWeaponProjectiles(WeaponType weapon, Point &dir, Point &shooterPos, Point &shooterVel, F32 shooterRadius, GameObject *shooter);

enum {
   NumSparkColors = 4,
};


};

#endif


