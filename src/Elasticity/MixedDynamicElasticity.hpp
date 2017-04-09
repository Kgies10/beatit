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
 * MixedDynamicElasticity.hpp
 *
 *  Created on: Feb 1, 2017
 *      Author: srossi
 */

#ifndef SRC_ELASTICITY_MixedDynamicElasticity_HPP_
#define SRC_ELASTICITY_MixedDynamicElasticity_HPP_

#include "Elasticity/Elasticity.hpp"
#include "Util/Enums.hpp"

namespace BeatIt {

class MixedDynamicElasticity : public virtual Elasticity {
public:
	typedef Elasticity super;
	MixedDynamicElasticity( libMesh::EquationSystems& es, std::string system_name );

    virtual void setupSystem(std::string section = "elasticity" );

	 void assemble_residual(double dt = 0.0, libMesh::NumericVector<libMesh::Number>* activation_ptr = nullptr);
    void update_displacements(double dt = 0.0);
    void advance();
    void project_pressure() {}
	virtual ~MixedDynamicElasticity();
    void solve_system();



	DynamicTimeIntegratorType M_timeIntegratorType;
    int M_saveStep;
};

} /* namespace BeatIt */

#endif /* SRC_ELASTICITY_MixedDynamicElasticity_HPP_ */
