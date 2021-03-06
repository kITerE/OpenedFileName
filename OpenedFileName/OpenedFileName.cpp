// OpenedFileName.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

#include "..\Include\OpenedFileNameDriver.h"

// ----------------------------------------------------------------------------

struct CFlag
{
    DWORD m_nValue;
    PCWSTR m_wszName;
};

// ----------------------------------------------------------------------------

namespace UserMode
{

namespace
{

constexpr CFlag g_NormalizationFlags[]
{
    { FILE_NAME_NORMALIZED, L"FILE_NAME_NORMALIZED" },
    { FILE_NAME_OPENED,     L"FILE_NAME_OPENED" },
};

constexpr CFlag g_VolumeFlags[]
{
    { VOLUME_NAME_DOS,      L"VOLUME_NAME_DOS" },
    { VOLUME_NAME_GUID,     L"VOLUME_NAME_GUID" },
    { VOLUME_NAME_NONE,     L"VOLUME_NAME_NONE" },
    { VOLUME_NAME_NT,       L"VOLUME_NAME_NT" },
};

}

void Run(HANDLE hFileObject)
{
    std::vector<WCHAR> wszFinalPath;
    constexpr DWORD nFinalPathChars = MAXSHORT / sizeof(WCHAR);
    wszFinalPath.resize(nFinalPathChars + 1);

    std::wcout << L" [+] GetFinalPathNameByHandle(...) " << std::endl;

    for (const auto &NormalizationFlag : g_NormalizationFlags)
    {
        for (const auto &VolumeFlag : g_VolumeFlags)
        {
            const auto nChars =
                ::GetFinalPathNameByHandle(
                    hFileObject,
                    wszFinalPath.data(),
                    nFinalPathChars,
                    NormalizationFlag.m_nValue | VolumeFlag.m_nValue);
            if (nChars)
            {
                std::wcout << NormalizationFlag.m_wszName << L" | " << VolumeFlag.m_wszName;
                std::wcout << L" == " << wszFinalPath.data() << std::endl;
            }
            else
            {
                auto hResult = HRESULT_FROM_WIN32(::GetLastError());

                std::wcerr << NormalizationFlag.m_wszName << L" | " << VolumeFlag.m_wszName;
                std::wcerr << L" failed. Error 0x" << std::hex << hResult << std::endl;
            }

        }
        std::wcout << std::endl;
    }
}

}

// ----------------------------------------------------------------------------

