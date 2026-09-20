#include "FFormDSPCore.h"
#include <iostream>
#include <fstream>
#include <numeric>
#include <iomanip>
int main(){
 fform::dsp::EngineConfig cfg; cfg.numChannels=1; cfg.sampleRate=48000; cfg.formantPreservation=false; cfg.transientPhaseLock=false;
 fform::dsp::FFormDSPCore e(cfg);
 constexpr int n=48000*2;
 std::vector<std::vector<float>> in(1,std::vector<float>(n));
 for(int i=0;i<n;i++) in[0][i]=0.3f*std::sin(2*3.141592653589793*440*i/48000.0);
 for(auto [label,ratio,pitch]: {std::tuple{"neutral",1.,1.},std::tuple{"octave",1.,2.},std::tuple{"stretch150",1.5,1.},std::tuple{"compress050",.5,1.}}) {
  std::vector<std::vector<float>> out; e.processOfflineBuffer(in,out,pitch,ratio);
  auto &v=out[0]; double sum=0,peak=0;int zero=0; for(int i=10000;i<std::min<int>(int(v.size())-10000,90000);i++){sum+=v[i]*v[i]; peak=std::max<double>(peak,std::abs(v[i])); if(v[i]*v[i+1]<0)zero++;}
  int count=std::min<int>(int(v.size())-10000,90000)-10000;
  std::cout << label << " length=" << v.size()<<" rms="<<std::sqrt(sum/count)<<" peak="<<peak<<" zeros="<<zero<<" segment_s="<<(count/48000.0)<<"\n";
  std::ofstream f(std::string("/mnt/data/SHIFT2_audit/")+label+".raw",std::ios::binary);
  f.write(reinterpret_cast<char*>(v.data()),v.size()*sizeof(float));
 }
}
