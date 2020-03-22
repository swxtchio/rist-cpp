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
                    rist_peer_config &rPeerConfig, enum rist_log_level logLevel = RIST_LOG_QUIET);

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
   * @brief Destroys the receiver
   *
   * Destroys the receiver and garbage collects all underlying assets.
   *
   */
  bool destroyReceiver();

  /**
   * @brief Data receive callback
   *
   * When receiving data from the sender this function is called.
   * You get a pointer to the data, the length and the NetworkConnection object containing your
   * object if you did put a object there.
   *
   * @param function getting data from the sender.
   */
  std::function<void(const uint8_t *pBuf, size_t len, std::shared_ptr<NetworkConnection> &rConnection)>
      networkDataCallback = nullptr;

  /**
   * @brief Validate connection callback
   *
   * If the reciever is in listen mode and a sender connrcts this method is called
   * Return a NetworkConnection if you want to accept this connection
   * You can attach any object to the NetworkConnection and the NetworkConnection object
   * will manage your objects lifecycle. Meaning it will release it when the connection
   * is terminated.
   *
   * @param function validating the connection.
   * @return a NetworkConnection object.
   */
  std::function<std::shared_ptr<NetworkConnection>(std::string ipAddress, uint16_t port)>
      validateConnectionCallback = nullptr;

  // Delete copy and move constructors and assign operators
  RISTNetReceiver(RISTNetReceiver const &) = delete;             // Copy construct
  RISTNetReceiver(RISTNetReceiver &&) = delete;                  // Move construct
  RISTNetReceiver &operator=(RISTNetReceiver const &) = delete;  // Copy assign
  RISTNetReceiver &operator=(RISTNetReceiver &&) = delete;       // Move assign

private:

  std::shared_ptr<NetworkConnection> validateConnectionStub(std::string ipAddress, uint16_t port);
  void dataFromClientStub(const uint8_t *buf, size_t len, std::shared_ptr<NetworkConnection> &connection);

  // Private method receiving the data from librist C-API
  static void receiveData(void *pArg,
                          struct rist_peer *pPeer,
                          uint64_t flowID,
                          const void *pBuf,
                          size_t len,
                          uint16_t srcPort,
                          uint16_t dstPort);

  // Private method called when a client connects
  static int clientConnect(void *pArg,
                           char *pConnectingIP,
                           uint16_t connectingPort,
                           char *pLocalIP,
                           uint16_t localPort,
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
                  rist_peer_config &rPeerConfig,
                  enum rist_log_level logLevel = RIST_LOG_QUIET,
                  enum rist_profile profile = RIST_PROFILE_MAIN,
                  uint32_t keepAlive = 5000,
                  uint32_t timeOut = 10000);

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


  bool sendData(const uint8_t *pData, size_t size);

  /**
   * @brief Destroys the sender
   *
   * Destroys the sender and garbage collects all underlying assets.
   *
   */
  bool destroySender();

  std::function<void(const uint8_t *pBuf, size_t len, std::shared_ptr<NetworkConnection> &connection)>
      networkDataCallback = nullptr;
  std::function<std::shared_ptr<NetworkConnection>(std::string ipAddress, uint16_t port)>
      validateConnectionCallback = nullptr;

  // Delete copy and move constructors and assign operators
  RISTNetSender(RISTNetSender const &) = delete;             // Copy construct
  RISTNetSender(RISTNetSender &&) = delete;                  // Move construct
  RISTNetSender &operator=(RISTNetSender const &) = delete;  // Copy assign
  RISTNetSender &operator=(RISTNetSender &&) = delete;       // Move assign

private:

  std::shared_ptr<NetworkConnection> validateConnectionStub(std::string ipAddress, uint16_t port);
  void dataFromClientStub(const uint8_t *buf, size_t len, std::shared_ptr<NetworkConnection> &connection);

  // Private method receiving the data from librist C-API
  static void receiveData(void *pArg, struct rist_peer *pPeer, const void *pBuffer, size_t len);

  // Private method called when a client connects
  static int clientConnect(void *pArg,
                           char *pConnectingIP,
                           uint16_t connectingPort,
                           char *pLocalIP,
                           uint16_t localPort,
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
};

#endif //CPPRISTWRAPPER__RISTNET_H