class CDriverInstaller
{
public:
    HRESULT Run()
    {
        HRESULT hResult;

        ATLASSERT(!m_hSysFile);

        const auto Resource = ::FindResource(nullptr, L"IDR_SYS", L"SYS");
        if (!Resource)
        {
            hResult = HRESULT_FROM_WIN32(::GetLastError());
            std::wcerr << L"Find SYS resource failed. Error 0x" << std::hex << hResult << std::endl;
            return hResult;
        }

        const auto nSizeOfResource = ::SizeofResource(NULL, Resource);
        if (!nSizeOfResource)
        {
            hResult = HRESULT_FROM_WIN32(::GetLastError());
            std::wcerr << L"Process SYS resource failed. Error 0x" << std::hex << hResult << std::endl;
            return hResult;
        }

        const auto pBuffer = ::LoadResource(nullptr, Resource);
        if (!pBuffer)
        {
            hResult = HRESULT_FROM_WIN32(::GetLastError());
            std::wcerr << L"Load SYS resource failed. Error 0x" << std::hex << hResult << std::endl;
            return hResult;
        }

        std::vector<WCHAR> wszSysFilePath;
        wszSysFilePath.resize(MAX_PATH + 1);
        if (!::GetSystemDirectory(wszSysFilePath.data(), MAX_PATH))
        {
            hResult = HRESULT_FROM_WIN32(::GetLastError());
            std::wcerr << L"Build SYS file path failed. Error 0x" << std::hex << hResult << std::endl;
            return hResult;
        }
        wcscat_s(wszSysFilePath.data(), MAX_PATH, L"\\drivers\\OpenedFileNameDriver.sys");

        {
            ATL::CAtlFile hSysFile;
            hResult =
                hSysFile.Create(
                    wszSysFilePath.data(),
                    GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_DELETE,
                    CREATE_ALWAYS);
            if (S_OK != hResult)
            {
                std::wcerr << L"Create " << wszSysFilePath.data() << L" failed. Error 0x" << std::hex << hResult << std::endl;
                return hResult;
            }

            hResult = hSysFile.Write(pBuffer, nSizeOfResource);
            if (S_OK != hResult)
            {
                std::wcerr << L"Write sys file failed. Error 0x" << std::hex << hResult << std::endl;
                return hResult;
            }
        }
        hResult =
            m_hSysFile.Create(
                wszSysFilePath.data(),
                DELETE,
                FILE_SHARE_READ | FILE_SHARE_DELETE,
                OPEN_EXISTING,
                FILE_FLAG_DELETE_ON_CLOSE);
        if (S_OK != hResult)
        {
            std::wcerr << L"Open " << wszSysFilePath.data() << L" failed. Error 0x" << std::hex << hResult << std::endl;
            return hResult;
        }

        ATLASSERT(!m_hScManager);
        m_hScManager.reset(::OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE));
        if (!m_hScManager)
        {
            hResult = HRESULT_FROM_WIN32(::GetLastError());
            std::wcerr << L"Open SC Manager failed. Error 0x" << std::hex << hResult << std::endl;
            return hResult;
        }

        m_hDriver.reset(
            ::CreateService(
                m_hScManager.get(),
                L"OpenedFileNameDriver",
                L"OpenedFileNameDriver",
                SERVICE_START | SERVICE_STOP | DELETE,
                SERVICE_KERNEL_DRIVER,
                SERVICE_DEMAND_START,
                SERVICE_ERROR_NORMAL,
                L"System32\\drivers\\OpenedFileNameDriver.sys",
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr));
        if (!m_hDriver)
        {
            hResult = HRESULT_FROM_WIN32(::GetLastError());
            std::wcerr << L"Create driver failed. Error 0x" << std::hex << hResult << std::endl;
            return hResult;
        }

        if (!::StartService(m_hDriver.get(), 0, nullptr))
        {
            hResult = HRESULT_FROM_WIN32(::GetLastError());
            std::wcerr << L"Start driver failed. Error 0x" << std::hex << hResult << std::endl;
            return hResult;
        }

        std::wcout << L" [i] Driver installed and started" << std::endl << std::endl;
        return S_OK;
    }

    ~CDriverInstaller()
    {
        if (m_hDriver)
        {
            SERVICE_STATUS ServiceStatus{};
            if (!::ControlService(m_hDriver.get(), SERVICE_CONTROL_STOP, &ServiceStatus))
            {
                const auto hResult = HRESULT_FROM_WIN32(::GetLastError());
                std::wcerr << L"Stop driver failed. Error 0x" << std::hex << hResult << std::endl;
            }

            if (!::DeleteService(m_hDriver.get()))
            {
                const auto hResult = HRESULT_FROM_WIN32(::GetLastError());
                std::wcerr << L"Delete driver failed. Error 0x" << std::hex << hResult << std::endl;
            }
            else
            {
                std::wcout << L" [i] Driver removed" << std::endl << std::endl;
            }
        }
    }

private:
    ATL::CAtlFile m_hSysFile;

    using CScHandle = std::unique_ptr<struct SC_HANDLE__, decltype(&CloseServiceHandle)>;
    CScHandle m_hScManager{nullptr, &CloseServiceHandle};
    CScHandle m_hDriver{nullptr, &CloseServiceHandle};
};

// ----------------------------------------------------------------------------

