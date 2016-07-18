iOS + ZeroTier SDK
====

Welcome!

Imagine a flat, encrypted, no-configuration LAN for all of the instances of your iOS app. 

This short tutorial will show you how to enable ZeroTier functionality for your iOS app with little to no code modification. Check out our [ZeroTier SDK](https://www.zerotier.com/blog) page for more info on how the integration works and [Shim Techniques](shims_zt_sdk.md) for a discussion of shims available for your app/technology.

In this example we aim to set up a minimal XCode project which contains all of the components necessary to enable ZeroTier for your app. If you'd rather skip all of these steps and grab the project code, look in the [sdk/iOS](https://github.com/zerotier/ZeroTierSDK/tree/dev/sdk/integrations/apple) folder of the source tree. Otherwise, let's get started!

***
**Step 1: Build iOS framework**

- `make ios_app_framework`
- This will output to `build/ios_app_framework/Release-iphoneos/ZeroTierSDK_iOS.framework`

**Step 2: Integrate SDK into project**

- Add the resultant framework package to your project
- Add `zerotiersdk/src` directory to `Build Settings -> Header Search Paths`
- Add `build/ios_app_framework/Release-iphoneos/` to *Build Settings -> Framework Search Paths*
- Add `ZeroTierSDK.frameworkiOS` to *General->Embedded Binaries*
- Add `src/SDK_XcodeWrapper.cpp` and `src/SDK_XcodeWrapper.hpp` to your project:
- Set `src/SDK_Apple-Bridging-Header.h` as your bridging-header in `Build Settings -> Objective-C Bridging-header`

**Step 3: Start the ZeroTier service**

Now find a place in your code to set up the ZeroTier service thread:

```
var service_thread : NSThread!
func zt_start_service() {
    let path = NSSearchPathForDirectoriesInDomains(NSSearchPathDirectory.DocumentDirectory, NSSearchPathDomainMask.UserDomainMask, true)
    start_service(path[0])
}
```

and then start it:

```
dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), {
    self.service_thread = NSThread(target:self, selector:"zt_start_service", object:nil)
    self.service_thread.start()
});
```

*NOTE: If you enabled the proxy service via `-DUSE_SOCKS_PROXY` it will start automatically and be reachable at `0.0.0.0:1337`*

**Step 4: Pick an API**

This integration allows for the following shim combinations:
- `Hook of BSD-like sockets`: Use BSD-like sockets as you normally would.
- `Proxy of NSStream`: Create NSStream. Configure stream for SOCKS5 Proxy. Use stream.
- `Direct Call`: Consult [src/SDK_Apple-Bridging-Header.h](src/SDK_Apple-Bridging-Header.h).

If functional interposition isn't available for the API or library you've chosen to use, ZeroTier offers a SOCKS5 proxy server which can allow connectivity to your virtual network as long as your client API supports the SOCKS5 protocol. This proxy service will run alongside the tap service and can be turned on by compiling with the `-DUSE_SOCKS_PROXY` flag in *Build Settings->Other C Flags*. By default, the proxy service is available at `0.0.0.0:1337`.

**Step 5: Join a network!**

Simply call `zt_join_network("XXXXXXXXXXXXXXXX")`

***
**NSStream and SOCKS Proxy:**

As an example, here's how one would configure a NSStream object to redirect all network activity to the ZeroTier SOCKS proxy server:

```
// BEGIN proxy configuration
let myDict:NSDictionary = [NSStreamSOCKSProxyHostKey : "0.0.0.0",
                           NSStreamSOCKSProxyPortKey : 1337,
                           NSStreamSOCKSProxyVersionKey : NSStreamSOCKSProxyVersion5]

inputStream!.setProperty(myDict, forKey: NSStreamSOCKSProxyConfigurationKey)
outputStream!.setProperty(myDict, forKey: NSStreamSOCKSProxyConfigurationKey)
// END proxy configuration
```

