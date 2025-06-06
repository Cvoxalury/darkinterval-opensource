#include "shaderlib/cshader.h"
class di_vertexlit_and_unlit_generic_ps20b_Static_Index
{
private:
	int m_nDETAILTEXTURE;
#ifdef _DEBUG
	bool m_bDETAILTEXTURE;
#endif
public:
	void SetDETAILTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDETAILTEXTURE = i;
#ifdef _DEBUG
		m_bDETAILTEXTURE = true;
#endif
	}
	void SetDETAILTEXTURE( bool i )
	{
		m_nDETAILTEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bDETAILTEXTURE = true;
#endif
	}
private:
	int m_nCUBEMAP;
#ifdef _DEBUG
	bool m_bCUBEMAP;
#endif
public:
	void SetCUBEMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCUBEMAP = i;
#ifdef _DEBUG
		m_bCUBEMAP = true;
#endif
	}
	void SetCUBEMAP( bool i )
	{
		m_nCUBEMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bCUBEMAP = true;
#endif
	}
private:
	int m_nDIFFUSELIGHTING;
#ifdef _DEBUG
	bool m_bDIFFUSELIGHTING;
#endif
public:
	void SetDIFFUSELIGHTING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDIFFUSELIGHTING = i;
#ifdef _DEBUG
		m_bDIFFUSELIGHTING = true;
#endif
	}
	void SetDIFFUSELIGHTING( bool i )
	{
		m_nDIFFUSELIGHTING = i ? 1 : 0;
#ifdef _DEBUG
		m_bDIFFUSELIGHTING = true;
#endif
	}
private:
	int m_nENVMAPMASK;
#ifdef _DEBUG
	bool m_bENVMAPMASK;
#endif
public:
	void SetENVMAPMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENVMAPMASK = i;
#ifdef _DEBUG
		m_bENVMAPMASK = true;
#endif
	}
	void SetENVMAPMASK( bool i )
	{
		m_nENVMAPMASK = i ? 1 : 0;
#ifdef _DEBUG
		m_bENVMAPMASK = true;
#endif
	}
private:
	int m_nBASEALPHAENVMAPMASK;
#ifdef _DEBUG
	bool m_bBASEALPHAENVMAPMASK;
#endif
public:
	void SetBASEALPHAENVMAPMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASEALPHAENVMAPMASK = i;
#ifdef _DEBUG
		m_bBASEALPHAENVMAPMASK = true;
#endif
	}
	void SetBASEALPHAENVMAPMASK( bool i )
	{
		m_nBASEALPHAENVMAPMASK = i ? 1 : 0;
#ifdef _DEBUG
		m_bBASEALPHAENVMAPMASK = true;
#endif
	}
private:
	int m_nSELFILLUM;
#ifdef _DEBUG
	bool m_bSELFILLUM;
#endif
public:
	void SetSELFILLUM( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSELFILLUM = i;
#ifdef _DEBUG
		m_bSELFILLUM = true;
#endif
	}
	void SetSELFILLUM( bool i )
	{
		m_nSELFILLUM = i ? 1 : 0;
#ifdef _DEBUG
		m_bSELFILLUM = true;
#endif
	}
private:
	int m_nVERTEXCOLOR;
#ifdef _DEBUG
	bool m_bVERTEXCOLOR;
#endif
public:
	void SetVERTEXCOLOR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVERTEXCOLOR = i;
#ifdef _DEBUG
		m_bVERTEXCOLOR = true;
#endif
	}
	void SetVERTEXCOLOR( bool i )
	{
		m_nVERTEXCOLOR = i ? 1 : 0;
#ifdef _DEBUG
		m_bVERTEXCOLOR = true;
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
	int m_nSELFILLUM_ENVMAPMASK_ALPHA;
#ifdef _DEBUG
	bool m_bSELFILLUM_ENVMAPMASK_ALPHA;
#endif
public:
	void SetSELFILLUM_ENVMAPMASK_ALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSELFILLUM_ENVMAPMASK_ALPHA = i;
#ifdef _DEBUG
		m_bSELFILLUM_ENVMAPMASK_ALPHA = true;
#endif
	}
	void SetSELFILLUM_ENVMAPMASK_ALPHA( bool i )
	{
		m_nSELFILLUM_ENVMAPMASK_ALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bSELFILLUM_ENVMAPMASK_ALPHA = true;
#endif
	}
