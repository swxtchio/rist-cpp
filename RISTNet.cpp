//
// Created by Anders Cedronius (Edgeware AB) on 2020-03-14.
//

#include "RISTNet.h"
#include "RISTNetInternal.h"

//---------------------------------------------------------------------------------------------------------------------
//
//
// RIST Network tools
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

bool RISTNetTools::buildRISTURL(std::string lIP, std::string lPort, std::string &rURL, bool lListen) {
  int lIPType;
  if (isIPv4(lIP)) {
    lIPType = AF_INET;
  } else if (isIPv6(lIP)) {
    lIPType = AF_INET6;
  } else {
    LOGGER(true, LOGG_ERROR, " " << "Provided IP-Address not valid.")
    return false;
  }
  int lPortNum = 0;
  std::stringstream lPortNumStr(lPort);
  lPortNumStr >> lPortNum;
  if (lPortNum < 1 || lPortNum > INT16_MAX) {
    LOGGER(true, LOGG_ERROR, " " << "Provided Port number not valid.")
    return false;
  }
  std::string lRistURL = "";
  if (lIPType == AF_INET) {
    lRistURL += "rist://";
  } else {
    lRistURL += "rist6://";
  }
  if (lListen) {
    lRistURL += "@";
  }
  if (lIPType == AF_INET) {
    lRistURL += lIP + ":" + lPort;
  } else {
    lRistURL += "[" + lIP + "]:" + lPort;
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
  // Set the callback stubs
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


std::shared_ptr<NetworkConnection> RISTNetReceiver::validateConnectionStub(std::string lIPAddress, uint16_t lPort) {
  LOGGER(true, LOGG_ERROR, "validateConnectionCallback not implemented. Will not accept connection from: " << lIPAddress << ":" << unsigned(lPort))
  return nullptr;
}

void RISTNetReceiver::dataFromClientStub(const uint8_t *pBuf, size_t lSize, std::shared_ptr<NetworkConnection> &rConnection) {
  LOGGER(true, LOGG_ERROR, "networkDataCallback not implemented. Data is lost")
}

void RISTNetReceiver::receiveData(void *pArg, struct rist_peer *pPeer, uint64_t lFlowID, const void *pBuf, size_t lSize, uint16_t lSrcPort, uint16_t lDstPort) {
  RISTNetReceiver *lWeakSelf = (RISTNetReceiver *) pArg;
  lWeakSelf -> mClientListMtx.lock();
  auto netObj = lWeakSelf -> mClientList.find(pPeer);
  if (netObj != lWeakSelf -> mClientList.end())
  {
    auto netCon = netObj -> second;
    lWeakSelf -> mClientListMtx.unlock();
    lWeakSelf -> networkDataCallback((const uint8_t *) pBuf, lSize, netCon, pPeer);
    return;
  } else {
    LOGGER(true, LOGG_ERROR, "receiveData mClientList <-> peer mismatch.")
  }
  lWeakSelf -> mClientListMtx.unlock();
}

void RISTNetReceiver::receiveOOBData(void *pArg, struct rist_peer *pPeer, const void *pBuffer, size_t lSize) {
  RISTNetReceiver *lWeakSelf = (RISTNetReceiver *) pArg;
  if (lWeakSelf -> networkOOBDataCallback) {  //This is a optional callback
    lWeakSelf->mClientListMtx.lock();
    auto netObj = lWeakSelf->mClientList.find(pPeer);
    if (netObj != lWeakSelf->mClientList.end()) {
      auto netCon = netObj->second;
      lWeakSelf->mClientListMtx.unlock();
      lWeakSelf->networkOOBDataCallback((const uint8_t *) pBuffer, lSize, netCon, pPeer);
      return;
    }
    lWeakSelf->mClientListMtx.unlock();
  }
}


int RISTNetReceiver::clientConnect(void *pArg, char *pConnectingIP, uint16_t lConnectingPort, char *pLocalIP, uint16_t lLocalPort, struct rist_peer *pPeer) {
  RISTNetReceiver *lWeakSelf = (RISTNetReceiver *) pArg;
  auto lNetObj = lWeakSelf -> validateConnectionCallback(std::string(pConnectingIP), lConnectingPort);
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

void RISTNetReceiver::getActiveClients(std::function<void(std::map<struct rist_peer *, std::shared_ptr<NetworkConnection>> &)> lFunction) {
  mClientListMtx.lock();
  if (lFunction) {
    lFunction(mClientList);
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
                                   RISTNetReceiver::RISTNetReceiverSettings &rSettings) {
  if (!rInterfaceList.size()) {
    LOGGER(true, LOGG_ERROR, "Interface list is empty.")
    return false;
  }

  int lStatus;
  lStatus = rist_server_create(&mRistReceiver, rSettings.mProfile);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_server_create fail.")
    return false;
  }

  mRistPeerConfig.recovery_mode = rSettings.mPeerConfig.recovery_mode;
  mRistPeerConfig.recovery_maxbitrate = rSettings.mPeerConfig.recovery_maxbitrate;
  mRistPeerConfig.recovery_maxbitrate_return = rSettings.mPeerConfig.recovery_maxbitrate_return;
  mRistPeerConfig.recovery_length_min = rSettings.mPeerConfig.recovery_length_min;
  mRistPeerConfig.recovery_length_max = rSettings.mPeerConfig.recovery_length_max;
  mRistPeerConfig.recover_reorder_buffer = rSettings.mPeerConfig.recover_reorder_buffer;
  mRistPeerConfig.recovery_rtt_min = rSettings.mPeerConfig.recovery_rtt_min;
  mRistPeerConfig.recovery_rtt_max = rSettings.mPeerConfig.recovery_rtt_max;
  mRistPeerConfig.weight = 5;
  mRistPeerConfig.bufferbloat_mode = rSettings.mPeerConfig.bufferbloat_mode;
  mRistPeerConfig.bufferbloat_limit = rSettings.mPeerConfig.bufferbloat_limit;
  mRistPeerConfig.bufferbloat_hard_limit = rSettings.mPeerConfig.bufferbloat_hard_limit;

  lStatus = rist_server_init(mRistReceiver, &mRistPeerConfig, rSettings.mLogLevel, clientConnect, clientDisconnect, this);
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
      destroyReceiver();
      return false;
    }
    lStatus = rist_server_add_peer(mRistReceiver, lURL.c_str());
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_server_add_peer fail.")
      destroyReceiver();
      return false;
    }
  }

  if (rSettings.mPSK.size()) {
    lStatus = rist_server_encrypt_enable(mRistReceiver, rSettings.mPSK.c_str(), rSettings.mPSK.size());
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_server_encrypt_enable fail.")
      destroyReceiver();
      return false;
    }
  }

  if (rSettings.mSessionTimeout) {
    lStatus = rist_server_set_session_timeout(mRistReceiver, rSettings.mSessionTimeout);
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_server_set_session_timeout fail.")
      destroyReceiver();
      return false;
    }
  }

  if (rSettings.mKeepAliveTimeout) {
    lStatus = rist_server_set_keepalive_timeout(mRistReceiver, rSettings.mKeepAliveTimeout);
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_server_set_session_timeout fail.")
      destroyReceiver();
      return false;
    }
  }

  if (rSettings.mMaxjitter) {
    lStatus = rist_server_set_max_jitter(mRistReceiver, rSettings.mMaxjitter);
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_server_set_max_jitter fail.")
      destroyReceiver();
      return false;
    }
  }

  lStatus = rist_server_oob_enable(mRistReceiver, receiveOOBData, this);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_server_oob_enable fail.")
    destroyReceiver();
    return false;
  }

  lStatus = rist_server_start(mRistReceiver, receiveData, this);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_server_init fail.")
    destroyReceiver();
    return false;
  }
  return true;
}

