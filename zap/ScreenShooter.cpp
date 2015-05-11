//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "ScreenShooter.h"

#ifndef BF_NO_SCREENSHOTS

#include "UI.h"
#include "UIEditor.h"
#include "UIManager.h"
#include "DisplayManager.h"
#include "ClientGame.h"
#include "VideoSystem.h"   // For setting screen geom vars
#include "stringUtils.h"

#include "zlib.h"

using namespace std;

namespace Zap
{

ScreenShooter::ScreenShooter()
{
   // Do nothing
}


ScreenShooter::~ScreenShooter()
{
   // Do nothing
}


void ScreenShooter::resizeViewportToCanvas(UIManager *uiManager)
{
   // Grab the canvas width/height and normalize our screen to it
   S32 width = DisplayManager::getScreenInfo()->getGameCanvasWidth();
   S32 height = DisplayManager::getScreenInfo()->getGameCanvasHeight();

   mGL->glViewport(0, 0, width, height);

   mGL->glMatrixMode(GLOPT::Projection);
   mGL->glLoadIdentity();

   mGL->glOrtho(0, width, height, 0, 0, 1);

   mGL->glMatrixMode(GLOPT::Modelview);
   mGL->glLoadIdentity();

   mGL->glScissor(0, 0, width, height);

   // Now render a frame to draw our new viewport to the back buffer
   mGL->glClear(GLOPT::ColorBufferBit);   // Not sure why this is needed
   uiManager->renderCurrent();
}


// Stolen from VideoSystem::actualizeScreenMode()
void ScreenShooter::restoreViewportToWindow(GameSettings *settings)
{
   DisplayMode displayMode = settings->getSetting<DisplayMode>(IniKey::WindowMode);

   // Set up video/window flags amd parameters and get ready to change the window
   S32 sdlWindowWidth, sdlWindowHeight;
   F64 orthoLeft, orthoRight, orthoTop, orthoBottom;

   VideoSystem::getWindowParameters(settings, displayMode, sdlWindowWidth, sdlWindowHeight, orthoLeft, orthoRight, orthoTop, orthoBottom);

   mGL->glViewport(0, 0, sdlWindowWidth, sdlWindowHeight);

   mGL->glMatrixMode(GLOPT::Projection);
   mGL->glLoadIdentity();

   mGL->glOrtho(orthoLeft, orthoRight, orthoBottom, orthoTop, 0, 1);

   mGL->glMatrixMode(GLOPT::Modelview);
   mGL->glLoadIdentity();

   // Now scissor
   if(displayMode == DISPLAY_MODE_FULL_SCREEN_UNSTRETCHED)
   {
      mGL->glScissor(DisplayManager::getScreenInfo()->getHorizPhysicalMargin(), // x
            DisplayManager::getScreenInfo()->getVertPhysicalMargin(),      // y
            DisplayManager::getScreenInfo()->getDrawAreaWidth(),           // width
            DisplayManager::getScreenInfo()->getDrawAreaHeight());         // height
   }
   else
      mGL->glScissor(0, 0, DisplayManager::getScreenInfo()->getWindowWidth(), DisplayManager::getScreenInfo()->getWindowHeight());
}



// Thanks to the good developers of naev for excellent code to base this off of.
// Much was copied directly.
void ScreenShooter::saveScreenshot(UIManager *uiManager, GameSettings *settings, string filename)
{
   string folder = settings->getFolderManager()->getScreenshotDir();

   // Let's find a filename to use
   makeSureFolderExists(folder);

   string fullFilename;
   S32 ctr = 0;

   if(filename == "")
   {
      while(true)    // Settle in for the long haul, boys.  This seems crazy...
      {
         fullFilename = joindir(folder, "screenshot_" + itos(ctr++) + ".png");

         if(!fileExists(fullFilename))
            break;
      }
   }
   else
      fullFilename = joindir(folder, filename + ".png");

   // We default to resizing the opengl viewport to the standard canvas size, unless we're
   // in the editor or our window is smaller than the canvas size
   bool doResize = (!uiManager->isCurrentUI<EditorUserInterface>()) &&
                     DisplayManager::getScreenInfo()->getWindowWidth() > DisplayManager::getScreenInfo()->getGameCanvasWidth();

   // Change opengl viewport temporarily to have consistent screenshot sizes
   if(doResize)
      resizeViewportToCanvas(uiManager);

   // Now let's grab them pixels
   S32 width;
   S32 height;

   // If we're resizing, use the default canvas size
   if(doResize)
   {
      width  = DisplayManager::getScreenInfo()->getGameCanvasWidth();
      height = DisplayManager::getScreenInfo()->getGameCanvasHeight();
   }
   // Otherwise just take the window size
   else
   {
      width  = DisplayManager::getScreenInfo()->getWindowWidth();
      height = DisplayManager::getScreenInfo()->getWindowHeight();
   }

   // Allocate buffer  GLubyte == U8
   U8 *screenBuffer = new U8[BytesPerPixel * width * height];  // Glubyte * 3 = 24 bits
   png_bytep *rows = new png_bytep[height];

   // Set alignment at smallest for compatibility
   mGL->glPixelStore(GLOPT::PackAlignment, 1);

   // Grab the front buffer with the new viewport
#ifndef BF_USE_GLES
   // GLES doesn't need this?
   mGL->glReadBuffer(GLOPT::Back);
#endif

   // Read pixels from buffer - slow operation
   mGL->glReadPixels(0, 0, width, height, GLOPT::Rgb, GLOPT::UnsignedByte, screenBuffer);

   // Change opengl viewport back to what it was
   if(doResize)
      restoreViewportToWindow(settings);

   // Convert Data
   for (S32 i = 0; i < height; i++)
      rows[i] = &screenBuffer[(height - i - 1) * (BytesPerPixel * width)];  // Backwards!

   // Write the PNG!
   if(!writePNG(fullFilename.c_str(), rows, width, height, PNG_COLOR_TYPE_RGB, BitDepth))
      logprintf(LogConsumer::LogError, "Creating screenshot failed!!");

   // Clean up
   delete [] rows;
   delete [] screenBuffer;
}

// We have created FILE* pointer, we write FILE* pointer here, not in libpng14.dll
// This is to avoid conflicts with different FILE struct between compilers.
// alternative is to make FILE* pointer get created, written and closed entirely inside libpng14.dll
static void PNGAPI png_user_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
   FILE *f = (png_FILE_p)(png_get_io_ptr(png_ptr));
   png_size_t check = fwrite(data, 1, length, f);
   if (check != length)
      png_error(png_ptr, "Write Error");
}


