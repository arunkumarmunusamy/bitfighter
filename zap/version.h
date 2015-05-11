//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _VERSION_H_
#define _VERSION_H_

#define ZAP_GAME_NAME "Bitfighter"

// Updated from 7 for 019
// Updated to 8 for 019a -- added master-generated IDs written after connect
#define MASTER_PROTOCOL_VERSION 8  // Change this when releasing an incompatible cm/sm protocol (must be int)
                                   // MASTER_PROTOCOL_VERSION = 4, client 015a and older (CS_PROTOCOL_VERSION <= 32) can not connect to our new master.

#define CS_PROTOCOL_VERSION 38     // Change this when releasing an incompatible cs protocol (must be int)
// 016 = 33 
// 017[ab] = 35
// 018[a] = 36
// 019 dev = 37
// 019 = 38

#define VERSION_016  3737
#define VERSION_017  4252
#define VERSION_017a 4265
#define VERSION_017b 4537
#define VERSION_018  6059
#define VERSION_018a 6800
#define VERSION_019  8814
#define VERSION_019a 9459
#define VERSION_019b 9517
#define VERSION_019c 9708
#define VERSION_019d 10314
#define VERSION_019e 10572

#define BUILD_VERSION VERSION_019e // Version of the game according to hg, will be unique every release (must be int)
                                   // Get from "hg summary"

#define ZAP_GAME_RELEASE "019e"    // Change this with every release -- for display purposes only, string,
                                   // will also be used for name of installer on windows, so be careful with spaces  
                                   // Used for GameRecorder.cpp, buildGameRecorderExtension

// ZAP_GAME_RELEASE_LONGSTRING is used for version display at main menu
#define ZAP_GAME_RELEASE_LONGSTRING ("Release " ZAP_GAME_RELEASE)
#define ZAP_GAME_DEV_LONGSTRING (ZAP_GAME_RELEASE " development")

#endif

