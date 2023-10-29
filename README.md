# Potato Injector
## Info
 A simple GUI CSGO injector with VAC bypass, capable of injecting a selected dll into CSGO game process. Latest release supports CS2...
## Screenshot
![Screenshot1](https://raw.githubusercontent.com/leo4048111/Potato-Injector/main/screenshots/screenshot1.png)  
![Screenshot2](https://raw.githubusercontent.com/leo4048111/Potato-Injector/main/screenshots/screenshot2.png)  
**Basic Menu Layout & Explained**
## Build Prerequisites
+ Installed Microsoft Visual Studio 2019+ 
+ Installed [DirectX Software Development Kit](https://www.microsoft.com/en-us/download/details.aspx?id=6812)
+ [BlackBone Static Library](https://github.com/DarthTon/Blackbone), build the project into `BlackBone.lib`(Release build) and `BlackBone-d.lib`(Debug build)
+ Put both .lib files under `$(ProjectDir)\dependency\blackbone\Lib`
## How to use?
+ Click `Patch VAC3`, then steam will close then automatically open.
+ Put all .dll files in `dlls` folder(automatically created).
+ Select the dll to inject, make sure CSGO game is up and running, then click `inject` to start injection.
+ Other labels and controls should be straightforward enough to comprehend.
## Credits
+ https://github.com/b1scoito/cozinha_loader From which I stole some readily available mapping and patching functions.
+ https://github.com/ocornut/imgui
+ https://github.com/DarthTon/Blackbone
## Notice
***Use this injector at your own risk.***
## Update
*** Compatibility updates, now works on CS2 smoothly...
