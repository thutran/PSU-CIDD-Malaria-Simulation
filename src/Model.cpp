/* 
 * File:   Model.cpp
 * Author: nguyentran
 * 
 * Created on March 22, 2013, 2:26 PM
 */
#include <fmt/format.h>
#include "Model.h"
#include "Population.h"
#include "Core/Config/Config.h"
#include "Person.h"
#include "Core/Random.h"
#include "MDC/ModelDataCollector.h"
#include "Events/BirthdayEvent.h"
#include "Events/ProgressToClinicalEvent.h"
#include "Events/EndClinicalDueToDrugResistanceEvent.h"
#include "Events/TestTreatmentFailureEvent.h"
#include "Events/UpdateWhenDrugIsPresentEvent.h"
#include "Events/EndClinicalByNoTreatmentEvent.h"
#include "Events/EndClinicalEvent.h"
#include "Drug.h"
#include "ImmuneSystem.h"
#include "SingleHostClonalParasitePopulations.h"
#include "Events/MatureGametocyteEvent.h"
#include "Events/MoveParasiteToBloodEvent.h"
#include "Events/UpdateEveryKDaysEvent.h"
#include "Reporters/Reporter.h"
#include "Events/CirculateToTargetLocationNextDayEvent.h"
#include "Events/ReturnToResidenceEvent.h"
#include "ClonalParasitePopulation.h"
#include "Events/SwitchImmuneComponentEvent.h"
#include "Events/ImportationPeriodicallyEvent.h"
#include "Events/ImportationEvent.h"
#include "easylogging++.h"
#include "Helpers/ObjectHelpers.h"
#include "Strategies/IStrategy.h"

Model* Model::MODEL = nullptr;
Config* Model::CONFIG = nullptr;
Random* Model::RANDOM = nullptr;
Scheduler* Model::SCHEDULER = nullptr;
ModelDataCollector* Model::DATA_COLLECTOR = nullptr;
Population* Model::POPULATION = nullptr;
// std::shared_ptr<spdlog::logger> LOGGER;


Model::Model(const int& object_pool_size) {
  initialize_object_pool(object_pool_size);
  random_ = new Random();
  config_ = new Config(this);
  scheduler_ = new Scheduler(this);
  population_ = new Population(this);
  data_collector_ = new ModelDataCollector(this);

  MODEL = this;
  CONFIG = config_;
  SCHEDULER = scheduler_;
  RANDOM = random_;
  DATA_COLLECTOR = data_collector_;
  POPULATION = population_;
  // LOGGER = spdlog::stdout_logger_mt("console");

  progress_to_clinical_update_function_ = new ClinicalUpdateFunction(this);
  immunity_clearance_update_function_ = new ImmunityClearanceUpdateFunction(this);
  having_drug_update_function_ = new ImmunityClearanceUpdateFunction(this);
  clinical_update_function_ = new ImmunityClearanceUpdateFunction(this);

  reporters_ = std::vector<Reporter *>();

  initial_seed_number_ = -1;
  config_filename_ = "config.yml";
  tme_filename_ = "tme.txt";
  override_parameter_filename_ = "";
  override_parameter_line_number_ = -1;
  gui_type_ = -1;
  is_farm_output_ = false;
}

Model::~Model() {
  release();

  release_object_pool();
}

