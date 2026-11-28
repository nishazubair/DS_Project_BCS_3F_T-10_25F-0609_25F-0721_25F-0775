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
    Flight* table[HASH_SIZE];          // hash table slots (pointers to list nodes)
    int state[HASH_SIZE];              // slot state: 0 empty, 1 used, 2 tombstone (deleted)
    int collisions, longest;           // statistics for displayStats

    // polynomial string hash reduced modulo the (prime) table size
    int hashFn(const string& s) {
        int h = 0;
        for (int i = 0; i < (int)s.size(); i++)
            h = (h * 31 + (unsigned char)s[i]) % HASH_SIZE;
        return h;
    }

    // probe linearly until the callsign is found or an EMPTY slot is hit
    // tombstones are skipped so earlier collisions stay reachable
    int slot(const string& cs) {
        int i = hashFn(cs), p = 0;
        while (state[i] != 0 && p < HASH_SIZE) {
            if (state[i] == 1 && table[i]->callsign == cs) return i;
            i = (i + 1) % HASH_SIZE;
            p++;
        }
        return -1;
    }
public:
    Flight* head, * tail; int count;    // list of active flights

    Registration() {
        head = tail = nullptr;
        chead = ctail = nullptr;
        count = collisions = longest = 0;
        for (int i = 0; i < HASH_SIZE; i++) { table[i] = nullptr; state[i] = 0; }
    }

    ~Registration() {
        while (head) { Flight* n = head->next; delete head; head = n; }
        while (chead) { Crew* n = chead->next; delete chead; chead = n; }
    }

    // O(1) average lookup through the hash table
    Flight* find(const string& cs) {
        int i = slot(cs);
        return i < 0 ? nullptr : table[i];
    }

    // append at the tail of the list and insert into the hash table
    // false for a duplicate callsign or a nearly full table
    bool addFlight(Flight* f) {
        if (count >= HASH_SIZE - 5 || find(f->callsign)) return false;
        f->next = nullptr;
        if (tail) tail->next = f; else head = f;
        tail = f;
        count++;

        int i = hashFn(f->callsign), p = 0;
        while (state[i] == 1) {         // linear probing: try the next slot
            i = (i + 1) % HASH_SIZE;
            p++;
            collisions++;
        }
        table[i] = f;
        state[i] = 1;
        if (p > longest) longest = p;
        return true;
    }

    // leave a tombstone in the hash table, unlink the node and free it
    bool removeFlight(const string& cs) {
        int i = slot(cs);
        if (i < 0) return false;
        Flight* f = table[i];
        state[i] = 2;

        Flight* p = nullptr, * c = head;
        while (c != f) { p = c; c = c->next; }
        if (p) p->next = f->next; else head = f->next;
        if (tail == f) tail = p;
        delete f;
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

    void displayStats() {
        cout << "Hash table: load factor " << (double)count / HASH_SIZE
             << ", collisions " << collisions << ", longest probe " << longest << endl;
    }
};

class EmergencyLanding {
    Queue<Flight*> q;      // routine flights (priority 9), first come first served
    Heap<Flight*> hp;      // emergencies (priority 1..8), most urgent first
public:
    void enqueueNormal(Flight* f) { q.push(f); }
    Flight* dequeueNormal() { return q.pop(); }
    Flight* frontNormal() { return q.front(); }
    bool isEmptyNormal() { return q.empty(); }

    // heap key combines both fields: key = priority * 1000 + arrivalTick
    // a smaller priority number wins, and on a tie the earlier arrival wins
    void insertEmergency(Flight* f) { hp.push(f->priority * 1000L + f->arrivalTick, f); }
    Flight* extractMin() { return hp.pop(); }
    Flight* peekMin() { return hp.top(); }
    bool isEmptyHeap() { return hp.empty(); }

    // a flight leaves both structures (landed, crashed, cleared or re-prioritised)
    void remove(Flight* f) {
        q.remove(f);
        hp.remove(f);
    }

    void displayNormalQueue() {
        cout << "Normal landing queue (" << q.size() << "):" << endl;
        for (int i = 0; i < q.size(); i++)
            cout << "  " << i + 1 << ". " << q.at(i)->callsign << " -> " << q.at(i)->runway << endl;
    }

    void displayHeap() {
        cout << "Emergency heap (" << hp.size() << "), index 0 is dispatched next:" << endl;
        for (int i = 0; i < hp.size(); i++)
            cout << "  [" << i << "] " << hp.at(i)->callsign << " " << PRIO_NAME[hp.at(i)->priority]
                 << " (arrived tick " << hp.at(i)->arrivalTick << ")" << endl;
    }
};

