//
//  NullUSBDriverUserClient.cpp
//  NullUSBDriver
//
//  Created by 欧阳宗桦 on 5/15/25.
//

#include <os/log.h>

#include <DriverKit/IOLib.h>
#include <DriverKit/IOMemoryMap.h>
#include <DriverKit/IOTimerDispatchSource.h>
#include <DriverKit/IOUserClient.h>
#include <DriverKit/OSData.h>

#include <USBDriverKit/IOUSBHostInterface.h>
#include <USBDriverKit/IOUSBHostPipe.h>

#include <time.h>

#include "NullUSBDriverUserClient.h"
#include "NullUSBDriver.h"

#define Log(fmt, ...) os_log(OS_LOG_DEFAULT, "NullUSBDriver UserClient - " fmt "\n", ##__VA_ARGS__)

#define EXTERNAL_METHOD_USB_TRANSMIT    1

struct NullUSBDriverUserClient_IVars {
    NullUSBDriver *kernel = nullptr;
    OSAction *testUserCallbackAction = nullptr;
    IODispatchQueue *dispatchQueue = nullptr;
    IOTimerDispatchSource *dispatchSource = nullptr;
    OSAction *testCallback = nullptr;
};

bool NullUSBDriverUserClient::init(void) {
    bool result = false;
    Log("init()");
    result = super::init();
    if (result != true) {
        Log("init() - super::init failed.");
        return false;
    }
    ivars = IONewZero(NullUSBDriverUserClient_IVars, 1);
    if (ivars == nullptr) {
        Log("init() - Failed to allocate memory for ivars.");
        return false;
    }
    Log("init() - Finished.");
    return true;
}

kern_return_t IMPL(NullUSBDriverUserClient, Start) {
    kern_return_t ret = kIOReturnSuccess;
    Log("Start()");
    ret = Start(provider, SUPERDISPATCH);
    if (ret != kIOReturnSuccess) {
        Log("Start() - super::Start failed with error: 0x%08x.", ret);
        return ret;
    }
    ret = IODispatchQueue::Create("NullDriverDispatchQueue", 0, 0, &ivars->dispatchQueue);
    if (ret != kIOReturnSuccess) {
        Log("Start() - Failed to create dispatch queue with error: 0x%08x.", ret);
        return ret;
    }
    ret = IOTimerDispatchSource::Create(ivars->dispatchQueue, &ivars->dispatchSource);
    if (ret != kIOReturnSuccess) {
        Log("Start() - Failed to create dispatch source with error: 0x%08x.", ret);
        return ret;
    }
    ret = CreateActionTimerCallbackTest(sizeof(void *), &ivars->testCallback);
    if (ret != kIOReturnSuccess) {
        Log("Start() - Failed to create action for simulated async event with error: 0x%08x.", ret);
        return ret;
    }
    ret = ivars->dispatchSource->SetHandler(ivars->testCallback);
    if (ret != kIOReturnSuccess) {
        Log("Start() - Failed to assign simulated action to handler with error: 0x%08x.", ret);
        return ret;
    }
    
    ret = RegisterService();
    if (ret != kIOReturnSuccess) {
        Log("Start() - Failed to register service with error: 0x%08x.", ret);
        return ret;
    }
    ivars->kernel = OSDynamicCast(NullUSBDriver, provider);
    Log("Start() - Finished.");
    ret = kIOReturnSuccess;
    return ret;
}

kern_return_t IMPL(NullUSBDriverUserClient, Stop) {
    kern_return_t ret = kIOReturnSuccess;
    Log("Stop()");
    ret = Stop(provider, SUPERDISPATCH);
    if (ret != kIOReturnSuccess) {
        Log("Stop() - super::Stop failed with error: 0x%08x.", ret);
    }
    Log("Stop() - Finished.");
    return ret;
}

