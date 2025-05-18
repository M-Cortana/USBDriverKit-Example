/*
See the LICENSE.txt file for this sample’s licensing information.

Abstract:
The view model that indicates the state of driver communication.
*/

import Foundation
import SwiftUI

@_silgen_name("SwiftDeviceAdded")
func SwiftDeviceAdded(refcon: UnsafeMutableRawPointer, connection: io_connect_t) {
    let viewModel: DriverCommunicationViewModel = Unmanaged<DriverCommunicationViewModel>.fromOpaque(refcon).takeUnretainedValue()
    viewModel.isConnected = true
    viewModel.connection = connection
}

@_silgen_name("SwiftDeviceRemoved")
func SwiftDeviceRemoved(refcon: UnsafeMutableRawPointer) {
    let viewModel: DriverCommunicationViewModel = Unmanaged<DriverCommunicationViewModel>.fromOpaque(refcon).takeUnretainedValue()
    viewModel.isConnected = false
}

class DriverCommunicationViewModel: NSObject, ObservableObject {

    @Published public var isConnected: Bool = false
    public var connection: io_connect_t = 0
    @Published public var switchState: Bool = false

    var opaqueSelf: UnsafeMutableRawPointer? = UnsafeMutableRawPointer(bitPattern: 0)

    override init() {
        super.init()
        // Create a reference to this view model so the C code can call back to it.
        self.opaqueSelf = Unmanaged.passRetained(self).toOpaque()
        // Let the C code set up the IOKit calls.
        UserClientSetup(opaqueSelf)
    }

    convenience init(isConnected: Bool) {
        self.init()
        self.isConnected = isConnected
    }

    deinit {
        // Take the last reference of the pointer so it can be freed.
        _ = Unmanaged<DriverCommunicationViewModel>.fromOpaque(self.opaqueSelf!).takeRetainedValue()
        // Let the C code clean up after itself.
        UserClientTeardown()
    }

    func SwiftCommunicateWithDriverTest(aIn: [UInt64]) -> [UInt64] {
        var input = aIn
        var output = [UInt64](repeating: 0, count: 1024)
        var outCnt: UInt32 = 1024
        print("Input Array: \(input)")
        CommunicateWithDriverTest(connection, &input, UInt32(aIn.count), &output, &outCnt)
        print("Output Array: \(output.dropLast(output.count - Int(outCnt) - 1))")
        return output.dropLast(output.count - Int(outCnt) - 1)
    }
    
    func SwiftAsyncCommunicateWithDriverTest(aIn: [UInt64]) {
        var input = aIn
        var output = [UInt64](repeating: 0, count: 1024)
        var outCnt: UInt32 = 1024
        AsyncCommunicateWithDriverTest(opaqueSelf, connection, &input, UInt32(aIn.count), &output, &outCnt);
    }
    
    func SwiftUSBTransmit(buffer: [UInt8]) {
        var buf = buffer;
        USBTransmit(connection, &buf, UInt32(buf.count))
    }
}
