//
// Created by Anders Cedronius (Edgeware AB) on 2020-03-14.
//

// Prefixes used
// m class member
// p pointer (*)
// r reference (&)
// l local scope

#ifndef CPPRISTWRAPPER__RISTNET_H
#define CPPRISTWRAPPER__RISTNET_H

#define CPP_WRAPPER_VERSION 20

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

 /**
  * \class NetworkConnection
  *
  * \brief
  *
  * A NetworkConnection class is the maintainer and carrier of the user class passed to the connection.
  *
  */
class NetworkConnection {
public:
  std::any mObject; //Contains your object
};

/**
 * \class RISTNetTools
 *
 * \brief
 *
 * A helper class for the RIST C++ wrapper
 *
 */
class RISTNetTools {
public:
  ///Build the librist url based on name/ip, port and if it's a listen or not peer
  bool buildRISTURL(std::string lIP, std::string lPort, std::string &rURL, bool lListen);
private:
  bool isIPv4(const std::string &rStr);
  bool isIPv6(const std::string &rStr);
};

//---------------------------------------------------------------------------------------------------------------------
//
//
// RISTNetReceiver  --  RECEIVER
//
//
//---------------------------------------------------------------------------------------------------------------------

 /**
  * \class RISTNetReceiver
  *
  * \brief
  *
  * A RISTNetReceiver receives data from a sender. The reciever can listen for a sender to connect
  * or connect to a sender that listens.
  *
  */
class RISTNetReceiver {
public:

  struct RISTNetReceiverSettings {
    enum rist_profile mProfile = RIST_PROFILE_MAIN;
    rist_peer_config mPeerConfig = {0};
    enum rist_log_level mLogLevel = RIST_LOG_QUIET;
    std::string mPSK = "";
    int mSessionTimeout = 0;
    int mKeepAliveTimeout = 0;
    int mMaxjitter = 0;
  };

  /// Constructor
  RISTNetReceiver();

  /// Destructor
  virtual ~RISTNetReceiver();

  /**
   * @brief Initialize receiver
   *
   * Initialize the receiver using the provided settings and parameters.
   *
   * @param rInterfaceList is a vector if interfaces containing a map of ip/port/mode (mode true == listen)
   * @param The peer configuration
   * @param loglevel Level of log messages to display
   * @return true on success
   */
  bool initReceiver(std::vector<std::tuple<std::string, std::string, bool>> &rInterfaceList,
                    RISTNetReceiverSettings &rSettings);

  /**
   * @brief Map of all active connections
   *
   * Get a map of all connected clients
   *
   * @param function getting the map of active clients (normally a lambda).
   */
  void getActiveClients(std::function<void(std::map<struct rist_peer *,
                                                    std::shared_ptr<NetworkConnection>> &)> function);

  /**
   * @brief Close all active connections
   *
   * Closes all active connections.
   *
   */
  void closeAllClientConnections();

  /**
   * @brief Send OOB data (Currently not working in librist)
   *
   * Sends OOB data to the specified peer
   * OOB data is encrypted (if used) but not protected for network loss
   *
   * @param target peer
   * @param pointer to the data
   * @param length of the data
   *
   */
  bool sendOOBData(struct rist_peer *pPeer ,const uint8_t *pData, size_t lSize);

  /**
   * @brief Destroys the receiver
   *
   * Destroys the receiver and garbage collects all underlying assets.
   *
   */
  bool destroyReceiver();

  /**
   * @brief Gets the version
   *
   * Gets the version of the C++ librist wrapper sender and librist
   *
   * @return The cpp wrapper version, rist major and minor version.
   */
  void getVersion(uint32_t &rCppWrapper, uint32_t &rRistMajor, uint32_t &rRistMinor);

  //To be implemented
  //void getInfo();

  /**
   * @brief Data receive callback
   *
   * When receiving data from the sender this function is called.
   * You get a pointer to the data, the length and the NetworkConnection object containing your
   * object if you did put a object there.
   *
   * @param function getting data from the sender.
   */
  std::function<void(const uint8_t *pBuf, size_t lSize, std::shared_ptr<NetworkConnection> &rConnection, struct rist_peer *pPeer)>
      networkDataCallback = nullptr;

