#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <applibs/networking.h>
#include <tlsutils/deviceauth.h>

/* Include header that defines log levels. Strict order */
#include "logging_levels.h"
#define LIBRARY_LOG_NAME     "AzSphere"
#define LIBRARY_LOG_LEVEL    LOG_INFO
#include "logging_stack.h"

#include "clock.h"
#include "sockets_posix.h"

/* The host and port from which to establish the connection. */
#define HOSTNAME             "baidu.com"
#define PORT                 80

static const char networkInterface[] = "wlan0";

static bool isNetworkInterfaceConnectedToInternet(void)
{
    Networking_InterfaceConnectionStatus status;
    if (Networking_GetInterfaceConnectionStatus(networkInterface, &status) != 0) {
        if (errno != EAGAIN) {
            Log_Debug("ERROR: Networking_GetInterfaceConnectionStatus: %d (%s)\n", errno, strerror(errno));
            return false;
        }
        Log_Debug("WARNING: Not doing download because the networking stack isn't ready yet.\n");
        return false;
    }

    if ((status & Networking_InterfaceConnectionStatus_ConnectedToInternet) == 0) {
        Log_Debug("WARNING: no internet connectivity.\n");
        return false;
    }

    return true;
}

int main(void)
{
    bool isInternetConnected = false;
    do {
        isInternetConnected = isNetworkInterfaceConnectedToInternet();
        Clock_SleepMs(1000);
    } while (isInternetConnected == false);

    int socket;
    ServerInfo_t serverInfo;
    SocketStatus_t socketStatus;

    serverInfo.pHostName = HOSTNAME;
    serverInfo.hostNameLength = strlen(HOSTNAME);
    serverInfo.port = PORT;

    socketStatus = Sockets_Connect(&socket, &serverInfo, 0, 0);
    if (socketStatus == SOCKETS_SUCCESS) {
        LogInfo(("Open socket %d succeed\n", socket));
    }

    socketStatus = Sockets_Disconnect(socket);
    if (socketStatus == SOCKETS_SUCCESS) {
        LogInfo(("Close socket succeed\n"));
    }

    return 0;
}
