//
//  Scenario.cpp
//  Project:RosterDesNurses
//
//  Created by J��r��my Omer on 18/12/2013.
//  Copyright (c) 2014 J��r��my Omer. All rights reserved.
//

#include <sstream>
#include <string>

#include "Scenario.h"
#include "MyTools.h"
#include "Nurse.h"

//-----------------------------------------------------------------------------
//
//  C l a s s   S c e n a r i o
//
//  Class that contains all the attributes describing the scenario
//
//-----------------------------------------------------------------------------

// Constructor and destructor
//
Scenario::Scenario(string name, int nbWeeks,
		int nbSkills, vector<string> intToSkill, map<string,int> skillToInt,
		int nbShifts, vector<string> intToShift, map<string,int> shiftToInt,
		vector<int> minConsShifts, vector<int> maxConsShifts,
		vector<int> nbForbiddenSuccessors, vector2D forbiddenSuccessors,
		int nbContracts, vector<string> intToContract, map<string,Contract*> contracts,
		int nbNurses, vector<Nurse>& theNurses, map<string,int> nurseNameToInt) :
		name_(name), nbWeeks_(nbWeeks),
		nbSkills_(nbSkills), intToSkill_(intToSkill), skillToInt_(skillToInt),
		nbShifts_(nbShifts), intToShift_(intToShift), shiftToInt_(shiftToInt),
		minConsShifts_(minConsShifts), maxConsShifts_(maxConsShifts),
		nbForbiddenSuccessors_(nbForbiddenSuccessors), forbiddenSuccessors_(forbiddenSuccessors),
		nbContracts_(nbContracts), intToContract_(intToContract), contracts_(contracts),
		nbNurses_(nbNurses), theNurses_(theNurses), nurseNameToInt_(nurseNameToInt),
		nbPositions_(0), nbShiftOffRequests_(0),
		pWeekDemand_(0){

	// To make sure that it is modified later when reading the history data file
	//
	thisWeek_ = -1;
	nbWeeksLoaded_ = 1;

	// Preprocess the vector of nurses
	// This creates the positions
	//
	this->preprocessTheNurses();
}

// Hybrid copy constructor : this is only called when constructing a new scenario that copies most parameters
// from the input scenario but for only a subgroup of nurses
//
Scenario::Scenario(Scenario* pScenario,  vector<Nurse>& theNurses, Demand* pDemand, Preferences* pWeekPreferences):
		name_(pScenario->name_), nbWeeks_(pScenario->nbWeeks_),
		nbSkills_(pScenario->nbSkills_), intToSkill_(pScenario->intToSkill_), skillToInt_(pScenario->skillToInt_),
		nbShifts_(pScenario->nbShifts_), intToShift_(pScenario->intToShift_), shiftToInt_(pScenario->shiftToInt_),
		minConsShifts_(pScenario->minConsShifts_), maxConsShifts_(pScenario->maxConsShifts_),
		nbForbiddenSuccessors_(pScenario->nbForbiddenSuccessors_), forbiddenSuccessors_(pScenario->forbiddenSuccessors_),
		nbContracts_(pScenario->nbContracts_), intToContract_(pScenario->intToContract_), contracts_(pScenario->contracts_),
		nbNurses_(theNurses.size()), theNurses_(theNurses), nurseNameToInt_(pScenario->nurseNameToInt_),
		thisWeek_(pScenario->thisWeek()), nbWeeksLoaded_(pScenario->nbWeeksLoaded()),
		nbPositions_(0), nursesPerPosition_(0), nbShiftOffRequests_(0), pWeekDemand_(0){

	// Preprocess the vector of nurses
	// This creates the positions
	//
	this->preprocessTheNurses();


	// The nurses are already preprocessed at this stage
	// Load the input week demand and preferences
	//
	this->linkWithDemand(pDemand);
	this->linkWithPreferences(*pWeekPreferences);
}

Scenario::Scenario(Scenario* pScenario):name_(pScenario->name_), nbWeeks_(pScenario->nbWeeks_),
nbSkills_(pScenario->nbSkills_), intToSkill_(pScenario->intToSkill_), skillToInt_(pScenario->skillToInt_),
nbShifts_(pScenario->nbShifts_), intToShift_(pScenario->intToShift_), shiftToInt_(pScenario->shiftToInt_),
minConsShifts_(pScenario->minConsShifts_), maxConsShifts_(pScenario->maxConsShifts_),
nbForbiddenSuccessors_(pScenario->nbForbiddenSuccessors_), forbiddenSuccessors_(pScenario->forbiddenSuccessors_),
nbContracts_(pScenario->nbContracts_), intToContract_(pScenario->intToContract_), contracts_(pScenario->contracts_),
nbNurses_(pScenario->nbNurses()), theNurses_(pScenario->theNurses_), nurseNameToInt_(pScenario->nurseNameToInt_),
thisWeek_(pScenario->thisWeek()), nbWeeksLoaded_(pScenario->nbWeeksLoaded()),
nbPositions_(0), nursesPerPosition_(0), nbShiftOffRequests_(0), pWeekDemand_(0) {
	// Preprocess the vector of nurses
	// This creates the positions
	//
	this->preprocessTheNurses();

}

