#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <queue>
#include <stack>
#include <iomanip>
#include <limits>
#include <cmath>

using namespace std;

string INPUT_FILE;
char SCHEDULING_ALGO_PARAM;
bool PRINT_QUEUE_INFO = false, PRINT_QUEUE_FLOOK_INFO = false, VERBOSE = false;

vector<int> timeInput;
vector<int> sectorInput;
float ioUtilTime = 0;
int totalTime = 1, totalMovement = 0, DIRECTION = 1, SECTOR_POINTER = 0, ACTIVE_PROCESS_ID = -1, COMPLETED_IO = 0;

class ProcessStats {
    public:
    int startTime, endTime;

    ProcessStats(): startTime(0), endTime(0) {}
    ProcessStats(int start, int end): startTime(start), endTime(end) {}
};
vector<ProcessStats*> processStats;

class Scheduler {
    public:
    int index = 0;

    void loadProcess() {
        if (index < timeInput.size() && timeInput[index] <= totalTime) {
            if (VERBOSE) {
                printf("%d: %5d add %d\n", totalTime, index, sectorInput[index]);
            }
            addProcess(index);
            index++;
        }
    }

    virtual void addProcess(int id) = 0;
    virtual int getNextProcessId() = 0;
};

class FIFO: public Scheduler {
    private:
    queue<int> processQueue;

    public:
    void addProcess(int id) {
        processQueue.push(id);
    }

    int getNextProcessId() {
        if (processQueue.empty()) {
            return -1;
        }
        int id = processQueue.front();
        processQueue.pop();
        return id;
    }
};

class SSTF: public Scheduler {
    private:
    list<ProcessStats*> processList;

    public:
    void addProcess(int id) {
        ProcessStats* ps = new ProcessStats(id, sectorInput[id]);
        processList.push_back(ps);
    }

    int getNextProcessId() {
        if (processList.empty()) {
            return -1;
        }
        string queStr = "";
        int lowestSeekTime = std::numeric_limits<int>::max(), id = -1;
        auto nextPs = processList.begin();
        for (auto it = processList.begin(); it != processList.end(); ++it) {
            int seekTime = abs((*it)->endTime - SECTOR_POINTER);
            if (PRINT_QUEUE_INFO) {
                queStr = queStr + std::to_string((*it)->startTime) + ":" + std::to_string(seekTime) + " ";
            }
            if (seekTime < lowestSeekTime) {
                nextPs = it;
                lowestSeekTime = seekTime;
                id = (*it)->startTime;
            }
        }
        if (PRINT_QUEUE_INFO) { printf("%8s%s\n", "", queStr.c_str()); }
        processList.erase(nextPs);
        return id;
    }
};

class LOOK: public Scheduler {
    private:
    list<int> processQueue;

    public:
    void addProcess(int id) {
        processQueue.push_back(id);
    }

