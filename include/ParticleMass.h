// Martin Duy Tat 1st May 2022
/**
 * ParticleMass is a namespace storing all the particle masses in a static map
 */

#ifndef PARTICLEMASS
#define PARTICLEMASS

namespace ParticleMass {
  /**
   * Get the particle mass from its PDG code (antiparticles have the same mass)
   * Throws std::invalid_argument if the code is not known, or if the particle
   * is in the table without a mass
   */
  double GetMass(int PID);
  /**
   * Get the particle charge, in units of e, from its PDG code
   * The sign of the charge is not the sign of the code for the leptons
   * Throws std::invalid_argument if the code is not known
   */
  int GetCharge(int PID);
}

#endif
