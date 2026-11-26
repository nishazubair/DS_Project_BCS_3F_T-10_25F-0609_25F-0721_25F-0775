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

// singly linked queue: head = front, tail = back
template <class T> class Queue {
    struct N { T v; N* next; };
    N* h, * t; int cnt;
public:
    Queue() {
        h = t = nullptr;
        cnt = 0;
    }

    ~Queue() { while (h) pop(); }

    void push(T v) {
        N* n = new N{ v, nullptr };
        if (t)
            t->next = n;
        else
            h = n;
        t = n;
        cnt++;
    }

    T pop() {
        N* n = h;
        T v = n->v;
        h = n->next;
        if (!h)
            t = nullptr;
        delete n;
        cnt--;
        return v;
    }

    T front() { return h->v; }
    bool empty() { return h == nullptr; }
    int size() { return cnt; }
    T at(int i) {      // i-th element (for display)
        N* c = h;
        while (i-- > 0)
            c = c->next;
        return c->v;
    }

    void remove(T v) {      // remove the first matching value
        N* p = nullptr, * c = h;
        while (c && !(c->v == v)) { p = c; c = c->next; }
        if (!c) return;
        if (p) p->next = c->next; else h = c->next;
        if (c == t) t = p;
        delete c; cnt--;
    }
};

// singly linked stack: top = newest
template <class T> class Stack {
    struct N { T v; N* next; };
    N* top;
public:
    Stack() { top = nullptr; }
    ~Stack() { clear(); }
    void push(T v) { top = new N{ v, top }; }
    T pop() { N* n = top; T v = n->v; top = n->next; delete n; return v; }
    bool empty() { return top == nullptr; }
    void clear() { while (top) pop(); }
};

// min-heap in a dynamic array: the smallest key is always at index 0
// a[] holds the values, k[] the keys; both arrays are swapped together
template <class T> class Heap {
    T* a; long* k; int n, cap;
    void swp(int i, int j) { T t = a[i]; a[i] = a[j]; a[j] = t; long q = k[i]; k[i] = k[j]; k[j] = q; }
    void up(int i) { while (i > 0 && k[(i - 1) / 2] > k[i]) { swp(i, (i - 1) / 2); i = (i - 1) / 2; } }
    void down(int i) {
        while (true) {
            int l = 2 * i + 1, r = l + 1, m = i;
            if (l < n && k[l] < k[m]) m = l;
            if (r < n && k[r] < k[m]) m = r;
            if (m == i) break;
            swp(i, m); i = m;
        }
    }
public:
    Heap() { cap = 64; n = 0; a = new T[cap]; k = new long[cap]; }
    ~Heap() { delete[] a; delete[] k; }
    void push(long key, T v) {
        if (n == cap) {     // array full: double its size
            cap *= 2; T* na = new T[cap]; long* nk = new long[cap];
            for (int i = 0; i < n; i++) { na[i] = a[i]; nk[i] = k[i]; }
            delete[] a; delete[] k; a = na; k = nk;
        }
        a[n] = v; k[n] = key; up(n); n++;
    }
    T pop() { T r = a[0]; n--; a[0] = a[n]; k[0] = k[n]; down(0); return r; }
    T top() { return a[0]; }
    bool empty() { return n == 0; }
    int size() { return n; }
    T at(int i) { return a[i]; }
    void remove(T v) {      // remove an arbitrary value, then repair the heap
        for (int i = 0; i < n; i++) if (a[i] == v) {
            n--;
            if (i != n) { a[i] = a[n]; k[i] = k[n]; up(i); down(i); }
            return;
        }
    }
};

class Registration {
    Crew* chead, * ctail;               // crew list (singly linked)
public:
    Flight* head, * tail; int count;    // list of active flights

    Registration() {
        head = tail = nullptr;
        chead = ctail = nullptr;
        count = 0;
    }

    ~Registration() {
        while (head) { Flight* n = head->next; delete head; head = n; }
        while (chead) { Crew* n = chead->next; delete chead; chead = n; }
    }

    // walks the whole list, so lookup is O(n)
    Flight* find(const string& cs) {
        for (Flight* f = head; f; f = f->next)
            if (f->callsign == cs)
                return f;
        return nullptr;
    }

    // append at the tail; false if the callsign already exists
    bool addFlight(Flight* f) {
        if (find(f->callsign)) return false;
        f->next = nullptr;
        if (tail) tail->next = f; else head = f;
        tail = f;
        count++;
        return true;
    }

    // unlink the node from the list and free it
    bool removeFlight(const string& cs) {
        Flight* p = nullptr, * c = head;
        while (c && c->callsign != cs) { p = c; c = c->next; }
        if (!c) return false;
        if (p) p->next = c->next; else head = c->next;
        if (tail == c) tail = p;
        delete c;
        count--;
        return true;
    }

    // every active flight burns 1 unit of fuel per tick
    void updateAllFlights() { for (Flight* f = head; f; f = f->next) f->fuel--; }

    void printFlight(Flight* f) {
        cout << f->callsign << " | " << f->type << " | " << f->airline << " | " << f->origin << "->" << f->dest << " | pax " << f->pax
            << " | fuel " << f->fuel << " | " << f->sector << " -> " << f->runway << " | " << PRIO_NAME[f->priority] << endl;
    }

    void displayAllFlights() {
        cout << "--- Flight Records (" << count << ") ---" << endl;
        for (Flight* f = head; f; f = f->next) printFlight(f);
    }

    // crew records are appended at the tail of their own list
    void addCrew(Crew* c) {
        c->next = nullptr;
        if (ctail) ctail->next = c; else chead = c;
        ctail = c;
    }

    void displayAllCrew() {
        cout << "--- Crew Members ---" << endl;
        for (Crew* c = chead; c; c = c->next)
            cout << c->id << " | " << c->name << " | " << c->role << " | " << c->callsign << endl;
    }

    // name of the pilot assigned to a callsign
    string pilotOf(const string& cs) {
        for (Crew* c = chead; c; c = c->next)
            if (c->callsign == cs && c->role == "Pilot")
                return c->name;
        return "unknown";
    }
};
