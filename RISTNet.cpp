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
// RISTNetReceiver  --  RECEIVER
//
//
//---------------------------------------------------------------------------------------------------------------------

RISTNetReceiver::RISTNetReceiver() {
  LOGGER(false, LOGG_NOTIFY, "RISTNetReceiver constructed");
}

RISTNetReceiver::~RISTNetReceiver() {
  if (mRistReceiver) {
    int status = rist_server_shutdown(mRistReceiver);
    if (status) {
      LOGGER(true, LOGG_ERROR, "rist_server_shutdown failure");
    }
  }
  LOGGER(false, LOGG_NOTIFY, "RISTNetReceiver destruct");
}

//---------------------------------------------------------------------------------------------------------------------
// RISTNetReceiver  --  Callbacks --- Start
//---------------------------------------------------------------------------------------------------------------------

void RISTNetReceiver::receiveData(void *arg, struct rist_peer *peer, uint64_t flow_id, const void *buf, size_t len, uint16_t src_port, uint16_t dst_port) {
  RISTNetReceiver *weakSelf=(RISTNetReceiver *)arg;
  if (weakSelf -> networkDataCallback) {
    weakSelf->mClientListMtx.lock();
    std::shared_ptr<NetworkConnection> netObj;
    if ( weakSelf -> clientList.find(peer) == weakSelf -> clientList.end() ) {
      netObj = nullptr;
    } else {
      netObj = weakSelf->clientList.find(peer)->second;
    }
    weakSelf->mClientListMtx.unlock();

   weakSelf -> networkDataCallback((const uint8_t*)buf, len, netObj);
  } else {
    LOGGER(true, LOGG_ERROR, "networkDataCallback not implemented");
  }
}

int RISTNetReceiver::clientConnect(void *arg, char* connecting_ip, uint16_t connecting_port, char* local_ip, uint16_t local_port, struct rist_peer *peer) {
  RISTNetReceiver *weakSelf=(RISTNetReceiver *)arg;
  std::shared_ptr<NetworkConnection> connectionObject = nullptr;
  if (weakSelf->validateConnectionCallback) {
    connectionObject =  weakSelf->validateConnectionCallback(std::string(connecting_ip), connecting_port);
  }
  if (connectionObject) {
    weakSelf->mClientListMtx.lock();
    weakSelf->clientList[peer] = connectionObject;
    weakSelf->mClientListMtx.unlock();
    return 1;
  }
  return 0;
}

void RISTNetReceiver::clientDisconnect(void *arg, struct rist_peer *peer) {
  RISTNetReceiver *weakSelf=(RISTNetReceiver *)arg;
  if ( weakSelf -> clientList.find(peer) == weakSelf -> clientList.end() ) {
    LOGGER(true, LOGG_ERROR, "RISTNetReceiver::clientDisconnect unknown peer");
  } else {
    weakSelf->mClientListMtx.lock();
    weakSelf->clientList.erase(weakSelf->clientList.find(peer)->first);
    weakSelf->mClientListMtx.unlock();
  }
}

//---------------------------------------------------------------------------------------------------------------------
// RISTNetReceiver  --  Callbacks --- End
//---------------------------------------------------------------------------------------------------------------------

void RISTNetReceiver::getActiveClients(std::function<void(std::map<struct rist_peer *, std::shared_ptr<NetworkConnection>> &)> function) {
  mClientListMtx.lock();
  if (function) {
    function(clientList);
  }
  mClientListMtx.unlock();
}

