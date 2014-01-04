//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------


#include "SlipZone.h"

#include "game.h"
#include "gameObjectRender.h"

#include "stringUtils.h"

namespace Zap
{

SlipZone::SlipZone()     // Constructor
{
   setTeam(0);
   mNetFlags.set(Ghostable);
   mObjectTypeNumber = SlipZoneTypeNumber;
   slipAmount = 0.1f;
}

// Destructor
SlipZone::~SlipZone()
{
   // Do nothing
}


SlipZone *SlipZone::clone() const
{
   return new SlipZone(*this);
}


void SlipZone::render()
{
   renderSlipZone(getOutline(), getFill(), getCentroid());
}


void SlipZone::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled)
{
   render();
   PolygonObject::renderEditor(currentScale, snappingToWallCornersEnabled);
}


S32 SlipZone::getRenderSortValue()
{
   return -1;
}


bool SlipZone::processArguments(S32 argc2, const char **argv2, Game *game)
{
   // Need to handle or ignore arguments that starts with letters,
   // so a possible future version can add parameters without compatibility problem.
   S32 argc = 0;
   const char *argv[Geometry::MAX_POLY_POINTS * 2 + 1];
   for(S32 i = 0; i < argc2; i++)  // the idea here is to allow optional R3.5 for rotate at speed of 3.5
   {
      char c = argv2[i][0];
      //switch(c)
      //{
      //case 'A': Something = atof(&argv2[i][1]); break;  // using second char to handle number
      //}
      if((c < 'a' || c > 'z') && (c < 'A' || c > 'Z'))
      {
			if(argc < Geometry::MAX_POLY_POINTS * 2 + 1)
         {  argv[argc] = argv2[i];
            argc++;
         }
      }
   }

   if(argc < 6)
      return false;

   if(argc & 1)   // Odd number of arg count (7,9,11) to allow optional slipAmount arg
   {
      slipAmount = (F32)atof(argv[0]);
      readGeom(argc, argv, 1, game->getLegacyGridSize());
   }
   else           // Even number of arg count (6,8,10)
      readGeom(argc, argv, 0, game->getLegacyGridSize());

   updateExtentInDatabase();

   return true;
}


const char *SlipZone::getEditorHelpString()
{
   return "Areas of higher than normal inertia.";
}


const char *SlipZone::getPrettyNamePlural()
{
   return "Inertia zones";
}


const char *SlipZone::getOnDockName()
{
   return "Inertia";
}


const char *SlipZone::getOnScreenName()
{
   return "Inertia";
}


string SlipZone::toLevelCode() const
{
   return string(appendId(getClassName())) + " " + ftos(slipAmount, 3) + " " + geomToLevelCode();
}


void SlipZone::onAddedToGame(Game *theGame)
{
   Parent::onAddedToGame(theGame);

   if(!isGhost())
      setScopeAlways();
}


const Vector<Point> *SlipZone::getCollisionPoly() const
{
   return getOutline();
}


bool SlipZone::collide(BfObject *hitObject) 
{
   if(!isGhost() && isShipType(hitObject->getObjectTypeNumber()))
   {
      //logprintf("IN A SLIP ZONE!!");
   }
   return false;
}


U32 SlipZone::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   packGeom(connection, stream);
   stream->write(slipAmount);
   return 0;
}


void SlipZone::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   unpackGeom(connection, stream);
   stream->read(&slipAmount);
}


TNL_IMPLEMENT_NETOBJECT(SlipZone);


};
