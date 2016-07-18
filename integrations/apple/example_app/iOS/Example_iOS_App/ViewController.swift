//
//  ViewController.swift
//  Example_iOS_App
//
//  Created by Joseph Henry on 7/10/16.
//  Copyright © 2016 ZeroTier Inc. All rights reserved.
//

import UIKit

class ViewController: UIViewController {

    var serverPort:UInt16 = 8080
    var serverAddr:String = "10.9.9.203"
    var selectedProtocol:Int32 = 0
    var sock:Int32 = -1
    var accepted_sock:Int32 = -1
    
    @IBOutlet weak var txtAddr: UITextField!
    @IBOutlet weak var txtPort: UITextField!
    
    @IBOutlet weak var txtTX: UITextField!
    @IBOutlet weak var txtRX: UITextField!
    
    @IBOutlet weak var btnTX: UIButton!
  
    
    @IBOutlet weak var btnRX: UIButton!
    @IBAction func UI_RX(sender: AnyObject) {
        // Use ordinary read/write calls on ZeroTier socket
        
        // TCP
        if(selectedProtocol == SOCK_STREAM)
        {
            var buffer = [UInt8](count: 100, repeatedValue: 0)
            let str = "GET / HTTP/1.0\r\n\r\n"
            //let str = "Welcome to the machine"
            print("strlen = %d\n", str.characters.count)
            let encodedDataArray = [UInt8](str.utf8)
            
            //            read(accepted_sock, UnsafeMutablePointer<Void>([txtTX.stringValue]), 128);
            read(accepted_sock, &buffer, 100);
            print(buffer)
            
        }
        // UDP
        if(selectedProtocol == SOCK_DGRAM)
        {
            // recvfrom
        }
    }
    
    
    
    @IBAction func UI_TX(sender: AnyObject) {
        // Use ordinary read/write calls on ZeroTier socket
        
        // TCP
        if(selectedProtocol == SOCK_STREAM)
        {
            print("writing...")
            write(sock, txtTX.description, txtTX.description.characters.count);
        }
        // UDP
        if(selectedProtocol == SOCK_DGRAM)
        {
            // sendto
        }
    }
    
    
    @IBOutlet weak var txtNWID: UITextField!
    
    @IBOutlet weak var btnJoinNetwork: UIButton!
    @IBAction func UI_JoinNetwork(sender: AnyObject) {
        zt_join_network(txtNWID.text!)
    }
    
    @IBOutlet weak var btnLeaveNetwork: UIButton!
    @IBAction func UI_LeaveNetwork(sender: AnyObject) {
        zt_leave_network(txtNWID.text!)
    }
    
    @IBOutlet weak var segmentProtocol: UISegmentedControl!
    @IBAction func protocolSelected(sender: AnyObject) {
        switch sender.selectedSegmentIndex
        {
        case 0:
            selectedProtocol = SOCK_STREAM
        case 1:
            selectedProtocol = SOCK_DGRAM
        default:
            break;
        }
    }

    @IBOutlet weak var btnConnect: UIButton!
    @IBAction func UI_Connect(sender: AnyObject) {
        // TCP
        if(selectedProtocol == SOCK_STREAM)
        {
            sock = zts_socket(AF_INET, SOCK_STREAM, 0)
            var addr = sockaddr_in(sin_len: UInt8(sizeof(sockaddr_in)),
                                   sin_family: UInt8(AF_INET),
                                   sin_port: UInt16(serverPort).bigEndian,
                                   sin_addr: in_addr(s_addr: 0),
                                   sin_zero: (0,0,0,0,0,0,0,0))
            
            inet_pton(AF_INET, serverAddr, &(addr.sin_addr));
            
            let connect_err = zts_connect(sock, UnsafePointer<sockaddr>([addr]), UInt32(addr.sin_len))
            print("connect_err = \(connect_err),\(errno)")
            
            if connect_err < 0 {
                let err = errno
                print("Error connecting IPv4 socket \(err)")
                return
            }
        }
        
        // UDP
        if(selectedProtocol == SOCK_DGRAM)
        {
            
        }
    }
    
    
    @IBOutlet weak var btnBind: UIButton!
    @IBAction func UI_Bind(sender: AnyObject) {
        // TCP
        if(selectedProtocol == SOCK_STREAM)
        {
            sock = zts_socket(AF_INET, SOCK_STREAM, 0)
            var addr = sockaddr_in(sin_len: UInt8(sizeof(sockaddr_in)),
                                   sin_family: UInt8(AF_INET),
                                   sin_port: UInt16(serverPort).bigEndian,
                                   sin_addr: in_addr(s_addr: 0),
                                   sin_zero: (0,0,0,0,0,0,0,0))
            
            inet_pton(AF_INET, serverAddr, &(addr.sin_addr));
            
            let bind_err = zts_bind(sock, UnsafePointer<sockaddr>([addr]), UInt32(addr.sin_len))
            
            print("bind_err = \(bind_err),\(errno)")
            
            if bind_err < 0 {
                let err = errno
                print("Error binding IPv4 socket \(err)")
                return
            }
            
            // Put socket into listening state
            zts_listen(sock, 1);
            
            // Accept connection
            var len:socklen_t = 0;
            var legIntPtr = withUnsafeMutablePointer(&len, { $0 })
            while(accepted_sock < 0) {
                accepted_sock = zts_accept(sock, UnsafeMutablePointer<sockaddr>([addr]), legIntPtr)
            }
            print("accepted connection")
        }
    }

    
    // ZeroTier service thread
    var service_thread : NSThread!
    func ztnc_start_service() {
        let path = NSSearchPathForDirectoriesInDomains(NSSearchPathDirectory.DocumentDirectory, NSSearchPathDomainMask.UserDomainMask, true)
        start_service_and_rpc(path[0],"565799d8f65063e5")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        
        txtNWID.text = "565799d8f65063e5"
        txtTX.text = "welcome to the machine"
        txtAddr.text = "10.9.9.203"
        txtPort.text = "8080"
        selectedProtocol = SOCK_STREAM
        
        // ZeroTier Service thread
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), {
            self.service_thread = NSThread(target:self, selector:"ztnc_start_service", object:nil)
            self.service_thread.start()
        });
                
        // Do any additional setup after loading the view, typically from a nib.
    }

    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
    }
}

