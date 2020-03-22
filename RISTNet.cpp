//
// Created by Anders Cedronius (Edgeware AB) on 2020-03-14.
//

#include "RISTNet.h"
#include "RISTNetInternal.h"

//---------------------------------------------------------------------------------------------------------------------
//
//
// Network tools
//
//
//---------------------------------------------------------------------------------------------------------------------

bool RISTNetTools::isIPv4(const std::string &rStr) {
  struct sockaddr_in lsa;
  return inet_pton(AF_INET, rStr.c_str(), &(lsa.sin_addr)) != 0;
}

bool RISTNetTools::isIPv6(const std::string &rStr) {
  struct sockaddr_in6 lsa;
  return inet_pton(AF_INET6, rStr.c_str(), &(lsa.sin6_addr)) != 0;
}

bool RISTNetTools::buildRISTURL(std::string ip, std::string port, std::string &rURL, bool listen) {
  int lIPType;
  if (isIPv4(ip)) {
    lIPType = AF_INET;
  } else if (isIPv6(ip)) {
    lIPType = AF_INET6;
  } else {
    LOGGER(true, LOGG_ERROR, " " << "Provided IP-Address not valid.")
    return false;
  }
  int lPort = 0;
  std::stringstream lPortNum(port);
  lPortNum >> lPort;
  if (lPort < 1 || lPort > INT16_MAX) {
    LOGGER(true, LOGG_ERROR, " " << "Provided Port number not valid.")
    return false;
  }
  std::string lRistURL = "";
  if (lIPType == AF_INET) {
    lRistURL += "rist://";
  } else {
    lRistURL += "rist6://";
  }
  if (listen) {
    lRistURL += "@";
  }
  if (lIPType == AF_INET) {
    lRistURL += ip + ":" + port;
  } else {
    lRistURL += "[" + ip + "]:" + port;
  }
  rURL = lRistURL;
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
  validateConnectionCallback = std::bind(&RISTNetReceiver::validateConnectionStub, this, std::placeholders::_1, std::placeholders::_2);
  networkDataCallback = std::bind(&RISTNetReceiver::dataFromClientStub, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
  LOGGER(false, LOGG_NOTIFY, "RISTNetReceiver constructed")
}

RISTNetReceiver::~RISTNetReceiver() {
  if (mRistReceiver) {
    int lStatus = rist_server_shutdown(mRistReceiver);
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_server_shutdown failure")
    }
  }
  LOGGER(false, LOGG_NOTIFY, "RISTNetReceiver destruct")
}

//---------------------------------------------------------------------------------------------------------------------
// RISTNetReceiver  --  Callbacks --- Start
//---------------------------------------------------------------------------------------------------------------------


std::shared_ptr<NetworkConnection> RISTNetReceiver::validateConnectionStub(std::string ipAddress, uint16_t port) {
  LOGGER(true, LOGG_ERROR, "networkDataCallback not implemented. Will not accept connection from: " << ipAddress << ":" << unsigned(port))
  return 0;
}

void RISTNetReceiver::dataFromClientStub(const uint8_t *buf, size_t len, std::shared_ptr<NetworkConnection> &connection) {
}

void RISTNetReceiver::receiveData(void *pArg, struct rist_peer *pPeer, uint64_t flow_id, const void *pBuf, size_t len, uint16_t src_port, uint16_t dst_port) {
  RISTNetReceiver *lWeakSelf = (RISTNetReceiver *) pArg;
  lWeakSelf -> mClientListMtx.lock();
  auto netObj = lWeakSelf -> mClientList.find(pPeer);
  if (netObj != lWeakSelf -> mClientList.end())
  {
    auto netCon = netObj -> second;
    lWeakSelf -> mClientListMtx.unlock();
    lWeakSelf -> networkDataCallback((const uint8_t *) pBuf, len, netCon);
    return;
  }
  lWeakSelf -> mClientListMtx.unlock();
}

int RISTNetReceiver::clientConnect(void *pArg, char* pConnectingIP, uint16_t connectingPort, char* pLocalIP, uint16_t localPort, struct rist_peer *pPeer) {
  RISTNetReceiver *lWeakSelf = (RISTNetReceiver *) pArg;
  auto lNetObj = lWeakSelf -> validateConnectionCallback(std::string(pConnectingIP), connectingPort);
  if (lNetObj) {
    lWeakSelf -> mClientListMtx.lock();
    lWeakSelf -> mClientList[pPeer] = lNetObj;
    lWeakSelf -> mClientListMtx.unlock();
    return 1; // Accept the connection
  }
  return 0; // Reject the connection
}

