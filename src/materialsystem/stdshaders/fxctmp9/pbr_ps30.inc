#include "shaderlib/cshader.h"
class pbr_ps30_Static_Index
{
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
	int m_nLIGHTMAPPED;
#ifdef _DEBUG
	bool m_bLIGHTMAPPED;
#endif
public:
	void SetLIGHTMAPPED( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLIGHTMAPPED = i;
#ifdef _DEBUG
		m_bLIGHTMAPPED = true;
#endif
	}
	void SetLIGHTMAPPED( bool i )
	{
		m_nLIGHTMAPPED = i ? 1 : 0;
#ifdef _DEBUG
		m_bLIGHTMAPPED = true;
#endif
	}
private:
	int m_nUSEENVAMBIENT;
#ifdef _DEBUG
	bool m_bUSEENVAMBIENT;
#endif
public:
	void SetUSEENVAMBIENT( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nUSEENVAMBIENT = i;
#ifdef _DEBUG
		m_bUSEENVAMBIENT = true;
#endif
	}
	void SetUSEENVAMBIENT( bool i )
	{
		m_nUSEENVAMBIENT = i ? 1 : 0;
#ifdef _DEBUG
		m_bUSEENVAMBIENT = true;
#endif
	}
private:
	int m_nEMISSIVE;
#ifdef _DEBUG
	bool m_bEMISSIVE;
#endif
public:
	void SetEMISSIVE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nEMISSIVE = i;
#ifdef _DEBUG
		m_bEMISSIVE = true;
#endif
	}
	void SetEMISSIVE( bool i )
	{
		m_nEMISSIVE = i ? 1 : 0;
#ifdef _DEBUG
		m_bEMISSIVE = true;
#endif
	}
private:
	int m_nPARALLAXLAYER;
#ifdef _DEBUG
	bool m_bPARALLAXLAYER;
#endif
public:
	void SetPARALLAXLAYER( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPARALLAXLAYER = i;
#ifdef _DEBUG
		m_bPARALLAXLAYER = true;
#endif
	}
	void SetPARALLAXLAYER( bool i )
	{
		m_nPARALLAXLAYER = i ? 1 : 0;
#ifdef _DEBUG
		m_bPARALLAXLAYER = true;
#endif
	}
private:
	int m_nENABLEACCUMULATION;
#ifdef _DEBUG
	bool m_bENABLEACCUMULATION;
#endif
public:
	void SetENABLEACCUMULATION( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENABLEACCUMULATION = i;
#ifdef _DEBUG
		m_bENABLEACCUMULATION = true;
#endif
	}
	void SetENABLEACCUMULATION( bool i )
	{
		m_nENABLEACCUMULATION = i ? 1 : 0;
#ifdef _DEBUG
		m_bENABLEACCUMULATION = true;
#endif
	}
private:
	int m_nACCUMULATEBUMPMAP;
#ifdef _DEBUG
	bool m_bACCUMULATEBUMPMAP;
#endif
public:
	void SetACCUMULATEBUMPMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nACCUMULATEBUMPMAP = i;
#ifdef _DEBUG
		m_bACCUMULATEBUMPMAP = true;
#endif
	}
	void SetACCUMULATEBUMPMAP( bool i )
	{
		m_nACCUMULATEBUMPMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bACCUMULATEBUMPMAP = true;
#endif
	}
private:
	int m_nBLENDED;
#ifdef _DEBUG
	bool m_bBLENDED;
#endif
public:
	void SetBLENDED( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLENDED = i;
#ifdef _DEBUG
		m_bBLENDED = true;
#endif
	}
	void SetBLENDED( bool i )
	{
		m_nBLENDED = i ? 1 : 0;
#ifdef _DEBUG
		m_bBLENDED = true;
#endif
	}
private:
	int m_nBLENDMODULATE;
#ifdef _DEBUG
	bool m_bBLENDMODULATE;
#endif
public:
	void SetBLENDMODULATE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nBLENDMODULATE = i;
#ifdef _DEBUG
		m_bBLENDMODULATE = true;
#endif
	}
	void SetBLENDMODULATE( bool i )
	{
		m_nBLENDMODULATE = i ? 1 : 0;
#ifdef _DEBUG
		m_bBLENDMODULATE = true;
#endif
	}
private:
	int m_nALPHABLEND;
#ifdef _DEBUG
	bool m_bALPHABLEND;
