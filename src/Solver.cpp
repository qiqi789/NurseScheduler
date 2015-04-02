/*
* Solver.cpp
*
*  Created on: 22 d��c. 2014
*      Author: jeremy
*/


#include "Solver.h"


//-----------------------------------------------------------------------------
//
//  C l a s s   S t a t N u r s e C t
//
// The instances of this class gather the status of the constraints that relate
// to the nurses.
//
//-----------------------------------------------------------------------------

// Constructor and destructor
//
StatCtNurse::StatCtNurse() {}
StatCtNurse::~StatCtNurse() {}

// initialize the statuses
//
void StatCtNurse::init(int nbDays) {
  nbDays_ = nbDays;

  // initialize all the cost vectors
  Tools::initVector(&costConsShifts_, nbDays_);
  Tools::initVector(&costConsDays_, nbDays_);
  Tools::initVector(&costConsDaysOff_, nbDays_);
  Tools::initVector(&costPref_, nbDays_);
  Tools::initVector(&costWeekEnd_, nbDays_);

  // initialize the violation vector
  for (int day = 0; day < nbDays_; day++) violSuccShifts_.push_back(false);
  for (int day = 0; day < nbDays_; day++) violSkill_.push_back(false);
}


//-----------------------------------------------------------------------------
//
//  C l a s s   L i v e N u r s e
//
// A live nurse is a nurse whose characteristics can evolve depending on
// the demand and on the planning that is being built
// They are needed in the solvers to duplicate the static nurses and define new
// attribute that can be modified.
//
//-----------------------------------------------------------------------------



// Constructor
//
LiveNurse::LiveNurse(const Nurse& nurse, Scenario* pScenario, int nbDays, int firstDay,
State* pStateIni,	map<int,set<int> >* pWishesOff):
Nurse(nurse.id_, nurse.name_, nurse.nbSkills_, nurse.skills_, nurse.pContract_),
pScenario_(pScenario), nbDays_(nbDays), firstDay_(firstDay),
pStateIni_(pStateIni), pWishesOff_(pWishesOff), pPosition_(0),
minWorkDays_(0), maxWorkDays_(0) {

  roster_.init(nbDays, firstDay);
  statCt_.init(nbDays);

  // initialize the states at each day
  states_.push_back(*pStateIni);
  for (int day = 0; day < nbDays_; day++) {
    State nextState;
    nextState.addDayToState(states_[day], 0);
    states_.push_back(nextState);
  }
}

LiveNurse::~LiveNurse() { }

// returns true if the nurse wishes the day-shift off
//
bool LiveNurse::wishesOff(int day, int shift) const {
  map<int,set<int> >::iterator itM = pWishesOff_->find(day);
  // If the day is not in the wish-list, no possible violation
  if(itM == pWishesOff_->end())  {
    return false;
  }
  // no preference either in the wish-list for that day
  else if(itM->second.find(shift) == itM->second.end()) {
    return false;
  }
  else {
    return true;
  }
}

// returns true if the nurses reached the maximum number of consecutive worked
// days or is resting and did not reach the minimum number of resting days yet
// if consecutive number of shifts will only be reached by violating maximum
// number of worked days, go to rest only if consecutive working days penalty
// is the the larger
//
bool LiveNurse::needRest(int day) {
  State state = states_[day];

  if (state.shift_>0 && state.consDaysWorked_ >= maxConsDaysWork()) {
    if (state.consShifts_ < pScenario_->minConsShifts_[state.shift_]) {
      return WEIGHT_CONS_SHIFTS > WEIGHT_CONS_DAYS_WORK ? false:true;
    }
    return true;
  }
  if (state.shift_==0 && state.consDaysOff_ <= minConsDaysOff()) {
    return true;
  }
  return false;
}

// returns true if the nurse needs to work one more day to reach the minimum
// number of consecutive working days or consecutive shifts
// if consecutive number of shifts will only be reached by violating maximum
// number of worked days, go to work only if consecutive shift penalty is
// the larger
bool LiveNurse::needWork(int day) {
  State state = states_[day];

  if (state.shift_>0) {
    if (state.consDaysWorked_ < minConsDaysWork()) {
      return true;
    }
    else if (state.consShifts_ < pScenario_->minConsShifts_[state.shift_]) {
      if (state.consDaysWorked_ >= maxConsDaysWork()) {
        return WEIGHT_CONS_SHIFTS > WEIGHT_CONS_DAYS_WORK ? true:false;
      }
      return true;
    }
  }

  return false;
}

