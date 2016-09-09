OSX + ZeroTier SDK
====

Welcome!

Imagine a flat, encrypted, no-configuration LAN for all of the instances of your OSX app. 

This short tutorial will show you how to enable ZeroTier functionality for your OSX app with little to no code modification. Check out our [ZeroTier SDK](https://www.zerotier.com/blog) page for more info on how the integration works.

***

## Via Traditional Linking (Everything bundled)

 - Use this if you'd like everything included in a single easy-to-use library.

```
make osx_shared_lib`
g++ app.cpp -o app libztosx.so
./app
```

## Via Traditional Linking (Service+Intercept model)

 - Use this model if you'd like multiple applications to talk to the same ZeroTierSDK service instance. Often the *intercept-model* is used when you don't have access to the source of an app and you'd like to re-direct its network calls.

Example:

    gcc app.c -o app libztintercept.so
    export ZT_NC_NETWORK=/tmp/sdk-test-home/nc_8056c2e21c000001

Start service

    ./zerotier-sdk-service -d -p8000 /tmp/sdk-test-home &

Run application

    ./app

## Via App Framework in XCode

***
**Step 1: Build OSX framework**

- `make osx_app_framework`
- This will output to `build/osx_app_framework/Release/ZeroTierSDK_OSX.framework`

**Step 2: Integrate SDK into project**

- Add the resultant framework package to your project
- Add `src` directory to *Build Settings -> Header Search Paths*
- Add `build/osx_app_framework/Release/` to *Build Settings -> Framework Search Paths*
- Add `ZeroTierSDK.frameworkOSX` to *General->Embedded Binaries*
- Add `src/ZTSDK.swift`, `src/SDK_XcodeWrapper.cpp`, and `src/SDK_XcodeWrapper.hpp` to your project:
- Set `src/SDK_Apple-Bridging-Header.h` as your bridging-header in *Build Settings -> Objective-C Bridging-header*

**Step 3: Start the ZeroTier service**

Start the service:

```
    zt.start_service("."); // Where the ZeroTier config files for this app will be stored
    zt.join_network(nwid);
```

Listen for incoming connections:

```
    let sock: Int32 = zt.socket(AF_INET, SOCK_STREAM, 0)
    let ztaddr: ZTAddress = ZTAddress(AF_INET, serverAddr, Int16(serverPort))
    let bind_err: Int32 = zt.bind(sock, ztaddr)
    zt_listen(sock, 1);
    let accepted_sock: Int32 = zt.accept(sock, ztaddr)
```

Or, establish a connection:

```
    let sock: Int32 = zt.socket(AF_INET, SOCK_STREAM, 0)
    let ztaddr: ZTAddress = ZTAddress(AF_INET, serverAddr, Int16(serverPort))
    let connect_err: Int32 = zt.connect(sock, ztaddr)
```

**Alternative APIs**

CLick [here](../../../../docs/api_discussion.md) to learn more about alternative APIs such as the Intercept and SOCKS5 Proxy.