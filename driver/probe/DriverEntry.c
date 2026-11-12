// Minimal WDM driver used to prove the manual build toolchain (no Visual
// Studio driver toolset required).
#include <ntddk.h>

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint("[CableAudio] M0 probe driver loaded\n");
    return STATUS_SUCCESS;
}
