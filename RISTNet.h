//
// Created by Anders Cedronius (Edgeware AB) on 2020-03-14.
//

#ifndef CPPRISTWRAPPER__RISTNET_H
#define CPPRISTWRAPPER__RISTNET_H

#include "librist.h"
#include <sys/time.h>
#include <any>
#include <tuple>
#include <vector>
#include <sstream>
#include <memory>
#include <atomic>
#include <map>
#include <functional>
#include <mutex>
#include <sys/socket.h>
#include <arpa/inet.h>

class NetworkConnection {
public:
  std::any mObject; //Contains your object
  struct rist_peer *peer; //Contains the handle to the underlying connection
};

//---------------------------------------------------------------------------------------------------------------------
//
//
// RISTNetReciever  --  RECIEVER
//
//
//---------------------------------------------------------------------------------------------------------------------

class RISTNetReciever {
public:

  RISTNetReciever();
  virtual ~RISTNetReciever();

  bool initReceiver(std::vector<std::tuple<std::string, std::string, bool>> &interfaceList,
                   rist_peer_config &peerConfig, enum rist_log_level logLevel = RIST_LOG_QUIET);
  void getActiveClients(std::function<void(std::map<struct rist_peer *, std::shared_ptr<NetworkConnection>> &)> function);
  void closeAllClientConnections();
  bool sendData(struct rist_peer *peer, const uint8_t *data, size_t size);


  std::function<void(const uint8_t *buf, size_t len, std::shared_ptr<NetworkConnection> &connection)> networkDataCallback = nullptr;
  std::function<std::shared_ptr<NetworkConnection>(std::string ipAddress, uint16_t port)> validateConnectionCallback = nullptr;

  // delete copy and move constructors and assign operators
  RISTNetReciever(RISTNetReciever const&) = delete;             // Copy construct
  RISTNetReciever(RISTNetReciever&&) = delete;                  // Move construct
  RISTNetReciever& operator=(RISTNetReciever const&) = delete;  // Copy assign
  RISTNetReciever& operator=(RISTNetReciever &&) = delete;      // Move assign

  std::vector<std::tuple<std::string, std::string>> mServerList;

private:
  static void receiveData(void *arg, struct rist_peer *peer, uint64_t flow_id, const void *buf, size_t len, uint16_t src_port, uint16_t dst_port);
  static void clientDisconnect(void *arg, struct rist_peer *peer);
  static int clientConnect(void *arg, char* connecting_ip, uint16_t connecting_port, char* local_ip, uint16_t local_port, struct rist_peer *peer);
  rist_server *mRistReceiver = nullptr;
  rist_peer_config mRistPeerConfig;
  std::mutex mClientListMtx;
  std::map<struct rist_peer*, std::shared_ptr<NetworkConnection>> clientList = {};
};

//---------------------------------------------------------------------------------------------------------------------
//
//
// RISTNetClient  --  CLIENT
//
//
//---------------------------------------------------------------------------------------------------------------------

class RISTNetClient {
public:
  RISTNetClient();
  virtual ~RISTNetClient();

  bool startClient(std::vector<std::tuple<std::string, std::string, uint32_t>> &serversList,
                   rist_peer_config &peerConfig,
                   enum rist_log_level logLevel = RIST_LOG_QUIET,
                   uint32_t keepAlive = 5000,
                   uint32_t timeOut = 10000);

  bool sendData(const uint8_t *data, size_t size);

  std::function<void(const uint8_t *buf, size_t len)> networkDataCallback = nullptr;

  // delete copy and move constructors and assign operators
  RISTNetClient(RISTNetClient const&) = delete;             // Copy construct
  RISTNetClient(RISTNetClient&&) = delete;                  // Move construct
  RISTNetClient& operator=(RISTNetClient const&) = delete;  // Copy assign
  RISTNetClient& operator=(RISTNetClient &&) = delete;      // Move assign
private:
  static void receiveData(void *arg, struct rist_peer *peer, const void *buffer, size_t len);
  static int serverConnect(void *arg, char* connecting_ip, uint16_t connecting_port, char* local_ip, uint16_t local_port, struct rist_peer *peer);
  static void serverDisconnect(void *arg, struct rist_peer *peer);

  rist_client *mRistClient = nullptr;;
  rist_peer_config mRistPeerConfig;
  std::mutex mPeerListMtx;
  std::vector<struct rist_peer*> mRistPeerList = {};
};

#endif //CPPRISTWRAPPER__RISTNET_H
