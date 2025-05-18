#include "UserClientCommunication.h"
#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <os/log.h>

#define Log(fmt, ...) os_log(OS_LOG_DEFAULT, "USB Test - " fmt "\n", ##__VA_ARGS__)

#define EXTERNAL_METHOD_USB_TRANSMIT    1

// If you don't know what value to use here, it should be identical to the IOUserClass value in your IOKitPersonalities.
// You can double check by searching with the `ioreg` command in your terminal.
// It will be of type "IOUserService" not "IOUserServer"
static const char* dextIdentifier = "NullUSBDriver";

static IONotificationPortRef globalNotificationPort = NULL;
static mach_port_t globalMachNotificationPort;
static CFRunLoopRef globalRunLoop = NULL;
static CFRunLoopSourceRef globalRunLoopSource = NULL;
static io_iterator_t globalDeviceAddedIter = IO_OBJECT_NULL;
static io_iterator_t globalDeviceRemovedIter = IO_OBJECT_NULL;

static inline void PrintErrorDetails(kern_return_t ret) {
    printf("\tSystem: 0x%02x\n", err_get_system(ret));
    printf("\tSubsystem: 0x%03x\n", err_get_sub(ret));
    printf("\tCode: 0x%04x\n", err_get_code(ret));
}

// MARK: C Constructors/Destructors

bool UserClientSetup(void* refcon) {
    kern_return_t ret = kIOReturnSuccess;

    globalRunLoop = CFRunLoopGetCurrent();
    if (globalRunLoop == NULL) {
        fprintf(stderr, "Failed to initialize globalRunLoop.\n");
        return false;
    }
    CFRetain(globalRunLoop);

    globalNotificationPort = IONotificationPortCreate(kIOMainPortDefault);
    if (globalNotificationPort == NULL) {
        fprintf(stderr, "Failed to initialize globalNotificationPort.\n");
        UserClientTeardown();
        return false;
    }
    
    globalMachNotificationPort = IONotificationPortGetMachPort(globalNotificationPort);
    if (globalMachNotificationPort == 0) {
        fprintf(stderr, "Failed to initialize globalMachNotificationPort.\n");
        UserClientTeardown();
        return false;
    }
    
    globalRunLoopSource = IONotificationPortGetRunLoopSource(globalNotificationPort);
    if (globalRunLoopSource == NULL) {
        fprintf(stderr, "Failed to initialize globalRunLoopSource.\n");
        return false;
    }

    // Establish our notifications in the run loop, so we can get callbacks.
    CFRunLoopAddSource(globalRunLoop, globalRunLoopSource, kCFRunLoopDefaultMode);

    /// - Tag: SetUpMatchingNotification
    CFMutableDictionaryRef matchingDictionary = IOServiceNameMatching(dextIdentifier);
    if (matchingDictionary == NULL) {
        fprintf(stderr, "Failed to initialize matchingDictionary.\n");
        UserClientTeardown();
        return false;
    }
    matchingDictionary = (CFMutableDictionaryRef)CFRetain(matchingDictionary);
    matchingDictionary = (CFMutableDictionaryRef)CFRetain(matchingDictionary);

    ret = IOServiceAddMatchingNotification(globalNotificationPort, kIOFirstMatchNotification, matchingDictionary, DeviceAdded, refcon, &globalDeviceAddedIter);
    if (ret != kIOReturnSuccess) {
        fprintf(stderr, "Add matching notification failed with error: 0x%08x.\n", ret);
        UserClientTeardown();
        return false;
    }
    DeviceAdded(refcon, globalDeviceAddedIter);

    ret = IOServiceAddMatchingNotification(globalNotificationPort, kIOTerminatedNotification, matchingDictionary, DeviceRemoved, refcon, &globalDeviceRemovedIter);
    if (ret != kIOReturnSuccess) {
        fprintf(stderr, "Add termination notification failed with error: 0x%08x.\n", ret);
        UserClientTeardown();
        return false;
    }
    DeviceRemoved(refcon, globalDeviceRemovedIter);

    return true;
}

