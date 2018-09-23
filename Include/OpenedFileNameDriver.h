
#pragma once

// ----------------------------------------------------------------------------

namespace OpenedFileNameDriver
{

// ----------------------------------------------------------------------------

struct ControlCode
{
enum EValue
{
    Call_IoQueryFileDosDeviceName = 
        CTL_CODE(FILE_DEVICE_UNKNOWN, 1, METHOD_BUFFERED, FILE_ANY_ACCESS),

    Call_FltGetFileNameInformationUnsafe =
        CTL_CODE(FILE_DEVICE_UNKNOWN, 2, METHOD_BUFFERED, FILE_ANY_ACCESS),

    Call_IoVolumeDeviceToDosName =
        CTL_CODE(FILE_DEVICE_UNKNOWN, 3, METHOD_BUFFERED, FILE_ANY_ACCESS),
};
};

// ----------------------------------------------------------------------------

#pragma pack(push, 1)
struct CIoQueryFileDosDeviceNameInput
{
    HANDLE m_hFileObject;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct CFltGetFileNameInformationUnsafeInput
{
    HANDLE m_hFileObject;
    ULONG m_nNameOptions;
};
#pragma pack(pop)

using CIoVolumeDeviceToDosNameInput = CIoQueryFileDosDeviceNameInput;

#pragma pack(push, 1)
struct COutput
{
    NTSTATUS m_Status;
    WCHAR m_FileName[ANYSIZE_ARRAY];
};
#pragma pack(pop)

// ----------------------------------------------------------------------------

constexpr WCHAR g_wszDeviceName[]{L"\\Device\\{1AA475E0-2240-486C-98A0-CF36555F5AE3}"};

// ----------------------------------------------------------------------------

}   // namespace OpenedFileNameDriver

// ----------------------------------------------------------------------------
