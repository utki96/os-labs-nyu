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

using namespace std;

bool VERBOSE = false;
string INPUT_FILE, RAND_FILE, SCHEDULING_ALGO_PARAM;
int QUANTUM = 10000, MAX_PRIO = 4;
int RAND_OFFSET = 0, RAND_LIMIT = 0;
int LAST_EVENT_TIME = 0, IO_TIME = 0, CPU_TIME = 0, PREVIOUS_TIMESTAMP = 0, IO_PROCESS_COUNT = 0;

enum ProcessState { CREATED, READY, RUNNING, BLOCKED, COMPLETED };
enum TransitionState { TRANS_TO_READY, TRANS_TO_PREEMPT, TRANS_TO_BLOCK, TRANS_TO_RUN, TRANS_TO_COMPLETE };

const char* ProcessStateText[] = {"CREATED", "READY", "RUNNG", "BLOCK", "COMPLETED" } ;

class Process {
    public:
    int id, arrivalTime, totalCpuTime, cpuBurst, ioBurst, staticPriority, dynamicPriority;
    int stateTs, waitTime, ioTime, remainingTime, quantumTime, completedTime;
    ProcessState processState;

    Process(int id, int arTime, int cpuTime, int cpuBurst, int ioBurst, int prio, ProcessState state): id(id), arrivalTime(arTime), stateTs(arrivalTime), totalCpuTime(cpuTime), 
        remainingTime(cpuTime), cpuBurst(cpuBurst), ioBurst(ioBurst), staticPriority(prio), dynamicPriority(prio - 1), processState(state) {
            waitTime = 0, ioTime = 0, quantumTime = 0, completedTime = 0;
        }
};

class Event {
    public:
    int timestamp;
    Process* process;
    TransitionState transition;

    Event(int ts, Process* p, TransitionState state): timestamp(ts), process(p), transition(state) {}
};

class Scheduler {
    public:
    virtual string getAlgorithmName() = 0;
    virtual void addProcess(Process* process) = 0;
    virtual Process* getNextProcess() = 0;
    virtual bool doesPreempt() = 0;
};

class DES {
    public:
    list<Event*> evtList;

    void putEvent(Event* event) {
        if (evtList.empty()) {
            evtList.push_front(event);
        } else {
            list<Event*>::iterator it = evtList.begin();
            while (it != evtList.end()) {
                if (event->timestamp >= (*it)->timestamp) {
                    it++;
                } else {
                    break;
                }
            }
            evtList.insert(it, event);
        }
    }

    Event* getEvent() {
        if (evtList.empty()) {
            return nullptr;
        }
        Event* evt = evtList.front();
        evtList.pop_front();
        return evt;
    }

    bool removeEvent(int timestamp, Process* process, TransitionState transition) {
        for (list<Event*>::iterator it = evtList.begin(); it != evtList.end(); it++) {
            if ((*it)->timestamp != timestamp && process == (*it)->process && transition == (*it)->transition) {
                evtList.remove(*it);
                return true;
            }
        }
        return false;
    }

    int getNextEventTime() {
        if (evtList.empty()) {
            return -1;
        }
        return evtList.front()->timestamp;
    }
};

vector<int> randvals;
vector<Process*> processList;
Process* CURRENT_RUNNING_PROCESS;
Event* PROCESS_EVT;

class FCFS: public Scheduler {
    private:
    queue<Process*> processQueue;

    public:
    string getAlgorithmName() {
        return "FCFS";
    }

    void addProcess(Process* process) {
        processQueue.push(process);
    }

    Process* getNextProcess() {
        if (processQueue.empty()) {
            return nullptr;
        }
        Process* p = processQueue.front();
        processQueue.pop();
        return p;
    }

    bool doesPreempt() {
        return false;
    }
};

class LCFS: public Scheduler {
    private:
    stack<Process*> processStack;

    public:
    string getAlgorithmName() {
        return "LCFS";
    }

    void addProcess(Process* process) {
        processStack.push(process);
    }

