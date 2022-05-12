#ifndef UTILITIES_H
#define UTILITIES_H
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <chrono>
#include <stdlib.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <math.h>
#include <set>
#include <aes.h>

using namespace std;
class Bid;
#define ID_SIZE 16
#define AES_KEY_SIZE 16
typedef array<uint8_t, AES_KEY_SIZE> prf_type;


class Utilities {
private:
    static int parseLine(char* line);
public:
    Utilities();
    static std::string base64_encode(const char* bytes_to_encode, unsigned int in_len);
    static std::string base64_decode(std::string const& enc);
    static std::string XOR(std::string value, std::string key);
    static void startTimer(int id);
    static double stopTimer(int id);
    static std::map<int, std::chrono::time_point<std::chrono::high_resolution_clock> > m_begs;
    static std::map<std::string, std::ofstream*> handlers;
    static void logTime(std::string filename, std::string content);
    static void initializeLogging(std::string filename);
    static void finalizeLogging(std::string filename);
    static std::array<uint8_t, 16> convertToArray(std::string value);
    static int getMem();
    static double getTimeFromHist(int id);
    static std::array<uint8_t, 16> encode(std::string keyword);
    static std::array<uint8_t, 16> encode(std::string keyword, unsigned char* curkey);
    static std::array<uint8_t, 16> encode(unsigned char* plaintext, unsigned char* curkey = NULL);
    static std::string decode(std::array<uint8_t, 16> data, unsigned char* curkey = NULL);
    static unsigned char key[16], iv[16];
    static int decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key, unsigned char *iv, unsigned char *plaintext);
    static int encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key, unsigned char *iv, unsigned char *ciphertext);
    static void handleErrors(void);
    static std::vector<std::string> splitData(const std::string& str, const std::string& delim);
    static void decode(std::array<uint8_t, 16> ciphertext, std::array<uint8_t, 16>& plaintext, unsigned char* curkey = NULL);
    static prf_type bitwiseXOR(prf_type input1, prf_type input2);
    static prf_type bitwiseXOR(unsigned char* input1, prf_type input2);
    static std::array<uint8_t, 16> generatePRF(unsigned char* keyword, unsigned char* prfkey);
    static Bid getBid(string input);
    static int getNodeOnPath(int leaf, int curDepth, int height);
    static vector<int> findCoveringSet(int a_w, int height);
    static std::string& ltrim(std::string &s);
    static std::string& rtrim(std::string &s);
    static std::string& trim(std::string &s);
    static int smallest_power_of_two_greater_than(int n);
    virtual ~Utilities();
};

#endif /* UTILITIES_H */

