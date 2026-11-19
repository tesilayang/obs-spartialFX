#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace spatial {
// OBS enumerates scene items from background to foreground. Anchors are depths
// along the import camera axis, expressed in importPlane's depth convention.
struct ImportDepthAnchor { size_t rank; double depth; };
inline double importDepth(size_t rank,size_t total,const std::vector<ImportDepthAnchor>&anchors,double nearDepth) {
    const ImportDepthAnchor *back=nullptr,*front=nullptr;
    for(const auto&a:anchors){
        if(!std::isfinite(a.depth))continue;
        if(a.rank==rank)return a.depth;
        if(a.rank<rank&&(!back||a.rank>back->rank))back=&a;
        if(a.rank>rank&&(!front||a.rank<front->rank))front=&a;
    }
    if(back&&front&&back->depth>front->depth){
        const double t=double(rank-back->rank)/double(front->rank-back->rank);
        return back->depth+(front->depth-back->depth)*t;
    }
    // Keep authored anchors intact, even if their depths intentionally invert
    // the OBS stack. Use the nearest anchor when both constraints cannot agree.
    if(back&&(!front||rank-back->rank<=front->rank-rank))
        return back->depth-std::min(180.0,std::max(0.0,back->depth-nearDepth)*.5);
    if(front)return front->depth+180.0;
    return double(total>rank?total-rank-1:0)*180.0;
}
}