// return true if the nurse is free to go to rest or work more without penalty
//==13776==    by 0x46A780: Solver::Solver(Scenario*, Demand*, Preferences*, std::vector<State, std::allocator<State> >*) (Solver.cpp:327)

bool LiveNurse::isFreeToChoose(int day) {
  return !needWork(day) && !needRest(day);
}

// check the satisfaction of the hard constraints and record the violations
// check the soft constraints and record the costs of the violations and the
// remaining margin for the satisfied ones.
//
void LiveNurse::checkConstraints(const Roster& roster,
  const vector<State>& states, StatCtNurse& stat) {

  // check the satisfaction of the hard constraints and record the violations
  //
  for (int day = 0; day < nbDays_; day++) {

    // Check that the nurse has the assigned skill
    //
    if (roster.shift(day)) {
      stat.violSkill_[day] = !hasSkill(roster.skill(day));
    }
    else {
      stat.violSkill_[day] = false;
    }

    // Check the forbidden successor constraint
    //
    int lastShift = states[day].shift_;   // last shift assigned to the nurse
    int thisShift = roster.shift(day);    // shift assigned on this day

    stat.violSuccShifts_[day] = pScenario_->isForbiddenSuccessor(thisShift,lastShift);
  }

  // check the soft constraints and record the costs of the violations and the
  // remaining margin for the satisfied ones.
  //
  for (int day = 1; day <= nbDays_; day++) {

    // shift assigned on the previous day
    int shift = states[day].shift_;
    int prevShift = states[day-1].shift_;

    // first look at consecutive working days or days off
    //
    int missingDays=0, extraDays=0;
    stat.costConsDays_[day-1] = 0;
    stat.costConsDaysOff_[day-1] = 0;

    // compute the violations of consecutive working days an
    if (shift) {
      if (prevShift == 0) {
        missingDays = minConsDaysOff()-states[day-1].consDaysOff_;
      }

      stat.costConsDaysOff_[day-1] += (missingDays>0) ? WEIGHT_CONS_DAYS_OFF*missingDays:0;
      stat.costConsDays_[day-1] += (states[day].consDaysWorked_>maxConsDaysWork()) ? WEIGHT_CONS_DAYS_WORK:0;
    }
    else {
      if (prevShift > 0) {
        missingDays =minConsDaysWork()-states[day-1].consDaysWorked_;
      }
      extraDays = states[day].consDaysOff_-maxConsDaysOff();

      stat.costConsDays_[day-1] += (missingDays>0) ? WEIGHT_CONS_DAYS_WORK*missingDays:0;
      stat.costConsDaysOff_[day-1] += (extraDays>0) ? WEIGHT_CONS_DAYS_OFF:0;
    }


    // check the consecutive same shifts
    //
    stat.costConsShifts_[day-1] = 0;
    int missingShifts = 0, extraShifts = 0;

    // count the penalty for minimum consecutive shifts only for the previous day
    // when the new shift is different
    if (shift != prevShift && prevShift > 0)  {
      missingShifts = pScenario_->minConsShifts_[prevShift]-states[day-1].consShifts_;
      stat.costConsShifts_[day-1] += (missingShifts>0) ? WEIGHT_CONS_SHIFTS*missingShifts:0;

      if (missingShifts>0)
        std::cout << "Day = " << day-states[day-1].consShifts_-1 << " ; nurse = " << id_ << " ; missing= " << missingShifts << std::endl;
    } 

    // count the penalty for maximum consecutive shifts when the shift is worked
    // the last day will then be counted
    if (shift > 0) {
      stat.costConsShifts_[day-1] += 
        (states[day].consShifts_>pScenario_->maxConsShifts_[shift]) ? WEIGHT_CONS_SHIFTS:0;

        if (states[day].consShifts_>pScenario_->maxConsShifts_[shift])
          std::cout << "Day = " << day-1 << " ; nurse = " << id_ << " ; One extra shift" << std::endl;
    }

      // it only makes sense if the nurse was working last day
    //   if (prevShift > 0) {
    //     missingShifts = pScenario_->minConsShifts_[prevShift]-states[day-1].consShifts_;
    //     extraShifts =  states[day-1].consShifts_-pScenario_->maxConsShifts_[prevShift];

    //     stat.costConsShifts_[day-1] += (extraShifts>0) ? WEIGHT_CONS_SHIFTS*extraShifts:0;

    //     if (extraShifts>0||missingShifts>0)
    //     std::cout << "Day = " << day-states[day-1].consShifts_-1 << " ; nurse = " << id_ << " ; extra= " << extraShifts << " ; missing= " << missingShifts << std::endl;
    //   }
    // }

    // if (shift > 0 && day == nbDays_) {
    //    int extraShifts =  states[day].consShifts_-pScenario_->maxConsShifts_[shift];
    //    stat.costConsShifts_[day-1] += (extraShifts>0) ? WEIGHT_CONS_SHIFTS*extraShifts:0;
    //    if (extraShifts>0)
    //       std::cout << "Day = " << day-states[day-1].consShifts_-1 << " ; nurse = " << id_ << " ; extra= " << extraShifts << std::endl;
    // }

    // check the preferences
    //
    map<int,set<int> >::iterator itM = pWishesOff_->find(day-1);
    // If the day is not in the wish-list, no possible violation
    if(itM == pWishesOff_->end())  {
      stat.costPref_[day-1] = 0;
    }
    // no preference either in the wish-list for that day
    else if(itM->second.find(shift) == itM->second.end()) {
      stat.costPref_[day-1] = 0;
    }
    else {
      stat.costPref_[day-1] = WEIGHT_PREFERENCES;
    }

    // check the complete week-end (only if the nurse requires them)
    // this cost is only assigned to the sundays
    //
    if ( Tools::isSunday(day-1) && needCompleteWeekends()) {
      if ( (shift > 0 && prevShift == 0) || ( shift == 0 && prevShift > 0 )) {
        stat.costWeekEnd_[day-1] = WEIGHT_COMPLETE_WEEKEND;
      }
    }

  } // end for day

  // get the costs due to total number of working days and week-ends
  //
  stat.costTotalDays_ = 0;
  stat.costTotalWeekEnds_ = 0;
  if (true) {//pScenario_->thisWeek() == pScenario_->nbWeeks_) {
    int missingDays=0, extraDays=0;
    missingDays = std::max(0, minTotalShifts() - states[nbDays_].totalDaysWorked_);
    extraDays = std::max(0, states[nbDays_].totalDaysWorked_-maxTotalShifts());
    stat.costTotalDays_ = WEIGHT_TOTAL_SHIFTS*(extraDays+missingDays);

    int extraWeekEnds = 0;
    extraWeekEnds = std::max(0, states[nbDays_].totalWeekendsWorked_-maxTotalWeekends());
    stat.costTotalWeekEnds_ = WEIGHT_TOTAL_WEEKENDS * extraWeekEnds;
  }
}

