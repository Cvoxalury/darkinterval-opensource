#include "cbase.h"
#include "c_sharedents_encompass.h"
#include "clienteffectprecachesystem.h"
// --------------------------------------
//  Energy wave defs
// --------------------------------------
enum
{
	TESTSHEET_NUM_HORIZONTAL_POINTS = 10,
	TESTSHEET_NUM_VERTICAL_POINTS = 10,
	TESTSHEET_NUM_CONTROL_POINTS = TESTSHEET_NUM_HORIZONTAL_POINTS * TESTSHEET_NUM_VERTICAL_POINTS,
	TESTSHEET_INITIAL_THETA = 135,
	TESTSHEET_INITIAL_PHI = 135,
};

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
C_TestSheetEffect::C_TestSheetEffect(TraceLineFunc_t traceline,
	TraceHullFunc_t traceHull) :
	CSheetSimulator(traceline, traceHull)
{
	Init(TESTSHEET_NUM_HORIZONTAL_POINTS, TESTSHEET_NUM_VERTICAL_POINTS, TESTSHEET_NUM_CONTROL_POINTS);
}

//-----------------------------------------------------------------------------
// compute rest positions of the springs
//-----------------------------------------------------------------------------
void C_TestSheetEffect::ComputeRestPositions()
{
	int i;

	// Set the initial directions and distances (in TESTSHEET space)...
	for (i = 0; i < TESTSHEET_NUM_VERTICAL_POINTS; ++i)
	{
		// Choose phi centered at pi/2
		float phi = (M_PI - m_SheetPhi) * 0.5f + m_SheetPhi *
			(float)i / (float)(TESTSHEET_NUM_VERTICAL_POINTS - 1);

		for (int j = 0; j < TESTSHEET_NUM_HORIZONTAL_POINTS; ++j)
		{
			// Choose theta centered at pi/2 also (y, or forward axis)
			float theta = (M_PI - m_SheetTheta) * 0.5f + m_SheetTheta *
				(float)j / (float)(TESTSHEET_NUM_HORIZONTAL_POINTS - 1);

			int idx = i * TESTSHEET_NUM_HORIZONTAL_POINTS + j;

			GetFixedPoint(idx).x = cos(theta) * sin(phi) * m_RestDistance;
			GetFixedPoint(idx).y = sin(theta) * sin(phi) * m_RestDistance;
			GetFixedPoint(idx).z = cos(phi) * m_RestDistance;
		}
	}


	// Compute box for fake volume testing
	Vector dist = GetFixedPoint(0) - GetFixedPoint(1);
	float l = dist.Length();
#ifdef DARKINTERVAL  // Do not use MSVC nonstandard extensions as it reduces portability.
	Vector mins(-l * 0.25f, -l * 0.25f, -l * 0.25f);
	Vector maxs(l * 0.25f, l * 0.25f, l * 0.25f);
	SetBoundingBox(mins, maxs);
#else
	SetBoundingBox(Vector(-l * 0.25f, -l * 0.25f, -l * 0.25f),
		Vector(l * 0.25f, l * 0.25f, l * 0.25f));
#endif
}

void C_TestSheetEffect::MakeSpring(int p1, int p2)
{
	Vector dist = GetFixedPoint(p1) - GetFixedPoint(p2);
	AddSpring(p1, p2, dist.Length());
}

void C_TestSheetEffect::ComputeSprings()
{
	// Do the main springs first
	// Compute springs and rest lengths...
	int i, j;
	for (i = 0; i < TESTSHEET_NUM_VERTICAL_POINTS; ++i)
	{
		for (j = 0; j < TESTSHEET_NUM_HORIZONTAL_POINTS; ++j)
		{
			// Here's the particle we're making springs for
			int idx = i * TESTSHEET_NUM_HORIZONTAL_POINTS + j;

			// Make a spring connected to the control point
			AddFixedPointSpring(idx, idx, 0.0f);
		}
	}

	// Add four corner springs
	MakeSpring(0, 3);
	MakeSpring(0, 12);
	MakeSpring(12, 15);
	MakeSpring(3, 15);
}

