#include "shaderlib/cshader.h"
class sky_hdr_vs20_Static_Index
{
public:
	sky_hdr_vs20_Static_Index( )
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
#endif // _DEBUG
		return 0;
	}
};
#define shaderStaticTest_sky_hdr_vs20 0
class sky_hdr_vs20_Dynamic_Index
{
public:
	sky_hdr_vs20_Dynamic_Index()
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
#endif // _DEBUG
		return 0;
	}
};
#define shaderDynamicTest_sky_hdr_vs20 0