void Model::initialize() {
  LOG(INFO) << "Model initilizing...";

  LOG(INFO) << "Initialize Random";
  //Initialize Random Seed
  random_->initialize(initial_seed_number_);

  LOG(INFO) << fmt::format("Read input file: {}", config_filename_);
  //Read input file
  config_->read_from_file(config_filename_);

  // modify parameters
  //modify parameters && update config
  LOG_IF(override_parameter_line_number_!= -1, INFO) << fmt::format("Override parameter from {} at line {}",
                                                                    override_parameter_filename_,
                                                                    override_parameter_line_number_);
  config_->override_parameters(override_parameter_filename_, override_parameter_line_number_);

  //add reporter here
  add_reporter(Reporter::MakeReport(Reporter::BFREPORTER));

  LOG(INFO) << "Initialing reports";
  //initialize reporters  
  for (Reporter* reporter : reporters_) {
    reporter->initialize();
  }


  LOG(INFO) << "Initialing scheduler";

  LOG(INFO) << "Starting day is " << CONFIG->starting_date();
  //initialize scheduler
  scheduler_->initialize(CONFIG->starting_date(), config_->total_time() + 2000);

  LOG(INFO) << "Initialing data collector";
  //initialize data_collector
  data_collector_->initialize();

  LOG(INFO) << "Initialing population";
  //initialize Population
  population_->initialize();

  LOG(INFO) << "Introducing initial cases";
  //initialize infected_cases
  population_->introduce_initial_cases();

  //initialize external population
  //    external_population_->initialize();


  LOG(INFO) << "Schedule for periodically importation event";
  //schedule for some special or periodic events
  for (auto& i : CONFIG->importation_parasite_periodically_info()) {
    ImportationPeriodicallyEvent::schedule_event(SCHEDULER,
                                                 i.location,
                                                 i.duration,
                                                 i.parasite_type_id,
                                                 i.number,
                                                 i.start_day);
  }

  LOG(INFO) << "Schedule for importation event at one time point";
  for (auto& i : CONFIG->importation_parasite_info()) {
    ImportationEvent::schedule_event(SCHEDULER, i.location,
                                     i.time,
                                     i.parasite_type_id,
                                     i.number);
  }
}

void Model::initialize_object_pool(const int& size) {
  BirthdayEvent::InitializeObjectPool(size);
  ProgressToClinicalEvent::InitializeObjectPool(size);
  EndClinicalDueToDrugResistanceEvent::InitializeObjectPool(size);
  UpdateWhenDrugIsPresentEvent::InitializeObjectPool(size);
  EndClinicalEvent::InitializeObjectPool(size);
  EndClinicalByNoTreatmentEvent::InitializeObjectPool(size);
  MatureGametocyteEvent::InitializeObjectPool(size);
  MoveParasiteToBloodEvent::InitializeObjectPool(size);
  UpdateEveryKDaysEvent::InitializeObjectPool(size);
  CirculateToTargetLocationNextDayEvent::InitializeObjectPool(size);
  ReturnToResidenceEvent::InitializeObjectPool(size);
  SwitchImmuneComponentEvent::InitializeObjectPool(size);
  ImportationPeriodicallyEvent::InitializeObjectPool(size);
  ImportationEvent::InitializeObjectPool(size);
  TestTreatmentFailureEvent::InitializeObjectPool(size);

  ClonalParasitePopulation::InitializeObjectPool(size);
  SingleHostClonalParasitePopulations::InitializeObjectPool();

  Drug::InitializeObjectPool(size);
  DrugsInBlood::InitializeObjectPool(size);

  //    InfantImmuneComponent::InitializeObjectPool(size);
  //    NonInfantImmuneComponent::InitializeObjectPool(size);

  ImmuneSystem::InitializeObjectPool(size);
  Person::InitializeObjectPool(size);
}

void Model::release_object_pool() {
  //    std::cout << "Release object pool" << std::endl;
  Person::ReleaseObjectPool();
  ImmuneSystem::ReleaseObjectPool();

  // TODO: Investigate why?
  //    InfantImmuneComponent::ReleaseObjectPool();
  //    NonInfantImmuneComponent::ReleaseObjectPool();

  DrugsInBlood::ReleaseObjectPool();
  Drug::ReleaseObjectPool();

  SingleHostClonalParasitePopulations::ReleaseObjectPool();
  ClonalParasitePopulation::ReleaseObjectPool();

  TestTreatmentFailureEvent::ReleaseObjectPool();
  ImportationEvent::ReleaseObjectPool();
  ImportationPeriodicallyEvent::ReleaseObjectPool();
  SwitchImmuneComponentEvent::ReleaseObjectPool();
  ReturnToResidenceEvent::ReleaseObjectPool();
  CirculateToTargetLocationNextDayEvent::ReleaseObjectPool();
  UpdateEveryKDaysEvent::ReleaseObjectPool();
  MoveParasiteToBloodEvent::ReleaseObjectPool();
  MatureGametocyteEvent::ReleaseObjectPool();
  EndClinicalByNoTreatmentEvent::ReleaseObjectPool();
  EndClinicalEvent::ReleaseObjectPool();
  UpdateWhenDrugIsPresentEvent::ReleaseObjectPool();
  EndClinicalDueToDrugResistanceEvent::ReleaseObjectPool();
  ProgressToClinicalEvent::ReleaseObjectPool();
  BirthdayEvent::ReleaseObjectPool();
}

