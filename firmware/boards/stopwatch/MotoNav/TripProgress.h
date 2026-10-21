#pragma once
#include <stdint.h>

/** 只平滑行程环的显示比例，实际已走和剩余里程仍由原来的导航数据计算。 */
class TripProgress {
  float from_=0, target_=0;
  uint32_t at_=0, sampleAt_=0, duration_=0;
  bool valid_=false;
public:
  void reset() { valid_=false; from_=target_=0; duration_=0; }
  float value(uint32_t now) const {
    if(!valid_)return 0;
    uint32_t elapsed=now-at_;
    if(duration_==0 || elapsed>=duration_)return target_;
    return from_+(target_-from_)*(elapsed/(float)duration_);
  }
  void sample(float fraction,uint32_t now) {
    float target=fraction<0?0:(fraction>1?1:fraction);
    if(!valid_) { valid_=true; from_=target_=target; at_=sampleAt_=now; duration_=0; return; }
    if(target==target_)return;
    uint32_t interval=now-sampleAt_;
    from_=value(now);target_=target;at_=sampleAt_=now;
    duration_=interval<200?200:(interval>2000?2000:interval);
  }
  bool moving(uint32_t now) const {
    return valid_ && now-at_<duration_ && from_!=target_;
  }
};
