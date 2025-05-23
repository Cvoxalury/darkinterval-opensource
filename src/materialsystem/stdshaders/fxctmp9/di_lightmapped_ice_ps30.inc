#include "shaderlib/cshader.h"
class di_lightmapped_ice_ps30_Static_Index
{
private:
	int m_nBACK_SURFACE;
#ifdef _DEBUG
	bool m_bBACK_SURFACE;
#endif
public:
	void SetBACK_SURFACE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBACK_SURFACE = i;
#ifdef _DEBUG
		m_bBACK_SURFACE = true;
#endif
	}
	void SetBACK_SURFACE( bool i )
	{
		m_nBACK_SURFACE = i ? 1 : 0;
#ifdef _DEBUG
		m_bBACK_SURFACE = true;
#endif
	}
private:
	int m_nLIGHT_WARP;
#ifdef _DEBUG
	bool m_bLIGHT_WARP;
#endif
public:
	void SetLIGHT_WARP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLIGHT_WARP = i;
#ifdef _DEBUG
		m_bLIGHT_WARP = true;
#endif
	}
	void SetLIGHT_WARP( bool i )
	{
		m_nLIGHT_WARP = i ? 1 : 0;
#ifdef _DEBUG
		m_bLIGHT_WARP = true;
#endif
	}
private:
	int m_nFRESNEL_WARP;
#ifdef _DEBUG
	bool m_bFRESNEL_WARP;
#endif
public:
	void SetFRESNEL_WARP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFRESNEL_WARP = i;
#ifdef _DEBUG
		m_bFRESNEL_WARP = true;
#endif
	}
	void SetFRESNEL_WARP( bool i )
	{
		m_nFRESNEL_WARP = i ? 1 : 0;
#ifdef _DEBUG
		m_bFRESNEL_WARP = true;
#endif
	}
private:
	int m_nHIGH_PRECISION_DEPTH;
#ifdef _DEBUG
	bool m_bHIGH_PRECISION_DEPTH;
#endif
public:
	void SetHIGH_PRECISION_DEPTH( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nHIGH_PRECISION_DEPTH = i;
#ifdef _DEBUG
		m_bHIGH_PRECISION_DEPTH = true;
#endif
	}
	void SetHIGH_PRECISION_DEPTH( bool i )
	{
		m_nHIGH_PRECISION_DEPTH = i ? 1 : 0;
#ifdef _DEBUG
		m_bHIGH_PRECISION_DEPTH = true;
#endif
	}
private:
	int m_nINTERIOR_LAYER;
#ifdef _DEBUG
	bool m_bINTERIOR_LAYER;
#endif
public:
	void SetINTERIOR_LAYER( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nINTERIOR_LAYER = i;
#ifdef _DEBUG
		m_bINTERIOR_LAYER = true;
#endif
	}
	void SetINTERIOR_LAYER( bool i )
	{
		m_nINTERIOR_LAYER = i ? 1 : 0;
#ifdef _DEBUG
		m_bINTERIOR_LAYER = true;
#endif
	}
private:
	int m_nOPACITY_TEXTURE;
#ifdef _DEBUG
	bool m_bOPACITY_TEXTURE;
#endif
public:
	void SetOPACITY_TEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nOPACITY_TEXTURE = i;
#ifdef _DEBUG
		m_bOPACITY_TEXTURE = true;
#endif
	}
	void SetOPACITY_TEXTURE( bool i )
	{
		m_nOPACITY_TEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bOPACITY_TEXTURE = true;
#endif
	}
private:
	int m_nFLASHLIGHTDEPTHFILTERMODE;
#ifdef _DEBUG
	bool m_bFLASHLIGHTDEPTHFILTERMODE;
#endif
public:
	void SetFLASHLIGHTDEPTHFILTERMODE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nFLASHLIGHTDEPTHFILTERMODE = i;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = true;
#endif
	}
	void SetFLASHLIGHTDEPTHFILTERMODE( bool i )
	{
		m_nFLASHLIGHTDEPTHFILTERMODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = true;
#endif
	}
private:
	int m_nCONTACT_SHADOW;
#ifdef _DEBUG
	bool m_bCONTACT_SHADOW;
#endif
public:
	void SetCONTACT_SHADOW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCONTACT_SHADOW = i;
#ifdef _DEBUG
		m_bCONTACT_SHADOW = true;
#endif
	}
	void SetCONTACT_SHADOW( bool i )
	{
		m_nCONTACT_SHADOW = i ? 1 : 0;
#ifdef _DEBUG
		m_bCONTACT_SHADOW = true;
#endif
	}
