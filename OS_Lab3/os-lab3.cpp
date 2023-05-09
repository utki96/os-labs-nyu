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

const int PAGE_TABLE_SIZE = 64;
int FRAME_TABLE_SIZE = 128;
enum PageOperation { READ, WRITE, CONTEXT_SWITCH, EXIT, MAP, UNMAP, IN, OUT, FIN, FOUT, ZERO, SEGV, SEGPROT };
const char* PageOperationText[] = {"READ", "WRITE", "CONTEXT_SWITCH", "EXIT", "MAP", "UNMAP", "IN", "OUT", "FIN", "FOUT", "ZERO", "SEGV", "SEGPROT"};
map<int, int> instrCostMap = { {WRITE, 1}, {READ, 1}, {CONTEXT_SWITCH, 130}, {EXIT, 1230}, {MAP, 350}, {UNMAP, 410}, {IN, 3200}, {OUT, 2750}, {FIN, 2350}, 
                        {FOUT, 2800}, {ZERO, 150}, {SEGV, 440}, {SEGPROT, 410} };

template<typename T> void printElement(T t, const int& width) {
    cout << right << setw(width) << setfill(' ') << t;
}

class VMA {
    public:
    int startVPage, endVPage, writeProtected, fileMapped; 

    VMA(int startVP, int endVP, int writePr, int fileMp): startVPage(startVP), endVPage(endVP), writeProtected(writePr), fileMapped(fileMp) {}
};

struct PTE {
    unsigned int frame: 7;
    unsigned int present: 1;
    unsigned int referenced: 1;
    unsigned int modified: 1;
    unsigned int writeProtected: 1;
    unsigned int pagedOut: 1;
    unsigned int fileMapped: 1;
    unsigned int existsVma: 1;

    PTE() : frame(0), present(0), referenced(0), modified(0), writeProtected(0), pagedOut(0), fileMapped(0), existsVma(0) {}
} pack;

class ProcessStats {
    public:
    unsigned long unmaps, maps, ins, outs, fins, fouts, zeros, segv, segprot;

    ProcessStats(): unmaps(0), maps(0), ins(0), outs(0), fins(0), fouts(0), zeros(0), segv(0), segprot(0) {}
};

class Process {
    public:
    int id;
    bool loadedBefore;
    vector<VMA*> vmas;
    PTE pte[PAGE_TABLE_SIZE];
    ProcessStats* pstats;

    Process(int id, ProcessStats* ps): id(id), loadedBefore(false), pstats(ps) {}

    void addVma(VMA* vma) {
        vmas.push_back(vma);
    }

    void loadPTEFromVMAs() {
        for (auto vma = vmas.begin(); vma != vmas.end(); ++vma) {
            for (int i = (*vma)->startVPage; i <= (*vma)->endVPage; i++) {
                pte[i].writeProtected = (*vma)->writeProtected;
                pte[i].fileMapped = (*vma)->fileMapped;
                pte[i].existsVma = 1;
            }
        }
    }
};

struct FrameEntry {
    int frameId, processId, pageValue, timeLastUse;
    unsigned int age;
    bool isMapped;

    FrameEntry(int id): frameId(id), age(0), timeLastUse(0), isMapped(false) {}
};

class Pager {
    public:
    virtual void frameSelected(FrameEntry* fte) = 0;
    virtual FrameEntry* getVictimFrame() = 0;
};

class FrameTable {
    public:
    vector<FrameEntry*> frames;
    list<FrameEntry*> freeFrames;
    Pager* pager;

    FrameTable(int frameTableSize, Pager* pg): pager(pg) {
        for (int i = 0; i < frameTableSize; i++) {
            FrameEntry* fe = new FrameEntry(i);
            frames.push_back(fe);
            freeFrames.push_back(fe);
        }
    }

    void makeFrameAvailable(FrameEntry* frame) {
        freeFrames.push_back(frame);
    }

