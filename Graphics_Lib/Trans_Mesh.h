#ifndef TRANS_MESH_H
#define TRANS_MESH_H

#include "Static_Mesh.h"
#include "../Utilities/MyMath.h"

class Trans_Mesh : public Static_Mesh{
public:

	Trans_Mesh(){ }
	virtual ~Trans_Mesh(){ DeInit();}

	virtual bool Init();

	float HighLight_Hit_Axis(const vec3& rayorig, const vec3& raydir);
	virtual float Ray_Tri_Intersect(const vec3& rayorig, const vec3& raydir) override;
	virtual float Ray_BV_Intersect(const vec3& rayorig, const vec3& raydir) override{ return Ray_Tri_Intersect(rayorig, raydir);}// just calls Ray_Tri_Intersect

	// need to make sure no funny stuff can happen. There is no rotation or scaling or position with the trans tool.. its automatic
	virtual void SetScaling(const vec3& v) {}
	virtual void SetRotation(const quat& q) { }
	virtual void SetRotation(const euler& e)override{ }

	virtual void Draw(const Base_Camera* camera, float dt=0) override;
	vec3 HitAxis;

protected:

	float Ray_Tri_Intersect(const vec3& rayorig, const vec3& raydir, mat4& world, uint16_t startindex, uint16_t numindices) const;
	vec3 LastCamPosition;
	vec3 XAxis_Color, YAxis_Color, ZAxis_Color, Hit_Axis_Color;

};


#endif

