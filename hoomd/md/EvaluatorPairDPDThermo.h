// Copyright (c) 2009-2016 The Regents of the University of Michigan
// This file is part of the HOOMD-blue project, released under the BSD 3-Clause License.


// Maintainer: phillicl

#ifndef __PAIR_EVALUATOR_DPD_H__
#define __PAIR_EVALUATOR_DPD_H__

#ifndef NVCC
#include <string>
#endif

#include "hoomd/HOOMDMath.h"

#ifdef NVCC
#include "hoomd/extern/saruprngCUDA.h"
#else
#include "hoomd/extern/saruprng.h"
#endif


/*! \file EvaluatorPairDPDThermo.h
    \brief Defines the pair evaluator class for the DPD conservative potential
*/

// need to declare these class methods with __device__ qualifiers when building in nvcc
// DEVICE is __host__ __device__ when included in nvcc and blank when included into the host compiler
#ifdef NVCC
#define DEVICE __device__
#else
#define DEVICE
#endif

// call different Saru PRNG initializers on the host / device
// SARU is SaruGPU Class when included in nvcc and Saru Class when included into the host compiler
#ifdef NVCC
#define SARU(ix,iy,iz) SaruGPU saru( (ix) , (iy) , (iz) )
#else
#define SARU(ix, iy, iz) Saru saru( (ix) , (iy) , (iz) )
#endif

// use different Saru PRNG returns on the host / device
// CALL_SARU is currently define to return a random Scalar for both the GPU and Host.  By changing saru.f to saru.d, a double could be returned instead.
#ifdef NVCC
#define CALL_SARU(x,y) saru.f( (x), (y))
#else
#define CALL_SARU(x,y) saru.f( (x), (y))
#endif