    Process* getNextProcess() {
        if (processStack.empty()) {
            return nullptr;
        }
        Process* p = processStack.top();
        processStack.pop();
        return p;
    }

    bool doesPreempt() {
        return false;
    }
};

class SRTF: public Scheduler {
    private:
    list<Process*> processQueue;

    public:
    string getAlgorithmName() {
        return "SRTF";
    }

    void addProcess(Process* process) {
        if (processQueue.empty()) {
            processQueue.push_front(process);
        } else {
            list<Process*>::iterator it = processQueue.begin();
            while (it != processQueue.end()) {
                if (process->remainingTime >= (*it)->remainingTime) {
                    it++;
                } else {
                    break;
                }
            }
            processQueue.insert(it, process);
        }
    }

    Process* getNextProcess() {
        if (processQueue.empty()) {
            return nullptr;
        }
        Process* p = processQueue.front();
        processQueue.pop_front();
        return p;
    }

    bool doesPreempt() {
        return false;
    }
};

class RR: public Scheduler {
    private:
    queue<Process*> processQueue;

    public:
    string getAlgorithmName() {
        return "RR " + to_string(QUANTUM);
    }

    void addProcess(Process* process) {
        if (process->dynamicPriority < 0) {
            process->dynamicPriority = process->staticPriority - 1;
        }
        processQueue.push(process);
    }

    Process* getNextProcess() {
        if (processQueue.empty()) {
            return nullptr;
        }
        Process* p = processQueue.front();
        processQueue.pop();
        return p;
    }

    bool doesPreempt() {
        return false;
    }
};

class PRIO: public Scheduler {
    private:
    int prio;
    vector<queue<Process*>> activeQueue;
    vector<queue<Process*>> expiredQueue;

    Process* findNextInActive() {
        int c = prio - 1;
        while (c >= 0) {
            if (activeQueue[c].empty()) {
                c--;
            } else {
                Process *p = activeQueue[c].front();
                activeQueue[c].pop();
                return p;
            }
        }
        return nullptr;
    }

    void swapQueues() {
        vector<queue<Process*>> ptrActive = activeQueue;
        activeQueue = expiredQueue;
        expiredQueue = ptrActive;
    }

    public:
    PRIO(int priority): prio(priority) {
        for (int i = 0; i < prio; i++) {
            activeQueue.push_back(queue<Process*>());
            expiredQueue.push_back(queue<Process*>());
        }
    }

    string getAlgorithmName() {
        return "PRIO " + to_string(QUANTUM);
    }

    void addProcess(Process* process) {
        if (process->dynamicPriority < 0) {
            process->dynamicPriority = process->staticPriority - 1;
            expiredQueue[process->dynamicPriority].push(process);
        } else {
            activeQueue[process->dynamicPriority].push(process);
        }
    }

    Process* getNextProcess() {
        Process* p = findNextInActive();
        if (p != nullptr) {
            return p;
        }
        swapQueues();
        return findNextInActive();
    }

    bool doesPreempt() {
        return false;
    }
};

class PREPRIO: public Scheduler {
    private:
    int prio;
    vector<queue<Process*>> activeQueue;
    vector<queue<Process*>> expiredQueue;

    Process* findNextInActive() {
        int c = prio - 1;
        while (c >= 0) {
            if (activeQueue[c].empty()) {
                c--;
            } else {
                Process *p = activeQueue[c].front();
                activeQueue[c].pop();
                // cout << "Process: " << p->id << ", prio: " << p->dynamicPriority << endl;
                return p;
            }
        }
        return nullptr;
    }

    void swapQueues() {
        vector<queue<Process*>> ptrActive = activeQueue;
        activeQueue = expiredQueue;
        expiredQueue = ptrActive;
    }

    public:
    PREPRIO(int priority): prio(priority) {
        for (int i = 0; i < prio; i++) {
            activeQueue.push_back(queue<Process*>());
            expiredQueue.push_back(queue<Process*>());
        }
    }