// Build States from the roster
//
void LiveNurse::buildStates(){
   for(int k=1; k<states_.size(); ++k)
      states_[k].addDayToState(states_[k-1], roster_.shift(k-1));
}


//-----------------------------------------------------------------------------
//
//  C l a s s   S o l v e r
//
//  Solves the offline problem
//  From a given problem (number of weeks, nurses, etc.), can compute a solution.
//
//-----------------------------------------------------------------------------

// Specific constructor
Solver::Solver(Scenario* pScenario, Demand* pDemand,
  Preferences* pPreferences, vector<State>* pInitState):
  pScenario_(pScenario),  pDemand_(pDemand),
  pPreferences_(pPreferences), pInitState_(pInitState),
  maxTotalStaffNoPenalty_(0), maxTotalStaff_(0), totalCostUnderStaffing_(0){

    // initialize the preprocessed data of the skills
    for (int sk = 0; sk < pScenario_->nbSkills_; sk++) {
      maxStaffPerSkill_.push_back(0);
      maxStaffPerSkillNoPenalty_.push_back(0);
      skillRarity_.push_back(1.0);
    }

    // copy the nurses in the live nurses vector
    for (int i = 0; i < pScenario_->nbNurses_; i++) {
      theLiveNurses_.push_back(
        new LiveNurse( (pScenario_->theNurses_[i]), pScenario_, pDemand_->nbDays_,
        pDemand_->firstDay_, &(*pInitState_)[i], &(pPreferences_->wishesOff_[i])  ) );
    }
  }

// Destructor
Solver::~Solver(){
   for(LiveNurse* pNurse: theLiveNurses_)
      delete pNurse;
}


//------------------------------------------------
// Preprocess the data
//------------------------------------------------