    std::tuple<int, std::string, bool> getProcessIdFromQueue(list<int>& processQueue) {
        string queStrings[2] = {"(", "("}, queStr = "";
        int processIds[2] = {-1, -1};     // 0 -> Positive, 1 -> Negative
        auto processRemPos = processQueue.begin(), processRemNeg = processQueue.begin(), processRem = processQueue.begin();
        int pid = -1;
        bool changeDir = false;
        for (auto it = processQueue.begin(); it != processQueue.end(); ++it) {
            int pid = (*it);
            int seekTime = sectorInput[pid] - SECTOR_POINTER;
            if (seekTime == 0) {
                if (PRINT_QUEUE_INFO) {
                    queStrings[0] = queStrings[0] + std::to_string(pid) + ":" + std::to_string(abs(seekTime)) + " ";
                    queStrings[1] = queStrings[1] + std::to_string(pid) + ":" + std::to_string(abs(seekTime)) + " ";
                }
                processIds[0] = pid;
                processRemPos = it;
                processIds[1] = pid;
                processRemNeg = it;
            } else if (seekTime > 0) {
                if (PRINT_QUEUE_INFO) {
                    queStrings[0] = queStrings[0] + std::to_string(pid) + ":" + std::to_string(abs(seekTime)) + " ";
                }
                if (processIds[0] == -1 || (seekTime < (sectorInput[processIds[0]] - SECTOR_POINTER))) {
                    processIds[0] = pid;
                    processRemPos = it;
                }
            } else {
                if (PRINT_QUEUE_INFO) {
                    queStrings[1] = queStrings[1] + std::to_string(pid) + ":" + std::to_string(abs(seekTime)) + " ";
                }
                if (processIds[1] == -1 || (abs(seekTime) < abs(sectorInput[processIds[1]] - SECTOR_POINTER))) {
                    processIds[1] = pid;
                    processRemNeg = it;
                }
            }
        }
        if (DIRECTION == 1) {
            if (processIds[0] != -1) {
                processRem = processRemPos;
                pid = processIds[0];
                queStr = queStrings[0];
            } else {
                changeDir = true;
                DIRECTION = -1;
                processRem = processRemNeg;
                pid = processIds[1];
                queStr = queStrings[1];
            }
        } else {
            if (processIds[1] != -1) {
                processRem = processRemNeg;
                pid = processIds[1];
                queStr = queStrings[1];
            } else {
                changeDir = true;
                DIRECTION = 1;
                processRem = processRemPos;
                pid = processIds[0];
                queStr = queStrings[0];
            }
        }
        processQueue.erase(processRem);
        return std::make_tuple(pid, queStr + ")", changeDir);
    }

    int getNextProcessId() {
        if (processQueue.empty()) {
            return -1;
        }
        auto result = getProcessIdFromQueue(processQueue);
        int pid = std::get<0>(result);
        string queStr = std::get<1>(result);
        bool changeDir = std::get<2>(result);
        if (PRINT_QUEUE_INFO && changeDir) { printf("%8sGet: () --> change direction to %d\n", "", DIRECTION); }
        if (PRINT_QUEUE_INFO) { printf("%8sGet: %s --> %d dir=%d\n", "", queStr.c_str(), pid, DIRECTION); }
        return pid;
    }
};

class CLOOK: public Scheduler {
    private:
    list<int> processQueue;

    public:
    void addProcess(int id) {
        processQueue.push_back(id);
    }

    int getNextProcessId() {
        if (processQueue.empty()) {
            return -1;
        }
        string queStr = "(";
        int processIds[2] = {-1, -1};     // 0 -> Positive, 1 -> Negative
        auto processRemPos = processQueue.begin(), processRemNeg = processQueue.begin(), processRem = processQueue.begin();
        int pid = -1;
        bool changeDir = false;
        for (auto it = processQueue.begin(); it != processQueue.end(); ++it) {
            int pidCurrent = (*it);
            int seekTime = sectorInput[pidCurrent] - SECTOR_POINTER;
            queStr = queStr + std::to_string(pidCurrent) + ":" + std::to_string(seekTime) + " ";
            if (seekTime >= 0) {
                if (processIds[0] == -1 || (seekTime < (sectorInput[processIds[0]] - SECTOR_POINTER))) {
                    processIds[0] = pidCurrent;
                    processRemPos = it;
                }
            } else {
                if (processIds[1] == -1 || (sectorInput[pidCurrent] < sectorInput[processIds[1]])) {
                    processIds[1] = pidCurrent;
                    processRemNeg = it;
                }
            }
        }
        if (processIds[0] != -1) {
            processRem = processRemPos;
            pid = processIds[0];
        } else {
            changeDir = true;
            processRem = processRemNeg;
            pid = processIds[1];
        }
        if (PRINT_QUEUE_INFO && changeDir) { printf("%8sGet: %s --> go to bottom and pick %d\n", "", (queStr + ")").c_str(), pid); }
        if (PRINT_QUEUE_INFO && ! changeDir) { printf("%8sGet: %s --> %d\n", "", (queStr + ")").c_str(), pid); }
        processQueue.erase(processRem);
        return pid;
    }
};

class FLOOK: public LOOK {
    private:
    list<int> activeQueue;
    list<int> addQueue;
    int ACTIVE_QUEUE = 1, ADD_QUEUE = 0;

