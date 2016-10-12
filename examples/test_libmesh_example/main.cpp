// The libMesh Finite Element Library.
// Copyright (C) 2002-2016 Benjamin S. Kirk, John W. Peterson, Roy H. Stogner

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA



// <h1> Systems Example 4 - Linear Elastic Cantilever </h1>
// \author David Knezevic
// \date 2012
//
// In this example we model a homogeneous isotropic cantilever
// using the equations of linear elasticity. We set the Poisson ratio to
// \nu = 0.3 and clamp the left boundary and apply a vertical load at the
// right boundary.


// C++ include files that we need
#include <iostream>
#include <algorithm>
#include <math.h>

// libMesh includes
#include "libmesh/libmesh.h"
#include "libmesh/mesh.h"
#include "libmesh/mesh_generation.h"
#include "libmesh/exodusII_io.h"
#include "libmesh/gnuplot_io.h"
#include "libmesh/linear_implicit_system.h"
#include "libmesh/equation_systems.h"
#include "libmesh/fe.h"
#include "libmesh/quadrature_gauss.h"
#include "libmesh/dof_map.h"
#include "libmesh/sparse_matrix.h"
#include "libmesh/numeric_vector.h"
#include "libmesh/dense_matrix.h"
#include "libmesh/dense_submatrix.h"
#include "libmesh/dense_vector.h"
#include "libmesh/dense_subvector.h"
#include "libmesh/perf_log.h"
#include "libmesh/elem.h"
#include "libmesh/boundary_info.h"
#include "libmesh/zero_function.h"
#include "libmesh/dirichlet_boundaries.h"
#include "libmesh/string_to_enum.h"
#include "libmesh/getpot.h"

// Bring in everything from the libMesh namespace
using namespace libMesh;

// Matrix and right-hand side assemble
void assemble_elasticity(EquationSystems & es,
                         const std::string & system_name);

// Define the elasticity tensor, which is a fourth-order tensor
// i.e. it has four indices i, j, k, l
Real eval_elasticity_tensor(unsigned int i,
                            unsigned int j,
                            unsigned int k,
                            unsigned int l);

// Begin the main program.
int main (int argc, char ** argv)
{
  // Initialize libMesh and any dependent libaries
  LibMeshInit init (argc, argv);

  // Initialize the cantilever mesh
  const unsigned int dim = 2;

  // Skip this 2D example if libMesh was compiled as 1D-only.
  libmesh_example_requires(dim <= LIBMESH_DIM, "2D support");

  // Create a 2D mesh distributed across the default MPI communicator.
  Mesh mesh(init.comm(), dim);
  auto eltype = TRI3;
  MeshTools::Generation::build_square (mesh,
                                       50, 10,
                                       0., 1.,
                                       0., 0.2,
                                       eltype);


  // Print information about the mesh to the screen.
  mesh.print_info();

  // Create an equation systems object.
  EquationSystems equation_systems (mesh);

  // Declare the system and its variables.
  // Create a system named "Elasticity"
  LinearImplicitSystem & system =
    equation_systems.add_system<LinearImplicitSystem> ("Elasticity");

  // Add two displacement variables, u and v, to the system
  auto order = FIRST;
  if(eltype == TRI6 || eltype == QUAD9) order = SECOND;
  unsigned int u_var = system.add_variable("u", order, LAGRANGE);
  unsigned int v_var = system.add_variable("v", order, LAGRANGE);

  system.attach_assemble_function (assemble_elasticity);

  // Construct a Dirichlet boundary condition object
  // We impose a "clamped" boundary condition on the
  // "left" boundary, i.e. bc_id = 3
  std::set<boundary_id_type> boundary_ids;
  boundary_ids.insert(3);

  // Create a vector storing the variable numbers which the BC applies to
  std::vector<unsigned int> variables(2);
  variables[0] = u_var; variables[1] = v_var;

  // Create a ZeroFunction to initialize dirichlet_bc
  ZeroFunction<> zf;

  DirichletBoundary dirichlet_bc(boundary_ids,
                                 variables,
                                 &zf);

  // We must add the Dirichlet boundary condition _before_
  // we call equation_systems.init()
  system.get_dof_map().add_dirichlet_boundary(dirichlet_bc);

  // Initialize the data structures for the equation system.
  equation_systems.init();

  // Print information about the system to the screen.
  equation_systems.print_info();

  // Solve the system
  system.solve();

  // Plot the solution
#ifdef LIBMESH_HAVE_EXODUS_API
  ExodusII_IO (mesh).write_equation_systems("displacement.e", equation_systems);
#endif // #ifdef LIBMESH_HAVE_EXODUS_API

  // All done.
  return 0;
}