    FrameEntry* getNextFrame() {
        FrameEntry* fe;
        if (!freeFrames.empty()) {
            fe = freeFrames.front();
            freeFrames.pop_front();
        } else {
            fe = pager->getVictimFrame();
        }
        pager->frameSelected(fe);
        return fe;
    }
};

class Instruction {
    public:
    char instrType;
    int value;

    Instruction(char inTp, int v): instrType(inTp), value(v) {}
};

string INPUT_FILE, RAND_FILE;
char PAGING_ALGO_PARAM;
FrameTable* frameTable = nullptr;
Process* CURRENT_PROCESS = nullptr;
vector<Instruction*> instructions;
vector<Process*> processList;
vector<int> randvals;
unsigned long instCount = 0, ctxSwitches = 0, processExits = 0;
unsigned long long totalCost = 0;
bool PRINT_PTE_EACH_INSTR = false, PRINT_ALL_PTE = false, PRINT_FTE_EACH_INSTR = false, PRINT_ASELECT = false;
bool PRINT_FINAL_PAGE_TABLES = false, PRINT_FINAL_FRAME_TABLE = false, PRINT_FINAL_STATS = false, PRINT_STEPS = false;
int RAND_OFFSET = 0, RAND_LIMIT = 0;


class FIFO: public Pager {
    public:
    int index = 0;

    void frameSelected(FrameEntry* fte) {
        // do nothing
    }

    FrameEntry* getVictimFrame() {
        if (PRINT_ASELECT) {
            cout << "ASELECT " << index << "\n";
        }
        FrameEntry* fe = frameTable->frames[index];
        index = (index + 1) % FRAME_TABLE_SIZE;
        return fe;
    }
};

int getRandomNumber(int MOD) {
    int val = randvals[RAND_OFFSET] % MOD;
    RAND_OFFSET = (RAND_OFFSET + 1) % RAND_LIMIT;
    return val;
}

class RANDOM: public Pager {
    public:
    void frameSelected(FrameEntry* fte) {
        // do nothing
    }

    FrameEntry* getVictimFrame() {
        int index = getRandomNumber(FRAME_TABLE_SIZE);
        FrameEntry* fe = frameTable->frames[index];
        return fe;
    }
};

class CLOCK: public Pager {
    public:
    int index = 0;

    void frameSelected(FrameEntry* fte) {
        // do nothing
    }

    FrameEntry* getVictimFrame() {
        int startIndex = index, count = 0;
        while (true) {
            FrameEntry* fe = frameTable->frames[index];
            int pId = fe->processId, vpage = fe->pageValue;
            count++;
            if (processList[pId]->pte[vpage].referenced == 1) {
                PTE* pte = &processList[pId]->pte[vpage];
                pte->referenced = 0;
                index = (index + 1) % FRAME_TABLE_SIZE;
            } else {
                if (PRINT_ASELECT) {
                    cout << "ASELECT " << startIndex << " " << count << "\n";
                }
                index = (index + 1) % FRAME_TABLE_SIZE;
                return fe;
            }
        }
    }
};

class NRU: public Pager {
    private:
    int index = 0;
    int lastInstrReset = -1;
    const int NUM_CLASSES = 4, RESET_THRESHOLD = 50;

    public:
    void frameSelected(FrameEntry* fte) {
        // do nothing
    }

    FrameEntry* getVictimFrame() {
        int victim[4] = {-1, -1, -1, -1};
        int startIndex = index, count = 0, lowestClass = NUM_CLASSES + 1, resetRef = (instCount - lastInstrReset >= RESET_THRESHOLD) ? 1 : 0;
        lastInstrReset = (resetRef == 1) ? instCount : lastInstrReset;
        
        while (count < FRAME_TABLE_SIZE) {
            FrameEntry* frame = frameTable->frames[index];
            int pId = frame->processId, vpage = frame->pageValue;
            int score = 2 * processList[pId]->pte[vpage].referenced + processList[pId]->pte[vpage].modified;
            if (victim[score] == -1) {
                victim[score] = index;
            }
            if (score < lowestClass) {
                lowestClass = score;
            }
            if (resetRef) {
                PTE* pte = &processList[pId]->pte[vpage];
                pte->referenced = 0;
            }
            index = (index + 1) % FRAME_TABLE_SIZE;
            count++;
            if (lowestClass == 0 && resetRef == 0) {
                break;
            }
        }
        
        index = (victim[lowestClass] + 1) % FRAME_TABLE_SIZE;
        if (PRINT_ASELECT) {
            cout << "ASELECT: hand="; printElement(startIndex, 2); cout << " " << resetRef << " | " << lowestClass << " ";
            printElement(victim[lowestClass], 2); cout << " "; printElement(count, 2); cout << "\n";
        }
        return frameTable->frames[victim[lowestClass]];
    }
};

