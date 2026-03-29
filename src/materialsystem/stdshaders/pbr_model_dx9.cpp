//// 
// TODO LIST:
// - option to use $accumulatemask as a mask for albedo replacement (to avoid snow inside of cars, etc.) // done, via $accumulatetexture's alpha
// - masked blending
// - animated emissive
// - mro scaling values
// - look into parallax mapping
// - tie accumulation direction and dot to map-placed entity // done, for now via worldspawn
// - alphablend modes? 
// - animated accumulate textures
////

// Includes for all shaders
#include "BaseVSShader.h"
#include "cpp_shader_constant_register_map.h"

// Includes for PS30
#include "pbr_vs30.inc"
#include "pbr_ps30.inc"

// Includes for PS20b
#include "pbr_vs20b.inc"
#include "pbr_ps20b.inc"

// Defining samplers
// FREE SAMPLERS: 7, 9
const Sampler_t SAMPLER_BASETEXTURE = SHADER_SAMPLER0;
const Sampler_t SAMPLER_NORMAL = SHADER_SAMPLER1;
const Sampler_t SAMPLER_ENVMAP = SHADER_SAMPLER2;
const Sampler_t SAMPLER_SHADOWDEPTH = SHADER_SAMPLER4;
const Sampler_t SAMPLER_RANDOMROTATION = SHADER_SAMPLER5;
const Sampler_t SAMPLER_FLASHLIGHT = SHADER_SAMPLER6;
const Sampler_t SAMPLER_LIGHTMAP = SHADER_SAMPLER7;
const Sampler_t SAMPLER_MRO = SHADER_SAMPLER10;
const Sampler_t SAMPLER_EMISSIVE = SHADER_SAMPLER11;
const Sampler_t SAMPLER_PARALLAX1 = SHADER_SAMPLER3;
const Sampler_t SAMPLER_PARALLAX2 = SHADER_SAMPLER13;
//const Sampler_t SAMPLER_DETAIL = SHADER_SAMPLER15;
const Sampler_t SAMPLER_ACCUMULATE_TEXTURE = SHADER_SAMPLER15; // dynamic snow, ash, rust...
const Sampler_t SAMPLER_ACCUMULATE_BUMP = SHADER_SAMPLER14; // dynamic accumulate normalmap

// Convars
static ConVar mat_fullbright("mat_fullbright", "0", FCVAR_CHEAT);
static ConVar mat_specular("mat_specular", "1", FCVAR_CHEAT);
static ConVar mat_pbr_force_20b("mat_pbr_force_20b", "0", FCVAR_CHEAT);

extern ConVar mat_pbr_disable_accumulation;

// Variables for this shader
struct PBR_Model_Vars_t
{
	PBR_Model_Vars_t()
	{
		memset(this, 0xFF, sizeof(*this));
	}

	int baseTexture;
	int baseTexture2;
	int baseColor;
	int normalTexture;
	int bumpMap;
	int bumpFrame;
//	int bumpScale;
	int envMap;
	int baseTextureFrame;
	int baseTextureTransform;
//	int useParallax;
//	int parallaxDepth;
//	int parallaxCenter;
	int alphaTestReference;
	int flashlightTexture;
	int flashlightTextureFrame;
	int emissiveTexture;
	int emissiveTint;
	int mroTexture;
	int mroFrame;
	int useEnvAmbient;

	// Masking, Tint, Contrast and Saturation Parameters
	int envMapMask;
	int envMapContrast;
	int envMapSaturation;
	int	envMapTint;
	int envMapMaskFrame;
	int envMapMaskInverse;

	// Detailtexture implementation
//	int detailTexture;
//	int detailBlendMode;
//	int detailBlendFactor;
//	int detailTransform;
//	int detailScale;
//	int detailTint;

// MRO tweak
	int MROtint;
	int MROtintReplace;

	// Dynamic accumulation implementation
	int accumDot;
	int accumDir;
	int enableAccumulation;
	int accumulateTexture; // if specified, its alpha will be used for blending between basetexture and accumulatedTexture.
	int accumulateBaseStrength;
	int accumulateBumpmap;
	int accumulateBumpStrength;
	int accumDotOverride;
	int accumDirOverride;
	int accumulateTextureTransform;

	// Alphablend
	int alphaBlend;
	int alphaBlendColor;

	// treesway
	int m_nTreeSway;
	int m_nTreeSwayHeight;
	int m_nTreeSwayStartHeight;
	int m_nTreeSwayRadius;
	int m_nTreeSwayStartRadius;
	int m_nTreeSwaySpeed;
	int m_nTreeSwaySpeedHighWindMultiplier;
	int m_nTreeSwayStrength;
	int m_nTreeSwayScrumbleSpeed;
	int m_nTreeSwayScrumbleStrength;
	int m_nTreeSwayScrumbleFrequency;
	int m_nTreeSwayFalloffExp;
	int m_nTreeSwayScrumbleFalloffExp;
	int m_nTreeSwaySpeedLerpStart;
	int m_nTreeSwaySpeedLerpEnd;
	int m_nTreeSwayStatic;
	int m_nTreeSwayStaticValues;

	// parallax
	int parallax1texture;
	int parallax2texture;
	int parallax1Depth;
	int parallax2Depth;
	int parallax1Strength;
	int parallax2Strength;
	int parallax1UVScale;
	int parallax2UVScale;
};

// Beginning the shader
BEGIN_VS_SHADER(PBR_Model, "PBR shader")