namespace KernelMode
{
namespace
{

namespace Driver = OpenedFileNameDriver;

static_assert(std::is_pod<Driver::CIoQueryFileDosDeviceNameInput>::value);
static_assert(std::is_pod<Driver::CFltGetFileNameInformationUnsafeInput>::value);
static_assert(std::is_pod<Driver::CIoVolumeDeviceToDosNameInput>::value);
static_assert(std::is_pod<Driver::COutput>::value);

constexpr CFlag g_NameFormatFlags[]
{
    { 0x00000001,   L"FLT_FILE_NAME_NORMALIZED" },
    { 0x00000002,   L"FLT_FILE_NAME_OPENED" },
    { 0x00000003,   L"FLT_FILE_NAME_SHORT" },
};

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif  // NT_SUCCESS

}

void Run(HANDLE hFileObject)
{
    HRESULT hResult;

    ATL::CAtlFile hControlDevice;
    {
        static constexpr WCHAR wszNtPrefix[]{L"\\\\?\\GLOBALROOT"};
        std::vector<WCHAR> wszControlDeviceName;
        wszControlDeviceName.resize(_countof(wszNtPrefix) + _countof(Driver::g_wszDeviceName));

        wcscpy_s(wszControlDeviceName.data(), wszControlDeviceName.size(), wszNtPrefix);
        wcscat_s(wszControlDeviceName.data(), wszControlDeviceName.size(), Driver::g_wszDeviceName);

        hResult =
            hControlDevice.Create(
                wszControlDeviceName.data(),
                GENERIC_READ,
                FILE_SHARE_READ,
                OPEN_EXISTING);
        if (S_OK != hResult)
        {
            std::wcerr << L"Open control driver device (" << wszControlDeviceName.data()  << L") failed. Error 0x" << std::hex << hResult << std::endl;
            return;
        }
    }

    union
    {
        BYTE Raw[FIELD_OFFSET(Driver::COutput, m_FileName) + (MAXSHORT / sizeof(WCHAR))];
        Driver::COutput Typed;
    } Output;

    const auto CallDeviceIoControl
    {
        [&](ULONG nControlCode, PVOID pInput, ULONG nInputSize, PCWSTR wszFlagName = nullptr)
        {
            DWORD dwReturned{};
            const auto bResult =
                ::DeviceIoControl(
                    hControlDevice,
                    nControlCode,
                    pInput,
                    nInputSize,
                    Output.Raw,
                    sizeof(Output.Raw),
                    &dwReturned,
                    nullptr);
            if (!bResult)
            {
                hResult = HRESULT_FROM_WIN32(::GetLastError());
                std::wcerr << L"DeviceIoControl(...) failed. Error 0x" << std::hex << hResult << std::endl;
            }
            else if (!NT_SUCCESS(Output.Typed.m_Status))
            {
                hResult = HRESULT_FROM_NT(Output.Typed.m_Status);
                if (wszFlagName)
                    std::wcerr << wszFlagName;
                else
                    std::wcerr << L"Call";
                std::wcerr << L" failed. Error 0x" << std::hex << hResult << std::endl;
            }
            else
            {
                ATLASSERT(dwReturned >= FIELD_OFFSET(Driver::COutput, m_FileName));
                const std::wstring Result
                {
                    &Output.Typed.m_FileName[0],
                    &Output.Typed.m_FileName[(dwReturned - FIELD_OFFSET(Driver::COutput, m_FileName)) / sizeof(WCHAR)]
                };
                if (wszFlagName)
                    std::wcout << wszFlagName << L" == ";
                std::wcout << Result << std::endl;
            }
        }
    };

    std::wcout << L" [+] FltGetFileNameInformationUnsafe(...)" << std::endl;

    for (const auto &NameFormatFlag : g_NameFormatFlags)
    {
        Driver::CFltGetFileNameInformationUnsafeInput Input
        {
            hFileObject,
            NameFormatFlag.m_nValue
        };
        CallDeviceIoControl(
            Driver::ControlCode::Call_FltGetFileNameInformationUnsafe,
            &Input,
            sizeof(Input),
            NameFormatFlag.m_wszName);
    }
    std::wcout << std::endl;

    std::wcout << L" [+] IoQueryFileDosDeviceName(...)" << std::endl;

    {
        Driver::CIoQueryFileDosDeviceNameInput Input
        {
            hFileObject
        };
        CallDeviceIoControl(
            Driver::ControlCode::Call_IoQueryFileDosDeviceName,
            &Input,
            sizeof(Input));
    }
    std::wcout << std::endl;

    std::wcout << L" [+] IoVolumeDeviceToDosName(...)" << std::endl;

    {
        Driver::CIoVolumeDeviceToDosNameInput Input
        {
            hFileObject
        };
        CallDeviceIoControl(
            Driver::ControlCode::Call_IoVolumeDeviceToDosName,
            &Input,
            sizeof(Input));
    }
    std::wcout << std::endl;
}

// ----------------------------------------------------------------------------

}