//------------------------------------------------------------------------------
// Purpose : Overwrite.  Energy wave does no collisions
// Input   :
// Output  :
//------------------------------------------------------------------------------
/*
void C_TestSheetEffect::DetectCollisions()
{
	return;
}
*/
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
ConVar sheetFixedSpringConstant("sheet_fixed_spring_constant", "200");
ConVar sheetPointSpringConstant("sheet_point_spring_constant", "0.5");
ConVar sheetSpringDampConstant("sheet_spring_damp_constant", "10.0");
ConVar sheetViscousDrag("sheet_viscous_drag", "0.25");
ConVar sheetRestDistance("sheet_rest_distance", "200");
ConVar sheetTheta("sheet_theta", "135");
ConVar sheetPhi("sheet_phi", "135");


void C_TestSheetEffect::Precache(void)
{
	SetFixedSpringConstant(sheetFixedSpringConstant.GetFloat()); //(1.0f);
	SetPointSpringConstant(sheetPointSpringConstant.GetFloat()); //(0.5f);
	SetSpringDampConstant(sheetSpringDampConstant.GetFloat()); //(10.0f);
	SetViscousDrag(sheetViscousDrag.GetFloat()); //(1.0f);
	SetCollisionGroup(COLLISION_GROUP_NONE);
	m_RestDistance = sheetRestDistance.GetFloat(); //200.0;

	m_SheetTheta = M_PI * sheetTheta.GetInt() / 180.0f;
	m_SheetPhi = M_PI * sheetPhi.GetInt() / 180.0f;

	// Computes the rest positions
	ComputeRestPositions();

	// Computes the springs
	ComputeSprings();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TestSheetEffect::Spawn(void)
{
	Precache();
}

//-----------------------------------------------------------------------------
// Computes the opacity....
//-----------------------------------------------------------------------------
float C_TestSheetEffect::ComputeOpacity(const Vector& pt, const Vector& center) const
{
	float dist = pt.DistTo(center) / m_RestDistance;
	dist = sqrt(dist);
	if (dist > 1.0)
		dist = 1.0f;
	return (1.0f - dist) * 35;
}

#if 1
CLIENTEFFECT_REGISTER_BEGIN(PrecacheSheetEffect)
CLIENTEFFECT_MATERIAL("shadertest/wireframevertexcolor")
CLIENTEFFECT_MATERIAL("effects/energywave/energywave")
CLIENTEFFECT_REGISTER_END()

//EXTERN_RECV_TABLE(DT_BaseAnimating);

IMPLEMENT_CLIENTCLASS_DT(C_TestSheetEntity, DT_TestSheetEntity, CTestSheetEntity)
END_RECV_TABLE()

// ----------------------------------------------------------------------------
// Functions.
// ----------------------------------------------------------------------------

C_TestSheetEntity::C_TestSheetEntity() : m_SheetEffect(NULL, NULL)
{
	m_pWireframe = NULL; // materials->FindMaterial("shadertest/wireframevertexcolor", NULL);
	m_pSheetMat = NULL; // materials->FindMaterial("effects/energywave/energywave", 0);
}

C_TestSheetEntity::~C_TestSheetEntity()
{
}

//-----------------------------------------------------------------------------
// Purpose: Cache the material handles
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_TestSheetEntity::InitMaterials(void)
{
	// Motion blur
	if (m_pWireframe == NULL)
	{
		m_pWireframe = materials->FindMaterial("shadertest/wireframevertexcolor", NULL);

		if (m_pWireframe == NULL)
			return false;
	}

	// Main body of the ball
	if (m_pSheetMat == NULL)
	{
		m_pSheetMat = materials->FindMaterial("effects/energywave/energywave", 0);

		if (m_pSheetMat == NULL)
			return false;
	}

	m_SheetEffect.Spawn();
	
	return true;
}

void C_TestSheetEntity::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged(updateType);

	if (updateType == DATA_UPDATE_CREATED)
	{
		InitMaterials();
		SetNextClientThink(CLIENT_THINK_ALWAYS);
	}
}