void RISTNetReceiver::clientDisconnect(void *pArg, struct rist_peer *pPeer) {
  RISTNetReceiver *lWeakSelf = (RISTNetReceiver *) pArg;
  lWeakSelf -> mClientListMtx.lock();
  if (lWeakSelf -> mClientList.find(pPeer) == lWeakSelf -> mClientList.end()) {
    LOGGER(true, LOGG_ERROR, "RISTNetReceiver::clientDisconnect unknown peer")
    lWeakSelf -> mClientListMtx.unlock();
    return;
  } else {
    lWeakSelf -> mClientList.erase(lWeakSelf -> mClientList.find(pPeer)->first);
  }
  lWeakSelf -> mClientListMtx.unlock();
}

//---------------------------------------------------------------------------------------------------------------------
// RISTNetReceiver  --  Callbacks --- End
//---------------------------------------------------------------------------------------------------------------------

void RISTNetReceiver::getActiveClients(std::function<void(std::map<struct rist_peer *, std::shared_ptr<NetworkConnection>> &)> function) {
  mClientListMtx.lock();
  if (function) {
    function(mClientList);
  }
  mClientListMtx.unlock();
}

void RISTNetReceiver::closeAllClientConnections() {
  mClientListMtx.lock();
  for (auto &rPeer: mClientList) {
    struct rist_peer *lPeer = rPeer.first;
    int lStatus = rist_server_disconnect_peer(mRistReceiver, lPeer);
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_server_disconnect_client failed: ")
    }
  }
  mClientList.clear();
  mClientListMtx.unlock();
}

bool RISTNetReceiver::destroyReceiver() {
  if (mRistReceiver) {
    int lStatus = rist_server_shutdown(mRistReceiver);
    mRistReceiver = nullptr;
    mClientListMtx.lock();
    mClientList.clear();
    mClientListMtx.unlock();
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_server_shutdown fail.")
      return false;
    }
  } else {
    LOGGER(true, LOGG_WARN, "RIST receiver not initialised.")
    return false;
  }
  return true;
}

bool RISTNetReceiver::initReceiver(std::vector<std::tuple<std::string, std::string, bool>> &rInterfaceList,
                                rist_peer_config &rPeerConfig, enum rist_log_level logLevel) {
  if (!rInterfaceList.size()) {
    LOGGER(true, LOGG_ERROR, "Interface list is empty.")
    return false;
  }

  int lStatus;
  lStatus = rist_server_create(&mRistReceiver, RIST_PROFILE_MAIN);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_server_create fail.")
    return false;
  }

  mRistPeerConfig.recovery_mode = rPeerConfig.recovery_mode;
  mRistPeerConfig.recovery_maxbitrate = rPeerConfig.recovery_maxbitrate;
  mRistPeerConfig.recovery_maxbitrate_return = rPeerConfig.recovery_maxbitrate_return;
  mRistPeerConfig.recovery_length_min = rPeerConfig.recovery_length_min;
  mRistPeerConfig.recovery_length_max = rPeerConfig.recovery_length_max;
  mRistPeerConfig.recover_reorder_buffer = rPeerConfig.recover_reorder_buffer;
  mRistPeerConfig.recovery_rtt_min = rPeerConfig.recovery_rtt_min;
  mRistPeerConfig.recovery_rtt_max = rPeerConfig.recovery_rtt_max;
  mRistPeerConfig.weight = 5;
  mRistPeerConfig.bufferbloat_mode = rPeerConfig.bufferbloat_mode;
  mRistPeerConfig.bufferbloat_limit = rPeerConfig.bufferbloat_limit;
  mRistPeerConfig.bufferbloat_hard_limit = rPeerConfig.bufferbloat_hard_limit;

  lStatus = rist_server_init(mRistReceiver,&mRistPeerConfig, logLevel, clientConnect, clientDisconnect, this);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_server_init fail.")
    destroyReceiver();
    return false;
  }

  for (auto &rInterface: rInterfaceList) {
    auto lIP = std::get<0>(rInterface);
    auto lPort = std::get<1>(rInterface);
    auto lMode = std::get<2>(rInterface);
    std::string lURL;
    if (!mNetTools.buildRISTURL(lIP, lPort, lURL, lMode)) {
      LOGGER(true, LOGG_ERROR, "Failed building URL.")
      return false;
    }
    lStatus = rist_server_add_peer(mRistReceiver, lURL.c_str());
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_server_add_peer fail.")
      destroyReceiver();
      return false;
    }
  }

  lStatus = rist_server_start(mRistReceiver, receiveData, this);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_server_init fail.")
    destroyReceiver();
    return false;
  }
  return true;
}