//! Class for evaluating the DPD Thermostat pair potential
/*! <b>General Overview</b>

    See EvaluatorPairLJ

    <b>DPD Thermostat and Conservative specifics</b>

    EvaluatorPairDPDThermo::evalForceAndEnergy evaluates the function:
    \f[ V_{\mathrm{DPD-C}}(r) = A \cdot \left( r_{\mathrm{cut}} - r \right)
                        - \frac{1}{2} \cdot \frac{A}{r_{\mathrm{cut}}} \cdot \left(r_{\mathrm{cut}}^2 - r^2 \right)\f]

    The DPD Conservative potential does not need charge or diameter. One parameter is specified and stored in a Scalar.
    \a A is placed in \a param.

    EvaluatorPairDPDThermo::evalForceEnergyThermo evaluates the function:
    \f{eqnarray*}
    F =   F_{\mathrm{C}}(r) + F_{\mathrm{R,ij}}(r_{ij}) +  F_{\mathrm{D,ij}}(v_{ij}) \\
    \f}

    \f{eqnarray*}
    F_{\mathrm{C}}(r) = & A \cdot  w(r_{ij}) \\
    F_{\mathrm{R, ij}}(r_{ij}) = & - \theta_{ij}\sqrt{3} \sqrt{\frac{2k_b\gamma T}{\Delta t}}\cdot w(r_{ij})  \\
    F_{\mathrm{D, ij}}(r_{ij}) = & - \gamma w^2(r_{ij})\left( \hat r_{ij} \circ v_{ij} \right)  \\
    \f}

    \f{eqnarray*}
    w(r_{ij}) = &\left( 1 - r/r_{\mathrm{cut}} \right)  & r < r_{\mathrm{cut}} \\
                     = & 0 & r \ge r_{\mathrm{cut}} \\
    \f}
    where \f$\hat r_{ij} \f$ is a normalized vector from particle i to particle j, \f$ v_{ij} = v_i - v_j \f$, and \f$ \theta_{ij} \f$ is a uniformly distributed
    random number in the range [-1, 1].

    The DPD Thermostat potential does not need charge or diameter. Two parameters are specified and stored in a Scalar.
    \a A and \a gamma are placed in \a param.

    These are related to the standard lj parameters sigma and epsilon by:
    - \a A = \f$ A \f$
    - \a gamma = \f$ \gamma \f$

*/
class EvaluatorPairDPDThermo
    {
    public:
        //! Define the parameter type used by this pair potential evaluator
        typedef Scalar2 param_type;

        //! Constructs the pair potential evaluator
        /*! \param _rsq Squared distance beteen the particles
            \param _rcutsq Sqauared distance at which the potential goes to 0
            \param _params Per type pair parameters of this potential
        */
        DEVICE EvaluatorPairDPDThermo(Scalar _rsq, Scalar _rcutsq, const param_type& _params)
            : rsq(_rsq), rcutsq(_rcutsq), a(_params.x), gamma(_params.y)
            {
            }

        //! Set i and j, (particle tags), and the timestep
        DEVICE void set_seed_ij_timestep(unsigned int seed, unsigned int i, unsigned int j, unsigned int timestep)
            {
            m_seed = seed;
            m_i = i;
            m_j = j;
            m_timestep = timestep;
            }

        //! Set the timestep size
        DEVICE void setDeltaT(Scalar dt)
            {
            m_deltaT = dt;
            }

        //! Set the velocity term
        DEVICE void setRDotV(Scalar dot)
            {
            m_dot = dot;
            }

        //! Set the temperature
        DEVICE void setT(Scalar Temp)
            {
            m_T = Temp;
            }

        //! Does not use diameter
        DEVICE static bool needsDiameter() { return false; }
        //! Accept the optional diameter values
        /*! \param di Diameter of particle i
            \param dj Diameter of particle j
        */
        DEVICE void setDiameter(Scalar di, Scalar dj) { }

        //! Yukawa doesn't use charge
        DEVICE static bool needsCharge() { return false; }
        //! Accept the optional diameter values
        /*! \param qi Charge of particle i
            \param qj Charge of particle j
        */
        DEVICE void setCharge(Scalar qi, Scalar qj) { }

        //! Evaluate the force and energy using the conservative force only
        /*! \param force_divr Output parameter to write the computed force divided by r.
            \param pair_eng Output parameter to write the computed pair energy
            \param energy_shift If true, the potential must be shifted so that V(r) is continuous at the cutoff
            \note There is no need to check if rsq < rcutsq in this method. Cutoff tests are performed
                  in PotentialPair.

            \return True if they are evaluated or false if they are not because we are beyond the cuttoff
        */
        DEVICE bool evalForceAndEnergy(Scalar& force_divr, Scalar& pair_eng, bool energy_shift)
            {
            // compute the force divided by r in force_divr
            if (rsq < rcutsq)
                {

                Scalar rinv = fast::rsqrt(rsq);
                Scalar r = Scalar(1.0) / rinv;
                Scalar rcutinv = fast::rsqrt(rcutsq);
                Scalar rcut = Scalar(1.0) / rcutinv;

                // force is easy to calculate
                force_divr = a*(rinv - rcutinv);
                pair_eng = a * (rcut - r) - Scalar(1.0/2.0) * a * rcutinv * (rcutsq - rsq);

                return true;
                }
            else
                return false;
            }

        //! Evaluate the force and energy using the thermostat
        /*! \param force_divr Output parameter to write the computed force divided by r.
            \param force_divr_cons Output parameter to write the computed conservative force divided by r.
            \param pair_eng Output parameter to write the computed pair energy
            \param energy_shift Ignored. DPD always goes to 0 at the cutoff.
            \note There is no need to check if rsq < rcutsq in this method. Cutoff tests are performed
                  in PotentialPair.

            \note The conservative part \b only must be output to \a force_divr_cons so that the virial may be
                  computed correctly.

            \return True if they are evaluated or false if they are not because we are beyond the cuttoff
        */
        DEVICE bool evalForceEnergyThermo(Scalar& force_divr, Scalar& force_divr_cons, Scalar& pair_eng, bool energy_shift)
            {
            // compute the force divided by r in force_divr
            if (rsq < rcutsq)
                {
                Scalar rinv = fast::rsqrt(rsq);
                Scalar r = Scalar(1.0) / rinv;
                Scalar rcutinv = fast::rsqrt(rcutsq);
                Scalar rcut = Scalar(1.0) / rcutinv;

                // force calculation

                unsigned int m_oi, m_oj;
                // initialize the RNG
                if (m_i > m_j)
                   {
                   m_oi = m_j;
                   m_oj = m_i;
                   }
                else
                   {
                   m_oi = m_i;
                   m_oj = m_j;
                   }

                SARU(m_oi, m_oj, m_seed + m_timestep);


                // Generate a single random number
                Scalar alpha = CALL_SARU(-1,1) ;

                // conservative dpd
                //force_divr = FDIV(a,r)*(Scalar(1.0) - r*rcutinv);
                force_divr = a*(rinv - rcutinv);

                //  conservative force only
                force_divr_cons = force_divr;

                //  Drag Term
                force_divr -=  gamma*m_dot*(rinv - rcutinv)*(rinv - rcutinv);

                //  Random Force
                force_divr += fast::rsqrt(m_deltaT/(m_T*gamma*Scalar(6.0)))*(rinv - rcutinv)*alpha;

                //conservative energy only
                pair_eng = a * (rcut - r) - Scalar(1.0/2.0) * a * rcutinv * (rcutsq - rsq);


                return true;
                }
            else
                return false;
            }

        #ifndef NVCC
        //! Get the name of this potential
        /*! \returns The potential name. Must be short and all lowercase, as this is the name energies will be logged as
            via analyze.log.
        */
        static std::string getName()
            {
            return std::string("dpd");
            }
        #endif

    protected:
        Scalar rsq;     //!< Stored rsq from the constructor
        Scalar rcutsq;  //!< Stored rcutsq from the constructor
        Scalar a;       //!< a parameter for potential extracted from params by constructor
        Scalar gamma;   //!< gamma parameter for potential extracted from params by constructor
        unsigned int m_seed; //!< User set seed for thermostat PRNG
        unsigned int m_i;   //!< index of first particle (should it be tag?).  For use in PRNG
        unsigned int m_j;   //!< index of second particle (should it be tag?). For use in PRNG
        unsigned int m_timestep; //!< timestep for use in PRNG
        Scalar m_T;         //!< Temperature for Themostat
        Scalar m_dot;       //!< Velocity difference dotted with displacement vector
        Scalar m_deltaT;   //!<  timestep size stored from constructor
    };

#undef SARU

#endif // __PAIR_EVALUATOR_DPD_H__