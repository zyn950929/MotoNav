#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
#include "RasterContinuity.h"
// These two headers are exact public 7.12.10-pages.4 source fixtures.
struct String:std::string {using std::string::string;using std::string::operator=;String()=default;String(int n):std::string(std::to_string(n)){};};
struct Bulk {uint8_t *buf=nullptr;uint32_t cap=0,total=0;int id=-1,parts=0,got=0;bool done=false;uint8_t mask[512]={};};
Bulk bulks[5];String pendingAck;bool navDirty=false;uint8_t *mapShow=nullptr;
int decodedCount=0;std::vector<uint8_t> decoded;RasterHeader decodedHeader;
int32_t rd32(const uint8_t*p){return rasterI32(p);}
void parseMap(const uint8_t*,uint32_t){}
void parseRoute(const uint8_t*,uint32_t){}
void parseGhost(const uint8_t*,uint32_t){}
bool parseRaster(const uint8_t*p,uint32_t len){
 RasterHeader h;if(!rasterCheck(p,len,h,1440000,720000))return false;
 decoded.resize(h.bytes);rasterDecode(p,h,decoded.data());decodedHeader=h;decodedCount++;return true;
}
#include "BulkReceiver.h"
void u16(std::vector<uint8_t>&b,int n){b.push_back(n&255);b.push_back(n>>8&255);}
void i32(std::vector<uint8_t>&b,int n){for(int k=0;k<4;k++)b.push_back((uint32_t)n>>(k*8));}
std::vector<std::vector<uint8_t>> packets(const std::vector<uint8_t>&data,int type,int id){
 const int payload=485,total=data.size(),parts=(total+payload-1)/payload;
 std::vector<std::vector<uint8_t>> result;
 for(int seq=0;seq<parts;seq++){
  int off=seq*payload,n=std::min(payload,total-off);
  std::vector<uint8_t>b={'M',(uint8_t)type,(uint8_t)id};u16(b,parts);u16(b,seq);i32(b,total);i32(b,off);
  b.insert(b.end(),data.begin()+off,data.begin()+off+n);assert(b.size()<=500 && b.size()<=512);result.push_back(b);
 }
 return result;
}
void send(const std::vector<uint8_t>&b){handleBulk(b.data(),b.size());}
int main(){
 std::vector<uint8_t> mem[5];for(int i=1;i<=4;i++){mem[i].resize(480000);bulks[i].buf=mem[i].data();bulks[i].cap=mem[i].size();}
 const int n=1200*1200;std::vector<uint8_t> source(n),data;
 for(int i=0;i<n;i++)source[i]=(i/13)%4;
 u16(data,1);i32(data,-1200);i32(data,1200);u16(data,1200);u16(data,1200);u16(data,2000);
 for(int i=0;i<n;){int count=std::min(13,n-i);data.push_back(((count-1)<<4)|source[i]);i+=count;}
 auto map=packets(data,3,1);auto route=packets(std::vector<uint8_t>(389,1),2,1);
 for(size_t i=0;i<map.size()/2;i++)send(map[i]);send(route[0]);assert(pendingAck=="K,2,1");pendingAck="";
 for(size_t i=map.size()/2;i<map.size();i++)send(map[i]);assert(pendingAck=="K,3,1" && decodedCount==1);
 for(int y=0;y<1200;y++)for(int x=0;x<1200;x++){uint8_t c=255;assert(rasterRead(decoded.data(),decodedHeader,x,y,c));assert(c==source[y*1200+x]);}
 // Actual receiver has one pendingAck string: a following route can overwrite K,3.
 send(route[0]);assert(pendingAck=="K,2,1");send(map.back());assert(pendingAck=="K,3,1" && decodedCount==1);
 // An incomplete map cannot be falsely confirmed by a tail probe.
 pendingAck="";auto second=packets(data,3,2);
 for(size_t i=0;i<second.size();i++)if(i!=17)send(second[i]);
 assert(pendingAck.empty());send(second.back());assert(pendingAck.empty() && decodedCount==1);
 send(second[17]);assert(pendingAck=="K,3,2" && decodedCount==2);
 pendingAck="";send(second.back());assert(pendingAck=="K,3,2" && decodedCount==2);
 printf("PASS published pages.4 receiver: 500-byte fragments, interleaved route, 1200x1200 exact decode, lost-K tail recovery, no false K for missing data (%zu parts)\n",map.size());
}