private:
	int m_nDETAIL_BLEND_MODE;
#ifdef _DEBUG
	bool m_bDETAIL_BLEND_MODE;
#endif
public:
	void SetDETAIL_BLEND_MODE( int i )
	{
		Assert( i >= 0 && i <= 5 );
		m_nDETAIL_BLEND_MODE = i;
#ifdef _DEBUG
		m_bDETAIL_BLEND_MODE = true;
#endif
	}
	void SetDETAIL_BLEND_MODE( bool i )
	{
		m_nDETAIL_BLEND_MODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bDETAIL_BLEND_MODE = true;
#endif
	}
private:
	int m_nSEAMLESS_BASE;
#ifdef _DEBUG
	bool m_bSEAMLESS_BASE;
#endif
public:
	void SetSEAMLESS_BASE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSEAMLESS_BASE = i;
#ifdef _DEBUG
		m_bSEAMLESS_BASE = true;
#endif
	}
	void SetSEAMLESS_BASE( bool i )
	{
		m_nSEAMLESS_BASE = i ? 1 : 0;
#ifdef _DEBUG
		m_bSEAMLESS_BASE = true;
#endif
	}
private:
	int m_nSEAMLESS_DETAIL;
#ifdef _DEBUG
	bool m_bSEAMLESS_DETAIL;
#endif
public:
	void SetSEAMLESS_DETAIL( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nSEAMLESS_DETAIL = i;
#ifdef _DEBUG
		m_bSEAMLESS_DETAIL = true;
#endif
	}
	void SetSEAMLESS_DETAIL( bool i )
	{
		m_nSEAMLESS_DETAIL = i ? 1 : 0;
#ifdef _DEBUG
		m_bSEAMLESS_DETAIL = true;
#endif
	}
private:
	int m_nDISTANCEALPHA;
#ifdef _DEBUG
	bool m_bDISTANCEALPHA;
#endif
public:
	void SetDISTANCEALPHA( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nDISTANCEALPHA = i;
#ifdef _DEBUG
		m_bDISTANCEALPHA = true;
#endif
	}
	void SetDISTANCEALPHA( bool i )
	{
		m_nDISTANCEALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bDISTANCEALPHA = true;
#endif
	}
private:
	int m_nDISTANCEALPHAFROMDETAIL;
#ifdef _DEBUG
	bool m_bDISTANCEALPHAFROMDETAIL;
#endif
public:
	void SetDISTANCEALPHAFROMDETAIL( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nDISTANCEALPHAFROMDETAIL = i;
#ifdef _DEBUG
		m_bDISTANCEALPHAFROMDETAIL = true;
#endif
	}
	void SetDISTANCEALPHAFROMDETAIL( bool i )
	{
		m_nDISTANCEALPHAFROMDETAIL = i ? 1 : 0;
#ifdef _DEBUG
		m_bDISTANCEALPHAFROMDETAIL = true;
#endif
	}
private:
	int m_nSOFT_MASK;
#ifdef _DEBUG
	bool m_bSOFT_MASK;
#endif
public:
	void SetSOFT_MASK( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nSOFT_MASK = i;
#ifdef _DEBUG
		m_bSOFT_MASK = true;
#endif
	}
	void SetSOFT_MASK( bool i )
	{
		m_nSOFT_MASK = i ? 1 : 0;
#ifdef _DEBUG
		m_bSOFT_MASK = true;
#endif
	}
private:
	int m_nOUTLINE;
#ifdef _DEBUG
	bool m_bOUTLINE;
