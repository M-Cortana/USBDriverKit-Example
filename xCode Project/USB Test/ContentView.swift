//
//  ContentView.swift
//  USB Test
//
//  Created by 欧阳宗桦 on 4/27/25.
//

import SwiftUI
import Foundation

struct ContentView: View {
    @ObservedObject public var viewModel: DriverCommunicationViewModel = .init()
    @State public var switchState: Bool = false
    var body: some View {
        VStack {
            Form {
                Section(header: Text("Install the Driver")) {
                    Button(
                        action: {
                            if let url = URL(string: UIApplication.openSettingsURLString) {
                                UIApplication.shared.open(url)
                            }
                        }, label: {
                            Text("Open Settings to Enable the Driver")
                        }
                    )
                    Text(viewModel.isConnected ? "Device Connected" : "Device Disconnected")
                }
                Section(header: Text("Unchecked Scalar Method")) {
                    Button("Async Communicate With Driver Test") {
                        _ = viewModel.SwiftCommunicateWithDriverTest(aIn: [1, 2, 3, 4, 5, 6, 7]);
                    }
                }
                Section(header: Text("Async Callback Method")) {
                    Button("Communicate With Driver Test") {
                        viewModel.SwiftAsyncCommunicateWithDriverTest(aIn: [2, 3, 4, 5, 6, 7]);
                    }
                }
                Section(header: Text("USB Communication")) {
                    HStack {
                        Spacer()
                        Toggle(isOn: $switchState){Text("LED")}.labelsHidden().toggleStyle(.switch).font(.largeTitle).scaleEffect(10).onChange(of: switchState) {
                            oldValue, newValue in
                            viewModel.switchState = newValue
                            if (newValue == true) {
                                viewModel.SwiftUSBTransmit(buffer: [0xEA, 0xEB, 0xEC, 0xED, 0xEE])
                            } else {
                                viewModel.SwiftUSBTransmit(buffer: [0xFA, 0xFB, 0xFC, 0xFD, 0xFE])
                            }
                        }
                        Spacer()
                    }.padding(250)
                }
            } //End Form
        } //End VStack
    }
}

#Preview {
    ContentView()
}