    string printList(list<int>& queue, bool getDiff, bool initialSpace, bool mulDir, bool absolute) {
        string strOut = "( ";
        if (! initialSpace) {
            strOut = "(";
        }
        for (auto it = queue.begin(); it != queue.end(); ++it) {
            string processStr =  to_string(*it) + ":" + to_string(sectorInput[(*it)]);
            if (getDiff) {
                if (mulDir) {
                    processStr = processStr + ":" + to_string((sectorInput[(*it)] - SECTOR_POINTER) * DIRECTION);
                } else if (absolute) {
                    processStr = processStr + ":" + to_string(abs(sectorInput[(*it)] - SECTOR_POINTER));
                } else {
                    processStr = processStr + ":" + to_string(sectorInput[(*it)] - SECTOR_POINTER);
                }
            }
            strOut = strOut + processStr + " ";
        }
        return strOut + ")";
    }

    void swapLists(list<int>* p1, list<int>* p2) {
        std::swap(*p1, *p2);
    }

    public:
    void addProcess(int id) {
        addQueue.push_back(id);
        if (PRINT_QUEUE_INFO) { printf("%3sQ=%d %s\n", "", ADD_QUEUE, printList(addQueue, false, true, false, false).c_str()); }
    }

    int getNextProcessId() {
        if (activeQueue.empty()) {
            if (addQueue.empty()) {
                return -1;
            }
            int temp = ACTIVE_QUEUE; ACTIVE_QUEUE = ADD_QUEUE; ADD_QUEUE = temp;
            swapLists(&activeQueue, &addQueue);
            return getNextProcessId();
        }
        string activeQueStr = "", activeQueStrDir = "", activeQueChDirStr = "", addQueStr = "";
        if (PRINT_QUEUE_INFO) {
            activeQueStr = printList(activeQueue, true, true, false, false), addQueStr = printList(addQueue, true, true, false, false);
            printf("AQ=%d dir=%d curtrack=%d:  Q[0] = %s  Q[1] = %s \n", ACTIVE_QUEUE, DIRECTION, SECTOR_POINTER, 
                ACTIVE_QUEUE == 0 ? activeQueStr.c_str() : addQueStr.c_str(), ACTIVE_QUEUE == 0 ? addQueStr.c_str() : activeQueStr.c_str());
            activeQueStr = printList(activeQueue, true, false, false, false);
            activeQueChDirStr = printList(activeQueue, true, false, false, true);
            activeQueStrDir = printList(activeQueue, true, false, true, false);
        }
        auto result = getProcessIdFromQueue(activeQueue);
        int pid = std::get<0>(result);
        string queStr = std::get<1>(result);
        bool changeDir = std::get<2>(result);
        if (PRINT_QUEUE_INFO) {
            if (changeDir) {
                printf("%8sGet: %s --> change direction to %d\n", "", activeQueStrDir.c_str(), DIRECTION);
                printf("%8sGet: %s --> %d dir=%d\n", "", activeQueChDirStr.c_str(), pid, DIRECTION);
            } else {
                printf("%8sGet: %s --> %d dir=%d\n", "", activeQueStr.c_str(), pid, DIRECTION);
            }
        }
        if (PRINT_QUEUE_FLOOK_INFO) { printf("%d: %7d get Q=%d\n", totalTime, pid, ACTIVE_QUEUE); }
        return pid;
    }
};

