# Debug Utilities

The RTXDI SDK contains several tools for debugging and visualizing data.

## Shader Debug Print

Shader debug printing is supported in D3D12 for a single pixel, which is selected by mouse click. Enabling it requires setting `SHADER_DEBUG_PRINT_ENABLED` to 1, as well as binding several buffers to the shader and running the proper initialization code, which can be seen in the `RAB_Buffers.hlsli` and `GenerateInitialSamples.hlsl` files. Once enabled, the `DebugPrint_` function can send formatted strings, which support primitive types such as scalars and vectors, back to the host side. When run from the MSVS debugger, this output shows up in the Output panel. When run standalone, it can be seen by running [Debug View](https://learn.microsoft.com/en-us/sysinternals/downloads/debugview). The `RuntimeShaderDebugPrintUtilities.hlsli` file contains helper functions for printing the `RTXDI_PTReservoir` and `RTXDI_PTPathTraceInvocationType` types.

`DebugPrint_` works like printf, but uses braced numbers instead of percentage signs to send out variables. For example, `DebugPrint_("Hello pixel: {0}", pixelPos);` and `DebugPrint_("Diffuse is {0} and specular is {1}", diffuse, specular);` will work as expected.

**Note:** `DebugPrint_` degrades shader performance proportional to its usage - the more and longer the strings, the slower the shader gets. Performance should be fine for debugging, but don't forget to turn it off to measure performance.

**Note:** There are rare cases when `DebugPrint_` may cause general shader corruption for unknown reasons, typically from overuse. Try disabling `DebugPrint_` if you find the sample has gone haywire.

## Path Visualization

The path visualization tool tracks the bounces of the path tracer and records each hit, normal vector, and NEE sample, if one is taken. All five path tracing invocations are supported - initial sampling, temporal shift, temporal inverse shift, spatial shift, and spatial inverse shift. Two additional passes of temporal retrace and spatial retrace re-run the path tracer from the temporal or spatial neighbor's primary surface to the maximum bounce depth. Paths can be individually enabled and colored.

Performance note: Enabling path visualization causes a minor hit to performance and is compiled out by default. To enable it, set the `PT_PATH_VIZ_ENABLED` define to 1 and recompile. The two temporal and spatial retrace options are additionally toggled by the `RTXDI_DEBUG` define, which lives in the runtime rather than in the sample, and it must also be set to 1 to enable them.

**Note:** Sometimes the path visualization tool ends up adding an extra vertex (and often normal and NEE ray) at the camera position as a result of encountering stale data. This shows up as a line jumping across the screen. Just try clicking again to get a different path.

## Intermediate Buffer Visualization

The Intermediate Buffer Visualization menu in the GUI allows users to visualize buffer data in several ways

- Intermediate Texture Display blits the selected buffer directly to the screen. The list includes GBuffer layers and their PSR counterparts, as well as lighting components such as direct and indirect illumination.
- Reservoir Subfield Display sends the chosen field of the selected reservoir type to the screen, transforming some values to help in visualization.
- NRD Validation blits the NRD validation texture to the screen, with the option of overlaying it on top of the regularly rendered output.