void RISTNetReceiver::closeAllClientConnections() {
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

bool RISTNetReceiver::initReceiver(std::vector<std::tuple<std::string, std::string, bool>> &interfaceList,
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

//---------------------------------------------------------------------------------------------------------------------
//
//
// RISTNetSender  --  CLIENT
//
//
//---------------------------------------------------------------------------------------------------------------------

RISTNetSender::RISTNetSender() {
  LOGGER(false, LOGG_NOTIFY, "RISTNetSender constructed");
}

RISTNetSender::~RISTNetSender() {
  if (mRistSender) {
    int status = rist_client_destroy(mRistSender);
    if (status) {
      LOGGER(true, LOGG_ERROR, "rist_client_destroy fail.");
    }
  }
  LOGGER(false, LOGG_NOTIFY, "RISTNetClient destruct.");
}

//---------------------------------------------------------------------------------------------------------------------
// RISTNetSender  --  Callbacks --- Start
//---------------------------------------------------------------------------------------------------------------------

void RISTNetSender::receiveData(void *arg, struct rist_peer *peer, const void *buffer, size_t len) {
  RISTNetSender *weakSelf=(RISTNetSender *)arg;
  if (weakSelf -> networkDataCallback) {
    weakSelf->mClientListMtx.lock();
    std::shared_ptr<NetworkConnection> netObj;
    if ( weakSelf -> clientList.find(peer) == weakSelf -> clientList.end() ) {
      netObj = nullptr;
    } else {
      netObj = weakSelf->clientList.find(peer)->second;
    }
    weakSelf->mClientListMtx.unlock();
    weakSelf -> networkDataCallback((const uint8_t*)buffer, len, netObj);
  } else {
    LOGGER(true, LOGG_ERROR, "networkDataCallback not implemented");
  }

}

int RISTNetSender::clientConnect(void *arg, char* connecting_ip, uint16_t connecting_port, char* local_ip, uint16_t local_port, struct rist_peer *peer) {
  RISTNetSender *weakSelf=(RISTNetSender *)arg;
  std::shared_ptr<NetworkConnection> connectionObject = nullptr;
  if (weakSelf->validateConnectionCallback) {
    connectionObject =  weakSelf->validateConnectionCallback(std::string(connecting_ip), connecting_port);
  }
  if (connectionObject) {
    weakSelf->mClientListMtx.lock();
    weakSelf->clientList[peer] = connectionObject;
    weakSelf->mClientListMtx.unlock();
    return 1;
  }
  return 0;
}

void RISTNetSender::clientDisconnect(void *arg, struct rist_peer *peer) {
  RISTNetSender *weakSelf=(RISTNetSender *)arg;
  if ( weakSelf -> clientList.find(peer) == weakSelf -> clientList.end() ) {
    LOGGER(true, LOGG_ERROR, "RISTNetReceiver::clientDisconnect unknown peer");
  } else {
    weakSelf->mClientListMtx.lock();
    weakSelf->clientList.erase(weakSelf->clientList.find(peer)->first);
    weakSelf->mClientListMtx.unlock();
  }
}

//---------------------------------------------------------------------------------------------------------------------
// RISTNetSender  --  Callbacks --- End
//---------------------------------------------------------------------------------------------------------------------

void RISTNetSender::getActiveClients(std::function<void(std::map<struct rist_peer *, std::shared_ptr<NetworkConnection>> &)> function) {
  mClientListMtx.lock();
  if (function) {
    function(clientList);
  }
  mClientListMtx.unlock();
}

void RISTNetSender::closeAllClientConnections() {
  mClientListMtx.lock();
  for (auto &x: clientList) {
    struct rist_peer *peer = x.first;
    int status = rist_client_remove_peer(mRistSender, peer);
    if (status) {
      LOGGER(true, LOGG_ERROR, "rist_client_remove_peer failed: ");
    }
  }
  clientList.clear();
  mClientListMtx.unlock();
}

bool RISTNetSender::initSender(std::vector<std::tuple<std::string, std::string, uint32_t, bool>> &serversList,
                               rist_peer_config &peerConfig,
                               enum rist_log_level logLevel,
                               uint32_t keepAlive,
                               uint32_t timeOut){
  if (!serversList.size()) {
    LOGGER(true, LOGG_ERROR, "list of servers is empty.");
    return false;
  }

  int status;
  status = rist_client_create(&mRistSender,RIST_MAIN);
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

  status = rist_client_init(mRistSender, adv_flow_id, logLevel, clientConnect, clientDisconnect, this);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_client_init fail.");
    return false;
  }

  for (auto &server: serversList) {
    std::string ristURL;
    auto listIP = std::get<0>(server);
    auto listPort = std::get<1>(server);
    auto listWeight = std::get<2>(server);
    auto listMode = std::get<3>(server);
    if (!ristNetBuildRISTURL(listIP, listPort, ristURL, listMode)) {
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
    mRistPeerConfig.weight = listWeight;
    mRistPeerConfig.bufferbloat_mode = peerConfig.bufferbloat_mode;
    mRistPeerConfig.bufferbloat_limit = peerConfig.bufferbloat_limit;
    mRistPeerConfig.bufferbloat_hard_limit = peerConfig.bufferbloat_hard_limit;

    struct rist_peer *peer;
    mPeerListMtx.lock();
    mRistPeerList.push_back(peer);
    status = rist_client_add_peer(mRistSender, &mRistPeerConfig, &mRistPeerList.back());
    mPeerListMtx.unlock();
    if (status) {
      LOGGER(true, LOGG_ERROR, "rist_client_add_peer fail.");
      //Implement garbage-collect
      return false;
    }


  }

  status = rist_client_set_keepalive_timeout(mRistSender, keepAlive);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_client_set_keepalive_timeout fail.");
    return false;
  }

  status = rist_client_set_session_timeout(mRistSender, timeOut);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_client_set_session_timeout fail.");
    return false;
  }

  status = rist_client_oob_enable(mRistSender, receiveData, this);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_client_oob_enable fail.");
    return false;
  }

  status = rist_client_start(mRistSender);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_client_start fail.");
    return false;
  }

  return true;
}

bool RISTNetSender::sendData(const uint8_t *data, size_t size) {
  int status;
  status = rist_client_write(mRistSender,data,size,0,0);
  if (status) {
    LOGGER(true, LOGG_ERROR, "rist_client_write failed: ");
    return false;
  }
  return true;
}
