# OS-SSE
This repository contains two SSE schemes with optimal and quasi-optimal search times (OSSE and LLSE). The open-source is based on our following under review paper:

Javad Ghareh Chamani, Dimitrios Papadopoulos, Mohammadamin Karbasforushan, Ioannis Demertzis. "Dynamic Searchable Encryption with Optimal Search in the Presence of Deletions", USENIX Security 2022 (under review)


### Pre-requisites: ###
Our schemes were tested with the following configuration
- 64-bit Ubuntu 16.04
- g++ = 5.5
- Make = 4.1

### Getting Started ###
OSSE and LLSE are provided in two separate subfolders. To build each of them: In the corresponding folder, execute

    make clean
    make

This will produce a sample application based on the target scheme in {OSSE/LLSE}/dist/Debug/GNU-Linux/ folder. To execute the sample application provided by us, simply execute  the following command in the scheme's root folder

    ./dist/Debug/GNU-Linux/osse   #for OSSE sample application
    ./dist/Debug/GNU-Linux/llse   #for LLSE sample application

### Contact ###
Feel free to reach out to me to get help in setting up our code or any other queries you may have related to our paper: jgc@cse.ust.hk