bool RISTNetReceiver::sendOOBData(struct rist_peer *pPeer ,const uint8_t *pData, size_t lSize) {
  if (!mRistReceiver) {
    LOGGER(true, LOGG_ERROR, "RISTNetReceiver not initialised.")
    return false;
  }
  int lStatus = rist_server_write_oob(mRistReceiver, pPeer, pData, lSize);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_server_write_oob failed.")
    destroyReceiver();
    return false;
  }
  return true;
}

void RISTNetReceiver::getVersion(uint32_t &rCppWrapper, uint32_t &rRistMajor, uint32_t &rRistMinor) {
  rCppWrapper = CPP_WRAPPER_VERSION;
  rRistMajor = RIST_PROTOCOL_VERSION;
  rRistMinor = RIST_SUBVERSION;
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
  LOGGER(true, LOGG_ERROR, "validateConnectionCallback not implemented. Will not accept connection from: " << ipAddress << ":" << unsigned(port))
  return 0;
}

void RISTNetSender::receiveOOBData(void *pArg, struct rist_peer *pPeer, const void *pBuffer, size_t lSize) {
  RISTNetSender *lWeakSelf = (RISTNetSender *) pArg;
  if (lWeakSelf -> networkOOBDataCallback) {  //This is a optional callback
    lWeakSelf->mClientListMtx.lock();
    auto netObj = lWeakSelf->mClientList.find(pPeer);
    if (netObj != lWeakSelf->mClientList.end()) {
      auto netCon = netObj->second;
      lWeakSelf->mClientListMtx.unlock();
      lWeakSelf->networkOOBDataCallback((const uint8_t *) pBuffer, lSize, netCon, pPeer);
      return;
    }
    lWeakSelf->mClientListMtx.unlock();
  }
}