#endif
public:
	void SetALPHABLEND( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nALPHABLEND = i;
#ifdef _DEBUG
		m_bALPHABLEND = true;
#endif
	}
	void SetALPHABLEND( bool i )
	{
		m_nALPHABLEND = i ? 1 : 0;
#ifdef _DEBUG
		m_bALPHABLEND = true;
#endif
	}
private:
	int m_nENVMAPMASKINALPHA;
#ifdef _DEBUG
	bool m_bENVMAPMASKINALPHA;
#endif
public:
	void SetENVMAPMASKINALPHA( int i )
	{
		Assert( i >= 0 && i <= 3 );
		m_nENVMAPMASKINALPHA = i;
#ifdef _DEBUG
		m_bENVMAPMASKINALPHA = true;
#endif
	}
	void SetENVMAPMASKINALPHA( bool i )
	{
		m_nENVMAPMASKINALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bENVMAPMASKINALPHA = true;
#endif
	}
private:
	int m_nINVERTENVMAPMASK;
#ifdef _DEBUG
	bool m_bINVERTENVMAPMASK;
#endif
public:
	void SetINVERTENVMAPMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nINVERTENVMAPMASK = i;
#ifdef _DEBUG
		m_bINVERTENVMAPMASK = true;
#endif
	}
	void SetINVERTENVMAPMASK( bool i )
	{
		m_nINVERTENVMAPMASK = i ? 1 : 0;
#ifdef _DEBUG
		m_bINVERTENVMAPMASK = true;
#endif
	}
public:
	pbr_ps30_Static_Index( )
	{
#ifdef _DEBUG
		m_bFLASHLIGHT = false;
#endif // _DEBUG
		m_nFLASHLIGHT = 0;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = false;
#endif // _DEBUG
		m_nFLASHLIGHTDEPTHFILTERMODE = 0;
#ifdef _DEBUG
		m_bLIGHTMAPPED = false;
#endif // _DEBUG
		m_nLIGHTMAPPED = 0;
#ifdef _DEBUG
		m_bUSEENVAMBIENT = false;
#endif // _DEBUG
		m_nUSEENVAMBIENT = 0;
#ifdef _DEBUG
		m_bEMISSIVE = false;
#endif // _DEBUG
		m_nEMISSIVE = 0;
#ifdef _DEBUG
		m_bPARALLAXLAYER = false;
#endif // _DEBUG
		m_nPARALLAXLAYER = 0;
#ifdef _DEBUG
		m_bENABLEACCUMULATION = false;
#endif // _DEBUG
		m_nENABLEACCUMULATION = 0;
#ifdef _DEBUG
		m_bACCUMULATEBUMPMAP = false;
#endif // _DEBUG
		m_nACCUMULATEBUMPMAP = 0;
#ifdef _DEBUG
		m_bBLENDED = false;
#endif // _DEBUG
		m_nBLENDED = 0;
#ifdef _DEBUG
		m_bBLENDMODULATE = false;
#endif // _DEBUG
		m_nBLENDMODULATE = 0;
#ifdef _DEBUG
		m_bALPHABLEND = false;
#endif // _DEBUG
		m_nALPHABLEND = 0;
#ifdef _DEBUG
		m_bENVMAPMASKINALPHA = false;
#endif // _DEBUG
		m_nENVMAPMASKINALPHA = 0;
#ifdef _DEBUG
		m_bINVERTENVMAPMASK = false;
#endif // _DEBUG
		m_nINVERTENVMAPMASK = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bFLASHLIGHT && m_bFLASHLIGHTDEPTHFILTERMODE && m_bLIGHTMAPPED && m_bUSEENVAMBIENT && m_bEMISSIVE && m_bPARALLAXLAYER && m_bENABLEACCUMULATION && m_bACCUMULATEBUMPMAP && m_bBLENDED && m_bBLENDMODULATE && m_bALPHABLEND && m_bENVMAPMASKINALPHA && m_bINVERTENVMAPMASK;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 80 * m_nFLASHLIGHT ) + ( 160 * m_nFLASHLIGHTDEPTHFILTERMODE ) + ( 480 * m_nLIGHTMAPPED ) + ( 960 * m_nUSEENVAMBIENT ) + ( 960 * m_nEMISSIVE ) + ( 1920 * m_nPARALLAXLAYER ) + ( 3840 * m_nENABLEACCUMULATION ) + ( 7680 * m_nACCUMULATEBUMPMAP ) + ( 15360 * m_nBLENDED ) + ( 30720 * m_nBLENDMODULATE ) + ( 30720 * m_nALPHABLEND ) + ( 61440 * m_nENVMAPMASKINALPHA ) + ( 245760 * m_nINVERTENVMAPMASK ) + 0;
	}
};
#define shaderStaticTest_pbr_ps30 psh_forgot_to_set_static_FLASHLIGHT + psh_forgot_to_set_static_FLASHLIGHTDEPTHFILTERMODE + psh_forgot_to_set_static_LIGHTMAPPED + psh_forgot_to_set_static_USEENVAMBIENT + psh_forgot_to_set_static_EMISSIVE + psh_forgot_to_set_static_PARALLAXLAYER + psh_forgot_to_set_static_ENABLEACCUMULATION + psh_forgot_to_set_static_ACCUMULATEBUMPMAP + psh_forgot_to_set_static_BLENDED + psh_forgot_to_set_static_BLENDMODULATE + psh_forgot_to_set_static_ALPHABLEND + psh_forgot_to_set_static_ENVMAPMASKINALPHA + psh_forgot_to_set_static_INVERTENVMAPMASK + 0
class pbr_ps30_Dynamic_Index
{
private:
	int m_nWRITEWATERFOGTODESTALPHA;
#ifdef _DEBUG
	bool m_bWRITEWATERFOGTODESTALPHA;
#endif
public:
	void SetWRITEWATERFOGTODESTALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWRITEWATERFOGTODESTALPHA = i;
#ifdef _DEBUG
		m_bWRITEWATERFOGTODESTALPHA = true;
#endif
	}
	void SetWRITEWATERFOGTODESTALPHA( bool i )
	{
		m_nWRITEWATERFOGTODESTALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bWRITEWATERFOGTODESTALPHA = true;
#endif
	}
