# Direct3D 12 port of AntTweakBar

Needs Visual studio 2015 and the Direct3D sdk that comes with it.

There are currently no binaries, you need to compile it yourself.

Init like this.
```C
// pDevice should be a ID3D12Device *
TwInit( TW_DIRECT3D12, pDevice );
````

Draw like this.
```C
// pGraphicsCommandList`should be a open ID3D12GraphicsCommandList *, close it and execute after the call.
TwDrawContext( pGraphicsCommandList );
```

There is also a minor D3D12 unrelated change to set the height of quaternion, normals and axis angle widgets. 
Use like this, `TwAddVarRW(twBar, "Camera", TW_TYPE_QUAT4F, &twRotation, "opened=true rows=8");` 
rows is the height of the widget as the height in text rows. 
If the font is 10 pixels high, the height of the widget would be 80. Max is 16 rows.

If you find bugs open issues, or better yet, send pull requests with fixes.

### Original Readme.txt from AntTweakBar development library follows

AntTweakBar is a small and easy-to-use C/C++ library that allows programmers
to quickly add a light and intuitive GUI into OpenGL and DirectX based 
graphic programs to interactively tweak parameters.

This package includes the development version of the AntTweakBar library 
for Windows, GNU/Linux and OSX, and some program examples (sources + binaries).

For installation and documentation please refer to:
http://anttweakbar.sourceforge.net/doc