//  MODULE 4 : BSTflight (Binary Search Tree for the flight manifest)
//  What it does: keeps all active flights sorted by callsign (BB-211,
//  BB-437, ...) so we can search, insert and delete a flight quickly.
//  Rule of the tree: smaller callsign goes LEFT, bigger goes RIGHT.
//  Note: the tree only stores a pointer to the Flight, it does not own
//  the Flight itself (Registration owns it).

// One box in the tree.
// f = the flight stored here, l = left child, r = right child,
// h = height of this node (a leaf has height 1)
struct TNode { Flight* f; TNode* l, * r; int h; };

class BSTflight {
protected:   // protected so the AVL class (Module 5) can use these later
    TNode* root;   // top of the tree (nullptr when the tree is empty)

    // Height of a node. An empty node (nullptr) has height 0.
    static int ht(TNode* n) { return n ? n->h : 0; }

    // Recalculate a node's height = 1 + the taller of its two children.
    static void upd(TNode* n) {
        int a = ht(n->l);
        int b = ht(n->r);
        n->h = 1 + (a > b ? a : b);
    }

    // Called after every insert/delete on the way back up.
    // Plain BST: only refresh the height.
    // "virtual" lets AVL replace this with a version that also balances.
    virtual TNode* fix(TNode* n) { 
        upd(n);
        return n;
    }      // BST: only refresh the height

    // Insert a flight (recursive). Returns the new root of this subtree.
    TNode* ins(TNode* n, Flight* f) {
        // Empty spot found: create the new node here (height 1)
        if (!n)
            return new TNode{ f, nullptr, nullptr, 1 };
        // Smaller callsign goes left, otherwise right
        if (f->callsign < n->f->callsign)
            n->l = ins(n->l, f);
        else
            n->r = ins(n->r, f);

        return fix(n);   // update height while coming back up
    }

    // Delete the flight with callsign 'cs' (recursive).
    // Returns the new root of this subtree.
    // Delete the flight with callsign 'cs' (recursive).
// Returns the new root of this subtree.
    TNode* del(TNode* n, const string& cs) {

        // CASE 0: tree/subtree is empty -> callsign not found, nothing to delete
        if (!n)
            return nullptr;

        // Not this node yet: keep searching
        if (cs < n->f->callsign) {
            n->l = del(n->l, cs);          // search in the left side
        }
        else if (cs > n->f->callsign) {
            n->r = del(n->r, cs);          // search in the right side
        }
        else {
            // ---- Found the node to delete ----

            // CASE 1: leaf node (no children)
            // Just delete it. Its parent will now point to nullptr.
            if (!n->l && !n->r) {
                delete n;
                return nullptr;
            }

            // CASE 2: only a RIGHT child (left is empty)
            // Delete the node and put its right child in its place.
            else if (!n->l) {
                TNode* c = n->r;
                delete n;
                return c;
            }

            // CASE 3: only a LEFT child (right is empty)
            // Delete the node and put its left child in its place.
            else if (!n->r) {
                TNode* c = n->l;
                delete n;
                return c;
            }

            // CASE 4: two children
            // Find the in-order successor = smallest node in the right subtree.
            // Copy its flight into this node, then delete the successor
            // from the right subtree (it has at most one child, so it falls
            // into case 1 or 2 on the recursive call).
            else {
                TNode* m = n->r;
                while (m->l)
                    m = m->l;
                n->f = m->f;
                n->r = del(n->r, m->f->callsign);
            }
        }

        return fix(n);   // update height (and balance in AVL) while coming back up
    }

    // In-order traversal (left, node, right) = prints flights in
    // sorted callsign order.
    void inorder(TNode* n) {
        if (!n) return;
        inorder(n->l);
        cout << "  " << n->f->callsign << " (" << n->f->type << ", " << n->f->sector << ")" << endl;
        inorder(n->r);
    }

    // Delete every node (post-order: children first, then the node).
    // This frees the tree nodes only, not the Flight objects.
    void destroy(TNode* n) { if (!n) return; destroy(n->l); destroy(n->r); delete n; }

public:
    BSTflight() { root = nullptr; }               // start with an empty tree
    virtual ~BSTflight() { destroy(root); }       // free all nodes when the object dies

    // Public functions: these just start the recursion from the root.
    void insert(Flight* f) { root = ins(root, f); }
    void remove(const string& cs) { root = del(root, cs); }

    // Search by callsign. Goes left or right at each step (no recursion).
    // Returns the Flight, or nullptr if it is not in the tree.
   // Search for a flight by callsign (no recursion, just a loop).
// Returns the Flight if found, or nullptr if it is not in the tree.
    Flight* search(const string& cs) {
        TNode* c = root;                  // start at the top of the tree

        while (c) {                       // keep going until we fall off the tree
            // Found it: this node's callsign matches
            if (cs == c->f->callsign) {
                return c->f;
            }
            // Our callsign is smaller: the flight can only be on the left side
            else if (cs < c->f->callsign) {
                c = c->l;
            }
            // Our callsign is bigger: the flight can only be on the right side
            else {
                c = c->r;
            }
        }

        return nullptr;                   // reached an empty spot: not in the tree
    }