#endif
public:
	void SetOUTLINE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nOUTLINE = i;
#ifdef _DEBUG
		m_bOUTLINE = true;
#endif
	}
	void SetOUTLINE( bool i )
	{
		m_nOUTLINE = i ? 1 : 0;
#ifdef _DEBUG
		m_bOUTLINE = true;
#endif
	}
private:
	int m_nOUTER_GLOW;
#ifdef _DEBUG
	bool m_bOUTER_GLOW;
#endif
public:
	void SetOUTER_GLOW( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nOUTER_GLOW = i;
#ifdef _DEBUG
		m_bOUTER_GLOW = true;
#endif
	}
	void SetOUTER_GLOW( bool i )
	{
		m_nOUTER_GLOW = i ? 1 : 0;
#ifdef _DEBUG
		m_bOUTER_GLOW = true;
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
	int m_nDEPTHBLEND;
#ifdef _DEBUG
	bool m_bDEPTHBLEND;
#endif
public:
	void SetDEPTHBLEND( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDEPTHBLEND = i;
#ifdef _DEBUG
		m_bDEPTHBLEND = true;
#endif
	}
	void SetDEPTHBLEND( bool i )
	{
		m_nDEPTHBLEND = i ? 1 : 0;
#ifdef _DEBUG
		m_bDEPTHBLEND = true;
#endif
	}
private:
	int m_nBLENDTINTBYBASEALPHA;
#ifdef _DEBUG
	bool m_bBLENDTINTBYBASEALPHA;
#endif
public:
	void SetBLENDTINTBYBASEALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLENDTINTBYBASEALPHA = i;
#ifdef _DEBUG
		m_bBLENDTINTBYBASEALPHA = true;
#endif
	}
	void SetBLENDTINTBYBASEALPHA( bool i )
	{
		m_nBLENDTINTBYBASEALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bBLENDTINTBYBASEALPHA = true;
#endif
	}
private:
	int m_nSRGB_INPUT_ADAPTER;
#ifdef _DEBUG
	bool m_bSRGB_INPUT_ADAPTER;
#endif
public:
	void SetSRGB_INPUT_ADAPTER( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSRGB_INPUT_ADAPTER = i;
#ifdef _DEBUG
		m_bSRGB_INPUT_ADAPTER = true;
#endif
	}
	void SetSRGB_INPUT_ADAPTER( bool i )
	{
		m_nSRGB_INPUT_ADAPTER = i ? 1 : 0;
#ifdef _DEBUG
		m_bSRGB_INPUT_ADAPTER = true;
#endif
	}
private:
	int m_nCUBEMAP_SPHERE_LEGACY;
#ifdef _DEBUG
	bool m_bCUBEMAP_SPHERE_LEGACY;
#endif
public:
	void SetCUBEMAP_SPHERE_LEGACY( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nCUBEMAP_SPHERE_LEGACY = i;
#ifdef _DEBUG
		m_bCUBEMAP_SPHERE_LEGACY = true;
#endif
	}
	void SetCUBEMAP_SPHERE_LEGACY( bool i )
	{
		m_nCUBEMAP_SPHERE_LEGACY = i ? 1 : 0;
#ifdef _DEBUG
		m_bCUBEMAP_SPHERE_LEGACY = true;
#endif
	}
