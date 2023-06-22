#pragma comment(lib, "wbemuuid.lib")

#include <comdef.h>
#include <wbemidl.h>
#include <stdio.h>

struct COM
{
	COM()
	{
		// Initialize COM
		hr = CoInitializeEx(0, COINIT_MULTITHREADED);
		if (FAILED(hr))
		{
			printf("CoInitializeEx: %x\n", hr);
			return;
		}

		hr = SetDefaultSecurity();
		if (FAILED(hr))
		{
			CoUninitialize();
		}
	}

	~COM()
	{
		if (!FAILED(hr))
		{
			CoUninitialize();
		}
	}

private:
	HRESULT SetDefaultSecurity()
	{
		HRESULT result;

		if (FAILED(hr))
		{
			return hr;
		}

		// Set default security settings
		result = CoInitializeSecurity(
			NULL,                        // Security descriptor    
			-1,                          // COM negotiates authentication service
			NULL,                        // Authentication services
			NULL,                        // Reserved
			RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication level for proxies
			RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation level for proxies
			NULL,                        // Authentication info
			EOAC_NONE,                   // Additional capabilities of the client or server
			NULL                         // Reserved
		);
		if (FAILED(result))
		{
			printf("CoInitializeSecurity: %x\n", result);
		}

		return result;
	}

	HRESULT hr = E_FAIL;
};

struct WMI
{
	WMI(COM& Com)
	{
		HRESULT hr;

		// Initialize IWbemLocator
		hr = CoCreateInstance(
			CLSID_WbemLocator,
			0,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&connector)
		);
		if (FAILED(hr))
		{
			printf("CoCreateInstance: %x\n", hr);
			return;
		}

		hr = DefaultConnect();
		if (FAILED(hr))
		{
			connector->Release();
		}
	}

	void SetupQuery(const wchar_t* Query, IEnumWbemClassObject** Enumerator)
	{
		HRESULT hr;

		if (proxy)
		{
			hr = proxy->ExecNotificationQuery(
				BSTR(L"WQL"),
				BSTR(Query),
				WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
				NULL,
				Enumerator
			);
			if (FAILED(hr))
			{
				printf("ExecNotificationQuery: %x\n", hr);
			}
		}
	}

	~WMI()
	{
		if (proxy)
		{
			proxy->Release();
		}

		if (connector)
		{
			connector->Release();
		}
	}

private:
	HRESULT DefaultConnect()
	{
		HRESULT hr;

		// Connect to IWbemServices
		hr = connector->ConnectServer(
			BSTR(L"root\\cimv2"),	// namespace
			NULL,					// User name 
			NULL,					// User password
			0,						// Locale 
			NULL,					// Security flags
			0,						// Authority 
			0,						// Context object 
			&proxy					// IWbemServices proxy
		);
		if (FAILED(hr))
		{
			printf("ConnectServer: %x\n", hr);
			return hr;
		}

		// Set security levels of the WMI connection
		hr = CoSetProxyBlanket(proxy,
			RPC_C_AUTHN_WINNT,
			RPC_C_AUTHZ_NONE,
			NULL,
			RPC_C_AUTHN_LEVEL_CALL,
			RPC_C_IMP_LEVEL_IMPERSONATE,
			NULL,
			EOAC_NONE
		);
		if (FAILED(hr))
		{
			printf("CoSetProxyBlanket: %x\n", hr);
		}

		return hr;
	}

	IWbemLocator* connector = nullptr;
	IWbemServices* proxy = nullptr;
};

struct ProcessCreation
{
	ProcessCreation(WMI& Wmi) : wmi(Wmi)
	{
		wmi.SetupQuery(
			L"SELECT * FROM __InstanceCreationEvent WITHIN 1 "
            L"WHERE TargetInstance ISA 'Win32_Process'",
			&enumerator
		);
	}

	LONG CheckForImageName(const wchar_t* ImageName)
	{
		IWbemClassObject* object = nullptr;
		IUnknown* process = nullptr;
		ULONG returned = 0;
		LONG pid = -1;
        HRESULT hr;
        _variant_t vProcess, vName, vPid;

		if (!enumerator)
		{
			goto fail;
		}
		
        hr = enumerator->Next(WBEM_NO_WAIT, 1, &object, &returned);
        if (FAILED(hr))
        {
			printf("IEnumWbemClassObject::Next: %x\n", hr);
            goto fail;
        }

		if (1 == returned)
		{
            hr = object->Get(L"TargetInstance", 0, &vProcess, 0, 0);
            if (FAILED(hr))
            {
                printf("IWbemClassObject::Get: %x\n", hr);
                goto fail;
            }

            object->Release();
            object = nullptr;

            process = vProcess;
            hr = process->QueryInterface(IID_PPV_ARGS(&object));
            process->Release();
            if (FAILED(hr))
            {
                printf("IUnknown::QueryInterface: %x\n", hr);
                goto fail;
            }

            hr = object->Get(L"Name", 0, &vName, 0, 0);
            if (FAILED(hr))
            {
                printf("IWbemClassObject::Get: %x\n", hr);
                goto fail;
            }

            if (!wcscmp(vName.bstrVal, ImageName))
            {
                hr = object->Get(L"Handle", 0, &vPid, 0, 0);
                if (FAILED(hr))
                {
                    printf("IWbemClassObject::Get: %x\n", hr);
                    goto fail;
                }

                pid = _wtoi(vPid.bstrVal);

                goto fail;
            }
		}

	fail:
		if (object)
		{
			object->Release();
		}

		return pid;
	}

private:
	WMI &wmi;
	IEnumWbemClassObject* enumerator = nullptr;
};