  /**
   * @brief OOB Data receive callback (__NULLABLE)
   *
   * When receiving data from the sender this function is called.
   * You get a pointer to the data, the length and the NetworkConnection object containing your
   * object if you did put a object there.
   *
   * @param function getting data from the sender.
   */
  std::function<void(const uint8_t *pBuf, size_t lSize, std::shared_ptr<NetworkConnection> &rConnection, struct rist_peer *pPeer)>
      networkOOBDataCallback = nullptr;

  /**
   * @brief Validate connection callback
   *
   * If the reciever is in listen mode and a sender connects this method is called
   * Return a NetworkConnection if you want to accept this connection
   * You can attach any object to the NetworkConnection and the NetworkConnection object
   * will manage your objects lifecycle. Meaning it will release it when the connection
   * is terminated.
   *
   * @param function validating the connection.
   * @return a NetworkConnection object or nullptr for rejecting.
   */
  std::function<std::shared_ptr<NetworkConnection>(std::string lIPAddress, uint16_t lPort)>
      validateConnectionCallback = nullptr;

  // Delete copy and move constructors and assign operators
  RISTNetReceiver(RISTNetReceiver const &) = delete;             // Copy construct
  RISTNetReceiver(RISTNetReceiver &&) = delete;                  // Move construct
  RISTNetReceiver &operator=(RISTNetReceiver const &) = delete;  // Copy assign
  RISTNetReceiver &operator=(RISTNetReceiver &&) = delete;       // Move assign

private:

  std::shared_ptr<NetworkConnection> validateConnectionStub(std::string lIPAddress, uint16_t lPort);
  void dataFromClientStub(const uint8_t *pBuf, size_t lSize, std::shared_ptr<NetworkConnection> &rConnection);

  // Private method receiving the data from librist C-API
  static void receiveData(void *pArg,
                          struct rist_peer *pPeer,
                          uint64_t lFlowID,
                          const void *pBuf,
                          size_t lSize,
                          uint16_t lSrcPort,
                          uint16_t lDstPort);

  // Private method receiving OOB data from librist C-API
  static void receiveOOBData(void *pArg, struct rist_peer *pPeer, const void *pBuffer, size_t lSize);

  // Private method called when a client connects
  static int clientConnect(void *pArg,
                           char *pConnectingIP,
                           uint16_t lConnectingPort,
                           char *pLocalIP,
                           uint16_t lLocalPort,
                           struct rist_peer *pPeer);

  // Private method called when a client disconnects
  static void clientDisconnect(void *pArg, struct rist_peer *pPeer);

  // The context of a RIST receiver
  rist_server *mRistReceiver = nullptr;

  // The configuration of the RIST receiver
  rist_peer_config mRistPeerConfig = {0};

  // The mutex protecting the list. since the list can be accessed from both librist and the C++ layer
  std::mutex mClientListMtx;

  // The list of connected clients
  std::map<struct rist_peer *, std::shared_ptr<NetworkConnection>> mClientList;

  // Internal tools used by the C++ wrapper
  RISTNetTools mNetTools = RISTNetTools();
};

//---------------------------------------------------------------------------------------------------------------------
//
//
// RISTNetSender  --  SENDER
//
//
//---------------------------------------------------------------------------------------------------------------------

/**
 * \class RISTNetSender
 *
 * \brief
 *
 * A RISTNetSender sends data to a receiver. The sender can listen for a receiver to connect
 * or connect to a receiver that listens.
 *
 */
class RISTNetSender {
public:

  struct RISTNetSenderSettings {
    enum rist_profile mProfile = RIST_PROFILE_MAIN;
    rist_peer_config mPeerConfig = {0};
    enum rist_log_level mLogLevel = RIST_LOG_QUIET;
    std::string mPSK = "";
    uint32_t mSessionTimeout = 0;
    uint32_t mKeepAliveTimeout = 0;
    int mMaxJitter = 0;
   };

  /// Constructor
  RISTNetSender();

  /// Destructor
  virtual ~RISTNetSender();

  /**
   * @brief Initialize sender
   *
   * Initialize the sender using the provided settings and parameters.
   *
   * @param WIP.. is about to change
   * @return true on success
   */
  bool initSender(std::vector<std::tuple<std::string, std::string, uint32_t, bool>> &rPeerList,
                  RISTNetSenderSettings &rSettings);

