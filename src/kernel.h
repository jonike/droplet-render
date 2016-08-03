#ifndef KERNEL_H
#define KERNEL_H

#define RENDER_TRANSPARENT 0x1

struct Light{
    dfloat3 direction;
    dfloat3 color;
    float angle;
    //float intensity;
};

#define BLCLOUD_MAX_RECURSION 32 //BLCLOUD_MAX_RECURSION
class ParallelLeafList{
public:
	ParallelLeafList(){}
	~ParallelLeafList(){}
    /*inline uint GetMaxSize(uint r) const{
		uint s = 0;
		for(uint i = 0; i < BLCLOUD_VSIZE; ++i){
			uint n = ls[r][i].size();
			if(n > s)
				s = n;
		}
		return s;
    }*/

	inline uint GetLeafCount(uint r, uint v) const{
		return ls[r][v].size();
	}

	inline uint GetLeaf(uint r, uint v, uint x) const{
		return ls[r][v][x];
	}

    std::vector<uint> ls[BLCLOUD_MAX_RECURSION][BLCLOUD_VSIZE];
};

class RenderKernel{
public:
	RenderKernel();
	~RenderKernel();
    bool Initialize(const Scene *, const dmatrix44 *, const dmatrix44 *, const std::vector<Light> *, uint, uint, uint, uint, uint, uint);
    void Render(uint, uint, uint);
	void Destroy();
	//
    //XMFLOAT4A *phb; //host buffer
    dfloat4 *phb; //host buffer
#ifndef BLCLOUD_CPU
	OctreeStructure *pob; //device octree
	class float4 *prt; //device buffer
#else
    const Scene *pscene;
	struct ArHosekSkyModelState *pskyms;
#endif
	Light *plights;
	uint lightc;
    dmatrix44 viewi;
    dmatrix44 proji;
    //uint samples;
    uint scattevs;
	uint rx;
	uint ry;
	uint w;
	uint h;
    uint flags;
};

#endif