int RISTNetSender::clientConnect(void *pArg, char *pConnectingIP, uint16_t lConnectingPort, char *pLocalIP, uint16_t lLocalPort, struct rist_peer *pPeer) {
  RISTNetSender *lWeakSelf = (RISTNetSender *) pArg;
  auto lNetObj = lWeakSelf -> validateConnectionCallback(std::string(pConnectingIP), lConnectingPort);
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

void RISTNetSender::getActiveClients(std::function<void(std::map<struct rist_peer *, std::shared_ptr<NetworkConnection>> &)> lFunction) {
  mClientListMtx.lock();
  if (lFunction) {
    lFunction(mClientList);
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
                               RISTNetSenderSettings &rSettings){
  if (!rPeerList.size()) {
    LOGGER(true, LOGG_ERROR, "list of servers is empty.")
    return false;
  }

  int lStatus;
  lStatus = rist_client_create(&mRistSender, rSettings.mProfile);
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

  lStatus = rist_client_init(mRistSender, lAdvFlowID, rSettings.mLogLevel, clientConnect, clientDisconnect, this);
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
    mRistPeerConfig.localport = rSettings.mPeerConfig.localport;
    mRistPeerConfig.recovery_mode = rSettings.mPeerConfig.recovery_mode;
    mRistPeerConfig.recovery_maxbitrate = rSettings.mPeerConfig.recovery_maxbitrate;
    mRistPeerConfig.recovery_maxbitrate_return = rSettings.mPeerConfig.recovery_maxbitrate_return;
    mRistPeerConfig.recovery_length_min = rSettings.mPeerConfig.recovery_length_min;
    mRistPeerConfig.recovery_length_max = rSettings.mPeerConfig.recovery_length_max;
    mRistPeerConfig.recover_reorder_buffer = rSettings.mPeerConfig.recover_reorder_buffer;
    mRistPeerConfig.recovery_rtt_min = rSettings.mPeerConfig.recovery_rtt_min;
    mRistPeerConfig.recovery_rtt_max = rSettings.mPeerConfig.recovery_rtt_max;
    mRistPeerConfig.weight = lWeight;
    mRistPeerConfig.bufferbloat_mode = rSettings.mPeerConfig.bufferbloat_mode;
    mRistPeerConfig.bufferbloat_limit = rSettings.mPeerConfig.bufferbloat_limit;
    mRistPeerConfig.bufferbloat_hard_limit = rSettings.mPeerConfig.bufferbloat_hard_limit;

    struct rist_peer *pPeer;
    lStatus = rist_client_add_peer(mRistSender, &mRistPeerConfig, &pPeer);
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_client_add_peer fail.")
      destroySender();
      return false;
    }
  }

  if (rSettings.mPSK.size()) {
    lStatus = rist_client_encrypt_enable(mRistSender, rSettings.mPSK.c_str(), rSettings.mPSK.size());
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_client_encrypt_enable fail.")
      destroySender();
      return false;
    }
  }

  if (rSettings.mSessionTimeout) {
    lStatus = rist_client_set_session_timeout(mRistSender, rSettings.mSessionTimeout);
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_client_set_session_timeout fail.")
      destroySender();
      return false;
    }
  }

  if (rSettings.mKeepAliveTimeout) {
    lStatus = rist_client_set_keepalive_timeout(mRistSender, rSettings.mKeepAliveTimeout);
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_client_set_keepalive_timeout fail.")
      destroySender();
      return false;
    }
  }

  if (rSettings.mMaxJitter) {
    lStatus = rist_client_set_max_jitter(mRistSender, rSettings.mMaxJitter);
    if (lStatus) {
      LOGGER(true, LOGG_ERROR, "rist_client_set_max_jitter fail.")
      destroySender();
      return false;
    }
  }

  lStatus = rist_client_oob_enable(mRistSender, receiveOOBData, this);
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

bool RISTNetSender::sendData(const uint8_t *pData, size_t lSize) {
  if (!mRistSender) {
    LOGGER(true, LOGG_ERROR, "RISTNetSender not initialised.")
    return false;
  }
  int lStatus = rist_client_write(mRistSender,pData,lSize,0,0);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_client_write failed.")
    destroySender();
    return false;
  }
  return true;
}

bool RISTNetSender::sendOOBData(struct rist_peer *pPeer ,const uint8_t *pData, size_t lSize) {
  if (!mRistSender) {
    LOGGER(true, LOGG_ERROR, "RISTNetSender not initialised.")
    return false;
  }
  int lStatus = rist_client_write_oob(mRistSender, pPeer, pData, lSize);
  if (lStatus) {
    LOGGER(true, LOGG_ERROR, "rist_client_write_oob failed.")
    destroySender();
    return false;
  }
  return true;
}

void RISTNetSender::getVersion(uint32_t &rCppWrapper, uint32_t &rRistMajor, uint32_t &rRistMinor) {
  rCppWrapper = CPP_WRAPPER_VERSION;
  rRistMajor = RIST_PROTOCOL_VERSION;
  rRistMinor = RIST_SUBVERSION;
}
