//
// Created by Anders Cedronius (Edgeware AB) on 2020-03-15.
//

#include <iostream>
#include <thread>
#include "RISTNet.h"

int packetCounter;

//This is my class managed by the network connection.
class MyClass {
public:
  MyClass() {
    isKnown = false;
  };
  std::atomic_bool isKnown;
};

//Return a connection object. (Return nullptr if you don't want to connect to that client)
std::shared_ptr<NetworkConnection> validateConnection(std::string ipAddress, uint16_t port) {
  std::cout << "Connecting IP: " << ipAddress << ":" << unsigned(port) << std::endl;
  auto a1 = std::make_shared<NetworkConnection>();
  auto a2 = std::make_shared<MyClass>();
  a1->object = std::make_shared<MyClass>();
  return a1;
}

void dataFromClient (const uint8_t *buf, size_t len, NetworkConnection &connection) {
  //Check the vector integrity
  bool testFail = false;
  for (int x = 0;x<len;x++) {
    if (buf[x] != (x & 0xff)) {
      testFail = true;
    }
  }

  if (testFail) {
    std::cout << "Did not recieve the correct data" << std::endl;
    packetCounter++;
  } else {
    std::cout << "Got " << unsigned(len) << " healthy bytes" << std::endl;
  }

}

int main() {
  std::cout << "cppRISTWrapper tests started" << std::endl;

  packetCounter = 0;

  //Create the server in a outer scope to destroy after the client is destroyed.
  RISTNetServer myRISTNetServer;
  myRISTNetServer.networkDataCallback=std::bind(&dataFromClient, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
  myRISTNetServer.validateConnectionCallback=std::bind(&validateConnection, std::placeholders::_1, std::placeholders::_2);
  {

    //Create a empty client.
    RISTNetClient myRISTNetClient;

    //---------------------
    //
    // set-up the server
    //
    //---------------------

    //List of interfaces to bind the server to
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
    if (!myRISTNetServer.startServer(interfaceListServer, myServerConfig, RIST_LOG_WARN)) {
      std::cout << "Failed starting server" << std::endl;
      return EXIT_FAILURE;
    }
    
    //---------------------
    //
    // set-up the client
    //
    //---------------------

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
    myRISTNetClient.startClient(serverAdresses, myClientConfig, RIST_LOG_WARN);

    std::vector<uint8_t> mydata(1000);
    std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });
    while (packetCounter++ < 10) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      std::cout << "bip" << std::endl;
      myRISTNetClient.sendData((const uint8_t *)mydata.data(), mydata.size());
    }

    std::cout << "RIST test end" << std::endl;
  }
  return EXIT_SUCCESS;

}