public:
	di_vertexlit_and_unlit_generic_ps20b_Static_Index( )
	{
#ifdef _DEBUG
		m_bDETAILTEXTURE = false;
#endif // _DEBUG
		m_nDETAILTEXTURE = 0;
#ifdef _DEBUG
		m_bCUBEMAP = false;
#endif // _DEBUG
		m_nCUBEMAP = 0;
#ifdef _DEBUG
		m_bDIFFUSELIGHTING = false;
#endif // _DEBUG
		m_nDIFFUSELIGHTING = 0;
#ifdef _DEBUG
		m_bENVMAPMASK = false;
#endif // _DEBUG
		m_nENVMAPMASK = 0;
#ifdef _DEBUG
		m_bBASEALPHAENVMAPMASK = false;
#endif // _DEBUG
		m_nBASEALPHAENVMAPMASK = 0;
#ifdef _DEBUG
		m_bSELFILLUM = false;
#endif // _DEBUG
		m_nSELFILLUM = 0;
#ifdef _DEBUG
		m_bVERTEXCOLOR = false;
#endif // _DEBUG
		m_nVERTEXCOLOR = 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = false;
#endif // _DEBUG
		m_nFLASHLIGHT = 0;
#ifdef _DEBUG
		m_bSELFILLUM_ENVMAPMASK_ALPHA = false;
#endif // _DEBUG
		m_nSELFILLUM_ENVMAPMASK_ALPHA = 0;
#ifdef _DEBUG
		m_bDETAIL_BLEND_MODE = false;
#endif // _DEBUG
		m_nDETAIL_BLEND_MODE = 0;
#ifdef _DEBUG
		m_bSEAMLESS_BASE = false;
#endif // _DEBUG
		m_nSEAMLESS_BASE = 0;
#ifdef _DEBUG
		m_bSEAMLESS_DETAIL = false;
#endif // _DEBUG
		m_nSEAMLESS_DETAIL = 0;
#ifdef _DEBUG
		m_bDISTANCEALPHA = false;
#endif // _DEBUG
		m_nDISTANCEALPHA = 0;
#ifdef _DEBUG
		m_bDISTANCEALPHAFROMDETAIL = false;
#endif // _DEBUG
		m_nDISTANCEALPHAFROMDETAIL = 0;
#ifdef _DEBUG
		m_bSOFT_MASK = false;
#endif // _DEBUG
		m_nSOFT_MASK = 0;
#ifdef _DEBUG
		m_bOUTLINE = false;
#endif // _DEBUG
		m_nOUTLINE = 0;
#ifdef _DEBUG
		m_bOUTER_GLOW = false;
#endif // _DEBUG
		m_nOUTER_GLOW = 0;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = false;
#endif // _DEBUG
		m_nFLASHLIGHTDEPTHFILTERMODE = 0;
#ifdef _DEBUG
		m_bDEPTHBLEND = false;
#endif // _DEBUG
		m_nDEPTHBLEND = 0;
#ifdef _DEBUG
		m_bBLENDTINTBYBASEALPHA = false;
#endif // _DEBUG
		m_nBLENDTINTBYBASEALPHA = 0;
#ifdef _DEBUG
		m_bSRGB_INPUT_ADAPTER = false;
#endif // _DEBUG
		m_nSRGB_INPUT_ADAPTER = 0;
#ifdef _DEBUG
		m_bCUBEMAP_SPHERE_LEGACY = false;
#endif // _DEBUG
		m_nCUBEMAP_SPHERE_LEGACY = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bDETAILTEXTURE && m_bCUBEMAP && m_bDIFFUSELIGHTING && m_bENVMAPMASK && m_bBASEALPHAENVMAPMASK && m_bSELFILLUM && m_bVERTEXCOLOR && m_bFLASHLIGHT && m_bSELFILLUM_ENVMAPMASK_ALPHA && m_bDETAIL_BLEND_MODE && m_bSEAMLESS_BASE && m_bSEAMLESS_DETAIL && m_bDISTANCEALPHA && m_bDISTANCEALPHAFROMDETAIL && m_bSOFT_MASK && m_bOUTLINE && m_bOUTER_GLOW && m_bFLASHLIGHTDEPTHFILTERMODE && m_bDEPTHBLEND && m_bBLENDTINTBYBASEALPHA && m_bSRGB_INPUT_ADAPTER && m_bCUBEMAP_SPHERE_LEGACY;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 6 * m_nDETAILTEXTURE ) + ( 12 * m_nCUBEMAP ) + ( 24 * m_nDIFFUSELIGHTING ) + ( 48 * m_nENVMAPMASK ) + ( 96 * m_nBASEALPHAENVMAPMASK ) + ( 192 * m_nSELFILLUM ) + ( 384 * m_nVERTEXCOLOR ) + ( 768 * m_nFLASHLIGHT ) + ( 1536 * m_nSELFILLUM_ENVMAPMASK_ALPHA ) + ( 3072 * m_nDETAIL_BLEND_MODE ) + ( 18432 * m_nSEAMLESS_BASE ) + ( 36864 * m_nSEAMLESS_DETAIL ) + ( 36864 * m_nDISTANCEALPHA ) + ( 36864 * m_nDISTANCEALPHAFROMDETAIL ) + ( 36864 * m_nSOFT_MASK ) + ( 36864 * m_nOUTLINE ) + ( 36864 * m_nOUTER_GLOW ) + ( 36864 * m_nFLASHLIGHTDEPTHFILTERMODE ) + ( 110592 * m_nDEPTHBLEND ) + ( 221184 * m_nBLENDTINTBYBASEALPHA ) + ( 442368 * m_nSRGB_INPUT_ADAPTER ) + ( 884736 * m_nCUBEMAP_SPHERE_LEGACY ) + 0;
	}
};
#define shaderStaticTest_di_vertexlit_and_unlit_generic_ps20b psh_forgot_to_set_static_DETAILTEXTURE + psh_forgot_to_set_static_CUBEMAP + psh_forgot_to_set_static_DIFFUSELIGHTING + psh_forgot_to_set_static_ENVMAPMASK + psh_forgot_to_set_static_BASEALPHAENVMAPMASK + psh_forgot_to_set_static_SELFILLUM + psh_forgot_to_set_static_VERTEXCOLOR + psh_forgot_to_set_static_FLASHLIGHT + psh_forgot_to_set_static_SELFILLUM_ENVMAPMASK_ALPHA + psh_forgot_to_set_static_DETAIL_BLEND_MODE + psh_forgot_to_set_static_SEAMLESS_BASE + psh_forgot_to_set_static_SEAMLESS_DETAIL + psh_forgot_to_set_static_DISTANCEALPHA + psh_forgot_to_set_static_DISTANCEALPHAFROMDETAIL + psh_forgot_to_set_static_SOFT_MASK + psh_forgot_to_set_static_OUTLINE + psh_forgot_to_set_static_OUTER_GLOW + psh_forgot_to_set_static_FLASHLIGHTDEPTHFILTERMODE + psh_forgot_to_set_static_DEPTHBLEND + psh_forgot_to_set_static_BLENDTINTBYBASEALPHA + psh_forgot_to_set_static_SRGB_INPUT_ADAPTER + psh_forgot_to_set_static_CUBEMAP_SPHERE_LEGACY + 0
class di_vertexlit_and_unlit_generic_ps20b_Dynamic_Index
{
private:
	int m_nLIGHTING_PREVIEW;
#ifdef _DEBUG
	bool m_bLIGHTING_PREVIEW;
#endif
public:
	void SetLIGHTING_PREVIEW( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nLIGHTING_PREVIEW = i;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = true;
#endif
	}
	void SetLIGHTING_PREVIEW( bool i )
	{
		m_nLIGHTING_PREVIEW = i ? 1 : 0;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = true;
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
	di_vertexlit_and_unlit_generic_ps20b_Dynamic_Index()
	{
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = false;
#endif // _DEBUG
		m_nLIGHTING_PREVIEW = 0;
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
		bool bAllDynamicVarsDefined = m_bLIGHTING_PREVIEW && m_bFLASHLIGHTSHADOWS;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nLIGHTING_PREVIEW ) + ( 3 * m_nFLASHLIGHTSHADOWS ) + 0;
	}
};
#define shaderDynamicTest_di_vertexlit_and_unlit_generic_ps20b psh_forgot_to_set_dynamic_LIGHTING_PREVIEW + psh_forgot_to_set_dynamic_FLASHLIGHTSHADOWS + 0
