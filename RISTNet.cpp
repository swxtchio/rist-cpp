//
// Created by Anders Cedronius (Edgeware AB) on 2020-03-14.
//

#include "RISTNet.h"
#include "RISTNetInternal.h"

//---------------------------------------------------------------------------------------------------------------------
//
//
// Global private methods
//
//
//---------------------------------------------------------------------------------------------------------------------

bool ristNetIsIPv4(const std::string &str) {
  struct sockaddr_in sa;
  return inet_pton(AF_INET, str.c_str(), &(sa.sin_addr)) != 0;
}

bool ristNetIsIPv6(const std::string &str) {
  struct sockaddr_in6 sa;
  return inet_pton(AF_INET6, str.c_str(), &(sa.sin6_addr)) != 0;
}

bool ristNetBuildRISTURL(std::string ip, std::string port, std::string &url, bool listen) {
  int ipType = AF_INET;
  if (ristNetIsIPv4(ip)) {
    ipType = AF_INET;
  } else if (ristNetIsIPv6(ip)) {
    ipType = AF_INET6;
  } else {
    LOGGER(true, LOGG_ERROR, " " << "Provided IP-Address not valid.");
    return false;
  }
  int x = 0;
  std::stringstream portNum(port);
  portNum >> x;
  if (x < 1 || x > INT16_MAX) {
    LOGGER(true, LOGG_ERROR, " " << "Provided Port number not valid.");
    return false;
  }
  std::string ristURL = "";
  if (ipType == AF_INET) {
    ristURL += "rist://";
  } else {
    ristURL += "rist6://";
  }
  if (listen) {
    ristURL += "@";
  }
  if (ipType == AF_INET) {
    ristURL += ip + ":" + port;
  } else {
    ristURL += "[" + ip + "]:" + port;
  }
  url = ristURL;
  return true;
}

//---------------------------------------------------------------------------------------------------------------------
//
//
// RISTNetReciever  --  RECEIVER
//
//
//---------------------------------------------------------------------------------------------------------------------

RISTNetReciever::RISTNetReciever() {
  LOGGER(false, LOGG_NOTIFY, "RISTNetReciever constructed");
}

RISTNetReciever::~RISTNetReciever() {
  if (mRistReceiver) {
    int status = rist_server_shutdown(mRistReceiver);
    if (status) {
      LOGGER(true, LOGG_ERROR, "rist_server_shutdown failure");
    }
  }
  LOGGER(false, LOGG_NOTIFY, "RISTNetReciever destruct");
}

//---------------------------------------------------------------------------------------------------------------------
// RISTNetReciever  --  Callbacks --- Start
//---------------------------------------------------------------------------------------------------------------------

void RISTNetReciever::receiveData(void *arg, struct rist_peer *peer, uint64_t flow_id, const void *buf, size_t len, uint16_t src_port, uint16_t dst_port) {
  RISTNetReciever *weakSelf=(RISTNetReciever *)arg;
  if (weakSelf -> networkDataCallback) {
    weakSelf->mClientListMtx.lock();
    std::shared_ptr<NetworkConnection> netObj;
    if ( weakSelf -> clientList.find(peer) == weakSelf -> clientList.end() ) {
      netObj = nullptr;
    } else {
      netObj = weakSelf->clientList.find(peer)->second;
      netObj->peer = peer;
    }
    weakSelf->mClientListMtx.unlock();

   weakSelf -> networkDataCallback((const uint8_t*)buf, len, netObj);
  } else {
    LOGGER(true, LOGG_ERROR, "networkDataCallback not implemented");
  }
}

int RISTNetReciever::clientConnect(void *arg, char* connecting_ip, uint16_t connecting_port, char* local_ip, uint16_t local_port, struct rist_peer *peer) {
  RISTNetReciever *weakSelf=(RISTNetReciever *)arg;
  std::shared_ptr<NetworkConnection> connectionObject = nullptr;
  if (weakSelf->validateConnectionCallback) {
    connectionObject =  weakSelf->validateConnectionCallback(std::string(connecting_ip), connecting_port);
  }
  if (connectionObject) {
    weakSelf->mClientListMtx.lock();
    connectionObject->peer = peer;
    weakSelf->clientList[peer] = connectionObject;
    weakSelf->mClientListMtx.unlock();
    return 1;
  }
  return 0;
}

void RISTNetReciever::clientDisconnect(void *arg, struct rist_peer *peer) {
  RISTNetReciever *weakSelf=(RISTNetReciever *)arg;
  if ( weakSelf -> clientList.find(peer) == weakSelf -> clientList.end() ) {
    LOGGER(true, LOGG_ERROR, "RISTNetServer::clientDisconnect unknown peer");
  } else {
    weakSelf->mClientListMtx.lock();
    weakSelf->clientList.erase(weakSelf->clientList.find(peer)->first);
    weakSelf->mClientListMtx.unlock();
  }
}

