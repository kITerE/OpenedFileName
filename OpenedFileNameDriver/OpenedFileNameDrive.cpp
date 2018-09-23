
#include <fltKernel.h>

#include "..\Include\OpenedFileNameDriver.h"

// ----------------------------------------------------------------------------

extern "C" DRIVER_INITIALIZE DriverEntry;
extern "C" DRIVER_UNLOAD DriverUnload;
extern "C" DRIVER_UNLOAD DriverUnload;
extern "C" DRIVER_DISPATCH DriverDispatchAlwaysSuccess;
extern "C" DRIVER_DISPATCH DriverDispatchDeviceControl;

// ----------------------------------------------------------------------------

#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DriverUnload)

// ----------------------------------------------------------------------------

namespace
{

// ----------------------------------------------------------------------------

PDEVICE_OBJECT g_pControlDeviceObject{};

// ----------------------------------------------------------------------------

NTSTATUS
ReferenceUserFileObject(
    IN  HANDLE hFileObject,
    OUT PFILE_OBJECT &pFileObject
)
{
    return 
        ObReferenceObjectByHandle(
                hFileObject,
                0,
                *IoFileObjectType,
                UserMode,
                reinterpret_cast<PVOID *>(&pFileObject),
                nullptr);
}

// ----------------------------------------------------------------------------

NTSTATUS
BuildOutputBuffer(
    IN  NTSTATUS CallStatus,
    IN  ULONG nOutputBufferLength,
    IN  const UNICODE_STRING *pusName OPTIONAL,
    OUT OpenedFileNameDriver::COutput &Output,
    OUT ULONG_PTR &nOutputBufferFilled
)
{
    if (NT_SUCCESS(CallStatus))
    {
        ASSERT(pusName);
        nOutputBufferFilled =
            FIELD_OFFSET(OpenedFileNameDriver::COutput, m_FileName) +
            pusName->Length;
    }
    else
    {
        nOutputBufferFilled =
            FIELD_OFFSET(OpenedFileNameDriver::COutput, m_FileName);
    }

    if (nOutputBufferFilled > nOutputBufferLength)
    {
        nOutputBufferFilled = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    Output.m_Status = CallStatus;

    if (NT_SUCCESS(CallStatus))
        RtlCopyMemory(Output.m_FileName, pusName->Buffer, pusName->Length);

    return STATUS_SUCCESS;
}

// ----------------------------------------------------------------------------

}

// ----------------------------------------------------------------------------

extern "C"
VOID
DriverUnload(
    _In_ PDRIVER_OBJECT pDriverObject
)
{
    UNREFERENCED_PARAMETER(pDriverObject);

    ::IoDeleteDevice(g_pControlDeviceObject);
}

// ----------------------------------------------------------------------------

NTSTATUS
DriverDispatchAlwaysSuccess(
    _In_ PDEVICE_OBJECT pDeviceObject,
    _Inout_ PIRP pIrp
)
{
    UNREFERENCED_PARAMETER(pDeviceObject);

    pIrp->IoStatus.Information = 0;
    pIrp->IoStatus.Status = STATUS_SUCCESS;

    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

// ----------------------------------------------------------------------------

NTSTATUS
DriverDispatchDeviceControl(
    _In_ PDEVICE_OBJECT pDeviceObject,
    _Inout_ PIRP pIrp
)
{
    UNREFERENCED_PARAMETER(pDeviceObject);

    NTSTATUS Status{STATUS_INVALID_DEVICE_REQUEST};
    ULONG_PTR nOutputBufferFilled{};

    union 
    {
        const OpenedFileNameDriver::CIoQueryFileDosDeviceNameInput *pIoQueryFileDosDeviceName;
        const OpenedFileNameDriver::CFltGetFileNameInformationUnsafeInput *pFltGetFileNameInformationUnsafe;
        const OpenedFileNameDriver::CIoVolumeDeviceToDosNameInput *pIoVolumeDeviceToDosName;
        OpenedFileNameDriver::COutput *pOutput;
        PVOID pRawBuffer;
    };
    pRawBuffer = pIrp->AssociatedIrp.SystemBuffer;

    const auto &Parameters = IoGetCurrentIrpStackLocation(pIrp)->Parameters.DeviceIoControl;
    switch (Parameters.IoControlCode)
    {
    case OpenedFileNameDriver::ControlCode::Call_IoQueryFileDosDeviceName:
        ASSERT(Parameters.InputBufferLength == sizeof(OpenedFileNameDriver::CIoQueryFileDosDeviceNameInput));
        if (Parameters.InputBufferLength != sizeof(OpenedFileNameDriver::CIoQueryFileDosDeviceNameInput))
        {
            Status = STATUS_INVALID_PARAMETER;
        }
        else
        {
            PFILE_OBJECT pFileObject{};
            Status =
                ReferenceUserFileObject(
                    pIoQueryFileDosDeviceName->m_hFileObject,
                    pFileObject);
            ASSERT(NT_SUCCESS(Status));
            if (!NT_SUCCESS(Status))
                break;

            POBJECT_NAME_INFORMATION pObjectNameInformation{};

            const auto CallStatus =
                ::IoQueryFileDosDeviceName(
                    pFileObject,
                    &pObjectNameInformation);

            Status =
                BuildOutputBuffer(
                    CallStatus,
                    Parameters.OutputBufferLength,
                    pObjectNameInformation ? &(pObjectNameInformation->Name) : nullptr,
                    *pOutput,
                    nOutputBufferFilled);

            if (pObjectNameInformation)
                ExFreePool(pObjectNameInformation);

            ::ObDereferenceObject(pFileObject);
        }
        break;

    case OpenedFileNameDriver::ControlCode::Call_FltGetFileNameInformationUnsafe:
        ASSERT(Parameters.InputBufferLength == sizeof(OpenedFileNameDriver::CFltGetFileNameInformationUnsafeInput));
        if (Parameters.InputBufferLength != sizeof(OpenedFileNameDriver::CFltGetFileNameInformationUnsafeInput))
        {
            Status = STATUS_INVALID_PARAMETER;
        }
        else
        {
            PFILE_OBJECT pFileObject{};
            Status =
                ReferenceUserFileObject(
                    pFltGetFileNameInformationUnsafe->m_hFileObject,
                    pFileObject);
            ASSERT(NT_SUCCESS(Status));
            if (!NT_SUCCESS(Status))
                break;

            PFLT_FILE_NAME_INFORMATION pFileNameInformation{};
            const auto CallStatus =
                ::FltGetFileNameInformationUnsafe(
                    pFileObject,
                    nullptr,
                    pFltGetFileNameInformationUnsafe->m_nNameOptions | FLT_FILE_NAME_QUERY_DEFAULT,
                    &pFileNameInformation);
            
            Status =
                BuildOutputBuffer(
                    CallStatus,
                    Parameters.OutputBufferLength,
                    pFileNameInformation ? &(pFileNameInformation->Name) : nullptr,
                    *pOutput,
                    nOutputBufferFilled);

            if (pFileNameInformation)
                ::FltReleaseFileNameInformation(pFileNameInformation);

            ::ObDereferenceObject(pFileObject);
        }
        break;

    case OpenedFileNameDriver::ControlCode::Call_IoVolumeDeviceToDosName:
        ASSERT(Parameters.InputBufferLength == sizeof(OpenedFileNameDriver::CIoVolumeDeviceToDosNameInput));
        if (Parameters.InputBufferLength != sizeof(OpenedFileNameDriver::CIoVolumeDeviceToDosNameInput))
        {
            Status = STATUS_INVALID_PARAMETER;
        }
        else
        {
            PFILE_OBJECT pFileObject{};
            Status =
                ReferenceUserFileObject(
                    pIoVolumeDeviceToDosName->m_hFileObject,
                    pFileObject);
            ASSERT(NT_SUCCESS(Status));
            if (!NT_SUCCESS(Status))
                break;

            UNICODE_STRING usVolumeName{};
            const auto CallStatus =
                ::IoVolumeDeviceToDosName(
                    pFileObject->DeviceObject,
                    &usVolumeName);

            Status =
                BuildOutputBuffer(
                    CallStatus,
                    Parameters.OutputBufferLength,
                    &usVolumeName,
                    *pOutput,
                    nOutputBufferFilled);

            if (usVolumeName.Buffer)
                ExFreePool(usVolumeName.Buffer);

            ::ObDereferenceObject(pFileObject);
        }
        break;
    }

    pIrp->IoStatus.Information = nOutputBufferFilled;
    pIrp->IoStatus.Status = Status;

    ASSERT(Status != STATUS_PENDING);
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return Status;
}

// ----------------------------------------------------------------------------

extern "C"
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT pDriverObject,
    _In_ PUNICODE_STRING pusRegistryPath
)
{
    UNREFERENCED_PARAMETER(pusRegistryPath);

    pDriverObject->DriverUnload = DriverUnload;

    pDriverObject->MajorFunction[IRP_MJ_CREATE] = DriverDispatchAlwaysSuccess;
    pDriverObject->MajorFunction[IRP_MJ_CLEANUP] = DriverDispatchAlwaysSuccess;
    pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDispatchDeviceControl;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverDispatchAlwaysSuccess;

    UNICODE_STRING usDeviceName;
    RtlInitUnicodeString(&usDeviceName, OpenedFileNameDriver::g_wszDeviceName);

    auto Status =
        ::IoCreateDevice(
            pDriverObject,
            0,
            &usDeviceName,
            FILE_DEVICE_UNKNOWN,
            0,
            FALSE,
            &g_pControlDeviceObject);
    ASSERT(NT_SUCCESS(Status));
    if (!NT_SUCCESS(Status))
        return Status;

    ASSERT(g_pControlDeviceObject);
    g_pControlDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

// ----------------------------------------------------------------------------