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


  std::function<void(const uint8_t *buf, size_t len, std::shared_ptr<NetworkConnection> &connection)> networkDataCallback = nullptr;
  std::function<std::shared_ptr<NetworkConnection>(std::string ipAddress, uint16_t port)> validateConnectionCallback = nullptr;

  // delete copy and move constructors and assign operators
  RISTNetReciever(RISTNetReciever const&) = delete;             // Copy construct
  RISTNetReciever(RISTNetReciever&&) = delete;                  // Move construct
  RISTNetReciever& operator=(RISTNetReciever const&) = delete;  // Copy assign
  RISTNetReciever& operator=(RISTNetReciever &&) = delete;      // Move assign

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

class RISTNetSender {
public:
  RISTNetSender();
  virtual ~RISTNetSender();

  bool initSender(std::vector<std::tuple<std::string, std::string, uint32_t, bool>> &serversList,
                   rist_peer_config &peerConfig,
                   enum rist_log_level logLevel = RIST_LOG_QUIET,
                   uint32_t keepAlive = 5000,
                   uint32_t timeOut = 10000);

  bool sendData(const uint8_t *data, size_t size);

  std::function<void(const uint8_t *buf, size_t len)> networkDataCallback = nullptr;
  std::function<std::shared_ptr<NetworkConnection>(std::string ipAddress, uint16_t port)> validateConnectionCallback = nullptr;

  // delete copy and move constructors and assign operators
  RISTNetSender(RISTNetSender const&) = delete;             // Copy construct
  RISTNetSender(RISTNetSender&&) = delete;                  // Move construct
  RISTNetSender& operator=(RISTNetSender const&) = delete;  // Copy assign
  RISTNetSender& operator=(RISTNetSender &&) = delete;      // Move assign
private:
  static void receiveData(void *arg, struct rist_peer *peer, const void *buffer, size_t len);
  static int clientConnect(void *arg, char* connecting_ip, uint16_t connecting_port, char* local_ip, uint16_t local_port, struct rist_peer *peer);
  static void clientDisconnect(void *arg, struct rist_peer *peer);

  rist_client *mRistSender = nullptr;;
  rist_peer_config mRistPeerConfig;
  std::mutex mPeerListMtx; //Fixme TODO?
  std::vector<struct rist_peer*> mRistPeerList = {};

  std::mutex mClientListMtx;
  std::map<struct rist_peer*, std::shared_ptr<NetworkConnection>> clientList = {};
};

#endif //CPPRISTWRAPPER__RISTNET_H
