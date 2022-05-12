#include "Client.h"

Client::Client() {
}

Client::~Client() {
}

Client::Client(Server* server, int N, int keywordSize) {
    this->server = server;
    this->keywordSize = keywordSize;
    bytes<Key> key{0};
    for (int i = 0; i < 16; i++) {
        IKey[i] = 1;
        DKey[i] = 2;
        secretKey[i] = 5;
    }
    this->Odel = new OMAP(N, key);
    this->Oparents = new OMAP(N * 2, key);
    this->N = N;
    this->height = (int) ceil(log2(N)) + 1; //root is level 1
    this->firstLeaf = (pow(2, height - 1) - 1);
}

void Client::insert(string keyword, int id, bool setup) {
    Odel->treeHandler->oram->totalRead = 0;
    Odel->treeHandler->oram->totalWrite = 0;
    Oparents->treeHandler->oram->totalRead = 0;
    Oparents->treeHandler->oram->totalWrite = 0;
    totalUpdateCommSize = 0;

    int src_cnt = 0, a_w = -1;
    Bid key = Utilities::getBid(keyword);
    string result = Mcnt[key];
    if (result != "") {
        vector<string> parts = Utilities::splitData(result, "#");
        src_cnt = stoi(parts[0]);
        a_w = stoi(parts[1]);
    }

    a_w++;
    Mcnt[key] = to_string(src_cnt) + "#" + to_string(a_w);


    int nodeIndex = Utilities::getNodeOnPath(a_w, height, height);

    //Compute Parent
    int parentIndex;
    if (MnextParents.count(key) == 0) {
        parentIndex = Utilities::getNodeOnPath(0, height - 1, height);
        int nextParent = ((nodeIndex + 1) + 1) / 2 - 1;
        MnextParents[key] = to_string(nextParent);
    } else {
        parentIndex = stoi(MnextParents[key]);
        int nextParent = ((nodeIndex + 1) + 1) / 2 - 1;
        MnextParents[key] = to_string(nextParent);
    }



    //Update Odel
    //----------------------------------------------------------
    Bid newKey = Utilities::getBid(keyword + "#" + to_string(id));
    if (setup) {
        setupOdel[newKey] = to_string(nodeIndex);
    } else {
        Odel->insert(newKey, to_string(nodeIndex));
    }
    //----------------------------------------------------------



    //Update I
    //----------------------------------------------------------
    prf_type mapKey, mapValue;
    prf_type k_w;
    int acc = 0;

    memset(k_w.data(), 0, AES_KEY_SIZE);
    copy(keyword.begin(), keyword.end(), k_w.data());
    *(int*) (&((unsigned char*) k_w.data())[AES_KEY_SIZE - 5]) = src_cnt;
    prf_type phi = Utilities::generatePRF(k_w.data(), secretKey);
    prf_type tk_I = Utilities::bitwiseXOR(IKey, phi);

    memset(mapKey.data(), 0, AES_KEY_SIZE);
    *(int*) (&(mapKey.data()[AES_KEY_SIZE - 5])) = nodeIndex;
    *(int*) (&(mapKey.data()[AES_KEY_SIZE - 9])) = acc;
    mapKey = Utilities::generatePRF(mapKey.data(), tk_I.data());

    prf_type plaintext;
    memset(plaintext.data(), 0, AES_KEY_SIZE);
    *(int*) (&(plaintext.data()[AES_KEY_SIZE - 5])) = id;
    *(int*) (&(plaintext.data()[AES_KEY_SIZE - 9])) = src_cnt;
    mapValue = Utilities::encode(plaintext.data(), secretKey);

    server->insert(mapKey, mapValue);
    //----------------------------------------------------------


    //Insert parent info in Oparent
    //----------------------------------------------------------
    Bid Gparent;
    memset(Gparent.id.data(), 0, ID_SIZE);
    copy(keyword.begin(), keyword.end(), Gparent.id.data());
    *(int*) (&Gparent.id[AES_KEY_SIZE - 4]) = nodeIndex;

    if (setup) {
        setupParents[Gparent] = "-#-#" + to_string(parentIndex) + "#0";
    } else {
        Oparents->insert(Gparent, "-#-#" + to_string(parentIndex) + "#0");
    }

    totalUpdateCommSize = sizeof (prf_type) /*key*/ + sizeof (prf_type) /*value*/+
            (Odel->treeHandler->oram->totalRead + Odel->treeHandler->oram->totalWrite) * (sizeof (int) /*size of bucket index*/ +Odel->treeHandler->oram->clen_size /*size of each bucket*/)+
            (Oparents->treeHandler->oram->totalRead + Oparents->treeHandler->oram->totalWrite) * (sizeof (int) /*size of bucket index*/ +Oparents->treeHandler->oram->clen_size /*size of each bucket*/);
}

