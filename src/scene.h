#ifndef SCENE_H
#define SCENE_H

#define BLCLOUD_uN 8

enum SCENE_CACHE_MODE{
    SCENE_CACHE_DISABLED,
    SCENE_CACHE_READ,
    SCENE_CACHE_WRITE
};

enum VOLUME_BUFFER{
	VOLUME_BUFFER_SDF,
	VOLUME_BUFFER_FOG,
	VOLUME_BUFFER_COUNT
};

class BoundingBox{
public:
    BoundingBox();
    BoundingBox(const float4 &, const float4 &);
    ~BoundingBox();
    bool Intersects(const float4 &, const float4 &, const float4 &) const;
    bool Intersects(const BoundingBox &) const;
    dfloat3 sc;
    dfloat3 se;
};

class OctreeStructure{
public:
	OctreeStructure();
	~OctreeStructure();
    dfloat4 ce; //(center.xyz,extent)
	uint chn[8];
	uint volx[VOLUME_BUFFER_COUNT]; //leaf volume index
	//
	//min/max query value to speed up rendering; sdf: min distance, fog: max density
	//Min sdf may not be needed as leaves are created only when surface exists
	float qval[VOLUME_BUFFER_COUNT];
};

class Octree{
public:
	Octree(uint);
	Octree(); //should only used by concurrent_vector initial allocator
	~Octree();
	void BuildPath(const float4 &, const float4 &, const float4 &, const float4 &, uint, uint, std::atomic<uint> *, std::atomic<uint> *, tbb::concurrent_vector<Octree> *, tbb::concurrent_vector<OctreeStructure> *, VOLUME_BUFFER);
    void BuildPath(const float4 &, const float4 &, const float4 &, const float4 &, const float4 &, uint, uint, std::atomic<uint> *, std::atomic<uint> *, tbb::concurrent_vector<Octree> *, tbb::concurrent_vector<OctreeStructure> *, VOLUME_BUFFER);
	void FreeRecursive();
	Octree *pch[8];
	uint x; //node index
    //std::atomic_flag lock; //MT node write-access
	tbb::spin_mutex m;
};

struct LeafVolume{
    float pvol[BLCLOUD_uN*BLCLOUD_uN*BLCLOUD_uN]; //pdst
};

namespace Node{
class NodeTree;
}

namespace SceneData{

class BaseObject{
public:
    BaseObject(Node::NodeTree *);
    virtual ~BaseObject();
    class Node::NodeTree *pnt;
};

class ParticleSystem : public BaseObject{
public:
    ParticleSystem(Node::NodeTree *);
    ~ParticleSystem();
    static void DeleteAll();
    std::vector<dfloat3> pl; //position
	std::vector<dfloat3> vl; //velocity
    static std::vector<ParticleSystem *> prss;
};

class SmokeCache : public BaseObject{
public:
	SmokeCache(Node::NodeTree *);
	~SmokeCache();
	static void DeleteAll();
	static std::vector<SmokeCache *> objs;
};

class Surface : public BaseObject{
public:
    Surface(Node::NodeTree *);
    ~Surface();
    static void DeleteAll();
    std::vector<dfloat3> vl;
    std::vector<uint> tl;
    static std::vector<Surface *> objs;
};

}

#ifdef OPENVDB_OPENVDB_HAS_BEEN_INCLUDED
typedef openvdb::tools::GridSampler<openvdb::FloatGrid, openvdb::tools::BoxSampler> FloatGridBoxSampler;

namespace Node{

//using InputNodeParams = std::tuple<SceneData::BaseObject *, openvdb::math::Transform::Ptr, const FloatGridBoxSampler *[VOLUME_BUFFER_COUNT]>;
using InputNodeParams = std::tuple<SceneData::BaseObject *, openvdb::math::Transform::Ptr, const FloatGridBoxSampler *, const FloatGridBoxSampler *, const FloatGridBoxSampler *>;
enum INP{
	INP_OBJECT,
	INP_TRANSFORM,
	INP_SDFSAMPLER,
	INP_QGRSAMPLER,
	INP_FOGSAMPLER
};

class ValueNodeParams : public IValueNodeParams{
public:
	ValueNodeParams(const dfloat3 *, const dfloat3 *, float, float, const FloatGridBoxSampler *[VOLUME_BUFFER_COUNT], const FloatGridBoxSampler *);
	~ValueNodeParams();
	const dfloat3 * GetVoxPosW() const;
	const dfloat3 * GetCptPosW() const;
	float GetLocalDistance() const;
	float GetLocalDensity() const;
	float SampleGlobalDistance(const dfloat3 &, bool) const;
	float SampleGlobalDensity(const dfloat3 &) const;
	//global
	//const openvdb::FloatGrid::Ptr pgrid[VOLUME_BUFFER_COUNT];
	const FloatGridBoxSampler *psampler[VOLUME_BUFFER_COUNT];
	const FloatGridBoxSampler *pqsampler;
	//local
	const dfloat3 *pvoxw;
	const dfloat3 *pcptw;
	float distance;
	float density;
};

}
#endif


//scene builder
class Scene{
public:
	Scene();
	~Scene();
    void Initialize(float, float, SCENE_CACHE_MODE);
	//void AddObject();
	//void BuildScene();
	void Destroy();
    LeafVolume *pbuf[VOLUME_BUFFER_COUNT]; //-> psdfb, pfogb
    uint index;
    uint leafx[VOLUME_BUFFER_COUNT];
	tbb::concurrent_vector<Octree> root;
	tbb::concurrent_vector<OctreeStructure> ob;
    //uint octreesize;
    /*enum CACHE_MODE{
        //
    };*/
};

#endif
