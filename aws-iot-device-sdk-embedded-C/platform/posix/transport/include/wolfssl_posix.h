/*
 * AWS IoT Device SDK for Embedded C V202011.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef WOLFSSL_POSIX_H_
#define WOLFSSL_POSIX_H_

/**************************************************/
/******* DO NOT CHANGE the following order ********/
/**************************************************/

/* Logging related header files are required to be included in the following order:
 * 1. Include the header file "logging_levels.h".
 * 2. Define LIBRARY_LOG_NAME and  LIBRARY_LOG_LEVEL.
 * 3. Include the header file "logging_stack.h".
 */

/* Include header that defines log levels. */
#include "logging_levels.h"

/* Logging configuration for the transport interface implementation which uses
 * WolfSSL and Sockets. */
#ifndef LIBRARY_LOG_NAME
    #define LIBRARY_LOG_NAME     "Transport_WolfSSL_Sockets"
#endif
#ifndef LIBRARY_LOG_LEVEL
    #define LIBRARY_LOG_LEVEL    LOG_INFO
#endif

#include "logging_stack.h"

/************ End of logging configuration ****************/

/* WolfSSL include. */
#include <wolfssl/ssl.h>

/* Transport includes. */
#include "transport_interface.h"

/* Socket include. */
#include "sockets_posix.h"

/**
 * @brief Definition of the network context for the transport interface
 * implementation that uses WolfSSL and POSIX sockets.
 *
 * @note For this transport implementation, the socket descriptor and
 * SSL context is used.
 */
struct NetworkContext
{
    int32_t socketDescriptor;
    WOLFSSL * pSsl;
};

/**
 * @brief WolfSSL Connect / Disconnect return status.
 */
typedef enum WolfsslStatus
{
    WOLFSSL_SUCCEED = 0,         /**< Function successfully completed. */
    WOLFSSL_INVALID_PARAMETER,   /**< At least one parameter was invalid. */
    WOLFSSL_INSUFFICIENT_MEMORY, /**< Insufficient memory required to establish connection. */
    WOLFSSL_INVALID_CREDENTIALS, /**< Provided credentials were invalid. */
    WOLFSSL_HANDSHAKE_FAILED,    /**< Performing TLS handshake with server failed. */
    WOLFSSL_API_ERROR,           /**< A call to a system API resulted in an internal error. */
    WOLFSSL_DNS_FAILURE,         /**< Resolving hostname of the server failed. */
    WOLFSSL_CONNECT_FAILURE      /**< Initial connection to the server failed. */
} WolfsslStatus_t;

/**
 * @brief Contains the credentials to establish a TLS connection.
 */
typedef struct WolfsslCredentials
{
    /**
     * @brief An array of ALPN protocols. Set to NULL to disable ALPN.
     *
     * See [this link]
     * (https://aws.amazon.com/blogs/iot/mqtt-with-tls-client-authentication-on-port-443-why-it-is-useful-and-how-it-works/)
     * for more information.
     */
    const char * pAlpnProtos;

    /**
     * @brief Length of the ALPN protocols array.
     */
    uint32_t alpnProtosLen;

    /**
     * @brief Set a host name to enable SNI. Set to NULL to disable SNI.
     *
     * @note This string must be NULL-terminated.
     */
    const char * sniHostName;

    /**
     * @brief Set the value for the TLS max fragment length (TLS MFLN)
     *
     * WolfSSL allows this value to be in the range of:
     * 
     *      WOLFSSL_MFL_2_9  = 1, //  512 bytes
     *      WOLFSSL_MFL_2_10 = 2, // 1024 bytes
     *      WOLFSSL_MFL_2_11 = 3, // 2048 bytes
     *      WOLFSSL_MFL_2_12 = 4, // 4096 bytes
     *      WOLFSSL_MFL_2_13 = 5, // 8192 bytes wolfSSL ONLY!!!
     *      WOLFSSL_MFL_2_8 = 6,  //  256 bytes wolfSSL ONLY!!!

     * @note By setting this to other value, WolfSSL uses the default value,
     * which is 16384.
     */
    uint8_t maxFragmentLength;

    /**
     * @brief Filepaths to certificates and private key that are used when
     * performing the TLS handshake.
     *
     * @note These strings must be NULL-terminated.
     */
    const char * pRootCaPath;     /**< @brief Filepath string to the trusted server root CA. */
    const char * pClientCertPath; /**< @brief Filepath string to the client certificate. */
    const char * pPrivateKeyPath; /**< @brief Filepath string to the client certificate's private key. */
} WolfsslCredentials_t;

/**
 * @brief Sets up a TLS session on top of a TCP connection using the WolfSSL API.
 *
 * @param[out] pNetworkContext The output parameter to return the created network context.
 * @param[in] pServerInfo Server connection info.
 * @param[in] pWolfsslCredentials Credentials for the TLS connection.
 * @param[in] sendTimeoutMs Timeout for transport send.
 * @param[in] recvTimeoutMs Timeout for transport recv.
 *
 * @note A timeout of 0 means infinite timeout.
 *
 * @return #WOLFSSL_SUCCESS on success;
 * #WOLFSSL_INVALID_PARAMETER, #WOLFSSL_INVALID_CREDENTIALS,
 * #WOLFSSL_INVALID_CREDENTIALS, #WOLFSSL_SYSTEM_ERROR on failure.
 */
WolfsslStatus_t Wolfssl_Connect( NetworkContext_t * pNetworkContext,
                                 const ServerInfo_t * pServerInfo,
                                 const WolfsslCredentials_t * pWolfsslCredentials,
                                 uint32_t sendTimeoutMs,
                                 uint32_t recvTimeoutMs );

/**
 * @brief Closes a TLS session on top of a TCP connection using the WolfSSL API.
 *
 * @param[out] pNetworkContext The output parameter to end the TLS session and
 * clean the created network context.
 *
 * @return #WOLFSSL_SUCCESS on success; #WOLFSSL_INVALID_PARAMETER on failure.
 */
WolfsslStatus_t Wolfssl_Disconnect( const NetworkContext_t * pNetworkContext );

/**
 * @brief Receives data over an established TLS session using the WolfSSL API.
 *
 * This can be used as #TransportInterface.recv function for receiving data
 * from the network.
 *
 * @param[in] pNetworkContext The network context created using Wolfssl_Connect API.
 * @param[out] pBuffer Buffer to receive network data into.
 * @param[in] bytesToRecv Number of bytes requested from the network.
 *
 * @return Number of bytes received if successful; negative value on error.
 */
int32_t Wolfssl_Recv( NetworkContext_t * pNetworkContext,
                      void * pBuffer,
                      size_t bytesToRecv );

/**
 * @brief Sends data over an established TLS session using the WolfSSL API.
 *
 * This can be used as the #TransportInterface.send function to send data
 * over the network.
 *
 * @param[in] pNetworkContext The network context created using Wolfssl_Connect API.
 * @param[in] pBuffer Buffer containing the bytes to send over the network stack.
 * @param[in] bytesToSend Number of bytes to send over the network.
 *
 * @return Number of bytes sent if successful; negative value on error.
 */
int32_t Wolfssl_Send( NetworkContext_t * pNetworkContext,
                      const void * pBuffer,
                      size_t bytesToSend );

#endif /* ifndef WOLFSSL_POSIX_H_ */
