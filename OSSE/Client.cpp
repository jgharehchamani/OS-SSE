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

    prf_type pure_k_w;
    memset(pure_k_w.data(), 0, AES_KEY_SIZE);
    copy(keyword.begin(), keyword.end(), pure_k_w.data());

    int src_cnt = 0, a_w = -1;
    Bid key = Utilities::getBid(keyword);
    string result = Mcnt[key];
    if (result != "") {
        vector<string> parts = Utilities::splitData(result, "#");
        src_cnt = stoi(parts[0]);
        a_w = stoi(parts[1]);
    } else {
        coverTreeStatus[pure_k_w] = 0;
        coverTreeStatusIsOne[pure_k_w] = 0;
    }

    a_w++;
    Mcnt[key] = to_string(src_cnt) + "#" + to_string(a_w);


    int nodeIndex = Utilities::getNodeOnPath(a_w, height, height);

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

    memset(k_w.data(), 0, AES_KEY_SIZE);
    copy(keyword.begin(), keyword.end(), k_w.data());
    *(int*) (&((unsigned char*) k_w.data())[AES_KEY_SIZE - 5]) = src_cnt;
    prf_type phi = Utilities::generatePRF(k_w.data(), secretKey);
    prf_type tk_I = Utilities::bitwiseXOR(IKey, phi);

    memset(mapKey.data(), 0, AES_KEY_SIZE);
    *(int*) (&(mapKey.data()[AES_KEY_SIZE - 5])) = nodeIndex;
    *(int*) (&(mapKey.data()[AES_KEY_SIZE - 9])) = 0;
    mapKey = Utilities::generatePRF(mapKey.data(), tk_I.data());

    prf_type plaintext;
    memset(plaintext.data(), 0, AES_KEY_SIZE);
    *(int*) (&(plaintext.data()[AES_KEY_SIZE - 5])) = id;
    *(int*) (&(plaintext.data()[AES_KEY_SIZE - 9])) = src_cnt;
    mapValue = Utilities::encode(plaintext.data(), secretKey);

    server->insert(mapKey, mapValue);
    //----------------------------------------------------------



    incrementCoverSetStatus(pure_k_w, a_w);
    totalUpdateCommSize = sizeof (prf_type) /*key*/ + sizeof (prf_type) /*value*/+
            (Odel->treeHandler->oram->totalRead + Odel->treeHandler->oram->totalWrite) * (sizeof (int) /*size of bucket index*/ +Odel->treeHandler->oram->clen_size /*size of each bucket*/);
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


    //Finding index and parent of target delete id
    //--------------------------------------------------
    string posStr;
    Bid newKey = Utilities::getBid(keyword + "#" + to_string(id));
    posStr = setup ? setupOdel[newKey] : Odel->find(newKey);
    int index = stoi(posStr);
    //--------------------------------------------------

    //extract diretion pattern
    vector < pair<bool, int> > goLeft;
    goLeft.reserve(height);
    int leaf = index;
    for (int d = height - 1; d > 0; d--) {
        if (leaf % 2 == 1) {
            goLeft.push_back(pair<bool, int>(true, leaf));
        } else {
            goLeft.push_back(pair<bool, int>(false, leaf));
        }
        leaf = (leaf + 1) / 2 - 1;
    }
    if (goLeft[goLeft.size() - 1].second == 1) {
        goLeft.push_back(pair<bool, int>(true, 0));
    } else {
        goLeft.push_back(pair<bool, int>(false, 0));
    }

    goLeft.push_back(pair<bool, int>(false, -1));

    std::reverse(goLeft.begin(), goLeft.end());

    int nextPowerOf2 = Utilities::smallest_power_of_two_greater_than(a_w + 2);
    int rootOfSubtree = Utilities::getNodeOnPath(0, height - log2(nextPowerOf2), height);
    int newRoot;
    newRoot = (rootOfSubtree + 1) / 2 - 1;


    int curNode = -1, actualNextNode = 0;
    int OMAPAccessCounter = 0;
    int parent = -1, grandParent = -1, sibling = -1, grandgrandparent = -1, siblingAcc = -1;
    vector<int> visitedNodes;
    vector<int> leftChildsNodes;
    vector<int> leftAccs;
    vector<int> rightChildsNodes;
    vector<int> rightAccs;
    vector<prf_type> IKeys, Ivalues;
    prf_type mapKey, mapValue;

    for (int i = 0; i < goLeft.size(); i++) {
        int leftChild, leftAcc, rightChild, rightAcc, curNodeAcc;
        OMAPAccessCounter++;
        Bid curNodeInfo;
        memset(curNodeInfo.id.data(), 0, ID_SIZE);
        copy(keyword.begin(), keyword.end(), curNodeInfo.id.data());
        *(int*) (&curNodeInfo.id[AES_KEY_SIZE - 4]) = curNode;
        if (setup) {
            std::array< byte_t, VALUE_SIZE> updateCurNode;
            if (setupParents.count(curNodeInfo) == 0) {
                if (goLeft[i].first) {
                    *(int*) (&(updateCurNode.data()[VALUE_SIZE - 4])) = curNode * 2 + 1;
                    *(int*) (&(updateCurNode.data()[VALUE_SIZE - 8])) = 0;
                    *(int*) (&(updateCurNode.data()[VALUE_SIZE - 12])) = -1;
                    *(int*) (&(updateCurNode.data()[VALUE_SIZE - 16])) = -1;
                    actualNextNode = curNode * 2 + 1;
                } else {
                    *(int*) (&(updateCurNode.data()[VALUE_SIZE - 4])) = -1;
                    *(int*) (&(updateCurNode.data()[VALUE_SIZE - 8])) = -1;
                    *(int*) (&(updateCurNode.data()[VALUE_SIZE - 12])) = curNode * 2 + 2;
                    *(int*) (&(updateCurNode.data()[VALUE_SIZE - 16])) = 0;
                    actualNextNode = curNode * 2 + 2;
                }
                *(int*) (&(updateCurNode.data()[VALUE_SIZE - 20])) = 0;
            } else {
                updateCurNode = setupParents[curNodeInfo];
                *(int*) (&(updateCurNode.data()[VALUE_SIZE - 20])) = (*(int*) (&(updateCurNode.data()[VALUE_SIZE - 20]))) + 1;
                if (goLeft[i].first) {
                    *(int*) (&(updateCurNode.data()[VALUE_SIZE - 8])) = (*(int*) (&(updateCurNode.data()[VALUE_SIZE - 8]))) + 1;
                    if (*(int*) (&(updateCurNode.data()[VALUE_SIZE - 4])) == -1) {
                        *(int*) (&(updateCurNode.data()[VALUE_SIZE - 4])) = goLeft[i].second;
                    }
                    actualNextNode = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 4]));

                } else {
                    *(int*) (&(updateCurNode.data()[VALUE_SIZE - 16])) = (*(int*) (&(updateCurNode.data()[VALUE_SIZE - 16]))) + 1;
                    if (*(int*) (&(updateCurNode.data()[VALUE_SIZE - 12])) == -1) {
                        *(int*) (&(updateCurNode.data()[VALUE_SIZE - 12])) = goLeft[i].second;
                    }
                    actualNextNode = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 12]));
                }
            }
            setupParents[curNodeInfo] = updateCurNode;
            if (newRoot == curNode) {
                Macc[key] = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 20]));
            }
            leftChild = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 4]));
            leftAcc = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 8]));
            rightChild = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 12]));
            rightAcc = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 16]));
            curNodeAcc = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 20]));
        } else {
            Oparents->getSet(curNodeInfo, [&](std::array< byte_t, VALUE_SIZE> value, bool found) {
                std::array< byte_t, VALUE_SIZE> updateCurNode;
                if (!found) {
                    if (goLeft[i].first) {
                        *(int*) (&(updateCurNode.data()[VALUE_SIZE - 4])) = curNode * 2 + 1;
                                *(int*) (&(updateCurNode.data()[VALUE_SIZE - 8])) = 0;
                                *(int*) (&(updateCurNode.data()[VALUE_SIZE - 12])) = -1;
                                *(int*) (&(updateCurNode.data()[VALUE_SIZE - 16])) = -1;
                                actualNextNode = curNode * 2 + 1;
                    } else {
                        *(int*) (&(updateCurNode.data()[VALUE_SIZE - 4])) = -1;
                                *(int*) (&(updateCurNode.data()[VALUE_SIZE - 8])) = -1;
                                *(int*) (&(updateCurNode.data()[VALUE_SIZE - 12])) = curNode * 2 + 2;
                                *(int*) (&(updateCurNode.data()[VALUE_SIZE - 16])) = 0;
                                actualNextNode = curNode * 2 + 2;
                    }
                    *(int*) (&(updateCurNode.data()[VALUE_SIZE - 20])) = 0;
                } else {
                    updateCurNode = value;
                            *(int*) (&(updateCurNode.data()[VALUE_SIZE - 20])) = (*(int*) (&(updateCurNode.data()[VALUE_SIZE - 20]))) + 1;
                    if (goLeft[i].first) {
                        *(int*) (&(updateCurNode.data()[VALUE_SIZE - 8])) = (*(int*) (&(updateCurNode.data()[VALUE_SIZE - 8]))) + 1;
                        if (*(int*) (&(updateCurNode.data()[VALUE_SIZE - 4])) == -1) {
                            *(int*) (&(updateCurNode.data()[VALUE_SIZE - 4])) = goLeft[i].second;
                        }
                        actualNextNode = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 4]));
                    } else {
                        *(int*) (&(updateCurNode.data()[VALUE_SIZE - 16])) = (*(int*) (&(updateCurNode.data()[VALUE_SIZE - 16]))) + 1;
                        if (*(int*) (&(updateCurNode.data()[VALUE_SIZE - 12])) == -1) {
                            *(int*) (&(updateCurNode.data()[VALUE_SIZE - 12])) = goLeft[i].second;
                        }
                        actualNextNode = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 12]));
                    }
                }
                if (newRoot == curNode) {
                    Macc[key] = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 20]));
                }
                leftChild = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 4]));
                leftAcc = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 8]));
                rightChild = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 12]));
                rightAcc = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 16]));
                curNodeAcc = *(int*) (&(updateCurNode.data()[VALUE_SIZE - 20]));
                return updateCurNode;
            });
        }


        memset(mapKey.data(), 0, AES_KEY_SIZE);
        *(int*) (&(mapKey.data()[AES_KEY_SIZE - 5])) = curNode;
        *(int*) (&(mapKey.data()[AES_KEY_SIZE - 9])) = curNodeAcc;
        mapKey = Utilities::generatePRF(mapKey.data(), tk_I.data());

        memset(mapValue.data(), 0, AES_KEY_SIZE);
        *(int*) (&(mapValue.data()[AES_KEY_SIZE - 8])) = leftAcc; //8-10
        *(int*) (&(mapValue.data()[AES_KEY_SIZE - 15])) = rightAcc; //1-3
        *(int*) (&(mapValue.data()[AES_KEY_SIZE - 5])) = leftChild; //11-14
        *(int*) (&(mapValue.data()[AES_KEY_SIZE - 12])) = rightChild; //4-7

        mapValue = Utilities::encode(mapValue.data(), tk_D.data());

        IKeys.push_back(mapKey);
        Ivalues.push_back(mapValue);

        leftChildsNodes.push_back((leftChild == -1 ? curNode * 2 + 1 : leftChild));
        rightChildsNodes.push_back(rightChild == -1 ? curNode * 2 + 2 : rightChild);
        leftAccs.push_back(leftAcc);
        rightAccs.push_back(rightAcc);
        visitedNodes.push_back(curNode);

        while (goLeft[i].second != actualNextNode) {
            i++;
        }
        curNode = actualNextNode;
    }

    if (visitedNodes.size() > 1) {
        parent = visitedNodes[visitedNodes.size() - 1];
        grandParent = visitedNodes[visitedNodes.size() - 2];
    } else if (visitedNodes.size() == 1) {
        parent = visitedNodes[0];
        grandParent = -1;
    } else {
        printf("ERROR\n");
    }

    if (leftChildsNodes[leftChildsNodes.size() - 1] == index) {

        sibling = rightChildsNodes[rightChildsNodes.size() - 1];
        siblingAcc = rightAccs[rightAccs.size() - 1];
    } else if (rightChildsNodes[rightChildsNodes.size() - 1] == index) {

        sibling = leftChildsNodes[leftChildsNodes.size() - 1];
        siblingAcc = leftAccs[leftAccs.size() - 1];
    } else {
        printf("ERROR\n");
    }



    while (OMAPAccessCounter < height - 2) {
        OMAPAccessCounter++;
        Bid testBid;
        testBid.setValue(1);
        Oparents->insert(testBid, "TEST");

        memset(mapKey.data(), 0, AES_KEY_SIZE);
        *(int*) (&(mapKey.data()[AES_KEY_SIZE - 5])) = -(rand());
        *(int*) (&(mapKey.data()[AES_KEY_SIZE - 9])) = -(rand());
        mapKey = Utilities::generatePRF(mapKey.data(), tk_I.data());
        auto encMapValue = Utilities::encode(mapValue.data(), tk_D.data());

        IKeys.push_back(mapKey);
        Ivalues.push_back(encMapValue);
    }

    bool parentIsLeft = false;
    for (int i = 0; i < goLeft.size(); i++) {
        if (goLeft[i].second == grandParent) {
            if (goLeft[i + 1].first) {
                parentIsLeft = true;
            } else {
                parentIsLeft = false;
            }
            break;
        }
    }

    int leftChild, leftAcc, rightChild, rightAcc, grandParentAcc;
    Bid grandParentInfo;
    memset(grandParentInfo.id.data(), 0, ID_SIZE);
    copy(keyword.begin(), keyword.end(), grandParentInfo.id.data());
    *(int*) (&grandParentInfo.id[AES_KEY_SIZE - 4]) = grandParent;
    if (setup) {
        std::array< byte_t, VALUE_SIZE> updatedGrandParent;
        updatedGrandParent = setupParents[grandParentInfo];
        if (grandParent == -1) {
            *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 4])) = sibling;
            *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 8])) = siblingAcc;
            *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 12])) = sibling;
            *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 16])) = siblingAcc;
        } else {
            if (parentIsLeft) {
                *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 4])) = sibling;
                *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 8])) = siblingAcc;
            } else {
                *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 12])) = sibling;
                *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 16])) = siblingAcc;
            }
        }
        setupParents[grandParentInfo] = updatedGrandParent;
        leftChild = *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 4]));
        leftAcc = *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 8]));
        rightChild = *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 12]));
        rightAcc = *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 16]));
        grandParentAcc = *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 20]));
    } else {
        Oparents->getSet(grandParentInfo, [&](std::array< byte_t, VALUE_SIZE> value, bool found) {
            std::array< byte_t, VALUE_SIZE> updatedGrandParent;
            updatedGrandParent = value;
            if (grandParent == -1) {
                *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 4])) = sibling;
                        *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 8])) = siblingAcc;
                        *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 12])) = sibling;
                        *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 16])) = siblingAcc;
            } else {
                if (parentIsLeft) {
                    *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 4])) = sibling;
                            *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 8])) = siblingAcc;
                } else {
                    *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 12])) = sibling;
                            *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 16])) = siblingAcc;
                }
            }
            leftChild = *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 4]));
            leftAcc = *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 8]));
            rightChild = *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 12]));
            rightAcc = *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 16]));
            grandParentAcc = *(int*) (&(updatedGrandParent.data()[VALUE_SIZE - 20]));
            return updatedGrandParent;
        });
    }

    memset(mapKey.data(), 0, AES_KEY_SIZE);
    *(int*) (&(mapKey.data()[AES_KEY_SIZE - 5])) = grandParent;
    *(int*) (&(mapKey.data()[AES_KEY_SIZE - 9])) = grandParentAcc;
    mapKey = Utilities::generatePRF(mapKey.data(), tk_I.data());

    // V = Enc(id,i_w)
    memset(mapValue.data(), 0, AES_KEY_SIZE);
    *(int*) (&(mapValue.data()[AES_KEY_SIZE - 8])) = (leftChild >= firstLeaf ? (int) 0 : (int) leftAcc);
    *(int*) (&(mapValue.data()[AES_KEY_SIZE - 15])) = (rightChild >= firstLeaf ? (int) 0 : (int) rightAcc);
    *(int*) (&(mapValue.data()[AES_KEY_SIZE - 5])) = leftChild;
    *(int*) (&(mapValue.data()[AES_KEY_SIZE - 12])) = rightChild;

    mapValue = Utilities::encode(mapValue.data(), tk_D.data());

    IKeys.push_back(mapKey);
    Ivalues.push_back(mapValue);

    server->insert(IKeys, Ivalues);

    vector<int> t_i_now = Utilities::findCoveringSet(a_w, height);
    bool foundIndex = false;


    //coverTreeStatus = 1 means that there are nodes in the subtree (bit 0 is for the biggest subtree and bit N is for the smallest newest subtree)
    //coverTreeStatusIsOne = 1 means that there is only one node in the subtree (bit 0 is for the biggest subtree and bit N is for the smallest newest subtree)

    prf_type pure_k_w;
    memset(pure_k_w.data(), 0, AES_KEY_SIZE);
    copy(keyword.begin(), keyword.end(), pure_k_w.data());

    while (index != 0 && !foundIndex) {
        for (int i = 0; i < t_i_now.size(); i++) {
            if (t_i_now[i] == index) {
                if (parent < t_i_now[i]) {
                    coverTreeStatus[pure_k_w] &= ~(1UL << i);
                    coverTreeStatusIsOne[pure_k_w] &= ~(1UL << i);
                } else if (parent >= t_i_now[i] && grandParent < t_i_now[i] && sibling >= firstLeaf) {
                    coverTreeStatusIsOne[pure_k_w] |= (1UL << i);
                }
                foundIndex = true;
                break;
            }
        }
        index = (index + 1) / 2 - 1;
    }

    totalUpdateCommSize = (sizeof (prf_type) /*key*/ + sizeof (prf_type) /*value*/) * IKeys.size() /*logN in the path*/+
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

    prf_type k_w;

    memset(k_w.data(), 0, AES_KEY_SIZE);
    copy(keyword.begin(), keyword.end(), k_w.data());
    *(int*) (&((unsigned char*) k_w.data())[AES_KEY_SIZE - 5]) = -1;
    prf_type phi = Utilities::generatePRF(k_w.data(), secretKey);
    prf_type keywordToken = Utilities::bitwiseXOR(IKey, phi);

    memset(k_w.data(), 0, AES_KEY_SIZE);
    copy(keyword.begin(), keyword.end(), k_w.data());
    *(int*) (&((unsigned char*) k_w.data())[AES_KEY_SIZE - 5]) = src_cnt;
    phi = Utilities::generatePRF(k_w.data(), secretKey);
    prf_type tk_I = Utilities::bitwiseXOR(IKey, phi);
    prf_type tk_D = Utilities::bitwiseXOR(DKey, phi);

    totalSearchCommSize = sizeof (prf_type) /*tk_I*/ + sizeof (prf_type) /*tk_D*/ + sizeof (prf_type) /*tk_ACK*/ + sizeof (prf_type) /*tk_ACV*/ + sizeof (int) /*a_w*/;
    Result ciphers = server->search(keywordToken, tk_I, tk_D, a_w, Macc[key], coverTreeStatus[key], coverTreeStatusIsOne[key]);
    totalSearchCommSize += (sizeof (int) /*index*/ + sizeof (prf_type) /*value*/) * ciphers.results.size(); //isLeaf can be determined based on index

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
    Macc.clear();
    MnextParents.clear();
    coverTreeStatus.clear();
    coverTreeStatusIsOne.clear();
}

void Client::incrementCoverSetStatus(Bid k_w, int a_w) {
    //bit 0 is for the first subtree bit 1 for second and bit n for the nth-subtree(newly added node)
    vector<int> t_i_now = Utilities::findCoveringSet(a_w, height);

    int tmp = 1;
    tmp = tmp << (t_i_now.size() - 1);


    if ((t_i_now[t_i_now.size() - 1] >= firstLeaf || (coverTreeStatus[k_w]>>(t_i_now.size() - 1)) % 2 == 0) || a_w == 0) {
        coverTreeStatusIsOne[k_w] |= tmp;
    } else {
        coverTreeStatusIsOne[k_w] &= ~(1UL << (t_i_now.size() - 1));
    }
    coverTreeStatus[k_w] |= tmp;
}