public:
	di_lightmapped_ice_ps30_Static_Index( )
	{
#ifdef _DEBUG
		m_bBACK_SURFACE = false;
#endif // _DEBUG
		m_nBACK_SURFACE = 0;
#ifdef _DEBUG
		m_bLIGHT_WARP = false;
#endif // _DEBUG
		m_nLIGHT_WARP = 0;
#ifdef _DEBUG
		m_bFRESNEL_WARP = false;
#endif // _DEBUG
		m_nFRESNEL_WARP = 0;
#ifdef _DEBUG
		m_bHIGH_PRECISION_DEPTH = false;
#endif // _DEBUG
		m_nHIGH_PRECISION_DEPTH = 0;
#ifdef _DEBUG
		m_bINTERIOR_LAYER = false;
#endif // _DEBUG
		m_nINTERIOR_LAYER = 0;
#ifdef _DEBUG
		m_bOPACITY_TEXTURE = false;
#endif // _DEBUG
		m_nOPACITY_TEXTURE = 0;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = false;
#endif // _DEBUG
		m_nFLASHLIGHTDEPTHFILTERMODE = 0;
#ifdef _DEBUG
		m_bCONTACT_SHADOW = false;
#endif // _DEBUG
		m_nCONTACT_SHADOW = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bBACK_SURFACE && m_bLIGHT_WARP && m_bFRESNEL_WARP && m_bHIGH_PRECISION_DEPTH && m_bINTERIOR_LAYER && m_bOPACITY_TEXTURE && m_bFLASHLIGHTDEPTHFILTERMODE && m_bCONTACT_SHADOW;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 20 * m_nBACK_SURFACE ) + ( 40 * m_nLIGHT_WARP ) + ( 80 * m_nFRESNEL_WARP ) + ( 160 * m_nHIGH_PRECISION_DEPTH ) + ( 320 * m_nINTERIOR_LAYER ) + ( 640 * m_nOPACITY_TEXTURE ) + ( 1280 * m_nFLASHLIGHTDEPTHFILTERMODE ) + ( 3840 * m_nCONTACT_SHADOW ) + 0;
	}
};
#define shaderStaticTest_di_lightmapped_ice_ps30 psh_forgot_to_set_static_BACK_SURFACE + psh_forgot_to_set_static_LIGHT_WARP + psh_forgot_to_set_static_FRESNEL_WARP + psh_forgot_to_set_static_HIGH_PRECISION_DEPTH + psh_forgot_to_set_static_INTERIOR_LAYER + psh_forgot_to_set_static_OPACITY_TEXTURE + psh_forgot_to_set_static_FLASHLIGHTDEPTHFILTERMODE + psh_forgot_to_set_static_CONTACT_SHADOW + 0
class di_lightmapped_ice_ps30_Dynamic_Index
{
private:
	int m_nNUM_LIGHTS;
#ifdef _DEBUG
	bool m_bNUM_LIGHTS;
#endif
public:
	void SetNUM_LIGHTS( int i )
	{
		Assert( i >= 0 && i <= 4 );
		m_nNUM_LIGHTS = i;
#ifdef _DEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
	void SetNUM_LIGHTS( bool i )
	{
		m_nNUM_LIGHTS = i ? 1 : 0;
#ifdef _DEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
private:
	int m_nFLASHLIGHT;
#ifdef _DEBUG
	bool m_bFLASHLIGHT;
#endif
public:
	void SetFLASHLIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLASHLIGHT = i;
#ifdef _DEBUG
		m_bFLASHLIGHT = true;
#endif
	}
	void SetFLASHLIGHT( bool i )
	{
		m_nFLASHLIGHT = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = true;
#endif
	}
private:
	int m_nFLASHLIGHTSHADOWS;
#ifdef _DEBUG
	bool m_bFLASHLIGHTSHADOWS;
#endif
public:
	void SetFLASHLIGHTSHADOWS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLASHLIGHTSHADOWS = i;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = true;
#endif
	}
	void SetFLASHLIGHTSHADOWS( bool i )
	{
		m_nFLASHLIGHTSHADOWS = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = true;
#endif
	}
public:
	di_lightmapped_ice_ps30_Dynamic_Index()
	{
#ifdef _DEBUG
		m_bNUM_LIGHTS = false;
#endif // _DEBUG
		m_nNUM_LIGHTS = 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = false;
#endif // _DEBUG
		m_nFLASHLIGHT = 0;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = false;
#endif // _DEBUG
		m_nFLASHLIGHTSHADOWS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bNUM_LIGHTS && m_bFLASHLIGHT && m_bFLASHLIGHTSHADOWS;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nNUM_LIGHTS ) + ( 5 * m_nFLASHLIGHT ) + ( 10 * m_nFLASHLIGHTSHADOWS ) + 0;
	}
};
#define shaderDynamicTest_di_lightmapped_ice_ps30 psh_forgot_to_set_dynamic_NUM_LIGHTS + psh_forgot_to_set_dynamic_FLASHLIGHT + psh_forgot_to_set_dynamic_FLASHLIGHTSHADOWS + 0