void C_TestSheetEntity::PostDataUpdate(DataUpdateType_t updateType)
{
	MarkMessageReceived();

	// Make sure that origin points to current origin, at least
	MoveToLastReceivedPosition();

	BaseClass::PostDataUpdate(updateType);
}

enum
{
	NUM_SUBDIVISIONS = 21,
};

static void ComputeIndices(int is, int it, int* idx)
{
	int is0 = (is > 0) ? (is - 1) : is;
	int it0 = (it > 0) ? (it - 1) : it;
	int is1 = (is < TESTSHEET_NUM_HORIZONTAL_POINTS - 1) ? is + 1 : is;
	int it1 = (it < TESTSHEET_NUM_HORIZONTAL_POINTS - 1) ? it + 1 : it;
	int is2 = is + 2;
	int it2 = it + 2;
	if (is2 >= TESTSHEET_NUM_HORIZONTAL_POINTS)
		is2 = TESTSHEET_NUM_HORIZONTAL_POINTS - 1;
	if (it2 >= TESTSHEET_NUM_HORIZONTAL_POINTS)
		it2 = TESTSHEET_NUM_HORIZONTAL_POINTS - 1;

	idx[0] = is0 + it0 * TESTSHEET_NUM_HORIZONTAL_POINTS;
	idx[1] = is + it0 * TESTSHEET_NUM_HORIZONTAL_POINTS;
	idx[2] = is1 + it0 * TESTSHEET_NUM_HORIZONTAL_POINTS;
	idx[3] = is2 + it0 * TESTSHEET_NUM_HORIZONTAL_POINTS;

	idx[4] = is0 + it * TESTSHEET_NUM_HORIZONTAL_POINTS;
	idx[5] = is + it * TESTSHEET_NUM_HORIZONTAL_POINTS;
	idx[6] = is1 + it * TESTSHEET_NUM_HORIZONTAL_POINTS;
	idx[7] = is2 + it * TESTSHEET_NUM_HORIZONTAL_POINTS;

	idx[8] = is0 + it1 * TESTSHEET_NUM_HORIZONTAL_POINTS;
	idx[9] = is + it1 * TESTSHEET_NUM_HORIZONTAL_POINTS;
	idx[10] = is1 + it1 * TESTSHEET_NUM_HORIZONTAL_POINTS;
	idx[11] = is2 + it1 * TESTSHEET_NUM_HORIZONTAL_POINTS;

	idx[12] = is0 + it2 * TESTSHEET_NUM_HORIZONTAL_POINTS;
	idx[13] = is + it2 * TESTSHEET_NUM_HORIZONTAL_POINTS;
	idx[14] = is1 + it2 * TESTSHEET_NUM_HORIZONTAL_POINTS;
	idx[15] = is2 + it2 * TESTSHEET_NUM_HORIZONTAL_POINTS;
}

