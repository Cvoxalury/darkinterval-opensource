//========================================================================//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef COMMON_HLSL_CONSTS_H_
#define COMMON_HLSL_CONSTS_H_
#define DARKINTERVAL_SHADERS
#ifdef NV3X
  #define PSHADER_VECT_SCALE 20.0
  #define VSHADER_VECT_SCALE (1.0 / (PSHADER_VECT_SCALE) )
#else
  #define PSHADER_VECT_SCALE 1.0
  #define VSHADER_VECT_SCALE 1.0
#endif

// GR - HDR luminance maps to 0..n range
// IMPORTANT: Keep the same value as in materialsystem_global.h
// HDRFIXME: Make this a pixel shader constant?
#define MAX_HDR_OVERBRIGHT 16.0f

#define LINEAR_FOG_COLOR 29
#define TONE_MAPPING_SCALE_PSH_CONSTANT 30
#ifdef DARKINTERVAL_SHADERS
#define DEPTH_FEATHER_PROJ_TO_VIEW_Z 	13
#define DEPTH_FEATHER_PROJ_TO_VIEW_W 	14
#define DEPTH_FEATHER_VIEWPORT_MAD 		15
#endif
#endif //#ifndef COMMON_HLSL_CONSTS_H_