    string getAlgorithmName() {
        return "PREPRIO " + to_string(QUANTUM);
    }

    void addProcess(Process* process) {
        if (process->dynamicPriority < 0) {
            process->dynamicPriority = process->staticPriority - 1;
            // cout << "Process: " << process->id << ", Prio: " << process->dynamicPriority <<  ", expired" << endl;
            expiredQueue[process->dynamicPriority].push(process);
        } else {
            // cout << "Process: " << process->id << ", Prio: " << process->dynamicPriority <<  ", active" << endl;
            activeQueue[process->dynamicPriority].push(process);
        }
    }

    Process* getNextProcess() {
        Process* p = findNextInActive();
        if (p != nullptr) {
            return p;
        }
        swapQueues();
        return findNextInActive();
    }

    bool doesPreempt() {
        return true;
    }
};

int getRandomNumber(int burst) {
    int val = 1 + (randvals[RAND_OFFSET] % burst);
    RAND_OFFSET = (RAND_OFFSET + 1) % RAND_LIMIT;
    return val;
}

void readInputFile(string fileName, DES& des) {
    ifstream inputFile(fileName);
    int id = 0;
    if (inputFile.is_open()) {
        string line;
        while (getline(inputFile, line)) {
            int tokens[4];
            stringstream ss(line);
            string token;
            int c = 0;
            while (ss >> token) {
                tokens[c++] = atoi(token.c_str());
            }
            Process* p = new Process(id, tokens[0], tokens[1], tokens[2], tokens[3], getRandomNumber(MAX_PRIO), CREATED);
            processList.push_back(p);
            Event* evt = new Event(p->arrivalTime, p, TRANS_TO_READY);
            des.putEvent(evt);
            id++;
        }
        inputFile.close();
    } else {
        cout << "Unable to open file " << fileName << endl;
    }
}

void readRandomValuesFile(string fileName) {
    ifstream inputFile(fileName);
    bool firstLine = true;
    if (inputFile.is_open()) {
        string line;
        while (getline(inputFile, line)) {
            int val = atoi(line.c_str());
            if (firstLine) {
                firstLine = false;
                RAND_LIMIT = val;
            } else {
                randvals.push_back(val);
            }
        }
        inputFile.close();
    } else {
        cout << "Unable to open file " << fileName << endl;
    }
}

bool processEvent(DES* des) {
    PROCESS_EVT = des->getEvent();
    return PROCESS_EVT != nullptr;
}

