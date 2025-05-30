#ifndef PORTAL_DYNAMICMESHRENDERINGUTILS_H
#define PORTAL_DYNAMICMESHRENDERINGUTILS_H

#ifdef _WIN32
#pragma once
#endif

#ifdef PORTAL_COMPILE

#include "materialsystem/IMaterial.h"
#include "mathlib/mathlib.h"
#include "portalrender.h"

struct PortalMeshPoint_t
{
	Vector vWorldSpacePosition;
	Vector2D texCoord;
};

int ClipPolyToPlane_LerpTexCoords( PortalMeshPoint_t *inVerts, int vertCount, PortalMeshPoint_t *outVerts, const Vector& normal, float dist, float fOnPlaneEpsilon );
void RenderPortalMeshConvexPolygon( PortalMeshPoint_t *pVerts, int iVertCount, const IMaterial *pMaterial, void *pBind );

void Clip_And_Render_Convex_Polygon( PortalMeshPoint_t *pVerts, int iVertCount, const IMaterial *pMaterial, void *pBind );


#endif //#ifndef PORTAL_DYNAMICMESHRENDERINGUTILS_H
#endif