Scenario::~Scenario(){
	// delete the contracts
	for(map<string,Contract*>::const_iterator itC = contracts_.begin(); itC != contracts_.end(); ++itC){
		delete (itC->second);
	}
  //delete pPositions_;
	while (!pPositions_.empty()){
		if (pPositions_.back()) delete pPositions_.back();
		pPositions_.pop_back();
	}

	delete pWeekDemand_;
}

// return true if the shift shNext is a forbidden successor of sh
//
bool Scenario::isForbiddenSuccessor(int shNext, int shLast) {
	if (shLast <= 0) return false;

	for (int i = 0; i < nbForbiddenSuccessors_[shLast]; i++) {
		if (shNext == forbiddenSuccessors_[shLast][i])  {
			return true;
		}
	}
	return false;
}

// update the scenario to treat a new week
//
void Scenario::updateNewWeek(Demand* pDemand, Preferences& preferences, vector<State> &initialStates) {

	// delete the current demand
	if(pWeekDemand_) delete pWeekDemand_;

	// set the demand, preferences and initial states
	this->linkWithDemand(pDemand);
	this->linkWithPreferences(preferences);
	this->setInitialState(initialStates);

	// update the index of the week
	thisWeek_++;

}

//------------------------------------------------
// Display functions
//------------------------------------------------

// Display methods: toString + override operator<< (easier)
//
string Scenario::toString(){
	std::stringstream rep;
	rep << "############################################################################" << std::endl;
	rep << "##############################    Scenario    ##############################" << std::endl;
	rep << "############################################################################" << std::endl;
	rep << "# " << std::endl;
	rep << "# NAME             \t= " << name_ << std::endl;
	rep << "# NUMBER_OF_WEEKS  \t= " << nbWeeks_ <<std::endl;
	rep << "# " << std::endl;
	rep << "# SKILLS           \t= " << nbSkills_ << std::endl;
	for(int i=0; i<nbSkills_; i++){
		rep << "#                  \t= " << i << ":" << intToSkill_[i] << std::endl;
	}
	rep << "# " << std::endl;
	rep << "# SHIFTS           \t= " << nbShifts_ << std::endl;
	for(int i=0; i<nbShifts_; i++){
		rep << "#                  \t= ";
		rep << i << ":" << intToShift_[i] << " \t(" << minConsShifts_[i] << "," << maxConsShifts_[i] << ")" << std::endl;
	}
	rep << "# " << std::endl;
	rep << "# FORBIDDEN        " << std::endl;
	for(int i=0; i<nbShifts_; i++){
		rep << "#\t\t\t" << intToShift_[i] << "\t-> ";
		for(int j=0; j<nbForbiddenSuccessors_[i]; j++){
			rep << intToShift_[forbiddenSuccessors_[i][j]] << " ";
		}
		rep << std::endl;
	}
	rep << "# CONTRACTS        " << std::endl;
	for(map<string,Contract*>::const_iterator itC = contracts_.begin(); itC != contracts_.end(); ++itC){
		rep << "#\t\t\t" << *(itC->second) << std::endl;
	}
	rep << "# " << std::endl;
	rep << "# NURSES           \t= " << nbNurses_ << std::endl;
	for(int i=0; i<nbNurses_; i++){
		rep << "#\t\t\t" << theNurses_[i].toString() << std::endl;
	}
	rep << "# " << std::endl;
	rep << "# POSITIONS        \t= " << nbPositions_ << std::endl;
	for (int i=0; i<nbPositions_; i++) {
		rep << "#\t\t\t" << pPositions_[i]->toString() << std::endl;
	}
	if (weekName_!=""){
		// write the demand using the member method toString
		// do not write the preprocessed information at this stage
		//
		rep << pWeekDemand_->toString(false) << std::endl;

		// write the preferences
		//
		rep << "# " << std::endl;
		rep << "# WISHED SHIFTS OFF" << std::endl;
		for(Nurse nurse:theNurses_){
			// Display only if the nurse has preferences
			map<int,set<int> >* prefNurse = weekPreferences_.nurseWishesOff(nurse.id_);
			if(!prefNurse->empty()){
				rep << "#\t\t\t" << nurse.id_ << "\t" << nurse.name_ << "\t";
				for(map<int,set<int> >::iterator itWishlist = prefNurse->begin(); itWishlist != prefNurse->end(); ++itWishlist){
					rep << Tools::intToDay(itWishlist->first) << ": ";
					set<int> dayList = itWishlist->second;
					bool first = true;
					for(set<int>::iterator itShift = dayList.begin(); itShift != dayList.end(); ++itShift){
						if(first) first = false; else rep << ",";
						rep << intToShift_[*itShift];
					}
					rep << "    ";
				}
				rep << std::endl;
			}
		}
	}
	if(thisWeek_ > -1){
		rep << "# " << std::endl;
		rep << "# INITIAL STATE    \t= WEEK Nb " << thisWeek_ << std::endl;
		for(int n=0; n<nbNurses_; n++){
			rep << "#\t\t\t" << theNurses_[n].name_ << " ";
			State s = initialState_[n];
			rep << s.totalDaysWorked_ << " " << s.totalWeekendsWorked_ << " " << intToShift_[s.shift_] << " ";
			if(s.shift_) rep << s.consShifts_ << " " << s.consDaysWorked_; else	rep << "0 0";
			if(s.shift_) rep << " 0"; else rep << " " << s.consShifts_;
			rep << std::endl;
		}
	}
	rep << "############################################################################" << std::endl;
	return rep.str();
}


