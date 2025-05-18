/*
See the LICENSE.txt file for this sample’s licensing information.

Abstract:
Declarations of C functions to perform calls to the driver and implement driver lifecycle callbacks.
*/
#ifndef UserClientCommunication_h
#define UserClientCommunication_h

#include <IOKit/IOTypes.h>


extern void SwiftDeviceAdded(void* refcon, io_connect_t connection);
extern void SwiftDeviceRemoved(void* refcon);
void DeviceAdded(void* refcon, io_iterator_t iterator);
void DeviceRemoved(void* refcon, io_iterator_t iterator);

bool UserClientSetup(void* refcon);
void UserClientTeardown(void);

bool CommunicateWithDriverTest(io_connect_t connection, uint64_t *pInput, uint32_t inSize, uint64_t *pOutput, uint32_t *outSize);
bool AsyncCommunicateWithDriverTest(void *refcon, io_connect_t connection, uint64_t *pInput, uint32_t inSize, uint64_t *pOutput, uint32_t *outSize);

bool USBTransmit(io_connect_t connection, uint8_t *pBuf, uint32_t len);

#endif /* UserClientCommunication_h */