//---------------------------------------------------------------------------------------------------------------------
//
//
// RISTNetSender  --  SENDER
//
//
//---------------------------------------------------------------------------------------------------------------------

RISTNetSender::RISTNetSender() {
  validateConnectionCallback = std::bind(&RISTNetSender::validateConnectionStub, this, std::placeholders::_1, std::placeholders::_2);
  networkDataCallback = std::bind(&RISTNetSender::dataFromClientStub, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
  LOGGER(false, LOGG_NOTIFY, "RISTNetSender constructed")
}

RISTNetSender::~RISTNetSender() {
  if (mRistSender) {
    int lStatus = rist_client_destroy(mRistSender);
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_client_destroy fail.")
    }
  }
  LOGGER(false, LOGG_NOTIFY, "RISTNetClient destruct.")
}

//---------------------------------------------------------------------------------------------------------------------
// RISTNetSender  --  Callbacks --- Start
//---------------------------------------------------------------------------------------------------------------------

std::shared_ptr<NetworkConnection> RISTNetSender::validateConnectionStub(std::string ipAddress, uint16_t port) {
  LOGGER(true, LOGG_ERROR, "networkDataCallback not implemented. Will not accept connection from: " << ipAddress << ":" << unsigned(port))
  return 0;
}

void RISTNetSender::dataFromClientStub(const uint8_t *buf, size_t len, std::shared_ptr<NetworkConnection> &connection) {
}

void RISTNetSender::receiveData(void *pArg, struct rist_peer *pPeer, const void *pBuffer, size_t len) {
  RISTNetSender *lWeakSelf = (RISTNetSender *) pArg;
  lWeakSelf -> mClientListMtx.lock();
  auto netObj = lWeakSelf -> mClientList.find(pPeer);
  if (netObj != lWeakSelf -> mClientList.end())
  {
    auto netCon = netObj -> second;
    lWeakSelf -> mClientListMtx.unlock();
    lWeakSelf -> networkDataCallback((const uint8_t *) pBuffer, len, netCon);
    return;
  }
  lWeakSelf -> mClientListMtx.unlock();
}

int RISTNetSender::clientConnect(void *pArg, char* pConnectingIP, uint16_t connectingPort, char* pLocalIP, uint16_t localPort, struct rist_peer *pPeer) {
  RISTNetSender *lWeakSelf = (RISTNetSender *) pArg;
  auto lNetObj = lWeakSelf -> validateConnectionCallback(std::string(pConnectingIP), connectingPort);
  if (lNetObj) {
    lWeakSelf -> mClientListMtx.lock();
    lWeakSelf -> mClientList[pPeer] = lNetObj;
    lWeakSelf -> mClientListMtx.unlock();
    return 1; // Accept the connection
  }
  return 0; // Reject the connection
}

void RISTNetSender::clientDisconnect(void *pArg, struct rist_peer *pPeer) {
  RISTNetSender *lWeakSelf = (RISTNetSender *) pArg;
  lWeakSelf -> mClientListMtx.lock();
  if ( lWeakSelf -> mClientList.find(pPeer) == lWeakSelf -> mClientList.end() ) {
    LOGGER(true, LOGG_ERROR, "RISTNetReceiver::clientDisconnect unknown peer")
    lWeakSelf -> mClientListMtx.unlock();
    return;
  } else {
    lWeakSelf -> mClientList.erase(lWeakSelf -> mClientList.find(pPeer) -> first);
  }
  lWeakSelf -> mClientListMtx.unlock();
}

//---------------------------------------------------------------------------------------------------------------------
// RISTNetSender  --  Callbacks --- End
//---------------------------------------------------------------------------------------------------------------------

void RISTNetSender::getActiveClients(std::function<void(std::map<struct rist_peer *, std::shared_ptr<NetworkConnection>> &)> function) {
  mClientListMtx.lock();
  if (function) {
    function(mClientList);
  }
  mClientListMtx.unlock();
}

void RISTNetSender::closeAllClientConnections() {
  mClientListMtx.lock();
  for (auto &rPeer: mClientList) {
    struct rist_peer *pPeer = rPeer.first;
    int status = rist_client_remove_peer(mRistSender, pPeer);
    if (status) {
      LOGGER(true, LOGG_ERROR, "rist_client_remove_peer failed: ")
    }
  }
  mClientList.clear();
  mClientListMtx.unlock();
}

