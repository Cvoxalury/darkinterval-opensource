#include "shaderlib/cshader.h"
class di_cable_vs20_Static_Index
{
public:
	di_cable_vs20_Static_Index( )
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
#define shaderStaticTest_di_cable_vs20 0
class di_cable_vs20_Dynamic_Index
{
private:
	int m_nDOWATERFOG;
#ifdef _DEBUG
	bool m_bDOWATERFOG;
#endif
public:
	void SetDOWATERFOG( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDOWATERFOG = i;
#ifdef _DEBUG
		m_bDOWATERFOG = true;
#endif
	}
	void SetDOWATERFOG( bool i )
	{
		m_nDOWATERFOG = i ? 1 : 0;
#ifdef _DEBUG
		m_bDOWATERFOG = true;
#endif
	}
public:
	di_cable_vs20_Dynamic_Index()
	{
#ifdef _DEBUG
		m_bDOWATERFOG = false;
#endif // _DEBUG
		m_nDOWATERFOG = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bDOWATERFOG;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nDOWATERFOG ) + 0;
	}
};
#define shaderDynamicTest_di_cable_vs20 vsh_forgot_to_set_dynamic_DOWATERFOG + 0