// go through the nurses to collect data regarding the potential shift and
// skill coverage of the nurses
//
void Solver::preprocessTheNurses() {
  // local variables for conciseness of the code
  //
  int nbNurses = pScenario_->nbNurses_, nbSkills = pScenario_->nbSkills_,
    nbDays = pDemand_->nbDays_;

  maxTotalStaff_ = 0;
  for (int n = 0; n < nbNurses; n++)	{
    LiveNurse* pNurse = theLiveNurses_[n];
    pNurse->maxWorkDays_ = 0;
    pNurse->minWorkDays_ = 0;

    // compute the maximum and minimum number of working days in the period of
    // the demand without getting any penalty for the number of consecutive
    // shifts
    // RqJO: this neglects the constraint of complete week-ends and the
    // preferences ; they should be added later
    //

    // first treat the history
    //
    // remaining number of days when building a maximum and minimum working days
    // periods
    int nbDaysMax=nbDays, nbDaysMin = nbDays;
    State* pState = &(pInitState_->at(n));
    if (pState->shift_ != 0) {
      // if the last shift was not a rest
      // keep working until maximum consecutive number of shifts for maximum
      // working days or until minimum consecutive number of shifts for minimum
      // working days
      pNurse->maxWorkDays_ = std::max(0, pNurse->maxConsDaysWork()-pState->consDaysWorked_);
      pNurse->minWorkDays_ = std::max(0, pNurse->minConsDaysWork()-pState->consDaysWorked_);

      // perform minimum rest for maximum working days and maximum rest for
      // minimum working days
      nbDaysMax = nbDays - pNurse->maxWorkDays_ - pNurse->minConsDaysOff();
      nbDaysMin = nbDays - pNurse->minWorkDays_ - pNurse->maxConsDaysOff();
    }
    else {
      // if the last shift was a rest
      // keep resting until minimum consecutive number of rests for maximum
      // working days or until maximum consecutive number of rests for minimum
      // working days
      nbDaysMax = nbDays - std::max(0, pNurse->minConsDaysOff()-pState->consDaysOff_);
      nbDaysMin = nbDays - std::max(0, pNurse->maxConsDaysWork()-pState->consDaysOff_);
    }

    // lengths of the stints maximizing and minimizing the percentage of working
    // days respectively
    //
    int lengthWorkStint = pNurse->maxConsDaysWork() + pNurse->minConsDaysOff();
    int lengthRestStint = pNurse->minConsDaysWork() + pNurse->maxConsDaysOff();

    // perform as many of these stints as possible
    //
    pNurse->maxWorkDays_ += (nbDaysMax/lengthWorkStint)*pNurse->maxConsDaysWork()+
      + std::min(nbDaysMax%lengthWorkStint, pNurse->maxConsDaysWork());
    pNurse->minWorkDays_ = (nbDaysMin/lengthRestStint)*pNurse->minConsDaysWork()+
      + std::min(nbDaysMin%lengthWorkStint, pNurse->minConsDaysWork());

    // add the working maximum number of working days to the maximum staffing
    //
    maxTotalStaffNoPenalty_ += pNurse->maxWorkDays_;
    maxTotalStaff_ += nbDays;
    for (int i = 0; i < pNurse->nbSkills_; i++) {
      // RqJO: the staffing per skill is very rough here since Nurses can have
      // multiple skills. A better data structure should be found.
      int sk = pNurse->skills_[i];
      maxStaffPerSkill_[sk] += nbDays;
      maxStaffPerSkillNoPenalty_[sk] += pNurse->maxWorkDays_;
    }

    // assign its position to the nurse
    // go through every existing position to see if the position of this nurse
    // has already been created
    bool isPosition = true;
    for (int i = 0; i < pScenario_->nbPositions() ; i++)	{
      Position* pPosition = pScenario_->pPositions()[i];
      isPosition = true;
      if (pPosition->nbSkills_ == pNurse->nbSkills_) {
        for (int i = 0; i < pNurse->nbSkills_; i++) {
          if (pNurse->skills_[i] != pPosition->skills_[i])	{
            isPosition = false;
            break;
          }
        }
      }
      else isPosition = false;

      if (isPosition) {
        pNurse->pPosition_ = pPosition;
        break;
      }
    }
    if (!isPosition) {
      Tools::throwError("The nurse has no position!");
    }
  }

  for (int sk = 0; sk < nbSkills; sk++) {
    std::cout << "Skill " << sk << " : " << maxStaffPerSkillNoPenalty_[sk] << std::endl;
  }

  // initialize to zero the satisfied demand
  //
  Tools::initVector3D(&satisfiedDemand_, pDemand_->nbDays_,
    pScenario_->nbShifts_, pScenario_->nbSkills_);
}


