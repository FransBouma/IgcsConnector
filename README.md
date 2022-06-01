# IGCS Connector
Reshade 5.1+ addin to create automated horizontal panorama shots and lightfield shots using IGCS based camera's. It also shows live camera information
obtained from the game engine. It will work with the camera versions listed below as the cameras have to be prepared to work with the connector. 

There's no 32bit version as there's no 32bit IGCS camera system prepared with the required API nor will there be in the future.

## How to use
Place the `Igcsconnector.addon` in the same folder as where the game exe is located. This is in most cases the same folder as where the Reshade 5.1+ dll
is located. Only for games which use Vulkan, the Reshade dll is likely elsewhere. For Unreal Engine powered games there might be two
game exe's: one in the game's installation folder, and one in a folder deeper into that folder, e.g. 
`GameName\Binaries\Win64\GameName-Win64-Shipping.exe`; the IGCS Connector addon has to be in that second folder, in our example:
`GameName\Binaries\Win64`. Reshade has to be placed in that folder as well.

Be sure to use the Reshade version which supports Addons (so the unsigned version). When you start your game, the `Addons` tab in 
the Reshade gui should show the IGCS Connector information and controls. 

To obtain the unsigned Reshade version, go to <https://reshade.me> and click on *Download*, then on the *Other builds* link. This will bring you to the 
release post of the latest Reshade version. Download the addon supporting version at the bottom of the post, using the *ReShade with full add-on support* 
link.

## Available features

### Screenshot taking

The IGCS connector has two screenshot types: horizontal panorama and lightfield. How to take screenshots with these is explained below. Both screenshot types
are taking multiple screenshots in the file format you specified and save them to disk in a pre-defined folder. You need external stitching software like
Microsoft Image Composition Editor or Photoshop to create a single image from the created screenshots. 

#### Horizontal panorama

A horizontal panorama consists of multiple shots taken by rotating the camera horizontally over a specified angle, preferably by overlapping the shots so 
stitching software can auto-layout the shots properly. For a good panorama, it's adviced to use a lower FoV value in the camera tools than you'd usually use

The following controls are available:

- **Screenshot output directory**: This is the root folder in which the shot folders are stored. Every session is stored in its own folder inside this folder, using the type and the date/time.
- **Number of frames to wait between steps**: This is the # of frames the addon will wait between each shot. Set this to a fairly high number if the game you're taking shots of needs several frames to build up the final image, e.g. because of raytracing or TAA
- **Multi-screenshot type**: This is set to Horizontal panorama in this case
- **File type**: The output file type. By default this is jpeg (98% max quality). 
- **Total field of view in panorama (in degrees)**: The total angle over which the shots are taken. The end result is a shot with a view angle of this angle. 
- **Percentage of overlap**: The higher value you specify the more shots are taken. 

#### Lightfield

A lightfield is a series of shots taken over a horizontal rail which are combined with specific software into a 3D 'lightfield' image which can be viewed
on 3D displays like the ones from [Looking Glass Factory](https://lookingglassfactory.com) 

The following controls are available:

- **Screenshot output directory**: This is the root folder in which the shot folders are stored. Every session is stored in its own folder inside this folder, using the type and the date/time.
- **Number of frames to wait between steps**: This is the # of frames the addon will wait between each shot. Set this to a fairly high number if the game you're taking shots of needs several frames to build up the final image, e.g. because of raytracing or TAA
- **Multi-screenshot type**: This is set to Lightfield in this case
- **File type**: The output file type. By default this is jpeg (98% max quality). 
- **Distance between Lightfield shots**: This is the step size, in world units, for the camera to step for each shot. Some engines have coordinates which are close together so you need a larger value, others have coordinates stretched out over the world so you need small values. 
- **Number of shots to take**: The number of shots to take in a session. 

#### Starting the session
When you enable the camera in the camera tools, you'll see two buttons: *Start screenshot session* and *Start test run*. The *Start test run* button will
perform the same action as the *Start screenshot session* but without taking and writing shots to disk. You can use this to check whether you wait enough 
between shots, have the right angles setup or the right distance specified etc. 

Clicking *Start screenshot session* will, if everything is ok, start a screenshot session, rotate the camera and take shots. After the session has been 
completed, it'll write the shots to disk in a new folder inside the root folder. 

If the camera is disabled the buttons aren't available and instead a text is shown which explains the camera is disabled.

### Camera tools info

This section displays live information about the camera in the engine, like coordinates, angles, rotation matrix up/front/right vectors, the current Field of
View (fov) in degrees and the camera quaternion. 

This information is read-only. 

## Supported cameras

The following cameras are supported with this addon: (All cameras are available on my [Patreon](https://patreon.com/Otis_Inf)

Camera tools | Version
--|--
Universal Unreal Engine Unlocker | v4.3+
The Witcher 3: Wild Hunt | v1.0.5
