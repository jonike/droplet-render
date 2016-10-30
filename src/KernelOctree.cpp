#include "main.h"
#include "scene.h"
#include "kernel.h"
#include "KernelOctree.h"

namespace KernelOctree{

//http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.29.987
static uint OctreeFirstNode(const dfloat3 &t0, const dfloat3 &tm){
	uint a = 0;
	if(t0.x > t0.y){
		if(t0.x > t0.z){
			if(tm.y < t0.x)
				a |= 2;
			if(tm.z < t0.x)
				a |= 1;
			return a;
		}
	}else{
		if(t0.y > t0.z){
			if(tm.x < t0.y)
				a |= 4;
			if(tm.z < t0.y)
				a |= 1;
			return a;
		}
	}

	if(tm.x < t0.z)
		a |= 4;
	if(tm.y < t0.z)
		a |= 2;
	return a;
}

static uint OctreeNextNode(const dfloat3 &tm, const duint3 &xyz){
	if(tm.x < tm.y){
		if(tm.x < tm.z)
			return xyz.x;
	}else{
		if(tm.y < tm.z)
			return xyz.y;
	}
	return xyz.z;
}

static bool OctreeProcessSubtreeRecursive(const dfloat3 &t0, const dfloat3 &t1, uint a, const tbb::concurrent_vector<OctreeStructure> *pob, uint n, uint l, float maxd, std::vector<ParallelLeafList::Node> *pls){
	//n: node index, l: octree level
	if(t1.x < 0.0f || t1.y < 0.0f || t1.z < 0.0f || (n == 0 && l > 0))
		return false;
	//if(level == mlevel){... return;} //leaf, volume index != ~0
	if((*pob)[n].volx[VOLUME_BUFFER_SDF] != ~0u || (*pob)[n].volx[VOLUME_BUFFER_FOG] != ~0u){
		float tr0 = std::max(std::max(t0.x,t0.y),t0.z);
		if(tr0 > maxd)
			return true;
		float tr1 = std::min(std::min(t1.x,t1.y),t1.z);
		pls->push_back(ParallelLeafList::Node(n,tr0,tr1));
		return false;
	}
	//dfloat3 tm = 0.5f*(t0+t1);
	dfloat3 tm = dfloat3(
		0.5f*(t0.x+t1.x),
		0.5f*(t0.y+t1.y),
		0.5f*(t0.z+t1.z));
	uint tn = OctreeFirstNode(t0,tm); //current node
	do{
		//octree node convention:
		//buildIndex(x) = {0, 4, 2, 6, 1, 5, 3, 7}[x] = buildIndex^-1(x)
		static const uint nla[] = {0,4,2,6,1,5,3,7};
		switch(tn){
		case 0:
			if(OctreeProcessSubtreeRecursive(t0,tm,a,pob,(*pob)[n].chn[nla[a]],l+1,maxd,pls))
				return true;
			tn = OctreeNextNode(tm,duint3(4,2,1));
			break;
		case 1:
			if(OctreeProcessSubtreeRecursive(dfloat3(t0.x,t0.y,tm.z),dfloat3(tm.x,tm.y,t1.z),a,pob,(*pob)[n].chn[nla[1^a]],l+1,maxd,pls))
				return true;
			tn = OctreeNextNode(dfloat3(tm.x,tm.y,t1.z),duint3(5,3,8));
			break;
		case 2:
			if(OctreeProcessSubtreeRecursive(dfloat3(t0.x,tm.y,t0.z),dfloat3(tm.x,t1.y,tm.z),a,pob,(*pob)[n].chn[nla[2^a]],l+1,maxd,pls))
				return true;
			tn = OctreeNextNode(dfloat3(tm.x,t1.y,tm.z),duint3(6,8,3));
			break;
		case 3:
			if(OctreeProcessSubtreeRecursive(dfloat3(t0.x,tm.y,tm.z),dfloat3(tm.x,t1.y,t1.z),a,pob,(*pob)[n].chn[nla[3^a]],l+1,maxd,pls))
				return true;
			tn = OctreeNextNode(dfloat3(tm.x,t1.y,t1.z),duint3(7,8,8));
			break;
		case 4:
			if(OctreeProcessSubtreeRecursive(dfloat3(tm.x,t0.y,t0.z),dfloat3(t1.x,tm.y,tm.z),a,pob,(*pob)[n].chn[nla[4^a]],l+1,maxd,pls))
				return true;
			tn = OctreeNextNode(dfloat3(t1.x,tm.y,tm.z),duint3(8,6,5));
			break;
		case 5:
			if(OctreeProcessSubtreeRecursive(dfloat3(tm.x,t0.y,tm.z),dfloat3(t1.x,tm.y,t1.z),a,pob,(*pob)[n].chn[nla[5^a]],l+1,maxd,pls))
				return true;
			tn = OctreeNextNode(dfloat3(t1.x,tm.y,t1.z),duint3(8,7,8));
			break;
		case 6:
			if(OctreeProcessSubtreeRecursive(dfloat3(tm.x,tm.y,t0.z),dfloat3(t1.x,t1.y,tm.z),a,pob,(*pob)[n].chn[nla[6^a]],l+1,maxd,pls))
				return true;
			tn = OctreeNextNode(dfloat3(t1.x,t1.y,tm.z),duint3(8,8,7));
			break;
		case 7:
			if(OctreeProcessSubtreeRecursive(dfloat3(tm.x,tm.y,tm.z),dfloat3(t1.x,t1.y,t1.z),a,pob,(*pob)[n].chn[nla[7^a]],l+1,maxd,pls))
				return true;
			tn = 8;
			break;
		}
	}while(tn < 8);

	return false;
}

static void OctreeProcessSubtreeIterative(const dfloat3 &_t0, const dfloat3 &_t1, uint a, const tbb::concurrent_vector<OctreeStructure> *pob, float maxd, std::vector<ParallelLeafList::Node> *pls){
#define MAX_DEPTH 16
	//n: node index, l: octree level
	dfloat3 t0[MAX_DEPTH], t1[MAX_DEPTH], tm[MAX_DEPTH];
	uint n[MAX_DEPTH], tn[MAX_DEPTH];

	bool p[MAX_DEPTH]; //stack pointer

	t0[0] = _t0;
	t1[0] = _t1;

	n[0] = 0;
	p[0] = false;

	for(uint l = 0;;){
		if(p[l]){
			static const uint nla[] = {0,4,2,6,1,5,3,7};
			switch(tn[l]){
#define SUBNODE(t0n,t1n,q,x)\
	t0[l+1] = t0n;\
	t1[l+1] = t1n;\
	tn[l] = OctreeNextNode(t1[l+1],q);\
	n[l+1] = (*pob)[n[l]].chn[nla[x^a]];\
	p[++l] = false;
			case 0:
				t0[l+1] = t0[l];
				t1[l+1] = tm[l];
				tn[l] = OctreeNextNode(tm[l],duint3(4,2,1));
				n[l+1] = (*pob)[n[l]].chn[nla[a]];
				p[++l] = false;
				break;
			case 1:
				SUBNODE(dfloat3(t0[l].x,t0[l].y,tm[l].z),
				dfloat3(tm[l].x,tm[l].y,t1[l].z),duint3(5,3,8),1);
				break;
			case 2:
				SUBNODE(dfloat3(t0[l].x,tm[l].y,t0[l].z),
				dfloat3(tm[l].x,t1[l].y,tm[l].z),duint3(6,8,3),2);
				break;
			case 3:
				SUBNODE(dfloat3(t0[l].x,tm[l].y,tm[l].z),
				dfloat3(tm[l].x,t1[l].y,t1[l].z),duint3(7,8,8),3);
				break;
			case 4:
				SUBNODE(dfloat3(tm[l].x,t0[l].y,t0[l].z),
				dfloat3(t1[l].x,tm[l].y,tm[l].z),duint3(8,6,5),4);
				break;
			case 5:
				SUBNODE(dfloat3(tm[l].x,t0[l].y,tm[l].z),
				dfloat3(t1[l].x,tm[l].y,t1[l].z),duint3(8,7,8),5);
				break;
			case 6:
				SUBNODE(dfloat3(tm[l].x,tm[l].y,t0[l].z),
				dfloat3(t1[l].x,t1[l].y,tm[l].z),duint3(8,8,7),6);
				break;
			case 7:
				t0[l+1] = dfloat3(tm[l].x,tm[l].y,tm[l].z);
				t1[l+1] = dfloat3(t1[l].x,t1[l].y,t1[l].z);
				tn[l] = 8;
				n[l+1] = (*pob)[n[l]].chn[nla[7^a]];
				p[++l] = false;
				break;
			default:
				if(l == 0)
					return;
				--l;
				break;
			}
		}else{
			if(t1[l].x < 0.0f || t1[l].y < 0.0f || t1[l].z < 0.0f || (n[l] == 0 && l > 0)){
				if(l == 0)
					return;
				--l;
				continue;
			}

			if((*pob)[n[l]].volx[VOLUME_BUFFER_SDF] != ~0u || (*pob)[n[l]].volx[VOLUME_BUFFER_FOG] != ~0u){
				float tr0 = std::max(std::max(t0[l].x,t0[l].y),t0[l].z);
				if(tr0 > maxd)
					return;
				float tr1 = std::min(std::min(t1[l].x,t1[l].y),t1[l].z);
				pls->push_back(ParallelLeafList::Node(n[l],tr0,tr1));
				--l;
				continue;
			}

			tm[l] = dfloat3(
				0.5f*(t0[l].x+t1[l].x),
				0.5f*(t0[l].y+t1[l].y),
				0.5f*(t0[l].z+t1[l].z));
			tn[l] = OctreeFirstNode(t0[l],tm[l]); //current node

			p[l] = true;
		}
	}
}

BaseOctreeTraverser::BaseOctreeTraverser(){
	//
}

BaseOctreeTraverser::~BaseOctreeTraverser(){
	//
}

OctreeFullTraverser::OctreeFullTraverser(){
	//
}

OctreeFullTraverser::~OctreeFullTraverser(){
	//
}

void OctreeFullTraverser::Initialize(const sfloat4 &ro, const sfloat4 &rd, const dintN &gm, const dfloatN &maxd, const tbb::concurrent_vector<OctreeStructure> *pob){
	for(uint i = 0; i < BLCLOUD_VSIZE; ++i){
		ls[i].clear();
		if(gm.v[i] == 0)
			continue;
		float4 ce = float4::load(&(*pob)[0].ce);
		float4 ro1 = ro.get(i)-ce+ce.splat<3>();
		float4 rd1 = rd.get(i);
		float4 scaabbmin = float4::zero();
		float4 scaabbmax = 2.0f*ce.splat<3>();

		dfloat3 ros = dfloat3(ro1);
		dfloat3 rds = dfloat3(rd1);

		uint a = 0;
		if(rds.x < 0.0f){
			ros.x = 2.0f*(*pob)[0].ce.w-ros.x;
			rds.x = -rds.x;
			a |= 4;
		}
		if(rds.y < 0.0f){
			ros.y = 2.0f*(*pob)[0].ce.w-ros.y;
			rds.y = -rds.y;
			a |= 2;
		}
		if(rds.z < 0.0f){
			ros.z = 2.0f*(*pob)[0].ce.w-ros.z;
			rds.z = -rds.z;
			a |= 1;
		}

		ro1 = float4::load(&ros);
		rd1 = float4::load(&rds);

		float4 t0 = (scaabbmin-ro1)/rd1;
		float4 t1 = (scaabbmax-ro1)/rd1;

		dfloat3 t0s = dfloat3(t0);
		dfloat3 t1s = dfloat3(t1);

		if(std::max(std::max(t0s.x,t0s.y),t0s.z) < std::min(std::min(t1s.x,t1s.y),t1s.z))
			OctreeProcessSubtreeRecursive(t0s,t1s,a,pob,0,0,maxd.v[i],&ls[i]);
	}
}

dintN OctreeFullTraverser::GetLeaf(uint i, duintN *pnodes, sfloat1 &tr0, sfloat1 &tr1){
	dintN mask;
	dfloatN TR0, TR1;
	for(uint j = 0; j < BLCLOUD_VSIZE; ++j){
		if(i >= ls[j].size()){
			mask.v[j] = 0;
			//pnodes->v[i] = ~0;
			continue;
		}
		pnodes->v[j] = std::get<0>(ls[j][i]);
		TR0.v[j] = std::get<1>(ls[j][i]);
		TR1.v[j] = std::get<2>(ls[j][i]);
		mask.v[j] = -1;
	}
	tr0 = sfloat1::load(&TR0);
	tr1 = sfloat1::load(&TR1);

	//return sint1::load(&mask);
	return mask;
}

OctreeStepTraverser::OctreeStepTraverser(){
	//
}

OctreeStepTraverser::~OctreeStepTraverser(){
	//
}

void OctreeStepTraverser::Initialize(const sfloat4 &ro, const sfloat4 &rd, const dintN &gm, const dfloatN &maxd, const tbb::concurrent_vector<OctreeStructure> *pob){
	//
}

dintN OctreeStepTraverser::GetLeaf(uint i, duintN *pnodes, sfloat1 &tr0, sfloat1 &tr1){
	//
}


}
