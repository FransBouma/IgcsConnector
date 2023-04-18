# IGCS Connector
ReShade 5.8+ addin to perform various actions within ReShade for connected camera tools. Features include: create automated horizontal panorama shots and lightfield shots and recording and playback of ReShade shaders during a camera path. Additionally, it also shows live camera information
obtained from the game engine. The documentation page of the camera tools for a particular game on the [Otis Photomode Mods site](https://opm.fransbouma.com) will 
mention if it supports the IGCSConnector and which version.

There's no 32bit version as there's no 32bit IGCS camera system prepared with the required API nor will there be in the future.

Requirements: ReShade 5.8+ with addon support, supported IGCS based camera tools.

## How to use
Place the `Igcsconnector.addon64` in the same folder as where the game exe is located. This is in most cases the same folder as where the ReShade 5.8+ dll
is located. Only for games which use Vulkan, the ReShade dll is likely elsewhere. For Unreal Engine powered games there might be two
game exe's: one in the game's installation folder, and one in a folder deeper into that folder, e.g. 
`GameName\Binaries\Win64\GameName-Win64-Shipping.exe`; the IGCS Connector addon has to be in that second folder, in our example:
`GameName\Binaries\Win64`. ReShade has to be placed in that folder as well.

Be sure to use the ReShade version which supports Addons (so the unsigned version). When you start your game, the `Addons` tab in 
the ReShade gui should show the IGCS Connector information and controls. 

To obtain the unsigned ReShade version, go to <https://ReShade.me> and click on *Download*, then on the bottom green button to download the **ReShade version with Add-on support**. 

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

### Recording and playback of ReShade states during camera paths

Starting with v2.0, the IGCSConnector will record the active ReShade state (which is: which effects are enabled with which parameters) when you add or update a 
camera path node in the IGCSClient of the connected camera tools. When you play back the camera path, the IGCSConnector will interpolate between the two states of
the camera path nodes the camera path is currently in. This way you can add powerful effects along your camera paths like dynamic depth of field, fog or any 
other ReShade effect you have at your disposal. 

If you don't want to record reshade effects, you can disable it by unchecking the *Record ReShade state with camera nodes* checkbox in the addon's settings in 
the ReShade overlay. 

This all might sound complicated, so let's go through a tutorial.

#### How to record and playback reshade efffects along a camera path

Say you created a camera path with four nodes and have no ReShade effects active. Playing back this camera path will change no effect within reshade. Now you 
want to add some Depth of Field starting at node two and ending at the end. Open the camera path window in the IGCS Client at the bottom and move to node two 
on the path. Now go to the game window and open ReShade and enable the Depth of Field shader you want to use, e.g. Cinematic DOF. Use manual focusing and
set up the focus and blur settings the way you like it for that node. To store the ReShade state for this node, go back to the camera path window and click 
the pencil icon next to the node. 

When you move to node three and four you'll see the Depth of Field shader is now also active for these nodes. What's left is to setup the focus and blur for 
node three and four. So in the camera path window move to node three and go back to the reshade window and change the Depth of Field focus and blur so it's 
correct for where the camera should be focusing on. When you're done, go back to the camera path window and click the pencil icon again to update the 
camera path node. 

Do the same for node four. When you now play back the camera path you'll see the Depth of Field effect being switched on at node two and it's updated along 
the path while the camera is moving. The IGCSConnector addon will interpolate between the settings for the Depth of Field shader (and other effects you 
might have enabled) of the camera path nodes. As on node one no Depth of Field shader was active it's not enabled. 

To fix that, go back to the camera path window, move to node one and then go back to reshade and enable the Depth of Field shader, and e.g. set the blur to 0.0. 
Don't forget to click the pencil icon in the camera path window. 

#### What's interpolated

The IGCSConnector will interpolate only floating point values. So if you change a value that's not a floating point value (a floating point value is a value with
a '.' in it, so 1.3 is a floating point value, 1 isn't) it can't be interpolated and it won't change during camera path playback. The same goes for other types
of settings like drop down lists and e.g. checkboxes. 

You can enable as much reshade effects as you like, so go wild!

## Supported cameras

Camera's build with the latest IGCS system are supported. All cameras are available on my [Patreon](https://patreon.com/Otis_Inf). Please check 
the [camera documentation site](https://opm.fransbouma.com) for details per camera if they support the IGCSConnector and which version. 