//------------------------------------------------
// Preprocess functions
//------------------------------------------------

// preprocess the nurses to get the types
//
void Scenario::preprocessTheNurses() {

	if (nbPositions_) {
		Tools::throwError("The nurse preprocessing is run for the second time!");
	}

	// Go through the nurses, and create their positions when it has not already
	// been done
	//
	vector<Nurse>::const_iterator itNurse = theNurses_.begin();
	for (Nurse nurse: theNurses_)	{
		bool positionExists = nbPositions_? true:false;
		int nbSkills = nurse.nbSkills_;
		vector<int> skills = nurse.skills_;
		vector<Position*>::iterator itPos = pPositions_.begin();

		// go through every existing position to see if the position of this nurse
		// has already been created
		for (itPos = pPositions_.begin(); itPos != pPositions_.end(); itPos++)	{
			positionExists = true;
			if ((*itPos)->nbSkills_ == nbSkills) {
				for (int i = 0; i < nbSkills; i++) {
					if (skills[i] != (*itPos)->skills_[i])	{
						positionExists = false;
						break;
					}
				}
			}
			else positionExists = false;
			if (positionExists) break;
		}

		// create the position if if doesn't exist
		if (!positionExists) {
			pPositions_.push_back(new Position(nbPositions_, nbSkills, skills));
			nbPositions_++;
		}
	}

	// build the list of position dominance
	for (int i=0; i<nbPositions_; i++) {
		for (int j=i+1; j<nbPositions_; j++) {
			if(pPositions_[i]->compare(*pPositions_[j]) == 1) {
				pPositions_[i]->addBelow(pPositions_[j]);
				pPositions_[j]->addAbove(pPositions_[i]);
			}
			if(pPositions_[i]->compare(*pPositions_[j]) == -1) {
				pPositions_[i]->addAbove(pPositions_[j]);
				pPositions_[j]->addBelow(pPositions_[i]);
			}
		}
	}

	// compute the rank of each position
	vector<bool> isRanked;
	bool isAllRanked = false;
	for (int i =0; i < nbPositions_; i++)	{
		isRanked.push_back(false);
		pPositions_[i]->rank(0);
		pPositions_[i]->resetAbove();
		pPositions_[i]->resetBelow();
	}
	while (!isAllRanked) {
		for (int i=0; i<nbPositions_; i++) {
			if (!isRanked[i]) {
				int rankIni = pPositions_[i]->rank();

				// go through the positions above the considered position to increment
				// its rank
				if (pPositions_[i]->nbAbove()) {
					for (int j = 0; j < pPositions_[i]->nbAbove(); j++) {
						int currentRank = pPositions_[i]->rank();
						int newRank = pPositions_[j]->rank()+1;
						pPositions_[i]->rank(std::max(currentRank, newRank));
					}
				}

				// the position is treated when the rank is not modified by the loop above
				if (pPositions_[i]->rank() == rankIni) isRanked[i] = true;
			}
		}

		// check if all the positions are ranked
		isAllRanked = true;
		for (int i=0; i<nbPositions_; i++) {
			if (!isRanked[i]) {
				isAllRanked = false;
				break;
			}
		}
	}

	// Organize the nurses by position
	for (int i = 0; i < nbPositions(); i++) {
		vector<Nurse> nursesInThisPosition;
		nursesPerPosition_.push_back(nursesInThisPosition);
	}
	for (Nurse nurse: theNurses_) {
		// the skills of the nurse need to be compared to the skills of each
		// existing position to determine the position of the nurse
		bool isPosition = true;
		for (int i = 0; i < nbPositions() ; i++)	{
			Position* pPosition = pPositions_[i];
			isPosition = true;
			if (pPosition->nbSkills_ == nurse.nbSkills_) {
				for (int i = 0; i < nurse.nbSkills_; i++) {
					if (nurse.skills_[i] != pPosition->skills_[i])	{
						isPosition = false;
						break;
					}
				}
			}
			else isPosition = false;

			if (isPosition) {
				nursesPerPosition_[pPosition->id()].push_back(nurse);
				break;
			}
		}
		if (!isPosition) {
			Tools::throwError("The nurse has no position!");
		}
	}
}

