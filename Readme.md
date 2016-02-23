# Direct3D 12 port of AntTweakBar

Needs Visual studio 2015 and the Direct3D sdk that comes with it.

There are currently no binaries, you need to compile it yourself.

Init like this.
```C
// 'pDevice' should be a 'ID3D12Device *'
TwInit( TW_DIRECT3D12, pDevice );
````

Draw like this.
```C
// 'pGraphicsCommandList' should be a open 'ID3D12GraphicsCommandList *', close it and execute after the call.
TwDrawContext( pGraphicsCommandList );
```

There is also a couple of minor changes not related to D3D12. 
```C
// Set the height of the mesh drawn for a Quaternion / Direction / Axis.
// Its is expressed in rows and is multiplied by the height of a character tow / height of the font.
// Max rows is 16
TwAddVarRW(twBar, "Camera", TW_TYPE_QUAT4F, &twRotation, "opened=true rows=8");

// You can define sub groups with the '/' divider.
// This will create 3 groups and place "Clear color" in the last group, "Framebuffer", all groups are created open.
TwAddVarRW(twBar, "Clear color", TW_TYPE_COLOR3F, &clearColor, "opened=false group='Misc/Colors/Framebuffer'");

// When defining groups you can set them as open/closed using '+'/'-', if they exist their opend state is changed.
// This will create or modify 2 groups, "Meshes" will be open, "Modifier" will be closed.
TwAddVarRW(twBar, "Scale", TW_TYPE_FLOAT, &scale, "group='-Meshes/+Modifiers'");
```

If you find bugs open issues, or better yet, send pull requests with fixes.

### Original Readme.txt from AntTweakBar development library follows

AntTweakBar is a small and easy-to-use C/C++ library that allows programmers
to quickly add a light and intuitive GUI into OpenGL and DirectX based 
graphic programs to interactively tweak parameters.

This package includes the development version of the AntTweakBar library 
for Windows, GNU/Linux and OSX, and some program examples (sources + binaries).

For installation and documentation please refer to:
http://anttweakbar.sourceforge.net/doc
