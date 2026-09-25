#include "runtime/RuntimeStack.hpp"
#include <iostream>
int main(){if(!shn::runtime::RunRuntimeSelfTest()){std::cerr<<"Runtime self-test FAILED\n";return 1;}std::cout<<"Runtime self-test PASSED: streaming, culling, animation, AVIF materials, navigation, physics, replication and persistence contracts loaded.\n";return 0;}