class AGING: public Pager {
    public:
    int index = 0;

    void frameSelected(FrameEntry* fte) {
        fte->age = 0;
    }

    FrameEntry* getVictimFrame() {
        int count = 0, lowestIndex = index, lowestAge = 0x80000001, startIndex = index;
        string frameOut = "";
        
        while (count < FRAME_TABLE_SIZE) {
            FrameEntry* fe = frameTable->frames[index];
            int pId = fe->processId, vpage = fe->pageValue;
            PTE* pte = &processList[pId]->pte[vpage];
            fe->age = fe->age >> 1;
            if (pte->referenced == 1) {
                fe->age = (fe->age | 0x80000000);
            }
            pte->referenced = 0;
            if (fe->age < lowestAge) {
                lowestAge = fe->age;
                lowestIndex = index;
            }
            if (PRINT_ASELECT) {
                std::stringstream ss;
                ss << std::hex << fe->age;
                frameOut = frameOut + " " + std::to_string(index) + ":" + ss.str();
            }
            index = (index + 1) % FRAME_TABLE_SIZE;
            count++;
        }

        if (PRINT_ASELECT) {
            cout << "ASELECT " << startIndex << "-" << ((index - 1 + FRAME_TABLE_SIZE) % FRAME_TABLE_SIZE) << " |" << frameOut << " | " << lowestIndex << "\n";
        }
        index = (lowestIndex + 1) % FRAME_TABLE_SIZE;
        return frameTable->frames[lowestIndex];
    }
};

class WSET: public Pager {
    private:
    int index = 0;
    const int TIME_THRESHOLD = 50;

    public:
    void frameSelected(FrameEntry* fte) {
        fte->timeLastUse = instCount;
    }

    FrameEntry* getVictimFrame() {
        int count = 0, lowestIndex = index, lowestTime = instCount, startIndex = index;
        string frameOut = "";
        
        while (count < FRAME_TABLE_SIZE) {
            count++;
            FrameEntry* fe = frameTable->frames[index];
            int pId = fe->processId, vpage = fe->pageValue;
            PTE* pte = &processList[pId]->pte[vpage];
            if (PRINT_ASELECT) {
                frameOut = frameOut + " " + to_string(index) + "(" + to_string(pte->referenced) + " " + to_string(pId) + ":" + to_string(vpage) + " " + 
                            to_string(fe->timeLastUse) + ")";
            }
            if (pte->referenced == 1) {
                fe->timeLastUse = instCount;
                pte->referenced = 0;
            } else if ((instCount - fe->timeLastUse) >= TIME_THRESHOLD) {
                lowestIndex = index;
                if (PRINT_ASELECT) { frameOut = frameOut + " STOP(" + to_string(count) + ")"; }
                break;
            } else if (fe->timeLastUse < lowestTime) {
                lowestTime = fe->timeLastUse;
                lowestIndex = index;
            }
            index = (index + 1) % FRAME_TABLE_SIZE;
        }

        if (PRINT_ASELECT) {
            cout << "ASELECT " << startIndex << "-" << ((startIndex - 1 + FRAME_TABLE_SIZE) % FRAME_TABLE_SIZE) << " |" << frameOut << " | " << lowestIndex << "\n";
        }
        index = (lowestIndex + 1) % FRAME_TABLE_SIZE;
        return frameTable->frames[lowestIndex];
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
        int numProcesses = stoi(readLineFromFile(&inputFile));
        for (int id = 0; id < numProcesses; id++) {
            ProcessStats* pstats = new ProcessStats();
            Process* proc = new Process(id, pstats);
            int numVma = stoi(readLineFromFile(&inputFile));
            for (int vma = 0; vma < numVma; vma++) {
                std::istringstream iss(readLineFromFile(&inputFile));
                int start, end, wp, fm;
                iss >> start >> end >> wp >> fm;
                VMA* vmaObj = new VMA(start, end, wp, fm);
                proc->addVma(vmaObj);
            }
            processList.push_back(proc);
        }
        string line = readLineFromFile(&inputFile);
        while (line != "") {
            std::istringstream iss(line);
            char inst; int val;
            iss >> inst >> val;
            Instruction* ins = new Instruction(inst, val);
            instructions.push_back(ins);
            line = readLineFromFile(&inputFile);
        }
        inputFile.close();
    } else {
        cout << "Unable to open file " << fileName << "\n";
    }
}

