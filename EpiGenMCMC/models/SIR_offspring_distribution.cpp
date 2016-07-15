//
//  SIR_offspring_distribution.cpp
//  EpiGenMCMC
//
//  Created by Lucy Li on 13/04/2016.
//  Copyright (c) 2016 Lucy Li, Imperial College London. All rights reserved.
//

#include "../model.h"

void Model::simulate(std::vector<double> & model_params, std::vector<std::string> & param_names, Trajectory * traj, int start_dt, int end_dt, double step_size, int total_dt,  std::mt19937_64 rng) {
    if (traj->get_state(1) < 1.0) {
        return;
    }
    /* If the order of parameters is not known beforehand
    double rateI2R=1.0;
    double k=1.0;
    double Rt=-1;
    double Beta=-1;
    for (int i=0; i!=param_names.size(); ++i) {
        if (param_names[i] == "rateI2R") rateI2R = model_params[i];
        if (param_names[i] == "k") k = model_params[i];
        if (param_names[i] == "R0") {
            Rt = model_params[i] / traj->get_init_state(0) * traj->get_state(0);
        }
        if (param_names[i] == "beta") {
            Beta = model_params[i];
        }
    }
    if (Beta < 0) Beta = Rt * rateI2R / traj->get_state(0);
    if (Rt < 0) Rt = Beta / rateI2R * traj->get_state(0);
     */
    // /* For slightly faster implementation, call parameters by index
    double rateI2R=model_params[2];
    double k=model_params[1];
    double Rt=model_params[0];
    double Beta = Rt * rateI2R / traj->get_state(0);
    double S2I = 0.0;
    double I2R = 0.0;
    double total_infectious=0.0;
    double new_infections=0.0;
    double p = rateI2R*step_size;
    // */
    for (int t=start_dt; t<end_dt; ++t) {
        //
        // Transitions
        //
        // Recoveries: I --> R
        if (traj->get_state(1) > 0) {
            double infected = traj->get_state(1);
            if (p >= 1.0) I2R = infected;
            else if (use_deterministic) {
                I2R = infected*p;
            }
            else {
                std::binomial_distribution<int> distribution(infected, p);
                I2R = distribution(rng);
            }
        }
        // Infections: S --> I
        if (I2R > 0.0) {
            total_infectious += I2R;
            traj->set_state(traj->get_state(2)+I2R, 2); //Recovered
            traj->set_traj(I2R, t-start_dt); // People are sampled, i.e. appear in the time-series at the time of recovery
            double currS = traj->get_state(0);
            if (currS > 0) {
                if (use_deterministic) {
                    S2I = Beta*currS*traj->get_state(1)*step_size;
                }
                else {
                    // Current reproductive number
                    Rt = Beta / rateI2R * currS;
                    // Draw from the negative binomial distribution (gamma-poisson mixture) to determine
                    // number of secondary infections
                    double alpha = k*I2R;
                    double scale = Rt / k;
                    double gamma_number;
                    std::gamma_distribution<double> gam (alpha, scale);
                    gamma_number = gam(rng);
                    std::poisson_distribution<int> pois (gamma_number);
                    S2I = pois(rng);
                }
                if (S2I > 0) {
                    S2I = std::min(currS, S2I);
                    new_infections += S2I;
                    traj->set_state(currS-S2I, 0); // Susceptible
                }
            }
            traj->set_state(traj->get_state(1)+S2I-I2R, 1); //Infectious
        }
        double num_infected = traj->get_state(1);
        // Record 1/N for coalescent rate calculation
        if (num_infected > 0.0) {
            traj->set_traj2(1.0/num_infected, t-start_dt);
        }
        else {
            traj->set_traj2(0.0, t-start_dt);
            break;
        }
        S2I=0.0;
        I2R=0.0;
    }
    // Empirical reproductive number
    if (new_infections > 0.0) {
        Rt = new_infections/total_infectious;
    }
    // Calculate coalescent rate as 1/I(t) / Tg * Rt * (1 + 1/k)
    for (int t=start_dt; t<end_dt; ++t) {
        double coal_rate = traj->get_traj2(t-start_dt)*Rt*rateI2R*(1.0+1.0/k);
        traj->set_traj2(coal_rate, t-start_dt);
    }
}
