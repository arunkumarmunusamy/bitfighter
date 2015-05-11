//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

// Hashing functions based on code written by Benjamin Gr�delbach, released Feb. 2005
// Rewritten to better fit with Bitfighter, and move all functions from class to namespace.
// Moved main functionality into IncrementalHasher object.

#ifndef MD5_UTILS_H
#define MD5_UTILS_H

#include <tomcrypt.h>
#include <string>

using std::string;

namespace Md5
{

class IncrementalHasher 
{
private:
   hash_state mHashState;

public:
   IncrementalHasher();
   void add(const string &line);
   string getHash();
};


// Creates a MD5 hash from "text" and returns it as string
string getHashFromString(const string &text);

// Gets hash with appended salt, and makes text lowercase for case insensitivity
string getSaltedHashFromString(const string &text);

// Creates an MD5 hash from the specified file and returns it as string
string getHashFromFile(const string &filename);

}

#endif
