//========================================================================//
//
// Purpose: Provide convenient mapping for shader constants
//
// $NoKeywords: $
//=============================================================================

#ifndef C_CODE_HACK
#include "common_vertexlitgeneric_dx9.h"
#endif

#define PSREG_SELFILLUMTINT						PSREG_CONSTANT_00
#define PSREG_DIFFUSE_MODULATION				PSREG_CONSTANT_01
#define PSREG_ENVMAP_TINT__SHADOW_TWEAKS		PSREG_CONSTANT_02
#define PSREG_SELFILLUM_SCALE_BIAS_EXP			PSREG_CONSTANT_03
#define PSREG_AMBIENT_CUBE						PSREG_CONSTANT_04
//		PSREG_AMBIENT_CUBE						PSREG_CONSTANT_05
//		PSREG_AMBIENT_CUBE						PSREG_CONSTANT_06
//		PSREG_AMBIENT_CUBE						PSREG_CONSTANT_07
//		PSREG_AMBIENT_CUBE						PSREG_CONSTANT_08
//		PSREG_AMBIENT_CUBE						PSREG_CONSTANT_09
#define PSREG_ENVMAP_FRESNEL__SELFILLUMMASK		PSREG_CONSTANT_10
#define PSREG_EYEPOS_SPEC_EXPONENT				PSREG_CONSTANT_11
#define PSREG_FOG_PARAMS						PSREG_CONSTANT_12
#define PSREG_FLASHLIGHT_ATTENUATION			PSREG_CONSTANT_13
#define PSREG_FLASHLIGHT_POSITION_RIM_BOOST		PSREG_CONSTANT_14
#define PSREG_FLASHLIGHT_TO_WORLD_TEXTURE		PSREG_CONSTANT_15
//		PSREG_FLASHLIGHT_TO_WORLD_TEXTURE		PSREG_CONSTANT_16
//		PSREG_FLASHLIGHT_TO_WORLD_TEXTURE		PSREG_CONSTANT_17
//		PSREG_FLASHLIGHT_TO_WORLD_TEXTURE		PSREG_CONSTANT_18
#define PSREG_FRESNEL_SPEC_PARAMS				PSREG_CONSTANT_19
#define PSREG_LIGHT_INFO_ARRAY					PSREG_CONSTANT_20
//		PSREG_LIGHT_INFO_ARRAY					PSREG_CONSTANT_21
//		PSREG_LIGHT_INFO_ARRAY					PSREG_CONSTANT_22
//		PSREG_LIGHT_INFO_ARRAY					PSREG_CONSTANT_23
//		PSREG_LIGHT_INFO_ARRAY					PSREG_CONSTANT_24
//		PSREG_LIGHT_INFO_ARRAY					PSREG_CONSTANT_25
#define PSREG_SPEC_RIM_PARAMS					PSREG_CONSTANT_26
// #define **free**								PSREG_CONSTANT_27	//actually using this often blows constant limits, since literals have to get stuffed somewhere...
#define	PSREG_FLASHLIGHT_COLOR					PSREG_CONSTANT_28
#define	PSREG_LINEAR_FOG_COLOR					PSREG_CONSTANT_29
#define	PSREG_LIGHT_SCALE						PSREG_CONSTANT_30
#define	PSREG_FLASHLIGHT_SCREEN_SCALE			PSREG_CONSTANT_31
//  --- End of ps_2_0 and ps_2_b constants ---


#ifndef C_CODE_HACK
//for fxc code, map the constants to register names.
#define PSREG_CONSTANT_00	c0
#define PSREG_CONSTANT_01	c1
#define PSREG_CONSTANT_02	c2
#define PSREG_CONSTANT_03	c3
#define PSREG_CONSTANT_04	c4
#define PSREG_CONSTANT_05	c5
#define PSREG_CONSTANT_06	c6
#define PSREG_CONSTANT_07	c7
#define PSREG_CONSTANT_08	c8
#define PSREG_CONSTANT_09	c9
#define PSREG_CONSTANT_10	c10
#define PSREG_CONSTANT_11	c11
#define PSREG_CONSTANT_12	c12
#define PSREG_CONSTANT_13	c13
#define PSREG_CONSTANT_14	c14
#define PSREG_CONSTANT_15	c15
#define PSREG_CONSTANT_16	c16
#define PSREG_CONSTANT_17	c17
#define PSREG_CONSTANT_18	c18
#define PSREG_CONSTANT_19	c19
#define PSREG_CONSTANT_20	c20
#define PSREG_CONSTANT_21	c21
#define PSREG_CONSTANT_22	c22
#define PSREG_CONSTANT_23	c23
#define PSREG_CONSTANT_24	c24
#define PSREG_CONSTANT_25	c25
#define PSREG_CONSTANT_26	c26
#define PSREG_CONSTANT_27	c27
#define PSREG_CONSTANT_28	c28
#define PSREG_CONSTANT_29	c29
#define PSREG_CONSTANT_30	c30
#define PSREG_CONSTANT_31	c31
#endif
