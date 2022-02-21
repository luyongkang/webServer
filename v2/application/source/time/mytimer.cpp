#include "time/mytimer.h"

#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

using std::localtime;
using std::put_time;
using std::string;
using std::stringstream;
using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::system_clock;

string getNowTime()
{
	static stringstream ss;
	ss.str("");

	auto nowTimePoint = system_clock::now();
	time_t nowTimeT = system_clock::to_time_t(nowTimePoint);
	ss << put_time(localtime(&nowTimeT), "%Y-%m-%d %H:%M:%S");

	auto timeSinceEpoch = nowTimePoint.time_since_epoch();
	int64_t microSecTime = duration_cast<microseconds>(timeSinceEpoch).count();
	unsigned short microSec = microSecTime % 1000;
	unsigned short milliSec = microSecTime / 1000 % 1000;
	ss << ":" << milliSec << ":" << microSec;

	return ss.str();
}