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

/* Standard includes. */
#include <assert.h>
#include <string.h>
#include <stdlib.h>

/* POSIX socket include. */
#include <unistd.h>

/* Transport interface include. */
#include "transport_interface.h"

#include "wolfssl_posix.h"

/*-----------------------------------------------------------*/

/**
 * @brief Label of root CA when calling @ref logPath.
 */
#define ROOT_CA_LABEL        "Root CA certificate"

/**
 * @brief Label of client certificate when calling @ref logPath.
 */
#define CLIENT_CERT_LABEL    "client's certificate"

/**
 * @brief Label of client key when calling @ref logPath.
 */
#define CLIENT_KEY_LABEL     "client's key"

/*-----------------------------------------------------------*/

/**
 * @brief Log the absolute path given a relative or absolute path.
 *
 * @param[in] path Relative or absolute path.
 * @param[in] fileType NULL-terminated string describing the file type to log.
 */
#if ( LIBRARY_LOG_LEVEL == LOG_DEBUG )
    static void logPath( const char * path,
                         const char * fileType );
#endif /* #if ( LIBRARY_LOG_LEVEL == LOG_DEBUG ) */

/**
 * @brief Add X509 certificate to the trusted list of root certificates.
 *
 * @param[out] pSslContext SSL context to which the trusted server root CA is to be added.
 * @param[in] pRootCaPath Filepath string to the trusted server root CA.
 *
 * @return 1 on success; -1, 0 on failure;
 */
static int32_t setRootCa( const WOLFSSL_CTX * pSslContext,
                          const char * pRootCaPath );

/**
 * @brief Set X509 certificate as client certificate for the server to authenticate.
 *
 * @param[out] pSslContext SSL context to which the client certificate is to be set.
 * @param[in] pClientCertPath Filepath string to the client certificate.
 *
 * @return 1 on success; 0 failure;
 */
static int32_t setClientCertificate( WOLFSSL_CTX * pSslContext,
                                     const char * pClientCertPath );

/**
 * @brief Set private key for the client's certificate.
 *
 * @param[out] pSslContext SSL context to which the private key is to be added.
 * @param[in] pPrivateKeyPath Filepath string to the client private key.
 *
 * @return 1 on success; 0 on failure;
 */
static int32_t setPrivateKey( WOLFSSL_CTX * pSslContext,
                              const char * pPrivateKeyPath );

/**
 * @brief Passes TLS credentials to the WolfSSL library.
 *
 * Provides the root CA certificate, client certificate, and private key to the
 * WolfSSL library. If the client certificate or private key is not NULL, mutual
 * authentication is used when performing the TLS handshake.
 *
 * @param[out] pSslContext SSL context to which the credentials are to be imported.
 * @param[in] pWolfsslCredentials TLS credentials to be imported.
 *
 * @return 1 on success; -1, 0 on failure;
 */
static int32_t setCredentials( WOLFSSL_CTX * pSslContext,
                               const WolfsslCredentials_t * pWolfsslCredentials );

/**
 * @brief Set optional configurations for the TLS connection.
 *
 * This function is used to set SNI, MFLN, and ALPN protocols.
 *
 * @param[in] pSsl SSL context to which the optional configurations are to be set.
 * @param[in] pWolfsslCredentials TLS credentials containing configurations.
 */
static void setOptionalConfigurations( WOLFSSL * pSsl,
                                       const WolfsslCredentials_t * pWolfsslCredentials );

/**
 * @brief Converts the sockets wrapper status to wolfssl status.
 *
 * @param[in] socketStatus Sockets wrapper status.
 *
 * @return #WOLFSSL_SUCCESS, #WOLFSSL_INVALID_PARAMETER, #WOLFSSL_DNS_FAILURE,
 * and #WOLFSSL_CONNECT_FAILURE.
 */
static WolfsslStatus_t convertToWolfsslStatus( SocketStatus_t socketStatus );

/*-----------------------------------------------------------*/

