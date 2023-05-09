#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <math.h>

using namespace std;

int lineNumber = 0;
int offset = 0;
int finalOffset = 0;
char *strBegin;
char *token;
ifstream MyReadFile;

bool eof = false;
const int NOT_PRESENT = -9999;
const int MAX_SIZE = 16;
const int MAX_INS_SIZE = 512;

int moduleCount = 0;
vector<string> symbolName;
vector<int> symbolValue;
vector<vector<string>> moduleUseSymbols;
vector<vector<string>> symbolsDefined;
set<string> symbolsUsed;
vector<string> moduleWarnings;
vector<string> linkerWarnings;
map<string, string> symbolWarnings;

string __parseerror(int errcode) {
    static string errstr[] = {
    "NUM_EXPECTED", // Number expect, anything >= 2^30 is not a number either
    "SYM_EXPECTED", // Symbol Expected
    "ADDR_EXPECTED", // Addressing Expected which is A/E/I/R
    "SYM_TOO_LONG", // Symbol Name is too long
    "TOO_MANY_DEF_IN_MODULE", // > 16
    "TOO_MANY_USE_IN_MODULE", // > 16
    "TOO_MANY_INSTR", // total num_instr exceeds memory size (512)
    };
    return "Parse Error line " + to_string(lineNumber) + " offset " + to_string(offset) + ": " + errstr[errcode] + "\n";
}

string getNextToken() {
    if (token != nullptr) {
        string returnToken(token);
        offset = token - strBegin + 1;
        token = strtok(nullptr, " \t");
        return returnToken;
    }

    string lineText;
    if (getline (MyReadFile, lineText)) {
        lineNumber++;
        finalOffset = lineText.length() + 1;
        char *lineCStr = new char[lineText.length() + 1];
        strcpy(lineCStr, lineText.c_str());
        strBegin = lineCStr;
        token = strtok(lineCStr, " \t");;
        return getNextToken();
    }
    offset = finalOffset;
    eof = true;
    return "";
}

bool isNumberWithinLimits(string& s) {
    int n = atoi(s.c_str());
    if (n < 0 || n >= (int) pow(2, 30)) {
        throw __parseerror(0);
    }
    return true;
}

bool isNumber(string& s) {
    for (char &ch : s) {
        if (isdigit(ch) == 0)
            return false;
    }
    return isNumberWithinLimits(s);
}

int readInt() {
    string nextToken = getNextToken();
    if (!nextToken.empty() && isNumber(nextToken)) {
        return atoi(nextToken.c_str());
    }
    throw __parseerror(0);
}

int getDefCount() {
    string nextToken = getNextToken();
    if (nextToken.empty()) {
        return 0;
    } else if (isNumber(nextToken)) {
        return atoi(nextToken.c_str());
    }
    throw __parseerror(0);
}

char readIEAR() {
    string nextToken = getNextToken();
    if (nextToken == "I" || nextToken == "E" || nextToken == "A" || nextToken == "R") {
        return nextToken[0];
    }
    throw __parseerror(2);
}

int getSymbolValue(string symbol) {
    for (int i = 0; i < symbolName.size(); i++) {
        if (symbolName[i] == symbol) {
            return symbolValue[i];
        }
    }
    return NOT_PRESENT;
}

void validateSymbol(string symbol) {
    if (!isalpha(symbol.at(0))) {
        throw __parseerror(1);
    }
    if (symbol.length() > MAX_SIZE) {
        throw __parseerror(3);
    }
}

void pass1_validate() {
    int baseInstr = 0;
    int defCount = getDefCount();
    while (! eof) {
        if (defCount > MAX_SIZE) {
            throw __parseerror(4);
        }
        vector<string> moduleSymbolName;
        vector<int> moduleSymbolValue;
        for (int i = 0; i < defCount; i++) {
            string symbol = getNextToken();
            if (symbol == "" || isNumber(symbol)) {
                throw __parseerror(1);
            }
            validateSymbol(symbol);
            int val = readInt();
            moduleSymbolName.push_back(symbol);
            moduleSymbolValue.push_back(val);
        }

        int useCount = readInt();
        if (useCount > MAX_SIZE) {
            throw __parseerror(5);
        }
        vector<string> moduleUse;
        for (int i = 0; i < useCount; i++) {
            string symbol = getNextToken();
            if (symbol == "" || isNumber(symbol)) {
                throw __parseerror(1);
            }
            validateSymbol(symbol);
            moduleUse.push_back(symbol);
        }
        moduleUseSymbols.push_back(moduleUse);

        int instCount = readInt();
        if (instCount > MAX_INS_SIZE || (instCount + baseInstr) > MAX_INS_SIZE) {
            throw __parseerror(6);
        }
        for (int i = 0; i < instCount; i++) {
            char addressMode = readIEAR();
            int operand = readInt();
            if (addressMode == 'E' && (operand % 1000) < moduleUse.size()) {
                symbolsUsed.insert(moduleUse[operand % 1000]);
            }
        }

        // Updating Symbol Table
        vector<string> moduleSymbolDefs;
        for (int i = 0; i < moduleSymbolName.size(); i++) {
            string symbol = moduleSymbolName[i];
            int val = moduleSymbolValue[i];
            int isSymbolPresent = getSymbolValue(symbol);
            if (isSymbolPresent == NOT_PRESENT) {
                symbolName.push_back(symbol);
                if (val >= instCount) {
                    printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n", moduleCount + 1, symbol.c_str(), val, instCount - 1);
                    val = 0;
                }
                symbolValue.push_back(baseInstr + val);
                moduleSymbolDefs.push_back(symbol);
            } else {
                printf("Warning: Module %d: %s redefined and ignored\n", moduleCount + 1, symbol.c_str());
                symbolWarnings[symbol] = "Error: This variable is multiple times defined; first value used";
            }
        }
        symbolsDefined.push_back(moduleSymbolDefs);
        baseInstr = baseInstr + instCount;
        moduleCount++;
        defCount = getDefCount();
    }
}