// ----------------------------------------------------------------------------

void RunTests(
    HANDLE hFileObject,
    std::unique_ptr<CDriverInstaller> &pDriverInstaller
)
{
    UserMode::Run(hFileObject);

    if (!pDriverInstaller)
    {
        std::unique_ptr<CDriverInstaller> pNewDriverInstaller{std::make_unique<CDriverInstaller>()};
        if (pNewDriverInstaller->Run() != S_OK)
            return;
        pDriverInstaller = std::move(pNewDriverInstaller);
    }

    KernelMode::Run(hFileObject);
}

// ----------------------------------------------------------------------------

int wmain(int argc, WCHAR *argv[])
{
    if (argc != 2)
    {
        if (argc != 1)
        {
            std::wcerr << L"Invalid command line" << std::endl;
        }
        std::wcout << L"Usage: " << argv[0] << L" <FILE_SYSTEM_PATH>" << std::endl;
        return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
    }

    HRESULT hResult;
    ATL::CAtlFile FileObject;
    std::unique_ptr<CDriverInstaller> pDriverInstaller;

    std::wcout << L" [i] Input path: " << argv[1] << std::endl << std::endl;

    hResult =
        FileObject.Create(
            argv[1],
            SYNCHRONIZE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS);
    if (S_OK != hResult)
    {
        std::wcerr << L"Open file object failed. Error 0x" << std::hex << hResult << std::endl;
        return hResult;
    }

    RunTests(FileObject, pDriverInstaller);

    std::wcout << L" [i] Re-open file " << argv[1] << L" without traverse privileges" << std::endl << std::endl;
    FileObject.Close();

    {
        ATL::CAccessToken ThreadToken;
        if (!ThreadToken.GetProcessToken(TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY))
        {
            hResult = HRESULT_FROM_WIN32(::GetLastError());
            std::wcerr << L"Open thread token failed. Error 0x" << std::hex << hResult << std::endl;
            return S_OK;
        }
        if (!ThreadToken.DisablePrivilege(SE_CHANGE_NOTIFY_NAME))
        {
            hResult = HRESULT_FROM_WIN32(::GetLastError());
            std::wcerr << L"Disable traverse privilege failed. Error 0x" << std::hex << hResult << std::endl;
            return S_OK;
        }
        hResult =
            FileObject.Create(
                argv[1],
                SYNCHRONIZE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS);
        if (!ThreadToken.EnablePrivilege(SE_CHANGE_NOTIFY_NAME))
        {
            hResult = HRESULT_FROM_WIN32(::GetLastError());
            std::wcerr << L"Revert traverse privilege failed. Error 0x" << std::hex << hResult << std::endl;
            return S_OK;
        }
        if (S_OK != hResult)
        {
            std::wcerr << L"Open file object failed. Error 0x" << std::hex << hResult << std::endl;
            return hResult;
        }
    }

    RunTests(FileObject, pDriverInstaller);

    return S_OK;
}

// ----------------------------------------------------------------------------
