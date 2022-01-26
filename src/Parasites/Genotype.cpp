/*
 * File:   Genotype.cpp
 * Author: Merlin
 *
 * Created on March 17, 2014, 2:33 PM
 */
#include "Genotype.h"

#include <algorithm>

#include "Core/Config/Config.h"
#include "Core/Random.h"
#include "Helpers/NumberHelpers.h"
#include "Model.h"
#include "Therapies/DrugDatabase.h"
#include "Therapies/SCTherapy.h"

Genotype::Genotype(const std::string &in_aa_sequence) : aa_sequence { in_aa_sequence } {
  // create aa structure
  std::string chromosome_str;
  std::istringstream tokenStream(in_aa_sequence);
  auto ii = 0;
  while (std::getline(tokenStream, chromosome_str, '|')) {
    std::string gene_str;
    std::istringstream chromosomeTokenStream(chromosome_str);
    auto jj = 0;
    while (std::getline(chromosomeTokenStream, gene_str, ',')) {
      pf_genotype_str[ii].push_back(gene_str);
      jj++;
    }
    ii++;
  }
}

Genotype::~Genotype() = default;

bool Genotype::resist_to(DrugType *dt) {
  // TODO: rework on this
  return false;
}

bool Genotype::resist_to(Therapy *therapy) {
  // TODO: rework on this
  return false;
}

Genotype *Genotype::combine_mutation_to(const int &locus, const int &value) {
  // TODO: rework on this
  return this;
}

double Genotype::get_EC50_power_n(DrugType *dt) const {
  return EC50_power_n[dt->id()];
}

std::ostream &operator<<(std::ostream &os, const Genotype &e) {
  os << e.genotype_id << "\t";
  os << e.get_aa_sequence();
  return os;
}

std::string Genotype::get_aa_sequence() const {
  return aa_sequence;
  //  std::stringstream ss;
  //
  //  for (auto &chromosome : aa_structure) {
  //    ss << chromosome;
  //    if (&chromosome != &aa_structure.back()){
  //      ss << "|";
  //    }
  //  }
  //
  //  return ss.str();
}
bool Genotype::is_valid(const PfGeneInfo &gene_info) {
  for (int chromosome_i = 0; chromosome_i < 14; ++chromosome_i) {
    auto chromosome_info = gene_info.chromosome_infos[chromosome_i];
    if (chromosome_info.gene_infos.size() != pf_genotype_str[chromosome_i].size()) return false;

    for (int gene_i = 0; gene_i < pf_genotype_str[chromosome_i].size(); ++gene_i) {
      auto gene_info = chromosome_info.gene_infos[gene_i];
      auto max_aa_pos = gene_info.max_copies > 1 ? pf_genotype_str[chromosome_i][gene_i].size() - 1
                                                 : pf_genotype_str[chromosome_i][gene_i].size();

      // check same size with aa postions info
      if (gene_info.aa_position_infos.size() != max_aa_pos) {
        std::cout << pf_genotype_str[chromosome_i][gene_i] << std::endl;
        std::cout << gene_info.aa_position_infos.size() << std::endl;
        return false;
      }

      for (int aa_i = 0; aa_i < max_aa_pos; ++aa_i) {
        auto aa_pos_info = gene_info.aa_position_infos[aa_i];
        auto element = pf_genotype_str[chromosome_i][gene_i][aa_i];

        if (std::find(aa_pos_info.amino_acids.begin(), aa_pos_info.amino_acids.end(), element)
            == aa_pos_info.amino_acids.end())
          return false;
      }

      // check number copy valid or not
      if (gene_info.max_copies > 1) {
        auto copy_number = NumberHelpers::char_to_single_digit_number(pf_genotype_str[chromosome_i][gene_i].back());
        if (copy_number > gene_info.max_copies) {
          return false;
        }
      }
    }
  }
  return true;
}
void Genotype::calculate_daily_fitness(const PfGeneInfo &gene_info) {
  daily_fitness_multiple_infection = 1.0;

  for (int chromosome_i = 0; chromosome_i < pf_genotype_str.size(); ++chromosome_i) {
    auto chromosome_info = gene_info.chromosome_infos[chromosome_i];

    for (int gene_i = 0; gene_i < pf_genotype_str[chromosome_i].size(); ++gene_i) {
      auto res_gene_info = chromosome_info.gene_infos[gene_i];
      auto max_aa_pos = res_gene_info.max_copies > 1 ? pf_genotype_str[chromosome_i][gene_i].size() - 1
                                                     : pf_genotype_str[chromosome_i][gene_i].size();
      for (int aa_i = 0; aa_i < max_aa_pos; ++aa_i) {
        // calculate cost of resistance
        auto aa_pos_info = res_gene_info.aa_position_infos[aa_i];
        auto element = pf_genotype_str[chromosome_i][gene_i][aa_i];

        auto it = std::find(aa_pos_info.amino_acids.begin(), aa_pos_info.amino_acids.end(), element);
        auto element_id = it - aa_pos_info.amino_acids.begin();

        auto cr = aa_pos_info.daily_crs[element_id];

        daily_fitness_multiple_infection *= (1 - cr);
      }

      // calculate for number copy variation
      if (res_gene_info.max_copies > 1) {
        auto copy_number = (int)pf_genotype_str[chromosome_i][gene_i].back() - 48;
        if (copy_number > 1) {
          daily_fitness_multiple_infection *= 1 - res_gene_info.cnv_daily_crs[copy_number - 1];
        }
      }
    }
  }
}

