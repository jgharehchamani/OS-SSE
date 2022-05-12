#ifndef SCHEME1_H
#define SCHEME1_H
#include <string>
#include <map>
#include <vector>
#include <array>
#include <iostream>
#include <sstream>
#include <mutex>
#include "Server.h"
#include "OMAP.h"
#include "Utilities.h"

using namespace std;

class Client {
public:
    Server* server;
    OMAP* Odel;
    OMAP* Oparents;
    int N;
    int height;
    unsigned char IKey[16], DKey[16], secretKey[16];
    map<Bid, string> Mcnt;
    map<Bid, string> MnextParents;
    map<Bid, string> setupOdel;
    map<Bid, string> setupParents;
    bool localStorage = false;
    int totalUpdateCommSize;
    int totalSearchCommSize;
    int keywordSize, firstLeaf;


public:
    Client();
    virtual ~Client();
    void reset();
    void insert(string keyword, int id, bool setup);
    void remove(string keyword, int id, bool setup);
    vector<int> search(string keyword);
    Client(Server* server, int maxOMAPSize, int keywordSize);
    void endSetup();
    int expected;

};

#endif /* SCHEME1_H */

