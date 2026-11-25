#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
using namespace std;

const int HASH_SIZE = 101;   // prime table size for the hash table
const int MAX_V = 20;        // max sectors (vertices) in the airspace graph

// index = priorityLevel (1 = most urgent ... 9 = routine)
const char* const PRIO_NAME[10] = { "", "WAR", "HIJACK", "CYBER", "MISSING", "FUEL", "MEDICAL", "WEATHER", "MECHANICAL", "NORMAL" };

// own random generator (linear congruential), seeded through seedValue
static unsigned long seedValue = 7;
inline int rnd(int n) {
    if (n <= 0) return 0;
    seedValue = seedValue * 1103515245UL + 12345UL;
    return (int)((seedValue >> 16) % n);
}

// files use '_' instead of spaces ("Control_Tower"); turn them back into spaces
inline string fixName(string s) {
    for (int i = 0; i < (int)s.size(); i++)
        if (s[i] == '_')
            s[i] = ' ';
    return s;
}

// converts lowercase letters to uppercase
inline string upper(string s) {
    for (int i = 0; i < (int)s.size(); i++)
        if (s[i] >= 'a' && s[i] <= 'z')
            s[i] -= 32;
    return s;
}

// reads one word from the keyboard (exits cleanly if the input ends)
inline string readWord() {
    string w;
    if (!(cin >> w))
        exit(0);
    return w;
}

// reads a non-negative number; returns -1 if the user typed anything else
inline int readInt() {
    string w = readWord();
    if (w.size() > 6) return -1;

    for (int i = 0; i < (int)w.size(); i++)
        if (w[i] < '0' || w[i] > '9')
            return -1;

    return atoi(w.c_str());
}

// prints a text file, skipping the first 'skip' lines
inline bool printFile(const string& fn, int skip = 0) {
    ifstream in(fn.c_str());
    if (!in) { cout << "[ATC Error] Cannot open " << fn << endl; return false; }
    string line; int i = 0;
    while (getline(in, line)) { if (i++ >= skip) cout << line << endl; }
    return true;
}

// escalation window (in ticks) for each priority
inline int windowFor(int p) {
    if (p == 1) return 2;                  // war
    if (p == 2) return 3;                  // hijack
    if (p == 5 || p == 8) return 5;        // fuel, mechanical
    if (p == 6 || p == 7) return 4;        // medical, weather
    return 8;                              // normal
}

struct Flight {
    string callsign, type, airline, origin, dest, threat, sector, runway;
    int pax, fuel, arrivalTick, priority, window, wait;   // wait = ticks left on the current corridor
    bool cleared, handled;                                // cleared for landing / emergency handled
    int route[MAX_V]; int routeLen, routePos;             // planned route (sector indices), routePos = current position
    Flight* next;                                         // next node of the linked flight list
    Flight() {
        pax = fuel = arrivalTick = wait = 0; priority = 9; window = windowFor(9);
        cleared = handled = false; routeLen = routePos = 0; next = nullptr;
    }
};

struct Crew { string id, name, role, callsign; Crew* next; };
