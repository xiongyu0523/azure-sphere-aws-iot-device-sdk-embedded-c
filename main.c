/* Standard includes. */
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* POSIX includes. */
#include <unistd.h>
#include <errno.h>

/* Azure Sphere Application library includes */
#include <applibs/log.h>
#include <applibs/networking.h>
#include <applibs/application.h>

/* Demos */
extern int mqtt_demo_basic_tls(int argc, char **argv);
extern int mqtt_demo_mutual_auth(int argc, char **argv);
extern int http_demo_s3_upload(int argc, char** argv);
extern int http_demo_s3_download(int argc, char **argv);
extern int shadow_demo_main(int argc, char** argv);

static bool isNetworkInterfaceConnectedToInternet(void)
{
    static const char networkInterface[] = "wlan0";

    Networking_InterfaceConnectionStatus status;
    if (Networking_GetInterfaceConnectionStatus(networkInterface, &status) != 0) {
        if (errno != EAGAIN) {
            Log_Debug("Networking_GetInterfaceConnectionStatus: %d (%s).\r\n", errno, strerror( errno ) );
            return false;
        }
        Log_Debug("Not doing download because the networking stack isn't ready yet.\r\n");
        return false;
    }

    if ((status & Networking_InterfaceConnectionStatus_ConnectedToInternet) == 0) {
        Log_Debug("No internet connectivity.\r\n");
        return false;
    }

    return true;
}

static bool isDeviceAuthenticationAttestationPassed(void)
{
    bool result = false;

    (void)Application_IsDeviceAuthReady(&result);
    if (!result) {
        Log_Debug("Device Authentication and Attestation isn't ready yet.\r\n");
    }

    return result;
}

int main(void)
{
    bool isInternetConnected = false;
    bool isDaaPassed = false;

    /* Wait for Azure Sphere to connect to Internet */
    do {
        isInternetConnected = isNetworkInterfaceConnectedToInternet();
    } while (!isInternetConnected);

    /* Wait for Azure Sphere to pass DAA so it will receive device certificate */
    do {
        isDaaPassed = isDeviceAuthenticationAttestationPassed();
    } while (!isDaaPassed);

    mqtt_demo_basic_tls(0, NULL);
    mqtt_demo_mutual_auth(0, NULL);
    http_demo_s3_upload(0, NULL);
    http_demo_s3_download(0, NULL);
    shadow_demo_main(0, NULL);
 
    return 0;
}