// Setting up vmt parameters
BEGIN_SHADER_PARAMS;
SHADER_PARAM(ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "0", "");
SHADER_PARAM(MRO, SHADER_PARAM_TYPE_TEXTURE, "", "Texture with metalness in R, roughness in G, ambient occlusion in B.");
SHADER_PARAM(MROFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $mro");
SHADER_PARAM(EMISSIVE, SHADER_PARAM_TYPE_TEXTURE, "", "Emission texture");
SHADER_PARAM(NORMALTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Normal texture (deprecated, use $bumpmap)");
SHADER_PARAM(BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "", "Normal texture");
SHADER_PARAM(BUMPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $bumpmap/$normaltexture");
//SHADER_PARAM(BUMPSCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "bumpmap strength multiplier");
SHADER_PARAM(USEENVAMBIENT, SHADER_PARAM_TYPE_BOOL, "0", "Use the cubemaps to compute ambient light.");
SHADER_PARAM(EMISSIVETINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Color to multiply emission texture with");

// Envmapping parameters
SHADER_PARAM(ENVMAP, SHADER_PARAM_TYPE_TEXTURE, "shadertest/shadertest_env", "Set the cubemap for this material.");
SHADER_PARAM(ENVMAPTINT, SHADER_PARAM_TYPE_COLOR, "", "Tints the $envmap");
SHADER_PARAM(ENVMAPCONTRAST, SHADER_PARAM_TYPE_FLOAT, "", "0-1 where 1 is full contrast and 0 is pure cubemap");
SHADER_PARAM(ENVMAPSATURATION, SHADER_PARAM_TYPE_FLOAT, "", "0-1 where 1 is default pure cubemap and 0 is b/w.");
SHADER_PARAM(ENVMAPMASK, SHADER_PARAM_TYPE_TEXTURE, "shadertest/shadertest_envmask", "Envmap mask");
SHADER_PARAM(ENVMAPMASKFRAME, SHADER_PARAM_TYPE_INTEGER, "", "")
SHADER_PARAM(INVERTENVMAPMASK, SHADER_PARAM_TYPE_BOOL, "0", "Whenever we want to invert the mask.");

// MRO tweaks
SHADER_PARAM(MROTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Scales or replaces MRO values");
SHADER_PARAM(MROTINTREPLACE, SHADER_PARAM_TYPE_FLOAT, "0.0", "If 1, MROTint values will set MRO values rather than scale them");

// Detail textures
//SHADER_PARAM(DETAIL, SHADER_PARAM_TYPE_TEXTURE, "", "Texture");
//SHADER_PARAM(DETAILTEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$detail & $detail2 texcoord transform");
//SHADER_PARAM(DETAILFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $detail");
//SHADER_PARAM(DETAILSCALE, SHADER_PARAM_TYPE_FLOAT, "4", "scale of the detail texture");
//SHADER_PARAM(DETAILTINT, SHADER_PARAM_TYPE_COLOR, "", "Tints $Detail");
//SHADER_PARAM(DETAILBLENDMODE, SHADER_PARAM_TYPE_INTEGER, "0", "Check VDC for more information");
//SHADER_PARAM(DETAILBLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "", "How much detail is it that you want?");

// Dynamic accumulation
SHADER_PARAM(ENABLEACCUMULATION, SHADER_PARAM_TYPE_BOOL, "0", "Perform dynamic accumulation pixels on this material");
SHADER_PARAM(ACCUMULATETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Accumulated layer texture");
SHADER_PARAM(ACCUMULATEBUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "", "Accumulated layer texture");
SHADER_PARAM(ACCUMULATEBASESTRENGTH, SHADER_PARAM_TYPE_FLOAT, "1.0", "");
SHADER_PARAM(ACCUMULATEBUMPSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "1.0", "");
SHADER_PARAM(ACCUMULATEDOTOVERRIDE, SHADER_PARAM_TYPE_FLOAT, "0", "If greater than 0, will override the accumulation dot dir taken from worldspawn settings");
SHADER_PARAM(ACCUMULATEDIROVERRIDE, SHADER_PARAM_TYPE_COLOR, "[-1 -1 -1]", "If greater than -1 -1 -1, will override the accumulation dir taken from worldspawn settings");
SHADER_PARAM(ACCUMULATETEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$accumulatetexture texcoord transform")

// Alphablend
SHADER_PARAM(ALPHABLEND, SHADER_PARAM_TYPE_BOOL, "0", "Use alphablend color replacement for skins");
SHADER_PARAM(ALPHABLENDCOLOR, SHADER_PARAM_TYPE_COLOR, "", "");

// Tree sway
SHADER_PARAM(TREESWAY, SHADER_PARAM_TYPE_INTEGER, "0", "")
SHADER_PARAM(TREESWAYHEIGHT, SHADER_PARAM_TYPE_FLOAT, "1000", "")
SHADER_PARAM(TREESWAYSTARTHEIGHT, SHADER_PARAM_TYPE_FLOAT, "0.2", "")
SHADER_PARAM(TREESWAYRADIUS, SHADER_PARAM_TYPE_FLOAT, "300", "")
SHADER_PARAM(TREESWAYSTARTRADIUS, SHADER_PARAM_TYPE_FLOAT, "0.1", "")
SHADER_PARAM(TREESWAYSPEED, SHADER_PARAM_TYPE_FLOAT, "1", "")
SHADER_PARAM(TREESWAYSPEEDHIGHWINDMULTIPLIER, SHADER_PARAM_TYPE_FLOAT, "2", "")
SHADER_PARAM(TREESWAYSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "10", "")
SHADER_PARAM(TREESWAYSCRUMBLESPEED, SHADER_PARAM_TYPE_FLOAT, "0.1", "")
SHADER_PARAM(TREESWAYSCRUMBLESTRENGTH, SHADER_PARAM_TYPE_FLOAT, "0.1", "")
SHADER_PARAM(TREESWAYSCRUMBLEFREQUENCY, SHADER_PARAM_TYPE_FLOAT, "0.1", "")
SHADER_PARAM(TREESWAYFALLOFFEXP, SHADER_PARAM_TYPE_FLOAT, "1.5", "")
SHADER_PARAM(TREESWAYSCRUMBLEFALLOFFEXP, SHADER_PARAM_TYPE_FLOAT, "1.0", "")
SHADER_PARAM(TREESWAYSPEEDLERPSTART, SHADER_PARAM_TYPE_FLOAT, "3", "")
SHADER_PARAM(TREESWAYSPEEDLERPEND, SHADER_PARAM_TYPE_FLOAT, "6", "")
SHADER_PARAM(TREESWAYSTATIC, SHADER_PARAM_TYPE_BOOL, "0", "")
SHADER_PARAM(TREESWAYSTATICVALUES, SHADER_PARAM_TYPE_VEC2, "[0.5 0.5]", "")

// model parallax
SHADER_PARAM(PARALLAX1TEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Accumulated layer texture");
SHADER_PARAM(PARALLAX2TEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Accumulated layer texture");
SHADER_PARAM(PARALLAX1DEPTH, SHADER_PARAM_TYPE_FLOAT, "0.5", "")
SHADER_PARAM(PARALLAX2DEPTH, SHADER_PARAM_TYPE_FLOAT, "0.5", "")
SHADER_PARAM(PARALLAX1STRENGTH, SHADER_PARAM_TYPE_FLOAT, "1.0", "")
SHADER_PARAM(PARALLAX2STRENGTH, SHADER_PARAM_TYPE_FLOAT, "1.0", "")
SHADER_PARAM(PARALLAX1UVSCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "")
SHADER_PARAM(PARALLAX2UVSCALE, SHADER_PARAM_TYPE_FLOAT, "1.0", "")

END_SHADER_PARAMS;

// Setting up variables for this shader
void SetupVars(PBR_Model_Vars_t &info)
{
	info.baseTexture = BASETEXTURE;
	info.baseColor = COLOR2;
	info.normalTexture = NORMALTEXTURE;
	info.bumpMap = BUMPMAP;
	info.bumpFrame = BUMPFRAME;
	//info.bumpScale = BUMPSCALE;
	info.baseTextureFrame = FRAME;
	info.baseTextureTransform = BASETEXTURETRANSFORM;
	info.alphaTestReference = ALPHATESTREFERENCE;
	info.flashlightTexture = FLASHLIGHTTEXTURE;
	info.flashlightTextureFrame = FLASHLIGHTTEXTUREFRAME;
	info.emissiveTexture = EMISSIVE;
	info.emissiveTint = EMISSIVETINT;
	info.mroTexture = MRO;
	info.mroFrame = MROFRAME;
	info.useEnvAmbient = USEENVAMBIENT;

	// Detail textures...
	//info.detailTexture = DETAIL;
	//info.detailTransform = DETAILTEXTURETRANSFORM;
	//	info.detailFrame = DETAILFRAME; // not supported yet... and who cares?
	//info.detailScale = DETAILSCALE;
	//info.detailTint = DETAILTINT;
	//info.detailBlendMode = DETAILBLENDMODE;
	//info.detailBlendFactor = DETAILBLENDFACTOR;

	// Envmapping...
	info.envMap = ENVMAP;
	info.envMapMask = ENVMAPMASK;
	info.envMapMaskFrame = ENVMAPMASKFRAME;
	info.envMapTint = ENVMAPTINT;
	info.envMapContrast = ENVMAPCONTRAST;
	info.envMapSaturation = ENVMAPSATURATION;
	info.envMapMaskInverse = INVERTENVMAPMASK;

	// Dynamic accumulation
	info.enableAccumulation = ENABLEACCUMULATION;
	info.accumulateTexture = ACCUMULATETEXTURE;
	info.accumulateBaseStrength = ACCUMULATEBASESTRENGTH;
	info.accumulateBumpmap = ACCUMULATEBUMPMAP;
	info.accumulateBumpStrength = ACCUMULATEBUMPSTRENGTH;
	info.accumDotOverride = ACCUMULATEDOTOVERRIDE;
	info.accumDirOverride = ACCUMULATEDIROVERRIDE;
	info.accumulateTextureTransform = ACCUMULATETEXTURETRANSFORM;

	// Alphablend
	info.alphaBlend = ALPHABLEND;
	info.alphaBlendColor = ALPHABLENDCOLOR;

	// treesway
	info.m_nTreeSway = TREESWAY;
	info.m_nTreeSwayHeight = TREESWAYHEIGHT;
	info.m_nTreeSwayStartHeight = TREESWAYSTARTHEIGHT;
	info.m_nTreeSwayRadius = TREESWAYRADIUS;
	info.m_nTreeSwayStartRadius = TREESWAYSTARTRADIUS;
	info.m_nTreeSwaySpeed = TREESWAYSPEED;
	info.m_nTreeSwaySpeedHighWindMultiplier = TREESWAYSPEEDHIGHWINDMULTIPLIER;
	info.m_nTreeSwayStrength = TREESWAYSTRENGTH;
	info.m_nTreeSwayScrumbleSpeed = TREESWAYSCRUMBLESPEED;
	info.m_nTreeSwayScrumbleStrength = TREESWAYSCRUMBLESTRENGTH;
	info.m_nTreeSwayScrumbleFrequency = TREESWAYSCRUMBLEFREQUENCY;
	info.m_nTreeSwayFalloffExp = TREESWAYFALLOFFEXP;
	info.m_nTreeSwayScrumbleFalloffExp = TREESWAYSCRUMBLEFALLOFFEXP;
	info.m_nTreeSwaySpeedLerpStart = TREESWAYSPEEDLERPSTART;
	info.m_nTreeSwaySpeedLerpEnd = TREESWAYSPEEDLERPEND;
	info.m_nTreeSwayStatic = TREESWAYSTATIC;
	info.m_nTreeSwayStaticValues = TREESWAYSTATICVALUES;

	// parallax
	info.parallax1texture = PARALLAX1TEXTURE;
	info.parallax2texture = PARALLAX2TEXTURE;
	info.parallax1Depth = PARALLAX1DEPTH;
	info.parallax1Strength = PARALLAX1STRENGTH;
	info.parallax1UVScale = PARALLAX1UVSCALE;
	info.parallax2Depth = PARALLAX1DEPTH;
	info.parallax2Strength = PARALLAX1STRENGTH;
	info.parallax2UVScale = PARALLAX1UVSCALE;

	// MRO tweaks
	info.MROtint = MROTINT;
	info.MROtintReplace = MROTINTREPLACE;
};

// Initializing parameters
SHADER_INIT_PARAMS()
{
	// Fallback for changed parameter
	if (params[NORMALTEXTURE]->IsDefined())
		params[BUMPMAP]->SetStringValue(params[NORMALTEXTURE]->GetStringValue());

	// Dynamic lights need a bumpmap
	if (!params[BUMPMAP]->IsDefined())
		params[BUMPMAP]->SetStringValue("dev/flat_normal");
	
	// Set a good default mro texture
	if (!params[MRO]->IsDefined())
		params[MRO]->SetStringValue("dev/default_mro");
	
	// PBR relies heavily on envmaps
	if (!params[ENVMAP]->IsDefined())
		params[ENVMAP]->SetStringValue("env_cubemap");
		
	// Check if the hardware supports flashlight border color
	if (g_pHardwareConfig->SupportsBorderColor())
	{
		params[FLASHLIGHTTEXTURE]->SetStringValue("effects/flashlight_border");
	}
	else
	{
		params[FLASHLIGHTTEXTURE]->SetStringValue("effects/flashlight001");
	}
};

// Define shader fallback
SHADER_FALLBACK
{
	// TODO: Reasonable fallback
	if (!(g_pHardwareConfig->SupportsPixelShaders_2_b()))
	{
		return "VertexLitGeneric";
	}

return 0;
};

SHADER_INIT
{
	SET_FLAGS(MATERIAL_VAR_MODEL);

	PBR_Model_Vars_t info;
	SetupVars(info);

	// Dynamic accumulation
	SET_PARAM_INT_IF_NOT_DEFINED(info.enableAccumulation, 0);
	SET_PARAM_FLOAT_IF_NOT_DEFINED(info.accumulateBaseStrength, 1.0f);
	SET_PARAM_FLOAT_IF_NOT_DEFINED(info.accumulateBumpStrength, 1.0f);
	SET_PARAM_FLOAT_IF_NOT_DEFINED(info.accumDotOverride, 0);

	SET_PARAM_INT_IF_NOT_DEFINED(info.alphaBlend, 0);

	SET_PARAM_FLOAT_IF_NOT_DEFINED(info.parallax1Depth, 0.5f);
	SET_PARAM_FLOAT_IF_NOT_DEFINED(info.parallax1Strength, 1.0f);
	SET_PARAM_FLOAT_IF_NOT_DEFINED(info.parallax1UVScale, 1.0f);

	SET_PARAM_FLOAT_IF_NOT_DEFINED(info.parallax2Depth, 0.5f);
	SET_PARAM_FLOAT_IF_NOT_DEFINED(info.parallax2Strength, 1.0f);
	SET_PARAM_FLOAT_IF_NOT_DEFINED(info.parallax2UVScale, 1.0f);

	Assert(info.flashlightTexture >= 0);
	LoadTexture(info.flashlightTexture, TEXTUREFLAGS_SRGB);

	if (params[info.baseTexture]->IsDefined())
	LoadTexture(info.baseTexture, TEXTUREFLAGS_SRGB);

	//Assert(info.bumpMap >= 0);
	if (params[BUMPMAP]->IsDefined())
	LoadBumpMap(info.bumpMap);

	//	Assert(info.envMap >= 0);
	int envMapFlags = g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE ? TEXTUREFLAGS_SRGB : 0;
	envMapFlags |= TEXTUREFLAGS_ALL_MIPS;
	LoadCubeMap(info.envMap, envMapFlags);

	if (!params[info.envMapMaskFrame]->IsDefined())
		params[info.envMapMaskFrame]->SetIntValue(0);

	if (!params[info.envMapTint]->IsDefined())
		params[info.envMapTint]->SetVecValue(1.0f, 1.0f, 1.0f);

	if (!params[info.envMapContrast]->IsDefined())
		params[info.envMapContrast]->SetFloatValue(0.0f);

	if (!params[info.envMapSaturation]->IsDefined())
		params[info.envMapSaturation]->SetFloatValue(1.0f);

	if (!params[info.envMapMaskInverse]->IsDefined())
		params[info.envMapMaskInverse]->SetIntValue(0);

	// No texture means no self-illum or env mask in base alpha
	if (!params[info.baseTexture]->IsDefined())
	{
		CLEAR_FLAGS(MATERIAL_VAR_SELFILLUM);
		CLEAR_FLAGS(MATERIAL_VAR_BASEALPHAENVMAPMASK);
	}

//	if (!params[info.normalTexture]->IsDefined())
//	{
//		CLEAR_FLAGS(MATERIAL_VAR_NORMALMAPALPHAENVMAPMASK);
//	}

	if (params[ENVMAPMASK]->IsDefined())
		LoadTexture(info.envMapMask, TEXTUREFLAGS_SRGB);

	if (params[EMISSIVE]->IsDefined())
		LoadTexture(info.emissiveTexture, TEXTUREFLAGS_SRGB);

	Assert(info.mroTexture >= 0);
	if (params[MRO]->IsDefined())
		LoadTexture(info.mroTexture);
	
/*	
	if (params[info.detailTexture]->IsDefined())
	{
		if (params[info.detailBlendMode]->GetIntValue() > 0)
		{
			LoadTexture(info.detailTexture);
		}
		else
		{
			LoadTexture(info.detailTexture, TEXTUREFLAGS_SRGB); // when mod2x, load as sRGB
		}
	}
*/
	// dynamic accumulation
	if (params[info.accumulateTexture]->IsDefined())
	{
		LoadTexture(info.accumulateTexture, TEXTUREFLAGS_SRGB);
	}

	if (params[info.accumulateBumpmap]->IsDefined())
	{
		LoadTexture(info.accumulateBumpmap); // this is a normalmap too, so no sRGB // fixme: shouldn't this be LoadBumpmap?
	}

	// parallax
	if (params[info.parallax1texture]->IsDefined())
	{
		LoadTexture(info.parallax1texture, TEXTUREFLAGS_SRGB );
	}
	
	if (params[info.parallax2texture]->IsDefined())
	{
		LoadTexture(info.parallax2texture, TEXTUREFLAGS_SRGB);
	}

	if (IS_FLAG_SET(MATERIAL_VAR_MODEL)) // Set material var2 flags specific to models
	{
		SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_HW_SKINNING);             // Required for skinning
		SET_FLAGS2(MATERIAL_VAR2_DIFFUSE_BUMPMAPPED_MODEL);         // Required for dynamic lighting
		SET_FLAGS2(MATERIAL_VAR2_NEEDS_TANGENT_SPACES);             // Required for dynamic lighting
		SET_FLAGS2(MATERIAL_VAR2_LIGHTING_VERTEX_LIT);              // Required for dynamic lighting
		SET_FLAGS2(MATERIAL_VAR2_NEEDS_BAKED_LIGHTING_SNAPSHOTS);   // Required for ambient cube
		SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_FLASHLIGHT);              // Required for flashlight
		SET_FLAGS2(MATERIAL_VAR2_USE_FLASHLIGHT);                   // Required for flashlight
	}
	else // Set material var2 flags specific to brushes
	{
		SET_FLAGS2(MATERIAL_VAR2_LIGHTING_LIGHTMAP);                // Required for lightmaps
		SET_FLAGS2(MATERIAL_VAR2_LIGHTING_BUMPED_LIGHTMAP);         // Required for lightmaps
		SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_FLASHLIGHT);              // Required for flashlight
		SET_FLAGS2(MATERIAL_VAR2_USE_FLASHLIGHT);                   // Required for flashlight
	}
	
	// Treesway
	InitIntParam(info.m_nTreeSway, params, 0);
	InitFloatParam(info.m_nTreeSwayHeight, params, 1000.0f);
	InitFloatParam(info.m_nTreeSwayStartHeight, params, 0.1f);
	InitFloatParam(info.m_nTreeSwayRadius, params, 300.0f);
	InitFloatParam(info.m_nTreeSwayStartRadius, params, 0.2f);
	InitFloatParam(info.m_nTreeSwaySpeed, params, 1.0f);
	InitFloatParam(info.m_nTreeSwaySpeedHighWindMultiplier, params, 2.0f);
	InitFloatParam(info.m_nTreeSwayStrength, params, 10.0f);
	InitFloatParam(info.m_nTreeSwayScrumbleSpeed, params, 5.0f);
	InitFloatParam(info.m_nTreeSwayScrumbleStrength, params, 10.0f);
	InitFloatParam(info.m_nTreeSwayScrumbleFrequency, params, 12.0f);
	InitFloatParam(info.m_nTreeSwayFalloffExp, params, 1.5f);
	InitFloatParam(info.m_nTreeSwayScrumbleFalloffExp, params, 1.0f);
	InitFloatParam(info.m_nTreeSwaySpeedLerpStart, params, 3.0f);
	InitFloatParam(info.m_nTreeSwaySpeedLerpEnd, params, 6.0f);
};

// Drawing the shader
SHADER_DRAW
{
	PBR_Model_Vars_t info;
	SetupVars(info);

	// Setting up booleans
	bool bHasBaseTexture = params[info.baseTexture]->IsTexture();

	bool bHasNormalTexture = params[info.bumpMap]->IsTexture();
	bool bHasMroTexture = params[info.mroTexture]->IsTexture();
	bool bHasEmissiveTexture = params[info.emissiveTexture]->IsTexture();
	bool bHasEmissiveTint = params[info.emissiveTint]->IsDefined();
	bool bHasEnvTexture = params[info.envMap]->IsTexture();
	bool bIsAlphaTested = IS_FLAG_SET(MATERIAL_VAR_ALPHATEST) != 0;
	bool bHasFlashlight = UsingFlashlight(params);
	bool bHasColor = params[info.baseColor]->IsDefined();
	bool bLightMapped = false;
	bool bUseEnvAmbient = (params[info.useEnvAmbient]->GetIntValue() == 1);
	int nEnvmapmaskMode = 0;
	if (params[info.envMapMask]->IsDefined() && params[info.envMapMask]->IsTexture() && !bHasEmissiveTexture) nEnvmapmaskMode = 1;
	if (IS_FLAG_SET(MATERIAL_VAR_BASEALPHAENVMAPMASK)) nEnvmapmaskMode = 2;
	if (IS_FLAG_SET(MATERIAL_VAR_NORMALMAPALPHAENVMAPMASK)) nEnvmapmaskMode = 3;
	bool bInvertEnvMapMask = (params[info.envMapMaskInverse]->GetIntValue() == 1);
	bool bEnableAccumulation = (params[info.enableAccumulation]->GetIntValue() == 1 && mat_pbr_disable_accumulation.GetBool() == 0);
	bool bHasAccumulateTexture = params[info.accumulateTexture]->IsTexture();
	bool bHasAccumulateBumpmap = params[info.accumulateBumpmap]->IsTexture();
	bool bUseAlphaBlend = (params[info.alphaBlend]->GetIntValue() == 1);
//	bool bHasDetailTexture = params[info.detailTexture]->IsTexture();
	bool bHasParallax1Layer = params[info.parallax1texture]->IsTexture();
	bool bHasParallax2Layer = params[info.parallax2texture]->IsTexture();
	bool bHasMROTint = params[info.MROtint]->IsDefined();

	// Treesway
	int nTreeSwayMode = clamp(GetIntParam(info.m_nTreeSway, params, 0), 0, 2);
	bool bTreeSway = nTreeSwayMode != 0;

	// Determining whether we're dealing with a fully opaque material
	BlendType_t nBlendType = EvaluateBlendRequirements(info.baseTexture, true);
	bool bFullyOpaque = (nBlendType != BT_BLENDADD) && (nBlendType != BT_BLEND) && !bIsAlphaTested;

	if (IsSnapshotting())
	{
		// If alphatest is on, enable it
		pShaderShadow->EnableAlphaTest(bIsAlphaTested);

		if (params[info.alphaTestReference]->GetFloatValue() > 0.0f)
		{
			pShaderShadow->AlphaFunc(SHADER_ALPHAFUNC_GEQUAL, params[info.alphaTestReference]->GetFloatValue());
		}

		if (bHasFlashlight)
		{
			pShaderShadow->EnableBlending(true);
			pShaderShadow->BlendFunc(SHADER_BLEND_ONE, SHADER_BLEND_ONE); // Additive blending
		}
		else
		{
			SetDefaultBlendingShadowState(info.baseTexture, true);
		}

		int nShadowFilterMode = bHasFlashlight ? g_pHardwareConfig->GetShadowFilterMode() : 0;

		// Setting up samplers
		pShaderShadow->EnableTexture(SAMPLER_BASETEXTURE, true);    // Basecolor texture
		pShaderShadow->EnableSRGBRead(SAMPLER_BASETEXTURE, true);   // Basecolor is sRGB
		pShaderShadow->EnableTexture(SAMPLER_EMISSIVE, true);       // Emissive texture
		pShaderShadow->EnableSRGBRead(SAMPLER_EMISSIVE, true);      // Emissive is sRGB
		pShaderShadow->EnableTexture(SAMPLER_LIGHTMAP, true);       // Lightmap texture
		pShaderShadow->EnableSRGBRead(SAMPLER_LIGHTMAP, false);     // Lightmaps aren't sRGB
		pShaderShadow->EnableTexture(SAMPLER_MRO, true);           // MRO texture
		pShaderShadow->EnableSRGBRead(SAMPLER_MRO, false);         // MRO isn't sRGB
		pShaderShadow->EnableTexture(SAMPLER_NORMAL, true);         // Normal texture
		pShaderShadow->EnableSRGBRead(SAMPLER_NORMAL, false);       // Normals aren't sRGB
		pShaderShadow->EnableTexture(SAMPLER_ACCUMULATE_TEXTURE, true);    // dynamic accumulate
		pShaderShadow->EnableSRGBRead(SAMPLER_ACCUMULATE_TEXTURE, true);   // is sRGB
		pShaderShadow->EnableTexture(SAMPLER_ACCUMULATE_BUMP, true);    // dynamic accumulate bump
		pShaderShadow->EnableSRGBRead(SAMPLER_ACCUMULATE_BUMP, false);   // is a normal map, no sRGB

		// If the flashlight is on, set up its textures
		if (bHasFlashlight)
		{
			pShaderShadow->EnableTexture(SAMPLER_SHADOWDEPTH, true);        // Shadow depth map
			pShaderShadow->SetShadowDepthFiltering(SAMPLER_SHADOWDEPTH);
			pShaderShadow->EnableSRGBRead(SAMPLER_SHADOWDEPTH, false);
			pShaderShadow->EnableTexture(SAMPLER_RANDOMROTATION, true);     // Noise map
			pShaderShadow->EnableTexture(SAMPLER_FLASHLIGHT, true);         // Flashlight cookie
			pShaderShadow->EnableSRGBRead(SAMPLER_FLASHLIGHT, true);

			FogToBlack(); // todo: explain why
		}
		else
		{
			DefaultFog(); // todo: explain why
		}

		// Setting up envmap
		if (bHasEnvTexture)
		{
			pShaderShadow->EnableTexture(SAMPLER_ENVMAP, true); // Envmap
			if (g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE)
			{
				pShaderShadow->EnableSRGBRead(SAMPLER_ENVMAP, true); // Envmap is only sRGB with HDR disabled?
			}
			
			if (nEnvmapmaskMode == 1)
			{
				pShaderShadow->EnableTexture(SAMPLER_EMISSIVE, true);
				pShaderShadow->EnableSRGBRead(SAMPLER_EMISSIVE, true);
			}
		}

		// parallax
		if (bHasParallax1Layer)
		{
			pShaderShadow->EnableTexture(SAMPLER_PARALLAX1, true); 
			pShaderShadow->EnableSRGBRead(SAMPLER_PARALLAX1, true); 
		}

		if (bHasParallax2Layer)
		{
			pShaderShadow->EnableTexture(SAMPLER_PARALLAX2, true);
			pShaderShadow->EnableSRGBRead(SAMPLER_PARALLAX2, true);
		}

/*
		// Detail texture
		if (bHasDetailTexture)
			pShaderShadow->EnableTexture(SAMPLER_DETAIL, true);
		{
			if (params[info.detailBlendMode]->GetIntValue() > 0)
			{
				pShaderShadow->EnableSRGBRead(SAMPLER_DETAIL, false);
			}
			else
			{
				pShaderShadow->EnableSRGBRead(SAMPLER_DETAIL, true);
			}
		}
*/		
		// Enabling sRGB writing
		// See common_ps_fxc.h line 349
		// PS2b shaders and up write sRGB
		pShaderShadow->EnableSRGBWrite(true);

		if (IS_FLAG_SET(MATERIAL_VAR_MODEL))
		{
			// We only need the position and surface normal
			unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;
			// We need three texcoords, all in the default float2 size
			pShaderShadow->VertexShaderVertexFormat(flags, 1, 0, 0);
		}
		else
		{
			// We need the position, surface normal, and vertex compression format
			unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL;
			// We only need one texcoord, in the default float2 size
			pShaderShadow->VertexShaderVertexFormat(flags, 3, 0, 0);
		}

		int useDynamicAccumulation = params[info.enableAccumulation]->GetIntValue() && (mat_pbr_disable_accumulation.GetBool() == 0);

		if (!g_pHardwareConfig->SupportsShaderModel_3_0() || mat_pbr_force_20b.GetBool())
		{
			// Setting up static vertex shader
			DECLARE_STATIC_VERTEX_SHADER(pbr_vs20b);
			SET_STATIC_VERTEX_SHADER_COMBO(BLENDED, false);
			SET_STATIC_VERTEX_SHADER_COMBO(TREESWAY, bTreeSway ? nTreeSwayMode : 0);
			SET_STATIC_VERTEX_SHADER(pbr_vs20b);

			// Setting up static pixel shader
			DECLARE_STATIC_PIXEL_SHADER(pbr_ps20b);
			SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHT, bHasFlashlight);
			SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode);
			SET_STATIC_PIXEL_SHADER_COMBO(LIGHTMAPPED, bLightMapped);
			SET_STATIC_PIXEL_SHADER_COMBO(EMISSIVE, bHasEmissiveTexture);
			SET_STATIC_PIXEL_SHADER_COMBO(ENVMAPMASKINALPHA, nEnvmapmaskMode);
			SET_STATIC_PIXEL_SHADER_COMBO(INVERTENVMAPMASK, !bInvertEnvMapMask);
			SET_STATIC_PIXEL_SHADER_COMBO(ENABLEACCUMULATION, useDynamicAccumulation);
			SET_STATIC_PIXEL_SHADER_COMBO(ACCUMULATEBUMPMAP, bHasAccumulateBumpmap);
			SET_STATIC_PIXEL_SHADER_COMBO(BLENDED, false); // cannot have blends on models
			SET_STATIC_PIXEL_SHADER_COMBO(BLENDMODULATE, false);
			SET_STATIC_PIXEL_SHADER_COMBO(ALPHABLEND, bUseAlphaBlend);
			SET_STATIC_PIXEL_SHADER(pbr_ps20b);
		}
		else
		{
			// Setting up static vertex shader
			DECLARE_STATIC_VERTEX_SHADER(pbr_vs30);
			SET_STATIC_VERTEX_SHADER_COMBO(BLENDED, false);
			SET_STATIC_VERTEX_SHADER_COMBO(TREESWAY, bTreeSway ? nTreeSwayMode : 0);
			SET_STATIC_VERTEX_SHADER(pbr_vs30);

			// Setting up static pixel shader
			DECLARE_STATIC_PIXEL_SHADER(pbr_ps30);
			SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHT, bHasFlashlight);
			SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode);
			SET_STATIC_PIXEL_SHADER_COMBO(LIGHTMAPPED, bLightMapped);
			SET_STATIC_PIXEL_SHADER_COMBO(USEENVAMBIENT, bUseEnvAmbient);
			SET_STATIC_PIXEL_SHADER_COMBO(ENVMAPMASKINALPHA, nEnvmapmaskMode);
			SET_STATIC_PIXEL_SHADER_COMBO(INVERTENVMAPMASK, !bInvertEnvMapMask);
			SET_STATIC_PIXEL_SHADER_COMBO(EMISSIVE, bHasEmissiveTexture);
			SET_STATIC_PIXEL_SHADER_COMBO(PARALLAXLAYER, bHasParallax1Layer);
			SET_STATIC_PIXEL_SHADER_COMBO(ENABLEACCUMULATION, useDynamicAccumulation);
			SET_STATIC_PIXEL_SHADER_COMBO(ACCUMULATEBUMPMAP, bHasAccumulateBumpmap);
			SET_STATIC_PIXEL_SHADER_COMBO(BLENDED, false); // cannot have blends on models
			SET_STATIC_PIXEL_SHADER_COMBO(BLENDMODULATE, false);
			SET_STATIC_PIXEL_SHADER_COMBO(ALPHABLEND, bUseAlphaBlend);
			SET_STATIC_PIXEL_SHADER(pbr_ps30);
		}

		// Setting up fog
		//	DefaultFog(); // todo: should it be here or above in flashlight check?

		// HACK HACK HACK - enable alpha writes all the time so that we have them for underwater stuff
		pShaderShadow->EnableAlphaWrites(bFullyOpaque);
	}
	else // Not snapshotting -- begin dynamic state
	{
		bool bLightingOnly = mat_fullbright.GetInt() == 2 && !IS_FLAG_SET(MATERIAL_VAR_NO_DEBUG_OVERRIDE);

		// Setting up albedo texture
		if (bHasBaseTexture)
		{
			BindTexture(SAMPLER_BASETEXTURE, info.baseTexture, info.baseTextureFrame);
		}
		else
		{
			pShaderAPI->BindStandardTexture(SAMPLER_BASETEXTURE, TEXTURE_GREY);
		}

		// Setting up vmt color
		float accumulateBaseStrength = params[info.accumulateBaseStrength]->GetFloatValue();
		Vector color;
		if (bHasColor)
		{
		//	if (bUseAlphaBlend)
		//	{
		//		params[info.alphaBlend]->GetVecValue(color.Base(), 3);
		//	}
		//	else
				params[info.baseColor]->GetVecValue(color.Base(), 3);
		}
		else
		{
			color = Vector{ 1.f, 1.f, 1.f };
		}
		float baseParams[4] = { color[0], color[1], color[2], accumulateBaseStrength};
		pShaderAPI->SetPixelShaderConstant(PSREG_SELFILLUMTINT, baseParams);

		// Dynamic accumulation
		float accumParams[4] = { 0.0, 0.0, 1.0, 0.5 };
		float accumParams2[4] = { 0.0, 0.0, 0.0, 0.0 };
		if (bUseAlphaBlend || bEnableAccumulation)
		{			
			// if alphablend is 0 (and it always is for brushes), alphablendcolor is still used - for accumulate tinting
			Vector alphaBlendColor;
			params[info.alphaBlendColor]->GetVecValue(alphaBlendColor.Base(), 3);
			accumParams2[1] = (alphaBlendColor.x);
			accumParams2[2] = (alphaBlendColor.y);
			accumParams2[3] = (alphaBlendColor.z);
		}
		if (bEnableAccumulation)
		{
#if 1
			Vector accumDir = pShaderAPI->GetVectorRenderingParameter(VECTOR_RENDERPARM_SNOW_DIRECTION);
			float accumDot = pShaderAPI->GetFloatRenderingParameter(FLOAT_RENDERPARM_SNOW_DOT);
#else
			Vector accumDir = Vector(0.f, 0.f, 1.0f);
			float accumDot = 0.5f;
			accumDot = mat_pbr_dynamicsnow_dot.GetFloat();
#endif
			if (params[info.accumDotOverride]->GetFloatValue() > 0.0f)
			{
				accumDot = params[info.accumDotOverride]->GetFloatValue();
			}

			if (params[info.accumDirOverride]->IsDefined())
			{
				//	float accumDirOverride[3] = { 0.0, 0.0, 1.0 };
				//	params[info.accumDirOverride]->GetVecValue(accumDirOverride, 3);

				//	if (accumDirOverride[0] > -1.0 && accumDirOverride[1] > -1.0 && accumDirOverride[2] > -1.0)
				//	{
				//		accumDir.x = accumDirOverride[0];
				//		accumDir.y = accumDirOverride[1];
				//		accumDir.z = accumDirOverride[2];
				//	}
			}

			accumParams[0] = accumDir.x;
			accumParams[1] = accumDir.y;
			accumParams[2] = accumDir.z;
			accumParams[3] = accumDot;

			accumParams2[0] = IS_PARAM_DEFINED(info.accumulateBumpStrength) ? params[info.accumulateBumpStrength]->GetFloatValue() : 1.0f;

			pShaderAPI->SetPixelShaderConstant(PSREG_ENVMAP_FRESNEL__SELFILLUMMASK, accumParams);
			pShaderAPI->SetPixelShaderConstant(PSREG_SELFILLUM_SCALE_BIAS_EXP, accumParams2);

			if (bHasAccumulateTexture)
			{
				BindTexture(SAMPLER_ACCUMULATE_TEXTURE, info.accumulateTexture);
			}
			else
			{
				pShaderAPI->BindStandardTexture(SAMPLER_ACCUMULATE_TEXTURE, TEXTURE_WHITE);
			}

			if (bHasAccumulateBumpmap)
			{
				BindTexture(SAMPLER_ACCUMULATE_BUMP, info.accumulateBumpmap);
			}
			else
			{
				if (bHasNormalTexture)
				{
					BindTexture(SAMPLER_ACCUMULATE_BUMP, info.normalTexture);
				}
				else
				{
					pShaderAPI->BindStandardTexture(SAMPLER_ACCUMULATE_BUMP, TEXTURE_NORMALMAP_FLAT);
				}
			}
		}

		// Setting up emissive texture
		if (bHasEmissiveTexture)
		{
			BindTexture(SAMPLER_EMISSIVE, info.emissiveTexture, 0);
		}
		else
		{
			pShaderAPI->BindStandardTexture(SAMPLER_EMISSIVE, TEXTURE_BLACK);
		}

		// Setting up environment map
		if (bHasEnvTexture)
		{
			BindTexture(SAMPLER_ENVMAP, info.envMap, 0);

			if (nEnvmapmaskMode == 1)
			{
				BindTexture(SAMPLER_EMISSIVE, info.envMapMask, info.envMapMaskFrame);
			}
		}
		else
		{
			pShaderAPI->BindStandardTexture(SAMPLER_ENVMAP, TEXTURE_BLACK);
		}
/*
		if (bHasDetailTexture)
		{
			//DevMsg("Detail Texture Set \n");
			BindTexture(SHADER_SAMPLER15, info.detailTexture, 0);
		}
*/

		// Setting up emissive scale
		Vector EmissiveTint;
		float EmissiveTintAndScale[4] = { 1.0, 1.0, 1.0, 1.0 };
		if (bHasEmissiveTint)
		{
			params[info.emissiveTint]->GetVecValue(EmissiveTint.Base(), 3);
		}
		else
		{
			EmissiveTint.Init(1.0f, 1.0f, 1.0f);
		}
		EmissiveTintAndScale[0] = EmissiveTint.x;
		EmissiveTintAndScale[1] = EmissiveTint.y;
		EmissiveTintAndScale[2] = EmissiveTint.z;
		EmissiveTintAndScale[3] = 1.0f;

		pShaderAPI->SetPixelShaderConstant(2, EmissiveTintAndScale);

		// Setting up normal map
		if (bHasNormalTexture)
		{
			BindTexture(SAMPLER_NORMAL, info.bumpMap, 0);
		}
		else
		{
			pShaderAPI->BindStandardTexture(SAMPLER_NORMAL, TEXTURE_NORMALMAP_FLAT);
		}

		// Setting up mro map
		if (bHasMroTexture)
		{
			BindTexture(SAMPLER_MRO, info.mroTexture, info.mroFrame);
		}
		else
		{
			pShaderAPI->BindStandardTexture(SAMPLER_MRO, TEXTURE_WHITE);
		}

		// Parallax
		// Setting up mro map
		if (bHasParallax1Layer)
		{
			BindTexture(SAMPLER_PARALLAX1, info.parallax1texture, 0); // basetexture2 - sampler3 - is our parallax too, because parallax doesn't happen in blend

			float parallaxParams[4] = { 0.5, 1.0, 1.0, 0.0 };

			parallaxParams[0] = IS_PARAM_DEFINED(info.parallax1Depth)
				? params[info.parallax1Depth]->GetFloatValue() : 0.5f;

			parallaxParams[1] = IS_PARAM_DEFINED(info.parallax1Strength)
				? params[info.parallax1Strength]->GetFloatValue() : 0.5f;

			parallaxParams[2] = IS_PARAM_DEFINED(info.parallax1UVScale)
				? params[info.parallax1UVScale]->GetFloatValue() : 1.0;
			
			pShaderAPI->SetPixelShaderConstant(19, parallaxParams);
			
			if (bHasParallax2Layer)
			{
				BindTexture(SAMPLER_PARALLAX2, info.parallax2texture, 0); // basetexture2 - sampler3 - is our parallax too, because parallax doesn't happen in blend

				float parallaxParams2[4] = { 0.5, 1.0, 1.0, 0.0 };

				parallaxParams2[0] = IS_PARAM_DEFINED(info.parallax2Depth)
					? params[info.parallax2Depth]->GetFloatValue() : 0.5f;

				parallaxParams2[1] = IS_PARAM_DEFINED(info.parallax2Strength)
					? params[info.parallax2Strength]->GetFloatValue() : 0.5f;

				parallaxParams2[2] = IS_PARAM_DEFINED(info.parallax2UVScale)
					? params[info.parallax2UVScale]->GetFloatValue() : 1.0;

				pShaderAPI->SetPixelShaderConstant(26, parallaxParams2);
			}
			else
			{
				pShaderAPI->BindStandardTexture(SAMPLER_PARALLAX2, TEXTURE_WHITE);
			}
		}
		else
		{
		//	pShaderAPI->BindStandardTexture(SAMPLER_PARALLAX1, TEXTURE_WHITE);
		}

		// Getting the light state
		LightState_t lightState;
		pShaderAPI->GetDX9LightState(&lightState);

		// Brushes don't need ambient cubes or dynamic lights
		if (!IS_FLAG_SET(MATERIAL_VAR_MODEL))
		{
			lightState.m_bAmbientLight = false;
			lightState.m_nNumLights = 0;
		}

		// Setting up the flashlight related textures and variables
		bool bFlashlightShadows = false;
		if (bHasFlashlight)
		{
			Assert(info.flashlightTexture >= 0 && info.flashlightTextureFrame >= 0);
			Assert(params[info.flashlightTexture]->IsTexture());
			BindTexture(SAMPLER_FLASHLIGHT, info.flashlightTexture, info.flashlightTextureFrame);
			VMatrix worldToTexture;
			ITexture *pFlashlightDepthTexture;
			FlashlightState_t state = pShaderAPI->GetFlashlightStateEx(worldToTexture, &pFlashlightDepthTexture);
			bFlashlightShadows = state.m_bEnableShadows && (pFlashlightDepthTexture != NULL);

			SetFlashLightColorFromState(state, pShaderAPI, PSREG_FLASHLIGHT_COLOR);

			if (pFlashlightDepthTexture && g_pConfig->ShadowDepthTexture() && state.m_bEnableShadows)
			{
				BindTexture(SAMPLER_SHADOWDEPTH, pFlashlightDepthTexture, 0);
				pShaderAPI->BindStandardTexture(SAMPLER_RANDOMROTATION, TEXTURE_SHADOW_NOISE_2D);
			}
		}

		// Getting fog info
		MaterialFogMode_t fogType = pShaderAPI->GetSceneFogMode();
		int fogIndex = (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z) ? 1 : 0;

		// Getting skinning info
		int numBones = pShaderAPI->GetCurrentNumBones();

		// Some debugging stuff
		bool bWriteDepthToAlpha = false;
		bool bWriteWaterFogToAlpha = false;
		if (bFullyOpaque)
		{
			bWriteDepthToAlpha = pShaderAPI->ShouldWriteDepthToDestAlpha();
			bWriteWaterFogToAlpha = (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z);
			AssertMsg(!(bWriteDepthToAlpha && bWriteWaterFogToAlpha),
				"Can't write two values to alpha at the same time.");
		}

		float vEyePos_SpecExponent[4];
		pShaderAPI->GetWorldSpaceCameraPosition(vEyePos_SpecExponent);

		// Determining the max level of detail for the envmap
		int iEnvMapLOD = 6;
		auto envTexture = params[info.envMap]->GetTextureValue();
		if (envTexture)
		{
			// Get power of 2 of texture width
			int width = envTexture->GetMappingWidth();
			int mips = 0;
			while (width >>= 1)
				++mips;

			// Cubemap has 4 sides so 2 mips less
			iEnvMapLOD = mips;
		}

		// Dealing with very high and low resolution cubemaps
		if (iEnvMapLOD > 12)
			iEnvMapLOD = 12;
		if (iEnvMapLOD < 4)
			iEnvMapLOD = 4;

		// This has some spare space
		vEyePos_SpecExponent[3] = iEnvMapLOD;
		pShaderAPI->SetPixelShaderConstant(PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1);

		if (!g_pHardwareConfig->SupportsShaderModel_3_0() || mat_pbr_force_20b.GetBool())
		{
			// Setting up dynamic vertex shader
			DECLARE_DYNAMIC_VERTEX_SHADER(pbr_vs20b);
			SET_DYNAMIC_VERTEX_SHADER_COMBO(DOWATERFOG, fogIndex);
			SET_DYNAMIC_VERTEX_SHADER_COMBO(SKINNING, numBones > 0);
			SET_DYNAMIC_VERTEX_SHADER_COMBO(LIGHTING_PREVIEW, pShaderAPI->GetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING) != 0);
			SET_DYNAMIC_VERTEX_SHADER_COMBO(COMPRESSED_VERTS, (int)vertexCompression);
			SET_DYNAMIC_VERTEX_SHADER_COMBO(NUM_LIGHTS, lightState.m_nNumLights);
			SET_DYNAMIC_VERTEX_SHADER(pbr_vs20b);

			// Setting up dynamic pixel shader
			DECLARE_DYNAMIC_PIXEL_SHADER(pbr_ps20b);
			SET_DYNAMIC_PIXEL_SHADER_COMBO(NUM_LIGHTS, lightState.m_nNumLights);
			SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha);
			SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITE_DEPTH_TO_DESTALPHA, bWriteDepthToAlpha);
			SET_DYNAMIC_PIXEL_SHADER_COMBO(PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo());
			SET_DYNAMIC_PIXEL_SHADER_COMBO(FLASHLIGHTSHADOWS, bFlashlightShadows);
			SET_DYNAMIC_PIXEL_SHADER(pbr_ps20b);
		}
		else
		{
			// Setting up dynamic vertex shader
			DECLARE_DYNAMIC_VERTEX_SHADER(pbr_vs30);
			SET_DYNAMIC_VERTEX_SHADER_COMBO(DOWATERFOG, fogIndex);
			SET_DYNAMIC_VERTEX_SHADER_COMBO(SKINNING, numBones > 0);
			SET_DYNAMIC_VERTEX_SHADER_COMBO(LIGHTING_PREVIEW, pShaderAPI->GetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING) != 0);
			SET_DYNAMIC_VERTEX_SHADER_COMBO(COMPRESSED_VERTS, (int)vertexCompression);
			SET_DYNAMIC_VERTEX_SHADER_COMBO(NUM_LIGHTS, lightState.m_nNumLights);
			SET_DYNAMIC_VERTEX_SHADER(pbr_vs30);

			// Setting up dynamic pixel shader
			DECLARE_DYNAMIC_PIXEL_SHADER(pbr_ps30);
			SET_DYNAMIC_PIXEL_SHADER_COMBO(NUM_LIGHTS, lightState.m_nNumLights);
			SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha);
			SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITE_DEPTH_TO_DESTALPHA, bWriteDepthToAlpha);
			SET_DYNAMIC_PIXEL_SHADER_COMBO(PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo());
			SET_DYNAMIC_PIXEL_SHADER_COMBO(FLASHLIGHTSHADOWS, bFlashlightShadows);
			SET_DYNAMIC_PIXEL_SHADER(pbr_ps30);
		}

		// Setting up base texture transform
		SetVertexShaderTextureTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, info.baseTextureTransform);

		if (bHasAccumulateTexture && g_pHardwareConfig->SupportsShaderModel_3_0() && !mat_pbr_force_20b.GetBool())
			SetVertexShaderTextureTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_10, info.accumulateTextureTransform);

		if (/*bHasDetailTexture ||*/ bTreeSway)
			SetVertexShaderTextureScaledTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, info.baseTextureTransform, 1);

		// Treesway
		if (bTreeSway)
		{
			float flParams[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

			flParams[0] = GetFloatParam(info.m_nTreeSwaySpeedHighWindMultiplier, params, 2.0f);
			flParams[1] = GetFloatParam(info.m_nTreeSwayScrumbleFalloffExp, params, 1.0f);
			flParams[2] = GetFloatParam(info.m_nTreeSwayFalloffExp, params, 1.0f);
			flParams[3] = GetFloatParam(info.m_nTreeSwayScrumbleSpeed, params, 3.0f);
			pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, flParams);

			flParams[0] = GetFloatParam(info.m_nTreeSwaySpeedLerpStart, params, 3.0f);
			flParams[1] = GetFloatParam(info.m_nTreeSwaySpeedLerpEnd, params, 6.0f);
			flParams[2] = 0;
			flParams[3] = 0;
			pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, flParams);

			flParams[0] = GetFloatParam(info.m_nTreeSwayHeight, params, 1000.0f);
			flParams[1] = GetFloatParam(info.m_nTreeSwayStartHeight, params, 0.1f);
			flParams[2] = GetFloatParam(info.m_nTreeSwayRadius, params, 300.0f);
			flParams[3] = GetFloatParam(info.m_nTreeSwayStartRadius, params, 0.2f);
			pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_8, flParams);

			flParams[0] = GetFloatParam(info.m_nTreeSwaySpeed, params, 1.0f);
			flParams[1] = GetFloatParam(info.m_nTreeSwayStrength, params, 10.0f);
			flParams[2] = GetFloatParam(info.m_nTreeSwayScrumbleFrequency, params, 12.0f);
			flParams[3] = GetFloatParam(info.m_nTreeSwayScrumbleStrength, params, 10.0f);
			pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_9, flParams);
		}
		//

		// This is probably important
		SetModulationPixelShaderDynamicState_LinearColorSpace(PSREG_DIFFUSE_MODULATION);

		// Send ambient cube to the pixel shader, force to black if not available
		pShaderAPI->SetPixelShaderStateAmbientLightCube(PSREG_AMBIENT_CUBE, !lightState.m_bAmbientLight);

		// Send lighting array to the pixel shader
		pShaderAPI->CommitPixelShaderLighting(PSREG_LIGHT_INFO_ARRAY);

		// Handle mat_fullbright 2 (diffuse lighting only)
		if (bLightingOnly)
		{
			pShaderAPI->BindStandardTexture(SAMPLER_BASETEXTURE, TEXTURE_GREY); // Basecolor
		}

		// Handle mat_specular 0 (no envmap reflections)
		if (!mat_specular.GetBool())
		{
			pShaderAPI->BindStandardTexture(SAMPLER_ENVMAP, TEXTURE_BLACK); // Envmap
		}

		// Sending fog info to the pixel shader
		pShaderAPI->SetPixelShaderFogParams(PSREG_FOG_PARAMS);

		// Set up shader modulation color
		float modulationColor[4] = { 1.0, 1.0, 1.0, 1.0 };
		ComputeModulationColor(modulationColor);
		float flLScale = pShaderAPI->GetLightMapScaleFactor();
		modulationColor[0] *= flLScale;
		modulationColor[1] *= flLScale;
		modulationColor[2] *= flLScale;
		pShaderAPI->SetPixelShaderConstant(PSREG_DIFFUSE_MODULATION, modulationColor);

		// MRO tweaks
		Vector MROtint;
		float MROTintReplace;
		float MROtintTweaks[4] = { 1.0, 1.0, 1.0, 1.0 };
		if (bHasMROTint)
		{
			params[info.MROtint]->GetVecValue(MROtint.Base(), 3);
			MROTintReplace = IS_PARAM_DEFINED(info.MROtintReplace) ? 1.0f : 0.0f;
		}
		else
		{
			MROtint.Init(1.0f, 1.0f, 1.0f);
			MROTintReplace = 0.0f;
		}
		MROtintTweaks[0] = MROtint.x;
		MROtintTweaks[1] = MROtint.y;
		MROtintTweaks[2] = MROtint.z;
		MROtintTweaks[3] = MROTintReplace;

		pShaderAPI->SetPixelShaderConstant(27, MROtintTweaks);

		// More flashlight related stuff
		if (bHasFlashlight)
		{
			VMatrix worldToTexture;
			float atten[4], pos[4], tweaks[4];

			const FlashlightState_t &flashlightState = pShaderAPI->GetFlashlightState(worldToTexture);
			SetFlashLightColorFromState(flashlightState, pShaderAPI, PSREG_FLASHLIGHT_COLOR);

			BindTexture(SAMPLER_FLASHLIGHT, flashlightState.m_pSpotlightTexture, flashlightState.m_nSpotlightTextureFrame);

			// Set the flashlight attenuation factors
			atten[0] = flashlightState.m_fConstantAtten;
			atten[1] = flashlightState.m_fLinearAtten;
			atten[2] = flashlightState.m_fQuadraticAtten;
			atten[3] = flashlightState.m_FarZ;
			pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_ATTENUATION, atten, 1);

			// Set the flashlight origin
			pos[0] = flashlightState.m_vecLightOrigin[0];
			pos[1] = flashlightState.m_vecLightOrigin[1];
			pos[2] = flashlightState.m_vecLightOrigin[2];
			pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_POSITION_RIM_BOOST, pos, 1);

			pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_TO_WORLD_TEXTURE, worldToTexture.Base(), 4);

			// Tweaks associated with a given flashlight
			tweaks[0] = ShadowFilterFromState(flashlightState);
			tweaks[1] = ShadowAttenFromState(flashlightState);
			HashShadow2DJitter(flashlightState.m_flShadowJitterSeed, &tweaks[2], &tweaks[3]);
			pShaderAPI->SetPixelShaderConstant(PSREG_ENVMAP_TINT__SHADOW_TWEAKS, tweaks, 1);
		}
		else
		{
			float fEnvMapSaturation = 1;
			fEnvMapSaturation = params[info.envMapSaturation]->GetFloatValue();
			float fEnvMapContrast = 0;
			fEnvMapContrast = params[info.envMapContrast]->GetFloatValue();

			if (!params[info.envMapSaturation]->GetFloatValue() != 0)
			{
				fEnvMapSaturation = 1.0f;
			}

			// Packing to fit it into 32 constant registers.
			float fEnvMapTint[3] = { 0, 0, 0 };
			params[info.envMapTint]->GetVecValue(fEnvMapTint, 3);
			// floor makes sure that envmapcontrast values below 0.01 doesn't affect the saturation.
			float fCombine[4] = { fEnvMapTint[0], fEnvMapTint[1], fEnvMapTint[2], (floor(fEnvMapContrast * 100) * 0.01) + (fEnvMapSaturation * 0.01) };

			pShaderAPI->SetPixelShaderConstant(31, fCombine);
		}

		// Treesway
		if (bTreeSway)
		{
			float fTempConst[4];
			fTempConst[1] = pShaderAPI->CurrentTime();
			if (params[info.m_nTreeSwayStatic]->GetIntValue() == 0)
			{
				const Vector& windDir = pShaderAPI->GetVectorRenderingParameter(VECTOR_RENDERPARM_WIND_DIRECTION);
				fTempConst[2] = windDir.x;
				fTempConst[3] = windDir.y;
			}
			else
			{
				// Use a static value instead of the env_wind value.
				params[info.m_nTreeSwayStaticValues]->GetVecValue(fTempConst + 2, 2);
			}
			pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_7, fTempConst);
		}

	}

	// Actually draw the shader
	Draw();
};

// Closing it off
END_SHADER;