// check the feasibility of the demand with these nurses
//
bool checkFeasibility() {
  return true;
}

// get the total cost of the current solution
// the solution is simply given by the roster of each nurse
double Solver::solutionCost() {
  double totalCost = 0.0;
  int nbNurses = pScenario_->nbNurses_, nbDays = pDemand_->nbDays_;
  int nbShifts = pScenario_->nbShifts_, nbSkills = pScenario_->nbSkills_;

  // reset the satisfied demand to compute it from scratch
  for(int day = 0; day < nbDays; day++) {
    for (int sh = 1; sh < nbShifts ; sh++) {
      for (int sk = 0; sk < nbSkills; sk++) {
        satisfiedDemand_[day][sh][sk] = 0;
      }
    }
  }

  // first add the individual cost of each nurse
  for (int n = 0; n < nbNurses; n++) {
    LiveNurse *pNurse = theLiveNurses_[n];
    pNurse->checkConstraints(pNurse->roster_, pNurse->states_, pNurse->statCt_);
    StatCtNurse stat = pNurse->statCt_;

    for (int day = 0; day < nbDays ; day++) {
      totalCost += stat.costConsDays_[day]+stat.costConsDaysOff_[day]+ 
        stat.costConsShifts_[day]+stat.costPref_[day]+stat.costWeekEnd_[day];

      if (pNurse->roster_.shift(day) > 0) {
        satisfiedDemand_[day][pNurse->roster_.shift(day)][pNurse->roster_.skill(day)]++;
      }
    }

    totalCost += stat.costTotalDays_+stat.costTotalWeekEnds_;
  }
  std::cout << "Total cost due to individual soft constraints = " << totalCost << std::endl;

  // add the cost of non-optimal demand
  for(int day = 0; day < nbDays; day++) {
    for (int sh = 1; sh < nbShifts ; sh++) {
      for (int sk = 0; sk < nbSkills; sk++) {
        int missingStaff;
        missingStaff = std::max(0, pDemand_->optDemand_[day][sh][sk] - satisfiedDemand_[day][sh][sk]);
        totalCost += WEIGHT_OPTIMAL_DEMAND*missingStaff;
      }
    }
  }

  return totalCost;
}

//------------------------------------------------
// Display functions
//------------------------------------------------

// display the whole solution
//
string Solver::solutionToString() {
  std::stringstream rep;
  int nbNurses = pScenario_->nbNurses_;
  int firstDay = pDemand_->firstDay_, nbDays = pDemand_->nbDays_;

  // write to stringstream that can then be printed in any output file
  // follow the template described by the competition
  rep << "SOLUTION" << std::endl;
  rep << pScenario_->thisWeek() << " " << pScenario_->name_ << std::endl;
  rep << std::endl;

  // compute the total number of assignments that are not rests
  // if no shift is assigned to a nurse on given, it still counts
  int nbAssignments = 0;
  for (int n = 0; n < nbNurses; n ++) {
    for (int day = firstDay; day < firstDay+nbDays; day++){
      if (theLiveNurses_[n]->roster_.shift(day) > 0) nbAssignments++;
    }
  }

  rep << "ASSIGNMENTS = " << nbAssignments << std::endl;
  for (int n = 0; n < nbNurses; n ++) {
    for (int day = firstDay; day < firstDay+nbDays; day++){
      int shift = theLiveNurses_[n]->roster_.shift(day);
      if (shift != 0) {
        rep << theLiveNurses_[n]->name_ << " "<< Tools::intToDay(day) << " ";
        if (shift > 0) {
          int skill = theLiveNurses_[n]->roster_.skill(day);
          rep << pScenario_->intToShift_[shift] << " ";
          rep << pScenario_->intToSkill_[skill] << std::endl;
        }
        else { // shift < 0 so no shif is assigned
          rep << "Unassigned TBD"<< std::endl;
          LiveNurse* pNurse = theLiveNurses_[n];
          std::cout  << "This shift " << pNurse->states_[day+1].shift_ << std::endl;
        }
      }
    }
  }



  return rep.str();
}


