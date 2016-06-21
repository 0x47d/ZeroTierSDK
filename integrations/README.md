ZeroTier Integrations
====

If you want everything built at once, type `make all` and go play outside for a little while, we'll copy all of the targets into the `build` directory for you along with specific instructions on how to use each binary.

*NOTE: For iOS/OSX Frameworks and Bundles to build, you will need XCode command line tools `xcode-select --install`, for Android JNI libraries to build you'll need to install [Android Studio](https://developer.android.com/studio/index.html), if you don't have these things install we will detect that and just skip those builds automatically.*

Below are the specific instructions for each integration requiring little to no modification to your code. Remember, with a full build we'll put a copy of the appropriate integration instructions in the resultant binary's folder for you anyway. 

For more support on these integrations take a look at the [docs](../docs) folder. Specifically the [Shims](../docs/shims_zt_sdk.md) and [SDK API](../docs/zt_sdk.md) overviews. 
Also stop by our [community section](https://www.zerotier.com/community/) for more in-depth discussion!

***
## Current Integrations


### Apple
##### iOS
 - [Embedding within an app](../docs/ios_zt_sdk.md)
 - [Unity3D plugin](../docs/unity3d_ios_zt_sdk.md)

##### OSX
 - [Embedding within an app](../docs/osx_zt_sdk.md) 
 - [Dynamic-linking into an app/service at runtime](../docs/osx_zt_sdk.md) 
 - [Unity3D plugin](../docs/unity3d_osx_zt_sdk.md) 

***
### Linux
 - [Dynamic-linking into an app/service at runtime](../docs/linux_zt_sdk.md) 
 - [Using the SDK with Docker](../docs/docker_linux_zt_sdk.md)

### Android
 - [Embedding within an app](../docs/android_zt_sdk.md) 
 - [Unity 3D plugin](../docs/unity3d_android_zt_sdk.md) 

***
### Windows
 - Anyone want to volunteer?
