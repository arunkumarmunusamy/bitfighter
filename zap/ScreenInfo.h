//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef SCREENINFO_H_
#define SCREENINFO_H_

#include "tnlTypes.h"
#include "Point.h"
#include "ConfigEnum.h"

#ifndef ZAP_DEDICATED
#  include "SDL.h"      // For SDL_VERSION_ATLEAST macro
#endif


using namespace TNL;

namespace Zap {


////////////////////////////////////////
////////////////////////////////////////

class ScreenInfo
{
private:
   static const S32 GAME_WIDTH  = 800;
   static const S32 GAME_HEIGHT = 600;

   F32 MIN_SCALING_FACTOR;       // Limits minimum window size

   Point mWindowMousePos, mCanvasMousePos;

   S32 mPhysicalScreenWidth, mPhysicalScreenHeight;
   S32 mGameCanvasWidth, mGameCanvasHeight;     // Size of screen; in game, will always be 800x600, but may be different in editor fullscreen
   S32 mPrevCanvasWidth, mPrevCanvasHeight;     // Previous size of screen
   S32 mWindowWidth, mWindowHeight;             // Window dimensions in physical pixels
   F32 mScalingRatioX, mScalingRatioY;          // Ratio of physical pixels to virtual pixels
   bool mIsLandscape;                           // Is our screen landscape or portrait?

   bool mActualized;                            // True once screen has been actualized

   F32 mPixelRatio;                             // Number of physical pixels that are used to draw a single virtual pixel
   void calcPixelRatio();

public:
   ScreenInfo();      // Constructor
   virtual ~ScreenInfo();

   F32 getMinScalingFactor();

   // Can't initialize until SDL has been set up
   void init(S32 physicalScreenWidth, S32 physicalScreenHeight);

   void setWindowSize(S32 width, S32 height);
   S32 getWindowWidth() const;
   S32 getWindowHeight() const;

   // The following methods return values in PHYSICAL pixels -- how large is the entire physical monitor?
   S32 getPhysicalScreenWidth() const;
   S32 getPhysicalScreenHeight() const;

   // Game canvas size in physical pixels, assuming full screen unstretched mode
   S32 getDrawAreaWidth() const;
   S32 getDrawAreaHeight() const;

   // This is the number of physical pixels that are used to draw a single virtual pixel -- larger windows will have larger pixelRaios
   F32 getPixelRatio() const;
   F32 getScalingRatio() const;

   // Dimensions of black bars in physical pixels in full-screen unstretched mode.  Does not reflect current window mode
   S32 getHorizPhysicalMargin() const;
   S32 getVertPhysicalMargin() const;

   // Dimensions of black bars in physical pixes, based on current window mode
   S32 getHorizPhysicalMargin(DisplayMode mode) const;
   S32 getVertPhysicalMargin (DisplayMode mode) const;

   // The following methods return values in VIRTUAL pixels, not accurate in editor
   void setGameCanvasSize(S32 width, S32 height);
   void resetGameCanvasSize();

   S32 getDefaultCanvasWidth() const;
   S32 getDefaultCanvasHeight() const;

   S32 getGameCanvasWidth() const;
   S32 getGameCanvasHeight() const;

   S32 getPrevCanvasWidth() const;
   S32 getPrevCanvasHeight() const;

   // Dimensions of black bars in game-sized pixels
   S32 getHorizDrawMargin() const;
   S32 getVertDrawMargin() const;

   bool isLandscape() const;     // Whether physical screen is landscape, or at least more landscape than our game window

   // Convert physical window screen coordinates into virtual, in-game coordinate
   Point convertWindowToCanvasCoord(S32 x, S32 y, DisplayMode mode);

   Point convertCanvasToWindowCoord(S32 x, S32 y, DisplayMode mode) const;
   Point convertCanvasToWindowCoord(F32 x, F32 y, DisplayMode mode) const;

   void setMousePos      (S32 x, S32 y, DisplayMode mode);
   void setCanvasMousePos(S32 x, S32 y, DisplayMode mode);

   const Point *getMousePos();
   const Point *getWindowMousePos();

   bool isActualized();
   void setActualized();

   // SDL information
#ifndef ZAP_DEDICATED
#  if SDL_VERSION_ATLEAST(2,0,0)
      SDL_Window *sdlWindow;
      SDL_GLContext *sdlGlContext;
#  endif
#endif

};

};

#endif /* SCREENINFO_H_ */