void C_TestSheetEntity::ComputePoint(float s, float t, Vector& pt, Vector& normal, float& opacity)
{
	int is = (int)s;
	int it = (int)t;
	if (is >= TESTSHEET_NUM_HORIZONTAL_POINTS)
		is -= 1;

	if (it >= TESTSHEET_NUM_VERTICAL_POINTS)
		it -= 1;

	int idx[16];
	ComputeIndices(is, it, idx);

	// The patch equation is:
	// px = S * M * Gx * M^T * T^T 
	// py = S * M * Gy * M^T * T^T 
	// pz = S * M * Gz * M^T * T^T 
	// where S = [s^3 s^2 s 1], T = [t^3 t^2 t 1]
	// M is the patch type matrix, in my case I'm using a catmull-rom
	// G is the array of control points. rows have constant t
	static VMatrix catmullRom(-0.5, 1.5, -1.5, 0.5,
		1, -2.5, 2, -0.5,
		-0.5, 0, 0.5, 0,
		0, 1, 0, 0);

	VMatrix controlPointsX, controlPointsY, controlPointsZ, controlPointsO;

	Vector pos;
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			const Vector& v = m_SheetEffect.GetPoint(idx[i * 4 + j]);

			controlPointsX[j][i] = v.x;
			controlPointsY[j][i] = v.y;
			controlPointsZ[j][i] = v.z;

			controlPointsO[j][i] = m_SheetEffect.ComputeOpacity(v, GetAbsOrigin());
		}
	}

	float fs = s - is;
	float ft = t - it;

	VMatrix temp, mgm[4];
	MatrixTranspose(catmullRom, temp);
	MatrixMultiply(controlPointsX, temp, mgm[0]);
	MatrixMultiply(controlPointsY, temp, mgm[1]);
	MatrixMultiply(controlPointsZ, temp, mgm[2]);
	MatrixMultiply(controlPointsO, temp, mgm[3]);

	MatrixMultiply(catmullRom, mgm[0], mgm[0]);
	MatrixMultiply(catmullRom, mgm[1], mgm[1]);
	MatrixMultiply(catmullRom, mgm[2], mgm[2]);
	MatrixMultiply(catmullRom, mgm[3], mgm[3]);

	Vector4D svec, tvec;
	float ft2 = ft * ft;
	tvec[0] = ft2 * ft; tvec[1] = ft2; tvec[2] = ft; tvec[3] = 1.0f;

	float fs2 = fs * fs;
	svec[0] = fs2 * fs; svec[1] = fs2; svec[2] = fs; svec[3] = 1.0f;

	Vector4D tmp;
	Vector4DMultiply(mgm[0], tvec, tmp);
	pt[0] = DotProduct4D(tmp, svec);
	Vector4DMultiply(mgm[1], tvec, tmp);
	pt[1] = DotProduct4D(tmp, svec);
	Vector4DMultiply(mgm[2], tvec, tmp);
	pt[2] = DotProduct4D(tmp, svec);

	Vector4DMultiply(mgm[3], tvec, tmp);
	opacity = DotProduct4D(tmp, svec);

	if ((s == 0.0f) || (t == 0.0f) ||
		(s == (TESTSHEET_NUM_HORIZONTAL_POINTS - 1.0f)) || (t == (TESTSHEET_NUM_VERTICAL_POINTS - 1.0f)))
	{
		opacity = 0.0f;
	}

	if ((s <= 0.3) || (t < 0.3))
	{
		opacity *= 0.35f;
	}
	if ((s == (TESTSHEET_NUM_HORIZONTAL_POINTS - 0.7f)) || (t == (TESTSHEET_NUM_VERTICAL_POINTS - 0.7f)))
	{
		opacity *= 0.35f;
	}

	if (opacity < 0.0f)
		opacity = 0.0f;
	else if (opacity > 255.0f)
		opacity = 255.0f;

	// Normal computation
	Vector4D dsvec, dtvec;
	dsvec[0] = 3.0f * fs2; dsvec[1] = 2.0f * fs; dsvec[2] = 1.0f; dsvec[3] = 0.0f;
	dtvec[0] = 3.0f * ft2; dtvec[1] = 2.0f * ft; dtvec[2] = 1.0f; dtvec[3] = 0.0f;

	Vector ds, dt;
	Vector4DMultiply(mgm[0], tvec, tmp);
	ds[0] = DotProduct4D(tmp, dsvec);
	Vector4DMultiply(mgm[1], tvec, tmp);
	ds[1] = DotProduct4D(tmp, dsvec);
	Vector4DMultiply(mgm[2], tvec, tmp);
	ds[2] = DotProduct4D(tmp, dsvec);

	Vector4DMultiply(mgm[0], dtvec, tmp);
	dt[0] = DotProduct4D(tmp, svec);
	Vector4DMultiply(mgm[1], dtvec, tmp);
	dt[1] = DotProduct4D(tmp, svec);
	Vector4DMultiply(mgm[2], dtvec, tmp);
	dt[2] = DotProduct4D(tmp, svec);

	CrossProduct(ds, dt, normal);
	VectorNormalize(normal);
}