  /**
   * @brief Map of all active connections
   *
   * Get a map of all connected clients
   *
   * @param function getting the map of active clients (normally a lambda).
   */
  void getActiveClients(std::function<void(std::map<struct rist_peer *,
                                                    std::shared_ptr<NetworkConnection>> &)> function);

  /**
   * @brief Close all active connections
   *
   * Closes all active connections.
   *
   */
  void closeAllClientConnections();

  /**
   * @brief Send data
   *
   * Sends data to the connected peers
   *
   * @param pointer to the data
   * @param length of the data
   *
   */
  bool sendData(const uint8_t *pData, size_t lSize);

  /**
  * @brief Send OOB data (Currently not working in librist)
  *
  * Sends OOB data to the specified peer
  * OOB data is encrypted (if used) but not protected for network loss
  *
  * @param target peer
  * @param pointer to the data
  * @param length of the data
  *
  */
  bool sendOOBData(struct rist_peer *pPeer ,const uint8_t *pData, size_t lSize);

  /**
   * @brief Destroys the sender
   *
   * Destroys the sender and garbage collects all underlying assets.
   *
   */
  bool destroySender();

  /**
   * @brief Gets the version
   *
   * Gets the version of the C++ librist wrapper sender and librist
   *
   * @return The cpp wrapper version, rist major and minor version.
   */
   void getVersion(uint32_t &rCppWrapper, uint32_t &rRistMajor, uint32_t &rRistMinor);

  //To be implemented
  //void getInfo();

  /**
   * @brief OOB Data receive callback (__NULLABLE)
   *
   * When receiving data from the sender this function is called.
   * You get a pointer to the data, the length and the NetworkConnection object containing your
   * object if you did put a object there.
   *
   * @param function getting data from the sender.
   */
  std::function<void(const uint8_t *pBuf, size_t lSize, std::shared_ptr<NetworkConnection> &rConnection, struct rist_peer *pPeer)>
      networkOOBDataCallback = nullptr;

  /**
   * @brief Validate connection callback
   *
   * If the sender is in listen mode and a receiver connects this method is called
   * Return a NetworkConnection if you want to accept this connection
   * You can attach any object to the NetworkConnection and the NetworkConnection object
   * will manage your objects lifecycle. Meaning it will release it when the connection
   * is terminated.
   *
   * @param function validating the connection.
   * @return a NetworkConnection object or nullptr for rejecting.
   */
  std::function<std::shared_ptr<NetworkConnection>(std::string lIPAddress, uint16_t lPort)>
      validateConnectionCallback = nullptr;

  // Delete copy and move constructors and assign operators
  RISTNetSender(RISTNetSender const &) = delete;             // Copy construct
  RISTNetSender(RISTNetSender &&) = delete;                  // Move construct
  RISTNetSender &operator=(RISTNetSender const &) = delete;  // Copy assign
  RISTNetSender &operator=(RISTNetSender &&) = delete;       // Move assign

private:

  std::shared_ptr<NetworkConnection> validateConnectionStub(std::string ipAddress, uint16_t port);
  void dataFromClientStub(const uint8_t *pBuf, size_t lSize, std::shared_ptr<NetworkConnection> &rConnection);

  // Private method receiving OOB data from librist C-API
  static void receiveOOBData(void *pArg, struct rist_peer *pPeer, const void *pBuffer, size_t lSize);

  // Private method called when a client connects
  static int clientConnect(void *pArg,
                           char *pConnectingIP,
                           uint16_t lConnectingPort,
                           char *pLocalIP,
                           uint16_t lLocalPort,
                           struct rist_peer *pPeer);

  // Private method called when a client disconnects
  static void clientDisconnect(void *pArg, struct rist_peer *pPeer);

  // The context of a RIST sender
  rist_client *mRistSender = nullptr;

  // The configuration of the RIST sender
  rist_peer_config mRistPeerConfig = {0};

  // The mutex protecting the list. since the list can be accessed from both librist and the C++ layer
  std::mutex mClientListMtx;

  // The list of connected clients
  std::map<struct rist_peer *, std::shared_ptr<NetworkConnection>> mClientList;

  // Internal tools used by the C++ wrapper
  RISTNetTools mNetTools = RISTNetTools();
};

#endif //CPPRISTWRAPPER__RISTNET_H