void readRandomFile(string fileName) {
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
        cout << "Unable to open file " << fileName << "\n";
    }
}

Pager* getPagingAlgorithm() {
    switch (PAGING_ALGO_PARAM) {
        case 'f': return new FIFO;
        case 'r': return new RANDOM;
        case 'c': return new CLOCK;
        case 'e': return new NRU;
        case 'a': return new AGING;
        case 'w': return new WSET;
        default:
            return nullptr;
    }
}

void readArguments(int argc, char** argv) {
    int opt;
    while ((opt = getopt (argc, argv, "f:a:o:")) != -1) {
        switch (opt) {
            case 'f': 
                FRAME_TABLE_SIZE = std::atoi(optarg);
                break;
            case 'a':
                PAGING_ALGO_PARAM = optarg[0];
                break;
            case 'o':
                if (optarg != NULL && optarg != "") {
                    PRINT_PTE_EACH_INSTR = std::strchr(optarg, 'x') != nullptr ? true : false;
                    PRINT_FTE_EACH_INSTR = std::strchr(optarg, 'f') != nullptr ? true : false;
                    PRINT_ASELECT = std::strchr(optarg, 'a') != nullptr ? true : false;
                    PRINT_ALL_PTE = std::strchr(optarg, 'y') != nullptr ? true : false;
                    PRINT_FINAL_PAGE_TABLES = std::strchr(optarg, 'P') != nullptr ? true : false;
                    PRINT_FINAL_FRAME_TABLE = std::strchr(optarg, 'F') != nullptr ? true : false;
                    PRINT_FINAL_STATS = std::strchr(optarg, 'S') != nullptr ? true : false;
                    PRINT_STEPS = std::strchr(optarg, 'O') != nullptr ? true : false;
                    break;
                }
            default:
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

void printProcessPageTable(Process* proc) {
    cout << "PT[" << proc->id << "]:";
    for (int index = 0; index < PAGE_TABLE_SIZE; index++) {
        if (proc->pte[index].existsVma == 0) {
            cout << " *";
        } else {
            string out = std::to_string(index) + ":";
            if (proc->pte[index].present == 0) {
                if (proc->pte[index].pagedOut == 1) {
                    cout << " #";
                } else {
                    cout << " *";
                }
            } else {
                out = out + (proc->pte[index].referenced == 1 ? "R" : "-");
                out = out + (proc->pte[index].modified == 1 ? "M" : "-");
                out = out + (proc->pte[index].pagedOut == 1 ? "S" : "-");
                cout << " " << out;
            }
        }
    }
    cout << "\n";
}

void printProcessPageTables() {
    for (auto it = processList.begin(); it != processList.end(); ++it) {
        printProcessPageTable(*it);
    }
}

void printFrameTable() {
    cout << "FT:";
    for (auto frame = frameTable->frames.begin(); frame != frameTable->frames.end(); ++frame) {
        if ((*frame)->isMapped) {
            cout << " " << (*frame)->processId << ":" << (*frame)->pageValue;
        } else {
            cout << " *";
        }
    }
    cout << "\n";
}

void printProcessStats(Process* proc) {
    ProcessStats* pstats = proc->pstats;
    printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
        proc->id, pstats->unmaps, pstats->maps, pstats->ins, pstats->outs, pstats->fins, pstats->fouts, pstats->zeros, pstats->segv, pstats->segprot);
}

void printProcessStats() {
    for (auto it = processList.begin(); it != processList.end(); ++it) {
        printProcessStats(*it);
    }
}

void clearVictimFrame(FrameEntry* frame) {
    if (PRINT_STEPS) { cout << " " << PageOperationText[UNMAP] << " " << frame->processId << ":" << frame->pageValue << "\n"; }
    totalCost = totalCost + instrCostMap[UNMAP];
    Process* victimProcess = processList[frame->processId];
    PTE* pte = &victimProcess->pte[frame->pageValue];
    pte->present = 0;
    victimProcess->pstats->unmaps = victimProcess->pstats->unmaps + 1;
    if (pte->modified == 1) {
        if (pte->fileMapped == 1) {
            if (PRINT_STEPS) { cout << " " << PageOperationText[FOUT] << "\n"; }
            victimProcess->pstats->fouts = victimProcess->pstats->fouts + 1;
            totalCost = totalCost + instrCostMap[FOUT];
        } else {
            if (PRINT_STEPS) { cout << " " << PageOperationText[OUT] << "\n"; }
            pte->pagedOut = 1;
            victimProcess->pstats->outs = victimProcess->pstats->outs + 1;
            totalCost = totalCost + instrCostMap[OUT];
        }
    }
    pte->referenced = 0;
    pte->modified = 0;
    frame->isMapped = false;
}

void releaseFrames(Process* process) {
    for (int index = 0; index < PAGE_TABLE_SIZE; index++) {
        PTE* pte = &process->pte[index];
        if (pte->present == 1) {
            FrameEntry* frame = frameTable->frames[pte->frame];
            if (PRINT_STEPS) { cout << " " << PageOperationText[UNMAP] << " " << frame->processId << ":" << frame->pageValue << "\n"; }
            totalCost = totalCost + instrCostMap[UNMAP];
            process->pstats->unmaps = process->pstats->unmaps + 1;
            frame->isMapped = false;
            if (pte->modified == 1 && pte->fileMapped == 1) {
                if (PRINT_STEPS) { cout << " " << PageOperationText[FOUT] << "\n"; }
                process->pstats->fouts = process->pstats->fouts + 1;
                totalCost = totalCost + instrCostMap[FOUT];
            }
            frameTable->makeFrameAvailable(frame);
        }
        pte->present = 0;
        pte->pagedOut = 0;
        pte->referenced = 0;
        pte->modified = 0;
    }
}

void loadNewFrame(FrameEntry* frame, PTE* pte, int vpage) {
    if (pte->fileMapped == 1) {
        if (PRINT_STEPS) { cout << " " << PageOperationText[FIN] << "\n"; }
        totalCost = totalCost + instrCostMap[FIN];
        CURRENT_PROCESS->pstats->fins = CURRENT_PROCESS->pstats->fins + 1;
    } else if (pte->pagedOut == 1) {
        if (PRINT_STEPS) { cout << " " << PageOperationText[IN] << "\n"; }
        totalCost = totalCost + instrCostMap[IN];
        CURRENT_PROCESS->pstats->ins = CURRENT_PROCESS->pstats->ins + 1;
    } else {
        if (PRINT_STEPS) { cout << " " << PageOperationText[ZERO] << "\n"; }
        totalCost = totalCost + instrCostMap[ZERO];
        CURRENT_PROCESS->pstats->zeros = CURRENT_PROCESS->pstats->zeros + 1;
    }

    if (PRINT_STEPS) { cout << " " << PageOperationText[MAP] << " " << frame->frameId << "\n"; }
    frame->processId = CURRENT_PROCESS->id;
    frame->pageValue = vpage;
    frame->isMapped = true;
    pte->present = 1;
    pte->frame = frame->frameId;
    totalCost = totalCost + instrCostMap[MAP];
    CURRENT_PROCESS->pstats->maps = CURRENT_PROCESS->pstats->maps + 1;
}

void runSimulation() {
    for (auto instr = instructions.begin(); instr != instructions.end(); instr++, instCount++) {
        bool printPTE = true;
        char instType = (*instr)->instrType;
        int vpage = (*instr)->value;
        if (PRINT_STEPS) { cout << instCount << ": ==> " << instType << " " << vpage << "\n"; }
        switch (instType) {
            case 'c':
                CURRENT_PROCESS = processList[vpage];
                if (! CURRENT_PROCESS->loadedBefore) {
                    CURRENT_PROCESS->loadPTEFromVMAs();
                    CURRENT_PROCESS->loadedBefore = true;
                }
                ctxSwitches++;
                totalCost = totalCost + instrCostMap[CONTEXT_SWITCH];
                printPTE = false;
                break;
            case 'e':
                /*if (PRINT_STEPS) {*/ cout << "EXIT current process " << CURRENT_PROCESS->id << "\n"; //}
                processExits++;
                totalCost = totalCost + instrCostMap[EXIT];
                releaseFrames(CURRENT_PROCESS);
                printPTE = false;
                break;
            case 'r':
            case 'w':
                PTE* pte = &CURRENT_PROCESS->pte[vpage];
                totalCost = totalCost + instrCostMap[READ];
                if (pte->present == 0) {
                    if (pte->existsVma == 0) {
                        if (PRINT_STEPS) { cout << " " << PageOperationText[SEGV] << "\n"; }
                        totalCost = totalCost + instrCostMap[SEGV];
                        CURRENT_PROCESS->pstats->segv = CURRENT_PROCESS->pstats->segv + 1;
                        printPTE = false;
                        break;
                    }
                    
                    FrameEntry* victimFrame = frameTable->getNextFrame();
                    if (victimFrame->isMapped && (victimFrame->processId != CURRENT_PROCESS->id || victimFrame->pageValue != vpage)) {
                        clearVictimFrame(victimFrame);
                    }
                    loadNewFrame(victimFrame, pte, vpage);
                }
                pte->referenced = 1;
                if (instType == 'w') {
                    if (pte->writeProtected == 1) {
                        if (PRINT_STEPS) { cout << " " << PageOperationText[SEGPROT] << "\n"; }
                        totalCost = totalCost + instrCostMap[SEGPROT];
                        CURRENT_PROCESS->pstats->segprot = CURRENT_PROCESS->pstats->segprot + 1;
                        break;
                    } else {
                        pte->modified = 1;
                    }
                }
                break;
        }
        if (PRINT_ALL_PTE &&  printPTE) {
            printProcessPageTables();
        } else if (PRINT_PTE_EACH_INSTR &&  printPTE) {
            printProcessPageTable(CURRENT_PROCESS);
        }
        if (PRINT_FTE_EACH_INSTR && printPTE) {
            printFrameTable();
        }
    }
}

void printSimulationStats() {
    printf("TOTALCOST %lu %lu %lu %llu %lu\n", instCount, ctxSwitches, processExits, totalCost, sizeof(PTE));
}

int main(int argc, char** argv) {
    try {
        readArguments(argc, argv);
        readInputFile(INPUT_FILE);
        readRandomFile(RAND_FILE);
        Pager* pager = getPagingAlgorithm();
        frameTable = new FrameTable(FRAME_TABLE_SIZE, pager);
        runSimulation();
        if (PRINT_FINAL_PAGE_TABLES) { printProcessPageTables(); }
        if (PRINT_FINAL_FRAME_TABLE) { printFrameTable(); }
        if (PRINT_FINAL_STATS) {
            printProcessStats();
            printSimulationStats();
        }
    } catch (const std::exception& e) {
        std::cerr << "Caught unexpected exception: " << e.what() << std::endl;
    } catch (...) {
        cout << "Default Error" << "\n";
    }
    return 0;
}