#pragma once
#include <algorithm>
#include <cmath>

namespace shn::gameplay {
struct CharacterMotor { float radius=.35f, height=1.8f, walkSpeed=4.5f, acceleration=18.f, gravity=24.f, verticalVelocity=0.f; bool grounded=true;
    void step(float dt, float inputX, float inputZ, float& x, float& y, float& z) {
        const float len=std::sqrt(inputX*inputX+inputZ*inputZ); if(len>1.f){inputX/=len;inputZ/=len;}
        const float targetX=inputX*walkSpeed, targetZ=inputZ*walkSpeed;
        x += targetX*std::min(1.f,acceleration*dt)*dt;
        z += targetZ*std::min(1.f,acceleration*dt)*dt;
        if(!grounded) verticalVelocity-=gravity*dt; else verticalVelocity=std::max(0.f,verticalVelocity);
        y += verticalVelocity*dt; if(y<=0.f){y=0.f;verticalVelocity=0.f;grounded=true;}
    }
    void jump(float impulse=8.f){ if(grounded){verticalVelocity=impulse;grounded=false;} }
};
}