void UserClientTeardown(void) {
    if (globalRunLoopSource) {
        CFRunLoopRemoveSource(globalRunLoop, globalRunLoopSource, kCFRunLoopDefaultMode);
        globalRunLoopSource = NULL;
    }

    if (globalNotificationPort) {
        IONotificationPortDestroy(globalNotificationPort);
        globalNotificationPort = NULL;
        globalMachNotificationPort = 0;
    }

    if (globalRunLoop) {
        CFRelease(globalRunLoop);
        globalRunLoop = NULL;
    }

    globalDeviceAddedIter = IO_OBJECT_NULL;
    globalDeviceRemovedIter = IO_OBJECT_NULL;
}

// MARK: Asynchronous Events

/// - Tag: DeviceAdded
void DeviceAdded(void* refcon, io_iterator_t iterator) {
    kern_return_t ret = kIOReturnSuccess;
    io_connect_t connection = IO_OBJECT_NULL;
    io_service_t device = IO_OBJECT_NULL;
    bool attemptedToMatchDevice = false;

    while ((device = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        attemptedToMatchDevice = true;

        // Open a connection to this user client as a server to that client, and store the instance in "service"
        ret = IOServiceOpen(device, mach_task_self_, 0, &connection);

        if (ret == kIOReturnSuccess) {
            fprintf(stdout, "Opened connection to dext.\n");
        } else {
            fprintf(stderr, "Failed opening connection to dext with error: 0x%08x.\n", ret);
            IOObjectRelease(device);
            return;
        }
        SwiftDeviceAdded(refcon, connection);
        IOObjectRelease(device);
    }
}

void DeviceRemoved(void* refcon, io_iterator_t iterator) {
    io_service_t device = IO_OBJECT_NULL;

    while ((device = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        fprintf(stdout, "Closed connection to dext.\n");
        IOObjectRelease(device);
        SwiftDeviceRemoved(refcon);
    }
}

bool CommunicateWithDriverTest(io_connect_t connection, uint64_t *pInput, uint32_t inSize, uint64_t *pOutput, uint32_t *outSize) {
    kern_return_t ret = kIOReturnSuccess;
    printf("C Input Count: %u\n", inSize);
    ret = IOConnectCallScalarMethod(connection, 23333333, pInput, inSize, pOutput, outSize);
    if (ret != kIOReturnSuccess) {
        return false;
    }
    *outSize = (uint32_t) *pOutput;
    printf("C Output Count: %u\n", *outSize);
    return true;
}

void AsyncCallback(void* refcon, IOReturn result, void** args, UInt32 numArgs) {
    printf("AsyncCallback(numArgs = %u)", numArgs);
}

bool AsyncCommunicateWithDriverTest(void *refcon, io_connect_t connection, uint64_t *pInput, uint32_t inSize, uint64_t *pOutput, uint32_t *outSize) {
    kern_return_t ret = kIOReturnSuccess;
    printf("Async C Input Count: %u\n", inSize);
    io_async_ref64_t asyncRef = {};
    asyncRef[kIOAsyncCalloutFuncIndex] = (io_user_reference_t) AsyncCallback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (io_user_reference_t) refcon;
    ret = IOConnectCallAsyncScalarMethod(connection, 2333, globalMachNotificationPort, asyncRef, kIOAsyncCalloutCount, pInput, inSize, pOutput, outSize);
    if (ret != kIOReturnSuccess) {
        return false;
    }
    return true;
}

bool USBTransmit(io_connect_t connection, uint8_t *pBuf, uint32_t len) {
    kern_return_t ret = kIOReturnSuccess;
    uint64_t aOut;
    uint32_t outCnt;
    printf("USBTransmit Count: %u\n", len);
    ret = IOConnectCallScalarMethod(connection, EXTERNAL_METHOD_USB_TRANSMIT, (uint64_t*) pBuf, len, &aOut, &outCnt);
    if (ret != kIOReturnSuccess) {
        return false;
    }
    return true;
}