#if ( LIBRARY_LOG_LEVEL == LOG_DEBUG )
    static void logPath( const char * path,
                         const char * fileType )
    {
        char * cwd = NULL;

        assert( path != NULL );
        assert( fileType != NULL );

        /* Unused parameter when logs are disabled. */
        ( void ) fileType;

        /* Log the absolute directory based on first character of path. */
        if( ( path[ 0 ] == '/' ) || ( path[ 0 ] == '\\' ) )
        {
            LogDebug( ( "Attempting to open %s: Path=%s.",
                        fileType,
                        path ) );
        }
        else
        {
            cwd = getcwd( NULL, 0 );
            LogDebug( ( "Attempting to open %s: Path=%s/%s.",
                        fileType,
                        cwd,
                        path ) );
        }

        /* Free cwd because getcwd calls malloc. */
        free( cwd );
    }
#endif /* #if ( LIBRARY_LOG_LEVEL == LOG_DEBUG ) */
/*-----------------------------------------------------------*/

static WolfsslStatus_t convertToWolfsslStatus( SocketStatus_t socketStatus )
{
    WolfsslStatus_t wolfsslStatus = WOLFSSL_INVALID_PARAMETER;

    switch( socketStatus )
    {
        case SOCKETS_SUCCESS:
            wolfsslStatus = WOLFSSL_SUCCEED;
            break;

        case SOCKETS_INVALID_PARAMETER:
            wolfsslStatus = WOLFSSL_INVALID_PARAMETER;
            break;

        case SOCKETS_DNS_FAILURE:
            wolfsslStatus = WOLFSSL_DNS_FAILURE;
            break;

        case SOCKETS_CONNECT_FAILURE:
            wolfsslStatus = WOLFSSL_CONNECT_FAILURE;
            break;

        default:
            LogError( ( "Unexpected status received from socket wrapper: Socket status = %u",
                        socketStatus ) );
            break;
    }

    return wolfsslStatus;
}
/*-----------------------------------------------------------*/

static int32_t setRootCa( const WOLFSSL_CTX* pSslContext,
                          const char* pRootCaPath)
{
    int32_t sslStatus;
    int     ret;

    assert(pSslContext != NULL);
    assert(pRootCaPath != NULL);

#if ( LIBRARY_LOG_LEVEL == LOG_DEBUG )
    logPath(pRootCaPath, ROOT_CA_LABEL);
#endif

    ret = wolfSSL_CTX_load_verify_locations(pSslContext, pRootCaPath, NULL);
    if( ret != WOLFSSL_SUCCESS ) 
    {
        LogError(("Failed to import root CA"));
        sslStatus = 0;
    } 
    else 
    {
        LogDebug(("Successfully imported root CA."));
        sslStatus = 1;
    }

#if defined ( AzureSpherePlatform )
    free(pRootCaPath);
#endif

    return sslStatus;
}
/*-----------------------------------------------------------*/

static int32_t setClientCertificate( WOLFSSL_CTX * pSslContext,
                                     const char * pClientCertPath )
{
    int32_t sslStatus = 0;
    int     ret = WOLFSSL_FAILURE;

    assert( pSslContext != NULL );
    assert( pClientCertPath != NULL );

    #if ( LIBRARY_LOG_LEVEL == LOG_DEBUG )
        logPath( pClientCertPath, CLIENT_CERT_LABEL );
    #endif

    /* Import the client certificate. */
    ret = wolfSSL_CTX_use_certificate_chain_file( pSslContext,
                                                  pClientCertPath );
    if( ret != WOLFSSL_SUCCESS)
    {
        LogError(("Failed to import client certificate."));
        sslStatus = 0;
    }
    else
    {
        LogDebug(("Successfully imported client certificate."));
        sslStatus = 1;
    }

    return sslStatus;
}
/*-----------------------------------------------------------*/

static int32_t setPrivateKey( WOLFSSL_CTX * pSslContext,
                              const char * pPrivateKeyPath )
{
    int32_t sslStatus = 0;
    int     ret = WOLFSSL_FAILURE;

    assert( pSslContext != NULL );
    assert( pPrivateKeyPath != NULL );

    #if ( LIBRARY_LOG_LEVEL == LOG_DEBUG )
        logPath( pPrivateKeyPath, CLIENT_KEY_LABEL );
    #endif

    /* Import the client certificate private key. */
    ret = wolfSSL_CTX_use_PrivateKey_file( pSslContext,
                                           pPrivateKeyPath,
                                           WOLFSSL_FILETYPE_PEM);

    if (ret != WOLFSSL_SUCCESS)
    {
        LogError( ( "Failed to import client certificate private key." ) );
        sslStatus = 0;
    }
    else
    {
        LogDebug( ( "Successfully imported client certificate private key." ) );
        sslStatus = 1;
    }

#if defined ( AzureSpherePlatform )
    free(pPrivateKeyPath);
#endif

    return sslStatus;
}
/*-----------------------------------------------------------*/

