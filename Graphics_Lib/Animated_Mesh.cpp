#include "stdafx.h"
#include "Animated_Mesh.h"
#include "Base_Camera.h"
#include "Assimp_Helper.h"
#include "Batch.h"

#define MAX_BONES 60

bool Animated_Mesh::Init(){
	return true;
}
void Animated_Mesh::DeInit(){
	OUTPUT_DEBUG_MSG("Animated_Mesh::DeInit");
	totaldt=0;
	Base_Mesh::DeInit();
	Bounding_Volume.clear();
	for(int i=0; i<MAX_VERTEXSTREAM; i++) VB[i].Destroy();
	IB.Destroy();
	for(size_t i =0; i< Batches.size(); i++) delete Batches[i];
	Batches.clear();
	CBuffer0.Destroy();
	CBuffer1.Destroy();
	CBuffer2.Destroy();
}
void Animated_Mesh::Generate_BV(){
	if(Vertices.empty()) return Bounding_Volume.clear();// create a small bounding volume to test against
	size_t i(0);
	do{	Bounding_Volume.Add(Vertices[i]); } while(++i!=Vertices.size());
}

float Animated_Mesh::Ray_Tri_Intersect(const vec3& rayorig, const vec3& raydir) {
	mat4 inverseW(GetWorld());
	inverseW.inverse();
	vec3 org(rayorig*inverseW), di(raydir);// transform these to the mesh's space so the checks are in object space, not world space
	TransformNormal(di, inverseW);
	// do all checks in OBJECT SPACE!!!
	di*=20000.0f;//make sure the ray is long enough
	if(Bounding_Volume.RayIntersect(org, di) == INFINITY) return INFINITY;// did not hit the aabb, therefore, we did not hit the object
	if(IB.Stride == 4){
		return RayTriangleIntersect(org, di, &Vertices[0], reinterpret_cast<const uint32_t*>(&Indices[0]),Indices.size()/2);
	} else {
		return RayTriangleIntersect(org, di, &Vertices[0],&Indices[0],Indices.size());
	}
}
float Animated_Mesh::Ray_BV_Intersect(const vec3& rayorig, const vec3& raydir) {
	mat4 inverseW(GetWorld());
	inverseW.inverse();
	vec3 org(rayorig*inverseW), di(raydir);// transform these to the mesh's space so the checks are in object space, not world space
	TransformNormal(di, inverseW);
	// do all checks in OBJECT SPACE!!!
	di*=20000.0f;//make sure the ray is long enough
	return Bounding_Volume.RayIntersect(org, di);
}
bool Animated_Mesh::Load(const std::string& file){
	OUTPUT_DEBUG_MSG("Animated_Mesh::Load file: '" + file+"'");
	if(!FileExists(file)) {
		OUTPUT_DEBUG_MSG("File does not exist");
		return false;
	}
	std::string ext = GetFileExtention(file);
	DeInit();// make sure to free any resources
	bool ret = false;
	if(ext == ".dsm") ret = Load_MyFormat(file);//.dsm is my file extention for models
	else ret = Load_Assimp(file);
	//Configure and load the appropriate shaders

	CBuffer0.Create(1, sizeof(Object_Transform), CONSTANT_BUFFER);
	CBuffer1.Create(1, sizeof(Material), CONSTANT_BUFFER);
	CBuffer2.Create(1, sizeof(mat4)*MAX_BONES, CONSTANT_BUFFER);
	if(ret) Generate_BV();
	return ret;
}
bool Animated_Mesh::Load_Assimp(const std::string& file){
	const aiScene* load =aiImportFile(file.c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded );
	if(load==NULL) {
		OUTPUT_DEBUG_MSG("Could not load the Model in Animated_Mesh::Load_MyFormat file: '" + file+"'");
		return false;
	}
	size_t numverts(0), currentvertex(0), currentindex(0), numindices(0);
	bool hasbones=false;
	// go through the mesh counting all the verts and indices that I will need
	for (unsigned int i = 0; i < load->mNumMeshes;++i) {
		numverts+=load->mMeshes[i]->mNumVertices;
		numindices+=load->mMeshes[i]->mNumFaces*3;
		if(load->mMeshes[i]->HasBones()) hasbones=true;
	}
	if(!hasbones) {
		aiReleaseImport(load);// free the resrouces
		OUTPUT_DEBUG_MSG("Could not load the Model in Animated_Mesh::Load_MyFormat file: '" + file+"', there were no bones deteected.");
		return false;
	}
	IB.Stride=2;
	if(numverts >= 65536) IB.Stride=4;

	Vertices.resize(numverts);
	Indices.resize((IB.Stride/2)*numindices);
	std::vector<Vertex_Types::Pos_Tex_Norm_Tang_Bone_Weight> tempverts(numverts);
	std::vector<std::string> bonenames;

	for (unsigned int i = 0; i < load->mNumMeshes;++i){
		Graphics::Texture diffuse, normal;
		const aiMesh* mesh = load->mMeshes[i];
		if(mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE) {
			OUTPUT_DEBUG_MSG("There are errors with this submesh, named: "<<mesh->mName.data<<"   Please, fix it");
			if(mesh->mPrimitiveTypes == aiPrimitiveType_LINE){
				OUTPUT_DEBUG_MSG("Problem: The mesh containes lines when it should only contain triangles");
			}else {
				OUTPUT_DEBUG_MSG("Problem: The mesh containes points when it should only contain triangles");
			}
			continue;
		}

		if (!mesh->HasTextureCoords(0))	{
			OUTPUT_DEBUG_MSG("There are errors with this submesh, named: "<<mesh->mName.data<<"   Please, fix it");
			OUTPUT_DEBUG_MSG("Problem: The mesh containes no texcoords, which means there will just be color displayed. This engine does not support color mesh displays, only textured mesh!");
			continue;
		} 
		if(!mesh->HasTangentsAndBitangents()) {
			OUTPUT_DEBUG_MSG("There are errors with this submesh, named: "<<mesh->mName.data<<"   Please, fix it");
			OUTPUT_DEBUG_MSG("Problem: Tangents were not created. No known fix");
			continue;
		}
		OUTPUT_DEBUG_MSG("Loading "<<mesh->mNumBones<<" bones . . .");
		for( unsigned int a = 0; a < mesh->mNumBones; a++)	{
			const aiBone* bone = mesh->mBones[a];
			size_t bonein(-1);
			for(size_t ib(0); ib< bonenames.size(); ib++){
				std::string tname = bone->mName.data;
				if(tname == bonenames[ib]){// found the bone.. break
					bonein=ib;
					break;
				}
			}
			if(bonein ==-1){//  did not find the bone, this is a new one push back
				bonein = bonenames.size();// get the index before insertion
				bonenames.push_back(bone->mName.data);
			}
			// there should only be 4 per vertex here because assimp guaranteees it, but if there are more, we are ok
			for( unsigned int b = 0; b < bone->mNumWeights; b++){
				if(			tempverts[bone->mWeights[b].mVertexId+ currentvertex].Weights.x <= 0.f) {
							tempverts[bone->mWeights[b].mVertexId+ currentvertex].Bones[0] = static_cast<float>(bonein);
							tempverts[bone->mWeights[b].mVertexId+ currentvertex].Weights.x = bone->mWeights[b].mWeight;
				} else if(	tempverts[bone->mWeights[b].mVertexId+ currentvertex].Weights.y <= 0.f){
							tempverts[bone->mWeights[b].mVertexId+ currentvertex].Bones[1] = static_cast<float>(bonein);
							tempverts[bone->mWeights[b].mVertexId+ currentvertex].Weights.y = bone->mWeights[b].mWeight;
				} else if(	tempverts[bone->mWeights[b].mVertexId+ currentvertex].Weights.z <= 0.f){
							tempverts[bone->mWeights[b].mVertexId+ currentvertex].Bones[2] = static_cast<float>(bonein);
							tempverts[bone->mWeights[b].mVertexId+ currentvertex].Weights.z = bone->mWeights[b].mWeight;
				} else if(	tempverts[bone->mWeights[b].mVertexId+ currentvertex].Weights.w <= 0.f){
							tempverts[bone->mWeights[b].mVertexId+ currentvertex].Bones[3] = static_cast<float>(bonein);
							tempverts[bone->mWeights[b].mVertexId+ currentvertex].Weights.w = bone->mWeights[b].mWeight;
				}
			}
			
		}
		for (unsigned int x = 0; x < mesh->mNumVertices;++x){
			Vertices[x + currentvertex] = tempverts[x + currentvertex].Pos = *reinterpret_cast<vec3*>(&mesh->mVertices[x]);
			tempverts[x + currentvertex].Tex =  *reinterpret_cast<vec2*>(&mesh->mTextureCoords[0][x]);
			tempverts[x + currentvertex].Norm = *reinterpret_cast<vec3*>(&mesh->mNormals[x]);
			tempverts[x + currentvertex].Tang = *reinterpret_cast<vec3*>(&mesh->mTangents[x]);
		}
		// check whether we can use 16 bit indices for our format... the ASSIMPOBLARBLA uses 32 bit indices for all theirs..
		if (IB.Stride == 4){
			uint32_t* pbData = reinterpret_cast<uint32_t*>(&Indices[currentindex]);
			for (unsigned int x = 0; x < mesh->mNumFaces;++x){
				for (unsigned int a = 0; a < 3 ;++a) {
					*pbData++ = static_cast<uint32_t>(mesh->mFaces[x].mIndices[a]+ currentvertex);
				}
			}
		} else {
			uint16_t* pbData = reinterpret_cast<uint16_t*>(&Indices[currentindex]);
			for (unsigned int x = 0; x < mesh->mNumFaces;++x){
				for (unsigned int a = 0; a < 3 ;++a) {
					*pbData++ = static_cast<uint16_t>(mesh->mFaces[x].mIndices[a]+ currentvertex);
				}
			}
		}
		//load the textures
		std::string pathtomodel(GetPath(file));
		Batch *batch = new Batch();

		LoadMaterials(mesh, load->mMaterials, batch, pathtomodel, Asset_Dir);
		batch->NumIndices=mesh->mNumFaces*3;
		batch->StartIndex = static_cast<uint32_t>(currentindex);
		batch->NumVerts= mesh->mNumVertices;
		// make sure to increment the ref count for thesse so they are properly destroyed

		currentvertex+=mesh->mNumVertices;
		currentindex+=mesh->mNumFaces*3;

		//For now, there will be a new shader for each material. I will create a shader cache where the graphics lib will cache the shaders and issue out already created shaders like it does with textures.
		Graphics::Shader_Macro macro1[] = {
			{"NORMALMAP", "1" }, 
			{"MATRIX_PALETTE_SIZE_DEFAULT", "60" }, 
			{NULL, NULL}
		};
		Graphics::Shader_Macro macro0[] = {
			{"NORMALMAP", "0" }, 
			{"MATRIX_PALETTE_SIZE_DEFAULT", "60" }, 
			{NULL, NULL}
		};

		Graphics::Shader_Macro* ptr = nullptr;
		if(batch->Has_NormalMap()) ptr = macro1;
		else ptr = macro0;

		batch->GetVS()->CompileShaderFromFile("Animated_Mesh.fx", "VS", "vs_4_0", ptr);

		FormatDesc lay[] = {
			FormatDesc(),
			FormatDesc(TYPE_TEXCOORD, FORMAT_FLOAT, 2),
			FormatDesc(TYPE_NORMAL, FORMAT_FLOAT, 3),
			FormatDesc(TYPE_TANGENT, FORMAT_FLOAT, 3),
			FormatDesc(TYPE_BONE, FORMAT_FLOAT, 4),
			FormatDesc(TYPE_WEIGHT, FORMAT_FLOAT, 4)
		};
		batch->GetVS()->CreateInputLayout(lay, sizeof(lay)/sizeof(FormatDesc));
		batch->GetPS()->CompileShaderFromFile("Animated_Mesh.fx", "PS", "ps_4_0", ptr);

		Batches.push_back(batch);
	}
	Animatior.Init(load);
	aiReleaseImport(load);// free the resrouces
	if(currentvertex==0) {// this could happen, if so GET OUTOF HERE
		OUTPUT_DEBUG_MSG("Problem loading the mesh, there were no vertices loaded. Failed to load the mesh");
		return false;
	}
	VB[0].Create(currentvertex, sizeof(Vertex_Types::Pos_Tex_Norm_Tang_Bone_Weight), VERTEX_BUFFER, IMMUTABLE, CPU_NONE, &tempverts[0] );
	IB.Create(currentindex, IB.Stride, INDEX_BUFFER, IMMUTABLE, CPU_NONE, &Indices[0]); // create index buffer!
	OUTPUT_DEBUG_MSG("Finished Loading the Mesh");
	Name=FileName =file;
	return true;
}
bool Animated_Mesh::Load_MyFormat(const std::string& file){
	return false;
}