// Saves a PNG
bool ScreenShooter::writePNG(const char *file_name, png_bytep *rows, S32 width, S32 height, S32 colorType, S32 bitDepth)
{
   png_structp pngContainer;
   png_infop pngInfo;
   FILE *file;

   // Open file for writing
   if (!(file = fopen(file_name, "wb"))) {
      logprintf(LogConsumer::LogError, "Unable to open '%s' for writing.", file_name);
      return false;
   }

   // Build out PNG container
   if (!(pngContainer = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
      logprintf(LogConsumer::LogError, "Unable to create PNG container.");
      fclose(file);
      return false;
   }

   // Build out PNG information
   pngInfo = png_create_info_struct(pngContainer);

   // Set the image details
   //png_init_io(pngContainer, file); // Having fwrite inside libpng14.dll (and fopen outside of .dll) can be crashy...
   png_set_write_fn(pngContainer, file, &png_user_write_data, NULL);

   png_set_compression_level(pngContainer, Z_DEFAULT_COMPRESSION);
   png_set_IHDR(pngContainer, pngInfo, width, height, bitDepth, colorType,
         PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
         PNG_FILTER_TYPE_DEFAULT);

   // Write the image
   png_write_info(pngContainer, pngInfo);
   png_write_image(pngContainer, rows);
   png_write_end(pngContainer, NULL);

   // Destroy memory allocated to the PNG container
   png_destroy_write_struct(&pngContainer, &pngInfo);

   // Close the file
   fclose(file);

   return true;
}

} /* namespace Zap */

#endif // BF_NO_SCREENSHOTS