// display the solution in a more readable format and append advanced
// information on the solution quality
//
string Solver::solutionToLogString() {
  std::stringstream rep;
  int nbNurses = pScenario_->nbNurses_, nbShifts = pScenario_->nbShifts_;
  int nbSkills = pScenario_->nbSkills_;
  int firstDay = pDemand_->firstDay_, nbDays = pDemand_->nbDays_;

  rep << "Complete shift schedule" << std::endl << std::endl;
  rep << "\t\t\t";
  for (int day = firstDay; day < firstDay+nbDays; day++) {
    rep << "| " << Tools::intToDay(day).at(0) << " ";
  }
  rep << "|" << std::endl;
  rep << "-------------------------------------"<< std::endl;

  for (int n = 0; n < nbNurses; n ++) {
    LiveNurse* pNurse = theLiveNurses_[n];
    rep << pNurse->name_ << "\t";
    for (int day = firstDay; day < firstDay+nbDays; day++){
      int shift = pNurse->roster_.shift(day);
      if (shift != 0) {
        rep << "| " <<  pScenario_->intToShift_[shift].at(0) << " ";
      }
      else {
        rep << "| - ";
      }
    }
    rep << "|" << std::endl;
  }
  rep << std::endl;

  // compute the total cost and in the mean time update the structures of each
  // live nurse that contains all the required information on soft and hard
  // constraints satisfaction
  //
  double totalCost = solutionCost();

  // store temporarily the data that is about to be written
  //
  int violMinCover = 0, violReqSkill = 0, violForbiddenSucc = 0;
  double costOptCover = 0, costTotalDays = 0, costTotalWeekEnds = 0;
  double costConsDays = 0, costConsDaysOff=0, costConsShifts = 0, costPref = 0, costWeekEnds = 0;

  // constraints related to the demand
  for (int day = firstDay; day < firstDay+nbDays; day++) {
    for (int sh = 1; sh < nbShifts; sh++) {
      for (int sk = 0; sk < nbSkills; sk++) {
        violMinCover += std::max(0,pDemand_->minDemand_[day][sh][sk]-satisfiedDemand_[day][sh][sk]);
        costOptCover += WEIGHT_OPTIMAL_DEMAND
          * std::max(0,pDemand_->optDemand_[day][sh][sk]-satisfiedDemand_[day][sh][sk]);
      }
    }
  }


  for (int n = 0; n < nbNurses; n ++) {
    LiveNurse* pNurse = theLiveNurses_[n];
    costTotalDays += pNurse->statCt_.costTotalDays_;
    costTotalWeekEnds += pNurse->statCt_.costTotalWeekEnds_;

    for (int day = firstDay; day < firstDay+nbDays; day++){
      // record the violations
      int skill = pNurse->roster_.skill(day);
      int shift = pNurse->roster_.shift(day);
      int prevShift = pNurse->states_[day].shift_;
      violReqSkill += shift == 0 ? 0 : (pNurse->hasSkill(skill)? 0:1);
      violForbiddenSucc += pScenario_->isForbiddenSuccessor(shift, prevShift)? 1: 0;

      // the other costs per soft constraint can be read from the stat structure
      costConsDays += pNurse->statCt_.costConsDays_[day];
      costConsDaysOff += pNurse->statCt_.costConsDaysOff_[day];
      costConsShifts += pNurse->statCt_.costConsShifts_[day];
      costPref += pNurse->statCt_.costPref_[day];
      costWeekEnds += pNurse->statCt_.costWeekEnd_[day];
    }
  }

  // write the status of hard and soft constraints
  //
  rep << "Hard constraints violations\n";
  rep << "---------------------------\n";
  rep << "Minimal coverage constraints: " << violMinCover << std::endl;
  rep << "Required skill constraints: " << violReqSkill << std::endl;
  rep << "Illegal shift type succession constraints: " << violForbiddenSucc << std::endl;
  rep << "Single assignment per day: 0" << std::endl;

  rep << "\nCost per constraint type\n";
  rep << "------------------------\n";
  rep << "Total assignment constraints: " << costTotalDays << std::endl;
  //rep << "Consecutive constraints: " << costConsDays+costConsShifts << std::endl;
  rep << "Consecutive working days constraints: " << costConsDays << std::endl;
  rep << "Consecutive days off constraints: " << costConsDaysOff << std::endl;
  rep << "Consecutive shifts constraints: " << costConsShifts << std::endl;
  rep << "Non working days constraints: " << std::endl;
  rep << "Preferences: " << costPref << std::endl;
  rep << "Max working weekend: " << costTotalWeekEnds << std::endl;
  rep << "Complete weekends: " << costWeekEnds << std::endl;
  rep << "Optimal coverage constraints: " << costOptCover << std::endl;

  rep << "\n---------------------------\n";
  rep << "\nTotal cost: " << totalCost << std::endl;

  return rep.str();
}
