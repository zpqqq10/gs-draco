<p align="center">
<img width="350px" src="docs/artwork/draco3d-vert.svg" />
</p>

Description
===========

Draco is a library for compressing and decompressing 3D geometric [meshes] and
[point clouds]. It is intended to improve the storage and transmission of 3D
graphics. This repo is built upon Draco 1.5.7 and support compressing all gaussian attributes.

Draco was designed and built for compression efficiency and speed. The code
supports compressing points, connectivity information, texture coordinates,
color information, normals, and any other generic attributes associated with
geometry. With Draco, applications using 3D graphics can be significantly
smaller without compromising visual fidelity. For users, this means apps can
now be downloaded faster, 3D graphics in the browser can load quicker, and VR
and AR scenes can now be transmitted with a fraction of the bandwidth and
rendered quickly.

Draco is released as C++ source code that can be used to compress 3D graphics
as well as C++ and Javascript decoders for the encoded data.

_**Contents**_

- [Description](#description)
- [Building](#building)
- [Usage](#usage)
  - [Unity](#unity)
  - [WASM and JavaScript Decoders](#wasm-and-javascript-decoders)
  - [Command Line Applications](#command-line-applications)
  - [Encoding Tool](#encoding-tool)
  - [Encoding Point Clouds](#encoding-point-clouds)
  - [Decoding Tool](#decoding-tool)
  - [glTF Transcoding Tool](#gltf-transcoding-tool)
  - [C++ Decoder API](#c-decoder-api)
  - [Javascript Encoder API](#javascript-encoder-api)
  - [Javascript Decoder API](#javascript-decoder-api)
  - [Javascript Decoder Performance](#javascript-decoder-performance)
  - [Metadata API](#metadata-api)
  - [NPM Package](#npm-package)
  - [three.js Renderer Example](#threejs-renderer-example)
- [GStatic Javascript Builds](#gstatic-javascript-builds)
- [Support](#support)
- [License](#license)
- [References](#references)

Building
========
See [BUILDING](BUILDING.md) for building instructions.

Usage
======

Unity
-----
For the best information about using Unity with Draco please visit https://github.com/atteneder/DracoUnity

For a simple example of using Unity with Draco see [README](unity/README.md) in the unity folder.

WASM and JavaScript Decoders
----------------------------

It is recommended to always pull your Draco WASM and JavaScript decoders from:

~~~~~ bash
https://www.gstatic.com/draco/v1/decoders/
~~~~~

Users will benefit from having the Draco decoder in cache as more sites start using the static URL.

Command Line Applications
------------------------

The default target created from the build files will be the `draco_encoder`
and `draco_decoder` command line applications. Additionally, `draco_transcoder`
is generated when CMake is run with the DRACO_TRANSCODER_SUPPORTED variable set
to ON (see [BUILDING](BUILDING.md#transcoder) for more details). For all
applications, if you run them without any arguments or `-h`, the applications
will output usage and options.

Encoding Tool
-------------

`draco_encoder` will read OBJ, STL or PLY files as input, and output
Draco-encoded files. We have included Stanford's [Bunny] mesh for testing. The
basic command line looks like this:

~~~~~ bash
./draco_encoder -i testdata/bun_zipper.ply -o out.drc
~~~~~

A value of `0` for the quantization parameter will not perform any quantization
on the specified attribute. Any value other than `0` will quantize the input
values for the specified attribute to that number of bits. For example:

~~~~~ bash
./draco_encoder -i testdata/bun_zipper.ply -o out.drc -qp 14
~~~~~

will quantize the positions to 14 bits (default is 11 for the position
coordinates).

In general, the more you quantize your attributes the better compression rate
you will get. It is up to your project to decide how much deviation it will
tolerate. In general, most projects can set quantization values of about `11`
without any noticeable difference in quality.

The compression level (`-cl`) parameter turns on/off different compression
features.

~~~~~ bash
./draco_encoder -i testdata/bun_zipper.ply -o out.drc -cl 8
~~~~~

In general, the highest setting, `10`, will have the most compression but
worst decompression speed. `0` will have the least compression, but best
decompression speed. The default setting is `7`.

Encoding Point Clouds
---------------------

You can encode point cloud data with `draco_encoder` by specifying the
`-point_cloud` parameter. If you specify the `-point_cloud` parameter with a
mesh input file, `draco_encoder` will ignore the connectivity data and encode
the positions from the mesh file.

~~~~~ bash
./draco_encoder -point_cloud -i testdata/bun_zipper.ply -o out.drc
~~~~~

This command line will encode the mesh input as a point cloud, even though the
input might not produce compression that is representative of other point
clouds. Specifically, one can expect much better compression rates for larger
and denser point clouds.

Decoding Tool
-------------

`draco_decoder` will read Draco files as input, and output OBJ, STL or PLY
files. The basic command line looks like this:

~~~~~ bash
./draco_decoder -i in.drc -o out.obj
~~~~~

glTF Transcoding Tool
---------------------

`draco_transcoder` can be used to add Draco compression to glTF assets. The
basic command line looks like this:

~~~~~ bash
./draco_transcoder -i in.glb -o out.glb
~~~~~

This command line will add geometry compression to all meshes in the `in.glb`
file. Quantization values for different glTF attributes can be specified
similarly to the `draco_encoder` tool. For example `-qp` can be used to define
quantization of the position attribute:

~~~~~ bash
./draco_transcoder -i in.glb -o out.glb -qp 12
~~~~~

C++ Decoder API
---------------

If you'd like to add decoding to your applications you will need to include
the `draco_dec` library. In order to use the Draco decoder you need to
initialize a `DecoderBuffer` with the compressed data. Then call
`DecodeMeshFromBuffer()` to return a decoded mesh object or call
`DecodePointCloudFromBuffer()` to return a decoded `PointCloud` object. For
example:

~~~~~ cpp
draco::DecoderBuffer buffer;
buffer.Init(data.data(), data.size());

const draco::EncodedGeometryType geom_type =
    draco::GetEncodedGeometryType(&buffer);
if (geom_type == draco::TRIANGULAR_MESH) {
  unique_ptr<draco::Mesh> mesh = draco::DecodeMeshFromBuffer(&buffer);
} else if (geom_type == draco::POINT_CLOUD) {
  unique_ptr<draco::PointCloud> pc = draco::DecodePointCloudFromBuffer(&buffer);
}
~~~~~

Please see [src/draco/mesh/mesh.h](src/draco/mesh/mesh.h) for the full `Mesh` class interface and
[src/draco/point_cloud/point_cloud.h](src/draco/point_cloud/point_cloud.h) for the full `PointCloud` class interface.


Javascript Encoder API
----------------------
The Javascript encoder is located in `javascript/draco_encoder.js`. The encoder
API can be used to compress mesh and point cloud. In order to use the encoder,
you need to first create an instance of `DracoEncoderModule`. Then use this
instance to create `MeshBuilder` and `Encoder` objects. `MeshBuilder` is used
to construct a mesh from geometry data that could be later compressed by
`Encoder`. First create a mesh object using `new encoderModule.Mesh()` . Then,
use `AddFacesToMesh()` to add indices to the mesh and use
`AddFloatAttributeToMesh()` to add attribute data to the mesh, e.g. position,
normal, color and texture coordinates. After a mesh is constructed, you could
then use `EncodeMeshToDracoBuffer()` to compress the mesh. For example:

~~~~~ js
const mesh = {
  indices : new Uint32Array(indices),
  vertices : new Float32Array(vertices),
  normals : new Float32Array(normals)
};

const encoderModule = DracoEncoderModule();
const encoder = new encoderModule.Encoder();
const meshBuilder = new encoderModule.MeshBuilder();
const dracoMesh = new encoderModule.Mesh();

const numFaces = mesh.indices.length / 3;
const numPoints = mesh.vertices.length;
meshBuilder.AddFacesToMesh(dracoMesh, numFaces, mesh.indices);

meshBuilder.AddFloatAttributeToMesh(dracoMesh, encoderModule.POSITION,
  numPoints, 3, mesh.vertices);
if (mesh.hasOwnProperty('normals')) {
  meshBuilder.AddFloatAttributeToMesh(
    dracoMesh, encoderModule.NORMAL, numPoints, 3, mesh.normals);
}
if (mesh.hasOwnProperty('colors')) {
  meshBuilder.AddFloatAttributeToMesh(
    dracoMesh, encoderModule.COLOR, numPoints, 3, mesh.colors);
}
if (mesh.hasOwnProperty('texcoords')) {
  meshBuilder.AddFloatAttributeToMesh(
    dracoMesh, encoderModule.TEX_COORD, numPoints, 3, mesh.texcoords);
}

if (method === "edgebreaker") {
  encoder.SetEncodingMethod(encoderModule.MESH_EDGEBREAKER_ENCODING);
} else if (method === "sequential") {
  encoder.SetEncodingMethod(encoderModule.MESH_SEQUENTIAL_ENCODING);
}

const encodedData = new encoderModule.DracoInt8Array();
// Use default encoding setting.
const encodedLen = encoder.EncodeMeshToDracoBuffer(dracoMesh,
                                                   encodedData);
encoderModule.destroy(dracoMesh);
encoderModule.destroy(encoder);
encoderModule.destroy(meshBuilder);

~~~~~
Please see [src/draco/javascript/emscripten/draco_web_encoder.idl](src/draco/javascript/emscripten/draco_web_encoder.idl) for the full API.

Javascript Decoder API
----------------------

The Javascript decoder is located in [javascript/draco_decoder.js](javascript/draco_decoder.js). The
Javascript decoder can decode mesh and point cloud. In order to use the
decoder, you must first create an instance of `DracoDecoderModule`. The
instance is then used to create `DecoderBuffer` and `Decoder` objects. Set
the encoded data in the `DecoderBuffer`. Then call `GetEncodedGeometryType()`
to identify the type of geometry, e.g. mesh or point cloud. Then call either
`DecodeBufferToMesh()` or `DecodeBufferToPointCloud()`, which will return
a Mesh object or a point cloud. For example:

~~~~~ js
// Create the Draco decoder.
const decoderModule = DracoDecoderModule();
const buffer = new decoderModule.DecoderBuffer();
buffer.Init(byteArray, byteArray.length);

// Create a buffer to hold the encoded data.
const decoder = new decoderModule.Decoder();
const geometryType = decoder.GetEncodedGeometryType(buffer);

// Decode the encoded geometry.
let outputGeometry;
let status;
if (geometryType == decoderModule.TRIANGULAR_MESH) {
  outputGeometry = new decoderModule.Mesh();
  status = decoder.DecodeBufferToMesh(buffer, outputGeometry);
} else {
  outputGeometry = new decoderModule.PointCloud();
  status = decoder.DecodeBufferToPointCloud(buffer, outputGeometry);
}

// You must explicitly delete objects created from the DracoDecoderModule
// or Decoder.
decoderModule.destroy(outputGeometry);
decoderModule.destroy(decoder);
decoderModule.destroy(buffer);
~~~~~

Please see [src/draco/javascript/emscripten/draco_web_decoder.idl](src/draco/javascript/emscripten/draco_web_decoder.idl) for the full API.

Javascript Decoder Performance
------------------------------

The Javascript decoder is built with dynamic memory. This will let the decoder
work with all of the compressed data. But this option is not the fastest.
Pre-allocating the memory sees about a 2x decoder speed improvement. If you
know all of your project's memory requirements, you can turn on static memory
by changing `CMakeLists.txt` accordingly.

Metadata API
------------
Starting from v1.0, Draco provides metadata functionality for encoding data
other than geometry. It could be used to encode any custom data along with the
geometry. For example, we can enable metadata functionality to encode the name
of attributes, name of sub-objects and customized information.
For one mesh and point cloud, it can have one top-level geometry metadata class.
The top-level metadata then can have hierarchical metadata. Other than that,
the top-level metadata can have metadata for each attribute which is called
attribute metadata. The attribute metadata should be initialized with the
correspondent attribute id within the mesh. The metadata API is provided both
in C++ and Javascript.
For example, to add metadata in C++:

~~~~~ cpp
draco::PointCloud pc;
// Add metadata for the geometry.
std::unique_ptr<draco::GeometryMetadata> metadata =
  std::unique_ptr<draco::GeometryMetadata>(new draco::GeometryMetadata());
metadata->AddEntryString("description", "This is an example.");
pc.AddMetadata(std::move(metadata));

// Add metadata for attributes.
draco::GeometryAttribute pos_att;
pos_att.Init(draco::GeometryAttribute::POSITION, nullptr, 3,
             draco::DT_FLOAT32, false, 12, 0);
const uint32_t pos_att_id = pc.AddAttribute(pos_att, false, 0);

std::unique_ptr<draco::AttributeMetadata> pos_metadata =
    std::unique_ptr<draco::AttributeMetadata>(
        new draco::AttributeMetadata(pos_att_id));
pos_metadata->AddEntryString("name", "position");

// Directly add attribute metadata to geometry.
// You can do this without explicitly add |GeometryMetadata| to mesh.
pc.AddAttributeMetadata(pos_att_id, std::move(pos_metadata));
~~~~~

To read metadata from a geometry in C++:

~~~~~ cpp
// Get metadata for the geometry.
const draco::GeometryMetadata *pc_metadata = pc.GetMetadata();

// Request metadata for a specific attribute.
const draco::AttributeMetadata *requested_pos_metadata =
  pc.GetAttributeMetadataByStringEntry("name", "position");
~~~~~

Please see [src/draco/metadata](src/draco/metadata) and [src/draco/point_cloud](src/draco/point_cloud) for the full API.

NPM Package
-----------
Draco NPM NodeJS package is located in [javascript/npm/draco3d](javascript/npm/draco3d). Please see the
doc in the folder for detailed usage.

three.js Renderer Example
-------------------------

Here's an [example] of a geometric compressed with Draco loaded via a
Javascript decoder using the `three.js` renderer.

Please see the [javascript/example/README.md](javascript/example/README.md) file for more information.

GStatic Javascript Builds
=========================

Prebuilt versions of the Emscripten-built Draco javascript decoders are hosted
on www.gstatic.com in version labeled directories:

https://www.gstatic.com/draco/versioned/decoders/VERSION/*

As of the v1.4.3 release the files available are:

- [draco_decoder.js](https://www.gstatic.com/draco/versioned/decoders/1.4.3/draco_decoder.js)
- [draco_decoder.wasm](https://www.gstatic.com/draco/versioned/decoders/1.4.3/draco_decoder.wasm)
- [draco_decoder_gltf.js](https://www.gstatic.com/draco/versioned/decoders/1.4.3/draco_decoder_gltf.js)
- [draco_decoder_gltf.wasm](https://www.gstatic.com/draco/versioned/decoders/1.4.3/draco_decoder_gltf.wasm)
- [draco_wasm_wrapper.js](https://www.gstatic.com/draco/versioned/decoders/1.4.3/draco_wasm_wrapper.js)
- [draco_wasm_wrapper_gltf.js](https://www.gstatic.com/draco/versioned/decoders/1.4.3/draco_wasm_wrapper_gltf.js)

Beginning with the v1.5.1 release assertion enabled builds of the following
files are available:

- [draco_decoder.js](https://www.gstatic.com/draco/versioned/decoders/1.5.1/with_asserts/draco_decoder.js)
- [draco_decoder.wasm](https://www.gstatic.com/draco/versioned/decoders/1.5.1/with_asserts/draco_decoder.wasm)
- [draco_wasm_wrapper.js](https://www.gstatic.com/draco/versioned/decoders/1.5.1/with_asserts/draco_wasm_wrapper.js)

Support
=======

For questions/comments please email <draco-3d-discuss@googlegroups.com>

If you have found an error in this library, please file an issue at
<https://github.com/google/draco/issues>

Patches are encouraged, and may be submitted by forking this project and
submitting a pull request through GitHub. See [CONTRIBUTING] for more detail.

License
=======
Licensed under the Apache License, Version 2.0 (the "License"); you may not
use this file except in compliance with the License. You may obtain a copy of
the License at

<http://www.apache.org/licenses/LICENSE-2.0>

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
License for the specific language governing permissions and limitations under
the License.

References
==========
[example]:https://storage.googleapis.com/demos.webmproject.org/draco/draco_loader_throw.html
[meshes]: https://en.wikipedia.org/wiki/Polygon_mesh
[point clouds]: https://en.wikipedia.org/wiki/Point_cloud
[Bunny]: https://graphics.stanford.edu/data/3Dscanrep/
[CONTRIBUTING]: https://raw.githubusercontent.com/google/draco/main/CONTRIBUTING.md

Bunny model from Stanford's graphic department <https://graphics.stanford.edu/data/3Dscanrep/>