void Simulation(Scheduler* scheduler, DES* des) {
    while (processEvent(des)) {
        Process* process = PROCESS_EVT->process;
        int currentTime = PROCESS_EVT->timestamp;
        TransitionState transition = PROCESS_EVT->transition;
        int timeInPrevState = currentTime - process->stateTs;
        // cout << currentTime << ": ID - " << process->id << " " << process->arrivalTime << " " << process->remainingTime << " " << process->cpuBurst << " " << process->ioBurst << " " << process->processState << " " << PROCESS_EVT->transition << "\n";
        
        if (IO_PROCESS_COUNT > 0) {
            IO_TIME = IO_TIME + (currentTime - PREVIOUS_TIMESTAMP);
        }
        if (CURRENT_RUNNING_PROCESS != nullptr) {
            CPU_TIME = CPU_TIME + (currentTime - PREVIOUS_TIMESTAMP);
        }
        bool callScheduler = false;
        PREVIOUS_TIMESTAMP = currentTime;
        switch(transition) {
            case TRANS_TO_READY: {
                if (VERBOSE) printf("%d %d %d: %s -> %s\n", currentTime, process->id, timeInPrevState, ProcessStateText[process->processState], "READY");
                if (process->processState == BLOCKED) {
                    IO_PROCESS_COUNT--;
                    process->ioTime = process->ioTime + timeInPrevState;
                    process->dynamicPriority = process->staticPriority - 1;
                }
                if (scheduler->doesPreempt() && CURRENT_RUNNING_PROCESS != nullptr && process->dynamicPriority > CURRENT_RUNNING_PROCESS->dynamicPriority) {
                    bool removed = des->removeEvent(currentTime, CURRENT_RUNNING_PROCESS, TRANS_TO_PREEMPT);
                    removed = removed || des->removeEvent(currentTime, CURRENT_RUNNING_PROCESS, TRANS_TO_BLOCK);
                    removed = removed || des->removeEvent(currentTime, CURRENT_RUNNING_PROCESS, TRANS_TO_COMPLETE);
                    if (removed) {
                        des->putEvent(new Event(currentTime, CURRENT_RUNNING_PROCESS, TRANS_TO_PREEMPT));
                    }
                }
                process->processState =  READY; process->stateTs = currentTime;
                scheduler->addProcess(process);
                callScheduler = true;
                break;
            }
            case TRANS_TO_RUN: {
                int runTime = process->quantumTime;
                if (runTime == 0) {
                    runTime = min(process->remainingTime, getRandomNumber(process->cpuBurst));
                    process->quantumTime = runTime;
                }
                if (VERBOSE) printf("%d %d %d: %s -> %s cb=%d rem=%d prio=%d\n", currentTime, process->id, timeInPrevState, ProcessStateText[process->processState], "RUNNG", runTime, process->remainingTime, process->dynamicPriority);
                process->processState =  RUNNING; process->stateTs = currentTime;
                process->waitTime = process->waitTime + timeInPrevState;
                if (QUANTUM < process->quantumTime) {
                    Event* evt = new Event(currentTime + QUANTUM, process, TRANS_TO_PREEMPT);
                    des->putEvent(evt);
                } else if (runTime == process->remainingTime) {
                    Event* evt = new Event(currentTime + runTime, process, TRANS_TO_COMPLETE);
                    des->putEvent(evt);
                } else {
                    Event* evt = new Event(currentTime + runTime, process, TRANS_TO_BLOCK);
                    des->putEvent(evt);
                }
                break;
            }
            case TRANS_TO_BLOCK: {
                IO_PROCESS_COUNT++;
                CURRENT_RUNNING_PROCESS = nullptr;
                int runTime = getRandomNumber(process->ioBurst);
                process->remainingTime = process->remainingTime - timeInPrevState; process->quantumTime = process->quantumTime - timeInPrevState;
                if (VERBOSE) printf("%d %d %d: %s -> %s  ib=%d rem=%d\n", currentTime, process->id, timeInPrevState, ProcessStateText[process->processState], "BLOCK", runTime, process->remainingTime);
                process->processState =  BLOCKED; process->stateTs = currentTime;
                Event* evt = new Event(currentTime + runTime, process, TRANS_TO_READY);
                des->putEvent(evt);
                callScheduler = true;
                break;
            }
            case TRANS_TO_PREEMPT: {
                process->remainingTime = process->remainingTime - timeInPrevState; process->quantumTime = process->quantumTime - timeInPrevState;
                if (VERBOSE) printf("%d %d %d: %s -> %s  cb=%d rem=%d prio=%d\n", currentTime, process->id, timeInPrevState, ProcessStateText[process->processState], "READY", process->quantumTime, process->remainingTime, process->dynamicPriority);
                process->dynamicPriority = process->dynamicPriority - 1;
                CURRENT_RUNNING_PROCESS = nullptr;
                process->processState =  READY; process->stateTs = currentTime;
                scheduler->addProcess(process);
                callScheduler = true;
                break;
            }
            case TRANS_TO_COMPLETE: {
                if (VERBOSE) printf("%d %d %d: %s\n", currentTime, process->id, timeInPrevState, "Done");
                CURRENT_RUNNING_PROCESS = nullptr;
                process->processState =  COMPLETED; process->completedTime = currentTime;
                LAST_EVENT_TIME = currentTime;
                callScheduler = true;
            }
        }

        if (callScheduler) {
            if (des->getNextEventTime() == currentTime)
                continue; //process next event from Event queue
            if (CURRENT_RUNNING_PROCESS == nullptr) {
                CURRENT_RUNNING_PROCESS = scheduler->getNextProcess();
                if (CURRENT_RUNNING_PROCESS == nullptr)
                    continue;
                Event* evt = new Event(currentTime, CURRENT_RUNNING_PROCESS, TRANS_TO_RUN);
                des->putEvent(evt);
            }
        }
    }
}

