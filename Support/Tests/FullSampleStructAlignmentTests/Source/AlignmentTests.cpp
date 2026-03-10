#include <cstdint>

#include <donut/core/math/math.h>
using namespace donut::math;

#include "SharedShaderInclude/ShaderDebug/ReSTIRShaderDebugParameters.h"
#include "SharedShaderInclude/ShaderDebug/ShaderDebugPrintShared.h"
#include "SharedShaderInclude/ShaderDebug/PTPathViz/PTPathVizConstants.h"
#include "SharedShaderInclude/ShaderDebug/ReservoirSubfieldVizPasses/DIReservoirVizParameters.h"
#include "SharedShaderInclude/ShaderDebug/ReservoirSubfieldVizPasses/GIReservoirVizParameters.h"
#include "SharedShaderInclude/ShaderDebug/ReservoirSubfieldVizPasses/PTReservoirVizParameters.h"
#include "SharedShaderInclude/BRDFPTParameters.h"
#include "SharedShaderInclude/PTParameters.h"
#include "SharedShaderInclude/ShaderParameters.h"

// Compile-time tests to ensure that all structs intended for use in
// constant buffers are 16-byte aligned.

static_assert(sizeof(PolymorphicLightInfo) % 16 == 0);

static_assert(sizeof(ShaderPrintCBData) % 16 == 0);

static_assert(sizeof(PTPathVizPreprocessConstants) % 16 == 0);
static_assert(sizeof(PTPathVizLineConstants) % 16 == 0);

static_assert(sizeof(DIReservoirVizParameters) % 16 == 0);
static_assert(sizeof(GIReservoirVizParameters) % 16 == 0);
static_assert(sizeof(PTReservoirVizParameters) % 16 == 0);

static_assert(sizeof(BRDFPathTracing_MaterialOverrideParameters) % 16 == 0);
static_assert(sizeof(BRDFPathTracing_SecondarySurfaceReSTIRDIParameters) % 16 == 0);
static_assert(sizeof(BRDFPathTracing_Parameters) % 16 == 0);

static_assert(sizeof(PTNeeParameters) % 16 == 0);
static_assert(sizeof(PTParameters) % 16 == 0);

static_assert(sizeof(ReSTIRShaderDebugParameters) % 16 == 0);

static_assert(sizeof(ResamplingConstants) % 16 == 0);