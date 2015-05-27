//
//  MyTools.h
//
//  Created by J������������������r������������������my Omer on 19/11/2013.
//  Copyright (c) 2013 J������������������r������������������my Omer. All rights reserved.
//

#ifndef __MyTools__
#define __MyTools__

#include <iostream>
#include <iomanip>      // std::setprecision
#include <sstream>
#include <fstream>
#include <map>
#include <set>
#include <streambuf>
#include <string>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <time.h>
#include <exception>
#include <algorithm>
#include <cfloat>
#include <random>

#define _USE_MATH_DEFINES // needed for the constant M_PI
#include <math.h>

using std::map;
using std::string;
using std::vector;
using std::set;

static const int DEBUG = 1;
static const string REST_SHIFT = "None";
static const int DECIMALS = 3; // precision when printing floats
static const double EPSILON = .00001; // precision for the column generation
static const double BRANCH_LB = 0.8; //branch on a column, if column > branchLB
static const int NB_SHIFT_UNLIMITED = 28;

static const int LARGE_SCORE = 9999999;
static const int LARGE_TIME = 9999999;


// definitions of multi-dimensional int vector types
//
typedef vector<vector<int> > vector2D;
typedef std::vector<std::vector<std::vector<int> > > vector3D;

namespace Tools{

// class defining my own type of exceptions
//
class myException : public std::exception
{
public:
  myException( const char * Msg, int Line )
  {
      std::ostringstream oss;
      oss << "Erreur ligne " << Line << " : "
          << Msg;
      this->msg = oss.str();
  }

  virtual ~myException() throw() {}

  virtual const char * what() const throw() { return this->msg.c_str();}

private:
    std::string msg;
};

// Throw an exception with the input message
//
void throwError(const char* exceptionMsg);


// Display a debug message
//
void debugMsg(const char* debugMsg, int debugLevel);

// Read a file stream until the separating character is met
//
bool readUntilChar(std::fstream *file, char separateur, std::string *pTitle);

//Create a random generator
//the objective is to be sure to have always the same sequence of number
//
std::minstd_rand getANewRandomGenerator();

//round with probability
int roundWithProbability(double number);

// convert a number to a string
//
template < typename T >
std::string itoa(T num){
  std::stringstream stream;
  stream << std::setprecision(1) << std::fixed << num;
  return stream.str();
}

// initializes a 1D, 2D or 3D Vector of the given size (filled only with zeroes)
//
void initVector(vector<int>* v1D, int m, int val=0);
void initVector2D(vector2D* v2D, int m, int n, int val=0);
void initVector3D(vector3D* v3D, int m, int n, int p, int val=0);
void initDoubleVector(vector<double>* v1D, int m, double val=0);
void initDoubleVector2D(vector< vector< double > >* v2D, int m, int n, double val=0);

// Creates 1D vectors with random values (uniform) within a given range.
//
vector<double> randomDoubleVector(int m, double minVal, double maxVal);
vector<vector<double> > randomDoubleVector2D(int m, int n, double minVal, double maxVal);

// Appends the values of v2 to at the end of v1
//
template < typename T >
vector<T> appendVectors(vector<T> & v1, vector<T> & v2){
	vector<T> ANS;
	for(int i=0; i<v1.size(); i++) ANS.push_back(v1[i]);
	for(int i=0; i<v2.size(); i++) ANS.push_back(v2[i]);
	return ANS;
}

// To get the day from its id and vice-versa
// First day is always supposed to be a Monday
//
string intToDay(int dayId);
int dayToInt(string day);
bool isSaturday(int dayId);
bool isSunday(int dayId);
bool isWeekend(int dayId);
int containsWeekend(int startDate, int endDate);

// High resolution timer class to profile the performance of the algorithms
// Warning : the timer class of the stl to not seem to be portable I observed
// problems on windows for instance and it requires some precompiler
// instructions to work on mac
//
class Timer
{
public:

	//constructor and destructor
	//
	Timer();
	~Timer() {}

private:
	timespec cpuInit_;
	timespec cpuSinceStart_;
	timespec cpuSinceInit_;

	int coStop_;	//number of times the timer was stopped
	bool isInit_;
	bool isStarted_;
	bool isStopped_;

public:
	void init();
	bool isInit() {return isInit_;}
	void start();
	void stop();

	// get the time spent since the initialization of the timer and since the last
	// time it was started
	//
	const double dSinceInit();
	const double dSinceStart();

};

// Instantiate an obect of this class to write directly in the attribute log
// file.
// The class can be initialized with an arbitrary width if all the outputs must
// have the same minimum width. Precision can be set to force a maximum width
// as far as floating numbers are concerned.
//
class LogOutput
{
private:
  std::ostream* pLogStream_;
  bool isFormatted_;
  int width_;
  int precision_;

public:
	LogOutput(string logName):width_(0), precision_(5) {
		if (logName.empty()) {
			pLogStream_ = &(std::cout);
		}
		else {
			pLogStream_ = new std::ofstream(logName.c_str(), std::fstream::out);
		}
		// logStream_.open(logName.c_str(), std::fstream::out);
	}
	LogOutput(string logName, int width):width_(width), precision_(5) {
		pLogStream_ = new std::ofstream(logName.c_str(), std::fstream::out);
		// logStream_.open(logName.c_str(), std::fstream::out);
	}

	~LogOutput() {
		// if (pLogStream_.is_open()) logStream_.close();
	}

	void close() {
		// if (logStream_.is_open()) logStream_.close();
	}

	// switch from unformatted to formatted inputs and reversely
	//
	void switchToFormatted(int width) {
		width_ = width;
	}
	void switchToUnformatted() {
		width_=0;
	}

	// set flags in the log stream
	//
	void setFlags(std::ios_base::fmtflags flag) {pLogStream_->flags(flag);}

	// modify the precision used to write in the stream
	//
	void setPrecision(int precision) {precision_=precision;}

  // modify the width of the fields
  //
  void setWidth(int width) {width_ = width;}

  void endl() {(*pLogStream_) << std::endl;}

	// redefine the output function
	//
	template<typename T>
	LogOutput& operator<<(const T& output)
	{
		pLogStream_->width(width_);
    	pLogStream_->unsetf ( std::ios::floatfield );
    	pLogStream_->precision(precision_);
		(*pLogStream_) << std::left << std::setprecision(5) << output;

		return *this;
	}

	// write in the command window as well as in the log file
	//
	template<typename T>
	void print(const T& output) {
		(*pLogStream_) << output;
		std::cout << output;
	}

	LogOutput& operator<<(std::ostream& (*func)(std::ostream&)) {

		func(*pLogStream_);
		return *this;
	}
};

// Can be used to create an output stream that writes with a
// constant width in all future calls of << operator
//
class FormattedOutput
{
private:
	int width;
	std::ostream& stream_obj;

public:
	FormattedOutput(std::ostream& obj, int w): width(w), stream_obj(obj) {}

	template<typename T>
	FormattedOutput& operator<<(const T& output)
	{
		stream_obj.width(width);
		stream_obj << output;

		return *this;
	}

	FormattedOutput& operator<<(std::ostream& (*func)(std::ostream&))
	{
		func(stream_obj);
		return *this;
	}
};

}
#endif /* defined(__IDSReseau__MyTools__) */
