/*****************************************************************************

YASK: Yet Another Stencil Kernel
Copyright (c) 2014-2017, Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

* The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

*****************************************************************************/

// Test the YASK stencil kernel API for C++.

#include "yask_kernel_api.hpp"
#include <iostream>

using namespace std;
using namespace yask;

int main() {

    // The factory from which all other kernel objects are made.
    yk_factory kfac;

    // Initalize MPI, etc.
    auto env = kfac.new_env();

    // Create settings and solution.
    auto settings = kfac.new_settings();
    auto soln = kfac.new_solution(env, settings);

    // Init global settings.
    for (auto dim_name : soln->get_domain_dim_names()) {

        // Set min. domain size in each dim.
        settings->set_rank_domain_size(dim_name, 150);

        // Set block size to 64 in z dim and 32 in other dims.
        if (dim_name == "z")
            settings->set_block_size(dim_name, 64);
        else
            settings->set_block_size(dim_name, 32);
    }

    // Simple rank configuration in 1st dim only.
    auto ddim1 = soln->get_domain_dim_name(0);
    settings->set_num_ranks(ddim1, env->get_num_ranks());

    // Allocate memory for any grids that do not have storage set.
    // Set other data structures needed for stencil application.
    soln->prepare_solution();

    // Print some info about the solution and init the grids.
    auto name = soln->get_name();
    cout << "Stencil-solution '" << name << "':\n";
    cout << "  Step dimension: '" << soln->get_step_dim_name() << "'\n";
    cout << "  Domain dimensions:";
    for (auto dname : soln->get_domain_dim_names())
        cout << " '" << dname << "'";
    cout << endl;
    for (auto grid : soln->get_grids()) {
        cout << "    " << grid->get_name() << "(";
        for (auto dname : grid->get_dim_names())
            cout << " '" << dname << "'";
        cout << ")\n";

        // Subset of domain.
        vector<idx_t> first_indices, last_indices;
        for (auto dname : grid->get_dim_names()) {
            if (dname == soln->get_step_dim_name()) {

                // initial timestep.
                first_indices.push_back(0); 
                last_indices.push_back(0);
            } else {

                // small cube in center of overall problem.
                idx_t psize = soln->get_overall_domain_size(dname);
                idx_t first_idx = min(soln->get_last_rank_domain_index(dname),
                                      max(soln->get_first_rank_domain_index(dname),
                                          psize/2 - 10));
                idx_t last_idx = min(soln->get_last_rank_domain_index(dname),
                                      max(soln->get_first_rank_domain_index(dname),
                                          psize/2 + 10));
                first_indices.push_back(first_idx);
                last_indices.push_back(last_idx);
            }
        }
        
        // Init the values in a 'hat' function.
        grid->set_all_elements_same(0.0);
        idx_t nset = grid->set_elements_in_slice_same(1.0, first_indices, last_indices);
        cout << "      " << nset << " element(s) set to 1.0.\n";
    }

    // Apply the stencil solution to the data.
    env->global_barrier();
    cout << "Running the solution for 1 step...\n";
    soln->run_solution(0);
    cout << "Running the solution for 100 more steps...\n";
    soln->run_solution(1, 100);

    cout << "End of YASK kernel API test.\n";
    return 0;
}