void printSymbolTable() {
    cout << "Symbol Table" << endl;
    for (int i = 0; i < symbolName.size(); i++) {
        cout << symbolName[i] << "=" << symbolValue[i] << " " << symbolWarnings[symbolName[i]] << endl;
    }
}

void pass1(string fileName) {
    lineNumber = 0; offset = 0; finalOffset = 0;
    MyReadFile.open(fileName);
    eof = false;
    pass1_validate();
    printSymbolTable();
    delete[] strBegin;
    delete[] token;
    MyReadFile.close();
}

string getModAddress(int instrCount, int limit) {
    string instr = to_string(instrCount);
    int len = instr.length();
    string prefix = "";
    for (int i = 0; i < limit - len; i++) {
        prefix.append("0");
    }
    return prefix.append(instr);
}

void pass2_evaluate() {
    int module = 0;
    int baseInstr = 0;
    int instrCount = 0;

    int defCount = getDefCount();
    while (! eof) {
        
        for (int i = 0; i < defCount; i++) {
            string symbol = getNextToken();
            int val = readInt();
        }

        int useCount = readInt();
        vector<string> moduleUse;
        for (int i = 0; i < useCount; i++) {
            string symbol = getNextToken();
        }

        set<int> symbolIndexUsed;
        int instCount = readInt();
        for (int i = 0; i < instCount; i++) {
            char addressMode = readIEAR();
            int operand = readInt();
            string errMessage = "";
            if (operand >= 10000) {
                operand = 9999;
                errMessage= " Error: Illegal opcode; treated as 9999";
            }
            if (errMessage != "" && addressMode != 'I') {
                cout << getModAddress(instrCount, 3) << ": " << getModAddress(operand, 4) << errMessage << endl;
            } else if (addressMode == 'R') {
                if (operand % 1000 >= instCount) {
                    errMessage = " Error: Relative address exceeds module size; zero used";
                    operand = ((int) operand / 1000) * 1000;
                }
                cout << getModAddress(instrCount, 3) << ": " << getModAddress(operand + baseInstr, 4) << errMessage << endl;
            } else if (addressMode == 'E') {
                int index = operand % 1000;
                if (index >= moduleUseSymbols[module].size()) {
                    errMessage = " Error: External address exceeds length of uselist; treated as immediate";
                    cout << getModAddress(instrCount, 3) << ": " << getModAddress(operand, 4) << errMessage << endl;
                } else {
                    string symbol = moduleUseSymbols[module][index];
                    symbolIndexUsed.insert(index);
                    int symbolValue = getSymbolValue(symbol);
                    if (symbolValue == NOT_PRESENT) {
                        symbolValue = 0;
                        errMessage = " Error: " + symbol + " is not defined; zero used";
                    }
                    cout << getModAddress(instrCount, 3) << ": " << getModAddress(((int) operand / 1000) * 1000 + symbolValue, 4) << errMessage << endl;
                }
            } else if (addressMode == 'I') {
                if (errMessage != "") {
                    errMessage = " Error: Illegal immediate value; treated as 9999";
                }
                cout << getModAddress(instrCount, 3) << ": " << getModAddress(operand, 4) << errMessage << endl;
            } else if (addressMode == 'A') {
                if (operand % 1000 >= 512) {
                    errMessage = " Error: Absolute address exceeds machine size; zero used";
                    operand = ((int) operand / 1000) * 1000;
                }
                cout << getModAddress(instrCount, 3) << ": " << getModAddress(operand, 4) << errMessage << endl;
            }
            instrCount++;
        }
        for (int i = 0; i < moduleUseSymbols[module].size(); i++) {
            if (symbolIndexUsed.find(i) == symbolIndexUsed.end()) {
                printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n", module + 1, moduleUseSymbols[module][i].c_str());
            }
        }
        for (int i = 0; i < symbolsDefined[module].size(); i++) {
            if (symbolsUsed.find(symbolsDefined[module][i]) == symbolsUsed.end()) {
                linkerWarnings.push_back("Warning: Module " + to_string(module + 1) + ": " +  symbolsDefined[module][i] + " was defined but never used\n");
            }
        }
        baseInstr = baseInstr + instCount;
        module++;
        defCount = getDefCount();
    }
    for (int i = 0; i < linkerWarnings.size(); i++) {
        if (i == 0) cout << endl;
        cout << linkerWarnings[i];
    }
}

void pass2(string fileName) {
    lineNumber = 0; offset = 0; finalOffset = 0;
    MyReadFile.open(fileName);
    eof = false;
    cout << endl << "Memory Map" << endl;
    pass2_evaluate();
    MyReadFile.close();
}

int main(int argc, char** argv) {
    string fileName = string(argv[1]);
    try {
        pass1(fileName);
        pass2(fileName);
    } catch (const string msg) {
        cout << msg.c_str() << endl;
    }
    return 0;
}
