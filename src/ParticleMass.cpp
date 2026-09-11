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

  int GetCharge(int PID) {
    // Charge of the particle with the positive PDG code, in units of e
    static const std::map<int, int> Charge{
      {11, -1},
      {13, -1},
      {15, -1},
      {211, +1},
      {321, +1},
      {2212, +1}
    };
    auto iter = Charge.find(std::abs(PID));
    if(iter == Charge.end()) {
      throw std::invalid_argument("Unknown particle ID " + std::to_string(PID));
    }
    // Antiparticles have negative PDG codes and the opposite charge
    return PID > 0 ? iter->second : -iter->second;
  }

}