//---------------------------------------------------------------------------------------------------------------------
// RISTNetReciever  --  Callbacks --- End
//---------------------------------------------------------------------------------------------------------------------

void RISTNetReciever::getActiveClients(std::function<void(std::map<struct rist_peer *, std::shared_ptr<NetworkConnection>> &)> function) {
  mClientListMtx.lock();
  if (function) {
    function(clientList);
  }
  mClientListMtx.unlock();
}

void RISTNetReciever::closeAllClientConnections() {
  mClientListMtx.lock();
  for (auto &x: clientList) {
    struct rist_peer *peer = x.first;
    int status = rist_server_disconnect_peer(mRistReceiver, peer);
    if (status) {
      LOGGER(true, LOGG_ERROR, "rist_server_disconnect_client failed: ");
    }
  }
  clientList.clear();
  mClientListMtx.unlock();
}

bool RISTNetReciever::initReceiver(std::vector<std::tuple<std::string, std::string, bool>> &interfaceList,
                                rist_peer_config &peerConfig, enum rist_log_level logLevel) {
  if (!interfaceList.size()) {
    LOGGER(true, LOGG_ERROR, "Interface list is empty.");
    return false;
  }

  int status;
  status = rist_server_create(&mRistReceiver,RIST_MAIN);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_server_create fail.");
    return false;
  }

  mRistPeerConfig.recovery_mode = peerConfig.recovery_mode;
  mRistPeerConfig.recovery_maxbitrate = peerConfig.recovery_maxbitrate;
  mRistPeerConfig.recovery_maxbitrate_return = peerConfig.recovery_maxbitrate_return;
  mRistPeerConfig.recovery_length_min = peerConfig.recovery_length_min;
  mRistPeerConfig.recovery_length_max = peerConfig.recovery_length_max;
  mRistPeerConfig.recover_reorder_buffer = peerConfig.recover_reorder_buffer;
  mRistPeerConfig.recovery_rtt_min = peerConfig.recovery_rtt_min;
  mRistPeerConfig.recovery_rtt_max = peerConfig.recovery_rtt_max;
  mRistPeerConfig.weight = 5;
  mRistPeerConfig.bufferbloat_mode = peerConfig.bufferbloat_mode;
  mRistPeerConfig.bufferbloat_limit = peerConfig.bufferbloat_limit;
  mRistPeerConfig.bufferbloat_hard_limit = peerConfig.bufferbloat_hard_limit;

  status = rist_server_init(mRistReceiver,&mRistPeerConfig, logLevel, clientConnect, clientDisconnect, this);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_server_init fail.");
    return false;
  }

  for (auto &interface: interfaceList) {
    auto listIP=std::get<0>(interface);
    auto listPort=std::get<1>(interface);
    auto listMode=std::get<2>(interface);
    std::string listURL;
    if (!ristNetBuildRISTURL(listIP, listPort, listURL, listMode)) {
      LOGGER(true, LOGG_ERROR, "Failed building URL.");
      return false;
    }
    status = rist_server_add_peer(mRistReceiver, listURL.c_str());
    if (status) {
      LOGGER(true, LOGG_ERROR, "rist_server_add_peer fail.");
      return false;
    }
  }

  status = rist_server_start(mRistReceiver, receiveData, this);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_server_init fail.");
    return false;
  }
  return true;
}

bool RISTNetReciever::sendData(struct rist_peer *peer,const uint8_t *data, size_t size) {
  if (!mRistReceiver) {
    LOGGER(true, LOGG_ERROR, "Receiver not active.");
    return false;
  }
  int status = rist_server_write_oob(mRistReceiver, peer, data, size);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_server_write_oob fail.");
    return false;
  }
  return true;
}

//---------------------------------------------------------------------------------------------------------------------
//
//
// RISTNetClient  --  CLIENT
//
//
//---------------------------------------------------------------------------------------------------------------------

RISTNetClient::RISTNetClient() {
  LOGGER(false, LOGG_NOTIFY, "RISTNetClient constructed");
}

RISTNetClient::~RISTNetClient() {
  if (mRistClient) {
    int status = rist_client_destroy(mRistClient);
    if (status) {
      LOGGER(true, LOGG_ERROR, "rist_client_destroy fail.");
    }
  }
  LOGGER(false, LOGG_NOTIFY, "RISTNetClient destruct.");
}

//---------------------------------------------------------------------------------------------------------------------
// RISTNetClient  --  Callbacks --- Start
//---------------------------------------------------------------------------------------------------------------------

