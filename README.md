![librist logo](cpprist.jpg)

# cppRISTWrapper


**(UNDER CONSTRUCTION.. Is currently not working)**



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

**Server:**

```cpp
 
//Create the server 
RISTNetServer myRISTNetServer;

//Register the callback  
myRISTNetServer.networkDataCallback=std::bind(&dataFromClient, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

//Build a list of interfaces:ports binding the server to (IPv4/IPv6)
//Atleast one interface has to be provided
std::vector<std::tuple<std::string, std::string>> interfaceListServer;
interfaceListServer.push_back(std::tuple<std::string, std::string>("0.0.0.0", "8000"));
interfaceListServer.push_back(std::tuple<std::string, std::string>("0.0.0.0", "9000"));

//Server Configuration
struct rist_peer_config myServerConfig;
myServerConfig.recovery_mode = RIST_RECOVERY_MODE_TIME;
myServerConfig.recovery_maxbitrate = 100;
myServerConfig.recovery_maxbitrate_return = 0;
myServerConfig.recovery_length_min = 1000;
myServerConfig.recovery_length_max = 1000;
myServerConfig.recover_reorder_buffer = 25;
myServerConfig.recovery_rtt_min = 50;
myServerConfig.recovery_rtt_max = 500;
myServerConfig.weight = 5;
myServerConfig.bufferbloat_mode = RIST_BUFFER_BLOAT_MODE_OFF;
myServerConfig.bufferbloat_limit = 6;
myServerConfig.bufferbloat_hard_limit = 20;

//Start the server
if (!myRISTNetServer.startServer(interfaceListServer, myServerConfig)) {
  std::cout << "Failed starting the server" << std::endl;
  return EXIT_FAILURE; //Deal with the error the way you want.
}

```

**Client:**

```cpp

//Create the client
RISTNetClient myRISTNetClient;

//List of server ip/ports connecting the client to + weight of the interfaces
std::vector<std::tuple<std::string, std::string, uint32_t >> serverAdresses;
serverAdresses.push_back(std::tuple<std::string, std::string, uint32_t>("127.0.0.1", "8000", 5));

struct rist_peer_config myClientConfig;
myClientConfig.localport = nullptr;
myClientConfig.recovery_mode = RIST_RECOVERY_MODE_TIME;
myClientConfig.recovery_maxbitrate = 100;
myClientConfig.recovery_maxbitrate_return = 0;
myClientConfig.recovery_length_min = 1000;
myClientConfig.recovery_length_max = 1000;
myClientConfig.recover_reorder_buffer = 25;
myClientConfig.recovery_rtt_min = 50;
myClientConfig.recovery_rtt_max = 500;
myClientConfig.bufferbloat_mode = RIST_BUFFER_BLOAT_MODE_OFF;
myClientConfig.bufferbloat_limit = 6;
myClientConfig.bufferbloat_hard_limit = 20;
myRISTNetClient.startClient(serverAdresses, myClientConfig);

//Send data to the server 
myRISTNetClient.sendData((const uint8_t *)mydata.data(), mydata.size());

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
