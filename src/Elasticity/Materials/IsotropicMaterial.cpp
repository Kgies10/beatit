/*
 ============================================================================

 .______    _______     ___   .___________.    __  .___________.
 |   _  \  |   ____|   /   \  |           |   |  | |           |
 |  |_)  | |  |__     /  ^  \ `---|  |----`   |  | `---|  |----`
 |   _  <  |   __|   /  /_\  \    |  |        |  |     |  |     
 |  |_)  | |  |____ /  _____  \   |  |        |  |     |  |     
 |______/  |_______/__/     \__\  |__|        |__|     |__|     
 
 BeatIt - code for cardiovascular simulations
 Copyright (C) 2016 Simone Rossi

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ============================================================================
 */

/*
 * IsotropicMaterial.cpp
 *
 *  Created on: Oct 21, 2016
 *      Author: srossi
 */

#include "Elasticity/Materials/IsotropicMaterial.hpp"
#include "libmesh/getpot.h"
namespace BeatIt {

Material* createIsotropicMaterial()
{
	return new IsotropicMaterial;
}

IsotropicMaterial::IsotropicMaterial()
{
	/*
	 *  The parameters will be stored using the following order
	 *  0: density = rho
	 *  1: shear modulus: mu
	 *  2: bulk modulus: kappa
	 */
	M_parameters.resize(3);
}

IsotropicMaterial::~IsotropicMaterial() {
	// TODO Auto-generated destructor stub
}

void
IsotropicMaterial::setup(GetPot& data, std::string section)
{
	std::cout << "* ISOTROPIC MATERIAL: Setup. Reading parameters from: " << section << std::endl;
	M_parameters[0] = data(section+"/rho", 0.0); // rho
	double E = data(section+"/E", 0.0);
	double nu = data(section+"/nu", 0.0);
	M_parameters[1] = E / ( 2.0 * ( 1 + nu ) );// mu
	M_parameters[2] = E / ( 3.0 * ( 1 - 2 *  nu ) );// kappa
    std::cout << "\t density = " << M_parameters[0] << std::endl;
    std::cout << "\t shear modulus = " << M_parameters[1] << std::endl;
    std::cout << "\t bulk modulus = " << M_parameters[2] << std::endl;
}

void
IsotropicMaterial::evaluateStress()
{
	double mu = M_parameters[1];
	double kappa = M_parameters[2];
	double W1 = mu / 2.0;

	M_Fk = M_identity + M_gradU;
	M_Ck = M_Fk.transpose() * M_Fk;
	M_Cinvk = M_Ck.inverse();
	M_Jk = M_Fk.det();
	double Jm23 = std::pow(M_Jk, -2.0/3.0);
	double I1 = M_Ck(0,0) + M_Ck(1,1) + M_Ck(2,2);
	auto Sdev = 2.0 * W1 * Jm23 * ( M_identity - I1 / 3.0 * M_Cinvk);
	double p = kappa * ( M_Jk - 1);
	auto Svol = M_Jk * p * M_Cinvk;
	M_total_stress = M_Fk * ( Sdev + Svol );
}


void
IsotropicMaterial::evaluateJacobian(  const libMesh::TensorValue <double>&  dU, double q)
{
	double mu = M_parameters[1];
	double kappa = M_parameters[2];
	double W1 = mu / 2.0;

	double Jm23 = std::pow(M_Jk, -2.0/3.0);
	auto  dF = dU;
	auto  dC = M_Fk.transpose() * dF + dF.transpose() * M_Fk;
	double I1 = M_Ck(0,0) + M_Ck(1,1) + M_Ck(2,2);
	auto dJm23dC = -  Jm23 / 3.0 * M_Cinvk.contract(dC);
	auto Jac = 2.0 * W1 * dJm23dC * ( M_identity - I1 / 3.0 * M_Cinvk);
	Jac -= 2.0 * W1 * Jm23 * M_identity.contract(dC) / 3.0 * M_Cinvk;
	Jac += 2.0 * W1 * Jm23 * I1 / 3.0 * M_Cinvk * dC * M_Cinvk;

	double p = kappa * ( M_Jk - 1);
	double dJp = p + M_Jk * kappa;

	Jac += 0.5 * dJp * M_Jk * M_Cinvk.contract(dC) * M_Cinvk;
	Jac -= M_Jk * p  * M_Cinvk * dC * M_Cinvk;
	M_total_jacobian = dU * M_total_stress + M_Fk * Jac;
}







} /* namespace BeatIt */


