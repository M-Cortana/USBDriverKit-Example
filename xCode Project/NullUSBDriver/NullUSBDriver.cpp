#include <os/log.h>

#include <DriverKit/IOLib.h>
#include <DriverKit/IOMemoryMap.h>
#include <DriverKit/IOTimerDispatchSource.h>
#include <DriverKit/IOUserClient.h>
#include <DriverKit/OSData.h>

#include <USBDriverKit/IOUSBHostInterface.h>
#include <USBDriverKit/IOUSBHostPipe.h>

#include <time.h>

#include "NullUSBDriver.h"
#include "NullUSBDriverUserClient.h"

// This log to makes it easier to parse out individual logs from the driver, since all logs will be prefixed with the same word/phrase.
// DriverKit logging has no logging levels; some developers might want to prefix errors differently than info messages.
// Another option is to #define a "debug" case where some log messages only exist when doing a debug build.
// To search for logs from this driver, use either: `sudo dmesg | grep NullUSBDriver` or use Console.app search to find messages that start with "NullUSBDriver -".
#define Log(fmt, ...) os_log(OS_LOG_DEFAULT, "NullUSBDriver - " fmt "\n", ##__VA_ARGS__)


struct NullUSBDriver_IVars {
    NullUSBDriverUserClient* userClient = nullptr;
    
    IOUSBHostInterface *interface;
    IOUSBHostDevice *device;
    IOUSBHostPipe *outPipe;
    //OSAction *ioCompleteCallback;
    IOBufferMemoryDescriptor *outData;
    IOAddressSegment outSeg;
};


bool NullUSBDriver::init(void) {
    bool result = false;
    Log("init()");
    result = super::init();
    if (result != true) {
        Log("init() - super::init failed.");
        return false;
    }
    ivars = IONewZero(NullUSBDriver_IVars, 1);
    if (ivars == nullptr) {
        Log("init() - Failed to allocate memory for ivars.");
        return false;
    }
    Log("init() - Finished.");
    return true;
}

kern_return_t IMPL(NullUSBDriver, Start) {
    kern_return_t ret = kIOReturnSuccess;
    Log("Start()");
    ret = Start(provider, SUPERDISPATCH);
    if (ret != kIOReturnSuccess) {
        Log("Start() - super::Start failed with error: 0x%08x.", ret);
        return ret;
    }
    
    ret = RegisterService();
    if (ret != kIOReturnSuccess) {
        Log("Start() - Failed to register service with error: 0x%08x.", ret);
        return ret;
    }
    //USB Start
    
    ivars->interface = OSDynamicCast(IOUSBHostInterface, provider);
    
    if (ivars->interface == nullptr) {
        Log("Start() - Failed to cast IOUSBHostInterface");
        ret = kIOReturnNoDevice;
        return ret;
    }
    ret = ivars->interface->Open(this, 0, NULL);
    if (ret != kIOReturnSuccess) {
        Log("Start() - Failed to open interface with error: 0x%08x.", ret);
        return ret;
    }
    ret = ivars->interface->CopyPipe(0x01, &ivars->outPipe);
    if (ret != kIOReturnSuccess) {
        Log("Start() - Failed to copy pipe with error: 0x%08x.", ret);
        return ret;
    }
    
    ret = ivars->interface->CreateIOBuffer(kIOMemoryDirectionInOut, 64, &(ivars->outData));
    if (ret != kIOReturnSuccess) {
        Log("Start() - Failed to create I/O buffer with error: 0x%08x.", ret);
        return ret;
    }
    
    Log("Start() - Finished.");
    ret = kIOReturnSuccess;
    return ret;
}

kern_return_t IMPL(NullUSBDriver, Stop) {
    kern_return_t ret = kIOReturnSuccess;
    Log("Stop()");
    ret = Stop(provider, SUPERDISPATCH);
    if (ret != kIOReturnSuccess) {
        Log("Stop() - super::Stop failed with error: 0x%08x.", ret);
    }
    Log("Stop() - Finished.");
    return ret;
}

void NullUSBDriver::free(void) {
    Log("free()");
    OSSafeReleaseNULL(ivars->userClient);
    IOSafeDeleteNULL(ivars, NullUSBDriver_IVars, 1);
    super::free();
}

kern_return_t IMPL(NullUSBDriver, NewUserClient) {
    kern_return_t ret = kIOReturnSuccess;
    IOService* client = nullptr;
    Log("NewUserClient()");
    ret = Create(this, "UserClientProperties", &client);
    if (ret != kIOReturnSuccess) {
        Log("NewUserClient() - Failed to create UserClientProperties with error: 0x%08x.", ret);
        return ret;
    }
    ivars->userClient = OSDynamicCast(NullUSBDriverUserClient, client);
    *userClient = ivars->userClient;
    if (*userClient == NULL) {
        Log("NewUserClient() - Failed to cast new client.");
        client->release();
        ret = kIOReturnError;
        return ret;
    }
    Log("NewUserClient() - Finished.");
    return ret;
}

kern_return_t NullUSBDriver::USBTransmit(uint8_t *pBuf, uint32_t len) {
    kern_return_t ret = kIOReturnSuccess;
    Log("USBTransmit() - len = %u", len);
    /*for (uint8_t *i = pBuf; i != pBuf + len; i++) {
        //Log("USBTransmit() Data: 0x%02X", *i);
    }*/
    ret = ivars->outData->GetAddressRange(&ivars->outSeg);
    Log("USBTransmit() - Address Range of 0x%08llX , len = %llu", ivars->outSeg.address, ivars->outSeg.length);
    if (ret != kIOReturnSuccess) {
        Log("USBTransmit() - GetAddressRange Failed with 0x%08x", ret);
    }
    uint32_t bytesTransferred = 0;
    void *addr = reinterpret_cast<void*> (ivars->outSeg.address);
    memcpy((uint8_t*) addr, pBuf, len);
    ret = ivars->outPipe->IO(ivars->outData, len, &bytesTransferred, 1000);
    if (ret != kIOReturnSuccess) {
        Log("USBTransmit() - Failed with error: 0x%08x", ret);
        PrintExtendedErrorInfo(ret);
    }
    return ret;
}


// MARK: Detail Helpers
void NullUSBDriver::PrintExtendedErrorInfo(kern_return_t ret) {
    Log("\tSystem: 0x%02x", err_get_system(ret));
    Log("\tSubsystem: 0x%03x", err_get_sub(ret));
    Log("\tCode: 0x%04x", err_get_code(ret));
}