string readLineFromFile(ifstream* fileStream) {
    string line;
    while (getline(*fileStream, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        return line;
    }
    return "";
}

void readInputFile(string fileName) {
    ifstream inputFile(fileName);
    if (inputFile.is_open()) {
        string line = readLineFromFile(&inputFile);
        while (line != "") {
            int time, sector;
            std::istringstream iss(line);
            iss >> time >> sector;
            timeInput.push_back(time);
            sectorInput.push_back(sector);
            ProcessStats* processStat = new ProcessStats();
            processStats.push_back(processStat);
            line = readLineFromFile(&inputFile);
        }
        inputFile.close();
    } else {
        cout << "Unable to open file " << fileName << "\n";
    }
}

void readArguments(int argc, char** argv) {
    int opt;
    while ((opt = getopt (argc, argv, "s:vqf")) != -1) {
        switch (opt) {
            case 's':
                SCHEDULING_ALGO_PARAM = optarg[0];
                break;
            case 'v': 
                VERBOSE = true;
                break;
            case 'q':
                PRINT_QUEUE_INFO = true;
                break;
            case 'f': 
                PRINT_QUEUE_FLOOK_INFO = true;
                break;
            default:
                break;
        }
    }
    int pos = 0;
        for (int index = optind; index < argc; index++) {
            if (pos == 0) {
                INPUT_FILE = argv[index];
            }
            pos++;
        }
}

Scheduler* getScheduler() {
    switch (SCHEDULING_ALGO_PARAM) {
        case 'N': return new FIFO;
        case 'S': return new SSTF;
        case 'L': return new LOOK;
        case 'C': return new CLOOK;
        case 'F': return new FLOOK;
        default: 
            printf("Incorrect Scheduling Param\n");
            return nullptr;
    }
}

void printProcessStats() {
    long turnAroundSum = 0, waitTimeSum = 0;
    int maxWaitTime = 0;
    for (int i = 0; i < timeInput.size(); i++) {
        ProcessStats* pStat = processStats[i];
        printf("%5d: %5d %5d %5d\n", i, timeInput[i], pStat->startTime, pStat->endTime);
        int waitTime = pStat->startTime - timeInput[i];
        maxWaitTime = max(maxWaitTime, waitTime);
        waitTimeSum = waitTimeSum + waitTime;
        turnAroundSum = turnAroundSum + (pStat->endTime - timeInput[i]);
    }
    printf("SUM: %d %d %.4lf %.2lf %.2lf %d\n", totalTime, totalMovement, (ioUtilTime / (double) totalTime), ((double) turnAroundSum / timeInput.size()), ((double) waitTimeSum / timeInput.size()), maxWaitTime);
}

void runSimulation(Scheduler* scheduler) {
    if (VERBOSE) { printf("TRACE\n"); }
    while (true) {
        scheduler->loadProcess();
        if (ACTIVE_PROCESS_ID == -1) {
            int nextProcessId = scheduler->getNextProcessId();
            if (nextProcessId == -1) {
                if (COMPLETED_IO == timeInput.size()) {
                    return;
                }
            } else {
                ACTIVE_PROCESS_ID = nextProcessId;
                ProcessStats* processStat = processStats[ACTIVE_PROCESS_ID];
                processStat->startTime = totalTime;
                if (VERBOSE) { printf("%d: %5d issue %d %d\n", totalTime, ACTIVE_PROCESS_ID, sectorInput[ACTIVE_PROCESS_ID], SECTOR_POINTER); }
                if (sectorInput[ACTIVE_PROCESS_ID] == SECTOR_POINTER) {
                    continue;
                }
                if (sectorInput[ACTIVE_PROCESS_ID] > SECTOR_POINTER) {
                    DIRECTION = 1;
                } else {
                    DIRECTION = -1;
                }
                continue;
            }
        } else {
            if (sectorInput[ACTIVE_PROCESS_ID] == SECTOR_POINTER) {
                if (VERBOSE) { printf("%d: %5d finish %d\n", totalTime, ACTIVE_PROCESS_ID, (totalTime - timeInput[ACTIVE_PROCESS_ID])); }
                ProcessStats* processStat = processStats[ACTIVE_PROCESS_ID];
                processStat->endTime = totalTime;
                ACTIVE_PROCESS_ID = -1;
                COMPLETED_IO++;
                continue;
            } else {
                SECTOR_POINTER = SECTOR_POINTER + DIRECTION;
                ioUtilTime++;
                totalMovement++;
            }
        }
        totalTime++;
    }
}

int main(int argc, char** argv) {
    try {
        readArguments(argc, argv);
        readInputFile(INPUT_FILE);
        Scheduler* scheduler = getScheduler();
        runSimulation(scheduler);
        printProcessStats();
    } catch (const std::exception& e) {
        std::cerr << "Caught unexpected exception: " << e.what() << std::endl;
    }
    return 0;
}