// Compare nurses in ascending order of their ideas
//
bool compareNursesById(Nurse* n1, Nurse* n2) {
	return (n1->id_ < n2->id_);
}

bool comparePairSecond(pair<int,int> p1, pair<int,int> p2) {
	return p1.second < p2.second;
}


// compute the connex components of the positions graph
// (one edge between two positions indicate that they share a skill)
//
void Scenario::computeConnexPositions() {

	vector<Position*> pRemainingPositions(pPositions_);

	Position* pPos = pRemainingPositions.back();
	pRemainingPositions.pop_back();


	// First build the connex components of positions
	while (!pRemainingPositions.empty()) {
		vector<Position*> connexPositions;
		connexPositions.push_back(pPos);

		vector<Position*> nextPositions;
		nextPositions.push_back(pPos);

		while (!nextPositions.empty()) {
			// treat the next position in the waiting list
			pPos = nextPositions.back();
			nextPositions.pop_back();

			// add all the positions with a common skill in the connex component
			int nbRemaining = pRemainingPositions.size();
			int ind = 0;
			while(ind < nbRemaining ) {
				if ( pPos->shareSkill(*pRemainingPositions[ind]) ) {
					// update the connex component, the waiting list and the list of
					// positions which are not in any connex component
					connexPositions.push_back(pRemainingPositions[ind]);
					nextPositions.push_back(pRemainingPositions[ind]);
					pRemainingPositions.erase(pRemainingPositions.begin()+ind);
					nbRemaining--;
				}
				else {
					ind++;
				}
			}
		}

		componentsOfConnexPositions_.push_back(connexPositions);

		// load the next remaining position
		if (!pRemainingPositions.empty()) {
			pPos = pRemainingPositions.back();
			pRemainingPositions.pop_back();
		}
	}

	// Get the nurses that belong to each component
	for (int c = 0; c < componentsOfConnexPositions_.size(); c++) {
		vector<Nurse> nursesInThisComponent;
		vector<Nurse*> pNursesInThisComponent;


		for (Position* p:componentsOfConnexPositions_[c]) {
			for (int i=0; i<nursesPerPosition_[p->id()].size(); i++) {
				// nursesInThisComponent.push_back(nursesPerPosition_[p->id()][i]);
				pNursesInThisComponent.push_back(&(nursesPerPosition_[p->id()][i]));
			}
		}
		// vector<pair<int,int>> vIdPos;
		// for (int i = 0; i <nursesInThisComponent.size(); i++) {
		// 	vIdPos.push_back(pair<int,int>(i,nursesInThisComponent[i].id_));
		// }
		// std::sort(vIdPos.begin(), vIdPos.end(),comparePairSecond);
		// vector<Nurse> nursesInThisComponentSorted;
		// for (int i = 0; i <nursesInThisComponent.size(); i++) {
		// 	nursesInThisComponentSorted.push_back(nursesInThisComponent[vIdPos[i].first]);
		// }

		std::stable_sort(pNursesInThisComponent.begin(),pNursesInThisComponent.end(),compareNursesById);
		for (Nurse* pNurse:pNursesInThisComponent) {
			nursesInThisComponent.push_back(*pNurse);
		}


		nursesPerConnexComponentOfPositions_.push_back(nursesInThisComponent);
	}
}
