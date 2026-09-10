// Martin Duy Tat 1st May 2022

#include<map>
#include<cstdlib>
#include<stdexcept>
#include<string>
#include"ParticleMass.h"

namespace ParticleMass {
  
  double GetMass(int PID) {
    static const std::map<int, double> Mass{
      {211, 0.139568},
      {321, 0.493677},
      {2212, 0.93827208816}
    };
    // Antiparticles have negative PDG codes and the same mass
    auto iter = Mass.find(std::abs(PID));
    if(iter == Mass.end()) {
      throw std::invalid_argument("Unknown particle ID " + std::to_string(PID));
    }
    return iter->second;
  }

}