void Client::remove(string keyword, int id, bool setup) {
    Odel->treeHandler->oram->totalRead = 0;
    Odel->treeHandler->oram->totalWrite = 0;
    Oparents->treeHandler->oram->totalRead = 0;
    Oparents->treeHandler->oram->totalWrite = 0;
    totalUpdateCommSize = 0;

    Bid key = Utilities::getBid(keyword);
    string result = Mcnt[key];
    vector<string> parts = Utilities::splitData(result, "#");
    int src_cnt = stoi(parts[0]);
    int a_w = stoi(parts[1]);

    prf_type k_w;
    memset(k_w.data(), 0, AES_KEY_SIZE);
    copy(keyword.begin(), keyword.end(), k_w.data());
    *(int*) (&((unsigned char*) k_w.data())[AES_KEY_SIZE - 5]) = src_cnt;
    prf_type phi = Utilities::generatePRF(k_w.data(), secretKey);
    prf_type tk_I = Utilities::bitwiseXOR(IKey, phi);
    prf_type tk_D = Utilities::bitwiseXOR(DKey, phi);

    bool parentIsLeftChild = false, nodeIsLeftChild = false;

    //Finding index and parent of target delete id
    //--------------------------------------------------
    string posStr;
    Bid newKey = Utilities::getBid(keyword + "#" + to_string(id));
    posStr = setup ? setupOdel[newKey] : Odel->find(newKey);
    int index = stoi(posStr);
    //--------------------------------------------------

    int sibling = -1;
    int GparentIndex = -1;
    int GGparentIndex = -1;
    int parentAcc, GparentAcc;
    int uncle = 0;

    Bid nodeInfo;
    memset(nodeInfo.id.data(), 0, ID_SIZE);
    copy(keyword.begin(), keyword.end(), nodeInfo.id.data());
    *(int*) (&nodeInfo.id[AES_KEY_SIZE - 4]) = index;
    string nodeStr = setup ? setupParents[nodeInfo] : Oparents->find(nodeInfo);
    parts = Utilities::splitData(nodeStr, "#");
    int parentIndex = stoi(parts[2]);


    Bid parentInfo;
    memset(parentInfo.id.data(), 0, ID_SIZE);
    copy(keyword.begin(), keyword.end(), parentInfo.id.data());
    *(int*) (&parentInfo.id[AES_KEY_SIZE - 4]) = parentIndex;
    string parentStr = setup ? setupParents[parentInfo] : Oparents->find(parentInfo);



    if (parentStr == "") {
        if (index % 2 == 1) {
            nodeIsLeftChild = true;
            sibling = index + 1;
        } else {
            nodeIsLeftChild = false;
            sibling = index - 1;
        }
        GparentIndex = (parentIndex + 1) / 2 - 1;
        parentAcc = -1;
    } else {
        parts = Utilities::splitData(parentStr, "#");
        if (stoi(parts[0]) == index) {
            nodeIsLeftChild = true;
            sibling = stoi(parts[1]);
        } else if (stoi(parts[1]) == index) {
            nodeIsLeftChild = false;
            sibling = stoi(parts[0]);
        } else {
            printf("index not found in parent");
            exit(0);
        }
        GparentIndex = stoi(parts[2]);
        if (parts[3] == "-") {
            parentAcc = -1;
        } else {
            parentAcc = stoi(parts[3]);
        }
    }

    int leftChild, rightChild;

    memset(parentInfo.id.data(), 0, ID_SIZE);
    copy(keyword.begin(), keyword.end(), parentInfo.id.data());
    *(int*) (&parentInfo.id[AES_KEY_SIZE - 4]) = GparentIndex;
    if (setup) {
        string GparentsStr = setupParents[parentInfo];
        if (GparentsStr == "") {
            if (parentIndex % 2 == 1) {
                parentIsLeftChild = true;
                uncle = parentIndex + 1;
            } else {
                parentIsLeftChild = false;
                uncle = parentIndex - 1;
            }
            parentIsLeftChild = parentIndex % 2 == 1 ? true : false;
            GparentAcc = -1;
            GGparentIndex = (GparentIndex + 1) / 2 - 1;
        } else {
            parts = Utilities::splitData(GparentsStr, "#");
            if (stoi(parts[0]) == parentIndex) {
                parentIsLeftChild = true;
                uncle = stoi(parts[1]);
            } else if (stoi(parts[1]) == parentIndex) {
                parentIsLeftChild = false;
                uncle = stoi(parts[0]);
            } else {
                printf("parent index not found in parent");
                exit(0);
            }
            GGparentIndex = stoi(parts[2]);
            GparentAcc = stoi(parts[3]);
        }

        if (parentIsLeftChild) {
            leftChild = sibling;
            rightChild = uncle;
        } else {
            rightChild = sibling;
            leftChild = uncle;
        }

        GparentAcc++;
        string res = to_string(leftChild) + "#" + to_string(rightChild) + "#" + to_string(GGparentIndex) + "#" + to_string(GparentAcc);
        setupParents[parentInfo] = res;
    } else {
        Oparents->getSet(parentInfo, [&](string GparentsStr) {
            if (GparentsStr == "") {
                if (parentIndex % 2 == 1) {
                    parentIsLeftChild = true;
                    uncle = parentIndex + 1;
                } else {
                    parentIsLeftChild = false;
                    uncle = parentIndex - 1;
                }
                parentIsLeftChild = parentIndex % 2 == 1 ? true : false;
                GparentAcc = -1;
                GGparentIndex = (GparentIndex + 1) / 2 - 1;
            } else {
                parts = Utilities::splitData(GparentsStr, "#");
                if (stoi(parts[0]) == parentIndex) {
                    parentIsLeftChild = true;
                    uncle = stoi(parts[1]);
                } else if (stoi(parts[1]) == parentIndex) {
                    parentIsLeftChild = false;
                    uncle = stoi(parts[0]);
                } else {
                    printf("parent index not found in parent");
                    exit(0);
                }
                GGparentIndex = stoi(parts[2]);
                GparentAcc = stoi(parts[3]);
            }

            if (parentIsLeftChild) {
                leftChild = sibling;
                rightChild = uncle;
            } else {
                rightChild = sibling;
                leftChild = uncle;
            }

            GparentAcc++;
            string res = to_string(leftChild) + "#" + to_string(rightChild) + "#" + to_string(GGparentIndex) + "#" + to_string(GparentAcc);
            return res;
        });
    }


    Bid siblingInfo;
    memset(siblingInfo.id.data(), 0, ID_SIZE);
    copy(keyword.begin(), keyword.end(), siblingInfo.id.data());
    *(int*) (&siblingInfo.id[AES_KEY_SIZE - 4]) = sibling;
    if (setup) {
        string siblingStr = setupParents[siblingInfo];
        string newSiblingStr = "";
        if (siblingStr == "") {
            if (sibling >= N - 1) {
                newSiblingStr = "-#-#" + to_string(GparentIndex) + "#-1";
            } else {
                newSiblingStr = to_string(sibling * 2 + 1) + "#" + to_string(sibling * 2 + 2) + "#" + to_string(GparentIndex) + "#-1";
            }
        } else {
            auto parts = Utilities::splitData(siblingStr, "#");
            newSiblingStr = parts[0] + "#" + parts[1] + "#" + to_string(GparentIndex) + "#" + parts[3];
        }
        setupParents[siblingInfo] = newSiblingStr;
    } else {
        Oparents->getSet(siblingInfo, [&](string siblingStr) {
            string newSiblingStr = "";
            if (siblingStr == "") {
                if (sibling >= N - 1) {
                    newSiblingStr = "-#-#" + to_string(GparentIndex) + "#-1";
                } else {
                    newSiblingStr = to_string(sibling * 2 + 1) + "#" + to_string(sibling * 2 + 2) + "#" + to_string(GparentIndex) + "#-1";
                }
            } else {
                auto parts = Utilities::splitData(siblingStr, "#");
                        newSiblingStr = parts[0] + "#" + parts[1] + "#" + to_string(GparentIndex) + "#" + parts[3];
            }
            return newSiblingStr;
        });
    }


    prf_type mapKey, mapValue;
    memset(mapKey.data(), 0, AES_KEY_SIZE);
    *(int*) (&(mapKey.data()[AES_KEY_SIZE - 5])) = GparentIndex;
    *(int*) (&(mapKey.data()[AES_KEY_SIZE - 9])) = GparentAcc;
    mapKey = Utilities::generatePRF(mapKey.data(), tk_I.data());

    memset(mapValue.data(), 0, AES_KEY_SIZE);
    *(int*) (&(mapValue.data()[AES_KEY_SIZE - 5])) = leftChild;
    *(int*) (&(mapValue.data()[AES_KEY_SIZE - 9])) = rightChild;
    mapValue = Utilities::encode(mapValue.data(), tk_D.data());


    server->insert(mapKey, mapValue);

    if (MnextParents[key] == to_string(parentIndex)) {
        MnextParents[key] = to_string(GparentIndex);
    }

    totalUpdateCommSize = sizeof (prf_type) /*key*/ + sizeof (prf_type) /*value*/+
            (Odel->treeHandler->oram->totalRead + Odel->treeHandler->oram->totalWrite) * (sizeof (int) /*size of bucket index*/ +Odel->treeHandler->oram->clen_size /*size of each bucket*/)+
            (Oparents->treeHandler->oram->totalRead + Oparents->treeHandler->oram->totalWrite) * (sizeof (int) /*size of bucket index*/ +Oparents->treeHandler->oram->clen_size /*size of each bucket*/);

}

