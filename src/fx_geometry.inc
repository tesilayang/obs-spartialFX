// Included in namespace spatial after model/catalog declarations and <cmath>.
// Run exactly once on an evaluated scene copy, after base animations and camera evaluation.
inline double fxCClamp(double value,double lower,double upper)
{
    return value<lower?lower:(value>upper?upper:value);
}
inline double fxCNoiseHash(double index,double seed)
{
    const double value=std::sin(index*127.1+seed*311.7)*43758.5453123;
    return value-std::floor(value);
}
inline double fxCNoise(double time,double seed)
{
    const double cell=std::floor(time),fraction=time-cell;
    const double smooth=fraction*fraction*(3.0-2.0*fraction);
    return (fxCNoiseHash(cell,seed)*(1.0-smooth)+fxCNoiseHash(cell+1.0,seed)*smooth)*2.0-1.0;
}
inline const FxInstance *fxCGeometryFx(const Layer &layer,const char *id)
{
    if(!layer.fx.attached||!layer.fx.groups[10])return nullptr;
    const auto *effect=findFx(layer.fx,id);
    return effect&&effect->enabled&&effect->strength>0?effect:nullptr;
}
inline void applyFxGeometry(Layer &layer,double seconds,const Camera &)
{
    if(!std::isfinite(seconds))return;
    if(const auto *effect=fxCGeometryFx(layer,"FX11.2")){
        const auto &v=effect->values;
        const double blend=fxCClamp(effect->strength,0.0,1.0);
        const double time=seconds*std::max(0.1,v[2]);
        layer.plane.position.x+=v[0]*blend*fxCNoise(time,v[4]+1.0);
        layer.plane.position.y+=v[1]*blend*fxCNoise(time,v[4]+17.0);
        layer.plane.rotation.z+=v[3]*blend*fxCNoise(time,v[4]+31.0);
    }
    if(const auto *effect=fxCGeometryFx(layer,"FX11.3")){
        const auto &v=effect->values;
        const double pulse=1.0+fxCClamp(effect->strength,0.0,1.0)*v[0]*std::sin(2.0*pi*(seconds*v[1]+v[2]));
        const int axes=int(v[3]+0.5);
        if(axes!=2)layer.plane.scaleX*=pulse;
        if(axes!=1)layer.plane.scaleY*=pulse;
    }
    if(const auto *effect=fxCGeometryFx(layer,"FX11.4")){
        const auto &v=effect->values;
        const double cycle=seconds*v[1]+v[4];
        const double phase=cycle-std::floor(cycle);
        const double duty=fxCClamp(v[3],0.01,0.99);
        double light=0.0;
        if(v[0]<0.5){
            const double wave=fxCClamp(0.5+0.5*std::sin(2.0*pi*phase),0.0,1.0);
            light=std::pow(wave,std::log(0.5)/std::log(duty));
        }else if(v[0]<1.5){
            light=phase<duty?1.0:0.0;
        }else{
            light=fxCNoiseHash(std::floor(cycle),v[5]+43.0)<duty?1.0:0.0;
        }
        const double target=v[2]+(1.0-v[2])*light;
        layer.opacity*=1.0-fxCClamp(effect->strength,0.0,1.0)*(1.0-target);
    }
}
inline void applyFxGeometry(Layer &layer,double seconds)
{
    applyFxGeometry(layer,seconds,Camera{});
}