static int32_t setCredentials( WOLFSSL_CTX * pSslContext,
                               const WolfsslCredentials_t * pWolfsslCredentials )
{
    int32_t sslStatus = 0;

    assert( pSslContext != NULL );
    assert( pWolfsslCredentials != NULL );

    if( pWolfsslCredentials->pRootCaPath != NULL )
    {
        sslStatus = setRootCa( pSslContext,
                               pWolfsslCredentials->pRootCaPath );
    }

    if( ( sslStatus == 1 ) &&
        ( pWolfsslCredentials->pClientCertPath != NULL ) )
    {
        sslStatus = setClientCertificate( pSslContext,
                                          pWolfsslCredentials->pClientCertPath );
    }

    if( ( sslStatus == 1 ) &&
        ( pWolfsslCredentials->pPrivateKeyPath != NULL ) )
    {
        sslStatus = setPrivateKey( pSslContext,
                                   pWolfsslCredentials->pPrivateKeyPath );
    }

    return sslStatus;
}
/*-----------------------------------------------------------*/

static void setOptionalConfigurations( WOLFSSL * pSsl,
                                       const WolfsslCredentials_t * pWolfsslCredentials)
{
    int32_t sslStatus = -1;
    int16_t readBufferLength = 0;
    int     ret = WOLFSSL_FAILURE;

    assert( pSsl != NULL );
    assert( pWolfsslCredentials != NULL );

    /* Set TLS ALPN if requested. */
    if( ( pWolfsslCredentials->pAlpnProtos != NULL ) &&
        ( pWolfsslCredentials->alpnProtosLen > 0U ) )
    {
        LogDebug( ( "Setting ALPN protos." ) );
        ret = wolfSSL_UseALPN( pSsl,
                             ( char * ) pWolfsslCredentials->pAlpnProtos,
                             ( unsigned int ) pWolfsslCredentials->alpnProtosLen,
                               WOLFSSL_ALPN_FAILED_ON_MISMATCH );

        if( ret != WOLFSSL_SUCCESS )
        {
            LogError( ( "Failed to set ALPN protos. %s",
                        pWolfsslCredentials->pAlpnProtos ) );
        }
    }

    /* Set TLS MFLN if requested. */
    if( pWolfsslCredentials->maxFragmentLength > 0U )
    {
#if !defined( AzureSpherePlatform )
        /* wolfSSL on Azure Sphere platform does not include wolfSSL_UseMaxFragment due to ABI consideration */
       
        LogDebug(("Setting max fragment length."));

        /* Set the maximum send fragment length. */

        ret = (int32_t)wolfSSL_UseMaxFragment(pSsl,
            (byte)pWolfsslCredentials->maxFragmentLength);

        if (ret != WOLFSSL_SUCCESS)
        {
            LogError(("Failed to set max send fragment length %u.",
                pWolfsslCredentials->maxFragmentLength));
        }
#endif
    }

    /* Enable SNI if requested. */
    if( pWolfsslCredentials->sniHostName != NULL )
    {
        LogDebug( ( "Setting server name for SNI." ) );

        ret = wolfSSL_UseSNI( pSsl,
                              WOLFSSL_SNI_HOST_NAME,
                              pWolfsslCredentials->sniHostName,
                              strlen( pWolfsslCredentials->sniHostName ) );

        if( sslStatus != WOLFSSL_SUCCESS )
        {
            LogError( ( "Failed to set server name %s for SNI.",
                        pWolfsslCredentials->sniHostName ) );
        }
    }
}
/*-----------------------------------------------------------*/