void C_TestSheetEntity::DrawWireframeModel()
{
	CMatRenderContextPtr pRenderContext(materials);
	IMesh* pMesh = pRenderContext->GetDynamicMesh(true, NULL, NULL, m_pWireframe);

	int numLines = (TESTSHEET_NUM_VERTICAL_POINTS - 1) * TESTSHEET_NUM_HORIZONTAL_POINTS +
		TESTSHEET_NUM_VERTICAL_POINTS * (TESTSHEET_NUM_HORIZONTAL_POINTS - 1);

	CMeshBuilder meshBuilder;
	meshBuilder.Begin(pMesh, MATERIAL_LINES, numLines);

	Vector tmp;
	for (int i = 0; i < TESTSHEET_NUM_VERTICAL_POINTS; ++i)
	{
		for (int j = 0; j < TESTSHEET_NUM_HORIZONTAL_POINTS; ++j)
		{
			if (i > 0)
			{
				meshBuilder.Position3fv(m_SheetEffect.GetPoint(j, i).Base());
				meshBuilder.Color4ub(255, 255, 255, 128);
				meshBuilder.AdvanceVertex();

				meshBuilder.Position3fv(m_SheetEffect.GetPoint(j, i - 1).Base());
				meshBuilder.Color4ub(255, 255, 255, 128);
				meshBuilder.AdvanceVertex();
			}

			if (j > 0)
			{
				meshBuilder.Position3fv(m_SheetEffect.GetPoint(j, i).Base());
				meshBuilder.Color4ub(255, 255, 255, 128);
				meshBuilder.AdvanceVertex();

				meshBuilder.Position3fv(m_SheetEffect.GetPoint(j - 1, i).Base());
				meshBuilder.Color4ub(255, 255, 255, 128);
				meshBuilder.AdvanceVertex();
			}
		}
	}

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Compute the TESTSHEET points using catmull-rom
//-----------------------------------------------------------------------------
void C_TestSheetEntity::ComputeTestSheetPoints(Vector* pt, Vector* normal, float* opacity)
{
	int i;
	for (i = 0; i < NUM_SUBDIVISIONS; ++i)
	{
		float t = (TESTSHEET_NUM_VERTICAL_POINTS - 1) * (float)i / (float)(NUM_SUBDIVISIONS - 1);
		for (int j = 0; j < NUM_SUBDIVISIONS; ++j)
		{
			float s = (TESTSHEET_NUM_HORIZONTAL_POINTS - 1) * (float)j / (float)(NUM_SUBDIVISIONS - 1);
			int idx = i * NUM_SUBDIVISIONS + j;

			ComputePoint(s, t, pt[idx], normal[idx], opacity[idx]);
		}
	}
}

//-----------------------------------------------------------------------------
// Draws the base TESTSHEET
//-----------------------------------------------------------------------------
#define TRANSITION_REGION_WIDTH 0.5f

void C_TestSheetEntity::DrawTestSheetPoints(Vector* pt, Vector* normal, float* opacity)
{
	CMatRenderContextPtr pRenderContext(materials);
	IMesh* pMesh = pRenderContext->GetDynamicMesh(true, NULL, NULL, m_pSheetMat);

	int numTriangles = (NUM_SUBDIVISIONS - 1) * (NUM_SUBDIVISIONS - 1) * 2;

	CMeshBuilder meshBuilder;
	meshBuilder.Begin(pMesh, MATERIAL_TRIANGLES, numTriangles);

	float du = 1.0f / (float)(NUM_SUBDIVISIONS - 1);
	float dv = du;

	unsigned char color[3];
	color[0] = 255;
	color[1] = 255;
	color[2] = 255;

	for (int i = 0; i < NUM_SUBDIVISIONS - 1; ++i)
	{
		float v = i * dv;
		for (int j = 0; j < NUM_SUBDIVISIONS - 1; ++j)
		{
			int idx = i * NUM_SUBDIVISIONS + j;
			float u = j * du;

			meshBuilder.Position3fv(pt[idx].Base());
			meshBuilder.Color4ub(color[0], color[1], color[2], opacity[idx]);
			meshBuilder.Normal3fv(normal[idx].Base());
			meshBuilder.TexCoord2f(0, u, v);
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv(pt[idx + NUM_SUBDIVISIONS].Base());
			meshBuilder.Color4ub(color[0], color[1], color[2], opacity[idx + NUM_SUBDIVISIONS]);
			meshBuilder.Normal3fv(normal[idx + NUM_SUBDIVISIONS].Base());
			meshBuilder.TexCoord2f(0, u, v + dv);
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv(pt[idx + 1].Base());
			meshBuilder.Color4ub(color[0], color[1], color[2], opacity[idx + 1]);
			meshBuilder.Normal3fv(normal[idx + 1].Base());
			meshBuilder.TexCoord2f(0, u + du, v);
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv(pt[idx + 1].Base());
			meshBuilder.Color4ub(color[0], color[1], color[2], opacity[idx + 1]);
			meshBuilder.Normal3fv(normal[idx + 1].Base());
			meshBuilder.TexCoord2f(0, u + du, v);
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv(pt[idx + NUM_SUBDIVISIONS].Base());
			meshBuilder.Color4ub(color[0], color[1], color[2], opacity[idx + NUM_SUBDIVISIONS]);
			meshBuilder.Normal3fv(normal[idx + NUM_SUBDIVISIONS].Base());
			meshBuilder.TexCoord2f(0, u, v + dv);
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv(pt[idx + NUM_SUBDIVISIONS + 1].Base());
			meshBuilder.Color4ub(color[0], color[1], color[2], opacity[idx + NUM_SUBDIVISIONS + 1]);
			meshBuilder.Normal3fv(normal[idx + NUM_SUBDIVISIONS + 1].Base());
			meshBuilder.TexCoord2f(0, u + du, v + dv);
			meshBuilder.AdvanceVertex();
		}
	}

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Main draw entry point
//-----------------------------------------------------------------------------
int	C_TestSheetEntity::DrawModel(int flags)
{
	if (!m_bReadyToDraw)
	{
		return 0;
	}

	// Make sure our materials are cached
	if (!InitMaterials())
	{
		//NOTENOTE: This means that a material was not found for the combine ball, so it may not render!
		AssertOnce(0);
		return 0;
	}
	
	// NOTE: We've got a stiff spring case here, we need to simulate at
	// a fairly fast timestep. A better solution would be to use an 
	// implicit method, which I'm going to not implement for the moment

	float dt = gpGlobals->frametime;
	m_SheetEffect.SetPosition(GetAbsOrigin(), GetAbsAngles());
	m_SheetEffect.Simulate(dt);

	Vector pt[NUM_SUBDIVISIONS * NUM_SUBDIVISIONS];
	Vector normal[NUM_SUBDIVISIONS * NUM_SUBDIVISIONS];
	float opacity[NUM_SUBDIVISIONS * NUM_SUBDIVISIONS];

	ComputeTestSheetPoints(pt, normal, opacity);

	DrawTestSheetPoints(pt, normal, opacity);

	DrawWireframeModel();

	BaseClass::DrawModel(flags);

	return 1;
}
#endif