bool RISTNetSender::destroySender() {
  if (mRistSender) {
    int lStatus = rist_client_destroy(mRistSender);
    mRistSender = nullptr;
    mClientListMtx.lock();
    mClientList.clear();
    mClientListMtx.unlock();
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_client_destroy fail.")
      return false;
    }
  } else {
    LOGGER(true, LOGG_WARN, "RIST sender not existing.")
  }
  return true;
}

bool RISTNetSender::initSender(std::vector<std::tuple<std::string, std::string, uint32_t, bool>> &rPeerList,
                               rist_peer_config &rPeerConfig,
                               enum rist_log_level logLevel,
                               enum rist_profile profile,
                               uint32_t keepAlive,
                               uint32_t timeOut){
  if (!rPeerList.size()) {
    LOGGER(true, LOGG_ERROR, "list of servers is empty.")
    return false;
  }

  int lStatus;
  lStatus = rist_client_create(&mRistSender, profile);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_client_create fail.")
    return false;
  }

  uint64_t lNow;
  struct timeval lTime;
  gettimeofday(&lTime, NULL);
  lNow = lTime.tv_sec * 1000000;
  lNow += lTime.tv_usec;
  uint32_t lAdvFlowID = (uint32_t)(lNow >> 16);
  lAdvFlowID &= ~(1UL << 0);

  lStatus = rist_client_init(mRistSender, lAdvFlowID, logLevel, clientConnect, clientDisconnect, this);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_client_init fail.")
    destroySender();
    return false;
  }

  for (auto &rSinglePeer: rPeerList) {
    std::string lRistURL;
    auto lIP = std::get<0>(rSinglePeer);
    auto lPort = std::get<1>(rSinglePeer);
    auto lWeight = std::get<2>(rSinglePeer);
    auto lMode = std::get<3>(rSinglePeer);
    if (!mNetTools.buildRISTURL(lIP, lPort, lRistURL, lMode)) {
      LOGGER(true, LOGG_ERROR, "Failed building URL")
      destroySender();
      return false;
    }

    mRistPeerConfig.address = lRistURL.c_str();
    mRistPeerConfig.localport = rPeerConfig.localport;
    mRistPeerConfig.recovery_mode = rPeerConfig.recovery_mode;
    mRistPeerConfig.recovery_maxbitrate = rPeerConfig.recovery_maxbitrate;
    mRistPeerConfig.recovery_maxbitrate_return = rPeerConfig.recovery_maxbitrate_return;
    mRistPeerConfig.recovery_length_min = rPeerConfig.recovery_length_min;
    mRistPeerConfig.recovery_length_max = rPeerConfig.recovery_length_max;
    mRistPeerConfig.recover_reorder_buffer = rPeerConfig.recover_reorder_buffer;
    mRistPeerConfig.recovery_rtt_min = rPeerConfig.recovery_rtt_min;
    mRistPeerConfig.recovery_rtt_max = rPeerConfig.recovery_rtt_max;
    mRistPeerConfig.weight = lWeight;
    mRistPeerConfig.bufferbloat_mode = rPeerConfig.bufferbloat_mode;
    mRistPeerConfig.bufferbloat_limit = rPeerConfig.bufferbloat_limit;
    mRistPeerConfig.bufferbloat_hard_limit = rPeerConfig.bufferbloat_hard_limit;

    struct rist_peer *pPeer;
    lStatus = rist_client_add_peer(mRistSender, &mRistPeerConfig, &pPeer);
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_client_add_peer fail.")
      destroySender();
      return false;
    }
  }

  lStatus = rist_client_set_keepalive_timeout(mRistSender, keepAlive);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_client_set_keepalive_timeout fail.")
    destroySender();
    return false;
  }

  lStatus = rist_client_set_session_timeout(mRistSender, timeOut);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_client_set_session_timeout fail.")
    destroySender();
    return false;
  }

  lStatus = rist_client_oob_enable(mRistSender, receiveData, this);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_client_oob_enable fail.")
    destroySender();
    return false;
  }

  lStatus = rist_client_start(mRistSender);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_client_start fail.")
    destroySender();
    return false;
  }
  return true;
}

bool RISTNetSender::sendData(const uint8_t *pData, size_t size) {
  if (!mRistSender) {
    LOGGER(true, LOGG_ERROR, "RISTNetSender not initialised.")
    return false;
  }
  int lStatus = rist_client_write(mRistSender,pData,size,0,0);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_client_write failed.")
    destroySender();
    return false;
  }
  return true;
}
