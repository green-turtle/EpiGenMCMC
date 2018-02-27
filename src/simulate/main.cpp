//
//  main.cpp
//  EpiGenMCMC
//
//  Created by Lucy Li on 07/04/2016.
//  Copyright (c) 2016 Lucy Li, Imperial College London. All rights reserved.
//

#include <omp.h>
#include <iostream>
#include <sstream>
#include "../model.h"

// Argument order:
//   1. Parameters
//   2. Replicates
//   3. Total dt
//   4. Size of simulation time step
//   5. Sum every time steps
//   6. Number of groups
//   7. Seed
//   8. Number of threads
//   9. Filename of inital states
//   10. Filename of output
int main(int argc, const char * argv[]) {
    // Read in parameter values etc.
    Parameter model_params(argv[1]);
    std::vector <double> values = model_params.get_values_vector();
    std::vector <std::string> param_names = model_params.get_names_vector();
    std::cout << "Read in a total of " << model_params.get_total_params() << " parameters." << std::endl;
    int replicates = std::stoi(argv[2]);
    int total_dt = std::stoi(argv[3]);
    double dt_size = std::stod(argv[4]);
    int sum_every = std::stoi(argv[5]);
    int num_groups = std::stoi(argv[6]);
    int seed_num = std::stoi(argv[7]);
    int num_threads = std::stoi(argv[8]);
    omp_set_dynamic(0);
    omp_set_num_threads(num_threads);
    std::string traj_input = argv[9];
    std::string traj_output = argv[10];
    std::vector <std::string> traj_output_threads(num_threads, traj_output);
    for (int tn=0; tn<num_threads; tn++) {
        std::string newname = traj_output+"_threads";
        newname += std::to_string(tn);
        newname += ".txt";
        traj_output_threads[tn] = newname;
    }
    // Initialise trajectories
    Trajectory init_traj(total_dt, num_groups);
    init_traj.resize(total_dt, num_groups);
    init_traj.initialise_states(traj_input);
    init_traj.initialise_file(traj_output, sum_every);
    std::vector <Trajectory> init_traj_threads (num_threads, init_traj);
    Model sim_model;
    // A single simulation
    if (replicates==1) {
        gsl_rng* rr = gsl_rng_alloc( gsl_rng_mt19937 );
        gsl_rng_set( rr, seed_num);
        sim_model.simulate(values, param_names, &init_traj, 0, total_dt, dt_size, total_dt, rr);
        init_traj.print_to_file(traj_output, sum_every, true);
        gsl_rng_free(rr);
    } else {
        // Parallelised simulations
        std::vector <Trajectory *> trajectories;
        for (int i=0; i<num_threads; ++i) {
            trajectories.push_back(new Trajectory(init_traj));
        }
        std::vector <std::vector<double> > values_threads (num_threads, values);
        std::vector <std::vector<std::string> > param_names_threads (num_threads, param_names);
        std::vector <int> total_dt_threads (num_threads, total_dt);
        std::vector <double> dt_size_threads (num_threads, dt_size);
        std::vector <Model> sim_model_tn(num_threads);
        gsl_rng** rng = new gsl_rng*[num_threads];
        for (int thread = 0; thread < num_threads; thread++) {
            rng[thread] = gsl_rng_alloc(gsl_rng_mt19937);
            gsl_rng_set(rng[thread], seed_num+thread);
        }
#pragma omp parallel for schedule(static,1)
        for (int tn=0; tn<num_threads; tn++) {
            for (int i=tn; i<replicates; i+=num_threads) {
                sim_model_tn[tn].simulate(values_threads[tn], param_names_threads[tn], trajectories[tn], 0, total_dt_threads[tn], dt_size_threads[tn], total_dt_threads[tn], rng[tn]);
                trajectories[tn]->print_to_file(i, traj_output_threads[tn], sum_every, true);
            }
        }
        for (int tn=0; tn<num_threads; tn++) {
            gsl_rng_free(rng[tn]);
        }
    }
    
    // concatenate files generated by different threads
    std::fstream final_file;
    final_file.open(traj_output, std::ios_base::app);
    std::vector <std::ifstream> files;
    for (int tn=0; tn<num_threads; tn++) {
        files.push_back(std::ifstream(traj_output_threads[tn]));
    }
    std::string line;
    for (int i=0; i<replicates; i++) {
        int tn = i%num_threads;
        std::getline(files[tn], line);
        final_file << line << std::endl;;
    }
    final_file.close();
    for (int tn=0; tn<num_threads; tn++) {
        files[tn].close();
        const char * filename_c = traj_output_threads[tn].c_str();
        remove(filename_c);
    }
    std::cout << "Simulation complete" << std::endl;
    return 0;
}
