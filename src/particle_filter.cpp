/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "helper_functions.h"

using std::string;
using std::vector;

#define YAW_RATE_ZERO_THRESHOLD 0.0001 // under this threshold, yaw rate is considered zero

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * TODO: Set the number of particles. Initialize all particles to 
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   * TODO: Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */
  num_particles = 100;  // TODO: Set the number of particles
  
  if (is_initialized)
    return;
  
  std::default_random_engine gen;
  
  // Normal Distribution for x, y and theta
  std::normal_distribution<double> dist_x(x, std[0]);
  std::normal_distribution<double> dist_y(y, std[1]);
  std::normal_distribution<double> dist_theta(theta, std[2]);
  
  for (int i = 0; i < num_particles; i++) {
    Particle n;
    n.id = i;
    n.x = dist_x(gen);
    n.y = dist_y(gen);
    n.theta = dist_theta(gen);
    n.weight = 1.0;
    
    particles.push_back(n);
  }
  
  is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * TODO: Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */
  std::default_random_engine gen;
  
  // Normal Distribution for x, y and theta
  std::normal_distribution<double> dist_x(0, std_pos[0]);
  std::normal_distribution<double> dist_y(0, std_pos[1]);
  std::normal_distribution<double> dist_theta(0, std_pos[2]);
  
  for (Particle& p : particles) {
    if (fabs(yaw_rate) < YAW_RATE_ZERO_THRESHOLD) {
      // yaw_rate is (almost) zero
      p.x += velocity * delta_t * cos(p.theta);
      p.y += velocity * delta_t * sin(p.theta);
    } else {
      p.x += (velocity / yaw_rate) * (sin(p.theta + yaw_rate * delta_t) - sin(p.theta));
      p.y += (velocity / yaw_rate) * (cos(p.theta) - cos(p.theta + yaw_rate * delta_t));
      p.theta += yaw_rate * delta_t;
    }
    
    // Add random Gaussian Noise
    p.x += dist_x(gen);
    p.y += dist_y(gen);
    p.theta += dist_theta(gen);
  }
}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * TODO: Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */
  double min_dist;
  double temp_dist;
  int id;
  
  for(LandmarkObs& observation : observations) {
    // Initialize the minimum distance to max value
    min_dist = std::numeric_limits<double>::max();
    
    for(const LandmarkObs& landmark : predicted) {
      // Calculate distance
      temp_dist = dist(landmark.x, landmark.y, observation.x, observation.y);
      
      // Update if new distance is less than minimum distance
      if(temp_dist < min_dist) {
        min_dist = temp_dist;
        id = landmark.id;
      }
    }
    
    // Update the observation id
    observation.id = id;
  }

}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * TODO: Update the weights of each particle using a mult-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */
  for (Particle& p : particles) {
    
    // Collect valid landmarks
    // NB: both landmarks and particles are in map coordinates
    vector<LandmarkObs> valid_landmarks;
    for (const auto& map_landmark : map_landmarks.landmark_list){
      double distance = dist(p.x, p.y, map_landmark.x_f, map_landmark.y_f);
      if( distance < sensor_range) { 
        // if the landmark is within the sensor range, save it to predictions
        LandmarkObs tmp;
        tmp.x = map_landmark.x_f;
        tmp.y = map_landmark.y_f;
        tmp.id = map_landmark.id_i;
        valid_landmarks.push_back(tmp);
      }
    }
    
    // Convert observations coordinates from vehicle to map
    vector<LandmarkObs> observations_map;
    double cos_theta = cos(p.theta);
    double sin_theta = sin(p.theta);

    for (const LandmarkObs& observation : observations){
      LandmarkObs tmp;
      tmp.x = p.x + cos_theta * observation.x - sin_theta * observation.y;
      tmp.y = p.y + sin_theta * observation.x + cos_theta * observation.y;
      observations_map.push_back(tmp);
    }
    
    // Finds which observations correspond to which landmarks
    dataAssociation(valid_landmarks, observations_map);
    
    // Calculate particle's weight
    p.weight = 1.0; // reset weight
    for (const LandmarkObs& observation : observations_map) {
      auto landmark = map_landmarks.landmark_list.at(observation.id - 1);
      double n_x = (observation.x - landmark.x_f) * (observation.x - landmark.x_f) / (2 * std_landmark[0] * std_landmark[0]);
      double n_y = (observation.y - landmark.y_f) * (observation.y - landmark.y_f) / (2 * std_landmark[1] * std_landmark[1]);
      double c =  1.0 / (2 * M_PI * std_landmark[0] * std_landmark[1]);
      
      double w = c * exp(- (n_x + n_y));
      p.weight *= w;
    }
    
    weights.push_back(p.weight);
  }
}

void ParticleFilter::resample() {
  /**
   * TODO: Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */
  
  std::random_device rd;
  std::mt19937 gen(rd());
  std::discrete_distribution<> dist(weights.begin(), weights.end()); // this distribution is already proportional to the weights
  
  // Vector which contains the resampled particles
  vector<Particle> resampled_particles;
  resampled_particles.resize(num_particles);
  
  int particle_index;
  for (int i = 0; i < num_particles; i++) {
    particle_index = dist(gen);
    resampled_particles[i] = particles[particle_index];
  }
  
  particles = resampled_particles;
  
  // Clear weight vector
  weights.clear();

}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}