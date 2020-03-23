//
// Created by Anders Cedronius (Edgeware AB) on 2020-03-15.
//

#include <iostream>
#include <thread>
#include "RISTNet.h"

//Create the receiver
RISTNetReceiver myRISTNetReceiver;

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
    std::cout << "Did not receive the correct data" << std::endl;
    packetCounter++;
  } else {
    std::cout << "Got " << unsigned(len) << " expexted data" << std::endl;
  }
}

int main() {

  uint32_t cppWrapperVersion;
  uint32_t ristMajor;
  uint32_t ristMinor;
  myRISTNetReceiver.getVersion(cppWrapperVersion, ristMajor, ristMinor);
  std::cout << "cppRISTWrapper version: " << unsigned(cppWrapperVersion) << "librist version: " << unsigned(ristMajor) << "." << unsigned(ristMinor) << std::endl;

  packetCounter = 0;


  //validate the connecting client
  myRISTNetReceiver.validateConnectionCallback =
      std::bind(&validateConnection, std::placeholders::_1, std::placeholders::_2);
  //receive data from the client
  myRISTNetReceiver.networkDataCallback =
      std::bind(&dataFromClient, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

  //---------------------
  //
  // set-up the receiver
  //
  //---------------------

  //List of ip(name)/ports and listen(true) or send mode
  std::vector<std::tuple<std::string, std::string, bool>> interfaceListServer;
  interfaceListServer.push_back(std::tuple<std::string, std::string, bool>("0.0.0.0", "8000", true));
  interfaceListServer.push_back(std::tuple<std::string, std::string, bool>("0.0.0.0", "9000", true));

  RISTNetReceiver::RISTNetReceiverSettings myReceiveConfiguration;
  myReceiveConfiguration.mPeerConfig.recovery_mode = RIST_RECOVERY_MODE_TIME;
  myReceiveConfiguration.mPeerConfig.recovery_maxbitrate = 100000;
  myReceiveConfiguration.mPeerConfig.recovery_maxbitrate_return = 0;
  myReceiveConfiguration.mPeerConfig.recovery_length_min = 1000;
  myReceiveConfiguration.mPeerConfig.recovery_length_max = 1000;
  myReceiveConfiguration.mPeerConfig.recover_reorder_buffer = 25;
  myReceiveConfiguration.mPeerConfig.recovery_rtt_min = 50;
  myReceiveConfiguration.mPeerConfig.recovery_rtt_max = 500;
  myReceiveConfiguration.mPeerConfig.weight = 5;
  myReceiveConfiguration.mPeerConfig.bufferbloat_mode = RIST_BUFFER_BLOAT_MODE_OFF;
  myReceiveConfiguration.mPeerConfig.bufferbloat_limit = 6;
  myReceiveConfiguration.mPeerConfig.bufferbloat_hard_limit = 20;

  myReceiveConfiguration.mLogLevel = RIST_LOG_WARN;
  //myReceiveConfiguration.mPSK = "fdijfdoijfsopsmcfjiosdmcjfiompcsjofi33849384983943"; //Enable encryption by providing a PSK

  //Initialize the receiver
  if (!myRISTNetReceiver.initReceiver(interfaceListServer, myReceiveConfiguration)) {
    std::cout << "Failed starting the server" << std::endl;
    return EXIT_FAILURE;
  }

  //---------------------
  //
  // set-up the sender
  //
  //---------------------

  //Create a sender.
  RISTNetSender myRISTNetSender;

  //List of ip(name)/ports, weight of the interface and listen(true) or send mode
  std::vector<std::tuple<std::string, std::string, uint32_t, bool>> serverAdresses;
  serverAdresses.push_back(std::tuple<std::string, std::string, uint32_t, bool>("127.0.0.1", "8000", 5, false));

  RISTNetSender::RISTNetSenderSettings mySendConfiguration;
  mySendConfiguration.mPeerConfig.recovery_mode = RIST_RECOVERY_MODE_TIME;
  mySendConfiguration.mPeerConfig.recovery_maxbitrate = 100000;
  mySendConfiguration.mPeerConfig.recovery_maxbitrate_return = 0;
  mySendConfiguration.mPeerConfig.recovery_length_min = 1000;
  mySendConfiguration.mPeerConfig.recovery_length_max = 1000;
  mySendConfiguration.mPeerConfig.recover_reorder_buffer = 25;
  mySendConfiguration.mPeerConfig.recovery_rtt_min = 50;
  mySendConfiguration.mPeerConfig.recovery_rtt_max = 500;
  mySendConfiguration.mPeerConfig.bufferbloat_mode = RIST_BUFFER_BLOAT_MODE_OFF;
  mySendConfiguration.mPeerConfig.bufferbloat_limit = 6;
  mySendConfiguration.mPeerConfig.bufferbloat_hard_limit = 20;

  mySendConfiguration.mLogLevel = RIST_LOG_WARN;
  //mySendConfiguration.mPSK = "fdijfdoijfsopsmcfjiosdmcjfiompcsjofi33849384983943"; //Enable encryption by providing a PSK

  myRISTNetSender.initSender(serverAdresses, mySendConfiguration);

  std::vector<uint8_t> mydata(1000);
  std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });
  while (packetCounter++ < 10) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "Send packet" << std::endl;
    myRISTNetSender.sendData((const uint8_t *) mydata.data(), mydata.size());
  }

  std::cout << "RIST test end" << std::endl;

  return EXIT_SUCCESS;

}
