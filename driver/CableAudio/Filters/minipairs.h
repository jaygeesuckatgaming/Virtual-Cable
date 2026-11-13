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

#include <devpkey.h>


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

//
// Per-cable endpoint names. The driver sets these on the KS interfaces when it
// starts, which is what Windows uses to name the audio endpoints. Doing it here
// avoids depending on the INF's interface AddReg being applied.
//
static const WCHAR Cable1Name[] = L"Virtual Cable 01";
static const WCHAR Cable2Name[] = L"Virtual Cable 02";
static const WCHAR Cable3Name[] = L"Virtual Cable 03";
static const WCHAR Cable4Name[] = L"Virtual Cable 04";

#define CABLE_NAME_PROPERTIES(name)                                        \
static const VIRTUALAUDIODRIVER_DEVPROPERTY name##Properties[] =           \
{                                                                          \
    {                                                                      \
        &DEVPKEY_DeviceInterface_FriendlyName,                             \
        DEVPROP_TYPE_STRING,                                               \
        sizeof(name),                                                      \
        (PVOID)name                                                        \
    }                                                                      \
};

CABLE_NAME_PROPERTIES(Cable1Name)
CABLE_NAME_PROPERTIES(Cable2Name)
CABLE_NAME_PROPERTIES(Cable3Name)
CABLE_NAME_PROPERTIES(Cable4Name)

#define DEFINE_RENDER_CABLE(var, topo, wave, index, props)                \
static ENDPOINT_MINIPAIR var =                                             \
{                                                                          \
    eSpeakerDevice,                                                        \
    topo,                                                                  \
    NULL,                                                                  \
    CreateMiniportTopologyVirtualAudioDriver,                              \
    &SpeakerTopoMiniportFilterDescriptor,                                  \
    SIZEOF_ARRAY(props),                                                   \
    props,                                                                 \
    wave,                                                                  \
    NULL,                                                                  \
    CreateMiniportWaveRTVirtualAudioDriver,                                \
    &SpeakerWaveMiniportFilterDescriptor,                                  \
    SIZEOF_ARRAY(props),                                                   \
    props,                                                                 \
    SPEAKER_DEVICE_MAX_CHANNELS,                                           \
    SpeakerPinDeviceFormatsAndModes,                                       \
    SIZEOF_ARRAY(SpeakerPinDeviceFormatsAndModes),                         \
    SpeakerTopologyPhysicalConnections,                                    \
    SIZEOF_ARRAY(SpeakerTopologyPhysicalConnections),                      \
    ENDPOINT_NO_FLAGS,                                                     \
    index                                                                  \
};

DEFINE_RENDER_CABLE(RenderCable1, L"TopologyCable1", L"WaveCable1", 0, Cable1NameProperties)
DEFINE_RENDER_CABLE(RenderCable2, L"TopologyCable2", L"WaveCable2", 1, Cable2NameProperties)
DEFINE_RENDER_CABLE(RenderCable3, L"TopologyCable3", L"WaveCable3", 2, Cable3NameProperties)
DEFINE_RENDER_CABLE(RenderCable4, L"TopologyCable4", L"WaveCable4", 3, Cable4NameProperties)
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

#define DEFINE_CAPTURE_CABLE(var, topo, wave, index, props)               \
static ENDPOINT_MINIPAIR var =                                             \
{                                                                          \
    eMicArrayDevice1,                                                      \
    topo,                                                                  \
    NULL,                                                                  \
    CreateMicArrayMiniportTopology,                                        \
    &MicArray1TopoMiniportFilterDescriptor,                                \
    SIZEOF_ARRAY(props),                                                   \
    props,                                                                 \
    wave,                                                                  \
    NULL,                                                                  \
    CreateMiniportWaveRTVirtualAudioDriver,                                \
    &MicArrayWaveMiniportFilterDescriptor,                                 \
    SIZEOF_ARRAY(props),                                                   \
    props,                                                                 \
    MICARRAY_DEVICE_MAX_CHANNELS,                                          \
    MicArrayPinDeviceFormatsAndModes,                                      \
    SIZEOF_ARRAY(MicArrayPinDeviceFormatsAndModes),                        \
    MicArray1TopologyPhysicalConnections,                                  \
    SIZEOF_ARRAY(MicArray1TopologyPhysicalConnections),                    \
    ENDPOINT_NO_FLAGS,                                                     \
    index                                                                  \
};

DEFINE_CAPTURE_CABLE(CaptureCable1, L"TopologyCableMic1", L"WaveCableMic1", 0, Cable1NameProperties)
DEFINE_CAPTURE_CABLE(CaptureCable2, L"TopologyCableMic2", L"WaveCableMic2", 1, Cable2NameProperties)
DEFINE_CAPTURE_CABLE(CaptureCable3, L"TopologyCableMic3", L"WaveCableMic3", 2, Cable3NameProperties)
DEFINE_CAPTURE_CABLE(CaptureCable4, L"TopologyCableMic4", L"WaveCableMic4", 3, Cable4NameProperties)

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
