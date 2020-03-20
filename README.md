![librist logo](cpprist.jpg)

# cppRISTWrapper


The C++ wrapper of [librist](https://code.videolan.org/rist/librist) is creating a thin C++ layer around librist.

The C++ wrapper has not implemented all librist functionality at this point.


## Building

Requires cmake version >= **3.10** and **C++17**

**Release:**

```sh
cmake -DCMAKE_BUILD_TYPE=Release .
make
```

***Debug:***

```sh
cmake -DCMAKE_BUILD_TYPE=Debug .
make
```

Output: 

**libristnet.a**

A static RIST C++ wrapper library 
 
**cppRISTWrapper**

*cppRISTWrapper* (executable) runs trough the unit tests and returns EXIT_SUCESS if all unit tests pass.

## Usage

The cppRISTWrapper > RISTNet class/library is divided into Server/Client. The Server/Client creation and configuration is detailed below.

**Reciever:**

```cpp
 
 //Create the receiver
 RISTNetReceiver myRISTNetReceiver;

//Register the callbacks  
//validate the connecting client
myRISTNetReceiver =
      std::bind(&validateConnection, std::placeholders::_1, std::placeholders::_2);
//recieve data from the client
myRISTNetReceiver =
      std::bind(&dataFromClient, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

//List of interfaces to bind the server to (true means listen mode)
std::vector<std::tuple<std::string, std::string, bool>> interfaceListServer;
interfaceListServer.push_back(std::tuple<std::string, std::string, bool>("0.0.0.0", "8000", true));
interfaceListServer.push_back(std::tuple<std::string, std::string, bool>("0.0.0.0", "9000", true));

//Receiver configuration (please see librist for details)
struct rist_peer_config myReceiverPeer;
myReceiverPeer.recovery_mode = RIST_RECOVERY_MODE_TIME;
myReceiverPeer.recovery_maxbitrate = 100;
myReceiverPeer.recovery_maxbitrate_return = 0;
myReceiverPeer.recovery_length_min = 1000;
myReceiverPeer.recovery_length_max = 1000;
myReceiverPeer.recover_reorder_buffer = 25;
myReceiverPeer.recovery_rtt_min = 50;
myReceiverPeer.recovery_rtt_max = 500;
myReceiverPeer.weight = 5;
myReceiverPeer.bufferbloat_mode = RIST_BUFFER_BLOAT_MODE_OFF;
myReceiverPeer.bufferbloat_limit = 6;
myReceiverPeer.bufferbloat_hard_limit = 20;

//Initialize the receiver
if (!myRISTNetReceiver.initReceiver(interfaceListServer, myReceiverPeer, RIST_LOG_WARN)) {
  std::cout << "Failed starting the server" << std::endl;
  return EXIT_FAILURE;
}

```

**Sender:**

```cpp

//Create a sender.
RISTNetSender myRISTNetSender;

//List of ip/ports, weight of the interface and listen(true) or send mode
std::vector<std::tuple<std::string, std::string, uint32_t, bool>> serverAdresses;
serverAdresses.push_back(std::tuple<std::string, std::string, uint32_t, bool>("127.0.0.1", "8000", 5, false));

struct rist_peer_config mySendPeer = {0};
mySendPeer.recovery_mode = RIST_RECOVERY_MODE_TIME;
mySendPeer.recovery_maxbitrate = 100;
mySendPeer.recovery_maxbitrate_return = 0;
mySendPeer.recovery_length_min = 1000;
mySendPeer.recovery_length_max = 1000;
mySendPeer.recover_reorder_buffer = 25;
mySendPeer.recovery_rtt_min = 50;
mySendPeer.recovery_rtt_max = 500;
mySendPeer.bufferbloat_mode = RIST_BUFFER_BLOAT_MODE_OFF;
mySendPeer.bufferbloat_limit = 6;
mySendPeer.bufferbloat_hard_limit = 20;
myRISTNetSender.initSender(serverAdresses, mySendPeer, RIST_LOG_WARN);

//Send data to a receiver 
myRISTNetSender.sendData((const uint8_t *) mydata.data(), mydata.size());

```

## Using libristnet in your CMake project

* **Step1** 

Add this in your CMake file.

```
#Include cppRISTWrapper
ExternalProject_Add(project_cppristwrapp
        GIT_REPOSITORY https://github.com/andersc/cppRISTWrapper
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap
        GIT_PROGRESS 1
        BUILD_COMMAND cmake --build ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap --config ${CMAKE_BUILD_TYPE} --target ristnet
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )
add_library(ristnet STATIC IMPORTED)
set_property(TARGET ristnet PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/libristnet.a)
add_dependencies(ristnet project_cppristwrapp)
```

* **Step2**

Link your library or executable.

```
target_link_libraries((your target) ristnet (the rest you want to link)) 
```

* **Step3** 

Add header file to your project.

```
#include "RISTNet.h"
```

You should now be able to use *libristnet* in your project and use any CMake supported IDE
