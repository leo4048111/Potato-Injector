# Potato Injector
## Info
 A simple GUI CSGO injector with VAC bypass, inject a selected dll to CSGO game process.
## Screenshot
![Screenshot1](https://github.com/leo4048111/Potato-Injector/tree/main/screenshots/screenshot1.png)
**Basic Menu Style**
![Screenshot2](https://github.com/leo4048111/Potato-Injector/tree/main/screenshots/screenshot2.png)
**Control Explained**
## Build Prerequisites
+ Installed Microsoft Visual Studio 2019+ 
+ [BlackBone Static Library](https://github.com/DarthTon/Blackbone), build the project into `BlackBone.lib`(Release build) and `BlackBone-d.lib`(Debug build)
+ Put both .lib files under `$(ProjectDir)\dependency\blackbone\Lib`
## How to use?
+ Click `Patch VAC3`, then steam will close then automatically open.
+ Put dlls in .dll folder.
+ Select the dll to inject, make sure CSGO game is up and running, then click `inject` to start injection.
+ Other labels and controls should be straightforward enough to comprehend.
## Credits
+ https://github.com/b1scoito/cozinha_loader From which I stole some readily available mapping and patching functions.
+ https://github.com/ocornut/imgui]
+ https://github.com/DarthTon/Blackbone
## Notice
***Use this injector at your own risk.***
