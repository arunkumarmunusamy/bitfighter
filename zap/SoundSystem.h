/*
 * SoundSystem.h
 *
 *  Created on: May 8, 2011
 *      Author: dbuck
 */

#ifndef SOUNDSYSTEM_H_
#define SOUNDSYSTEM_H_

#include "ConfigEnum.h"     // For sfxSets

#ifndef ZAP_DEDICATED
#include "alInclude.h"
#include "../alure/AL/alure.h"
#else
class alureStream;
#endif

#include "tnlTypes.h"
#include "tnlVector.h"

#include <string>

// forward declarations
typedef unsigned int ALuint;

namespace TNL {
   template <class T> class RefPtr;
   class ByteBuffer;
   typedef RefPtr<ByteBuffer> ByteBufferPtr;
};

using namespace TNL;
using namespace std;

namespace Zap {

// forward declarations
class Point;
class SoundEffect;
typedef RefPtr<SoundEffect> SFXHandle;

// Must keep this aligned with sfxProfilesModern[] and sfxProfilesClassic[]
enum SFXProfiles
{
   // Utility sounds
   SFXVoice,
   SFXNone,

   SFXPlayerJoined,
   SFXPlayerLeft,

   // Weapon noises
   SFXPhaserProjectile,
   SFXPhaserImpact,
   SFXBounceProjectile,
   SFXBounceImpact,
   SFXTripleProjectile,
   SFXTripleImpact,
   SFXTurretProjectile,
   SFXTurretImpact,

   SFXGrenadeProjectile,

   SFXMineDeploy,
   SFXMineArm,
   SFXMineExplode,

   SFXSpyBugDeploy,
   SFXSpyBugExplode,

   SFXAsteroidExplode,

   // Ship noises
   SFXShipExplode,
   SFXShipHeal,
   SFXShipBoost,
   SFXShipHit,    // Ship is hit by a projectile

   SFXBounceWall,
   SFXBounceObject,
   SFXBounceShield,

   SFXShieldActive,
   SFXSensorActive,
   SFXRepairActive,
   SFXCloakActive,

   // Flag noises
   SFXFlagCapture,
   SFXFlagDrop,
   SFXFlagReturn,
   SFXFlagSnatch,

   // Teleport noises
   SFXTeleportIn,
   SFXTeleportOut,

   SFXGoFastOutside,
   SFXGoFastInside,

   // Forcefield noises
   SFXForceFieldUp,
   SFXForceFieldDown,

   // UI noises
   SFXUIBoop,
   SFXUICommUp,
   SFXUICommDown,
   SFXIncomingMessage,

   // Other
   SFXCoreHeartbeat,
   SFXCoreExplode,
   SFXCoreExplodeSecondary,

   NumSFXBuffers     // Count of the number of SFX sounds we have
};

enum MusicState {
   MusicPlaying,
   MusicStopped,
   MusicPaused
};

enum MusicInfoType {
   MusicTypeMenu = 0,
   MusicTypeGame,
   MaxMusicTypes,
};

struct MusicInfo
{
   MusicInfoType type;
   MusicState state;
   ALuint source;
   alureStream* stream;
};


class SoundSystem
{
private:
   static const S32 NumMusicStreamBuffers = 3;
   static const S32 MusicChunkSize = 250000;
   static const S32 NumVoiceChatBuffers = 32;
   static const S32 NumSamples = 16;

   // Sound Effect functions
   static void updateGain(SFXHandle& effect, F32 sfxVolLevel, F32 voiceVolLevel);
   static void playOnSource(SFXHandle& effect, F32 sfxVol, F32 voiceVol);

   static void game_music_end_callback(void* userData, ALuint source);
   static void menu_music_end_callback(void* userData, ALuint source);

   static string mMusicDir;
   static string mMenuMusicFile;

   static bool mMenuMusicValid;
   static bool mGameMusicValid;

   static Vector<string> mMusicList;
   static S32 mCurrentlyPlayingIndex;

   static bool musicSystemValid();

public:
   SoundSystem();
   virtual ~SoundSystem();

   // General functions
   static void init(sfxSets sfxSet, const string &sfxDir, const string &musicDir, float musicVol);
   static void shutdown();
   static void setListenerParams(Point pos, Point velocity);
   static void processAudio(F32 sfxVol, F32 musicVol, F32 voiceVol);

   // Sound Effect functions
   static void processSoundEffects(F32 sfxVol, F32 voiceVol);
   static SFXHandle playSoundEffect(U32 profileIndex, F32 gain = 1.0f);
   static SFXHandle playSoundEffect(U32 profileIndex, Point position);
   static SFXHandle playSoundEffect(U32 profileIndex, Point position, Point velocity, F32 gain = 1.0f);
   static void playSoundEffect(const SFXHandle &effect);
   static SFXHandle playRecordedBuffer(ByteBufferPtr p, F32 gain);
   static void stopSoundEffect(SFXHandle &effect);
   static void unqueueBuffers(S32 sourceIndex);
   static void setMovementParams(SFXHandle& effect, Point position, Point velocity);
   static void updateMovementParams(SFXHandle& effect);

   // Voice Chat functions
   static void processVoiceChat();
   static void queueVoiceChatBuffer(const SFXHandle &effect, ByteBufferPtr p);
   static bool startRecording();
   static void captureSamples(ByteBufferPtr sampleBuffer);
   static void stopRecording();

   // Music functions
   static void processMusic(F32 newMusicVolLevel);
   static void playGameMusic();
   static void playMenuMusic();
   static void stopMusic(MusicInfo &musicInfo);
   static void pauseMusic(MusicInfo &musicInfo);
   static void resumeMusic(MusicInfo &musicInfo);
};

}

#endif /* SOUNDSYSTEM_H_ */