WolfsslStatus_t Wolfssl_Connect( NetworkContext_t * pNetworkContext,
                                 const ServerInfo_t * pServerInfo,
                                 const WolfsslCredentials_t * pWolfsslCredentials,
                                 uint32_t sendTimeoutMs,
                                 uint32_t recvTimeoutMs )
{
    SocketStatus_t socketStatus = SOCKETS_SUCCESS;
    WolfsslStatus_t returnStatus = WOLFSSL_SUCCEED;
    int32_t sslStatus = 0;
    uint8_t sslObjectCreated = 0;
    WOLFSSL_CTX *pSslContext = NULL;
    int ret = WOLFSSL_FAILURE;

    /* Validate parameters. */
    if( pNetworkContext == NULL )
    {
        LogError( ( "Parameter check failed: pNetworkContext is NULL." ) );
        returnStatus = WOLFSSL_INVALID_PARAMETER;
    }
    else if( pWolfsslCredentials == NULL )
    {
        LogError( ( "Parameter check failed: pWolfsslCredentials is NULL." ) );
        returnStatus = WOLFSSL_INVALID_PARAMETER;
    }
    else
    {
        /* Empty else. */
    }

    /* Establish the TCP connection. */
    if( returnStatus == WOLFSSL_SUCCEED)
    {
        socketStatus = Sockets_Connect( &pNetworkContext->socketDescriptor,
                                        pServerInfo,
                                        sendTimeoutMs,
                                        recvTimeoutMs );

        /* Convert socket wrapper status to wolfssl status. */
        returnStatus = convertToWolfsslStatus( socketStatus );
    }

    /* Create SSL context. */
    if( returnStatus == WOLFSSL_SUCCEED )
    {
        pSslContext = wolfSSL_CTX_new(wolfTLSv1_2_client_method() );

        if( pSslContext == NULL )
        {
            LogError( ( "Creation of a new WOLFSSL_CTX object failed." ) );
            returnStatus = WOLFSSL_API_ERROR;
        }
    }

    /* Setup credentials. */
    if( returnStatus == WOLFSSL_SUCCEED)
    {
        /* wolfSSL default is to block with blocking io and auto retry. 
         * No need for SSL_MODE_AUTO_RETRY */

        sslStatus = setCredentials( pSslContext,
                                    pWolfsslCredentials );

        if( sslStatus != 1 )
        {
            LogError( ( "Setting up credentials failed." ) );
            returnStatus = WOLFSSL_INVALID_CREDENTIALS;
        }
    }

    /* Create a new SSL session. */
    if( returnStatus == WOLFSSL_SUCCEED )
    {
        pNetworkContext->pSsl = wolfSSL_new( pSslContext );

        if( pNetworkContext->pSsl == NULL )
        {
            LogError( ( "SSL_new failed to create a new SSL context." ) );
            returnStatus = WOLFSSL_API_ERROR;
        }
        else
        {
            sslObjectCreated = 1u;
        }
    }

    /* Setup the socket to use for communication. */
    if( returnStatus == WOLFSSL_SUCCEED )
    {

#if !defined( AzureSpherePlatform )
        /* wolfSSL on Azure Sphere platform does not include wolfSSL_CTX_set_verify due to ABI consideration */

        wolfSSL_CTX_set_verify( pNetworkContext->pSsl, WOLFSSL_VERIFY_PEER, NULL );
#endif

        ret = wolfSSL_set_fd( pNetworkContext->pSsl, pNetworkContext->socketDescriptor );

        if( ret != WOLFSSL_SUCCESS )
        {
            LogError( ( "Failed to set the socket fd to SSL context." ) );
            returnStatus = WOLFSSL_API_ERROR;
        }
    }

    /* Perform the TLS handshake. */
    if( returnStatus == WOLFSSL_SUCCEED )
    {
        setOptionalConfigurations( pNetworkContext->pSsl, pWolfsslCredentials );

        ret = wolfSSL_connect( pNetworkContext->pSsl );

        if( ret != WOLFSSL_SUCCESS )
        {
            LogError( ( "Failed to perform TLS handshake." ) );
            returnStatus = WOLFSSL_HANDSHAKE_FAILED;
        }
    }

    /* Verify X509 certificate from peer. */
    if( returnStatus == WOLFSSL_SUCCEED )
    {
#if !defined( AzureSpherePlatform )
        /* wolfSSL on Azure Sphere platform does not include wolfSSL_get_verify_result due to ABI consideration */

        long verifyPeerCertStatus = wolfSSL_get_verify_result( pNetworkContext->pSsl );

        if( verifyPeerCertStatus != X509_V_OK )
        {
            LogError( ( "Failed to verify X509 certificate from peer." ) );
            returnStatus = WOLFSSL_HANDSHAKE_FAILED;
        }
#endif
    }

    /* Free the SSL context. */
    if( pSslContext != NULL )
    {
        wolfSSL_CTX_free( pSslContext );
    }

    /* Clean up on error. */
    if( ( returnStatus != WOLFSSL_SUCCEED ) && ( sslObjectCreated == 1u ) )
    {
        wolfSSL_free( pNetworkContext->pSsl );
        pNetworkContext->pSsl = NULL;
    }

    /* Log failure or success depending on status. */
    if( returnStatus != WOLFSSL_SUCCEED )
    {
        LogError( ( "Failed to establish a TLS connection." ) );
    }
    else
    {
        LogDebug( ( "Established a TLS connection." ) );
    }

    return returnStatus;
}
/*-----------------------------------------------------------*/

