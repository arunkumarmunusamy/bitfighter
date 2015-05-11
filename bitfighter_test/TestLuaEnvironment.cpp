//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "TestUtils.h"

#include "ServerGame.h"
#include "gameType.h"
#include "Level.h"
#include "luaLevelGenerator.h"
#include "SystemFunctions.h"

#include "gtest/gtest.h"

namespace Zap
{

using namespace std;
using namespace TNL;

class LuaEnvironmentTest : public testing::Test {
protected:
   ServerGame *serverGame;
   GameSettingsPtr settings;  // Will be cleaned up automatically

   lua_State *L;

   LuaLevelGenerator *levelgen;
   GamePair pair;

   LuaEnvironmentTest() : pair(GamePair("", 0))    // Start with an empty level and no clients
   {
      int x = 0;
   }


   virtual void SetUp() 
   {
      serverGame = pair.server;
      settings = serverGame->getSettingsPtr();

      ASSERT_EQ(0, serverGame->getLevel()->findObjects_fast()->size()) << 
                "Database should be empty on a new level with no clients!";

      // Check that the environment was set up during construction of GamePair
      ASSERT_TRUE(LuaScriptRunner::getL());

      // Set up a levelgen object, with no script
      levelgen = new LuaLevelGenerator(serverGame);

      // Ensure environment set-up
      ASSERT_TRUE(levelgen->prepareEnvironment());

      // Grab our Lua state
      L = LuaScriptRunner::getL();
   }


   virtual void TearDown()
   {
      delete levelgen;
      LuaScriptRunner::shutdown();
   }


   bool existsFunctionInEnvironment(const string &functionName)
   {
      return LuaScriptRunner::loadFunction(L, levelgen->getScriptId(), functionName.c_str());
   }
};


TEST_F(LuaEnvironmentTest, sanityCheck)
{
   // Test exception throwing -- for some reason, test triggers SEH exception if code is passed directly
   string code = "a = b.b";      // Illegal code
   EXPECT_FALSE(levelgen->runString(code));
}


TEST_F(LuaEnvironmentTest, sandbox)
{
   // Ensure that local setmetatable refs in sandbox are not globalized somehow
   EXPECT_FALSE(existsFunctionInEnvironment("smt"));
   EXPECT_FALSE(existsFunctionInEnvironment("gmt"));

   // Sandbox prohibits access to unsafe functions, a few listed here
   EXPECT_FALSE(existsFunctionInEnvironment("setfenv"));
   EXPECT_FALSE(existsFunctionInEnvironment("setmetatable"));

   // But it should not interfere with permitted functions
   EXPECT_TRUE(existsFunctionInEnvironment("unpack"));
   EXPECT_TRUE(existsFunctionInEnvironment("ipairs"));
   EXPECT_TRUE(existsFunctionInEnvironment("require"));
}


TEST_F(LuaEnvironmentTest, scriptIsolation)
{
   LuaLevelGenerator levelgen2(serverGame);
   levelgen2.prepareEnvironment();
   lua_State *L = levelgen->getL();

   // All scripts should have separate environment tables
   lua_getfield(L, LUA_REGISTRYINDEX, levelgen->getScriptId());
   lua_getfield(L, LUA_REGISTRYINDEX, levelgen2.getScriptId());
   EXPECT_FALSE(lua_equal(L, -1, -2));
   lua_pop(L, 2);

   // Scripts can mess with their own environment, but not others'
   EXPECT_TRUE(levelgen->runString("levelgen = nil"));
   EXPECT_TRUE(levelgen->runString("assert(levelgen == nil)"));
   EXPECT_TRUE(levelgen2.runString("assert(levelgen ~= nil)"));

   EXPECT_TRUE(levelgen->runString("BfObject= nil"));
   EXPECT_TRUE(levelgen->runString("assert(BfObject== nil)"));
   EXPECT_TRUE(levelgen2.runString("assert(BfObject~= nil)"));

   /* A true deep copy is needed before these will pass
   EXPECT_TRUE(levelgen->runString("Timer.foo = 'test'"));
   EXPECT_TRUE(levelgen->runString("assert(Timer.foo == 'test')"));
   EXPECT_TRUE(levelgen2.runString("assert(Timer.foo ~= 'test')"));
   */
}


TEST_F(LuaEnvironmentTest, immutability)
{
   EXPECT_FALSE(levelgen->runString("string.sub = nil"));
}


TEST_F(LuaEnvironmentTest, findAllObjects)
{
   EXPECT_TRUE(levelgen->runString("t = bf:findAllObjects()"));
   ASSERT_TRUE(levelgen->runString("assert(#t == 0)"));

   // Level will have 3 items: 2 ResourceItems, and one TestItem
   EXPECT_TRUE(levelgen->runString("bf:addItem(ResourceItem.new(point.new(0,0)))"));
   EXPECT_TRUE(levelgen->runString("bf:addItem(ResourceItem.new(point.new(300,300)))"));
   EXPECT_TRUE(levelgen->runString("bf:addItem(TestItem.new(point.new(200,200)))"));

   EXPECT_TRUE(levelgen->runString("t = { }"));
   EXPECT_TRUE(levelgen->runString("assert(#t == 0)"));
   EXPECT_TRUE(levelgen->runString("t = bf:findAllObjects()"));
   EXPECT_TRUE(levelgen->runString("assert(#t == 3)"));

   EXPECT_TRUE(levelgen->runString("t = bf:findAllObjects(ObjType.ResourceItem)"));
   EXPECT_TRUE(levelgen->runString("assert(#t == 2)"));
   EXPECT_TRUE(levelgen->runString("t = bf:findAllObjects(ObjType.ResourceItem)"));

   EXPECT_TRUE(levelgen->runString("t = bf:findAllObjects()"));
   EXPECT_TRUE(levelgen->runString("assert(#t == 3)"));
   EXPECT_TRUE(levelgen->runString("t = bf:findAllObjects(ObjType.ResourceItem)"));
   EXPECT_TRUE(levelgen->runString("assert(#t == 2)")) << "t had 3 items, but should have been cleared before adding 2 more";
   EXPECT_TRUE(levelgen->runString("t = bf:findAllObjects(ObjType.ResourceItem)"));
   EXPECT_TRUE(levelgen->runString("assert(#t == 2)"));
}


};
