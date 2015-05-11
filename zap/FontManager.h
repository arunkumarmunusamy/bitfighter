//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _FONT_MANAGER_H_
#define _FONT_MANAGER_H_

#include "tnlTypes.h"
#include "freeglut_stroke.h"   // Our stroke font handler

#include "FontContextEnum.h"
#include "RenderManager.h"

#include <string>

using namespace TNL;
using namespace std;

struct sth_stash;

namespace Zap
{

class BfFont;
class GameSettings;

class FontManager: RenderManager
{

private:
   static sth_stash *mStash;
   static bool mUsingExternalFonts;

   static BfFont *getFont(FontId currentFontId);

   static F32 getStrokeFontStringLength(const SFG_StrokeFont *font, const char* string);
   static F32 getTtfFontStringLength(BfFont *font, const char* string);

public:
   FontManager();          // Constructor
   virtual ~FontManager(); // Destructor

   static void initialize(GameSettings *settings, bool useExternalFonts = true);
   static void reinitialize(GameSettings *settings);
   static void cleanup();

   static sth_stash *getStash();

   static void drawTTFString(BfFont *font, const char *string, F32 size);
   static void drawStrokeCharacter(const SFG_StrokeFont *font, S32 character);

   static F32 getStringLength(const char* string);

   static void renderString(F32 size, const char *string);

   static void setFont(FontId fontId);
   static void setFontContext(FontContext fontContext);

   static void pushFontContext(FontContext fontContext);
   static void popFontContext();
};


////////////////////////////////////////
////////////////////////////////////////

class BfFont 
{
private:
   bool mIsStrokeFont;
   bool mOk;

   S32 mStashFontId;

   const SFG_StrokeFont *mStrokeFont;     // Will be NULL for TTF fonts
   static const char *SystemFontDirectories[];

public:
   BfFont(const ::SFG_StrokeFont *strokeFont);              // Stroke font constructor
   BfFont(const string &fontFile, GameSettings *settings);  // TTF font constructor
   virtual ~BfFont();                                       // Destructor

   const SFG_StrokeFont *getStrokeFont();
   bool isStrokeFont();
   S32 getStashFontId();

};


};

#endif

