#  Copyright (c) Microsoft Corporation. All rights reserved.
#  Licensed under the MIT License.

cmake_minimum_required (VERSION 3.10)

project (azure-sphere-aws-iot-device-sdk-embedded-c C)

azsphere_configure_tools(TOOLS_REVISION "20.10")
azsphere_configure_api(TARGET_API_SET "7")

add_compile_definitions(AzureSpherePlatform)

include_directories(${CMAKE_SOURCE_DIR} 
					${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/libraries/aws/device-defender-for-aws-iot-embedded-sdk/source/include
					${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/libraries/aws/device-shadow-for-aws-iot-embedded-sdk/source/include
					${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/libraries/aws/jobs-for-aws-iot-embedded-sdk/source/include
					${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/libraries/standard/coreJSON/source/include
					${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/libraries/standard/coreMQTT/source/include
					${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/libraries/standard/coreMQTT/source/interface
                    ${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/libraries/standard/coreHTTP/source/include
                    ${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/libraries/standard/coreHTTP/source/dependency/3rdparty/http_parser
                    ${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/platform/include
					${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/platform/posix/transport/include
					${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/demos
					${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/demos/logging-stack
					${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/demos/mqtt
                    ${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/demos/http
                    ${CMAKE_SOURCE_DIR}/aws-iot-device-sdk-embedded-C/demos/http/common/include
                    )

# Create executable
add_executable (${PROJECT_NAME} 
				main.c
				aws-iot-device-sdk-embedded-C/demos/mqtt/mqtt_demo_basic_tls/mqtt_demo_basic_tls.c
				aws-iot-device-sdk-embedded-C/demos/mqtt/mqtt_demo_mutual_auth/mqtt_demo_mutual_auth.c
				aws-iot-device-sdk-embedded-C/demos/http/http_demo_s3_download/http_demo_s3_download.c
				aws-iot-device-sdk-embedded-C/demos/http/http_demo_s3_upload/http_demo_s3_upload.c
                aws-iot-device-sdk-embedded-C/demos/http/common/src/http_demo_utils.c
				aws-iot-device-sdk-embedded-C/platform/posix/clock_posix.c
				aws-iot-device-sdk-embedded-C/platform/posix/retry_utils_posix.c
				aws-iot-device-sdk-embedded-C/platform/posix/transport/src/sockets_posix.c
				aws-iot-device-sdk-embedded-C/platform/posix/transport/src/wolfssl_posix.c
				aws-iot-device-sdk-embedded-C/libraries/standard/coreMQTT/source/core_mqtt.c
				aws-iot-device-sdk-embedded-C/libraries/standard/coreMQTT/source/core_mqtt_serializer.c
				aws-iot-device-sdk-embedded-C/libraries/standard/coreMQTT/source/core_mqtt_state.c
                aws-iot-device-sdk-embedded-C/libraries/standard/coreHTTP/source/core_http_client.c
                aws-iot-device-sdk-embedded-C/libraries/standard/coreHTTP/source/dependency/3rdparty/http_parser/http_parser.c
				)
target_link_libraries (${PROJECT_NAME} applibs pthread gcc_s c tlsutils wolfssl)

azsphere_target_add_image_package(${PROJECT_NAME} RESOURCE_FILES "certs/mosquitto.org.crt" "certs/StarfieldClass2CA.crt" "certs/BaltimoreCyberTrustRoot.crt")
