/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    minipairs.h

Abstract:

    Local audio endpoint filter definitions. 
--*/

#ifndef _VIRTUALAUDIODRIVER_MINIPAIRS_H_
#define _VIRTUALAUDIODRIVER_MINIPAIRS_H_

#include "speakertopo.h"
#include "speakertoptable.h"
#include "speakerwavtable.h"

#include "micarraytopo.h"
#include "micarray1toptable.h"
#include "micarraywavtable.h"


NTSTATUS
CreateMiniportWaveRTVirtualAudioDriver
( 
    _Out_       PUNKNOWN *,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN,
    _In_        POOL_FLAGS,
    _In_        PUNKNOWN,
    _In_opt_    PVOID,
    _In_        PENDPOINT_MINIPAIR
);

NTSTATUS
CreateMiniportTopologyVirtualAudioDriver
( 
    _Out_       PUNKNOWN *,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN,
    _In_        POOL_FLAGS,
    _In_        PUNKNOWN,
    _In_opt_    PVOID,
    _In_        PENDPOINT_MINIPAIR
);

//
// Render miniports.
//

/*********************************************************************
* Topology/Wave bridge connection for speaker (internal)             *
*                                                                    *
*              +------+                +------+                      *
*              | Wave |                | Topo |                      *
*              |      |                |      |                      *
* System   --->|0    1|--------------->|0    1|---> Line Out         *
*              |      |                |      |                      *
*              +------+                +------+                      *
*********************************************************************/
static
PHYSICALCONNECTIONTABLE SpeakerTopologyPhysicalConnections[] =
{
    {
        KSPIN_TOPO_WAVEOUT_SOURCE,  // TopologyIn
        KSPIN_WAVE_RENDER3_SOURCE,   // WaveOut
        CONNECTIONTYPE_WAVE_OUTPUT
    }
};

#define DEFINE_RENDER_CABLE(var, topo, wave, index)                       \
static ENDPOINT_MINIPAIR var =                                             \
{                                                                          \
    eSpeakerDevice,                                                        \
    topo,                                                                  \
    NULL,                                                                  \
    CreateMiniportTopologyVirtualAudioDriver,                              \
    &SpeakerTopoMiniportFilterDescriptor,                                  \
    0, NULL,                                                               \
    wave,                                                                  \
    NULL,                                                                  \
    CreateMiniportWaveRTVirtualAudioDriver,                                \
    &SpeakerWaveMiniportFilterDescriptor,                                  \
    0,                                                                     \
    NULL,                                                                  \
    SPEAKER_DEVICE_MAX_CHANNELS,                                           \
    SpeakerPinDeviceFormatsAndModes,                                       \
    SIZEOF_ARRAY(SpeakerPinDeviceFormatsAndModes),                         \
    SpeakerTopologyPhysicalConnections,                                    \
    SIZEOF_ARRAY(SpeakerTopologyPhysicalConnections),                      \
    ENDPOINT_NO_FLAGS,                                                     \
    index                                                                  \
};

DEFINE_RENDER_CABLE(RenderCable1, L"TopologyCable1", L"WaveCable1", 0)
DEFINE_RENDER_CABLE(RenderCable2, L"TopologyCable2", L"WaveCable2", 1)
DEFINE_RENDER_CABLE(RenderCable3, L"TopologyCable3", L"WaveCable3", 2)
DEFINE_RENDER_CABLE(RenderCable4, L"TopologyCable4", L"WaveCable4", 3)
//
// Capture miniports.
//

/*********************************************************************
* Topology/Wave bridge connection for mic array  1 (front)           *
*                                                                    *
*              +------+    +------+                                  *
*              | Topo |    | Wave |                                  *
*              |      |    |      |                                  *
*  Mic in  --->|0    1|===>|0    1|---> Capture Host Pin             *
*              |      |    |      |                                  *
*              +------+    +------+                                  *
*********************************************************************/
static
PHYSICALCONNECTIONTABLE MicArray1TopologyPhysicalConnections[] =
{
    {
        KSPIN_TOPO_BRIDGE,          // TopologyOut
        KSPIN_WAVE_BRIDGE,          // WaveIn
        CONNECTIONTYPE_TOPOLOGY_OUTPUT
    }
};

#define DEFINE_CAPTURE_CABLE(var, topo, wave, index)                      \
static ENDPOINT_MINIPAIR var =                                             \
{                                                                          \
    eMicArrayDevice1,                                                      \
    topo,                                                                  \
    NULL,                                                                  \
    CreateMicArrayMiniportTopology,                                        \
    &MicArray1TopoMiniportFilterDescriptor,                                \
    0, NULL,                                                               \
    wave,                                                                  \
    NULL,                                                                  \
    CreateMiniportWaveRTVirtualAudioDriver,                                \
    &MicArrayWaveMiniportFilterDescriptor,                                 \
    0,                                                                     \
    NULL,                                                                  \
    MICARRAY_DEVICE_MAX_CHANNELS,                                          \
    MicArrayPinDeviceFormatsAndModes,                                      \
    SIZEOF_ARRAY(MicArrayPinDeviceFormatsAndModes),                        \
    MicArray1TopologyPhysicalConnections,                                  \
    SIZEOF_ARRAY(MicArray1TopologyPhysicalConnections),                    \
    ENDPOINT_NO_FLAGS,                                                     \
    index                                                                  \
};

DEFINE_CAPTURE_CABLE(CaptureCable1, L"TopologyCableMic1", L"WaveCableMic1", 0)
DEFINE_CAPTURE_CABLE(CaptureCable2, L"TopologyCableMic2", L"WaveCableMic2", 1)
DEFINE_CAPTURE_CABLE(CaptureCable3, L"TopologyCableMic3", L"WaveCableMic3", 2)
DEFINE_CAPTURE_CABLE(CaptureCable4, L"TopologyCableMic4", L"WaveCableMic4", 3)

//=============================================================================
//
// Render miniport pairs. NOTE: the split of render and capture is arbitrary and
// unnessary, this array could contain capture endpoints.
//
static
PENDPOINT_MINIPAIR  g_RenderEndpoints[] =
{
    &RenderCable1,
    &RenderCable2,
    &RenderCable3,
    &RenderCable4,
};

#define g_cRenderEndpoints  (SIZEOF_ARRAY(g_RenderEndpoints))

//=============================================================================
//
// Capture miniport pairs. NOTE: the split of render and capture is arbitrary and
// unnessary, this array could contain render endpoints.
//
static
PENDPOINT_MINIPAIR  g_CaptureEndpoints[] =
{
    &CaptureCable1,
    &CaptureCable2,
    &CaptureCable3,
    &CaptureCable4,
};

#define g_cCaptureEndpoints (SIZEOF_ARRAY(g_CaptureEndpoints))

//=============================================================================
//
// Total miniports = # endpoints * 2 (topology + wave).
//
#define g_MaxMiniports  ((g_cRenderEndpoints + g_cCaptureEndpoints) * 2)

#endif // _VIRTUALAUDIODRIVER_MINIPAIRS_H_