string getId(int pid, int limit) {
    string instr = to_string(pid);
    int len = instr.length();
    string prefix = "";
    for (int i = 0; i < limit - len; i++) {
        prefix.append("0");
    }
    return prefix.append(instr);
}

template<typename T> void printElement(T t, const int& width) {
    cout << right << setw(width) << setfill(' ') << t;
}

void printStats(Scheduler* scheduler) {
    cout << scheduler->getAlgorithmName() << endl;
    double cpuUtil = 0.0, ioUtil = 0.0, avgTurn = 0.0, avgWait = 0.0;
    for (Process* p : processList) {
        cout << getId(p->id, 4) << ": ";
        printElement(p->arrivalTime, 4); cout << " "; printElement(p->totalCpuTime, 4); cout << " "; printElement(p->cpuBurst, 4); cout << " ";
        printElement(p->ioBurst, 4); cout << " "; printElement(p->staticPriority, 1);
        cout << " | ";
        printElement(p->completedTime, 5); cout << " "; printElement(p->completedTime - p->arrivalTime, 5); cout << " "; 
        printElement(p->ioTime, 5); cout << " "; printElement(p->waitTime, 5);
        cout << endl;
        ioUtil = ioUtil + p->ioTime;
        avgTurn = avgTurn + p->completedTime - p->arrivalTime;
        avgWait = avgWait + p->waitTime;
    }
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", LAST_EVENT_TIME, (CPU_TIME * 100.0) / (double) LAST_EVENT_TIME, (IO_TIME * 100.0) / (double) LAST_EVENT_TIME, 
        avgTurn / processList.size(), avgWait / processList.size(), (processList.size() * 100.0) / (double) LAST_EVENT_TIME);
}

Scheduler* getSchedulingAlgorithm(string& param) {
    char algo;
    int quantum = 0, maxPrio = 0;
    int n = sscanf(param.c_str(), "%c%d:%d", &algo, &quantum, &maxPrio);
    if (n == 3) {
        QUANTUM = quantum;
        MAX_PRIO = maxPrio;
    } else if (n == 2) {
        QUANTUM = quantum;
    }

    if (algo == 'F') {
        return new FCFS;
    } else if (algo == 'L') {
        return new LCFS;
    } else if (algo == 'S') {
        return new SRTF;
    } else if (algo == 'R') {
        return new RR;
    } else if (algo == 'P') {
        return new PRIO(MAX_PRIO);
    } else {
        return new PREPRIO(MAX_PRIO);
    }
}

void readArguments(int argc, char** argv) {
    int opt;
    while ((opt = getopt (argc, argv, "vtepis:")) != -1) {
        switch (opt) {
            case 'v': 
                VERBOSE = true;
                break;
            case 's':
                SCHEDULING_ALGO_PARAM = optarg;
                break;
        }
    }
    int pos = 0;
        for (int index = optind; index < argc; index++) {
            if (pos == 0) {
                INPUT_FILE = argv[index];
            }
            if (pos == 1) {
                RAND_FILE = argv[index];
            }
            pos++;
        }
}

int main(int argc, char** argv) {
    DES des;
    try {
        readArguments(argc, argv);
        Scheduler* scheduler = getSchedulingAlgorithm(SCHEDULING_ALGO_PARAM);
        readRandomValuesFile(RAND_FILE);
        readInputFile(INPUT_FILE, des);
        Simulation(scheduler, &des);
        printStats(scheduler);
    } catch (...) {
        cout << "Default Error" << endl;
    }
    return 0;
}