void RISTNetClient::receiveData(void *arg, struct rist_peer *peer, const void *buffer, size_t len) {
  RISTNetClient *weakSelf=(RISTNetClient *)arg;
  if (weakSelf -> networkDataCallback) {
    weakSelf -> networkDataCallback((const uint8_t*)buffer, len);
  } else {
    LOGGER(true, LOGG_ERROR, "networkDataCallback not implemented");
  }

}

int RISTNetClient::serverConnect(void *arg, char* connecting_ip, uint16_t connecting_port, char* local_ip, uint16_t local_port, struct rist_peer *peer) {
  //The C++ wrapper is rejecting all server connections at this time
  return 0;
}

void RISTNetClient::serverDisconnect(void *arg, struct rist_peer *peer) {

}

//---------------------------------------------------------------------------------------------------------------------
// RISTNetClient  --  Callbacks --- End
//---------------------------------------------------------------------------------------------------------------------


bool RISTNetClient::startClient(std::vector<std::tuple<std::string, std::string, uint32_t>> &serversList,
                                rist_peer_config &peerConfig,
                                enum rist_log_level logLevel,
                                uint32_t keepAlive,
                                uint32_t timeOut) {
  if (!serversList.size()) {
    LOGGER(true, LOGG_ERROR, "list of servers is empty.");
    return false;
  }

  int status;
  status = rist_client_create(&mRistClient,RIST_MAIN);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_client_create fail.");
    return false;
  }

  uint64_t now;
  struct timeval time;
  gettimeofday(&time, NULL);
  now = time.tv_sec * 1000000;
  now += time.tv_usec;
  uint32_t adv_flow_id = (uint32_t)(now >> 16);
  adv_flow_id &= ~(1UL << 0);

  status = rist_client_init(mRistClient, adv_flow_id, logLevel, serverConnect, serverDisconnect, this);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_client_init fail.");
    return false;
  }

  for (auto &server: serversList) {
    std::string ristURL;
    auto ip = std::get<0>(server);
    auto port = std::get<1>(server);
    auto weight = std::get<2>(server);
    if (!ristNetBuildRISTURL(ip, port, ristURL, false)) {
      LOGGER(true, LOGG_ERROR, "Failed building URL");
      return false;
    }

    mRistPeerConfig.address = ristURL.c_str();
    mRistPeerConfig.localport = peerConfig.localport;
    mRistPeerConfig.recovery_mode = peerConfig.recovery_mode;
    mRistPeerConfig.recovery_maxbitrate = peerConfig.recovery_maxbitrate;
    mRistPeerConfig.recovery_maxbitrate_return = peerConfig.recovery_maxbitrate_return;
    mRistPeerConfig.recovery_length_min = peerConfig.recovery_length_min;
    mRistPeerConfig.recovery_length_max = peerConfig.recovery_length_max;
    mRistPeerConfig.recover_reorder_buffer = peerConfig.recover_reorder_buffer;
    mRistPeerConfig.recovery_rtt_min = peerConfig.recovery_rtt_min;
    mRistPeerConfig.recovery_rtt_max = peerConfig.recovery_rtt_max;
    mRistPeerConfig.weight = weight;
    mRistPeerConfig.bufferbloat_mode = peerConfig.bufferbloat_mode;
    mRistPeerConfig.bufferbloat_limit = peerConfig.bufferbloat_limit;
    mRistPeerConfig.bufferbloat_hard_limit = peerConfig.bufferbloat_hard_limit;

    struct rist_peer *peer;
    mPeerListMtx.lock();
    mRistPeerList.push_back(peer);
    status = rist_client_add_peer(mRistClient, &mRistPeerConfig, &mRistPeerList.back());
    mPeerListMtx.unlock();
    if (status) {
      LOGGER(true, LOGG_ERROR, "rist_client_add_peer fail.");
      //Implement garbage-collect
      return false;
    }


  }

  status = rist_client_set_keepalive_timeout(mRistClient, keepAlive);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_client_set_keepalive_timeout fail.");
    return false;
  }

  status = rist_client_set_session_timeout(mRistClient, timeOut);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_client_set_session_timeout fail.");
    return false;
  }

  status = rist_client_oob_enable(mRistClient, receiveData, this);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_client_oob_enable fail.");
    return false;
  }

  status = rist_client_start(mRistClient);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_client_start fail.");
    return false;
  }

  return true;
}

bool RISTNetClient::sendData(const uint8_t *data, size_t size) {
  int status;
  status = rist_client_write(mRistClient,data,size,0,0);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_client_write failed: ");
    return false;
  }
  return true;
}
