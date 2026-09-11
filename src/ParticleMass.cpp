// Martin Duy Tat 1st May 2022

#include<map>
#include<optional>
#include<cstdlib>
#include<stdexcept>
#include<string>
#include"ParticleMass.h"

namespace ParticleMass {

  namespace {
    /**
     * What is known about the particle with the positive PDG code
     * The mass is empty when no mass is listed for that particle
     */
    struct Properties {
      std::optional<double> Mass;
      int Charge;
    };
    /**
     * The single list of particles, keyed by the positive PDG code
     * Antiparticles have the same mass and the opposite charge
     */
    const Properties& Find(int PID) {
      static const std::map<int, Properties> Table{
	{11,   {std::nullopt,  -1}},
	{13,   {std::nullopt,  -1}},
	{15,   {std::nullopt,  -1}},
	{211,  {0.139568,      +1}},
	{321,  {0.493677,      +1}},
	{2212, {0.93827208816, +1}}
      };
      auto iter = Table.find(std::abs(PID));
      if(iter == Table.end()) {
	throw std::invalid_argument("Unknown particle ID " + std::to_string(PID));
      }
      return iter->second;
    }
  }

  double GetMass(int PID) {
    const auto &properties = Find(PID);
    if(!properties.Mass) {
      throw std::invalid_argument("No mass is listed for particle ID "
				  + std::to_string(PID));
    }
    return *properties.Mass;
  }

  int GetCharge(int PID) {
    const int Charge = Find(PID).Charge;
    return PID > 0 ? Charge : -Charge;
  }

}
