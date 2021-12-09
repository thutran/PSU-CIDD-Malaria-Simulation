/* 
 * File:   DrugType.cpp
 * Author: nguyentran
 * 
 * Created on June 3, 2013, 10:59 AM
 */

#include "DrugType.h"
#include <cmath>
#include "Parasites/Genotype.h"
#include "Model.h"
#include "Core/Config/Config.h"
#include "Core/Random.h"
#include "easylogging++.h"


#ifndef LOG2_10
#define LOG2_10 3.32192809489
#endif

DrugType::DrugType() : id_(0), drug_half_life_(0), maximum_parasite_killing_rate_(0),
                       p_mutation_(0), k_(0), cut_off_percent_(0), n_(1) {}

DrugType::~DrugType() = default;

double DrugType::get_parasite_killing_rate_by_concentration(const double &concentration, const double &EC50_power_n) {
  const auto con_power_n = pow(concentration, n_);
  return maximum_parasite_killing_rate_ * con_power_n / (con_power_n + EC50_power_n);
}

double DrugType::n() {
  return n_;
}

void DrugType::set_n(const double &n) {
  n_ = n;
  //    set_EC50_power_n(pow(EC50_, n_));
}

int DrugType::get_total_duration_of_drug_activity(const int &dosing_days) const {
  //CutOffPercent is 10
  //log2(100.0 / 10.0) = 3.32192809489
  return dosing_days + ceil(drug_half_life_ * LOG2_10);
}

double DrugType::infer_ec50(Genotype* genotype) {
  // TODO: rework on this

  return 0.65;

  assert(false);
  el::Logger* defaultLogger = el::Loggers::getLogger("default");
  defaultLogger->fatal("EC50 not match for genotype: %s" , *genotype);
  //hopefully it will never reach here
  return 0;
}