private:
	int m_nPIXELFOGTYPE;
#ifdef _DEBUG
	bool m_bPIXELFOGTYPE;
#endif
public:
	void SetPIXELFOGTYPE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPIXELFOGTYPE = i;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
	void SetPIXELFOGTYPE( bool i )
	{
		m_nPIXELFOGTYPE = i ? 1 : 0;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
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
	int m_nWRITE_DEPTH_TO_DESTALPHA;
#ifdef _DEBUG
	bool m_bWRITE_DEPTH_TO_DESTALPHA;
#endif
public:
	void SetWRITE_DEPTH_TO_DESTALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWRITE_DEPTH_TO_DESTALPHA = i;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = true;
#endif
	}
	void SetWRITE_DEPTH_TO_DESTALPHA( bool i )
	{
		m_nWRITE_DEPTH_TO_DESTALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = true;
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
	pbr_ps30_Dynamic_Index()
	{
#ifdef _DEBUG
		m_bWRITEWATERFOGTODESTALPHA = false;
#endif // _DEBUG
		m_nWRITEWATERFOGTODESTALPHA = 0;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = false;
#endif // _DEBUG
		m_nPIXELFOGTYPE = 0;
#ifdef _DEBUG
		m_bNUM_LIGHTS = false;
#endif // _DEBUG
		m_nNUM_LIGHTS = 0;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = false;
#endif // _DEBUG
		m_nWRITE_DEPTH_TO_DESTALPHA = 0;
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
		bool bAllDynamicVarsDefined = m_bWRITEWATERFOGTODESTALPHA && m_bPIXELFOGTYPE && m_bNUM_LIGHTS && m_bWRITE_DEPTH_TO_DESTALPHA && m_bFLASHLIGHTSHADOWS;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nWRITEWATERFOGTODESTALPHA ) + ( 2 * m_nPIXELFOGTYPE ) + ( 4 * m_nNUM_LIGHTS ) + ( 20 * m_nWRITE_DEPTH_TO_DESTALPHA ) + ( 40 * m_nFLASHLIGHTSHADOWS ) + 0;
	}
};
#define shaderDynamicTest_pbr_ps30 psh_forgot_to_set_dynamic_WRITEWATERFOGTODESTALPHA + psh_forgot_to_set_dynamic_PIXELFOGTYPE + psh_forgot_to_set_dynamic_NUM_LIGHTS + psh_forgot_to_set_dynamic_WRITE_DEPTH_TO_DESTALPHA + psh_forgot_to_set_dynamic_FLASHLIGHTSHADOWS + 0
