#pragma once

#define USE_SQUIRREL 1

#if USE_SQUIRREL

struct SQVM;
class SquirrelBindings
{
public:
	static void Bind( SQVM* a_SquirrelVM );
	static bool IsBound;
};

#endif