void NullUSBDriverUserClient::free(void) {
    Log("free()");
    OSSafeReleaseNULL(ivars->testUserCallbackAction);
    OSSafeReleaseNULL(ivars->dispatchSource);
    OSSafeReleaseNULL(ivars->dispatchQueue);
    OSSafeReleaseNULL(ivars->testCallback);
    IOSafeDeleteNULL(ivars, NullUSBDriverUserClient_IVars, 1);
    super::free();
}


kern_return_t NullUSBDriverUserClient::CommunicateWithUserClientTest(IOUserClientMethodArguments *arguments) {
    Log("CommunicateWithUserClientTest() with Input Count of %u", arguments->scalarInputCount);
    *(arguments->scalarOutput) = arguments->scalarInputCount;
    *(arguments->scalarOutput + 1) =  *(arguments->scalarInput);
    for (uint64_t *i = (uint64_t*) arguments->scalarInput + 1, *j = arguments->scalarOutput + 2; i != arguments->scalarInput + arguments->scalarInputCount; i++, j++) {
        *j = *(j - 1) + *i;
    }
    return kIOReturnSuccess;
}

kern_return_t NullUSBDriverUserClient::AsyncCommunicateWithDriverTest(IOUserClientMethodArguments *arguments) {
    Log("AsyncCommunicateWithDriverTest()");
    ivars->testUserCallbackAction = arguments->completion;
    ivars->testUserCallbackAction->retain();
    const uint64_t oneSecondsInNanoSeconds = 1000000000;
    const uint64_t twoSecondsInNanoSeconds = 2000000000;
    uint64_t currentTime = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
    Log("AsyncCommunicateWithDriverTest() Sleeping async...");
    ivars->dispatchSource->WakeAtTime(kIOTimerClockMonotonicRaw, currentTime + twoSecondsInNanoSeconds, oneSecondsInNanoSeconds);
    return kIOReturnSuccess;
}

kern_return_t NullUSBDriverUserClient::USBTransmit(IOUserClientMethodArguments *arguments) {
    Log("USBTransmit()");
    uint32_t inSize = arguments->scalarInputCount;
    ivars->kernel->USBTransmit((uint8_t*) arguments->scalarInput, inSize);
    return kIOReturnSuccess;
}

// MARK: ExternalMethod Handler
// This method is called when an application calls IOConnectCall...Method.
// All of the passed inputs and outputs are accessible through the "arguments" variable.
// Never trust any of the data that has been passed in, such as sizes, as these can be used as attack vectors on your code.
// For example, an incorrect size may allow for a buffer overflow attack on the dext.
kern_return_t NullUSBDriverUserClient::ExternalMethod(uint64_t selector, IOUserClientMethodArguments* arguments, const IOUserClientMethodDispatch* dispatch, OSObject* target, void* reference)
{
    Log("ExternalMethod(selector = %llu)", selector);
    
    kern_return_t ret = kIOReturnSuccess;
    if (arguments == nullptr) {
        Log("Arguments were null.");
        ret = kIOReturnBadArgument;
        return ret;
    }
    
    if (arguments->scalarInput == nullptr) {
        Log("Scalar input memory is null.");
        ret = kIOReturnBadArgument;
        return ret;
    }
    if (arguments->scalarOutput == nullptr) {
        Log("Scalar output memory is null.");
        ret = kIOReturnBadArgument;
        return ret;
    }
    
    switch (selector) {
        case EXTERNAL_METHOD_USB_TRANSMIT:
            return USBTransmit(arguments);
        case 2333:
            return AsyncCommunicateWithDriverTest(arguments);
        case 23333333:
            return CommunicateWithUserClientTest(arguments);
    }
    
    return ret;
}

void IMPL(NullUSBDriverUserClient, TimerCallbackTest) {
    Log("TimerCallbackTest()");
    uint64_t aOut[5] = {0, 8, 2, 5, 1};
    uint32_t outCnt = 5;
    if (ivars->testUserCallbackAction == nullptr) {
        Log("TimerCallbackTest() Null testUserCallbackAction");
        return;
    }
    AsyncCompletion(ivars->testUserCallbackAction, kIOReturnSuccess, aOut, outCnt);
}