bool Animated_Mesh::Save(const std::string& file){
		return false;
}

void Animated_Mesh::Draw(const Base_Camera* camera, float dt){
	totaldt += dt;
	Graphics::SetTopology(PRIM_TRIANGLELIST);

	Graphics::DepthStates::DepthTest.Bind();
	Graphics::RasterizerStates::CullBack.Bind();
	Graphics::BlendStates::No_Blend.Bind();
	Object_Transform cb;
	cb.Inv_Trans_World = cb.World = GetWorld();
	cb.View_Proj= camera->Trans_VP;
	cb.World.Transpose();
	cb.Inv_Trans_World.inverse();
	cb.Inv_Trans_World.Transpose();
	CBuffer0.Update(&cb);

	VB[0].BindAsVertexBuffer();
	IB.BindAsIndexBuffer();
	std::vector<mat4> transformed_bones = Animatior.GetTransforms(totaldt);
	for(int i=0; i<transformed_bones.size(); i++) transformed_bones[i].Transpose();
	CBuffer2.Update(&transformed_bones[0]);

	for(auto beg = Batches.begin(); beg != Batches.end(); beg++){
		Batch& batch = **beg;

		batch.GetVS()->Bind();
		batch.GetVS()->SetConstantBuffer(CBuffer0, 0);
		batch.GetVS()->SetConstantBuffer(CBuffer2, 1);

		batch.GetPS()->Bind();
		batch.GetPS()->SetSampler(Graphics::Samplers::Linear);
		CBuffer1.Update(batch.GetMaterial());
		batch.GetPS()->SetConstantBuffer(CBuffer1, 0);

		Graphics::Texture *t = batch.GetFirstMap();
		for(unsigned int slot=0; slot<batch.GetNum_ofMaps(); slot++) batch.GetPS()->SetResource(t + slot, slot);

		Graphics::DrawIndexed((*beg)->StartIndex, (*beg)->NumIndices);
	}
}