void Model::run() {
  LOG(INFO) << "Model starting...";
  before_run();
  scheduler_->run();
  after_run();
  LOG(INFO) << "Model finished.";
}

void Model::before_run() {
  LOG(INFO) << "Perform before run events";
  for (auto* reporter : reporters_) {
    reporter->before_run();
  }
}

void Model::after_run() {
  LOG(INFO) << "Perform after run events";

  data_collector_->update_after_run();

  for (auto* reporter : reporters_) {
    reporter->after_run();
  }
}

void Model::begin_time_step() {
  //reset daily variables
  data_collector_->begin_time_step();

  // TODO: turn on and off time for art mutation in the input file
  //        turn on artemnisinin mutation at intervention day
  //    if (current_time_ == Model::CONFIG->start_intervention_day()) {
  //      Model::CONFIG->drug_db()->drug_db()[0]->set_p_mutation(0.005);
  //    }

  report_begin_of_time_step();
  perform_infection_event();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void Model::daily_update(const int& current_time) {
  population_->perform_birth_event();
  ///for safety remove all dead by calling perform_death_event
  population_->perform_death_event();
  population_->perform_circulation_event();

  //update / calculate daily UTL  
  data_collector_->end_of_time_step();

  //update force of infection
  population_->update_force_of_infection(current_time);

  //check to switch strategy
  config_->strategy()->update_end_of_time_step();
}

void Model::monthly_update() {
  monthly_report();

  data_collector()->monthly_update();
  //update treatment coverage
  config_->treatment_coverage_model()->monthly_update();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void Model::yearly_update() {
   data_collector_->perform_yearly_update();
}

void Model::release() {
  //    std::cout << "Model Release" << std::endl;
  ObjectHelpers::delete_pointer<ClinicalUpdateFunction>(progress_to_clinical_update_function_);
  ObjectHelpers::delete_pointer<ImmunityClearanceUpdateFunction>(immunity_clearance_update_function_);
  ObjectHelpers::delete_pointer<ImmunityClearanceUpdateFunction>(having_drug_update_function_);
  ObjectHelpers::delete_pointer<ImmunityClearanceUpdateFunction>(clinical_update_function_);

  ObjectHelpers::delete_pointer<Population>(population_);
  //   ObjectHelpers::DeletePointer<ExternalPopulation>(external_population_);
  ObjectHelpers::delete_pointer<Scheduler>(scheduler_);
  ObjectHelpers::delete_pointer<ModelDataCollector>(data_collector_);

  ObjectHelpers::delete_pointer<Config>(config_);
  ObjectHelpers::delete_pointer<Random>(random_);

  for (Reporter* reporter : reporters_) {
    ObjectHelpers::delete_pointer<Reporter>(reporter);
  }
  reporters_.clear();

  MODEL = nullptr;
  CONFIG = nullptr;
  SCHEDULER = nullptr;
  RANDOM = nullptr;
  DATA_COLLECTOR = nullptr;
  POPULATION = nullptr;
}

void Model::perform_infection_event() const {
  population_->perform_infection_event();
}

void Model::monthly_report() {
  data_collector_->perform_population_statistic();

  for (auto* reporter : reporters_) {
    reporter->monthly_report();
  }

}

void Model::report_begin_of_time_step() {
  for (auto* reporter : reporters_) {
    reporter->begin_time_step();
  }
}

void Model::add_reporter(Reporter* reporter) {
  reporters_.push_back(reporter);
  reporter->set_model(this);
}
