#pragma once
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <string>
#include <mutex>
#include <memory>

// default value
#define PATH_FILE "/home/aponyatov/Desktop/tracelogger/log.txt" 

using Clock = std::chrono::steady_clock;

class TraceLogger {
public:
	TraceLogger(const std::string& name_file = PATH_FILE) {
        out_file.open(name_file, std::ios_base::out | std::ios_base::app);
        out_file << "Construct TraceLogger";
        PrintTimeCure();
    }


    template<typename T>
    void WriteToLog(const std::string& name_function, const std::string& str, T&& value) {
        out_file << name_function << ": " << str << std::forward<T>(value);
        PrintTimeCure();
    }

    template<typename First, typename ... Rest>
    void WriteToLog(const std::string& name_function, const std::string& str, First&& first, Rest&& ... rest) {
        WriteToLog(name_function, str, first);
        WriteToLog(name_function, std::forward<Rest>(rest)...);
    }

	~TraceLogger(){
        out_file << "Destruct TraceLogger";
        PrintTimeCure();
    }
	
private:

    void PrintTimeCure() {
        out_file << " | " << getTimeStr() << std::endl;
    }

    std::string getTimeStr() const {
        std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::string s(100, 0);
        int size_actual = std::strftime(&s[0], s.size(), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        s.resize(size_actual);
        return s;
    }
    
    std::ofstream out_file;
};

TraceLogger& log() {
    static TraceLogger log;
    return log;
}

