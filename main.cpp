//
// Created by Anders Cedronius (Edgeware AB) on 2020-03-15.
//

#include <iostream>
#include <thread>
#include "RISTNet.h"

//Create the Server
RISTNetReciever myRISTNetReciever;

int packetCounter;

//This is my class managed by the network connection.
class MyClass {
public:
  MyClass() {
    someVariable = 10;
    std::cout << "My class is now created and some variable is containing the value: " << unsigned(someVariable)
              << std::endl;
  };
  virtual ~MyClass() {
    std::cout << "My class is now destroyed and some variable is containing the value: " << unsigned(someVariable)
              << std::endl;
  };
  int someVariable = 0;
};

//Return a connection object. (Return nullptr if you don't want to connect to that client)
std::shared_ptr<NetworkConnection> validateConnection(std::string ipAddress, uint16_t port) {
  std::cout << "Connecting IP: " << ipAddress << ":" << unsigned(port) << std::endl;

  //Do we want to allow this connection?
  //Do we have enough resources to accept this connection...

  // if not then -> return nullptr;
  // else return a ptr to a NetworkConnection.
  // this NetworkConnection may contain a pointer to any C++ object you provide.
  // That object ptr will be passed to you when the client communicates with you.
  // If the network connection is dropped the destructor in your class is called as long
  // as you do not also hold a reference to that pointer since it's shared.

  auto a1 = std::make_shared<NetworkConnection>(); // Create the network connection
  a1->mObject = std::make_shared<MyClass>(); // Attach your object.
  return a1;
}

void dataFromClient(const uint8_t *buf, size_t len, std::shared_ptr<NetworkConnection> &connection) {

  //Get back your class like this ->
  if (connection) {
    auto v = std::any_cast<std::shared_ptr<MyClass> &>(connection->mObject);
    v->someVariable++;
  }

  //Check the vector integrity
  bool testFail = false;
  for (int x = 0; x < len; x++) {
    if (buf[x] != (x & 0xff)) {
      testFail = true;
    }
  }

  if (testFail) {
    std::cout << "Did not recieve the correct data" << std::endl;
    packetCounter++;
  } else {
    std::cout << "Got " << unsigned(len) << " expexted data" << std::endl;
    uint8_t data[1000];
    myRISTNetReciever.sendData(connection->peer,&data[0],1000);
  }
}

void dataFromServer(const uint8_t *buffer, size_t len) {
  std::cout << "data from server" << std::endl;
}

int main() {
  std::cout << "cppRISTWrapper tests started" << std::endl;

  packetCounter = 0;


  //validate the connecting client
  myRISTNetReciever.validateConnectionCallback =
      std::bind(&validateConnection, std::placeholders::_1, std::placeholders::_2);
  //recieve data from the client
  myRISTNetReciever.networkDataCallback =
      std::bind(&dataFromClient, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

  //---------------------
  //
  // set-up the server
  //
  //---------------------

  //List of interfaces to bind the server to
  std::vector<std::tuple<std::string, std::string, bool>> interfaceListServer;
  interfaceListServer.push_back(std::tuple<std::string, std::string, bool>("0.0.0.0", "8000", true));
  interfaceListServer.push_back(std::tuple<std::string, std::string, bool>("0.0.0.0", "9000", true));

  //Server Configuration (please see librist for details)
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

  //Start the server
  if (!myRISTNetReciever.initReceiver(interfaceListServer, myReceiverPeer, RIST_LOG_WARN)) {
    std::cout << "Failed starting the server" << std::endl;
    return EXIT_FAILURE;
  }

  //---------------------
  //
  // set-up the client
  //
  //---------------------

  //Create a client.
  RISTNetClient myRISTNetClient;
  myRISTNetClient.networkDataCallback = std::bind(&dataFromServer, std::placeholders::_1, std::placeholders::_2);

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
    std::cout << "Send packet" << std::endl;
    myRISTNetClient.sendData((const uint8_t *) mydata.data(), mydata.size());
  }

  std::cout << "RIST test end" << std::endl;

  return EXIT_SUCCESS;

}