void Genotype::calculate_EC50_power_n(const PfGeneInfo &gene_info, DrugDatabase *drug_db) {
  EC50_power_n.resize(drug_db->size());
  for (const auto &[drug_id, dt] : *drug_db) {
    EC50_power_n[drug_id] = dt->base_EC50;
  }

  for (int chromosome_i = 0; chromosome_i < pf_genotype_str.size(); ++chromosome_i) {
    auto chromosome_info = gene_info.chromosome_infos[chromosome_i];

    for (int gene_i = 0; gene_i < pf_genotype_str[chromosome_i].size(); ++gene_i) {
      auto res_gene_info = chromosome_info.gene_infos[gene_i];
      auto max_aa_pos = res_gene_info.max_copies > 1 ? pf_genotype_str[chromosome_i][gene_i].size() - 1
                                                     : pf_genotype_str[chromosome_i][gene_i].size();
      std::vector<int> number_of_effective_mutations_in_same_genes(drug_db->size(), 0);

      for (int aa_i = 0; aa_i < max_aa_pos; ++aa_i) {
        // calculate cost of resistance
        auto aa_pos_info = res_gene_info.aa_position_infos[aa_i];
        auto element = pf_genotype_str[chromosome_i][gene_i][aa_i];
        auto it = std::find(aa_pos_info.amino_acids.begin(), aa_pos_info.amino_acids.end(), element);
        auto element_id = it - aa_pos_info.amino_acids.begin();

        for (const auto &[drug_id, dt] : *drug_db) {
          if (aa_pos_info.multiplicative_effect_on_EC50.find(drug_id)
              != aa_pos_info.multiplicative_effect_on_EC50.end()) {
            if (aa_pos_info.multiplicative_effect_on_EC50[drug_id][element_id] > 1) {
            }
            auto multiplicative_effect_factor = aa_pos_info.multiplicative_effect_on_EC50[drug_id][element_id];

            if (multiplicative_effect_factor > 1) {
              // encounter resistant aa
              number_of_effective_mutations_in_same_genes[drug_id] += 1;
              if (number_of_effective_mutations_in_same_genes[drug_id] > 1
                  && res_gene_info.multiplicative_effect_on_EC50_for_2_or_more_mutations.find(drug_id)
                         != res_gene_info.multiplicative_effect_on_EC50_for_2_or_more_mutations.end()) {
                // if multiplicative effect can apply to this drug
                multiplicative_effect_factor =
                    res_gene_info.multiplicative_effect_on_EC50_for_2_or_more_mutations[drug_id];
              }
            }
            EC50_power_n[drug_id] *= multiplicative_effect_factor;
          }
        }
      }

      // calculate for number copy variation
      if (res_gene_info.max_copies > 1) {
        auto copy_number = (int)pf_genotype_str[chromosome_i][gene_i].back() - 48;
        if (copy_number > 1) {
          for (const auto &[drug_id, dt] : *drug_db) {
            if (res_gene_info.cnv_multiplicative_effect_on_EC50.find(drug_id)
                != res_gene_info.cnv_multiplicative_effect_on_EC50.end()) {
              EC50_power_n[drug_id] *= res_gene_info.cnv_multiplicative_effect_on_EC50[drug_id][copy_number - 1];
            }
          }
        }
      }
    }
  }

  // power n
  for (const auto &[drug_id, dt] : *drug_db) {
    EC50_power_n[drug_id] = pow(EC50_power_n[drug_id], dt->n());
  }
}
Genotype *Genotype::perform_mutation_by_drug(Config *pConfig, Random *pRandom, DrugType *pDrugType) const {
  std::string new_aa_sequence { aa_sequence };

  // draw random aa position
  auto aa_pos_id = pRandom->random_uniform(pDrugType->resistant_aa_location.size());
  auto aa_pos = pDrugType->resistant_aa_location[aa_pos_id];

  if (aa_pos.is_copy_number) {
    // draw a random copy number
    auto new_copy_number =
        pRandom->random_uniform(
            pConfig->pf_gene_info().chromosome_infos[aa_pos.chromosome_id].gene_infos[aa_pos.gene_id].max_copies)
        + 1;
    new_aa_sequence[aa_pos.aa_index_in_aa_string] = NumberHelpers::single_digit_number_to_char(new_copy_number);
  } else {
    // draw random aa id
    auto new_aa_id = pRandom->random_uniform(pConfig->pf_gene_info()
                                                 .chromosome_infos[aa_pos.chromosome_id]
                                                 .gene_infos[aa_pos.gene_id]
                                                 .aa_position_infos[aa_pos.aa_id]
                                                 .amino_acids.size());
    auto new_aa = pConfig->pf_gene_info()
                      .chromosome_infos[aa_pos.chromosome_id]
                      .gene_infos[aa_pos.gene_id]
                      .aa_position_infos[aa_pos.aa_id]
                      .amino_acids[new_aa_id];
    new_aa_sequence[aa_pos.aa_index_in_aa_string] = new_aa;
  }
  
  // get genotype pointer from gene database based on aa sequence
  return pConfig->genotype_db.get_genotype(new_aa_sequence, pConfig);
}