    void inorderTraversal() { inorder(root); }      // prints the manifest sorted by callsign
    int height() { return ht(root); }               // height of the whole tree (used in BST vs AVL test)
    void destroyTree() { destroy(root); root = nullptr; }   // empty the tree but keep using it
};

//  MODULE 5 : AVLflight (self-balancing version of the BST)
//  What it does: works exactly like BSTflight, but after every insert
//  or delete it checks if the tree has become lopsided and fixes it
//  with rotations. This keeps the height small (about log n).
//  It inherits everything from BSTflight and only replaces fix().
//  Balance factor of a node = height(left) - height(right)
//  AVL rule: the balance factor must always be -1, 0 or +1.

class AVLflight : public BSTflight {

    // ---- Single rotation to the RIGHT (used when the LEFT side is too tall) ----
    //        y                x
    //       / \              / \
    //      x   C    -->     A   y
    //     / \                  / \
    //    A   B                B   C
    TNode* rotateRight(TNode* y) {
        TNode* x = y->l;      // x is y's left child, it will become the new top
        y->l = x->r;          // x's right subtree (B) moves to y's left
        x->r = y;             // y becomes x's right child
        upd(y);               // y is lower now, so update its height FIRST
        upd(x);               // then update x (the new top)
        return x;             // x is the new root of this subtree
    }

    // ---- Single rotation to the LEFT (used when the RIGHT side is too tall) ----
    //      x                    y
    //     / \                  / \
    //    A   y      -->       x   C
    //       / \              / \
    //      B   C            A   B
    TNode* rotateLeft(TNode* x) {
        TNode* y = x->r;      // y is x's right child, it will become the new top
        x->r = y->l;          // y's left subtree (B) moves to x's right
        y->l = x;             // x becomes y's left child
        upd(x);               // x is lower now, so update its height FIRST
        upd(y);               // then update y (the new top)
        return y;             // y is the new root of this subtree
    }

    // ---- Double rotation: Left-Right (LR case) ----
    // The left child is heavy on ITS RIGHT side, so a single right
    // rotation is not enough. First rotate the left child to the left,
    // then rotate the node itself to the right.
    TNode* rotateLeftRight(TNode* n) {
        n->l = rotateLeft(n->l);   // step 1: fix the left child
        return rotateRight(n);     // step 2: rotate the node
    }

    // ---- Double rotation: Right-Left (RL case) ----
    // The right child is heavy on ITS LEFT side. First rotate the right
    // child to the right, then rotate the node itself to the left.
    TNode* rotateRightLeft(TNode* n) {
        n->r = rotateRight(n->r);  // step 1: fix the right child
        return rotateLeft(n);      // step 2: rotate the node
    }

protected:
    // Replaces BSTflight::fix(). Called on the way back up after every
    // insert and delete, so every node on the path gets checked.
    TNode* fix(TNode* n) override {
        upd(n);                            // first refresh this node's height

        int b = ht(n->l) - ht(n->r);       // balance factor

        // ---- LEFT side too tall (b > 1) ----
        if (b > 1) {
            // LR case: the left child leans to the right -> double rotation
            if (ht(n->l->l) < ht(n->l->r)) {
                n = rotateLeftRight(n);
            }
            // LL case: the left child leans left (or is even) -> single rotation
            else {
                n = rotateRight(n);
            }
        }
        // ---- RIGHT side too tall (b < -1) ----
        else if (b < -1) {
            // RL case: the right child leans to the left -> double rotation
            if (ht(n->r->r) < ht(n->r->l)) {
                n = rotateRightLeft(n);
            }
            // RR case: the right child leans right (or is even) -> single rotation
            else {
                n = rotateLeft(n);
            }
        }

        return n;   // return the (possibly new) top of this subtree
    }
};

// BST vs AVL height test: 15 callsigns inserted in sorted order
int main() {
    BSTflight tb;        // plain BST
    AVLflight ta;        // self-balancing AVL
    Flight d[15];        // 15 dummy flights (the trees only store pointers to these)

    for (int i = 0; i < 15; i++) {
        // Zero-padded names: AA-01 ... AA-15
        // (padding matters: without it "AA-10" would sort before "AA-2" as text)
        d[i].callsign = string("AA-") + (i + 1 < 10 ? "0" : "") + to_string(i + 1);
        tb.insert(&d[i]);
        ta.insert(&d[i]);
    }

    cout << "BST height = " << tb.height() << endl;
    cout << "AVL height = " << ta.height() << endl;
    return 0;
}
