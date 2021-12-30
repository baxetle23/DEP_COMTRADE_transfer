#ifndef TRACER_LOGGER_H_
#define TRACER_LOGGER_H_
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <string>
#include <mutex>
#include <memory>

// // default value
#define PATH_FILE "/mnt/user/log.txt" 
#define PATH_SCRIPT "/home/sort_file.sh"

using Clock = std::chrono::steady_clock;

class TraceLogger {
public:
	TraceLogger(const std::string& name_file = PATH_FILE) {
        out_file.open(name_file, std::ios_base::out | std::ios_base::app);
        out_file << "Construct TraceLogger";
        std::cout << "Construct TraceLogger";
        PrintTimeCure();
    }



    // void WriteToLog(const std::string& name_function) {
    //     out_file << name_function;
    //     std::cout << name_function;
    //     PrintTimeCure();
    // }

    void WriteToLog(const std::string& name_function, const std::string& string) {
        out_file << name_function << ": " << string;
        std::cout << name_function << ": " << string;
        PrintTimeCure();
    }

    template<typename T>
    void WriteToLog(const std::string& name_function, const std::string& str, T&& value) {
        out_file << name_function << ": " << str << std::forward<T>(value);
        std::cout << name_function << ": " << str << std::forward<T>(value);
        PrintTimeCure();
    }

    template<typename First, typename ... Rest>
    void WriteToLog(const std::string& name_function, const std::string& str, First&& first, Rest&& ... rest) {
        WriteToLog(name_function, str, first);
        WriteToLog(name_function, std::forward<Rest>(rest)...);
    }

	~TraceLogger(){
        out_file << "Destruct TraceLogger";
        std::cout << "Destruct TraceLogger";
        PrintTimeCure();
    }
	
private:

    void PrintTimeCure() {
        out_file << " | " << getTimeStr() << std::endl;
        std::cout << " | " << getTimeStr() << std::endl;
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

#endif