void assemble_elasticity(EquationSystems & es,
                         const std::string & system_name)
{
    double E = 250.0;
    double nu = 0.49995;
    double mu = E / 2.0 / (1+nu);
    double kappa = E / 3.0 / (1-2*nu);
  libmesh_assert_equal_to (system_name, "Elasticity");

  const MeshBase & mesh = es.get_mesh();

  const unsigned int dim = mesh.mesh_dimension();

  LinearImplicitSystem & system = es.get_system<LinearImplicitSystem>("Elasticity");

  const unsigned int u_var = system.variable_number ("u");
  const unsigned int v_var = system.variable_number ("v");

  const DofMap & dof_map = system.get_dof_map();
  FEType fe_type = dof_map.variable_type(0);
  UniquePtr<FEBase> fe (FEBase::build(dim, fe_type));
  QGauss qrule (dim, fe_type.default_quadrature_order());
  fe->attach_quadrature_rule (&qrule);

  UniquePtr<FEBase> fe_face (FEBase::build(dim, fe_type));
  QGauss qface(dim-1, fe_type.default_quadrature_order());
  fe_face->attach_quadrature_rule (&qface);

  const std::vector<Real> & JxW = fe->get_JxW();
  const std::vector<std::vector<RealGradient> > & dphi = fe->get_dphi();

  DenseMatrix<Number> Ke;
  DenseVector<Number> Fe;
  DenseMatrix<Number> strain(2,2);
  DenseMatrix<Number> stress(2,2);
  DenseMatrix<Number> test(2,2);
  DenseMatrix<Number> I(2,2);
  I(0,0) = 1; I(1, 0) = 0;
  I(1,0) = 0; I(1, 1) = 1;
  DenseSubMatrix<Number>
    Kuu(Ke), Kuv(Ke),
    Kvu(Ke), Kvv(Ke);

  DenseSubVector<Number>
    Fu(Fe),
    Fv(Fe);

  std::vector<dof_id_type> dof_indices;
  std::vector<dof_id_type> dof_indices_u;
  std::vector<dof_id_type> dof_indices_v;

  MeshBase::const_element_iterator       el     = mesh.active_local_elements_begin();
  const MeshBase::const_element_iterator end_el = mesh.active_local_elements_end();

  for ( ; el != end_el; ++el)
    {
      const Elem * elem = *el;

      dof_map.dof_indices (elem, dof_indices);
      dof_map.dof_indices (elem, dof_indices_u, u_var);
      dof_map.dof_indices (elem, dof_indices_v, v_var);

      const unsigned int n_dofs   = dof_indices.size();
      const unsigned int n_u_dofs = dof_indices_u.size();
      const unsigned int n_v_dofs = dof_indices_v.size();

      fe->reinit (elem);

      Ke.resize (n_dofs, n_dofs);
      Fe.resize (n_dofs);

      Kuu.reposition (u_var*n_u_dofs, u_var*n_u_dofs, n_u_dofs, n_u_dofs);
      Kuv.reposition (u_var*n_u_dofs, v_var*n_u_dofs, n_u_dofs, n_v_dofs);

      Kvu.reposition (v_var*n_v_dofs, u_var*n_v_dofs, n_v_dofs, n_u_dofs);
      Kvv.reposition (v_var*n_v_dofs, v_var*n_v_dofs, n_v_dofs, n_v_dofs);

      Fu.reposition (u_var*n_u_dofs, n_u_dofs);
      Fv.reposition (v_var*n_u_dofs, n_v_dofs);

      for (unsigned int qp=0; qp<qrule.n_points(); qp++)
      {

        for (unsigned int i=0; i<n_u_dofs; i++)
        {
          for (unsigned int j=0; j<n_u_dofs; j++)
          {
              for (unsigned int idim=0; idim<2; idim++)
              {
                  strain *= 0.0;
                  strain(idim,0) = dphi[i][qp](0);
                  strain(idim,1) = dphi[i][qp](1);
                  stress *= 0.0;
                  double tre = strain(0,0) + strain(1,1);

                  stress = mu * ( strain - 2. /3.0 * tre * I ) + kappa * tre * I;
                  for (unsigned int jdim=0; jdim<2; jdim++)
                  {

                      test *= 0.0;
                      test(jdim,0) = dphi[j][qp](0);
                      test(jdim,1) = dphi[j][qp](1);
                      auto val = strain(0,0) * test(0,0) + strain(0,1) * test(0,1)
                                 strain(1,0) * test(1,0) + strain(1,1) * test(1,1);
                      Kuu(i,j) += JxW[qp] * dphi[i][qp](C_j)*dphi[j][qp](C_l))
                  }
              }
          }
        }
//          for (unsigned int i=0; i<n_u_dofs; i++)
//            for (unsigned int j=0; j<n_u_dofs; j++)
//              {
//                // Tensor indices
//                unsigned int C_i, C_j, C_k, C_l;
//                C_i=0, C_k=0;
//
//                C_j=0, C_l=0;
//                Kuu(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//
//                C_j=1, C_l=0;
//                Kuu(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//
//                C_j=0, C_l=1;
//                Kuu(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//
//                C_j=1, C_l=1;
//                Kuu(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//              }

//          for (unsigned int i=0; i<n_u_dofs; i++)
//            for (unsigned int j=0; j<n_v_dofs; j++)
//              {
//                // Tensor indices
//                unsigned int C_i, C_j, C_k, C_l;
//                C_i=0, C_k=1;
//
//                C_j=0, C_l=0;
//                Kuv(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//
//                C_j=1, C_l=0;
//                Kuv(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//
//                C_j=0, C_l=1;
//                Kuv(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//
//                C_j=1, C_l=1;
//                Kuv(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//              }

//          for (unsigned int i=0; i<n_v_dofs; i++)
//            for (unsigned int j=0; j<n_u_dofs; j++)
//              {
//                // Tensor indices
//                unsigned int C_i, C_j, C_k, C_l;
//                C_i=1, C_k=0;
//
//                C_j=0, C_l=0;
//                Kvu(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//
//                C_j=1, C_l=0;
//                Kvu(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//
//                C_j=0, C_l=1;
//                Kvu(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//
//                C_j=1, C_l=1;
//                Kvu(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//              }

//          for (unsigned int i=0; i<n_v_dofs; i++)
//            for (unsigned int j=0; j<n_v_dofs; j++)
//              {
//                // Tensor indices
//                unsigned int C_i, C_j, C_k, C_l;
//                C_i=1, C_k=1;
//
//                C_j=0, C_l=0;
//                Kvv(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//
//                C_j=1, C_l=0;
//                Kvv(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//
//                C_j=0, C_l=1;
//                Kvv(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//
//                C_j=1, C_l=1;
//                Kvv(i,j) += JxW[qp]*(eval_elasticity_tensor(C_i, C_j, C_k, C_l) * dphi[i][qp](C_j)*dphi[j][qp](C_l));
//              }
        }

      {
        for (unsigned int side=0; side<elem->n_sides(); side++)
          if (elem->neighbor(side) == libmesh_nullptr)
            {
              const std::vector<std::vector<Real> > & phi_face = fe_face->get_phi();
              const std::vector<Real> & JxW_face = fe_face->get_JxW();

              fe_face->reinit(elem, side);

              if (mesh.get_boundary_info().has_boundary_id (elem, side, 1)) // Apply a traction on the right side
                {
                  for (unsigned int qp=0; qp<qface.n_points(); qp++)
                    for (unsigned int i=0; i<n_v_dofs; i++)
                      Fv(i) += JxW_face[qp] * (-1.) * phi_face[i][qp];
                }
            }
      }

      dof_map.constrain_element_matrix_and_vector (Ke, Fe, dof_indices);

      system.matrix->add_matrix (Ke, dof_indices);
      system.rhs->add_vector    (Fe, dof_indices);
    }
}

Real eval_elasticity_tensor(unsigned int i,
                            unsigned int j,
                            unsigned int k,
                            unsigned int l)
{
  // Define the Poisson ratio
  const Real nu = 0.49995;
  // Define the Lame constants (lambda_1 and lambda_2) based on Poisson ratio
  const Real lambda_1 = nu / ((1. + nu) * (1. - 2.*nu));
  const Real lambda_2 = 0.5 / (1 + nu);

  // Define the Kronecker delta functions that we need here
  Real delta_ij = (i == j) ? 1. : 0.;
  Real delta_il = (i == l) ? 1. : 0.;
  Real delta_ik = (i == k) ? 1. : 0.;
  Real delta_jl = (j == l) ? 1. : 0.;
  Real delta_jk = (j == k) ? 1. : 0.;
  Real delta_kl = (k == l) ? 1. : 0.;

  return lambda_1 * delta_ij * delta_kl + lambda_2 * (delta_ik * delta_jl + delta_il * delta_jk);
}
