#include "Client.h"
#include <iostream>
#include <vector>
#include "Utilities.h"
#include <iostream>
#include <random>
#include <limits>

int main(int argc, char** argv) {

    Server server(64);
    Client client(&server, 64, 3);

    client.insert("hello", 0, false);
    client.insert("hello", 1, false);
    client.insert("hello", 2, false);
    client.insert("hello", 3, false);
    client.insert("hello", 4, false);
    client.remove("hello", 1, false);
    client.remove("hello", 2, false);
    auto rr = client.search("hello");
    printf("result size:%d\n", (int) rr.size());
    return 0;

}