WolfsslStatus_t Wolfssl_Disconnect( const NetworkContext_t * pNetworkContext )
{
    SocketStatus_t socketStatus = SOCKETS_INVALID_PARAMETER;

    if( pNetworkContext == NULL )
    {
        /* No need to update the status here. The socket status
         * SOCKETS_INVALID_PARAMETER will be converted to wolfssl
         * status WOLFSSL_INVALID_PARAMETER before returning from this
         * function. */
        LogError( ( "Parameter check failed: pNetworkContext is NULL." ) );
    }
    else
    {
        if( pNetworkContext->pSsl != NULL )
        {
            /* WOLFSSL shutdown should be called twice. */
            if( wolfSSL_shutdown( pNetworkContext->pSsl ) == WOLFSSL_SHUTDOWN_NOT_DONE )
            {
                ( void ) wolfSSL_shutdown( pNetworkContext->pSsl );
            }

            wolfSSL_free( pNetworkContext->pSsl );
        }

        /* Tear down the socket connection, pNetworkContext != NULL here. */
        socketStatus = Sockets_Disconnect( pNetworkContext->socketDescriptor );
    }

    return convertToWolfsslStatus( socketStatus );
}
/*-----------------------------------------------------------*/

int32_t Wolfssl_Recv( NetworkContext_t * pNetworkContext,
                      void * pBuffer,
                      size_t bytesToRecv )
{
    int bytesReceived = 0;
    int sslError = 0;

    /* Unused parameter when logs are disabled. */
    (void)sslError;

    if( pNetworkContext == NULL )
    {
        LogError( ( "Parameter check failed: pNetworkContext is NULL." ) );
    }
    else if( pNetworkContext->pSsl != NULL )
    {
        /* blocking SSL read of data. */
        bytesReceived = wolfSSL_read( pNetworkContext->pSsl,
                                      pBuffer,
                                      ( int ) bytesToRecv );

        /* Handle error return status if transport read did not succeed. */
        if( bytesReceived <= 0 )
        {
            sslError = wolfSSL_get_error( pNetworkContext->pSsl, bytesReceived );

            if( sslError == WOLFSSL_ERROR_WANT_READ )
            {
                /* There is no data to receive at this time. */
                bytesReceived = 0;
            }
            else
            {
                LogError( ( "Failed to receive data over network: error = %d.", sslError ) );
            }
        }
    }
    else
    {
        LogError( ( "Failed to receive data over network: "
                    "SSL object in network context is NULL." ) );
    }

    return ( int32_t ) bytesReceived;
}
/*-----------------------------------------------------------*/

int32_t Wolfssl_Send( NetworkContext_t * pNetworkContext,
                      const void * pBuffer,
                      size_t bytesToSend )
{
    int bytesSent = 0;
    int sslError = 0;

    /* Unused parameter when logs are disabled. */
    ( void ) sslError;

    if( pNetworkContext == NULL )
    {
        LogError( ( "Parameter check failed: pNetworkContext is NULL." ) );
    }
    else if( pNetworkContext->pSsl != NULL )
    {
        /* blocking SSL write of data. */
        bytesSent = wolfSSL_write( pNetworkContext->pSsl,
                                   pBuffer,
                                   ( int ) bytesToSend );

        if( bytesSent <= 0 )
        {
            sslError = wolfSSL_get_error( pNetworkContext->pSsl, bytesSent );

            LogError( ( "Failed to send data over network: error = %d.", sslError ) );
        }
    }
    else
    {
        LogError( ( "Failed to send data over network: "
                    "SSL object in network context is NULL." ) );
    }

    return bytesSent;
}
/*-----------------------------------------------------------*/