vector<int> Client::search(string keyword) {
    totalSearchCommSize = 0;

    Bid key = Utilities::getBid(keyword);
    string result = Mcnt[key];
    if (result == "") {
        return vector<int>();
    }

    vector<string> parts = Utilities::splitData(result, "#");
    int src_cnt = stoi(parts[0]);
    int a_w = stoi(parts[1]);
    Mcnt[key] = to_string(src_cnt + 1) + "#" + to_string(a_w);

    prf_type k_w, k_w_0;

    memset(k_w.data(), 0, AES_KEY_SIZE);
    memset(k_w_0.data(), 0, AES_KEY_SIZE);
    copy(keyword.begin(), keyword.end(), k_w.data());
    copy(keyword.begin(), keyword.end(), k_w_0.data());
    *(int*) (&((unsigned char*) k_w.data())[AES_KEY_SIZE - 5]) = src_cnt;
    prf_type phi = Utilities::generatePRF(k_w.data(), secretKey);
    prf_type tk_0 = Utilities::generatePRF(k_w_0.data(), secretKey);
    prf_type tk_I = Utilities::bitwiseXOR(IKey, phi);
    prf_type tk_D = Utilities::bitwiseXOR(DKey, phi);

    totalSearchCommSize = sizeof (prf_type) /*tk_I*/ + sizeof (prf_type) /*tk_D*/ + sizeof (prf_type) /*tk_ACK*/ + sizeof (prf_type) /*tk_ACV*/ + sizeof (int) /*a_w*/;
    Result ciphers = server->search(tk_I, tk_D, tk_0, a_w);
    totalSearchCommSize += (sizeof (int) /*index*/ + sizeof (prf_type) /*value*/ + sizeof (int) /*acc*/) * ciphers.results.size(); //isLeaf can be determined based on index

    vector<int> finalRes;
    for (ResultRow resultRow : ciphers.results) {
        prf_type plaintext;
        Utilities::decode(resultRow.value, plaintext, secretKey);
        int id = *(int*) (&(plaintext.data()[AES_KEY_SIZE - 5]));
        finalRes.push_back(id);
    }
    return finalRes;
}

void Client::endSetup() {
    Odel->setupInsert(setupOdel);
    Oparents->setupInsert(setupParents);
    for (int i = 0; i < 1000; i++) {
        if (i % 100 == 0)
            cout << i << "/1000" << endl;
        Bid testBid;
        testBid.setValue(1);
        Odel->insert(testBid, "TEST");
    }
    for (int i = 0; i < 1000; i++) {
        if (i % 100 == 0)
            cout << i << "/1000" << endl;
        Bid testBid;
        testBid.setValue(1);
        Oparents->insert(testBid, "TEST");
    }
}

void Client::reset() {
    delete Odel;
    delete Oparents;
    bytes<Key> key{0};
    this->Odel = new OMAP(N, key);
    this->Oparents = new OMAP(N * 2, key);
    Mcnt.clear();
    MnextParents.clear();
}
